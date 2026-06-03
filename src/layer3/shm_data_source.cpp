// shm_data_source.cpp
// ShmDataSource 实现：从共享内存读 v1.2 协议，转 DisplaySnapshot 推送

#include "shm_data_source.h"
#include "layer1/shm/shm_display.h"
#include "layer2/time_util.h"

#include <QDebug>
#include <cstring>
#include <cmath>

// 指示灯 ID 顺序（来自 shm_display.h 的 ShmIndicatorId enum）
// IND_LEFT_TURN=0, IND_RIGHT_TURN=1, IND_PARK_BRAKE=2,
// IND_READY_GO=3, IND_BAT_WARN=4, IND_ENGINE=5,
// IND_HIGH_VOLT=6, IND_FOG_LIGHT=7, IND_SEATBELT=8, IND_TIRE_PRESSURE=9, IND_COUNT=10
// （无需重新定义，直接用）

// 座位顺序
enum SeatId {
    SEAT_DRIVER = 0,
    SEAT_PASSENGER,
    SEAT_REAR_LEFT,
    SEAT_REAR_CENTER,
    SEAT_REAR_RIGHT
};

ShmDataSource::ShmDataSource(QObject* parent) : QObject(parent) {}

ShmDataSource::~ShmDataSource() {
    stop();
}

bool ShmDataSource::start() {
    if (m_running) return true;

    // 打开共享内存
    if (shm_display_open() < 0) {
        qWarning() << "[ShmDataSource] Failed to open shm at" << SHM_DISPLAY_PATH;
        m_snapshot.health = HEALTH_DISCONNECTED;
        if (m_healthCb) m_healthCb(HEALTH_DISCONNECTED);
    } else {
        qDebug() << "[ShmDataSource] Opened shm at" << SHM_DISPLAY_PATH;
    }

    // 启动 16ms 定时器
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ShmDataSource::onTick);
    m_timer->start(m_tickIntervalMs);

    m_running = true;
    return true;
}

void ShmDataSource::stop() {
    if (!m_running) return;
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    shm_display_close();
    m_running = false;
}

void ShmDataSource::onTick() {
    const uint64_t now = candash::now_monotonic_ms();

    // ─── 1. 健康检查 ───
    int health = shm_display_health_check();
    HealthStatus new_health;
    if (health != 0) {
        new_health = HEALTH_DISCONNECTED;
    } else {
        const uint64_t age = shm_display_age_ms(now);
        if (age == UINT64_MAX) {
            new_health = HEALTH_WAITING;
        } else if (age > SHM_HEARTBEAT_TIMEOUT_MS) {
            new_health = HEALTH_STALE;
        } else {
            new_health = HEALTH_OK;
        }
    }

    // 健康状态变化时推送
    if (new_health != m_lastHealth) {
        m_snapshot.health = new_health;
        m_lastHealth = new_health;
        if (m_healthCb) m_healthCb(new_health);
    }

    // 离线时不推送数据快照（防 stale UI）
    if (new_health != HEALTH_OK) {
        return;
    }

    // ─── 2. 轮询新帧 ───
    uint64_t ts = shm_display_poll(m_lastCommitTs);
    if (ts == m_lastCommitTs) {
        return;  // 无新帧
    }
    m_lastCommitTs = ts;

    // ─── 3. 读取数据 ───
    DisplayDataShm shm = {};
    int rc = shm_display_read(&shm, nullptr);
    if (rc < 0) {
        static int last_err = 0;
        if (rc != last_err) {
            qWarning() << "[ShmDataSource] shm_display_read failed:" << rc
                       << "(-2=ABI, -3=checksum)";
            last_err = rc;
        }
        return;
    }

    // ─── 4. 业务转换：ShmDisplayData → DisplaySnapshot ───
    DisplaySnapshot next;
    convertSnapshot(shm, next);

    // FPS 窗口
    if (m_fpsWindowStart == 0) m_fpsWindowStart = now;
    m_fpsCountInWindow++;
    if (now - m_fpsWindowStart >= 1000) {
        m_fps = static_cast<double>(m_fpsCountInWindow) * 1000.0
                / static_cast<double>(now - m_fpsWindowStart);
        m_fpsWindowStart = now;
        m_fpsCountInWindow = 0;
    }

    // 丢帧检测
    if (m_lastFrameSeq > 0 && shm.frame_seq > m_lastFrameSeq + 1) {
        m_droppedFrames += (shm.frame_seq - m_lastFrameSeq - 1);
    }
    m_lastFrameSeq = shm.frame_seq;
    next.meta.dropped_frames = m_droppedFrames;

    // ─── 4.5. 派生指标: TripComputer (v3 探针延伸) ───
    // 用 shm commit 时间戳驱动 tick (而非 wall clock), 保证:
    //   - 与数据流同步: 数据帧到了才推进小计
    //   - 重放/录像数据可重现: 用帧时间戳而非真实时间
    m_trip.tick(static_cast<uint64_t>(shm.last_commit_ms), next.data.vehicle_speed);
    m_trip.tickEnergy(static_cast<uint64_t>(shm.last_commit_ms),
                      next.data.bat_volt, next.data.bat_curr,
                      next.data.bat_soc, next.data.ev_range);
    next.trip_distance_km            = m_trip.tripDistanceKm();
    next.trip_avg_speed_kmh          = m_trip.tripAvgSpeedKmh();
    next.trip_duration_s             = m_trip.tripDurationS();
    next.trip_is_moving              = m_trip.isMoving();
    next.trip_energy_kwh             = m_trip.energyKWh();
    next.trip_efficiency_kwh100km    = m_trip.efficiencyKWh100Km();
    next.trip_range_confidence_pct   = m_trip.rangeConfidencePct();

    // ─── 4.6. 主题 (PR 7) ───
    // 同样用 shm commit 时间戳驱动 tick, 16ms 节奏与数据流同步
    // QML 端 setThemeMode → m_theme.setMode() → 下次 onTick 自然反映到 snapshot
    m_theme.tick(static_cast<uint64_t>(shm.last_commit_ms));
    const candash::ThemeColors tc = m_theme.colors();
    next.theme_mode                = static_cast<uint8_t>(m_theme.currentMode());
    next.theme_is_day              = m_theme.isDay() ? 1u : 0u;
    next.theme_color_background    = tc.background;
    next.theme_color_foreground    = tc.foreground;
    next.theme_color_accent        = tc.accent;
    next.theme_color_warning       = tc.warning;
    next.theme_color_critical      = tc.critical;

    // ─── 4.7. WarningManager (PR 9) ───
    // tick 处理 hold 过期, 然后把 m_warning.activeWarnings() 复制到 snapshot
    // 报警推入 (pushAlarm) 由 AlarmRuntime 触发 — 本 PR 还未接入, 测试用 pushWarningForTest 注入
    m_warning.tick(static_cast<uint64_t>(shm.last_commit_ms));
    const auto& warns = m_warning.activeWarnings();
    next.warning_count = 0;
    next.has_critical  = m_warning.hasCritical() ? 1u : 0u;
    const size_t cap = std::min(warns.size(), static_cast<size_t>(DISPLAY_WARNING_MAX));
    for (size_t i = 0; i < cap; i++) {
        const auto& src = warns[i];
        auto& dst = next.active_warnings[i];
        std::memcpy(dst.name, src.name, sizeof(dst.name));
        std::memcpy(dst.text_zh, src.text_zh, sizeof(dst.text_zh));
        std::memcpy(dst.text_en, src.text_en, sizeof(dst.text_en));
        dst.severity      = src.severity;
        dst.priority      = src.priority;
        dst.color         = src.color;
        dst.first_seen_ms = src.first_seen_ms;
        dst.last_seen_ms  = src.last_seen_ms;
        dst.dedup_count   = src.dedup_count;
        next.warning_count = static_cast<uint8_t>(i + 1);
    }
    // 剩余 slot 清零 (避免脏数据)
    for (size_t i = cap; i < DISPLAY_WARNING_MAX; i++) {
        std::memset(&next.active_warnings[i], 0, sizeof(DisplayActiveWarning));
    }

    // ─── 5. 推送快照 ───
    m_snapshot = next;
    if (m_updateCb) m_updateCb(m_snapshot);
}

void ShmDataSource::convertSnapshot(const DisplayDataShm& shm, DisplaySnapshot& out) const {
    // ─── 28 业务字段：手动逐字段 copy（DisplayData 与 DisplayDataShm 字段顺序/类型不一致）───
    out.data.bat_volt       = shm.bat_volt;
    out.data.bat_curr       = shm.bat_curr;
    out.data.bat_soc        = shm.bat_soc;
    out.data.battery_temp   = shm.battery_temp;
    out.data.vehicle_speed  = shm.vehicle_speed;
    out.data.brake          = shm.brake;
    out.data.motor_rpm      = static_cast<int16_t>(shm.motor_rpm);
    out.data.motor_temp     = shm.motor_temp;
    out.data.driver_occupied   = shm.driver_occupied;
    out.data.passenger_occupied = shm.passenger_occupied;
    out.data.driver_buckled     = shm.driver_buckled;
    out.data.passenger_buckled  = shm.passenger_buckled;
    out.data.rear_buckle        = shm.rear_buckle;
    out.data.engine_rpm         = shm.engine_rpm;
    // engine_fault / charge_fault 不在 shm 协议中 → 保持 0
    out.data.charge_status      = shm.charge_status;
    out.data.charge_power       = shm.charge_power;
    out.data.energy_mode        = shm.energy_mode;
    out.data.ev_range           = shm.ev_range;
    out.data.fuel_level         = shm.fuel_level;
    out.data.fuel_range         = shm.fuel_range;
    out.data.gear_status        = shm.gear_status;
    // tire_pressure_* 不在 shm 协议中（PR1 加到 DisplayData）→ 保持 0
    // charge_fault 同上

    // 帧元数据
    out.meta.timestamp_ms = shm.last_commit_ms;
    out.meta.frame_seq = shm.frame_seq;
    out.meta.updated_mask = shm.updated_mask;
    out.meta.dropped_frames = 0;  // 由调用方填

    // 健康（已是 HEALTH_OK，调用方已过滤）
    out.health = HEALTH_OK;

    // 报警（从 shm.alarm_active + alarm_message_zh 解析）
    out.alarm_count = 0;
    if (shm.alarm_active != 0) {
        // 简化：单条报警（shm 当前只传 1 条）
        AlarmEvent& evt = out.alarms[0];
        std::strncpy(evt.name, "active_alarm", sizeof(evt.name) - 1);
        std::strncpy(evt.text_zh, shm.alarm_message_zh, sizeof(evt.text_zh) - 1);
        std::strncpy(evt.text_en, shm.alarm_message_zh, sizeof(evt.text_en) - 1);  // 无 EN fallback
        evt.priority = 0;
        evt.color_r = 0xFF; evt.color_g = 0x44; evt.color_b = 0x00;  // 默认橙红
        out.alarm_count = 1;
    }

    // 安全带状态
    auto setSeat = [&](int idx, bool occupied, bool buckled) {
        out.seat_belt.seats[idx].occupied = occupied;
        out.seat_belt.seats[idx].buckled = buckled;
        out.seat_belt.seats[idx].warning = (occupied && !buckled && shm.vehicle_speed > 5.0f);
    };
    setSeat(SEAT_DRIVER, shm.driver_occupied != 0, shm.driver_buckled != 0);
    setSeat(SEAT_PASSENGER, shm.passenger_occupied != 0, shm.passenger_buckled != 0);
    setSeat(SEAT_REAR_LEFT,   (shm.rear_buckle & 0x01) != 0, (shm.rear_buckle & 0x01) != 0);
    setSeat(SEAT_REAR_CENTER, (shm.rear_buckle & 0x02) != 0, (shm.rear_buckle & 0x02) != 0);
    setSeat(SEAT_REAR_RIGHT,  (shm.rear_buckle & 0x04) != 0, (shm.rear_buckle & 0x04) != 0);
    out.seat_belt.warning_active = false;
    for (int i = 0; i < SEAT_COUNT; i++) {
        if (out.seat_belt.seats[i].warning) {
            out.seat_belt.warning_active = true;
            break;
        }
    }

    // 指示灯（shm 提供 10 个 IND_* 枚举 + 2 个 padding slot）
    for (int i = 0; i < DISPLAY_INDICATOR_COUNT && i < IND_COUNT; i++) {
        out.indicators.lights[i].on = (shm.indicators[i].on != 0);
        out.indicators.lights[i].flash = (shm.indicators[i].flash != 0);
        out.indicators.lights[i].hz = shm.indicators[i].hz_x10 / 10.0f;
    }

    // is_moving
    out.is_moving = (shm.vehicle_speed > 1.0f);
}

// ─── 主题 setter 实现 (PR 7) ───
// 非 inline, 避免 m_theme 类内引用未声明的顺序依赖
void ShmDataSource::resetTripForTest()                  { m_trip.reset(); }
void ShmDataSource::setThemeModeForTest(candash::ThemeMode m) { m_theme.setMode(m); }
void ShmDataSource::setThemeHourForTest(uint8_t h)       { m_theme.setCurrentHour(h); }
void ShmDataSource::setThemeSunriseForTest(uint8_t h)    { m_theme.setSunriseHour(h); }
void ShmDataSource::setThemeSunsetForTest(uint8_t h)     { m_theme.setSunsetHour(h); }
void ShmDataSource::resetThemeForTest()                  { m_theme.reset(); }

// ─── WarningManager setter 实现 (PR 9, 测试用注入) ───
void ShmDataSource::pushWarningForTest(const char* name, uint8_t priority,
                                    uint8_t r, uint8_t g, uint8_t b,
                                    uint64_t now_ms) {
candash::AlarmEvent evt{};
std::strncpy(evt.name, name, sizeof(evt.name) - 1);
std::strncpy(evt.text_zh, "测试告警", sizeof(evt.text_zh) - 1);
std::strncpy(evt.text_en, "test warning", sizeof(evt.text_en) - 1);
evt.priority = priority;
evt.color_r = r; evt.color_g = g; evt.color_b = b;
// 0 表示用 wall clock (跟 onTick 的 shm.last_commit_ms 同一时间基准, 避免 hold 误清)
if (now_ms == 0) now_ms = candash::now_monotonic_ms();
m_warning.pushAlarm(evt, now_ms);
}
void ShmDataSource::tickWarningForTest(uint64_t now_ms)  { m_warning.tick(now_ms); }
void ShmDataSource::resetWarningForTest()                 { m_warning.reset(); }

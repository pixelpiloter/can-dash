// shm_data_source.h
// IDataSource 的共享内存实现
//
// 从 /dev/shm/can_display 读取 v1.2 协议数据，转换为 DisplaySnapshot 推送
// 内部：16ms QTimer 轮询（测试时可注入 MockClock）
//
// 不依赖 Qt UI 概念（仅用 QTimer 作定时器，binder 层不感知）

#pragma once

#include "idata_source.h"
#include "layer1/shm/shm_display.h"  // DisplayDataShm 完整定义（用于 convertSnapshot）
#include "layer2/trip_computer.h"     // 派生指标 (v3 探针延伸, 9b88428)
#include "layer2/theme_manager.h"     // 主题 (PR 7)
#include "layer2/warning_manager.h"   // 警告 (PR 9)
#include "layer2/view_manager.h"      // 视图模式 (PR 13)
#include "layer2/settings_manager.h"  // 用户偏好 (PR 13)
#include "layer2/chime_manager.h"     // 声音提示 (PR 14)
#include <QTimer>
#include <QObject>
#include <atomic>

class ShmDataSource : public QObject, public IDataSource {
    Q_OBJECT
public:
    explicit ShmDataSource(QObject* parent = nullptr);
    ~ShmDataSource() override;

    // IDataSource 接口
    bool start() override;
    void stop() override;
    bool isRunning() const override { return m_running; }
    DisplaySnapshot snapshot() const override { return m_snapshot; }
    HealthStatus health() const override { return m_snapshot.health; }
    void setUpdateCallback(UpdateCallback cb) override { m_updateCb = std::move(cb); }
    void setHealthCallback(HealthCallback cb) override { m_healthCb = std::move(cb); }

    // 测试钩子
    // 注入 tick 间隔（默认 16ms；测试可改为 1ms）
    void setTickIntervalMs(int ms) { m_tickIntervalMs = ms; }

    // 手动触发一次 tick（用于单元测试不依赖定时器）
    void tickForTest() { onTick(); }

    // 重置小计 (Q_INVOKABLE 走 DashboardBackend 暴露给 QML 的 "重置" 按钮)
    void resetTripForTest();

    // ─── 主题 setter (QML 端切换 DAY/NIGHT/AUTO, PR 7) ───
    // 非 inline, 在 .cpp 实现 — 避开 "m_theme 在类内引用未声明" 顺序依赖
    void setThemeModeForTest(candash::ThemeMode mode);
    void setThemeHourForTest(uint8_t hour);
    void setThemeSunriseForTest(uint8_t hour);
    void setThemeSunsetForTest(uint8_t hour);
    void resetThemeForTest();

    // ─── WarningManager setter (PR 9, 测试用注入) ───
    // 生产环境报警由 AlarmRuntime 推入, 测试通过这两个 setter 直接模拟
    void pushWarningForTest(const char* name, uint8_t priority,
                            uint8_t r=0xFF, uint8_t g=0x44, uint8_t b=0x00,
                            uint64_t now_ms=0);
    void tickWarningForTest(uint64_t now_ms);
    void resetWarningForTest();

    // ─── ViewManager setter (PR 13, 测试用注入) ───
    // 生产环境 gear/charge 由 can-processor 推入 shm, ShmDataSource 在 onTick
    // 里把 shm.gear_status/charge_status 桥接到 m_view. 测试用 setter 模拟。
    void setViewGearForTest(uint8_t gear);
    void setViewChargeForTest(uint8_t charge);
    void setViewGearChargeForTest(uint8_t gear, uint8_t charge);  // 一步设
    void tickViewForTest(uint64_t now_ms);
    void resetViewForTest();

    // ─── SettingsManager setter (PR 13, QML 端切换单位/亮度) ───
    // 非 inline, .cpp 实现 — 避开 "m_settings 在类内引用未声明" 顺序依赖
    void setSettingsUnitsForTest(uint8_t units);  // 0=METRIC, 1=IMPERIAL
    void setSettingsBrightnessForTest(uint8_t pct);  // 0-100, 自动 clamp
    void resetSettingsForTest();

    // ─── ChimeManager setter (PR 14, QML 端切换静音/音量) ───
    // 非 inline, .cpp 实现 — 避开 "m_chime 在类内引用未声明" 顺序依赖
    void setChimeEnabledForTest(bool enabled);
    void setChimeVolumeForTest(uint8_t pct);  // 0-100, 自动 clamp
    void resetChimeForTest();

private slots:
    void onTick();

private:
    // ─── 业务转换（把 DisplayDataShm 转为 DisplaySnapshot）───
    void convertSnapshot(const DisplayDataShm& shm, DisplaySnapshot& out) const;

    QTimer* m_timer = nullptr;
    int m_tickIntervalMs = 16;

    std::atomic<bool> m_running{false};
    mutable DisplaySnapshot m_snapshot{};
    HealthStatus m_lastHealth = HEALTH_DISCONNECTED;
    uint64_t m_lastCommitTs = 0;
    uint32_t m_lastFrameSeq = 0;
    uint64_t m_droppedFrames = 0;

    // FPS 滑动窗口
    uint64_t m_fpsWindowStart = 0;
    uint32_t m_fpsCountInWindow = 0;
    double m_fps = 0.0;

    // 派生指标 (v3 探针延伸, commit 9b88428)
    TripComputer m_trip;

    // 主题 (PR 7) — 状态由 ShmDataSource 唯一持有, binder 只读透传
    candash::ThemeManager m_theme;

    // WarningManager (PR 9) — 状态由 ShmDataSource 唯一持有, binder 只读透传
    candash::WarningManager m_warning;

    // ViewManager (PR 13) — 状态由 ShmDataSource 唯一持有, binder 只读透传
    candash::ViewManager m_view;

    // SettingsManager (PR 13) — 状态由 ShmDataSource 唯一持有, binder 只读透传
    // settings 不随时间漂移 (tick no-op), 但保留 16ms 节奏以跟数据流同步
    candash::SettingsManager m_settings;

    // ChimeManager (PR 14) — 状态由 ShmDataSource 唯一持有, binder 只读透传
    // onTick 里桥接 m_warning.currentSeverity() → m_chime.onWarningTriggered()
    // tick 推进 chime.end_ms 超期清除, 跟 shm commit 时间戳同步
    candash::ChimeManager m_chime;
    // 防抖上次触发 severity (避免 onTick 16ms 重复触发同 severity)
    // 0=INFO (静默, 不需触发), 1=WARNING, 2=CRITICAL
    uint8_t  m_lastChimeSeverity = 0;

    // 回调
    UpdateCallback m_updateCb;
    HealthCallback m_healthCb;
};

// qt_data_binder.cpp
// QtDataBinder 实现：DisplaySnapshot → Q_PROPERTY 映射

#include "qt_data_binder.h"
#include "layer1/shm/shm_display.h"  // SHM_FIELD_* 枚举
#include "layer2/language_manager.h"
#include "layer2/time_util.h"

#include <QDebug>
#include <cmath>
#include <cstring>

QtDataBinder::QtDataBinder(QObject* parent) : QObject(parent) {}
QtDataBinder::~QtDataBinder() {}

void QtDataBinder::initLanguage(const LanguageEntry* translations, int translation_count) {
    m_langManager = new LanguageManager();
    m_langManager->init(translations, translation_count);
}

void QtDataBinder::onDataUpdated(const DisplaySnapshot& s) {
    // ─── 1. displayData ───
    QVariantMap newData = buildDisplayData(s.data);
    if (newData != m_displayData) {
        m_displayData = newData;
        emit displayDataChanged();
    }

    // ─── 2. 帧元数据 + 数据健康 ───
    bool dataHealthDirty = false;
    qulonglong age = 0;
    if (m_lastTimestampMs > 0) {
        age = static_cast<qulonglong>(s.meta.timestamp_ms - m_lastTimestampMs);
    }
    m_lastTimestampMs = s.meta.timestamp_ms;
    if (age != m_dataAgeMs) { m_dataAgeMs = age; dataHealthDirty = true; }

    qulonglong curSeq = s.meta.frame_seq;
    if (curSeq != m_frameSeq) { m_frameSeq = curSeq; dataHealthDirty = true; }
    if (s.meta.dropped_frames != m_droppedFrames) {
        m_droppedFrames = s.meta.dropped_frames;
        dataHealthDirty = true;
    }
    QVariantMap validity = buildFieldValidity(s.meta.updated_mask);
    if (validity != m_fieldValidity) {
        m_fieldValidity = validity;
        dataHealthDirty = true;
    }
    if (dataHealthDirty) emit dataHealthChanged();

    // ─── 3. 报警状态 ───
    bool alarmActive = (s.alarm_count > 0);
    QString alarmMsg = alarmActive ? QString::fromUtf8(s.alarms[0].text_zh) : QString();
    bool alarmChanged = false;
    if (alarmActive != m_alarmActive) { m_alarmActive = alarmActive; alarmChanged = true; }
    if (alarmMsg != m_alarmMessageZh) { m_alarmMessageZh = alarmMsg; alarmChanged = true; }
    if (alarmActive) {
        QVariantList newList = buildAlarmList(s);
        if (newList != m_alarmList) { m_alarmList = newList; alarmChanged = true; }
    } else if (!m_alarmList.isEmpty()) {
        m_alarmList.clear();
        alarmChanged = true;
    }
    if (alarmChanged) emit alarmActiveChanged();

    // ─── 4. 安全带状态 ───
    QVariantList newSeatStates = buildSeatIconStates(s.seat_belt);
    QString seatMsg = buildSeatBeltMessage(s.seat_belt);
    bool seatChanged = false;
    if (newSeatStates != m_seatIconStates) { m_seatIconStates = newSeatStates; seatChanged = true; }
    if (s.seat_belt.warning_active != m_seatBeltActive) { m_seatBeltActive = s.seat_belt.warning_active; seatChanged = true; }
    if (seatMsg != m_seatBeltMessage) { m_seatBeltMessage = seatMsg; seatChanged = true; }
    if (seatChanged) emit seatBeltChanged();

    // ─── 5. 指示灯 ───
    QVariantMap indStates = buildIndicatorStates(s.indicators);
    if (indStates != m_indicatorStates) {
        m_indicatorStates = indStates;
        emit indicatorStatesChanged();
    }

    // ─── 6. is_moving ───
    if (s.is_moving != m_isMoving) {
        m_isMoving = s.is_moving;
        emit movingChanged();
    }

    // ─── 7. 派生指标: TripComputer (v3 探针延伸) ───
    // 用 dirty flag 避免每 16ms 重复 emit (QML 端会反复触发 Binding 求值)
    bool tripDirty = false;
    if (s.trip_distance_km            != m_tripDistanceKm)            { m_tripDistanceKm            = s.trip_distance_km;            tripDirty = true; }
    if (s.trip_avg_speed_kmh          != m_tripAvgSpeedKmh)          { m_tripAvgSpeedKmh          = s.trip_avg_speed_kmh;          tripDirty = true; }
    if (s.trip_duration_s             != m_tripDurationS)             { m_tripDurationS             = s.trip_duration_s;             tripDirty = true; }
    if (s.trip_is_moving              != m_tripIsMoving)              { m_tripIsMoving              = s.trip_is_moving;              tripDirty = true; }
    if (s.trip_energy_kwh             != m_tripEnergyKWh)             { m_tripEnergyKWh             = s.trip_energy_kwh;             tripDirty = true; }
    if (s.trip_efficiency_kwh100km    != m_tripEfficiencyKWh100Km)    { m_tripEfficiencyKWh100Km    = s.trip_efficiency_kwh100km;   tripDirty = true; }
    if (s.trip_range_confidence_pct   != m_tripRangeConfidencePct)   { m_tripRangeConfidencePct    = s.trip_range_confidence_pct;  tripDirty = true; }
    if (tripDirty) emit tripChanged();

    // ─── 8. 主题 (PR 7) — dirty flag, mode 或 5 色任一变化才 emit ───
    // 避免 16ms 重复 emit 触发 QML Binding 反复求值
    bool themeDirty = false;
    if (s.theme_mode                != m_themeMode)                { m_themeMode                = s.theme_mode;                themeDirty = true; }
    if (s.theme_is_day              != m_themeIsDay)               { m_themeIsDay              = s.theme_is_day;               themeDirty = true; }
    if (s.theme_color_background    != m_themeColorBackground)     { m_themeColorBackground    = s.theme_color_background;     themeDirty = true; }
    if (s.theme_color_foreground    != m_themeColorForeground)     { m_themeColorForeground    = s.theme_color_foreground;     themeDirty = true; }
    if (s.theme_color_accent        != m_themeColorAccent)         { m_themeColorAccent        = s.theme_color_accent;         themeDirty = true; }
    if (s.theme_color_warning       != m_themeColorWarning)        { m_themeColorWarning       = s.theme_color_warning;        themeDirty = true; }
    if (s.theme_color_critical      != m_themeColorCritical)       { m_themeColorCritical      = s.theme_color_critical;       themeDirty = true; }
    if (themeDirty) emit themeChanged();
}

void QtDataBinder::onHealthChanged(HealthStatus new_health) {
    bool newOnline = (new_health == HEALTH_OK);
    QString newStatus = QString::fromUtf8(health_status_str(new_health));
    if (newOnline != m_processorOnline || newStatus != m_processorStatus) {
        m_processorOnline = newOnline;
        m_processorStatus = newStatus;
        qWarning() << "[QtDataBinder] Health:" << newStatus;
        emit healthChanged();
    }
}

QVariantMap QtDataBinder::buildDisplayData(const DisplayData& d) const {
    QVariantMap m;
    m["bat_volt"] = d.bat_volt;
    m["bat_curr"] = d.bat_curr;
    m["bat_soc"] = d.bat_soc;
    m["battery_temp"] = d.battery_temp;
    m["vehicle_speed"] = d.vehicle_speed;
    m["brake"] = d.brake;
    m["motor_rpm"] = d.motor_rpm;
    m["motor_temp"] = d.motor_temp;
    m["driver_occupied"] = d.driver_occupied;
    m["passenger_occupied"] = d.passenger_occupied;
    m["driver_buckled"] = d.driver_buckled;
    m["passenger_buckled"] = d.passenger_buckled;
    m["rear_buckle"] = d.rear_buckle;
    m["engine_rpm"] = d.engine_rpm;
    m["engine_fault"] = d.engine_fault;
    m["charge_status"] = d.charge_status;
    m["charge_fault"] = d.charge_fault;
    m["charge_power"] = d.charge_power;
    m["energy_mode"] = d.energy_mode;
    m["ev_range"] = d.ev_range;
    m["fuel_level"] = d.fuel_level;
    m["fuel_range"] = d.fuel_range;
    m["gear_status"] = d.gear_status;
    m["tire_pressure_fl"] = d.tire_pressure_fl;
    m["tire_pressure_fr"] = d.tire_pressure_fr;
    m["tire_pressure_rl"] = d.tire_pressure_rl;
    m["tire_pressure_rr"] = d.tire_pressure_rr;
    return m;
}

QVariantMap QtDataBinder::buildFieldValidity(uint32_t updated_mask) const {
    static const struct { int bit; const char* name; } kFieldMap[] = {
        {SHM_FIELD_MOTOR_RPM,         "motor_rpm"},
        {SHM_FIELD_VEHICLE_SPEED,     "vehicle_speed"},
        {SHM_FIELD_BAT_VOLT,          "bat_volt"},
        {SHM_FIELD_BAT_CURR,          "bat_curr"},
        {SHM_FIELD_BAT_SOC,           "bat_soc"},
        {SHM_FIELD_MOTOR_TEMP,        "motor_temp"},
        {SHM_FIELD_BRAKE,             "brake"},
        {SHM_FIELD_DRIVER_OCCUPIED,   "driver_occupied"},
        {SHM_FIELD_PASSENGER_OCCUPIED,"passenger_occupied"},
        {SHM_FIELD_DRIVER_BUCKLED,    "driver_buckled"},
        {SHM_FIELD_PASSENGER_BUCKLED, "passenger_buckled"},
        {SHM_FIELD_REAR_BUCKLE,       "rear_buckle"},
    };
    QVariantMap m;
    for (const auto& f : kFieldMap) {
        m[f.name] = (updated_mask & (1U << f.bit)) != 0;
    }
    return m;
}

QVariantList QtDataBinder::buildAlarmList(const DisplaySnapshot& s) const {
    QVariantList list;
    for (uint8_t i = 0; i < s.alarm_count; i++) {
        const AlarmEvent& e = s.alarms[i];
        QVariantMap m;
        m["name"] = QString::fromUtf8(e.name);
        m["text_zh"] = QString::fromUtf8(e.text_zh);
        m["text_en"] = QString::fromUtf8(e.text_en);
        m["priority"] = e.priority;
        m["color"] = QString("#%1%2%3")
            .arg(e.color_r, 2, 16, QChar('0'))
            .arg(e.color_g, 2, 16, QChar('0'))
            .arg(e.color_b, 2, 16, QChar('0'));
        list.append(m);
    }
    return list;
}

QVariantList QtDataBinder::buildSeatIconStates(const SeatBeltState& sb) const {
    static const char* kPosKeys[SEAT_COUNT] = {
        "seatbelt.driver", "seatbelt.passenger",
        "seatbelt.rear_left", "seatbelt.rear_center", "seatbelt.rear_right"
    };
    QVariantList list;
    for (int i = 0; i < SEAT_COUNT; i++) {
        QVariantMap m;
        m["id"] = QString::fromUtf8(kPosKeys[i]);
        m["occupied"] = sb.seats[i].occupied;
        m["buckled"] = sb.seats[i].buckled;
        m["warning"] = sb.seats[i].warning;
        list.append(m);
    }
    return list;
}

QVariantMap QtDataBinder::buildIndicatorStates(const IndicatorStates& ind) const {
    // 指示灯
    static const char* kIndKeys[DISPLAY_INDICATOR_COUNT] = {
        "left_turn_light", "right_turn_light", "park_brake_light",
        "ready_go_light", "bat_warn_light", "check_engine_light",
        "high_voltage_light", "fog_light",
        "seatbelt_warning", "tire_pressure_light",
        "extra10", "extra11"  // 预留 2 个槽位
    };
    QVariantMap m;
    for (int i = 0; i < DISPLAY_INDICATOR_COUNT; i++) {
        m[QString::fromUtf8(kIndKeys[i])] = indicatorSlotToMap(ind.lights[i]);
    }
    return m;
}

QString QtDataBinder::buildSeatBeltMessage(const SeatBeltState& sb) const {
    if (!sb.warning_active) return QString();
    bool isEn = (m_currentLanguage == "en_US");
    QStringList positions;
    static const char* kPosKeys[SEAT_COUNT] = {
        "seatbelt.driver", "seatbelt.passenger",
        "seatbelt.rear_left", "seatbelt.rear_center", "seatbelt.rear_right"
    };
    for (int i = 0; i < SEAT_COUNT; i++) {
        if (sb.seats[i].warning) {
            const char* label = m_langManager ? m_langManager->tr(kPosKeys[i]) : kPosKeys[i];
            positions.append(QString::fromUtf8(label));
        }
    }
    if (positions.isEmpty()) return QString();
    QString positionsStr = positions.join(",");
    return isEn
        ? QString::fromUtf8("%1 please buckle up").arg(positionsStr)
        : QString::fromUtf8("%1请系安全带").arg(positionsStr);
}

QVariantMap QtDataBinder::indicatorSlotToMap(const IndicatorState& s) const {
    QVariantMap m;
    m["on"] = s.on;
    m["flash"] = s.flash;
    m["hz"] = s.hz;
    return m;
}

// ─── QML 接口 ───
QVariant QtDataBinder::get(const QString& key) const {
    return m_displayData.value(key);
}

void QtDataBinder::set(const QString& key, const QVariant& value) {
    Q_UNUSED(key); Q_UNUSED(value);
}

bool QtDataBinder::indicatorOn(const QString& key) const {
    return m_indicatorStates.value(key).toMap().value("on", false).toBool();
}

void QtDataBinder::setIndicator(const QString& widget_id, bool on, bool flash, float hz) {
    QVariantMap state;
    state["on"] = on;
    state["flash"] = flash;
    state["hz"] = hz;
    m_indicatorStates[widget_id] = state;
    emit indicatorStatesChanged();
}

QString QtDataBinder::tr(const QString& key) const {
    if (!m_langManager) return key;
    return QString::fromUtf8(m_langManager->tr(key.toUtf8().constData()));
}

void QtDataBinder::setLanguage(const QString& lang) {
    Language newLang = (lang == "en_US") ? LANG_EN_US : LANG_ZH_CN;
    if (m_langManager) {
        m_langManager->setLanguage(newLang);
        m_currentLanguage = lang;
        emit languageChanged();
    }
}

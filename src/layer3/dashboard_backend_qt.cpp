// dashboard_backend_qt.cpp
// Layer 3: Qt 适配层 - DashboardBackend（仅显示，从共享内存读取）
// 从共享内存读取displayData，暴露给QML

#include "dashboard_backend_qt.h"
#include "layer1/shm/shm_display.h"
#include "layer2/language_manager.h"

#include <QTimer>
#include <QDebug>

// ─── 构造函数/析构 ───────────────────────────────────────
DashboardBackend::DashboardBackend(QObject* parent)
    : QObject(parent) {}

DashboardBackend::~DashboardBackend() {
    shm_display_close();
}

// ─── 初始化 ───────────────────────────────────────────────
void DashboardBackend::init() {
    // ─── 打开共享内存（读）──────────────────────────────
    if (shm_display_open() < 0) {
        qWarning() << "[Dashboard] Failed to open shared memory at" << SHM_DISPLAY_PATH;
    } else {
        qDebug() << "[Dashboard] Opened shared memory at" << SHM_DISPLAY_PATH;
    }

    // ─── 语言管理器 ─────────────────────────────────────
    m_langManager = new LanguageManager();
    m_langManager->init(TRANSLATIONS, TRANSLATION_COUNT);

    // ─── 定时器（~60fps = 16ms）──────────────────────────
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &DashboardBackend::onTick);
    m_tickTimer->start(16);

    qDebug() << "DashboardBackend::init() done";
}

// ─── 定时 tick ────────────────────────────────────────────
void DashboardBackend::onTick() {
    static uint64_t last_ts = 0;
    static bool ever_connected = false;

    uint64_t ts = shm_display_poll(last_ts);
    if (ts == last_ts) return;  // 无变化
    last_ts = ts;

    DisplayDataShm data = {};
    shm_display_read(&data);

    if (!ever_connected) {
        qDebug() << "[Dashboard] Processor connected (timestamp:" << ts << ")";
        ever_connected = true;
    }

    // ─── 更新 displayData ─────────────────────────
    QVariantMap newData;
    newData["motor_rpm"] = data.motor_rpm;
    newData["vehicle_speed"] = data.vehicle_speed;
    newData["bat_volt"] = data.bat_volt;
    newData["bat_curr"] = data.bat_curr;
    newData["bat_soc"] = data.bat_soc;
    newData["motor_temp"] = data.motor_temp;
    newData["brake"] = data.brake;
    newData["driver_occupied"] = data.driver_occupied;
    newData["passenger_occupied"] = data.passenger_occupied;
    newData["driver_buckled"] = data.driver_buckled;
    newData["passenger_buckled"] = data.passenger_buckled;
    newData["rear_buckle"] = data.rear_buckle;
    m_displayData = newData;
    emit displayDataChanged();

    // ─── 更新报警状态 ──────────────────────────────
    bool alarmActive = data.alarm_active != 0;
    QString alarmMsg = QString::fromUtf8(data.alarm_message_zh);
    bool changed = false;
    if (alarmActive != m_backendAlarmActive) {
        m_backendAlarmActive = alarmActive;
        changed = true;
    }
    if (alarmMsg != m_backendAlarmMessageZh) {
        m_backendAlarmMessageZh = alarmMsg;
        changed = true;
    }
    if (changed) emit alarmActiveChanged();

    // ─── 更新指示灯状态 ───────────────────────────
    QVariantMap indStates;
    indStates["left_turn_light"] = indicatorSlotToMap(data.indicators[IND_LEFT_TURN]);
    indStates["right_turn_light"] = indicatorSlotToMap(data.indicators[IND_RIGHT_TURN]);
    indStates["park_brake_light"] = indicatorSlotToMap(data.indicators[IND_PARK_BRAKE]);
    indStates["ready_go_light"] = indicatorSlotToMap(data.indicators[IND_READY_GO]);
    indStates["bat_warn_light"] = indicatorSlotToMap(data.indicators[IND_BAT_WARN]);
    indStates["check_engine_light"] = indicatorSlotToMap(data.indicators[IND_ENGINE]);
    indStates["high_voltage_light"] = indicatorSlotToMap(data.indicators[IND_HIGH_VOLT]);
    indStates["fog_light"] = indicatorSlotToMap(data.indicators[IND_FOG_LIGHT]);
    indStates["seatbelt_warning"] = indicatorSlotToMap(data.indicators[IND_SEATBELT]);
    indStates["tire_pressure_light"] = indicatorSlotToMap(data.indicators[IND_TIRE_PRESSURE]);
    if (indStates != m_indicatorStates) {
        m_indicatorStates = indStates;
        emit indicatorStatesChanged();
    }

    // ─── 更新 isMoving ──────────────────────────────
    bool moving = data.vehicle_speed > 1.0f;
    if (moving != m_isMoving) {
        m_isMoving = moving;
        emit movingChanged();
    }
}

// ─── 工具 ────────────────────────────────────────────────
QVariantMap DashboardBackend::indicatorSlotToMap(const ShmIndicatorSlot& slot) const {
    QVariantMap m;
    m["on"] = slot.on != 0;
    m["flash"] = slot.flash != 0;
    m["hz"] = slot.hz_x10 / 10.0f;
    return m;
}

// ─── QML 接口 ─────────────────────────────────────────────
QVariant DashboardBackend::get(const QString& key) const {
    return m_displayData.value(key);
}

void DashboardBackend::set(const QString& key, const QVariant& value) {
    Q_UNUSED(key); Q_UNUSED(value);
}

// ─── 多语言 ────────────────────────────────────────────────
QString DashboardBackend::tr(const QString& key) const {
    if (!m_langManager) return key;
    const char* result = m_langManager->tr(key.toUtf8().constData());
    return QString::fromUtf8(result);
}

void DashboardBackend::setLanguage(const QString& lang) {
    Language newLang = (lang == "en_US") ? LANG_EN_US : LANG_ZH_CN;
    if (m_langManager) {
        m_langManager->setLanguage(newLang);
        m_currentLanguage = lang;
        emit languageChanged();
    }
}

QString DashboardBackend::currentFont() const {
    if (!m_langManager) return "Noto Sans CJK SC, sans-serif";
    return QString::fromUtf8(m_langManager->currentFontFamily());
}

// ─── 指示灯查询 ───────────────────────────────────────────
bool DashboardBackend::indicatorOn(const QString& key) const {
    return m_indicatorStates.value(key).toMap().value("on", false).toBool();
}

// ─── 指示灯控制（来自 QML）───────────────────────────────
void DashboardBackend::setIndicator(const QString& widget_id, bool on, bool flash, float hz) {
    QVariantMap state;
    state["on"] = on;
    state["flash"] = flash;
    state["hz"] = hz;
    m_indicatorStates[widget_id] = state;
    emit indicatorStatesChanged();
}

void DashboardBackend::setIndicator(const QString& widget_id, bool on) {
    setIndicator(widget_id, on, false, 0.0f);
}

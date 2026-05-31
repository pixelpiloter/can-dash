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

    // ─── 填充 alarmList（供 AlarmBanner QML 组件使用）───
    if (alarmActive && !alarmMsg.isEmpty()) {
        // 解析 alarm_message_zh，格式："name:text_zh;text_en|..."
        // 目前 shared memory 只有一条消息，用 "||" 分隔多个报警
        QStringList alarms = alarmMsg.split("||", Qt::SkipEmptyParts);
        QVariantList newList;
        for (const QString& entry : alarms) {
            QStringList parts = entry.split(":", Qt::SkipEmptyParts);
            QString name, text;
            if (parts.size() >= 2) {
                name = parts[0].trimmed();
                text = parts[1].trimmed();
            } else {
                text = entry.trimmed();
            }
            // 根据报警名称确定颜色和字体大小（默认值）
            QString color = "#FF4400";
            int fontSize = 28;
            if (name.contains("soc") || name.contains("电量") || name.contains("fuel") || name.contains("燃油")) {
                color = "#FFAA00";  // 黄色：中等优先级
                fontSize = 28;
            } else if (name.contains("critical") || name.contains("严重") || name.contains("soc_critical")) {
                color = "#FF0000";  // 红色：紧急
                fontSize = 32;
            }
            QVariantMap alarm;
            alarm["text_zh"] = text;
            alarm["text_en"] = text;  // backend 无现成英文，用中文代替
            alarm["color"] = color;
            alarm["font_size"] = fontSize;
            newList.append(alarm);
        }
        if (newList != m_alarmList) {
            m_alarmList = newList;
            changed = true;
        }
    } else {
        if (!m_alarmList.isEmpty()) {
            m_alarmList.clear();
            changed = true;
        }
    }

    if (changed) emit alarmActiveChanged();

    // ─── 更新安全带状态 ────────────────────────────
    // 座位顺序: driver=0, passenger=1, rear_left=2, rear_center=3, rear_right=4
    // rear_buckle bit: bit0=后左, bit1=后中, bit2=后右
    QVariantList newSeatStates;
    bool seatBeltWarning = false;
    QStringList unbuckledPositions;

    auto evalSeat = [&](int idx, bool occupied, bool buckled, const char* posKey) {
        bool warning = false;
        if (occupied && !buckled && data.vehicle_speed > 5.0f) {
            warning = true;
            seatBeltWarning = true;
            const char* label = m_langManager->tr(posKey);
            unbuckledPositions.append(QString::fromUtf8(label));
        }
        QVariantMap seat;
        seat["id"] = QString::fromUtf8(posKey);
        seat["occupied"] = occupied;
        seat["buckled"] = buckled;
        seat["warning"] = warning;
        newSeatStates.append(seat);
    };

    evalSeat(0, data.driver_occupied != 0, data.driver_buckled != 0, "seatbelt.driver");
    evalSeat(1, data.passenger_occupied != 0, data.passenger_buckled != 0, "seatbelt.passenger");
    evalSeat(2, (data.rear_buckle & 0x01) != 0, (data.rear_buckle & 0x01) != 0, "seatbelt.rear_left");
    evalSeat(3, (data.rear_buckle & 0x02) != 0, (data.rear_buckle & 0x02) != 0, "seatbelt.rear_center");
    evalSeat(4, (data.rear_buckle & 0x04) != 0, (data.rear_buckle & 0x04) != 0, "seatbelt.rear_right");

    // 生成 i18n 报警消息
    QString seatBeltMsg;
    if (!unbuckledPositions.isEmpty()) {
        QString positionsStr = unbuckledPositions.join(",");
        bool isZh = (m_langManager->currentLocale() != nullptr &&
                      strcmp(m_langManager->currentLocale(), "en_US") == 0);
        if (unbuckledPositions.size() == 1) {
            seatBeltMsg = isZh
                ? QString::fromUtf8("%1请系安全带").arg(positionsStr)
                : QString::fromUtf8("%1 please buckle up").arg(positionsStr);
        } else {
            seatBeltMsg = isZh
                ? QString::fromUtf8("%1请系安全带").arg(positionsStr)
                : QString::fromUtf8("%1 please buckle up").arg(positionsStr);
        }
    }

    bool seatChanged = false;
    if (newSeatStates != m_seatIconStates) {
        m_seatIconStates = newSeatStates;
        seatChanged = true;
    }
    if (seatBeltWarning != m_seatBeltActive) {
        m_seatBeltActive = seatBeltWarning;
        seatChanged = true;
    }
    if (seatBeltMsg != m_seatBeltMessage) {
        m_seatBeltMessage = seatBeltMsg;
        seatChanged = true;
    }
    if (seatChanged) emit seatBeltWarningChanged();

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

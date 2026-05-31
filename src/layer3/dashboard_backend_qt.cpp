// dashboard_backend_qt.cpp
// Layer 3: Qt 适配层实现

#include "dashboard_backend_qt.h"
#include "../layer2/can_converter.h"
#include "../layer2/alarm_runtime.h"
#include "../layer2/seat_belt_runtime.h"
#include "../layer2/indicator_runtime.h"
#include "../layer2/language_manager.h"

#include "../generated/can_field_def.h"
#include "../generated/alarm_rule_def.h"
#include "../generated/seat_belt_def.h"
#include "../generated/indicator_def.h"

#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QLocalSocket>

DashboardBackend::DashboardBackend(QObject* parent)
    : QObject(parent) {}

DashboardBackend::~DashboardBackend() = default;

void DashboardBackend::init() {
    // ─── 初始化 Layer 2 Runtime ───
    m_converter = new CanConverter();
    m_converter->init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);

    // 报警 Runtime
    AlarmCallbacks alarmCb = {};
    m_alarmRuntime = new AlarmRuntime(alarmCb);
    m_alarmRuntime->init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                         ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);

    // 安全带 Runtime
    m_seatBeltRuntime = new SeatBeltRuntime();
    m_seatBeltRuntime->init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT,
                             &SEAT_BELT_CONFIG);

    // 指示灯 Runtime
    m_indicatorRuntime = new IndicatorRuntime(IndicatorCallbacks{});
    m_indicatorRuntime->init(INDICATOR_TABLE, INDICATOR_TABLE_COUNT);

    // 语言管理器（默认中文）
    m_langManager = new LanguageManager();
    m_currentLang = "zh_CN";

    // ─── 定时器（~100ms）───
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &DashboardBackend::onTick);
    m_tickTimer->start(100);

    qDebug() << "DashboardBackend::init() done";

    // ─── 启动 Unix Socket 服务器 ───
    startSocketServer();
}

// ─── 语言切换 ───
QString DashboardBackend::currentLanguage() const {
    return m_currentLang;
}

void DashboardBackend::setLanguage(const QString& lang) {
    if (lang == "zh_CN" || lang == "en_US") {
        m_currentLang = lang;
        m_langManager->setLanguage(lang == "zh_CN" ? LANG_ZH_CN : LANG_EN_US);
        emit languageChanged();
        qDebug() << "[i18n] Language switched to:" << lang;
    }
}

QString DashboardBackend::currentFont() const {
    return QString::fromUtf8(m_langManager->currentFontFamily());
}

QString DashboardBackend::tr(const QString& key) const {
    return QString::fromUtf8(m_langManager->tr(key.toUtf8().constData()));
}

// ─── 指示灯控制 ───
void DashboardBackend::setIndicator(const QString& id, bool on, bool flash, float hz) {
    m_indicatorRuntime->setIndicator(id.toUtf8().constData(), on, flash, hz);

    // 更新本地缓存供 QML 读取
    QVariantMap state;
    state["on"] = on;
    state["flash"] = flash;
    state["flashHz"] = hz;
    m_indicatorStates[id] = state;
    emit indicatorStatesChanged();
}

bool DashboardBackend::indicatorOn(const QString& id) const {
    return m_indicatorRuntime->isOn(id.toUtf8().constData());
}

void DashboardBackend::onCanFrameReceived(quint32 canId, const QByteArray& data) {
    DisplayData dd = {};
    memset(&dd, 0, sizeof(DisplayData));
    uint32_t updatedMask = m_converter->processFrame(
        canId,
        reinterpret_cast<const uint8_t*>(data.constData()),
        data.size(),
        dd
    );

    if (updatedMask == 0) {
        return;
    }

    // 更新显示数据：只更新本帧实际携带的字段
    QVariantMap newData = m_displayData;
    bool has_bat_volt  = false, has_bat_curr  = false, has_bat_soc  = false;
    bool has_speed     = false, has_rpm        = false, has_motor_temp = false;
    for (int i = 0; i < m_converter->fieldCount(); i++) {
        if ((updatedMask & (1U << i)) == 0) continue;
        const CanFieldDef* def = &m_converter->fieldTable()[i];
        if (strcmp(def->display_key, "bat_volt") == 0)       has_bat_volt  = true;
        if (strcmp(def->display_key, "bat_curr") == 0)       has_bat_curr  = true;
        if (strcmp(def->display_key, "bat_soc") == 0)        has_bat_soc   = true;
        if (strcmp(def->display_key, "vehicle_speed") == 0)  has_speed     = true;
        if (strcmp(def->display_key, "motor_rpm") == 0)       has_rpm       = true;
        if (strcmp(def->display_key, "motor_temp") == 0)     has_motor_temp = true;
    }
    if (has_bat_volt)   newData["bat_volt"] = dd.bat_volt;
    if (has_bat_curr)   newData["bat_curr"] = dd.bat_curr;
    if (has_bat_soc)    newData["bat_soc"] = dd.bat_soc;
    if (has_speed)      newData["vehicle_speed"] = dd.vehicle_speed;
    if (has_rpm)        newData["motor_rpm"] = dd.motor_rpm;
    if (has_motor_temp) newData["motor_temp"] = dd.motor_temp;
    m_displayData = newData;
    static int tick_count = 0;
    if (++tick_count % 20 == 0) {
        qDebug() << "CAN: speed=" << dd.vehicle_speed << " bat_volt=" << dd.bat_volt
                 << " soc=" << dd.bat_soc << " rpm=" << dd.motor_rpm;
    }
    emit displayDataChanged();
}

void DashboardBackend::onTick() {
    uint64_t nowMs = QDateTime::currentMSecsSinceEpoch();

    // ─── 读取所有可用的 CAN 帧 ───
    if (m_socketConnection && m_socketConnection->bytesAvailable() > 0) {
        QByteArray allData = m_socketConnection->readAll();
        int offset = 0;
        while (offset + 5 <= allData.size()) {
            quint32 canId;
            memcpy(&canId, allData.constData() + offset, 4);
            quint8 dlc = static_cast<quint8>(allData[offset + 4]);
            int frameSize = 5 + dlc;
            if (offset + frameSize > allData.size()) break;
            QByteArray frameData = allData.mid(offset + 5, dlc);
            offset += frameSize;
            onCanFrameReceived(canId, frameData);
        }
    }

    // 驱动所有 Runtime tick
    m_alarmRuntime->tick(nowMs);
    m_seatBeltRuntime->tick(nowMs);
    m_indicatorRuntime->tick(nowMs);

    // 更新行驶状态
    bool moving = m_displayData.value("vehicle_speed", 0.0f).toFloat() > 1.0f;
    if (m_isMoving != moving) {
        m_isMoving = moving;
        emit movingChanged();
    }

    // 更新安全带状态
    updateSeatBeltStates();
}

void DashboardBackend::updateSeatBeltStates() {
    SeatBeltQueryResult result = {};
    m_seatBeltRuntime->query(result);

    if (m_seatBeltActive != result.anyWarning) {
        m_seatBeltActive = result.anyWarning;
        emit seatBeltWarningChanged();
    }

    if (result.anyWarning) {
        m_seatBeltMessage = QString::fromUtf8(result.messageZh);
        emit seatBeltWarningChanged();
    }

    QVariantList states;
    const auto& sbStates = m_seatBeltRuntime->states();
    for (int i = 0; i < sbStates.seatCount; i++) {
        QVariantMap seat;
        seat["id"] = sbStates.seats[i].positionId;
        seat["buckled"] = sbStates.seats[i].beltBuckled;
        seat["occupied"] = sbStates.seats[i].seatOccupied;
        seat["warning"] = sbStates.seats[i].warning;
        seat["hint"] = sbStates.seats[i].hint;
        states.append(seat);
    }
    m_seatIconStates = states;
    emit seatIconStatesChanged();
}

QVariant DashboardBackend::get(const QString& key) const {
    return m_displayData.value(key);
}

void DashboardBackend::set(const QString& key, const QVariant& value) {
    Q_UNUSED(key);
    Q_UNUSED(value);
}

void DashboardBackend::startSocketServer() {
    QLocalServer::removeServer("/tmp/can_dash_socket");

    m_socketServer = new QLocalServer(this);
    if (!m_socketServer->listen("/tmp/can_dash_socket")) {
        qWarning() << "Failed to listen on /tmp/can_dash_socket:" << m_socketServer->errorString();
        return;
    }
    qDebug() << "[SocketServer] Listening on /tmp/can_dash_socket";
}

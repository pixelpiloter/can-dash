// dashboard_backend_qt.cpp
// Layer 3: Qt 适配层实现
// 组合所有 Layer 2 Runtime，暴露 Q_PROPERTY 给 QML

#include "dashboard_backend_qt.h"
#include "../layer2/can_converter.h"
#include "../layer2/alarm_runtime.h"
#include "../layer2/seat_belt_runtime.h"
#include "../layer2/indicator_runtime.h"

#include "../generated/can_field_def.h"
#include "../generated/alarm_rule_def.h"
#include "../generated/seat_belt_def.h"
#include "../generated/indicator_def.h" // INDICATOR_TABLE, INDICATOR_TABLE_COUNT

#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QLocalSocket>

// ─── 常量 ────────────────────────────────────────────────
static const char* SOCKET_PATH = "/tmp/can_dash_socket";

// ─── 构造/析构 ────────────────────────────────────────────
DashboardBackend::DashboardBackend(QObject* parent)
    : QObject(parent) {}

DashboardBackend::~DashboardBackend() {
    if (m_socketServer) {
        m_socketServer->close();
        delete m_socketServer;
    }
    if (m_socketConnection) {
        m_socketConnection->deleteLater();
    }
}

// ─── 初始化 ───────────────────────────────────────────────
void DashboardBackend::init() {
    // ─── Layer 2 组件初始化 ───
    m_converter = new CanConverter();
    m_converter->init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);

    // 报警 Runtime
    AlarmCallbacks alarmCb = {};
    alarmCb.onIndicatorUpdate = [](const char* widget_id, bool on, bool flash,
                                    float flash_hz, void* user_data) {
        auto* self = static_cast<DashboardBackend*>(user_data);
        self->setIndicator(QString::fromUtf8(widget_id), on, flash, flash_hz);
    };
    alarmCb.onAlarmTextUpdate = [](const char* text_zh, const char* text_en,
                                    void* user_data) {
        auto* self = static_cast<DashboardBackend*>(user_data);
        Q_UNUSED(text_en);
        self->m_backendAlarmMessageZh = QString::fromUtf8(text_zh);
        self->m_backendAlarmActive = true;
        emit self->alarmActiveChanged();
    };
    alarmCb.onAlarmStateChanged = [](const char* alarm_name, bool active,
                                      void* user_data) {
        auto* self = static_cast<DashboardBackend*>(user_data);
        if (active) {
            self->m_backendAlarmActive = true;
            self->m_backendAlarmMessageZh = QString::fromUtf8(alarm_name);
        } else {
            self->m_backendAlarmMessageZh = QString();
            self->m_backendAlarmActive = false;
        }
        emit self->alarmActiveChanged();
    };
    alarmCb.user_data = this;
    m_alarmRuntime = new AlarmRuntime(alarmCb);
    m_alarmRuntime->init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                          ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);

    // 安全带 Runtime
    m_seatBeltRuntime = new SeatBeltRuntime();
    m_seatBeltRuntime->init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT,
                             &SEAT_BELT_CONFIG);

    // 指示灯 Runtime
    IndicatorCallbacks indicatorCb = {};
    indicatorCb.onStateChange = [](const char* id, bool on, bool flash,
                                    float hz, void* user_data) {
        auto* self = static_cast<DashboardBackend*>(user_data);
        QString key = QString::fromUtf8(id);
        QVariantMap state;
        state["on"] = on;
        state["flash"] = flash;
        state["hz"] = hz;
        self->m_indicatorStates[key] = state;
        emit self->indicatorStatesChanged();
    };
    indicatorCb.user_data = this;
    m_indicatorRuntime = new IndicatorRuntime(indicatorCb);
    m_indicatorRuntime->init(INDICATOR_TABLE, INDICATOR_TABLE_COUNT);

    // ─── Unix Socket 服务器 ───
    m_socketServer = new QLocalServer(this);
    QLocalServer::removeServer(SOCKET_PATH);
    if (!m_socketServer->listen(SOCKET_PATH)) {
        qWarning() << "[SocketServer] Failed to listen:" << m_socketServer->errorString();
    } else {
        qDebug() << "[SocketServer] Listening on" << SOCKET_PATH;
    }
    connect(m_socketServer, &QLocalServer::newConnection,
            this, &DashboardBackend::onNewConnection);

    // ─── 定时器（~100ms）───
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &DashboardBackend::onTick);
    m_tickTimer->start(100);

    qDebug() << "DashboardBackend::init() done";
}

// ─── 定时 tick ────────────────────────────────────────────
void DashboardBackend::onTick() {
    uint64_t nowMs = QDateTime::currentMSecsSinceEpoch();

    // 轮询 socket 连接接受
    while (m_socketServer->hasPendingConnections()) {
        QLocalSocket* client = m_socketServer->nextPendingConnection();
        if (m_socketConnection) {
            m_socketConnection->disconnectFromServer();
            m_socketConnection->deleteLater();
        }
        m_socketConnection = client;
        connect(m_socketConnection, &QLocalSocket::readyRead,
                this, &DashboardBackend::onSocketReadyRead);
        connect(m_socketConnection, &QLocalSocket::disconnected,
                this, [this]() {
            qDebug() << "[SocketServer] Client disconnected";
            m_socketConnection = nullptr;
        });
        qDebug() << "[SocketServer] Client connected";
    }

    // 轮询 socket 数据
    if (m_socketConnection && m_socketConnection->isValid()) {
        while (m_socketConnection->bytesAvailable() > 0) {
            QByteArray data = m_socketConnection->read(256);
            if (!data.isEmpty()) {
                handleCanFrameData(data, 0);
            }
        }
    }

    // 驱动 Runtime tick
    m_alarmRuntime->tick(nowMs);
    m_seatBeltRuntime->tick(nowMs);

    // 更新 isMoving
    float speed = m_displayData.value("vehicle_speed", 0.0f).toFloat();
    bool moving = speed > 1.0f;
    if (m_isMoving != moving) {
        m_isMoving = moving;
        emit movingChanged();
    }

    updateSeatBeltStates();
}

// ─── Socket 数据处理 ──────────────────────────────────────
void DashboardBackend::onNewConnection() {
    qDebug() << "[SocketServer] New incoming connection";
}

void DashboardBackend::onSocketReadyRead() {
    if (!m_socketConnection) return;
    QByteArray data = m_socketConnection->readAll();
    if (!data.isEmpty()) {
        handleCanFrameData(data, 0);
    }
}

void DashboardBackend::handleCanFrameData(const QByteArray& data, quint32) {
    // Unix Socket 帧格式：[can_id(4B LE)][dlc(1B)][data]
    // 变长帧，dlc=实际字节数
    const int MIN_FRAME = 5; // 4B ID + 1B DLC
    m_rxBuffer.append(data);

    while (m_rxBuffer.size() >= MIN_FRAME) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(m_rxBuffer.constData());
        quint32 canId = *reinterpret_cast<const quint32*>(ptr);
        uint8_t dlc = ptr[4];
        int frameLen = 5 + dlc;

        if (m_rxBuffer.size() < frameLen) break;

        QByteArray frameData = QByteArray::fromRawData(
            reinterpret_cast<const char*>(ptr + 5), dlc);
        m_rxBuffer.remove(0, frameLen);

        onCanFrameReceived(canId, frameData);
    }
}

// ─── CAN 帧处理 ───────────────────────────────────────────
void DashboardBackend::onCanFrameReceived(quint32 canId, const QByteArray& data) {
    DisplayData dd = {};
    memset(&dd, 0, sizeof(dd));

    uint32_t updatedMask = m_converter->processFrame(
        canId,
        reinterpret_cast<const uint8_t*>(data.constData()),
        data.size(),
        dd);

    if (updatedMask == 0) return;

    // 从旧数据继承，只更新当前帧携带的字段
    QVariantMap newData = m_displayData;

    // 遍历 CAN_FIELD_TABLE，找到当前帧更新的字段
    for (int i = 0; i < CAN_FIELD_TABLE_COUNT; i++) {
        const CanFieldDef* def = &CAN_FIELD_TABLE[i];
        if (def->can_id != canId) continue;
        int bit = i;
        if ((updatedMask & (1U << bit)) == 0) continue;

        // 根据 display_key 写入 DisplayData 字段
        float value = 0.0f;
        if (strcmp(def->display_key, "bat_volt") == 0) {
            value = dd.bat_volt;
            newData["bat_volt"] = value;
        } else if (strcmp(def->display_key, "bat_curr") == 0) {
            value = dd.bat_curr;
            newData["bat_curr"] = value;
        } else if (strcmp(def->display_key, "bat_soc") == 0) {
            value = dd.bat_soc;
            newData["bat_soc"] = value;
        } else if (strcmp(def->display_key, "vehicle_speed") == 0) {
            value = dd.vehicle_speed;
            newData["vehicle_speed"] = value;
        } else if (strcmp(def->display_key, "brake") == 0) {
            value = dd.brake;
            newData["brake"] = value;
        } else if (strcmp(def->display_key, "motor_rpm") == 0) {
            value = dd.motor_rpm;
            newData["motor_rpm"] = value;
        } else if (strcmp(def->display_key, "motor_temp") == 0) {
            value = dd.motor_temp;
            newData["motor_temp"] = value;
        } else if (strcmp(def->display_key, "driver_occupied") == 0) {
            value = dd.driver_occupied;
            newData["driver_occupied"] = value;
        } else if (strcmp(def->display_key, "passenger_occupied") == 0) {
            value = dd.passenger_occupied;
            newData["passenger_occupied"] = value;
        } else if (strcmp(def->display_key, "driver_buckled") == 0) {
            value = dd.driver_buckled;
            newData["driver_buckled"] = value;
        } else if (strcmp(def->display_key, "passenger_buckled") == 0) {
            value = dd.passenger_buckled;
            newData["passenger_buckled"] = value;
        } else if (strcmp(def->display_key, "rear_buckle") == 0) {
            value = dd.rear_buckle;
            newData["rear_buckle"] = value;
        }

        // 通知报警 Runtime
        m_alarmRuntime->onValueChanged(def->display_key, value);
    }

    if (!newData.isEmpty()) {
        m_displayData = newData;
        emit displayDataChanged();
    }
}

// ─── 安全带状态 ───────────────────────────────────────────
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

// ─── 指示灯控制 ───────────────────────────────────────────
void DashboardBackend::setIndicator(const QString& widget_id, bool on,
                                    bool flash, float hz) {
    QVariantMap state;
    state["on"] = on;
    state["flash"] = flash;
    state["hz"] = hz;
    m_indicatorStates[widget_id] = state;
    emit indicatorStatesChanged();
}

// ─── QML 接口 ─────────────────────────────────────────────
QVariant DashboardBackend::get(const QString& key) const {
    return m_displayData.value(key);
}

void DashboardBackend::set(const QString& key, const QVariant& value) {
    Q_UNUSED(key);
    Q_UNUSED(value);
    qWarning() << "DashboardBackend::set() called - this should not happen";
}

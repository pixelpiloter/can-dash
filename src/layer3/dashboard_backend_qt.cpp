// dashboard_backend_qt.cpp
// Layer 3: Qt 适配层实现

#include "dashboard_backend_qt.h"
#include "../layer2/can_converter.h"
#include "../layer2/alarm_runtime.h"
#include "../layer2/seat_belt_runtime.h"

#include "../generated/can_field_def.h"
#include "../generated/alarm_rule_def.h"
#include "../generated/seat_belt_def.h"

#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QLocalSocket>
#include <QThread>

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

    // ─── 定时器（~100ms）───
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &DashboardBackend::onTick);
    m_tickTimer->start(100);

    qDebug() << "DashboardBackend::init() done";

    // ─── 启动 Unix Socket 服务器 ───
    startSocketServer();
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
        // 调试：打印未能解析的帧
        qDebug() << "DROP: canId=0x" << QString::number(canId, 16) << " dataLen=" << data.size();
        return;
    }

    // 提取关键变量（简化版，实际通过 name→index 映射查找）
    // 这里演示：假设 updatedMask 的 bit0=bat_volt, bit1=bat_soc, bit2=speed
    // 实际项目通过 display_key 名称查找字段

    // 更新显示数据：只更新本帧实际携带的字段（由 updatedMask 决定）
    // 未携带的字段保留旧值（从 m_displayData 继承）
    QVariantMap newData = m_displayData;
    // 检查哪些字段被本帧更新了
    bool has_bat_volt  = false, has_bat_curr  = false, has_bat_soc  = false;
    bool has_speed     = false, has_rpm        = false, has_motor_temp = false;
    for (int i = 0; i < m_converter->fieldCount(); i++) {
        if ((updatedMask & (1U << i)) == 0) continue;
        const CanFieldDef* def = &m_converter->fieldTable()[i];
        if (strcmp(def->display_key, "bat_volt") == 0)       has_bat_volt  = true;
        if (strcmp(def->display_key, "bat_curr") == 0)       has_bat_curr  = true;
        if (strcmp(def->display_key, "bat_soc") == 0)        has_bat_soc   = true;
        if (strcmp(def->display_key, "vehicle_speed") == 0)  has_speed     = true;
        if (strcmp(def->display_key, "rpm") == 0)             has_rpm       = true;
        if (strcmp(def->display_key, "motor_temp") == 0)      has_motor_temp = true;
    }
    if (has_bat_volt)   newData["bat_volt"] = dd.bat_volt;
    if (has_bat_curr)   newData["bat_curr"] = dd.bat_curr;
    if (has_bat_soc)    newData["bat_soc"] = dd.bat_soc;
    if (has_speed)      newData["vehicle_speed"] = dd.vehicle_speed;
    if (has_rpm)        newData["rpm"] = dd.motor_rpm;
    if (has_motor_temp) newData["motor_temp"] = dd.motor_temp;
    m_displayData = newData;
    static int tick_count = 0;
    if (++tick_count % 20 == 0) {
        qDebug() << "CAN: speed=" << dd.vehicle_speed << " bat_volt=" << dd.bat_volt << " soc=" << dd.bat_soc;
    } else {
        qDebug() << "QML: spd=" << dd.vehicle_speed << " v=" << dd.bat_volt << " soc=" << dd.bat_soc;
    }
    emit displayDataChanged();

    // 转发给报警 Runtime（通过 display_key 匹配）
    // 简化：直接按变量名分发
    // 实际：通过 CAN_FIELD_TABLE 的 display_key 索引映射
    (void)updatedMask;
}

void DashboardBackend::onTick() {
    uint64_t nowMs = QDateTime::currentMSecsSinceEpoch();

    // ─── 检查新的 socket 连接 ───
    if (!m_socketConnection && m_socketServer->hasPendingConnections()) {
        m_socketConnection = m_socketServer->nextPendingConnection();
        if (m_socketConnection) {
            qDebug() << "[SocketServer] Client connected";
            connect(m_socketConnection, &QLocalSocket::disconnected, this, [this]() {
                qDebug() << "[SocketServer] Client disconnected";
                m_socketConnection->deleteLater();
                m_socketConnection = nullptr;
            });
        }
    }

    // ─── 读取所有可用的 CAN 帧 ───
    if (m_socketConnection && m_socketConnection->bytesAvailable() > 0) {
        // 读取所有完整帧（每帧 13 字节: 4 can_id + 1 dlc + dlc data, max 13）
        QByteArray allData = m_socketConnection->readAll();
        int offset = 0;
        while (offset + 5 <= allData.size()) {
            // 解析 header: 4 bytes can_id + 1 byte dlc
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

    // 更新座位图标状态列表
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
    // 单向数据流，QML 不写回
    qWarning() << "DashboardBackend::set() called - this should not happen";
    (void)key;
    (void)value;
}

void DashboardBackend::onSocketReadyRead() {
    // 只在有挂起连接时接受（已在 m_socketConnection 里则跳过）
    if (!m_socketConnection) {
        m_socketConnection = m_socketServer->nextPendingConnection();
        if (!m_socketConnection) return;
        qDebug() << "[SocketServer] Client connected";
        connect(m_socketConnection, &QLocalSocket::readyRead, this, &DashboardBackend::onSocketReadyRead);
        connect(m_socketConnection, &QLocalSocket::disconnected, this, [this]() {
            qDebug() << "[SocketServer] Client disconnected";
            m_socketConnection->deleteLater();
            m_socketConnection = nullptr;
        });
    }

    if (!m_socketConnection) return;

    // 追加新数据到缓冲池
    QByteArray incoming = m_socketConnection->readAll();
    if (incoming.isEmpty()) return;
    m_rxBuffer.append(incoming);

    // 逐帧解析缓冲池
    int pos = 0;
    while (pos + 5 <= m_rxBuffer.size()) {
        // 读 CAN ID (4B) + DLC (1B)
        quint32 canId;
        memcpy(&canId, m_rxBuffer.data() + pos, 4);
        pos += 4;

        quint8 dlc = static_cast<quint8>(static_cast<unsigned char>(m_rxBuffer[pos]));
        pos += 1;

        int dataLen = qMin<int>(dlc, 8);
        if (pos + dataLen > m_rxBuffer.size()) {
            // 数据不完整，等待下一批
            break;
        }
        QByteArray data = m_rxBuffer.mid(pos, dataLen);
        pos += dataLen;

        onCanFrameReceived(canId, data);
    }

    // 保留未解析完的尾随字节
    if (pos > 0) {
        m_rxBuffer = m_rxBuffer.mid(pos);
    }
}

void DashboardBackend::startSocketServer() {
    // 清理旧 socket 文件
    QLocalServer::removeServer("/tmp/can_dash_socket");

    m_socketServer = new QLocalServer(this);
    if (!m_socketServer->listen("/tmp/can_dash_socket")) {
        qWarning() << "Failed to listen on /tmp/can_dash_socket:" << m_socketServer->errorString();
        return;
    }
    qDebug() << "[SocketServer] Listening on /tmp/can_dash_socket";
    // 连接由 onTick 轮询处理，不再需要 newConnection 信号
}

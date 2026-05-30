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
}

void DashboardBackend::onCanFrameReceived(quint32 canId, const QByteArray& data) {
    DisplayData dd = {};
    uint32_t updatedMask = m_converter->processFrame(
        canId,
        reinterpret_cast<const uint8_t*>(data.constData()),
        data.size(),
        dd
    );

    if (updatedMask == 0) return;

    // 提取关键变量（简化版，实际通过 name→index 映射查找）
    // 这里演示：假设 updatedMask 的 bit0=bat_volt, bit1=bat_soc, bit2=speed
    // 实际项目通过 display_key 名称查找字段

    // 更新显示数据
    m_displayData["bat_volt"] = dd.bat_volt;
    m_displayData["bat_curr"] = dd.bat_curr;
    m_displayData["bat_soc"] = dd.bat_soc;
    m_displayData["vehicle_speed"] = dd.vehicle_speed;
    emit displayDataChanged();

    // 转发给报警 Runtime（通过 display_key 匹配）
    // 简化：直接按变量名分发
    // 实际：通过 CAN_FIELD_TABLE 的 display_key 索引映射
    (void)updatedMask;
}

void DashboardBackend::onTick() {
    uint64_t nowMs = QDateTime::currentMSecsSinceEpoch();

    // 驱动所有 Runtime tick
    m_alarmRuntime->tick(nowMs);
    m_seatBeltRuntime->tick(nowMs);

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

// dashboard_backend_qt.h
// Layer 3: Qt 适配层 - DashboardBackend
// 组合所有 Layer 2 Runtime，暴露 Q_PROPERTY 给 QML

#pragma once
#include <QObject>
#include <QVariant>
#include <QTimer>
#include <QMap>
#include <QLocalServer>

// Layer 2 前向声明
class CanConverter;
class AlarmRuntime;
class SeatBeltRuntime;

class DashboardBackend : public QObject {
    Q_OBJECT

    // ─── 报警状态（QML 绑定）───
    Q_PROPERTY(bool alarmActive READ alarmActive NOTIFY alarmActiveChanged)
    Q_PROPERTY(QString alarmMessageZh READ alarmMessageZh NOTIFY alarmActiveChanged)
    Q_PROPERTY(QVariantList alarmList READ alarmList NOTIFY alarmActiveChanged)

    // ─── 安全带状态（QML 绑定）───
    Q_PROPERTY(bool seatBeltWarningActive READ seatBeltWarningActive NOTIFY seatBeltWarningChanged)
    Q_PROPERTY(QString seatBeltMessage READ seatBeltMessage NOTIFY seatBeltWarningChanged)
    Q_PROPERTY(QVariantList seatIconStates READ seatIconStates NOTIFY seatIconStatesChanged)
    Q_PROPERTY(bool isMoving READ isMoving NOTIFY movingChanged)

    // ─── 显示数据（QML 绑定）───
    Q_PROPERTY(QVariantMap displayData READ displayData NOTIFY displayDataChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend();

    void init();

    // Qt 属性访问器
    bool alarmActive() const { return m_alarmActive; }
    QString alarmMessageZh() const { return m_alarmMessageZh; }
    QVariantList alarmList() const { return m_alarmList; }

    bool seatBeltWarningActive() const { return m_seatBeltActive; }
    QString seatBeltMessage() const { return m_seatBeltMessage; }
    QVariantList seatIconStates() const { return m_seatIconStates; }
    bool isMoving() const { return m_isMoving; }

    QVariantMap displayData() const { return m_displayData; }

    // QML 暴露的通用查询接口
    Q_INVOKABLE QVariant get(const QString& key) const;
    Q_INVOKABLE void set(const QString& key, const QVariant& value);

signals:
    void alarmActiveChanged();
    void seatBeltWarningChanged();
    void seatIconStatesChanged();
    void movingChanged();
    void displayDataChanged();

public slots:
    // 接收 CAN 帧（来自 CanReceiverQt）
    void onCanFrameReceived(quint32 canId, const QByteArray& data);

    // 定时更新
    void onTick();

private:
    void updateSeatBeltStates();
    void startSocketServer();
    void onSocketReadyRead();

    // Layer 2 运行时（纯 C++）
    CanConverter*    m_converter = nullptr;
    AlarmRuntime*   m_alarmRuntime = nullptr;
    SeatBeltRuntime* m_seatBeltRuntime = nullptr;

    // 状态
    bool m_alarmActive = false;
    QString m_alarmMessageZh;
    QVariantList m_alarmList;

    bool m_seatBeltActive = false;
    QString m_seatBeltMessage;
    QVariantList m_seatIconStates;

    bool m_isMoving = false;
    QVariantMap m_displayData;

    QTimer* m_tickTimer = nullptr;
    QLocalServer* m_socketServer = nullptr;
    QLocalSocket* m_socketConnection = nullptr;
    QByteArray m_rxBuffer;  // 跨 readyRead 调用的解析缓冲
};

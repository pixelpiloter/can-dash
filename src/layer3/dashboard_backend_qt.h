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
class IndicatorRuntime;
class LanguageManager;

class DashboardBackend : public QObject {
    Q_OBJECT

    // ─── 语言切换 ───
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString currentFont READ currentFont NOTIFY languageChanged)

    // ─── 报警状态 ───
    Q_PROPERTY(bool alarmActive READ alarmActive NOTIFY alarmActiveChanged)
    Q_PROPERTY(QString alarmMessageZh READ alarmMessageZh NOTIFY alarmActiveChanged)
    Q_PROPERTY(QVariantList alarmList READ alarmList NOTIFY alarmActiveChanged)

    // ─── 安全带状态 ───
    Q_PROPERTY(bool seatBeltWarningActive READ seatBeltWarningActive NOTIFY seatBeltWarningChanged)
    Q_PROPERTY(QString seatBeltMessage READ seatBeltMessage NOTIFY seatBeltWarningChanged)
    Q_PROPERTY(QVariantList seatIconStates READ seatIconStates NOTIFY seatIconStatesChanged)
    Q_PROPERTY(bool isMoving READ isMoving NOTIFY movingChanged)

    // ─── 显示数据 ───
    Q_PROPERTY(QVariantMap displayData READ displayData NOTIFY displayDataChanged)

    // ─── 指示灯状态 ───
    Q_PROPERTY(QVariantMap indicatorStates READ indicatorStates NOTIFY indicatorStatesChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend();

    void init();

    // Qt 属性访问器
    QString currentLanguage() const;
    void setLanguage(const QString& lang);
    QString currentFont() const;

    bool alarmActive() const { return m_backendAlarmActive; }
    QString alarmMessageZh() const { return m_backendAlarmMessageZh; }
    QVariantList alarmList() const { return m_alarmList; }

    bool seatBeltWarningActive() const { return m_seatBeltActive; }
    QString seatBeltMessage() const { return m_seatBeltMessage; }
    QVariantList seatIconStates() const { return m_seatIconStates; }
    bool isMoving() const { return m_isMoving; }

    QVariantMap displayData() const { return m_displayData; }
    QVariantMap indicatorStates() const { return m_indicatorStates; }

    // QML 暴露的通用查询接口
    Q_INVOKABLE QVariant get(const QString& key) const;
    Q_INVOKABLE void set(const QString& key, const QVariant& value);

    // 多语言翻译
    Q_INVOKABLE QString tr(const QString& key) const;

    // 指示灯控制（由 QML 或 CAN 数据调用）
    Q_INVOKABLE void setIndicator(const QString& id, bool on, bool flash = false, float hz = 0.0f);
    Q_INVOKABLE bool indicatorOn(const QString& id) const;

signals:
    void languageChanged();
    void alarmActiveChanged();
    void seatBeltWarningChanged();
    void seatIconStatesChanged();
    void movingChanged();
    void displayDataChanged();
    void indicatorStatesChanged();

public slots:
    void onCanFrameReceived(quint32 canId, const QByteArray& data);
    void onTick();

private:
    void updateSeatBeltStates();
    void startSocketServer();
    void onSocketReadyRead();

    // Layer 2 运行时
    CanConverter*       m_converter    = nullptr;
    AlarmRuntime*       m_alarmRuntime = nullptr;
    SeatBeltRuntime*    m_seatBeltRuntime = nullptr;
    IndicatorRuntime*    m_indicatorRuntime = nullptr;
    LanguageManager*     m_langManager  = nullptr;

    // 状态
    QString m_currentLang = "zh_CN";
    bool m_alarmActive = false;
    QString m_alarmMessageZh;
    // 后端报警状态（由 alarm runtime 回调驱动）
    bool m_backendAlarmActive = false;
    QString m_backendAlarmMessageZh;
    QVariantList m_alarmList;

    bool m_seatBeltActive = false;
    QString m_seatBeltMessage;
    QVariantList m_seatIconStates;

    bool m_isMoving = false;
    QVariantMap m_displayData;
    QVariantMap m_indicatorStates;  // id → { on, flash, flashHz }

    QTimer* m_tickTimer = nullptr;
    QLocalServer* m_socketServer = nullptr;
    QLocalSocket* m_socketConnection = nullptr;
    QByteArray m_rxBuffer;
};

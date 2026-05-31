// dashboard_backend_qt.h
// Layer 3: Qt 适配层 - DashboardBackend（仅显示，从共享内存读取）
// 暴露 Q_PROPERTY 给 QML

#pragma once
#include <QObject>
#include <QVariant>
#include <QTimer>
#include <QVariantMap>
#include "layer1/shm/shm_display.h"

class LanguageManager;

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

    // ─── 指示灯状态（QML 绑定）───
    Q_PROPERTY(QVariantMap indicatorStates READ indicatorStates NOTIFY indicatorStatesChanged)

    // ─── 多语言 ───
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString currentFont READ currentFont NOTIFY languageChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend();

    void init();

    // Qt 属性访问器
    bool alarmActive() const { return m_backendAlarmActive; }
    QString alarmMessageZh() const { return m_backendAlarmMessageZh; }
    QVariantList alarmList() const { return m_alarmList; }

    bool seatBeltWarningActive() const { return m_seatBeltActive; }
    QString seatBeltMessage() const { return m_seatBeltMessage; }
    QVariantList seatIconStates() const { return m_seatIconStates; }
    bool isMoving() const { return m_isMoving; }

    QVariantMap displayData() const { return m_displayData; }
    QVariantMap indicatorStates() const { return m_indicatorStates; }

    // QML 通用接口
    Q_INVOKABLE QVariant get(const QString& key) const;
    Q_INVOKABLE void set(const QString& key, const QVariant& value);

    // 多语言
    Q_INVOKABLE QString tr(const QString& key) const;
    Q_INVOKABLE void setLanguage(const QString& lang);
    QString currentLanguage() const { return m_currentLanguage; }
    QString currentFont() const;

    // 指示灯查询
    Q_INVOKABLE bool indicatorOn(const QString& key) const;

    // 指示灯控制（来自 QML，仅调试用）
    Q_INVOKABLE void setIndicator(const QString& widget_id, bool on, bool flash, float hz);
    Q_INVOKABLE void setIndicator(const QString& widget_id, bool on);

signals:
    void alarmActiveChanged();
    void indicatorStatesChanged();
    void seatBeltWarningChanged();
    void seatIconStatesChanged();
    void movingChanged();
    void displayDataChanged();
    void languageChanged();

public slots:
    void onTick();

private:
    QVariantMap indicatorSlotToMap(const ShmIndicatorSlot& slot) const;

    LanguageManager* m_langManager = nullptr;
    QString m_currentLanguage = "zh_CN";

    // 报警状态
    bool m_backendAlarmActive = false;
    QString m_backendAlarmMessageZh;
    QVariantList m_alarmList;

    // 安全带状态
    bool m_seatBeltActive = false;
    QString m_seatBeltMessage;
    QVariantList m_seatIconStates;

    bool m_isMoving = false;
    QVariantMap m_displayData;
    QVariantMap m_indicatorStates;

    QTimer* m_tickTimer = nullptr;
};

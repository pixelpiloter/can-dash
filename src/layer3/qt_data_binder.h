// qt_data_binder.h
// IDataBinder 的 Qt/QML 实现
//
// 把 DisplaySnapshot 转成 Q_PROPERTY + QVariantMap，QML 用 Connections 订阅
// 不读 shm（由 DataSource 推送），不做业务转换（已在 DataSource 完成）
// 仅做"映射"：C struct → QVariantMap + i18n + alarm list 解析

#pragma once

#include "idata_binder.h"
#include "layer2/language_manager.h"  // LanguageEntry
#include <QObject>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QString>

class LanguageManager;

class QtDataBinder : public QObject, public IDataBinder {
    Q_OBJECT

    // ─── 报警状态 ───
    Q_PROPERTY(bool alarmActive READ alarmActive NOTIFY alarmActiveChanged)
    Q_PROPERTY(QString alarmMessageZh READ alarmMessageZh NOTIFY alarmActiveChanged)
    Q_PROPERTY(QVariantList alarmList READ alarmList NOTIFY alarmActiveChanged)

    // ─── 安全带状态 ───
    Q_PROPERTY(bool seatBeltWarningActive READ seatBeltWarningActive NOTIFY seatBeltChanged)
    Q_PROPERTY(QString seatBeltMessage READ seatBeltMessage NOTIFY seatBeltChanged)
    Q_PROPERTY(QVariantList seatIconStates READ seatIconStates NOTIFY seatBeltChanged)
    Q_PROPERTY(bool isMoving READ isMoving NOTIFY movingChanged)

    // ─── 显示数据 ───
    Q_PROPERTY(QVariantMap displayData READ displayData NOTIFY displayDataChanged)

    // ─── 指示灯状态 ───
    Q_PROPERTY(QVariantMap indicatorStates READ indicatorStates NOTIFY indicatorStatesChanged)

    // ─── 处理器健康 ───
    Q_PROPERTY(bool processorOnline READ processorOnline NOTIFY healthChanged)
    Q_PROPERTY(QString processorStatus READ processorStatus NOTIFY healthChanged)

    // ─── 数据健康（FPS/age/seq/dropped）───
    Q_PROPERTY(qulonglong dataAgeMs READ dataAgeMs NOTIFY dataHealthChanged)
    Q_PROPERTY(qulonglong frameSeq READ frameSeq NOTIFY dataHealthChanged)
    Q_PROPERTY(double dataFps READ dataFps NOTIFY dataHealthChanged)
    Q_PROPERTY(qulonglong droppedFrames READ droppedFrames NOTIFY dataHealthChanged)
    Q_PROPERTY(QVariantMap fieldValidity READ fieldValidity NOTIFY dataHealthChanged)

    // ─── 派生指标: TripComputer (v3 探针延伸) ───
    Q_PROPERTY(float tripDistanceKm READ tripDistanceKm NOTIFY tripChanged)
    Q_PROPERTY(float tripAvgSpeedKmh READ tripAvgSpeedKmh NOTIFY tripChanged)
    Q_PROPERTY(uint tripDurationS READ tripDurationS NOTIFY tripChanged)
    Q_PROPERTY(bool tripIsMoving READ tripIsMoving NOTIFY tripChanged)

public:
    explicit QtDataBinder(QObject* parent = nullptr);
    ~QtDataBinder() override;

    // IDataBinder 接口
    void onDataUpdated(const DisplaySnapshot& snapshot) override;
    void onHealthChanged(HealthStatus new_health) override;

    // 初始化 i18n（独立于 DataSource）
    void initLanguage(const LanguageEntry* translations, int translation_count);

    // ─── 属性访问器 ───
    bool alarmActive() const { return m_alarmActive; }
    QString alarmMessageZh() const { return m_alarmMessageZh; }
    QVariantList alarmList() const { return m_alarmList; }

    bool seatBeltWarningActive() const { return m_seatBeltActive; }
    QString seatBeltMessage() const { return m_seatBeltMessage; }
    QVariantList seatIconStates() const { return m_seatIconStates; }
    bool isMoving() const { return m_isMoving; }

    QVariantMap displayData() const { return m_displayData; }
    QVariantMap indicatorStates() const { return m_indicatorStates; }

    bool processorOnline() const { return m_processorOnline; }
    QString processorStatus() const { return m_processorStatus; }

    qulonglong dataAgeMs() const { return m_dataAgeMs; }
    qulonglong frameSeq() const { return m_frameSeq; }
    double dataFps() const { return m_dataFps; }
    qulonglong droppedFrames() const { return m_droppedFrames; }
    QVariantMap fieldValidity() const { return m_fieldValidity; }

    // 派生指标
    float tripDistanceKm() const   { return m_tripDistanceKm; }
    float tripAvgSpeedKmh() const  { return m_tripAvgSpeedKmh; }
    uint tripDurationS() const     { return m_tripDurationS; }
    bool tripIsMoving() const      { return m_tripIsMoving; }

    // QML 通用接口
    Q_INVOKABLE QVariant get(const QString& key) const;
    Q_INVOKABLE void set(const QString& key, const QVariant& value);
    Q_INVOKABLE bool indicatorOn(const QString& key) const;
    Q_INVOKABLE void setIndicator(const QString& widget_id, bool on, bool flash, float hz);
    Q_INVOKABLE QString tr(const QString& key) const;
    Q_INVOKABLE void setLanguage(const QString& lang);

signals:
    void alarmActiveChanged();
    void seatBeltChanged();
    void movingChanged();
    void displayDataChanged();
    void indicatorStatesChanged();
    void healthChanged();
    void dataHealthChanged();
    void languageChanged();
    void tripChanged();  // v3 探针延伸: 派生指标变更

private:
    QVariantMap buildDisplayData(const DisplayData& d) const;
    QVariantMap buildFieldValidity(uint32_t updated_mask) const;
    QVariantList buildAlarmList(const DisplaySnapshot& s) const;
    QVariantList buildSeatIconStates(const SeatBeltState& sb) const;
    QVariantMap buildIndicatorStates(const IndicatorStates& ind) const;
    QString buildSeatBeltMessage(const SeatBeltState& sb) const;
    QVariantMap indicatorSlotToMap(const IndicatorState& s) const;

    LanguageManager* m_langManager = nullptr;
    QString m_currentLanguage = "zh_CN";

    // 报警
    bool m_alarmActive = false;
    QString m_alarmMessageZh;
    QVariantList m_alarmList;

    // 安全带
    bool m_seatBeltActive = false;
    QString m_seatBeltMessage;
    QVariantList m_seatIconStates;

    bool m_isMoving = false;
    QVariantMap m_displayData;
    QVariantMap m_indicatorStates;

    // 健康
    bool m_processorOnline = false;
    QString m_processorStatus = QStringLiteral("disconnected");

    // 数据健康
    qulonglong m_dataAgeMs = 0;
    qulonglong m_frameSeq = 0;
    double m_dataFps = 0.0;
    qulonglong m_droppedFrames = 0;
    QVariantMap m_fieldValidity;

    // 派生指标
    float m_tripDistanceKm = 0.0f;
    float m_tripAvgSpeedKmh = 0.0f;
    uint m_tripDurationS = 0;
    bool m_tripIsMoving = false;

    // 缓存：上次推送的时间戳（用于算 dataAgeMs）
    uint64_t m_lastTimestampMs = 0;
};

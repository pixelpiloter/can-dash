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
    // PR 4: 能耗 + 续航
    Q_PROPERTY(float tripEnergyKWh READ tripEnergyKWh NOTIFY tripChanged)
    Q_PROPERTY(float tripEfficiencyKWh100Km READ tripEfficiencyKWh100Km NOTIFY tripChanged)
    Q_PROPERTY(float tripRangeConfidencePct READ tripRangeConfidencePct NOTIFY tripChanged)

    // ─── 主题 (PR 7) — 单一 NOTIFY 共享 6 个颜色属性, QML 端读 6 字段只触发 1 次重算 ───
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeChanged)
    Q_PROPERTY(bool themeIsDay READ themeIsDay NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorBackground READ themeColorBackground NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorForeground READ themeColorForeground NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorAccent READ themeColorAccent NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorWarning READ themeColorWarning NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorCritical READ themeColorCritical NOTIFY themeChanged)

    // ─── WarningManager (PR 9) — 共享 warningChanged(), count/list/has_critical ───
    Q_PROPERTY(QVariantList warningActiveList READ warningActiveList NOTIFY warningChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY warningChanged)
    Q_PROPERTY(bool hasCritical READ hasCritical NOTIFY warningChanged)

    // ─── SettingsManager (PR 13) — 共享 settingsChanged(), units/brightness ───
    Q_PROPERTY(int settingsUnits READ settingsUnits NOTIFY settingsChanged)
    Q_PROPERTY(int settingsBrightness READ settingsBrightness NOTIFY settingsChanged)

    // ─── ViewManager (PR 13) — 共享 viewChanged(), mode/gear/charge/chargeActive ───
    // QML 端通过 viewMode 切 StackView, isChargeView 用来点亮充电动画, gear/charge 用于调试
    Q_PROPERTY(int viewMode READ viewMode NOTIFY viewChanged)
    Q_PROPERTY(bool isChargeView READ isChargeView NOTIFY viewChanged)
    Q_PROPERTY(int viewGear READ viewGear NOTIFY viewChanged)
    Q_PROPERTY(int viewCharge READ viewCharge NOTIFY viewChanged)

    // ─── ChimeManager (PR 14) — 共享 chimeChanged(), 5 字段同发同更 ───
    // QML 端按 has_active 决定是否播放音效, freq/duration/repeat 给 QSoundEffect 用
    // 注: enabled/volume 是配置 (QML 写入), 跟 chimeActive 等状态 (ShmDataSource 推) 走不同 setter
    Q_PROPERTY(bool chimeActive READ chimeActive NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeSeverity READ chimeSeverity NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeFrequencyHz READ chimeFrequencyHz NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeDurationMs READ chimeDurationMs NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeRepeatCount READ chimeRepeatCount NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeVolumePct READ chimeVolumePct NOTIFY chimeChanged)

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
    float tripEnergyKWh() const    { return m_tripEnergyKWh; }
    float tripEfficiencyKWh100Km() const  { return m_tripEfficiencyKWh100Km; }
    float tripRangeConfidencePct() const { return m_tripRangeConfidencePct; }

    // 主题 (PR 7)
    int themeMode() const        { return static_cast<int>(m_themeMode); }
    bool themeIsDay() const      { return m_themeIsDay != 0; }
    uint themeColorBackground() const { return static_cast<uint>(m_themeColorBackground); }
    uint themeColorForeground() const { return static_cast<uint>(m_themeColorForeground); }
    uint themeColorAccent() const     { return static_cast<uint>(m_themeColorAccent); }
    uint themeColorWarning() const    { return static_cast<uint>(m_themeColorWarning); }
    uint themeColorCritical() const   { return static_cast<uint>(m_themeColorCritical); }

    // 警告 (PR 9)
    QVariantList warningActiveList() const { return m_warningActiveList; }
    int  warningCount() const              { return m_warningCount; }
    bool hasCritical() const               { return m_hasCritical; }

    // 设置 (PR 13) — 透传到 ShmDataSource.m_settings
    int settingsUnits() const       { return static_cast<int>(m_settingsUnits); }
    int settingsBrightness() const  { return static_cast<int>(m_settingsBrightness); }

    // 视图 (PR 13) — 透传到 ShmDataSource.m_view
    int  viewMode() const     { return static_cast<int>(m_viewMode); }   // 0=DRIVE, 1=CHARGE, 2=SETUP
    bool isChargeView() const { return m_viewMode == 1; }                 // 派生: 充电视图中
    int  viewGear() const     { return static_cast<int>(m_viewGear); }    // 0=P, 1=R, 2=N, 3=D, 4=S
    int  viewCharge() const   { return static_cast<int>(m_viewCharge); }  // 0=idle, 1+=charging

    // 声音 (PR 14) — 透传到 ShmDataSource.m_chime
    bool chimeActive() const        { return m_chimeActive; }
    int  chimeSeverity() const      { return static_cast<int>(m_chimeSeverity); }
    int  chimeFrequencyHz() const   { return static_cast<int>(m_chimeFrequencyHz); }
    int  chimeDurationMs() const    { return static_cast<int>(m_chimeDurationMs); }
    int  chimeRepeatCount() const   { return static_cast<int>(m_chimeRepeatCount); }
    int  chimeVolumePct() const     { return static_cast<int>(m_chimeVolumePct); }

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
    void themeChanged();  // PR 7: 主题模式或 5 色任一变化
    void warningChanged();  // PR 9: warningCount/list/hasCritical 任一变化
    void settingsChanged();  // PR 13: settingsUnits/brightness 任一变化
    void viewChanged();      // PR 13: viewMode/gear/charge 任一变化
    void chimeChanged();     // PR 14: chimeActive/severity/freq/duration/repeat/volume 任一变化

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
    float m_tripEnergyKWh = 0.0f;
    float m_tripEfficiencyKWh100Km = 0.0f;
    float m_tripRangeConfidencePct = 100.0f;

    // 主题 (PR 7) — 由 ShmDataSource 推过来, 缓存到这些字段
    uint8_t m_themeMode = 2;             // 默认 AUTO (0=DAY, 1=NIGHT, 2=AUTO)
    uint8_t m_themeIsDay = 1;            // 派生
    uint8_t _m_themePad[2] = {0, 0};
    uint32_t m_themeColorBackground = 0xFFFFFFFFu;  // 默认 DAY 色
    uint32_t m_themeColorForeground = 0xFF111111u;
    uint32_t m_themeColorAccent     = 0xFF1976D2u;
    uint32_t m_themeColorWarning    = 0xFFFFC107u;
    uint32_t m_themeColorCritical   = 0xFFD32F2Fu;

    // 警告 (PR 9) — 由 ShmDataSource 推过来, 缓存到这些字段
    QVariantList m_warningActiveList;
    int          m_warningCount = 0;
    bool         m_hasCritical  = false;

    // 设置 (PR 13) — 由 ShmDataSource 推过来, 缓存到这些字段
    uint8_t m_settingsUnits      = 0;     // 0=METRIC, 1=IMPERIAL
    uint8_t m_settingsBrightness = 80;    // 默认 80 (跟 SettingsManager::kDefaultBrightness 对齐)
    uint8_t _m_settingsPad[2]     = {0, 0};

    // 视图 (PR 13) — 由 ShmDataSource 推过来, 缓存到这些字段
    uint8_t m_viewMode   = 0;    // 0=DRIVE (默认), 1=CHARGE, 2=SETUP
    uint8_t m_viewGear   = 0;    // 0=P, 1=R, 2=N, 3=D, 4=S
    uint8_t m_viewCharge = 0;    // 0=idle, 1+=charging
    uint8_t _m_viewPad   = 0;

    // 声音 (PR 14) — 由 ShmDataSource 推过来, 缓存到这些字段
    // chimeActive=0 时其他字段为 0 (清空) 或上次值 (QML 端按 chimeActive 过滤)
    bool    m_chimeActive      = false;
    uint8_t m_chimeSeverity    = 0;   // 0=INFO, 1=WARNING, 2=CRITICAL
    uint8_t _m_chimePad0[1]    = {0};
    uint16_t m_chimeFrequencyHz = 0;
    uint16_t m_chimeDurationMs  = 0;
    uint8_t  m_chimeRepeatCount = 0;
    uint8_t  m_chimeVolumePct   = 80;  // 默认 80 (跟 L2 ChimeManager::kDefaultConfig 对齐)

    // 缓存：上次推送的时间戳（用于算 dataAgeMs）
    uint64_t m_lastTimestampMs = 0;
};

// dashboard_backend.h
// DashboardBackend 胶水层：组合 IDataSource + IDataBinder
//
// 这是 Qt 版的胶水，未来 Kanzi 版只需替换 QtDataBinder → KanziDataBinder
// 业务侧（DataSource）和 UI 侧（Binder）完全解耦
//
// 使用方式：
//   DashboardBackend backend;
//   backend.init();           // 默认 ShmDataSource + QtDataBinder
//   // QML 端：context->setContextProperty("dashboard", &backend);

#pragma once

#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <memory>

class IDataSource;
class IDataBinder;

class DashboardBackend : public QObject {
    Q_OBJECT
    // 透传 QtDataBinder 的 Q_PROPERTY 给 QML
    Q_PROPERTY(QVariantMap displayData READ displayData NOTIFY displayDataChanged)
    Q_PROPERTY(QVariantMap indicatorStates READ indicatorStates NOTIFY indicatorStatesChanged)
    Q_PROPERTY(bool alarmActive READ alarmActive NOTIFY alarmActiveChanged)
    Q_PROPERTY(QString alarmMessageZh READ alarmMessageZh NOTIFY alarmActiveChanged)
    Q_PROPERTY(QVariantList alarmList READ alarmList NOTIFY alarmActiveChanged)
    Q_PROPERTY(bool seatBeltWarningActive READ seatBeltWarningActive NOTIFY seatBeltChanged)
    Q_PROPERTY(QString seatBeltMessage READ seatBeltMessage NOTIFY seatBeltChanged)
    Q_PROPERTY(QVariantList seatIconStates READ seatIconStates NOTIFY seatBeltChanged)
    Q_PROPERTY(bool isMoving READ isMoving NOTIFY movingChanged)
    Q_PROPERTY(bool processorOnline READ processorOnline NOTIFY healthChanged)
    Q_PROPERTY(QString processorStatus READ processorStatus NOTIFY healthChanged)
    Q_PROPERTY(qulonglong dataAgeMs READ dataAgeMs NOTIFY dataHealthChanged)
    Q_PROPERTY(qulonglong frameSeq READ frameSeq NOTIFY dataHealthChanged)
    Q_PROPERTY(double dataFps READ dataFps NOTIFY dataHealthChanged)
    Q_PROPERTY(qulonglong droppedFrames READ droppedFrames NOTIFY dataHealthChanged)
    Q_PROPERTY(QVariantMap fieldValidity READ fieldValidity NOTIFY dataHealthChanged)

    // 派生指标 (v3 探针延伸)
    Q_PROPERTY(float tripDistanceKm READ tripDistanceKm NOTIFY tripChanged)
    Q_PROPERTY(float tripAvgSpeedKmh READ tripAvgSpeedKmh NOTIFY tripChanged)
    Q_PROPERTY(uint tripDurationS READ tripDurationS NOTIFY tripChanged)
    Q_PROPERTY(bool tripIsMoving READ tripIsMoving NOTIFY tripChanged)
    // PR 4: 能耗 + 续航
    Q_PROPERTY(float tripEnergyKWh READ tripEnergyKWh NOTIFY tripChanged)
    Q_PROPERTY(float tripEfficiencyKWh100Km READ tripEfficiencyKWh100Km NOTIFY tripChanged)
    Q_PROPERTY(float tripRangeConfidencePct READ tripRangeConfidencePct NOTIFY tripChanged)

    // 主题 (PR 7)
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeChanged)
    Q_PROPERTY(bool themeIsDay READ themeIsDay NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorBackground READ themeColorBackground NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorForeground READ themeColorForeground NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorAccent READ themeColorAccent NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorWarning READ themeColorWarning NOTIFY themeChanged)
    Q_PROPERTY(uint themeColorCritical READ themeColorCritical NOTIFY themeChanged)

    // 警告 (PR 9) — 透传到 QtDataBinder
    Q_PROPERTY(QVariantList warningActiveList READ warningActiveList NOTIFY warningChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY warningChanged)
    Q_PROPERTY(bool hasCritical READ hasCritical NOTIFY warningChanged)

    // 设置 (PR 13) — 透传到 QtDataBinder (QML 端能改单位/亮度)
    Q_PROPERTY(int settingsUnits READ settingsUnits NOTIFY settingsChanged)
    Q_PROPERTY(int settingsBrightness READ settingsBrightness NOTIFY settingsChanged)

    // 视图 (PR 13) — 透传到 QtDataBinder (QML 端切 StackView)
    Q_PROPERTY(int viewMode READ viewMode NOTIFY viewChanged)
    Q_PROPERTY(bool isChargeView READ isChargeView NOTIFY viewChanged)
    Q_PROPERTY(int viewGear READ viewGear NOTIFY viewChanged)
    Q_PROPERTY(int viewCharge READ viewCharge NOTIFY viewChanged)

    // 声音 (PR 14) — 透传到 QtDataBinder (QML 端按 chimeActive 触发音效)
    Q_PROPERTY(bool chimeActive READ chimeActive NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeSeverity READ chimeSeverity NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeFrequencyHz READ chimeFrequencyHz NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeDurationMs READ chimeDurationMs NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeRepeatCount READ chimeRepeatCount NOTIFY chimeChanged)
    Q_PROPERTY(int  chimeVolumePct READ chimeVolumePct NOTIFY chimeChanged)

    // 自检 (PR 17) — 透传到 QtDataBinder (QML 端按 status 切顶部状态条颜色)
    Q_PROPERTY(int selfTestStatus READ selfTestStatus NOTIFY selfTestChanged)
    Q_PROPERTY(int selfTestCriticalReceived READ selfTestCriticalReceived NOTIFY selfTestChanged)
    Q_PROPERTY(int selfTestCriticalTotal READ selfTestCriticalTotal NOTIFY selfTestChanged)
    Q_PROPERTY(int selfTestCriticalStuck READ selfTestCriticalStuck NOTIFY selfTestChanged)
    Q_PROPERTY(int selfTestWarnStuck READ selfTestWarnStuck NOTIFY selfTestChanged)
    Q_PROPERTY(int selfTestOutOfRange READ selfTestOutOfRange NOTIFY selfTestChanged)

    // 跛行模式 (PR 44) — 透传到 QtDataBinder (QML 端弹 L1/L2/L3 警告条)
    Q_PROPERTY(int limpHomeLevel READ limpHomeLevel NOTIFY limpHomeChanged)
    Q_PROPERTY(bool limpHomeActive READ limpHomeActive NOTIFY limpHomeChanged)
    Q_PROPERTY(QString limpHomeMessageZh READ limpHomeMessageZh NOTIFY limpHomeChanged)
    Q_PROPERTY(QString limpHomeMessageEn READ limpHomeMessageEn NOTIFY limpHomeChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend() override;

    // 初始化（默认 ShmDataSource + QtDataBinder）
    void init();

    // 注入自定义 DataSource / Binder（用于测试或换 Kanzi）
    void setDataSource(std::unique_ptr<IDataSource> source);
    void setDataBinder(std::unique_ptr<IDataBinder> binder);

    // ─── 透传给 QtDataBinder（QML 可见接口）───
    QVariantMap displayData() const;
    QVariantMap indicatorStates() const;
    bool alarmActive() const;
    QString alarmMessageZh() const;
    QVariantList alarmList() const;
    bool seatBeltWarningActive() const;
    QString seatBeltMessage() const;
    QVariantList seatIconStates() const;
    bool isMoving() const;
    bool processorOnline() const;
    QString processorStatus() const;
    qulonglong dataAgeMs() const;
    qulonglong frameSeq() const;
    double dataFps() const;
    qulonglong droppedFrames() const;
    QVariantMap fieldValidity() const;

    // 派生指标 (v3 探针延伸)
    float tripDistanceKm() const;
    float tripAvgSpeedKmh() const;
    uint tripDurationS() const;
    bool tripIsMoving() const;
    // PR 4: 能耗 + 续航
    float tripEnergyKWh() const;
    float tripEfficiencyKWh100Km() const;
    float tripRangeConfidencePct() const;

    // 主题 (PR 7) — 透传到 QtDataBinder
    int themeMode() const;
    bool themeIsDay() const;
    uint themeColorBackground() const;
    uint themeColorForeground() const;
    uint themeColorAccent() const;
    uint themeColorWarning() const;
    uint themeColorCritical() const;

    // 警告 (PR 9) — 透传到 QtDataBinder
    QVariantList warningActiveList() const;
    int warningCount() const;
    bool hasCritical() const;

    // 设置 (PR 13) — 透传到 QtDataBinder
    int settingsUnits() const;
    int settingsBrightness() const;

    // 视图 (PR 13) — 透传到 QtDataBinder
    int  viewMode() const;
    bool isChargeView() const;
    int  viewGear() const;
    int  viewCharge() const;

    // 声音 (PR 14) — 透传到 QtDataBinder
    bool chimeActive() const;
    int  chimeSeverity() const;
    int  chimeFrequencyHz() const;
    int  chimeDurationMs() const;
    int  chimeRepeatCount() const;
    int  chimeVolumePct() const;

    // 自检 (PR 17) — 透传到 QtDataBinder
    int  selfTestStatus() const;
    int  selfTestCriticalReceived() const;
    int  selfTestCriticalTotal() const;
    int  selfTestCriticalStuck() const;
    int  selfTestWarnStuck() const;
    int  selfTestOutOfRange() const;

    // 跛行模式 (PR 44) — 透传到 QtDataBinder
    int     limpHomeLevel() const;
    bool    limpHomeActive() const;
    QString limpHomeMessageZh() const;
    QString limpHomeMessageEn() const;

    Q_INVOKABLE QVariant get(const QString& key) const;
    Q_INVOKABLE void set(const QString& key, const QVariant& value);
    Q_INVOKABLE bool indicatorOn(const QString& key) const;
    Q_INVOKABLE void setIndicator(const QString& widget_id, bool on, bool flash = false, float hz = 1.0f);
    Q_INVOKABLE QString tr(const QString& key) const;
    Q_INVOKABLE void setLanguage(const QString& lang);
    // QML 端 "重置小计" 按钮调用
    Q_INVOKABLE void resetTrip();
    // 主题 (PR 7): 透传到 ShmDataSource.m_theme, 下次 16ms tick 自动反映到 QML
    Q_INVOKABLE void setThemeMode(int mode);  // 0=DAY, 1=NIGHT, 2=AUTO
    Q_INVOKABLE void resetTheme();
    // 设置 (PR 13): 透传到 ShmDataSource.m_settings, 下次 16ms tick 自动反映到 QML
    Q_INVOKABLE void setSettingsUnits(int units);       // 0=METRIC, 1=IMPERIAL
    Q_INVOKABLE void setSettingsBrightness(int pct);    // 0-100, 自动 clamp
    Q_INVOKABLE void resetSettings();
    // 声音 (PR 14): 透传到 ShmDataSource.m_chime, 下次 16ms tick 自动反映到 QML
    Q_INVOKABLE void setChimeEnabled(bool enabled);    // 全局静音开关
    Q_INVOKABLE void setChimeVolume(int pct);          // 0-100, 自动 clamp
    Q_INVOKABLE void resetChime();
    // 自检 (PR 17): 透传到 ShmDataSource.m_self_test, 下次 16ms tick 自动反映到 QML
    Q_INVOKABLE void resetSelfTest();

signals:
    void displayDataChanged();
    void indicatorStatesChanged();
    void alarmActiveChanged();
    void seatBeltChanged();
    void movingChanged();
    void healthChanged();
    void dataHealthChanged();
    void languageChanged();
    void tripChanged();  // v3 探针延伸: 派生指标变更
    void themeChanged();  // PR 7: 主题模式或 5 色任一变化
    void warningChanged();  // PR 9: warningCount/list/hasCritical 任一变化
    void settingsChanged();  // PR 13: settingsUnits/brightness 任一变化
    void viewChanged();      // PR 13: viewMode/gear/charge 任一变化
    void chimeChanged();     // PR 14: chimeActive/severity/freq/duration/repeat/volume 任一变化
    void selfTestChanged();  // PR 17: selfTestStatus + 5 计数任一变化
    void limpHomeChanged();  // PR 44: limpHomeLevel + active + msg_zh + msg_en 任一变化

private:
    std::unique_ptr<IDataSource> m_source;
    std::unique_ptr<IDataBinder> m_binder;

    // 拿到 QtDataBinder 的具体指针（用于 Q_PROPERTY 透传）
    class QtDataBinder* m_qtBinder = nullptr;
    // 拿到 ShmDataSource 的具体指针（用于 resetTrip 这种"反方向"操作）
    class ShmDataSource* m_shmSource = nullptr;
};

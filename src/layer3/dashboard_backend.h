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

    Q_INVOKABLE QVariant get(const QString& key) const;
    Q_INVOKABLE void set(const QString& key, const QVariant& value);
    Q_INVOKABLE bool indicatorOn(const QString& key) const;
    Q_INVOKABLE void setIndicator(const QString& widget_id, bool on, bool flash, float hz);
    Q_INVOKABLE QString tr(const QString& key) const;
    Q_INVOKABLE void setLanguage(const QString& lang);
    // QML 端 "重置小计" 按钮调用
    Q_INVOKABLE void resetTrip();
    // 主题 (PR 7): 透传到 ShmDataSource.m_theme, 下次 16ms tick 自动反映到 QML
    Q_INVOKABLE void setThemeMode(int mode);  // 0=DAY, 1=NIGHT, 2=AUTO
    Q_INVOKABLE void resetTheme();

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

private:
    std::unique_ptr<IDataSource> m_source;
    std::unique_ptr<IDataBinder> m_binder;

    // 拿到 QtDataBinder 的具体指针（用于 Q_PROPERTY 透传）
    class QtDataBinder* m_qtBinder = nullptr;
    // 拿到 ShmDataSource 的具体指针（用于 resetTrip 这种"反方向"操作）
    class ShmDataSource* m_shmSource = nullptr;
};

// dashboard_backend.cpp
// DashboardBackend 胶水层实现

#include "dashboard_backend.h"
#include "shm_data_source.h"
#include "qt_data_binder.h"
#include "layer2/language_manager.h"  // TRANSLATIONS, TRANSLATION_COUNT
#include "file_logger.h"
#include "qml_logger_bridge.h"

#include <QDebug>
#include <QVariant>

// 前向声明 QtDataBinder 的具体方法（避免循环 include）
// 注：实际转发通过 m_qtBinder 调用

DashboardBackend::DashboardBackend(QObject* parent) : QObject(parent) {}
DashboardBackend::~DashboardBackend() = default;

void DashboardBackend::init() {
    // 安装 FileLogger 的 qInstallMessageHandler
    FileLogger::instance().installMessageHandler();
    FileLogger::instance().info(QStringLiteral("QmlDataBinder"),
                                QStringLiteral("QmlDataBinder started"));

    // 默认：ShmDataSource + QtDataBinder
    auto source = std::make_unique<ShmDataSource>();
    auto binder = std::make_unique<QtDataBinder>();

    // i18n 初始化
    binder->initLanguage(TRANSLATIONS, TRANSLATION_COUNT);

    // 缓存 QtDataBinder 指针（用于 Q_PROPERTY 透传）
    m_qtBinder = binder.get();

    // 转发 QtDataBinder 信号到 DashboardBackend（QML 才能监听到）
    connect(m_qtBinder, &QtDataBinder::displayDataChanged, this, &DashboardBackend::displayDataChanged);
    connect(m_qtBinder, &QtDataBinder::indicatorStatesChanged, this, &DashboardBackend::indicatorStatesChanged);
    connect(m_qtBinder, &QtDataBinder::alarmActiveChanged, this, &DashboardBackend::alarmActiveChanged);
    connect(m_qtBinder, &QtDataBinder::seatBeltChanged, this, &DashboardBackend::seatBeltChanged);
    connect(m_qtBinder, &QtDataBinder::movingChanged, this, &DashboardBackend::movingChanged);
    connect(m_qtBinder, &QtDataBinder::healthChanged, this, &DashboardBackend::healthChanged);
    connect(m_qtBinder, &QtDataBinder::dataHealthChanged, this, &DashboardBackend::dataHealthChanged);
    connect(m_qtBinder, &QtDataBinder::languageChanged, this, &DashboardBackend::languageChanged);
    connect(m_qtBinder, &QtDataBinder::tripChanged, this, &DashboardBackend::tripChanged);  // v3 探针延伸
    connect(m_qtBinder, &QtDataBinder::themeChanged, this, &DashboardBackend::themeChanged);  // PR 7
    connect(m_qtBinder, &QtDataBinder::warningChanged, this, &DashboardBackend::warningChanged);  // PR 9
    connect(m_qtBinder, &QtDataBinder::settingsChanged, this, &DashboardBackend::settingsChanged);  // PR 13
    connect(m_qtBinder, &QtDataBinder::viewChanged,     this, &DashboardBackend::viewChanged);      // PR 13
    connect(m_qtBinder, &QtDataBinder::chimeChanged,    this, &DashboardBackend::chimeChanged);     // PR 14
    connect(m_qtBinder, &QtDataBinder::selfTestChanged, this, &DashboardBackend::selfTestChanged);  // PR 17
    connect(m_qtBinder, &QtDataBinder::limpHomeChanged, this, &DashboardBackend::limpHomeChanged);  // PR 44

    // 业务注入
    m_binder = std::move(binder);
    m_source = std::move(source);
    m_shmSource = dynamic_cast<ShmDataSource*>(m_source.get());

    // 连接 DataSource → Binder + 启动
    m_source->setUpdateCallback([this](const DisplaySnapshot& s) {
        m_binder->onDataUpdated(s);
    });
    m_source->setHealthCallback([this](HealthStatus h) {
        m_binder->onHealthChanged(h);
    });
    m_source->start();
}

void DashboardBackend::setDataSource(std::unique_ptr<IDataSource> source) {
    m_source = std::move(source);
    m_shmSource = dynamic_cast<ShmDataSource*>(m_source.get());

    // 连接 DataSource → Binder（任何 binder 都行）
    if (m_source && m_binder) {
        m_source->setUpdateCallback([this](const DisplaySnapshot& s) {
            m_binder->onDataUpdated(s);
        });
        m_source->setHealthCallback([this](HealthStatus h) {
            m_binder->onHealthChanged(h);
        });
        m_source->start();
    }
}

void DashboardBackend::setDataBinder(std::unique_ptr<IDataBinder> binder) {
    // 尝试把 binder 当作 QtDataBinder（如果是的话，cache 指针 + connect signals）
    // 使用 dynamic_cast（需 RTTI，已默认开）
    if (auto* qt = dynamic_cast<QtDataBinder*>(binder.get())) {
        m_qtBinder = qt;
        // 转发信号到 DashboardBackend
        connect(m_qtBinder, &QtDataBinder::displayDataChanged, this, &DashboardBackend::displayDataChanged);
        connect(m_qtBinder, &QtDataBinder::indicatorStatesChanged, this, &DashboardBackend::indicatorStatesChanged);
        connect(m_qtBinder, &QtDataBinder::alarmActiveChanged, this, &DashboardBackend::alarmActiveChanged);
        connect(m_qtBinder, &QtDataBinder::seatBeltChanged, this, &DashboardBackend::seatBeltChanged);
        connect(m_qtBinder, &QtDataBinder::movingChanged, this, &DashboardBackend::movingChanged);
        connect(m_qtBinder, &QtDataBinder::healthChanged, this, &DashboardBackend::healthChanged);
        connect(m_qtBinder, &QtDataBinder::dataHealthChanged, this, &DashboardBackend::dataHealthChanged);
        connect(m_qtBinder, &QtDataBinder::languageChanged, this, &DashboardBackend::languageChanged);
        connect(m_qtBinder, &QtDataBinder::tripChanged, this, &DashboardBackend::tripChanged);  // v3 探针延伸
        connect(m_qtBinder, &QtDataBinder::themeChanged, this, &DashboardBackend::themeChanged);  // PR 7
        connect(m_qtBinder, &QtDataBinder::warningChanged, this, &DashboardBackend::warningChanged);  // PR 9
        connect(m_qtBinder, &QtDataBinder::chimeChanged,    this, &DashboardBackend::chimeChanged);  // PR 14
        connect(m_qtBinder, &QtDataBinder::selfTestChanged, this, &DashboardBackend::selfTestChanged);  // PR 17
        connect(m_qtBinder, &QtDataBinder::limpHomeChanged, this, &DashboardBackend::limpHomeChanged);  // PR 44
    }
    m_binder = std::move(binder);
}

// ─── 透传实现 ───
QVariantMap DashboardBackend::displayData() const { return m_qtBinder ? m_qtBinder->displayData() : QVariantMap(); }
QVariantMap DashboardBackend::indicatorStates() const { return m_qtBinder ? m_qtBinder->indicatorStates() : QVariantMap(); }
bool DashboardBackend::alarmActive() const { return m_qtBinder && m_qtBinder->alarmActive(); }
QString DashboardBackend::alarmMessageZh() const { return m_qtBinder ? m_qtBinder->alarmMessageZh() : QString(); }
QVariantList DashboardBackend::alarmList() const { return m_qtBinder ? m_qtBinder->alarmList() : QVariantList(); }
bool DashboardBackend::seatBeltWarningActive() const { return m_qtBinder && m_qtBinder->seatBeltWarningActive(); }
QString DashboardBackend::seatBeltMessage() const { return m_qtBinder ? m_qtBinder->seatBeltMessage() : QString(); }
QVariantList DashboardBackend::seatIconStates() const { return m_qtBinder ? m_qtBinder->seatIconStates() : QVariantList(); }
bool DashboardBackend::isMoving() const { return m_qtBinder && m_qtBinder->isMoving(); }
bool DashboardBackend::processorOnline() const { return m_qtBinder && m_qtBinder->processorOnline(); }
QString DashboardBackend::processorStatus() const { return m_qtBinder ? m_qtBinder->processorStatus() : QString(); }
qulonglong DashboardBackend::dataAgeMs() const { return m_qtBinder ? m_qtBinder->dataAgeMs() : 0; }
qulonglong DashboardBackend::frameSeq() const { return m_qtBinder ? m_qtBinder->frameSeq() : 0; }
double DashboardBackend::dataFps() const { return m_qtBinder ? m_qtBinder->dataFps() : 0.0; }
qulonglong DashboardBackend::droppedFrames() const { return m_qtBinder ? m_qtBinder->droppedFrames() : 0; }
QVariantMap DashboardBackend::fieldValidity() const { return m_qtBinder ? m_qtBinder->fieldValidity() : QVariantMap(); }

float DashboardBackend::tripDistanceKm() const   { return m_qtBinder ? m_qtBinder->tripDistanceKm()   : 0.0f; }
float DashboardBackend::tripAvgSpeedKmh() const  { return m_qtBinder ? m_qtBinder->tripAvgSpeedKmh()  : 0.0f; }
uint  DashboardBackend::tripDurationS() const    { return m_qtBinder ? m_qtBinder->tripDurationS()    : 0; }
bool  DashboardBackend::tripIsMoving() const     { return m_qtBinder && m_qtBinder->tripIsMoving(); }
float DashboardBackend::tripEnergyKWh() const          { return m_qtBinder ? m_qtBinder->tripEnergyKWh()          : 0.0f; }
float DashboardBackend::tripEfficiencyKWh100Km() const{ return m_qtBinder ? m_qtBinder->tripEfficiencyKWh100Km(): 0.0f; }
float DashboardBackend::tripRangeConfidencePct() const{ return m_qtBinder ? m_qtBinder->tripRangeConfidencePct(): 100.0f; }

// 主题 getter 透传 (PR 7)
int   DashboardBackend::themeMode() const              { return m_qtBinder ? m_qtBinder->themeMode()              : 2; }
bool  DashboardBackend::themeIsDay() const             { return m_qtBinder && m_qtBinder->themeIsDay(); }
uint  DashboardBackend::themeColorBackground() const   { return m_qtBinder ? m_qtBinder->themeColorBackground()  : 0xFFFFFFFFu; }
uint  DashboardBackend::themeColorForeground() const   { return m_qtBinder ? m_qtBinder->themeColorForeground()  : 0xFF111111u; }
uint  DashboardBackend::themeColorAccent() const       { return m_qtBinder ? m_qtBinder->themeColorAccent()      : 0xFF1976D2u; }
uint  DashboardBackend::themeColorWarning() const      { return m_qtBinder ? m_qtBinder->themeColorWarning()     : 0xFFFFC107u; }
uint  DashboardBackend::themeColorCritical() const     { return m_qtBinder ? m_qtBinder->themeColorCritical()    : 0xFFD32F2Fu; }

// 警告 getter 透传 (PR 9)
QVariantList DashboardBackend::warningActiveList() const { return m_qtBinder ? m_qtBinder->warningActiveList() : QVariantList(); }
int   DashboardBackend::warningCount() const              { return m_qtBinder ? m_qtBinder->warningCount()     : 0; }
bool  DashboardBackend::hasCritical() const               { return m_qtBinder && m_qtBinder->hasCritical(); }

// 自检 getter 透传 (PR 17)
int   DashboardBackend::selfTestStatus() const            { return m_qtBinder ? m_qtBinder->selfTestStatus()            : 0; }
int   DashboardBackend::selfTestCriticalReceived() const  { return m_qtBinder ? m_qtBinder->selfTestCriticalReceived()  : 0; }
int   DashboardBackend::selfTestCriticalTotal() const     { return m_qtBinder ? m_qtBinder->selfTestCriticalTotal()     : 0; }
int   DashboardBackend::selfTestCriticalStuck() const     { return m_qtBinder ? m_qtBinder->selfTestCriticalStuck()     : 0; }
int   DashboardBackend::selfTestWarnStuck() const         { return m_qtBinder ? m_qtBinder->selfTestWarnStuck()         : 0; }
int   DashboardBackend::selfTestOutOfRange() const        { return m_qtBinder ? m_qtBinder->selfTestOutOfRange()        : 0; }

// 跛行模式 getter 透传 (PR 44)
int     DashboardBackend::limpHomeLevel() const     { return m_qtBinder ? m_qtBinder->limpHomeLevel()     : 0; }
bool    DashboardBackend::limpHomeActive() const    { return m_qtBinder && m_qtBinder->limpHomeActive(); }
QString DashboardBackend::limpHomeMessageZh() const { return m_qtBinder ? m_qtBinder->limpHomeMessageZh() : QString(); }
QString DashboardBackend::limpHomeMessageEn() const { return m_qtBinder ? m_qtBinder->limpHomeMessageEn() : QString(); }

// 设置 getter 透传 (PR 13)
int   DashboardBackend::settingsUnits() const             { return m_qtBinder ? m_qtBinder->settingsUnits()     : 0; }
int   DashboardBackend::settingsBrightness() const        { return m_qtBinder ? m_qtBinder->settingsBrightness() : 80; }

// 视图 getter 透传 (PR 13)
int   DashboardBackend::viewMode() const                  { return m_qtBinder ? m_qtBinder->viewMode()     : 0; }
bool  DashboardBackend::isChargeView() const              { return m_qtBinder && m_qtBinder->isChargeView(); }
int   DashboardBackend::viewGear() const                  { return m_qtBinder ? m_qtBinder->viewGear()     : 0; }
int   DashboardBackend::viewCharge() const                { return m_qtBinder ? m_qtBinder->viewCharge()   : 0; }

// 声音 getter 透传 (PR 14)
bool DashboardBackend::chimeActive() const                { return m_qtBinder && m_qtBinder->chimeActive(); }
int  DashboardBackend::chimeSeverity() const              { return m_qtBinder ? m_qtBinder->chimeSeverity()     : 0; }
int  DashboardBackend::chimeFrequencyHz() const           { return m_qtBinder ? m_qtBinder->chimeFrequencyHz()  : 0; }
int  DashboardBackend::chimeDurationMs() const            { return m_qtBinder ? m_qtBinder->chimeDurationMs()   : 0; }
int  DashboardBackend::chimeRepeatCount() const           { return m_qtBinder ? m_qtBinder->chimeRepeatCount()  : 0; }
int  DashboardBackend::chimeVolumePct() const             { return m_qtBinder ? m_qtBinder->chimeVolumePct()    : 80; }

void DashboardBackend::resetTrip() {
    // 通过具体指针调 (而不是 IDataSource 接口), 避免污染抽象边界
    if (m_shmSource) m_shmSource->resetTripForTest();
}

void DashboardBackend::setThemeMode(int mode) {
    // 0=DAY, 1=NIGHT, 2=AUTO — 透传到 ShmDataSource.m_theme, 下次 16ms tick 自动反映
    if (m_shmSource) {
        m_shmSource->setThemeModeForTest(static_cast<candash::ThemeMode>(mode));
    }
}

void DashboardBackend::resetTheme() {
    if (m_shmSource) m_shmSource->resetThemeForTest();
}

// 设置 (PR 13) — 透传到 ShmDataSource.m_settings, 下次 16ms tick 自动反映
void DashboardBackend::setSettingsUnits(int units) {
    if (m_shmSource) {
        m_shmSource->setSettingsUnitsForTest(static_cast<uint8_t>(units));
    }
}
void DashboardBackend::setSettingsBrightness(int pct) {
    if (m_shmSource) {
        m_shmSource->setSettingsBrightnessForTest(static_cast<uint8_t>(pct));
    }
}
void DashboardBackend::resetSettings() {
    if (m_shmSource) m_shmSource->resetSettingsForTest();
}

// 声音 (PR 14) — 透传到 ShmDataSource.m_chime, 下次 16ms tick 自动反映到 QML
void DashboardBackend::setChimeEnabled(bool enabled) {
    if (m_shmSource) m_shmSource->setChimeEnabledForTest(enabled);
}
void DashboardBackend::setChimeVolume(int pct) {
    if (m_shmSource) m_shmSource->setChimeVolumeForTest(static_cast<uint8_t>(pct));
}
void DashboardBackend::resetChime() {
    if (m_shmSource) m_shmSource->resetChimeForTest();
}

// 自检 (PR 17) — 透传到 ShmDataSource.m_self_test
void DashboardBackend::resetSelfTest() {
    if (m_shmSource) m_shmSource->resetSelfTestForTest();
}

QVariant DashboardBackend::get(const QString& key) const { return m_qtBinder ? m_qtBinder->get(key) : QVariant(); }
void DashboardBackend::set(const QString& key, const QVariant& value) { if (m_qtBinder) m_qtBinder->set(key, value); }
bool DashboardBackend::indicatorOn(const QString& key) const { return m_qtBinder && m_qtBinder->indicatorOn(key); }
void DashboardBackend::setIndicator(const QString& widget_id, bool on, bool flash, float hz) {
    if (m_qtBinder) m_qtBinder->setIndicator(widget_id, on, flash, hz);
}
QString DashboardBackend::tr(const QString& key) const { return m_qtBinder ? m_qtBinder->tr(key) : key; }
void DashboardBackend::setLanguage(const QString& lang) { if (m_qtBinder) m_qtBinder->setLanguage(lang); }

// 日志 (REQ-LOG-002): 写 FileLogger + 转发 QmlLogger
void DashboardBackend::logInfo(const QString& source, const QString& message) {
    FileLogger::instance().info(source, message);
    QmlLoggerBridge::instance().forwardLog(1, source, message);
}
void DashboardBackend::logWarn(const QString& source, const QString& message) {
    FileLogger::instance().warn(source, message);
    QmlLoggerBridge::instance().forwardLog(2, source, message);
}
void DashboardBackend::logError(const QString& source, const QString& message) {
    FileLogger::instance().error(source, message);
    QmlLoggerBridge::instance().forwardLog(3, source, message);
}
void DashboardBackend::logDebug(const QString& source, const QString& message) {
    FileLogger::instance().debug(source, message);
    QmlLoggerBridge::instance().forwardLog(0, source, message);
}

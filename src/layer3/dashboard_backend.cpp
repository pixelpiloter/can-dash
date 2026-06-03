// dashboard_backend.cpp
// DashboardBackend 胶水层实现

#include "dashboard_backend.h"
#include "shm_data_source.h"
#include "qt_data_binder.h"
#include "layer2/language_manager.h"  // TRANSLATIONS, TRANSLATION_COUNT

#include <QDebug>
#include <QVariant>

// 前向声明 QtDataBinder 的具体方法（避免循环 include）
// 注：实际转发通过 m_qtBinder 调用

DashboardBackend::DashboardBackend(QObject* parent) : QObject(parent) {}
DashboardBackend::~DashboardBackend() = default;

void DashboardBackend::init() {
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

void DashboardBackend::resetTrip() {
    // 通过具体指针调 (而不是 IDataSource 接口), 避免污染抽象边界
    if (m_shmSource) m_shmSource->resetTripForTest();
}

QVariant DashboardBackend::get(const QString& key) const { return m_qtBinder ? m_qtBinder->get(key) : QVariant(); }
void DashboardBackend::set(const QString& key, const QVariant& value) { if (m_qtBinder) m_qtBinder->set(key, value); }
bool DashboardBackend::indicatorOn(const QString& key) const { return m_qtBinder && m_qtBinder->indicatorOn(key); }
void DashboardBackend::setIndicator(const QString& widget_id, bool on, bool flash, float hz) {
    if (m_qtBinder) m_qtBinder->setIndicator(widget_id, on, flash, hz);
}
QString DashboardBackend::tr(const QString& key) const { return m_qtBinder ? m_qtBinder->tr(key) : key; }
void DashboardBackend::setLanguage(const QString& lang) { if (m_qtBinder) m_qtBinder->setLanguage(lang); }

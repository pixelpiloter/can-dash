// shm_data_source.h
// IDataSource 的共享内存实现
//
// 从 /dev/shm/can_display 读取 v1.2 协议数据，转换为 DisplaySnapshot 推送
// 内部：16ms QTimer 轮询（测试时可注入 MockClock）
//
// 不依赖 Qt UI 概念（仅用 QTimer 作定时器，binder 层不感知）

#pragma once

#include "idata_source.h"
#include "layer1/shm/shm_display.h"  // DisplayDataShm 完整定义（用于 convertSnapshot）
#include "layer2/trip_computer.h"     // 派生指标 (v3 探针延伸, 9b88428)
#include <QTimer>
#include <QObject>
#include <atomic>

class ShmDataSource : public QObject, public IDataSource {
    Q_OBJECT
public:
    explicit ShmDataSource(QObject* parent = nullptr);
    ~ShmDataSource() override;

    // IDataSource 接口
    bool start() override;
    void stop() override;
    bool isRunning() const override { return m_running; }
    DisplaySnapshot snapshot() const override { return m_snapshot; }
    HealthStatus health() const override { return m_snapshot.health; }
    void setUpdateCallback(UpdateCallback cb) override { m_updateCb = std::move(cb); }
    void setHealthCallback(HealthCallback cb) override { m_healthCb = std::move(cb); }

    // 测试钩子
    // 注入 tick 间隔（默认 16ms；测试可改为 1ms）
    void setTickIntervalMs(int ms) { m_tickIntervalMs = ms; }

    // 手动触发一次 tick（用于单元测试不依赖定时器）
    void tickForTest() { onTick(); }

    // 重置小计 (Q_INVOKABLE 走 DashboardBackend 暴露给 QML 的 "重置" 按钮)
    void resetTripForTest() { m_trip.reset(); }

private slots:
    void onTick();

private:
    // ─── 业务转换（把 DisplayDataShm 转为 DisplaySnapshot）───
    void convertSnapshot(const DisplayDataShm& shm, DisplaySnapshot& out) const;

    QTimer* m_timer = nullptr;
    int m_tickIntervalMs = 16;

    std::atomic<bool> m_running{false};
    mutable DisplaySnapshot m_snapshot{};
    HealthStatus m_lastHealth = HEALTH_DISCONNECTED;
    uint64_t m_lastCommitTs = 0;
    uint32_t m_lastFrameSeq = 0;
    uint64_t m_droppedFrames = 0;

    // FPS 滑动窗口
    uint64_t m_fpsWindowStart = 0;
    uint32_t m_fpsCountInWindow = 0;
    double m_fps = 0.0;

    // 派生指标 (v3 探针延伸, commit 9b88428)
    TripComputer m_trip;

    // 回调
    UpdateCallback m_updateCb;
    HealthCallback m_healthCb;
};

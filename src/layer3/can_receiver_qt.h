// can_receiver_qt.h
// Layer 3: Qt 事件循环集成的 CAN 数据接收器
//
// 从 Unix Domain Socket (/tmp/can_processor_socket, SOCK_STREAM) 接收 CAN 帧，
// 按协议 [can_id:4B LE][dlc:1B][data:dlc] 解析，然后：
//   - 回调 IDataSource::UpdateCallback（DisplaySnapshot 推送），或
//   - 直接调用 CanSignalMonitor::onCanFrame（信号级监控）
//
// 继承 QObject 以集成 Qt 事件循环（QSocketNotifier 依赖 QObject）。
//
// 设计约束（Layer 2 / Layer 3 规范）：
//   - 禁止 new/malloc：所有缓冲用 static buffer 或栈
//   - 禁止 include <QtCore> 以外的 Qt 头文件（本文件是 Layer 3，QObject 允许）

#pragma once

#include "idata_source.h"
#include "display_data_types.h"   // DisplaySnapshot（typedef struct，不能前向声明）
#include "layer2/can_signal_monitor.h"
#include <QObject>
#include <QSocketNotifier>
#include <QString>

class CanReceiverQt : public QObject {
    Q_OBJECT

public:
    // socket_path: Unix Domain Socket 路径（默认 /tmp/can_processor_socket）
    // monitor: 可选信号监控器（直接 onCanFrame 路径）；传 nullptr 则只用 UpdateCallback
    explicit CanReceiverQt(const char* socket_path = nullptr, CanSignalMonitor* monitor = nullptr,
                          QObject* parent = nullptr);
    ~CanReceiverQt() override;

    // IDataSource 兼容：启动接收器
    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    // IDataSource::UpdateCallback 注册（m_updateCb）
    void setUpdateCallback(IDataSource::UpdateCallback cb);

    // IDataSource 兼容：同步快照（返回空 snapshot，本接收器不缓存 DisplaySnapshot）
    DisplaySnapshot snapshot() const;
    HealthStatus health() const;
    void setHealthCallback(IDataSource::HealthCallback cb);

Q_SIGNALS:
    // 帧到达信号（用于调试 / QML 监听）
    void canFrameReceived(uint32_t can_id, const QByteArray& data);

    // 健康状态变化信号
    void healthChanged(int health);

private Q_SLOTS:
    void onListenReadable();
    void onConnReadable();

private:
    // 内部解析：尝试从 rx_buf_ 头部取出一帧
    // 成功返回 true，out_can_id/out_dlc/out_data 已填充
    // 数据不足返回 false（需继续接收）
    // 协议错误返回 false 并丢弃一字节（同步）
    bool tryParseFrame(uint32_t& out_can_id, uint8_t& out_dlc,
                       uint8_t* out_data, size_t out_capacity);

    // 接受新连接（listen fd 可读时）
    void acceptClient();

    // 关闭当前连接（conn fd 关闭/出错时）
    void closeClient();

    // 处理已读取的字节（追加到 rx_buf_ 并解析）
    void processReceivedData();

    // 切换为监听 conn fd 模式
    void switchToConnMode();
    // 切换为监听 listen fd 模式
    void switchToListenMode();

    // 两个持久 notifier（避免在 slot 内 delete/recreate QSocketNotifier 导致死锁）
    QSocketNotifier* m_listenNotifier = nullptr;
    QSocketNotifier* m_connNotifier = nullptr;

    // 监听套接字（仅用于 accept）
    int m_listenFd = -1;
    // 已连接套接字（数据读写）
    int m_connFd = -1;

    // socket 路径
    char m_socketPath[128] = {};

    // 运行状态
    bool m_running = false;

    // 静态接收缓冲（车规：无动态分配）
    static constexpr size_t RX_BUF_SIZE = 256;
    uint8_t m_rxBuf[RX_BUF_SIZE] = {};
    size_t m_rxLen = 0;  // 已接收字节数

    // 静态帧缓冲（最大 8 字节 data）
    uint8_t m_frameData[8] = {};

    // 可选信号监控器（直接 onCanFrame 路径）
    CanSignalMonitor* m_monitor = nullptr;

    // 回调
    IDataSource::UpdateCallback m_updateCb;
    IDataSource::HealthCallback m_healthCb;
};

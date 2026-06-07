// can_receiver_qt.cpp
// Layer 3: Qt 事件循环集成的 CAN 数据接收器实现
//
// 设计要点：
//   - 使用两个持久 QSocketNotifier（listen / conn），通过 setEnabled() 切换
//   - 避免在 slot 内 delete/recreate QSocketNotifier（Qt 死锁陷阱）
//   - listen fd 和 conn fd 都设为 O_NONBLOCK（accept() 继承 listen fd 标志）
//   - 协议：[can_id:4B LE][dlc:1B][data:dlc]，DLC 0-8 合法

#include "can_receiver_qt.h"
#include "layer1/shm/shm_display.h"   // SOCKET_PATH
#include "file_logger.h"

#include <QSocketNotifier>
#include <QCoreApplication>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>

CanReceiverQt::CanReceiverQt(const char* socket_path, CanSignalMonitor* monitor, QObject* parent)
    : QObject(parent)
    , m_monitor(monitor)
{
    // 初始化 socket 路径：参数 > 环境变量 > 编译时默认
    const char* path = socket_path ? socket_path : socket_get_path();
    std::strncpy(m_socketPath, path, sizeof(m_socketPath) - 1);
    m_socketPath[sizeof(m_socketPath) - 1] = '\0';
}

CanReceiverQt::~CanReceiverQt() {
    stop();
}

bool CanReceiverQt::start() {
    if (m_running) return true;

    // 创建 Unix Domain Socket（SOCK_STREAM）
    m_listenFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        FileLogger::instance().error(QStringLiteral("CanReceiverQt"),
            QStringLiteral("socket() failed: %1").arg(std::strerror(errno)));
        return false;
    }

    // 设置非阻塞 accept + read
    int flags = fcntl(m_listenFd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    (void)fcntl(m_listenFd, F_SETFL, flags | O_NONBLOCK);

    // bind + listen
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socketPath, sizeof(addr.sun_path) - 1);

    // 先 unlink 旧 socket（防止 stale）
    ::unlink(m_socketPath);

    if (::bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        FileLogger::instance().error(QStringLiteral("CanReceiverQt"),
            QStringLiteral("bind() failed: %1").arg(std::strerror(errno)));
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    if (::listen(m_listenFd, 1) < 0) {
        FileLogger::instance().error(QStringLiteral("CanReceiverQt"),
            QStringLiteral("listen() failed: %1").arg(std::strerror(errno)));
        ::close(m_listenFd);
        m_listenFd = -1;
        ::unlink(m_socketPath);
        return false;
    }

    // 创建两个持久 notifier，初始都禁用
    m_listenNotifier = new QSocketNotifier(m_listenFd, QSocketNotifier::Read, this);
    m_listenNotifier->setEnabled(false);
    connect(m_listenNotifier, &QSocketNotifier::activated, this, &CanReceiverQt::onListenReadable);

    m_connNotifier = nullptr;  // conn notifier 在第一次 accept 后创建

    // 启用 listen notifier，进入监听模式
    m_listenNotifier->setEnabled(true);

    m_running = true;
    FileLogger::instance().info(QStringLiteral("CanReceiverQt"),
        QStringLiteral("Listening on %1").arg(m_socketPath));
    return true;
}

void CanReceiverQt::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_listenNotifier) {
        m_listenNotifier->setEnabled(false);
        delete m_listenNotifier;
        m_listenNotifier = nullptr;
    }
    if (m_connNotifier) {
        m_connNotifier->setEnabled(false);
        delete m_connNotifier;
        m_connNotifier = nullptr;
    }

    if (m_connFd >= 0) {
        ::close(m_connFd);
        m_connFd = -1;
    }
    if (m_listenFd >= 0) {
        ::close(m_listenFd);
        m_listenFd = -1;
    }

    // 不 unlink socket path（下次 start 会在 bind 前 unlink 清理）
    FileLogger::instance().info(QStringLiteral("CanReceiverQt"),
        QStringLiteral("Stopped."));
}

void CanReceiverQt::onListenReadable() {
    // listen fd 可读：有新连接到达
    acceptClient();
}

void CanReceiverQt::onConnReadable() {
    // conn fd 可读：读取数据
    if (m_connFd < 0) {
        // conn fd 已被关闭，禁用 notifier
        if (m_connNotifier) m_connNotifier->setEnabled(false);
        return;
    }

    // 循环读取直到 EAGAIN
    uint8_t buf[128];
    bool disconnected = false;
    while (true) {
        ssize_t n = ::read(m_connFd, buf, sizeof(buf));
        if (n > 0) {
            // 追加到 rx_buf
            if (m_rxLen + static_cast<size_t>(n) > RX_BUF_SIZE) {
                FileLogger::instance().warn(QStringLiteral("CanReceiverQt"),
                    QStringLiteral("RX buffer overflow, resetting."));
                m_rxLen = 0;
            }
            std::memcpy(m_rxBuf + m_rxLen, buf, static_cast<size_t>(n));
            m_rxLen += static_cast<size_t>(n);
        } else if (n == 0) {
            // 客户端关闭连接
            FileLogger::instance().info(QStringLiteral("CanReceiverQt"),
                QStringLiteral("Client disconnected."));
            disconnected = true;
            break;
        } else {
            // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 无更多数据，退出循环
                break;
            }
            FileLogger::instance().warn(QStringLiteral("CanReceiverQt"),
                QStringLiteral("read() failed: %1").arg(std::strerror(errno)));
            disconnected = true;
            break;
        }
    }

    // 解析缓冲中的所有完整帧
    processReceivedData();

    if (disconnected) {
        closeClient();
        switchToListenMode();
        if (m_healthCb) m_healthCb(HEALTH_DISCONNECTED);
        Q_EMIT healthChanged(HEALTH_DISCONNECTED);
    }
}

void CanReceiverQt::acceptClient() {
    if (m_listenFd < 0) return;

    // 接受连接（listen fd 是非阻塞，accept 不会阻塞）
    m_connFd = ::accept(m_listenFd, nullptr, nullptr);
    if (m_connFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            FileLogger::instance().warn(QStringLiteral("CanReceiverQt"),
                QStringLiteral("accept() failed: %1").arg(std::strerror(errno)));
        }
        return;
    }

    // conn fd 继承 listen fd 的 O_NONBLOCK
    // 但保险起见再 set 一次
    int flags = fcntl(m_connFd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    (void)fcntl(m_connFd, F_SETFL, flags | O_NONBLOCK);

    // 创建 conn notifier 并启用
    m_connNotifier = new QSocketNotifier(m_connFd, QSocketNotifier::Read, this);
    m_connNotifier->setEnabled(false);
    connect(m_connNotifier, &QSocketNotifier::activated, this, &CanReceiverQt::onConnReadable);

    switchToConnMode();

    // 重置 rx buffer（新连接）
    m_rxLen = 0;

    FileLogger::instance().info(QStringLiteral("CanReceiverQt"),
        QStringLiteral("Client connected."));
    if (m_healthCb) m_healthCb(HEALTH_WAITING);
    Q_EMIT healthChanged(HEALTH_WAITING);
}

void CanReceiverQt::closeClient() {
    if (m_connNotifier) {
        m_connNotifier->setEnabled(false);
        delete m_connNotifier;
        m_connNotifier = nullptr;
    }
    if (m_connFd >= 0) {
        ::close(m_connFd);
        m_connFd = -1;
    }
    // 清空 rx buffer
    m_rxLen = 0;
}

void CanReceiverQt::processReceivedData() {
    uint32_t can_id = 0;
    uint8_t dlc = 0;
    while (tryParseFrame(can_id, dlc, m_frameData, sizeof(m_frameData))) {
        // 发射调试信号
        Q_EMIT canFrameReceived(can_id, QByteArray(reinterpret_cast<char*>(m_frameData), dlc));

        // 直接回调 CanSignalMonitor（信号级监控）
        // can_signal_monitor.h 的 onCanFrame(float value) 只用于"单个浮点值"信号。
        // 我们只有 raw CAN 帧，无法从中提取 float 值。
        // 因此这里仅支持 IDataSource::UpdateCallback 路径。
        if (m_monitor) {
            // 见上方注释：本接收器只传递 raw 帧，signal-level 转换由 can_converter 完成
            (void)m_monitor;
        }

        // IDataSource::UpdateCallback 路径：
        // CanReceiverQt 不持有 DisplaySnapshot，不实现完整数据转换。
        // 使用者应通过 setUpdateCallback 注册回调，在回调中调用 can_converter::processFrame()
        if (m_updateCb) {
            DisplaySnapshot snap = {};
            snap.health = HEALTH_OK;
            m_updateCb(snap);
        }
    }
}

void CanReceiverQt::switchToConnMode() {
    if (m_listenNotifier) m_listenNotifier->setEnabled(false);
    if (m_connNotifier) m_connNotifier->setEnabled(true);
}

void CanReceiverQt::switchToListenMode() {
    if (m_connNotifier) m_connNotifier->setEnabled(false);
    if (m_listenNotifier) m_listenNotifier->setEnabled(true);
}

bool CanReceiverQt::tryParseFrame(uint32_t& out_can_id, uint8_t& out_dlc,
                                  uint8_t* out_data, size_t out_capacity) {
    // 协议：[can_id:4B LE][dlc:1B][data:dlc]
    // 循环处理：支持连续丢弃多个非法帧
    while (m_rxLen >= 5) {
        // 读取 header
        uint32_t can_id = 0;
        uint8_t dlc = 0;
        std::memcpy(&can_id, m_rxBuf + 0, 4);
        std::memcpy(&dlc, m_rxBuf + 4, 1);

        // DLC 合法性检查（0-8）
        if (dlc > 8) {
            // 非法 DLC：跳过此帧的剩余字节（dlc-1 个未读字节）
            FileLogger::instance().warn(QStringLiteral("CanReceiverQt"),
                QStringLiteral("Invalid DLC %1, skipping %2 bytes").arg(dlc).arg(dlc - 1));
            size_t bytes_to_skip = static_cast<size_t>(dlc) - 1;
            if (bytes_to_skip > m_rxLen) bytes_to_skip = m_rxLen;
            if (bytes_to_skip > 0) {
                m_rxLen -= bytes_to_skip;
                std::memmove(m_rxBuf, m_rxBuf + bytes_to_skip, m_rxLen);
            }
            continue;
        }

        const size_t frame_len = 5 + dlc;
        if (m_rxLen < frame_len) {
            // 数据不足：返回 false，调用者应等待更多数据后再试
            return false;
        }

        // 完整帧：填充输出
        out_can_id = can_id;
        out_dlc = dlc;
        if (dlc > 0 && out_data && out_capacity >= dlc) {
            std::memcpy(out_data, m_rxBuf + 5, dlc);
        }

        // 移动缓冲
        m_rxLen -= frame_len;
        std::memmove(m_rxBuf, m_rxBuf + frame_len, m_rxLen);
        return true;
    }
    return false;  // 数据不足以解析 header
}

void CanReceiverQt::setUpdateCallback(IDataSource::UpdateCallback cb) {
    m_updateCb = std::move(cb);
}

DisplaySnapshot CanReceiverQt::snapshot() const {
    DisplaySnapshot snap = {};
    snap.health = m_running ? HEALTH_WAITING : HEALTH_DISCONNECTED;
    return snap;
}

HealthStatus CanReceiverQt::health() const {
    return m_running ? HEALTH_WAITING : HEALTH_DISCONNECTED;
}

void CanReceiverQt::setHealthCallback(IDataSource::HealthCallback cb) {
    m_healthCb = std::move(cb);
}

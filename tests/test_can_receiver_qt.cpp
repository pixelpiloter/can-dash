// tests/test_can_receiver_qt.cpp
// CanReceiverQt 单元测试
//
// 覆盖：
//   1. 正常帧接收：发送一帧，验证 canFrameReceived 信号触发
//   2. socket 断开重连：client 断开，验证健康状态变化
//   3. 非法帧处理：发送一个完整 bad 帧（DLC=9），验证不触发信号；
//      然后发送 good 帧，验证 receiver 仍然正常工作
//
// 测试方法：
//   - 用临时 Unix Domain Socket（/tmp/candash_test_receiver.sock）
//   - CanReceiverQt.start() 创建监听 socket
//   - client 连接并发送测试帧
//   - 用 QTimer::singleShot 驱动 Qt 事件循环处理 socket I/O

#include "layer3/can_receiver_qt.h"
#include "layer3/display_data_types.h"

#include <QCoreApplication>
#include <QTimer>
#include <QByteArray>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { \
        g_test_passed++; \
        printf("  PASS: %s\n", msg); \
    } else { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

// 工具：发一帧到 socket
static bool sendFrame(int fd, uint32_t can_id, uint8_t dlc, const uint8_t* data) {
    uint8_t header[5] = {};
    std::memcpy(header + 0, &can_id, 4);
    header[4] = dlc;
    if (::write(fd, header, 5) != 5) return false;
    if (dlc > 0 && data) {
        if (::write(fd, data, dlc) != static_cast<ssize_t>(dlc)) return false;
    }
    return true;
}

// 工具：驱动 Qt 事件循环 ms 毫秒
static void pumpEvents(QCoreApplication& app, int ms) {
    QTimer::singleShot(ms, &app, &QCoreApplication::quit);
    app.exec();
}

// ── 测试 1: 正常帧接收 ──────────────────────────────────────
static bool g_frame_received = false;
static uint32_t g_frame_can_id = 0;
static QByteArray g_frame_data;

static void test_normal_frame_reception(const char* sock_path, QCoreApplication& app) {
    printf("  [test] normal_frame_reception\n");
    ::unlink(sock_path);
    g_frame_received = false;
    g_frame_can_id = 0;
    g_frame_data.clear();

    CanReceiverQt receiver(sock_path, nullptr);
    QObject::connect(&receiver, &CanReceiverQt::canFrameReceived,
                     [](uint32_t can_id, const QByteArray& data) {
        g_frame_received = true;
        g_frame_can_id = can_id;
        g_frame_data = data;
    });

    receiver.start();
    TEST_ASSERT(receiver.isRunning(), "receiver started");

    // 连接 client
    int client_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    TEST_ASSERT(client_fd >= 0, "client socket created");

    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    // 等待 receiver 启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool connected = (::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    TEST_ASSERT(connected, "client connected to receiver");

    // 发送一帧: can_id=0x123, dlc=3, data=[0xAA,0xBB,0xCC]
    const uint8_t data[] = {0xAA, 0xBB, 0xCC};
    bool sent = sendFrame(client_fd, 0x123, 3, data);
    TEST_ASSERT(sent, "frame sent");

    // 驱动 Qt 事件循环处理 I/O
    pumpEvents(app, 100);

    TEST_ASSERT(g_frame_received, "frame received signal fired");
    TEST_ASSERT(g_frame_can_id == 0x123, "can_id decoded correctly");
    TEST_ASSERT(g_frame_data.size() == 3
                && static_cast<uint8_t>(g_frame_data[0]) == 0xAA
                && static_cast<uint8_t>(g_frame_data[1]) == 0xBB
                && static_cast<uint8_t>(g_frame_data[2]) == 0xCC,
               "data decoded correctly");

    ::close(client_fd);
    receiver.stop();
    ::unlink(sock_path);
}

// ── 测试 2: socket 断开重连 ─────────────────────────────────
static bool g_disconnected_received = false;
static bool g_connected_received = false;

static void test_socket_disconnect_reconnect(const char* sock_path, QCoreApplication& app) {
    printf("  [test] socket_disconnect_reconnect\n");
    ::unlink(sock_path);
    g_disconnected_received = false;
    g_connected_received = false;

    CanReceiverQt receiver(sock_path, nullptr);
    receiver.setHealthCallback([](HealthStatus h) {
        if (h == HEALTH_DISCONNECTED) g_disconnected_received = true;
        if (h == HEALTH_WAITING) g_connected_received = true;
    });
    receiver.start();
    TEST_ASSERT(receiver.isRunning(), "receiver started");

    // 连接 client
    int client_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    (void)::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    pumpEvents(app, 100);
    TEST_ASSERT(g_connected_received, "health cb: connected -> HEALTH_WAITING");

    // client 断开
    ::close(client_fd);
    pumpEvents(app, 100);
    TEST_ASSERT(g_disconnected_received, "health cb: disconnected -> HEALTH_DISCONNECTED");

    receiver.stop();
    ::unlink(sock_path);
}

// ── 测试 3: 非法帧处理（DLC > 8）────────────────────────────
// 策略：
//   1. 先发送一个 good 帧，验证接收正常（基线）
//   2. 再发送一个 bad 帧（DLC=9，协议不支持），验证不触发信号
//   3. 再发送另一个 good 帧，验证 receiver 在 bad 帧后仍能正常工作
//
// 字节布局（分 3 次 send，3 次 pumpEvents）：
//   send #1: [can_id=0x100, dlc=2, data=0x11,0x22] → 7 bytes
//   send #2: [can_id=0x200, dlc=9] → 5 bytes (incomplete, no data sent)
//   send #3: [can_id=0x300, dlc=3, data=0xAA,0xBB,0xCC] → 12 bytes
static void test_invalid_dlc_handling(const char* sock_path, QCoreApplication& app) {
    printf("  [test] invalid_dlc_handling\n");
    ::unlink(sock_path);

    CanReceiverQt receiver(sock_path, nullptr);
    int frame_count = 0;
    uint32_t last_can_id = 0;
    QByteArray last_data;

    QObject::connect(&receiver, &CanReceiverQt::canFrameReceived,
                     [&frame_count, &last_can_id, &last_data](uint32_t can_id, const QByteArray& data) {
        frame_count++;
        last_can_id = can_id;
        last_data = data;
    });
    receiver.start();

    // 连接 client
    int client_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    (void)::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    pumpEvents(app, 50);  // 连接建立

    // Step 1: 发送合法帧 can_id=0x100
    frame_count = 0;
    const uint8_t data1[] = {0x11, 0x22};
    bool sent1 = sendFrame(client_fd, 0x100, 2, data1);
    TEST_ASSERT(sent1, "good frame #1 sent");
    pumpEvents(app, 100);
    TEST_ASSERT(frame_count == 1, "good frame #1 received (frame_count==1)");
    TEST_ASSERT(last_can_id == 0x100, "good frame #1 can_id correct");
    TEST_ASSERT(last_data.size() == 2
                && static_cast<uint8_t>(last_data[0]) == 0x11
                && static_cast<uint8_t>(last_data[1]) == 0x22,
               "good frame #1 data correct");

    // Step 2: 发送非法帧 can_id=0x200, dlc=9（不发送数据）
    // 接收器会尝试解析，发现 dlc=9 > 8，跳过此帧
    frame_count = 0;
    bool sent2 = sendFrame(client_fd, 0x200, 9, nullptr);
    TEST_ASSERT(sent2, "bad frame (dlc=9) sent");
    pumpEvents(app, 100);
    // 帧不完整（没有发送数据），接收器应跳过
    // 注意：由于 dlc=9，接收器会跳过剩余 9 字节。
    // 此时缓冲中只有 5 字节（bad header），skip = min(9-1, 5) = 5，全部跳过。
    // 缓冲清空，等待更多数据。
    TEST_ASSERT(frame_count == 0, "incomplete bad frame not emitted");

    // Step 3: 发送合法帧 can_id=0x300
    frame_count = 0;
    const uint8_t data3[] = {0xAA, 0xBB, 0xCC};
    bool sent3 = sendFrame(client_fd, 0x300, 3, data3);
    TEST_ASSERT(sent3, "good frame #2 sent");
    pumpEvents(app, 100);
    TEST_ASSERT(frame_count == 1, "good frame #2 received after bad frame (frame_count==1)");
    TEST_ASSERT(last_can_id == 0x300, "good frame #2 can_id correct");
    TEST_ASSERT(last_data.size() == 3
                && static_cast<uint8_t>(last_data[0]) == 0xAA
                && static_cast<uint8_t>(last_data[1]) == 0xBB
                && static_cast<uint8_t>(last_data[2]) == 0xCC,
               "good frame #2 data correct");

    ::close(client_fd);
    receiver.stop();
    ::unlink(sock_path);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const char* sock_path = "/tmp/candash_test_receiver.sock";

    printf("test_can_receiver_qt\n");

    test_normal_frame_reception(sock_path, app);
    test_socket_disconnect_reconnect(sock_path, app);
    test_invalid_dlc_handling(sock_path, app);

    printf("\nResults: %d/%d passed\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

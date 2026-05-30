// test_event_bus.cpp
// Layer 2 EventBus 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include "../src/layer2/event_bus.h"

static int subscribe_count = 0;
static int wildcard_count = 0;
static std::vector<std::string> received_keys;
static std::vector<float> received_values;

static void reset_counters() {
    subscribe_count = 0;
    wildcard_count = 0;
    received_keys.clear();
    received_values.clear();
}

static void test_handler(const Event& e) {
    subscribe_count++;
    received_keys.push_back(e.key);
    received_values.push_back(e.value);
}

static void test_wildcard_handler(const Event& e) {
    wildcard_count++;
    received_keys.push_back(e.key);
    received_values.push_back(e.value);
}

int main() {
    printf("=== EventBus 单元测试 ===\n");

    EventBus& bus = EventBus::instance();

    // ─── 测试1：精确订阅 ───
    printf("\n[测试1] 精确订阅 key\n");
    reset_counters();
    int sub1 = bus.subscribe("can.bat_volt", test_handler);
    assert(sub1 > 0);

    bus.publish(Event{"can.bat_volt", 380.5f, 0.0f, 1000, nullptr});
    assert(subscribe_count == 1);
    assert(received_keys[0] == "can.bat_volt");
    assert(received_values[0] == 380.5f);
    printf("  ✓ 精确订阅收到正确值\n");
    bus.unsubscribe(sub1);  // 清理，避免污染后续测试

    // ─── 测试2：不匹配其他 key ───
    printf("\n[测试2] 精确订阅不匹配其他 key\n");
    reset_counters();
    int sub1a = bus.subscribe("can.bat_volt", test_handler);
    bus.publish(Event{"can.speed", 100.0f, 0.0f, 1000, nullptr});
    assert(subscribe_count == 0);
    bus.unsubscribe(sub1a);
    printf("  ✓ 未订阅的 key 不触发回调\n");

    // ─── 测试3：通配符订阅 ───
    printf("\n[测试3] 通配符订阅 can.*\n");
    reset_counters();
    int sub2 = bus.subscribeWildcard("can.*", test_wildcard_handler);

    bus.publish(Event{"can.bat_volt", 400.0f, 0.0f, 1000, nullptr});
    bus.publish(Event{"can.speed", 120.0f, 0.0f, 1000, nullptr});
    bus.publish(Event{"vehicle.mode", 1.0f, 0.0f, 1000, nullptr}); // 不应匹配
    assert(wildcard_count == 2);
    printf("  ✓ 通配符匹配多个 key\n");

    // ─── 测试4：取消订阅 ───
    printf("\n[测试4] 取消订阅\n");
    reset_counters();
    int sub4a = bus.subscribe("can.bat_volt", test_handler);
    bus.unsubscribe(sub4a);

    bus.publish(Event{"can.bat_volt", 410.0f, 0.0f, 1000, nullptr});
    assert(subscribe_count == 0);
    printf("  ✓ 取消订阅后不触发回调\n");

    // ─── 测试5：多个订阅同一 key ───
    printf("\n[测试5] 多个订阅同一 key\n");
    reset_counters();
    int sub3 = bus.subscribe("can.bat_volt", test_handler);
    int sub4 = bus.subscribe("can.bat_volt", test_handler);

    bus.publish(Event{"can.bat_volt", 425.0f, 0.0f, 1000, nullptr});
    assert(subscribe_count == 2);
    printf("  ✓ 多个订阅者均收到消息\n");

    bus.unsubscribe(sub3);
    bus.unsubscribe(sub4);
    bus.unsubscribe(sub2);  // wildcard subscription from test 3

    // ─── 测试6：Event 成员字段传递正确 ───
    printf("\n[测试6] Event 成员字段传递\n");
    reset_counters();
    bus.subscribe("test.event", [](const Event& e) {
        subscribe_count++;
        assert(e.key == "test.event");
        assert(e.value == 99.5f);
        assert(e.prev_value == 88.0f);
        assert(e.timestamp_ms == 12345);
        assert(e.source == (void*)0x1234);
    });
    bus.publish(Event{"test.event", 99.5f, 88.0f, 12345, (void*)0x1234});
    assert(subscribe_count == 1);
    printf("  ✓ Event 所有字段正确传递\n");

    // ─── 测试7：rvalue reference 发布 ───
    printf("\n[测试7] rvalue reference 发布\n");
    reset_counters();
    bus.subscribe("rvalue.test", test_handler);
    bus.publish(Event{"rvalue.test", 77.0f, 0.0f, 1000, nullptr});
    assert(subscribe_count == 1);
    assert(received_values[0] == 77.0f);
    printf("  ✓ rvalue reference 发布成功\n");

    printf("\n所有测试通过。\n");
    return 0;
}

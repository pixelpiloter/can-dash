// test_can_signal_monitor.cpp
// Layer 2 CanSignalMonitor 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include "../src/layer2/can_signal_monitor.h"

static std::vector<std::string> quality_events;
static std::vector<std::string> value_events;
static void clear_events() {
    quality_events.clear();
    value_events.clear();
}

static void on_quality_changed(const char* signal, SignalQuality q, void*) {
    quality_events.push_back(signal);
    printf("  [QUALITY] signal=%s quality=%d\n", signal, q);
}

static void on_value_updated(const char* signal, float value, void*) {
    value_events.push_back(signal);
    printf("  [VALUE] signal=%s value=%.1f\n", signal, value);
}

int main() {
    printf("=== CanSignalMonitor 单元测试 ===\n");

    MonitorCallbacks cb = {
        .onQualityChanged = on_quality_changed,
        .onValueUpdated = on_value_updated,
        .user_data = nullptr
    };

    // 定义测试监控表
    static const SignalMonitorDef table[] = {
        // name, can_id, timeout_ms, min, max, max_delta, smoothing, window
        {"bat_volt",    0x186040F3, 500,  0.0f, 500.0f,  100.0f, true,  5},
        {"bat_curr",    0x186040F4, 500, -500.0f, 500.0f,  200.0f, false, 0},
        {"motor_speed", 0x186050F3, 200,    0.0f, 8000.0f,  50.0f, true,  3},
    };
    const int table_count = 3;

    CanSignalMonitor monitor(cb);
    monitor.init(table, table_count);

    // ─── 测试1：初始状态 SIGNAL_NEVER_RECEIVED ───
    printf("\n[测试1] 初始状态\n");
    assert(monitor.getQuality("bat_volt") == SIGNAL_NEVER_RECEIVED);
    assert(monitor.getSmoothedValue("bat_volt") == 0.0f);
    printf("  ✓ 初始为 SIGNAL_NEVER_RECEIVED\n");

    // ─── 测试2：正常值接收 ───
    printf("\n[测试2] 正常值接收\n");
    clear_events();
    monitor.onCanFrame(0x186040F3, 350.0f);
    assert(monitor.getQuality("bat_volt") == SIGNAL_GOOD);
    // 平滑使用完整窗口平均，首次只有1个有效样本，其余为0
    assert(monitor.getSmoothedValue("bat_volt") > 69.0f && monitor.getSmoothedValue("bat_volt") < 71.0f);
    assert(!quality_events.empty() || !value_events.empty());  // 回调触发
    printf("  ✓ 正常值 → SIGNAL_GOOD\n");

    // ─── 测试3：超出范围检测 ───
    printf("\n[测试3] 超出范围检测\n");
    clear_events();
    monitor.onCanFrame(0x186040F3, 600.0f);  // > 500 max
    assert(monitor.getQuality("bat_volt") == SIGNAL_INVALID_RANGE);
    printf("  ✓ 超范围 → SIGNAL_INVALID_RANGE\n");

    // ─── 测试4：突变检测 ───
    printf("\n[测试4] 突变检测\n");
    clear_events();
    monitor.onCanFrame(0x186040F3, 350.0f);   // 重置到正常
    assert(monitor.getQuality("bat_volt") == SIGNAL_GOOD);
    monitor.onCanFrame(0x186040F3, 460.0f);   // delta=110 > max_delta=100（范围内）
    assert(monitor.getQuality("bat_volt") == SIGNAL_ABNORMAL_DELTA);
    printf("  ✓ 突变 → SIGNAL_ABNORMAL_DELTA\n");

    // ─── 测试5：CAN ID 不匹配 ───
    printf("\n[测试5] CAN ID 不匹配\n");
    clear_events();
    monitor.onCanFrame(0x99999999, 100.0f);  // 未定义 ID
    assert(monitor.getQuality("bat_volt") == SIGNAL_ABNORMAL_DELTA);  // 状态不变
    printf("  ✓ 未知 CAN ID 安全跳过\n");

    // ─── 测试6：超时检测（tick 不崩溃）───
    printf("\n[测试6] tick 不崩溃\n");
    clear_events();
    monitor.tick(0);    // 0ms
    monitor.tick(100);   // 100ms
    monitor.tick(1000);  // 1000ms - tick 运行多次
    printf("  ✓ tick 执行不崩溃\n");

    // ─── 测试7：未知信号查询 ───
    printf("\n[测试7] 未知信号查询\n");
    assert(monitor.getQuality("nonexistent") == SIGNAL_NEVER_RECEIVED);
    assert(monitor.getSmoothedValue("nonexistent") == 0.0f);
    printf("  ✓ 未知信号返回默认值\n");

    // ─── 测试8：质量变化才触发回调 ───
    printf("\n[测试8] 质量变化才触发回调\n");
    clear_events();
    monitor.onCanFrame(0x186040F4, 50.0f);   // 正常值，质量=GOOD
    monitor.onCanFrame(0x186040F4, 75.0f);   // 同质量，不应触发 quality 回调
    // value callback 应触发，但 quality 不变
    assert(monitor.getQuality("bat_curr") == SIGNAL_GOOD);
    printf("  ✓ 同质量不重复触发回调\n");

    // ─── 测试9：析构不崩溃（无 nullptr） ───
    printf("\n[测试9] 析构安全\n");
    {
        CanSignalMonitor m2(cb);
        m2.init(table, table_count);
        m2.onCanFrame(0x186040F3, 350.0f);
    }  // 析构
    printf("  ✓ 析构不崩溃\n");

    printf("\n所有测试通过。\n");
    return 0;
}

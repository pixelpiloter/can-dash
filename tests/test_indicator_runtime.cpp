// test_indicator_runtime.cpp
// Layer 2 IndicatorRuntime 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include "../src/layer2/indicator_runtime.h"

static std::vector<std::string> state_changes;
static void clear_changes() { state_changes.clear(); }

static void on_state_change(const char* id, bool on, bool flash, float hz, void*) {
    state_changes.push_back(id);
    printf("  [STATE_CHANGE] id=%s on=%d flash=%d hz=%.1f\n", id, on, flash, hz);
}

int main() {
    printf("=== IndicatorRuntime 单元测试 ===\n");

    IndicatorCallbacks cb = {
        .onStateChange = on_state_change,
        .user_data = nullptr
    };

    // 定义测试指示灯表
    static const IndicatorDef table[] = {
        {"bat_warn_light",   "warning_bat_red.png",   "warning_bat_dim.png",  60, 60, 600, 260},
        {"engine_warn_light","warning_engine.png",   "warning_engine_dim.png", 60, 60, 540, 260},
        {"turn_left_light",  "turn_left.png",         "turn_left_dim.png",   40, 40, 200, 200},
    };
    const int table_count = 3;

    IndicatorRuntime runtime(cb);
    runtime.init(table, table_count);

    // ─── 测试1：初始状态全部关闭 ───
    printf("\n[测试1] 初始状态全部关闭\n");
    assert(runtime.activeCount() == 0);
    assert(!runtime.isOn("bat_warn_light"));
    assert(!runtime.isOn("engine_warn_light"));
    printf("  ✓ 初始状态全部关闭\n");

    // ─── 测试2：点亮指示灯 ───
    printf("\n[测试2] 点亮指示灯\n");
    clear_changes();
    runtime.setIndicator("bat_warn_light", true, false, 0.0f);
    assert(runtime.isOn("bat_warn_light"));
    assert(runtime.activeCount() == 1);
    assert(state_changes.size() == 1);
    assert(state_changes[0] == "bat_warn_light");
    printf("  ✓ 点亮后 activeCount=1，回调触发\n");

    // ─── 测试3：闪烁指示灯 ───
    printf("\n[测试3] 闪烁指示灯\n");
    clear_changes();
    runtime.setIndicator("engine_warn_light", true, true, 2.0f);
    assert(runtime.isOn("engine_warn_light"));
    assert(state_changes.size() == 1);
    assert(state_changes[0] == "engine_warn_light");
    printf("  ✓ 闪烁参数正确传递\n");

    // ─── 测试4：关闭指示灯 ───
    printf("\n[测试4] 关闭指示灯\n");
    clear_changes();
    runtime.setIndicator("bat_warn_light", false, false, 0.0f);
    assert(!runtime.isOn("bat_warn_light"));
    assert(runtime.activeCount() == 1);  // engine_warn_light 仍亮
    assert(state_changes.size() == 1);
    printf("  ✓ 关闭后 activeCount=1（只剩 engine）\n");

    // ─── 测试5：未知 widget id 不崩溃 ───
    printf("\n[测试5] 未知 widget id 不崩溃\n");
    clear_changes();
    runtime.setIndicator("nonexistent_light", true, false, 0.0f);
    assert(state_changes.empty());
    printf("  ✓ 未知 id 安全跳过\n");

    // ─── 测试6：全部关闭 ───
    printf("\n[测试6] 全部关闭\n");
    runtime.setIndicator("engine_warn_light", false, false, 0.0f);
    assert(runtime.activeCount() == 0);
    printf("  ✓ 全部关闭后 activeCount=0\n");

    // ─── 测试7：tick 不崩溃 ───
    printf("\n[测试7] tick 不崩溃\n");
    runtime.tick(1000);
    runtime.tick(2000);
    assert(runtime.activeCount() == 0);
    printf("  ✓ tick 正确执行\n");

    // ─── 测试8：多次切换状态 ───
    printf("\n[测试8] 多次切换状态\n");
    for (int i = 0; i < 5; i++) {
        runtime.setIndicator("turn_left_light", true, false, 0.0f);
        runtime.setIndicator("turn_left_light", false, false, 0.0f);
    }
    assert(!runtime.isOn("turn_left_light"));
    assert(runtime.activeCount() == 0);
    printf("  ✓ 多次切换稳定\n");

    printf("\n所有测试通过。\n");
    return 0;
}

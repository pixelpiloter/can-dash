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

    // 定义测试指示灯表（字段顺序：id, type, image_on, image_off, x, y, width, height, flash_on_fault）
    static const IndicatorDef table[] = {
        {"bat_warn_light",    "light", "warning_bat_red.png",   "warning_bat_dim.png",   600, 260, 60, 60, false},
        {"engine_warn_light", "light", "warning_engine.png",     "warning_engine_dim.png", 540, 260, 60, 60, false},
        {"turn_left_light",   "light", "turn_left.png",          "turn_left_dim.png",     200, 200, 40, 40, false},
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

    // ─── 测试9：re-init 内存安全 (反复 init 不泄漏 / 不崩溃) ───
    // 背景: indicator_runtime.cpp::init() 早期版本 m_states = new[] 但 re-init 时未 delete[] 旧指针,
    //       反复 init 触发泄漏. 修复后: init 入口先 delete[] 旧 m_states.
    printf("\n[测试9] re-init 内存安全\n");
    {
        IndicatorRuntime rt(cb);
        // 反复 100 次 init — 每次应正确重置 activeCount / 状态表
        for (int i = 0; i < 100; i++) {
            rt.init(table, table_count);
            assert(rt.activeCount() == 0);  // 新 init 后所有灯应关闭
            assert(!rt.isOn("bat_warn_light"));
            assert(!rt.isOn("engine_warn_light"));
        }
        // 100 次 re-init 后功能仍正常
        rt.init(table, table_count);
        rt.setIndicator("bat_warn_light", true, false, 0.0f);
        assert(rt.isOn("bat_warn_light"));
        assert(rt.activeCount() == 1);

        // 替换为更小的表 (1 个指示灯), 旧 m_states 应被释放
        static const IndicatorDef small_table[] = {
            {"only_one", "light", "a.png", "a_dim.png", 0, 0, 50, 50, false},
        };
        rt.init(small_table, 1);
        assert(rt.activeCount() == 0);
        assert(!rt.isOn("bat_warn_light"));  // 旧 widget 不应再存在
        rt.setIndicator("only_one", true, false, 0.0f);
        assert(rt.isOn("only_one"));
    }
    printf("  ✓ re-init 100 次 + 替换表大小 不崩溃, 状态机仍正常\n");

    printf("\n所有测试通过。\n");
    return 0;
}

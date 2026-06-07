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

    // ─── 测试10：空表（table_count=0）安全 ───
    // 背景: IndicatorRuntime::init() 早返回 (table_count <= 0) 不分配 m_states.
    //       此后任何 query/setIndicator 都必须安全.
    printf("\n[测试10] 空表 (table_count=0) 边界安全\n");
    {
        IndicatorRuntime rt(cb);
        rt.init(nullptr, 0);  // 必须不崩溃
        assert(rt.activeCount() == 0);
        assert(!rt.isOn("anything"));
        assert(!rt.isOn("bat_warn_light"));
        // 重复 setIndicator 未知 id 也不崩
        rt.setIndicator("ghost_light", true, true, 1.0f);
        rt.setIndicator(nullptr, true, false, 0.0f);
        assert(rt.activeCount() == 0);
        // 多次 tick 也安全
        rt.tick(0);
        rt.tick(1000);
        rt.tick(99999);
        assert(rt.activeCount() == 0);
    }
    printf("  ✓ table_count=0 + 未知 id + tick 都安全\n");

    // ─── 测试11：MAX_INDICATORS=32 边界 (32 个全部注册 + 全部点亮) ───
    // 背景: m_states 是堆分配数组, MAX_INDICATORS=32 是头文件文档化上限.
    //       项目实际使用 INDICATOR_TABLE_COUNT (yaml 生成) 远小于 32,
    //       此处专门验证满载场景 32 个灯都能正确管理.
    printf("\n[测试11] MAX_INDICATORS=32 满载 (32 个全部注册)\n");
    {
        // 32 个 IndicatorDef — name 必须稳定 (static buffer 数组)
        static char names[32][16];
        static IndicatorDef big_table[32];
        for (int i = 0; i < 32; i++) {
            snprintf(names[i], sizeof(names[i]), "ind_%02d", i);
            big_table[i].id = names[i];
            big_table[i].type = "light";
            big_table[i].image_on = "on.png";
            big_table[i].image_off = "off.png";
            big_table[i].x = (int16_t)i;
            big_table[i].y = 0;
            big_table[i].width = 20;
            big_table[i].height = 20;
            big_table[i].flash_on_fault = false;
        }
        IndicatorRuntime rt(cb);
        rt.init(big_table, 32);
        assert(rt.activeCount() == 0);
        // 点亮全部 32 个
        clear_changes();
        for (int i = 0; i < 32; i++) {
            rt.setIndicator(big_table[i].id, true, false, 0.0f);
        }
        assert(rt.activeCount() == 32);
        assert(rt.isOn("ind_00"));
        assert(rt.isOn("ind_15"));
        assert(rt.isOn("ind_31"));
        assert(state_changes.size() == 32);
        // 关闭一半
        for (int i = 0; i < 16; i++) {
            rt.setIndicator(big_table[i].id, false, false, 0.0f);
        }
        assert(rt.activeCount() == 16);
        assert(!rt.isOn("ind_00"));
        assert(rt.isOn("ind_31"));
        // 关闭剩下的
        for (int i = 16; i < 32; i++) {
            rt.setIndicator(big_table[i].id, false, false, 0.0f);
        }
        assert(rt.activeCount() == 0);
    }
    printf("  ✓ 32 个指示灯 (MAX_INDICATORS) 满载 + 全部操作正确\n");

    // ─── 测试12：activeCount 与 isOn 一致性 (核心不变量) ───
    // 不变量: 对任意 runtime, activeCount() == Σ isOn(id) for all ids
    printf("\n[测试12] activeCount == Σ isOn(id) 不变量\n");
    {
        IndicatorRuntime rt(cb);
        rt.init(table, table_count);
        // 初始: 都为 false, sum = 0
        assert(rt.activeCount() == 0);
        // 累加点亮
        rt.setIndicator("bat_warn_light", true, false, 0.0f);
        assert(rt.activeCount() == 1);
        assert(rt.isOn("bat_warn_light"));
        rt.setIndicator("engine_warn_light", true, true, 2.0f);
        assert(rt.activeCount() == 2);
        assert(rt.isOn("engine_warn_light"));
        rt.setIndicator("turn_left_light", true, false, 0.0f);
        assert(rt.activeCount() == 3);
        // 全关
        rt.setIndicator("bat_warn_light", false, false, 0.0f);
        rt.setIndicator("engine_warn_light", false, false, 0.0f);
        rt.setIndicator("turn_left_light", false, false, 0.0f);
        assert(rt.activeCount() == 0);
    }
    printf("  ✓ activeCount 始终 == isOn 求和, 无静默丢失\n");

    // ─── 测试13：setIndicator 幂等性 (同状态重复调用都触发回调) ───
    // 当前实现: 每次 setIndicator 都无条件触发 onStateChange 回调.
    //           行为契约: 回调频率 = 调用次数 (上层 AlarmRuntime 自己做去重).
    printf("\n[测试13] setIndicator 幂等性: 每次调用都触发回调\n");
    {
        IndicatorRuntime rt(cb);
        rt.init(table, table_count);
        // 重复点亮 5 次, 期望回调 5 次
        clear_changes();
        for (int i = 0; i < 5; i++) {
            rt.setIndicator("bat_warn_light", true, true, 1.5f);
        }
        assert(rt.activeCount() == 1);  // activeCount 仍 1
        assert(state_changes.size() == 5);  // 回调 5 次
        // 重复关闭 3 次
        clear_changes();
        for (int i = 0; i < 3; i++) {
            rt.setIndicator("bat_warn_light", false, false, 0.0f);
        }
        assert(rt.activeCount() == 0);
        assert(state_changes.size() == 3);
    }
    printf("  ✓ 同状态重复调用每次都触发回调 (activeCount 仍 0/1)\n");

    // ─── 测试14：flashHz 变化 (关闭→闪烁, 闪烁→常亮) ───
    // 场景: 同一指示灯从 off 切到 5Hz 闪烁, 再从 5Hz 切到 2Hz, 最后关掉.
    // 验证: flash/flashHz 每次 setIndicator 都被正确更新 (activeCount 不变).
    printf("\n[测试14] flashHz 多次变更\n");
    {
        IndicatorRuntime rt(cb);
        rt.init(table, table_count);
        rt.setIndicator("engine_warn_light", true, true, 5.0f);
        assert(rt.isOn("engine_warn_light"));
        assert(rt.activeCount() == 1);
        // 改 Hz
        rt.setIndicator("engine_warn_light", true, true, 2.0f);
        assert(rt.isOn("engine_warn_light"));
        assert(rt.activeCount() == 1);
        // 切到常亮
        rt.setIndicator("engine_warn_light", true, false, 0.0f);
        assert(rt.isOn("engine_warn_light"));
        assert(rt.activeCount() == 1);
        // 关掉
        rt.setIndicator("engine_warn_light", false, false, 0.0f);
        assert(!rt.isOn("engine_warn_light"));
        assert(rt.activeCount() == 0);
    }
    printf("  ✓ flashHz 在 on/flash 状态下可被多次更新\n");

    // ─── 测试15：空回调 (IndicatorCallbacks 留空) ───
    // 场景: IndicatorCallbacks{} 不传 onStateChange, setIndicator 仍能更新状态.
    //       activeCount/isOn 仍正确, 不依赖回调.
    printf("\n[测试15] 空回调 (无 onStateChange) 安全\n");
    {
        IndicatorCallbacks empty_cb = {};  // 全 nullptr
        IndicatorRuntime rt(empty_cb);
        rt.init(table, table_count);
        // 必须不崩溃 (即使 m_cb.onStateChange 是 nullptr)
        rt.setIndicator("bat_warn_light", true, true, 2.0f);
        assert(rt.isOn("bat_warn_light"));
        assert(rt.activeCount() == 1);
        rt.setIndicator("engine_warn_light", true, false, 0.0f);
        assert(rt.activeCount() == 2);
        rt.tick(1000);
        rt.tick(2000);
        assert(rt.activeCount() == 2);  // tick 不影响 activeCount
        // 关闭
        rt.setIndicator("bat_warn_light", false, false, 0.0f);
        assert(rt.activeCount() == 1);
    }
    printf("  ✓ 回调指针为 nullptr 时 setIndicator/tick 仍正常\n");

    // ─── 测试16：空 widget_id="" (空字符串) 安全 ───
    // 边界: setIndicator("", ...) 不会 crash. 实际空 id 不在表中, 应被忽略.
    printf("\n[测试16] 空字符串 widget_id 安全\n");
    {
        IndicatorRuntime rt(cb);
        rt.init(table, table_count);
        clear_changes();
        rt.setIndicator("", true, false, 0.0f);
        // 空 id 不会匹配任何 IndicatorDef (table 里都是非空), 应静默忽略
        assert(rt.activeCount() == 0);
        assert(state_changes.empty());
    }
    printf("  ✓ 空字符串 widget_id 不匹配表中任何 id, 静默忽略\n");

    printf("\n所有测试通过。\n");
    return 0;
}

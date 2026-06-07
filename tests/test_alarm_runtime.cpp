// test_alarm_runtime.cpp
// Layer 2 AlarmRuntime 单元测试（纯 C++，无 Qt）
//
// 覆盖 alarm_rules.yaml 生成的 18 条规则的关键行为:
//   - 初始状态 (activeCount / isActive / m_anyActive)
//   - 防抖 (duration_ms/100 次连续 onValueChanged 才触发)
//   - 阈值过滤 (< / > / == / 边界值)
//   - display_key 三处一致 (改 bat_volt 不影响 bat_soc 规则)
//   - 多个规则同 key (bat_volt 触发 bat_overvolt + bat_undervolt 二选一)
//   - 清除 (value 跌破阈值后 activeCount 回 0)
//   - acknowledge (确认后同 key 同阈值不再触发)
//   - 回调计数 (onIndicatorUpdate / onAlarmTextUpdate)
//   - getActiveAlarms 返回 status 字段正确
//
// 关键不变量: yaml_to_c.py 生成的 display_key_index 必须正确指向
// DISPLAY_KEY_TABLE, 否则改 bat_volt 会误触发 bat_soc 规则 (PR 7 修过此 bug)

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cmath>
#include <limits>
#include "layer2/alarm_runtime.h"
#include "generated/alarm_rule_def.h"

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do {                                 \
    g_test_count++;                                                 \
    if (cond) {                                                     \
        g_test_passed++;                                            \
        printf("  ✓ %s\n", msg);                                    \
    } else {                                                        \
        printf("  ✗ %s (line %d)\n", msg, __LINE__);                \
    }                                                               \
} while(0)

// ─── 回调计数器 ───
static int g_indicator_count = 0;
static int g_alarm_text_count = 0;
static int g_state_change_count = 0;
static char g_last_alarm_name[64] = {};

void testIndicatorCb(const char* widget, bool on, bool flash, float hz, void*) {
    g_indicator_count++;
    (void)widget; (void)on; (void)flash; (void)hz;
}

void testAlarmTextCb(const char* zh, const char* en, void*) {
    g_alarm_text_count++;
    (void)zh; (void)en;
}

void testStateChangeCb(const char* alarm_name, bool active, void*) {
    g_state_change_count++;
    if (alarm_name) {
        std::strncpy(g_last_alarm_name, alarm_name, sizeof(g_last_alarm_name) - 1);
        g_last_alarm_name[sizeof(g_last_alarm_name) - 1] = '\0';
    }
    (void)active;
}

static void resetCounters() {
    g_indicator_count = 0;
    g_alarm_text_count = 0;
    g_state_change_count = 0;
    g_last_alarm_name[0] = '\0';
}

// 验证两个数组的 display_key_index 都指向 DISPLAY_KEY_TABLE 中的有效条目
// (yaml_to_c.py 致命陷阱 PR 7 修过: 之前硬编码 0, 任何 value 变化都跑所有规则)
static void test_display_key_table_validity() {
    int invalid = 0;
    for (int i = 0; i < ALARM_RULE_TABLE_COUNT; i++) {
        uint8_t idx = ALARM_RULE_TABLE[i].display_key_index;
        if (idx >= DISPLAY_KEY_TABLE_COUNT) {
            invalid++;
            continue;
        }
        const char* key = DISPLAY_KEY_TABLE[idx];
        if (key == nullptr || key[0] == '\0') invalid++;
    }
    TEST_ASSERT(invalid == 0, "所有 18 条规则的 display_key_index 指向 DISPLAY_KEY_TABLE 有效条目");
    TEST_ASSERT(ALARM_RULE_TABLE_COUNT == 18, "alarm_rules.yaml 生成 18 条规则");
    TEST_ASSERT(DISPLAY_KEY_TABLE_COUNT == 28, "can_ids.yaml 生成 28 个 display_key");
    TEST_ASSERT(ALARM_ACTION_TABLE_COUNT == 39, "alarm_rules.yaml 生成 39 个 action");
}

int main() {
    printf("=== AlarmRuntime 单元测试 (PR 21 — 升级 stub 为真实用例) ===\n\n");

    test_display_key_table_validity();

    AlarmCallbacks cb = {
        .onIndicatorUpdate = testIndicatorCb,
        .onAlarmTextUpdate = testAlarmTextCb,
        .onAlarmStateChanged = testStateChangeCb,
        .user_data = nullptr
    };

    // ─── Test 1: 初始状态 ───
    printf("\n[1] 初始状态: 无活跃报警, 所有 isActive 返回 false:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        TEST_ASSERT(rt.activeCount() == 0, "activeCount() == 0 初始");
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "isActive('bat_overvolt') == false 初始");
        TEST_ASSERT(rt.isActive("motor_overspeed") == false, "isActive('motor_overspeed') == false 初始");
        TEST_ASSERT(rt.isActive("nonexistent") == false, "isActive(未知名) == false");
    }

    // ─── Test 2: 触发 bat_overvolt (duration_ms=200, 需要 2 次 onValueChanged) ───
    printf("\n[2] bat_overvolt 防抖: 1 次 onValueChanged 不触发, 2 次触发:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);   // tick_count=1, threshold=2 → 不到
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "1 次 425V → bat_overvolt 未触发 (tick_count=1<2)");
        TEST_ASSERT(rt.activeCount() == 0, "activeCount 仍为 0");

        rt.onValueChanged("bat_volt", 430.0f);   // tick_count=2 → 触发
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "2 次 425V+ → bat_overvolt 触发 (tick_count=2>=2)");
        TEST_ASSERT(rt.activeCount() == 1, "activeCount == 1");
        TEST_ASSERT(g_state_change_count == 1, "onAlarmStateChanged 触发 1 次");
        TEST_ASSERT(std::strcmp(g_last_alarm_name, "bat_overvolt") == 0,
                    "onAlarmStateChanged 收到的 name = 'bat_overvolt'");
        TEST_ASSERT(g_indicator_count >= 1, "onIndicatorUpdate 至少调 1 次 (bat_warn_light flash)");
        TEST_ASSERT(g_alarm_text_count >= 1, "onAlarmTextUpdate 至少调 1 次");
    }

    // ─── Test 3: 阈值边界 (条件不满足时, 不应触发) ───
    printf("\n[3] 阈值边界: bat_volt=350 永远不触发 bat_overvolt (>420):\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        for (int i = 0; i < 20; i++) {
            rt.onValueChanged("bat_volt", 350.0f);
        }
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "20 次 350V → bat_overvolt 不触发");
        TEST_ASSERT(rt.activeCount() == 0, "activeCount 仍为 0");
        TEST_ASSERT(g_state_change_count == 0, "onAlarmStateChanged 未触发");
    }

    // ─── Test 4: 跌破阈值后清除 (trigger → below → cleared) ───
    printf("\n[4] 清除: bat_overvolt 触发后, 跌破阈值 (350V) 应清除:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "触发 bat_overvolt");

        rt.onValueChanged("bat_volt", 350.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "跌破阈值 → bat_overvolt 清除");
        TEST_ASSERT(rt.activeCount() == 0, "activeCount 回到 0");
        TEST_ASSERT(g_state_change_count == 2, "onAlarmStateChanged 调 2 次 (active=true, active=false)");
    }

    // ─── Test 5: display_key 过滤 (改 bat_volt 不应触发 motor_overtemp) ───
    printf("\n[5] display_key 过滤: 推 bat_volt 不影响 motor_overtemp (motor_temp 规则):\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        // 极端: bat_volt 推到 5000, motor_overtemp 规则必须不受影响
        for (int i = 0; i < 20; i++) {
            rt.onValueChanged("bat_volt", 5000.0f);
        }
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "bat_volt 5000 → bat_overvolt 触发");
        TEST_ASSERT(rt.isActive("motor_overtemp") == false,
                    "bat_volt 5000 不会误触发 motor_overtemp (key 过滤生效)");
    }

    // ─── Test 6: 多规则同 key (bat_volt 同时配 bat_overvolt >420 和 bat_undervolt <280) ───
    printf("\n[6] bat_volt 多规则: >420 触发 bat_overvolt, <280 触发 bat_undervolt, 350 都不触发:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        // 区间 1: 425 → 触发 overvolt
        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "425V → bat_overvolt 触发");
        TEST_ASSERT(rt.isActive("bat_undervolt") == false, "425V → bat_undervolt 不触发");

        // 切到 350 (中间值) → 都清
        rt.onValueChanged("bat_volt", 350.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "350V → bat_overvolt 清除");
        TEST_ASSERT(rt.isActive("bat_undervolt") == false, "350V → bat_undervolt 不触发");

        // 区间 3: 200 → 触发 undervolt (duration=200, 2 次)
        rt.onValueChanged("bat_volt", 200.0f);
        rt.onValueChanged("bat_volt", 180.0f);
        TEST_ASSERT(rt.isActive("bat_undervolt") == true, "180V → bat_undervolt 触发");
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "180V → bat_overvolt 不触发");
    }

    // ─── Test 7: EQ 条件 (energy_mode==0 触发 ev_mode_active, duration=100, 1 次即触发) ───
    printf("\n[7] EQ 条件: energy_mode==0 触发 ev_mode_active (duration=100, 1 次即触发):\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("energy_mode", 5.0f);
        TEST_ASSERT(rt.isActive("ev_mode_active") == false, "energy_mode=5 → ev_mode_active 不触发");

        rt.onValueChanged("energy_mode", 0.0f);
        TEST_ASSERT(rt.isActive("ev_mode_active") == true,
                    "energy_mode=0 (1 次) → ev_mode_active 触发 (duration=100 → threshold=1)");
        TEST_ASSERT(rt.activeCount() == 1, "activeCount == 1");
    }

    // ─── Test 8: 防抖重置 (低于阈值后重新累计 tick_count) ───
    printf("\n[8] 防抖重置: 1 次 425V 后降到 350, 再回 425 仍需 2 次才触发:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);  // tick_count=1
        rt.onValueChanged("bat_volt", 350.0f);  // tick_count=0 (reset)
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "降到 350 → tick_count 重置为 0, 不触发");

        rt.onValueChanged("bat_volt", 425.0f);  // tick_count=1
        TEST_ASSERT(rt.isActive("bat_overvolt") == false,
                    "回升到 425 重新累计 tick_count=1, 不到 2 不触发");

        rt.onValueChanged("bat_volt", 430.0f);  // tick_count=2 → 触发
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "再 1 次 → 触发");
    }

    // ─── Test 9: acknowledge (确认后, 同 key 再次触发被吞) ───
    printf("\n[9] acknowledge: 确认后 bat_overvolt 不再触发:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);   // 触发
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "触发 bat_overvolt");

        rt.acknowledge("bat_overvolt");            // 确认
        // 跌破再回升 → 不会再触发 (因为 acknowledged && !active 跳过)
        rt.onValueChanged("bat_volt", 350.0f);
        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == false,
                    "acknowledge 后跌破再回升, bat_overvolt 不再触发");
    }

    // ─── Test 10: getActiveAlarms 返回正确 status ───
    printf("\n[10] getActiveAlarms: 触发 motor_overtemp 后 status 字段正确:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        // motor_overtemp: motor_temp > 120, HIGH, duration=500 → 5 次
        for (int i = 0; i < 5; i++) {
            rt.onValueChanged("motor_temp", 130.0f);
        }
        TEST_ASSERT(rt.isActive("motor_overtemp") == true, "motor_overtemp 触发 (5 次 130℃)");

        AlarmStatus buf[8] = {};
        int n = 8;
        rt.getActiveAlarms(buf, &n);
        TEST_ASSERT(n == 1, "getActiveAlarms 返回 1 条");
        TEST_ASSERT(std::strcmp(buf[0].name, "motor_overtemp") == 0, "status.name = 'motor_overtemp'");
        TEST_ASSERT(buf[0].active == true, "status.active = true");
        TEST_ASSERT(buf[0].flash == true, "HIGH priority → status.flash = true");
        TEST_ASSERT(buf[0].color == 0xFF4400U, "status.color = 0xFF4400U (橙色)");
    }

    // ─── Test 11: 多个报警同时活跃 (motor_overtemp + tire_pressure_low) ───
    // 用 fuel_level=10 触发 fuel_low (10<15) 但不触发 fuel_critical (10<5 false)
    // 避免 bat_soc=5 同时触发 bat_soc_low + soc_critical_low (双规则同 key)
    printf("\n[11] 多报警并行: motor_temp 高 + tire_pressure 低 → 同时 2 条活跃:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        // motor_overtemp: motor_temp > 120, HIGH, duration=500 → 5 次
        for (int i = 0; i < 5; i++) {
            rt.onValueChanged("motor_temp", 130.0f);
        }
        // tire_pressure_low: tire_pressure < 1.8, HIGH, duration=200 → 2 次
        for (int i = 0; i < 2; i++) {
            rt.onValueChanged("tire_pressure", 1.5f);
        }

        TEST_ASSERT(rt.isActive("motor_overtemp") == true, "motor_overtemp 触发");
        TEST_ASSERT(rt.isActive("tire_pressure_low") == true, "tire_pressure_low 触发");
        TEST_ASSERT(rt.activeCount() == 2, "activeCount == 2 (并行 2 条独立 key)");

        AlarmStatus buf[8] = {};
        int n = 8;
        rt.getActiveAlarms(buf, &n);
        TEST_ASSERT(n == 2, "getActiveAlarms 返回 2 条");
    }

    // ─── Test 12: 清除只关对应规则 (motor_overtemp 降, tire_pressure_low 仍 active) ───
    printf("\n[12] 独立清除: 降 motor_temp 不影响 tire_pressure_low:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        for (int i = 0; i < 5; i++) rt.onValueChanged("motor_temp", 130.0f);
        for (int i = 0; i < 2; i++) rt.onValueChanged("tire_pressure", 1.5f);
        TEST_ASSERT(rt.activeCount() == 2, "初始 2 条 active");

        rt.onValueChanged("motor_temp", 80.0f);  // 跌破 120 → 清除
        TEST_ASSERT(rt.isActive("motor_overtemp") == false, "motor_temp 80 → motor_overtemp 清除");
        TEST_ASSERT(rt.isActive("tire_pressure_low") == true, "tire_pressure_low 仍 active (独立)");
        TEST_ASSERT(rt.activeCount() == 1, "activeCount = 1");
    }

    // ─── Test 13: 回调总数 (每条 bat_overvolt 触发 = 1 indicator + 1 text + 1 state) ───
    printf("\n[13] 回调次数: bat_overvolt 触发恰好 1 indicator + 1 text + 1 state:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(g_indicator_count == 1, "onIndicatorUpdate 调 1 次 (bat_warn_light on)");
        TEST_ASSERT(g_alarm_text_count == 1, "onAlarmTextUpdate 调 1 次 (电池过压)");
        TEST_ASSERT(g_state_change_count == 1, "onAlarmStateChanged 调 1 次");
    }

    // ─── Test 14: 清除时回调 (clearAlarm 关 indicator) ───
    printf("\n[14] 清除回调: 跌破阈值后 onIndicatorUpdate 再调 1 次 (off):\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        int after_trigger = g_indicator_count;
        TEST_ASSERT(after_trigger == 1, "触发后 indicator_count=1");

        rt.onValueChanged("bat_volt", 350.0f);  // 清除
        TEST_ASSERT(g_indicator_count == after_trigger + 1,
                    "清除后 indicator_count +1 (关 bat_warn_light)");
        TEST_ASSERT(g_state_change_count == 2, "state_change_count=2 (active=true, false)");
    }

    // ─── Test 15: 重触发 (清除后再次越过阈值能重新触发) ───
    printf("\n[15] 重触发: 清除后再次越过 bat_volt=425, bat_overvolt 能再次触发:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "首次触发");

        rt.onValueChanged("bat_volt", 350.0f);  // 清除
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "清除");

        // 不调用 acknowledge → 重触发应正常工作
        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == true, "重触发成功 (未 acknowledge)");
        TEST_ASSERT(g_state_change_count == 3, "state_change_count=3 (true, false, true)");
    }

    // ─── Test 16: re-init 内存安全 (反复 init/析构 不泄漏 / 不崩溃) ───
    // 背景: alarm_runtime.cpp::init() 早期版本 m_states = new[] 但 re-init 时未 delete[] 旧指针,
    //       导致每次重新 init 泄漏 ~sizeof(AlarmState) * rule_count 字节.
    // 修复: init() 入口先 delete[] m_states (if not nullptr), 类似 can_signal_monitor 的 re-init 保护.
    printf("\n[16] re-init 内存安全: 反复 init() 100 次 不崩溃 / 状态正确:\n");
    {
        AlarmRuntime rt(cb);
        // 反复 100 次 init() — 若有泄漏, 长时间跑才会崩; 单元测试中验证:
        //   (a) 反复 init 后 isActive 仍正常工作
        //   (b) activeCount 始终 0 (新 init 重置所有状态)
        for (int i = 0; i < 100; i++) {
            rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                    ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
            TEST_ASSERT(rt.activeCount() == 0,
                        "第 N 次 init 后 activeCount==0 (无残留报警状态)");
            TEST_ASSERT(rt.isActive("bat_overvolt") == false,
                        "第 N 次 init 后 bat_overvolt==false (重置)");
        }
        // 再 init 一次 + 触发一个报警, 确认功能仍正常
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        rt.onValueChanged("bat_volt", 425.0f);
        rt.onValueChanged("bat_volt", 430.0f);
        TEST_ASSERT(rt.isActive("bat_overvolt") == true,
                    "100 次 re-init 后 仍能正常触发 bat_overvolt");
    }
    printf("  ✓ re-init 100 次无崩溃, 状态机仍正常工作 (AddressSanitizer/valgrind 建议跑)\n");

    // ─── Test 17: NaN/Inf 拒绝 (浮点黑洞: 拒绝评估, 不污染状态) ───
    // (防止 COND_NE (value != threshold) 对 NaN 永远 true 而误触发)
    printf("\n[17] NaN/Inf 拒绝: onValueChanged(NaN) / onValueChanged(Inf) 不评估任何规则:\n");
    {
        AlarmRuntime rt(cb);
        rt.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();

        // 17a) NaN 输入: 不应触发任何回调, 不应进入 tick 累计
        rt.onValueChanged("bat_volt", std::nanf(""));
        TEST_ASSERT(g_state_change_count == 0, "NaN 不触发 onAlarmStateChanged");
        TEST_ASSERT(g_indicator_count == 0, "NaN 不触发 onIndicatorUpdate");
        TEST_ASSERT(g_alarm_text_count == 0, "NaN 不触发 onAlarmTextUpdate");
        TEST_ASSERT(rt.activeCount() == 0, "NaN 后 activeCount 仍 0");
        TEST_ASSERT(rt.isActive("bat_overvolt") == false, "NaN 后 bat_overvolt 未触发");

        // 17b) +Inf / -Inf: 同样拒绝
        rt.onValueChanged("bat_volt", std::numeric_limits<float>::infinity());
        rt.onValueChanged("bat_volt", -std::numeric_limits<float>::infinity());
        TEST_ASSERT(g_state_change_count == 0, "+Inf/-Inf 也不触发 onAlarmStateChanged");
        TEST_ASSERT(rt.activeCount() == 0, "Inf 序列后 activeCount 仍 0");

        // 17c) NaN 不应消耗 tick_count: 一次正常 425V 后, 再次 425V 应触发
        // (验证 NaN 没把 tick_count 重置为 0, 也没累加)
        rt.onValueChanged("bat_volt", 425.0f);  // tick_count=1
        rt.onValueChanged("bat_volt", std::nanf(""));  // 应跳过, tick_count 仍 1
        rt.onValueChanged("bat_volt", 425.0f);  // tick_count=2 → 触发
        TEST_ASSERT(rt.isActive("bat_overvolt") == true,
                    "NaN 不消耗 tick_count: 1次425V + NaN + 1次425V = 2次 → bat_overvolt 触发");
        TEST_ASSERT(g_state_change_count == 1, "NaN 中断后仅 1 次 state change (真触发那 1 次)");

        // 17d) 已 active 的报警遇到 NaN: 不应被错误清除
        // (清空 rt, 重新触发 bat_overvolt 到 active=true, 再灌 NaN, 确认仍 active)
        AlarmRuntime rt2(cb);
        rt2.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                 ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
        resetCounters();
        rt2.onValueChanged("bat_volt", 425.0f);
        rt2.onValueChanged("bat_volt", 430.0f);  // 触发, active=true
        TEST_ASSERT(rt2.isActive("bat_overvolt") == true, "基线: bat_overvolt 已 active");

        rt2.onValueChanged("bat_volt", std::nanf(""));  // NaN 不应 clear
        TEST_ASSERT(rt2.isActive("bat_overvolt") == true,
                    "NaN 不应清除已 active 的报警 (保守策略: 宁可保留误报, 不丢真报)");
    }

    // ─── 汇总 ───
    printf("\n────────────────────────────────────\n");
    printf("通过: %d / %d (%.1f%%)\n", g_test_passed, g_test_count,
           g_test_count > 0 ? (100.0 * g_test_passed / g_test_count) : 0.0);

    if (g_test_passed != g_test_count) {
        printf("✗ %d 个断言失败\n", g_test_count - g_test_passed);
        return 1;
    }
    printf("✓ 所有 AlarmRuntime 单元测试通过。\n");
    return 0;
}

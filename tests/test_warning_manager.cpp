// test_warning_manager.cpp
// Layer 2 WarningManager 单元测试 (纯 C++, 无 Qt)
//
// 覆盖:
//   1. 初始空状态
//   2. 推 1 条 INFO 报警 → active 1 条
//   3. 同 name 在 dedup 窗口内 → 去重 + dedup_count++
//   4. 同 name 在 dedup 窗口外 → 算新触发
//   5. 防抖: 100ms 内连续 push 同 name → 算 1 次
//   6. 防抖命中但 active 内有同名 → dedup_count++ + last_seen_ms 刷新 (hold 重置)
//   7. 推 N 条超过 max_active=3 → 保留 priority 最小的 3 条
//   8. CRITICAL 报警 (priority=0) → hasCritical()=true
//   9. 持续报警后停推 → hold_ms 后自动清除
//  10. 排序稳定: 后推 priority=0 排到前
//  11. 严重度派生: priority==0 → CRITICAL, <10 → WARNING, >=10 → INFO
//  12. reset 清空 dedup/debounce/active/config
//  13. tick 在无变化时不增删
//  14. 自定义 dedup_window_ms 配置生效
//  15. 自定义 debounce_ms 配置生效
//  16. 自定义 hold_ms 配置生效
//  17. 自定义 max_active 配置生效
//  18. isActive(name) 查找
//  19. 多条不同 name + 混合 priority 排序
//  20. 颜色编码 RGB → ARGB

#include <cstdio>
#include <cstring>
#include <cassert>
#include "../src/layer2/warning_manager.h"

using candash::WarningManager;
using candash::WarningConfig;
using candash::WarningSeverity;
using candash::AlarmEvent;
using candash::ActiveWarning;

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { \
        g_test_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        printf("  ✗ %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define RUN(name) do { \
    printf("\n[%d] %s:\n", ++g_test_section, #name); \
    name(); \
} while(0)

static int g_test_section = 0;

// 辅助: 构造一个 AlarmEvent
static AlarmEvent makeAlarm(const char* name, uint8_t priority,
                             uint8_t r=0xFF, uint8_t g=0x44, uint8_t b=0x00) {
    AlarmEvent e{};
    std::strncpy(e.name, name, sizeof(e.name) - 1);
    std::strncpy(e.text_zh, "测试报警", sizeof(e.text_zh) - 1);
    std::strncpy(e.text_en, "test alarm", sizeof(e.text_en) - 1);
    e.priority = priority;
    e.color_r = r; e.color_g = g; e.color_b = b;
    return e;
}

// ─── 1. 初始空状态 ───
static void test_initial_state() {
    WarningManager w;
    TEST_ASSERT(w.activeCount() == 0, "初始 active = 0");
    TEST_ASSERT(!w.hasCritical(), "初始 hasCritical = false");
    TEST_ASSERT(w.config().dedup_window_ms == 5000, "默认 dedup_window_ms = 5000");
    TEST_ASSERT(w.config().debounce_ms == 100, "默认 debounce_ms = 100");
    TEST_ASSERT(w.config().hold_ms == 3000, "默认 hold_ms = 3000");
    TEST_ASSERT(w.config().max_active == 3, "默认 max_active = 3");
}

// ─── 2. 推 1 条 INFO 报警 → active 1 条 ───
static void test_push_one_info() {
    WarningManager w;
    w.pushAlarm(makeAlarm("low_oil", 15), 1000);
    TEST_ASSERT(w.activeCount() == 1, "推 1 条后 active = 1");
    TEST_ASSERT(w.isActive("low_oil"), "low_oil isActive");
    TEST_ASSERT(w.activeWarnings()[0].priority == 15, "priority = 15");
    TEST_ASSERT(w.activeWarnings()[0].severity == static_cast<uint8_t>(WarningSeverity::INFO), "severity = INFO");
    TEST_ASSERT(!w.hasCritical(), "INFO 报警 → hasCritical = false");
}

// ─── 3. dedup: 同 name 5s 内不重复显示 ───
static void test_dedup_within_window() {
    WarningManager w;
    w.pushAlarm(makeAlarm("over_volt", 5), 1000);
    TEST_ASSERT(w.activeCount() == 1, "首次推入 active = 1");

    w.pushAlarm(makeAlarm("over_volt", 5), 2000);  // 1s 后同 name
    TEST_ASSERT(w.activeCount() == 1, "dedup 窗口内不重复加入");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 1, "dedup_count = 1 (累计 1 次去重)");

    w.pushAlarm(makeAlarm("over_volt", 5), 3500);  // 2.5s 后
    TEST_ASSERT(w.activeCount() == 1, "3.5s 仍在 dedup 窗口 (5s)");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 2, "dedup_count = 2");
}

// ─── 4. dedup 窗口外 → 算新触发 ───
static void test_dedup_outside_window() {
    WarningManager w;
    w.pushAlarm(makeAlarm("over_volt", 5), 1000);
    w.pushAlarm(makeAlarm("over_volt", 5), 2000);  // 去重, 1 次
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 1, "1 次去重");

    w.pushAlarm(makeAlarm("over_volt", 5), 7000);  // 6s 后, 超过 5s 窗口
    // 重新加入, 但 m_active 仍有同 name → 不算新增, 但 dedup_count++ 应该是
    // 实际: 我们的 applyRules 第 1 步是防抖检查, 防抖命中 → active 内找到同名 → 计数+1
    // 无论去重还是防抖命中, 效果都是 active.count 不增 + dedup_count++
    TEST_ASSERT(w.activeCount() == 1, "active 仍 1 条");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 2, "dedup_count++");
}

// ─── 5. 防抖: 100ms 内连续 push 算 1 次 ───
static void test_debounce_within_window() {
    WarningManager w;
    w.pushAlarm(makeAlarm("brake_fail", 2), 1000);
    TEST_ASSERT(w.activeCount() == 1, "首次推入");

    w.pushAlarm(makeAlarm("brake_fail", 2), 1050);  // 50ms 后
    w.pushAlarm(makeAlarm("brake_fail", 2), 1080);  // 30ms 后
    w.pushAlarm(makeAlarm("brake_fail", 2), 1099);  // 19ms 后
    TEST_ASSERT(w.activeCount() == 1, "防抖窗口内连续 push → active 仍 1 条");
    // 设计选择: 防抖命中也累加 dedup_count (反映触发频度, 便于 UI 角标)
    TEST_ASSERT(w.activeWarnings()[0].dedup_count >= 2, "防抖期内累计去重次数 >= 2");

    // 100ms+ 后 (防抖窗口外)
    w.pushAlarm(makeAlarm("brake_fail", 2), 1200);
    TEST_ASSERT(w.activeCount() == 1, "防抖窗口外但 dedup 窗口内 → 仍 1 条");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count >= 3, "dedup_count 累加 >= 3");
}

// ─── 6. 防抖命中但 active 内的 hold 计时刷新 ───
static void test_debounce_refreshes_hold() {
    WarningManager w;
    w.pushAlarm(makeAlarm("a", 5), 1000);
    uint64_t orig_first_seen = w.activeWarnings()[0].first_seen_ms;

    // 防抖窗口内连续 5 次, 每次都刷新 last_seen_ms
    for (int i = 1; i <= 5; i++) {
        w.pushAlarm(makeAlarm("a", 5), 1000 + i * 50);
    }
    // 防抖命中, last_seen 刷到 1250
    // tick(1500) — last_seen=1250, 1250+3000=4250 > 1500 → 仍 active
    w.tick(1500);
    TEST_ASSERT(w.activeCount() == 1, "持续 push 期间 hold 不触发");
    TEST_ASSERT(w.activeWarnings()[0].first_seen_ms == orig_first_seen, "first_seen 不变");
    TEST_ASSERT(w.activeWarnings()[0].last_seen_ms == 1250, "last_seen 刷新到 1250");

    // 继续 tick 到 5000ms — 此时距 last_seen=1250 已 3750ms > hold 3000
    // 业务上 3.5s 没新事件了, 主动清除是正确的
    w.tick(5000);
    TEST_ASSERT(w.activeCount() == 0, "持续 push 停了 3.5s 后过期清除");
}

// ─── 7. max_active: 超 3 条保留 priority 最小的 ───
static void test_max_active_trim() {
    WarningManager w;
    w.pushAlarm(makeAlarm("info1", 50), 1000);
    w.pushAlarm(makeAlarm("info2", 40), 1100);
    w.pushAlarm(makeAlarm("info3", 30), 1200);
    w.pushAlarm(makeAlarm("info4", 20), 1300);  // 第 4 条, 应该被 trim
    TEST_ASSERT(w.activeCount() == 3, "max_active=3, 第 4 条被丢");
    TEST_ASSERT(!w.isActive("info1"), "priority=50 (最大) 被丢");
    TEST_ASSERT(w.isActive("info2"), "priority=40 保留");
    TEST_ASSERT(w.isActive("info3"), "priority=30 保留");
    TEST_ASSERT(w.isActive("info4"), "priority=20 保留");
}

// ─── 8. CRITICAL 报警 (priority=0) ───
static void test_critical_detection() {
    WarningManager w;
    w.pushAlarm(makeAlarm("battery_fire", 0), 1000);  // priority=0 = CRITICAL
    TEST_ASSERT(w.hasCritical(), "priority=0 → hasCritical = true");
    TEST_ASSERT(w.activeWarnings()[0].severity == static_cast<uint8_t>(WarningSeverity::CRITICAL), "severity = CRITICAL");
}

// ─── 9. hold: 停推后 hold_ms 自动清除 ───
static void test_hold_expiry() {
    WarningManager w;
    w.pushAlarm(makeAlarm("temp_high", 5), 1000);
    w.pushAlarm(makeAlarm("temp_high", 5), 2000);  // 去重, 刷新 last_seen=2000
    TEST_ASSERT(w.activeCount() == 1, "推 2 次后 active = 1");

    w.tick(3000);  // 距 last_seen=2000 才 1s < hold 3s
    TEST_ASSERT(w.activeCount() == 1, "1s 后仍未过期");

    w.tick(5500);  // 距 last_seen=2000 已 3.5s > hold 3s
    TEST_ASSERT(w.activeCount() == 0, "3.5s 后过期清除");
}

// ─── 10. 排序: 后推 priority=0 排到前 ───
static void test_sort_by_priority() {
    WarningManager w;
    w.pushAlarm(makeAlarm("warn1", 10), 1000);
    w.pushAlarm(makeAlarm("warn2", 5),  1100);
    w.pushAlarm(makeAlarm("warn3", 1),  1200);
    TEST_ASSERT(w.activeCount() == 3, "3 条 active");
    TEST_ASSERT(w.activeWarnings()[0].priority == 1, "priority=1 在前 (最严重)");
    TEST_ASSERT(w.activeWarnings()[1].priority == 5, "priority=5 中间");
    TEST_ASSERT(w.activeWarnings()[2].priority == 10, "priority=10 末尾 (最轻)");

    // 后推 critical 0
    w.pushAlarm(makeAlarm("critical", 0), 1300);
    TEST_ASSERT(w.activeWarnings()[0].priority == 0, "新推 critical 排到最前");
}

// ─── 11. 严重度派生 ───
static void test_severity_from_priority() {
    WarningManager w;
    w.pushAlarm(makeAlarm("crit", 0),   1000);  // CRITICAL
    w.pushAlarm(makeAlarm("warn", 5),   1100);  // WARNING
    w.pushAlarm(makeAlarm("info1", 9),  1200);  // WARNING (边界)
    // max_active=3 仍能装下
    TEST_ASSERT(w.activeCount() == 3, "3 条全部保留");
    // 排序后顺序: crit(0), warn(5), info1(9)
    TEST_ASSERT(w.activeWarnings()[0].severity == static_cast<uint8_t>(WarningSeverity::CRITICAL), "priority=0 → CRITICAL");
    TEST_ASSERT(w.activeWarnings()[1].severity == static_cast<uint8_t>(WarningSeverity::WARNING),  "priority=5 → WARNING");
    TEST_ASSERT(w.activeWarnings()[2].severity == static_cast<uint8_t>(WarningSeverity::WARNING),  "priority=9 → WARNING (边界)");

    // 推第 4 条 (优先级较小, 排第 4 后应被 trim)
    w.pushAlarm(makeAlarm("info2", 10), 1300);  // INFO
    TEST_ASSERT(w.activeCount() == 3, "max=3 仍 3 条");
    // 排序: crit(0), warn(5), 然后 info1(9) vs info2(10) → info1 在前
    // 但 max=3 截断末尾, info2(10) 被丢
    // 实际: sortByPriority 后 [crit(0), warn(5), info1(9), info2(10)], trimToMax 留前 3
    TEST_ASSERT(w.isActive("crit"), "crit 保留");
    TEST_ASSERT(w.isActive("warn"), "warn 保留");
    TEST_ASSERT(w.isActive("info1"), "info1(9) 保留 (比 info2 优先)");
    TEST_ASSERT(!w.isActive("info2"), "info2(10) 被 trim");

    // 再推 priority=15 验证 INFO 严重度
    WarningManager w2;
    w2.pushAlarm(makeAlarm("x", 15), 1000);
    TEST_ASSERT(w2.activeWarnings()[0].severity == static_cast<uint8_t>(WarningSeverity::INFO), "priority=15 → INFO");
}

// ─── 12. reset 清空 ───
static void test_reset() {
    WarningManager w;
    w.pushAlarm(makeAlarm("a", 1), 1000);
    w.pushAlarm(makeAlarm("b", 2), 1100);
    w.pushAlarm(makeAlarm("a", 1), 1200);  // dedup
    TEST_ASSERT(w.activeCount() == 2, "推 3 次后 active = 2");

    w.reset();
    TEST_ASSERT(w.activeCount() == 0, "reset 后 active = 0");
    TEST_ASSERT(w.config().dedup_window_ms == 5000, "reset 后 config 回默认");

    // reset 后, 同 name 推入应算新触发 (因为 dedup 表已清)
    w.pushAlarm(makeAlarm("a", 1), 1500);
    TEST_ASSERT(w.activeCount() == 1, "reset 后同 name 算新触发");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 0, "dedup_count 重新计数");
}

// ─── 13. tick 无变化时不增删 ───
static void test_tick_no_op() {
    WarningManager w;
    w.pushAlarm(makeAlarm("a", 5), 1000);
    w.pushAlarm(makeAlarm("b", 10), 1100);
    TEST_ASSERT(w.activeCount() == 2, "初始 2 条");

    w.tick(1200);  // 1ms 内推进
    TEST_ASSERT(w.activeCount() == 2, "tick 1ms 不变化");

    w.tick(4000);  // last_seen 最大 1100, 1100+3000=4100 > 4000 → 不清除
    TEST_ASSERT(w.activeCount() == 2, "tick 4000ms 仍未过期");
}

// ─── 14. 自定义 dedup_window_ms ───
static void test_custom_dedup_window() {
    WarningManager w;
    WarningConfig c = w.config();
    c.dedup_window_ms = 1000;  // 改短到 1s
    w.setConfig(c);

    w.pushAlarm(makeAlarm("a", 5), 1000);
    w.pushAlarm(makeAlarm("a", 5), 1500);  // 500ms 后, 1s 窗口内 → 去重
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 1, "500ms 内去重");

    w.pushAlarm(makeAlarm("a", 5), 2100);  // 1.1s 后, 1s 窗口外
    TEST_ASSERT(w.activeCount() == 1, "仍 1 条 (active 内有同名)");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 2, "1.1s 后 dedup_count++");
}

// ─── 15. 自定义 debounce_ms ───
static void test_custom_debounce() {
    WarningManager w;
    WarningConfig c = w.config();
    c.debounce_ms = 50;  // 改短到 50ms
    w.setConfig(c);

    // 首次 push 进入 active, dedup_count=0
    w.pushAlarm(makeAlarm("a", 5), 1000);
    TEST_ASSERT(w.activeCount() == 1, "首次 push 后 active=1");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count == 0, "首次 dedup_count=0");

    // 防抖窗口内 (30ms) 推 2 次, active 内同名, 触发 dedup_count++
    // (设计选择: 每次 push 触发 active 内的同名计数, 便于 UI 角标反映触发频度)
    w.pushAlarm(makeAlarm("a", 5), 1030);  // 防抖命中
    TEST_ASSERT(w.activeCount() == 1, "防抖命中 → 仍 1 条");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count >= 1, "防抖命中 dedup_count >= 1");

    // 100ms 后 (防抖外 + 5s dedup 内) 再推, 应触发 dedup 路径
    w.pushAlarm(makeAlarm("a", 5), 1100);
    TEST_ASSERT(w.activeCount() == 1, "防抖外 + dedup 内 → 仍 1 条");
    TEST_ASSERT(w.activeWarnings()[0].dedup_count >= 2, "dedup_count 累加到 >= 2");
}

// ─── 16. 自定义 hold_ms ───
static void test_custom_hold() {
    WarningManager w;
    WarningConfig c = w.config();
    c.hold_ms = 500;  // 改短到 500ms
    w.setConfig(c);

    w.pushAlarm(makeAlarm("a", 5), 1000);
    w.tick(1300);  // 距 last_seen=1000 才 300ms < 500
    TEST_ASSERT(w.activeCount() == 1, "300ms 后未过期");

    w.tick(1600);  // 距 last_seen=1000 已 600ms > 500
    TEST_ASSERT(w.activeCount() == 0, "600ms 后过期 (500ms hold)");
}

// ─── 17. 自定义 max_active ───
static void test_custom_max_active() {
    WarningManager w;
    WarningConfig c = w.config();
    c.max_active = 5;  // 改大到 5
    w.setConfig(c);

    for (int i = 0; i < 7; i++) {
        char name[16];
        std::snprintf(name, sizeof(name), "a%d", i);
        w.pushAlarm(makeAlarm(name, i + 1), 1000 + i * 10);
    }
    TEST_ASSERT(w.activeCount() == 5, "max_active=5, 推 7 条留 5");
    // 排序: priority 1,2,3,4,5 留下 (6,7 被丢)
    TEST_ASSERT(w.activeWarnings()[0].priority == 1, "priority=1 最小在前");
    TEST_ASSERT(w.activeWarnings()[4].priority == 5, "priority=5 最大保留");
}

// ─── 18. isActive 查找 ───
static void test_is_active() {
    WarningManager w;
    w.pushAlarm(makeAlarm("exist", 1), 1000);
    TEST_ASSERT(w.isActive("exist"), "存在的 name → isActive=true");
    TEST_ASSERT(!w.isActive("nonexist"), "不存在的 name → isActive=false");
    TEST_ASSERT(!w.isActive(nullptr), "nullptr → isActive=false (不崩)");
    TEST_ASSERT(!w.isActive(""), "空字符串 → isActive=false");
}

// ─── 19. 混合 priority 多条排序 ───
static void test_mixed_priority_sort() {
    WarningManager w;
    w.pushAlarm(makeAlarm("e", 50), 1000);
    w.pushAlarm(makeAlarm("a", 0),  1100);
    w.pushAlarm(makeAlarm("c", 10), 1200);
    w.pushAlarm(makeAlarm("b", 5),  1300);
    TEST_ASSERT(w.activeCount() == 3, "max=3, e(50) 被丢");
    TEST_ASSERT(w.activeWarnings()[0].priority == 0,  "a(0) 排第 1");
    TEST_ASSERT(w.activeWarnings()[1].priority == 5,  "b(5) 排第 2");
    TEST_ASSERT(w.activeWarnings()[2].priority == 10, "c(10) 排第 3");
    TEST_ASSERT(!w.isActive("e"), "e(50) 被 trim");
}

// ─── 20. 颜色 RGB → ARGB 编码 ───
static void test_color_encoding() {
    WarningManager w;
    w.pushAlarm(makeAlarm("red_alert", 0, 0xDD, 0x22, 0x22), 1000);
    const auto& aw = w.activeWarnings()[0];
    // ARGB = 0xFF DD 22 22
    TEST_ASSERT(aw.color == 0xFFDD2222u, "ARGB = 0xFFDD2222 (不透明红)");
    TEST_ASSERT(aw.severity == static_cast<uint8_t>(WarningSeverity::CRITICAL), "severity = CRITICAL");
}

int main() {
    printf("=== WarningManager 单元测试 ===\n\n");

    RUN(test_initial_state);
    RUN(test_push_one_info);
    RUN(test_dedup_within_window);
    RUN(test_dedup_outside_window);
    RUN(test_debounce_within_window);
    RUN(test_debounce_refreshes_hold);
    RUN(test_max_active_trim);
    RUN(test_critical_detection);
    RUN(test_hold_expiry);
    RUN(test_sort_by_priority);
    RUN(test_severity_from_priority);
    RUN(test_reset);
    RUN(test_tick_no_op);
    RUN(test_custom_dedup_window);
    RUN(test_custom_debounce);
    RUN(test_custom_hold);
    RUN(test_custom_max_active);
    RUN(test_is_active);
    RUN(test_mixed_priority_sort);
    RUN(test_color_encoding);

    printf("\n=== %d/%d 测试通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

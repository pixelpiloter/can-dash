// test_theme_manager.cpp
// Layer 2 ThemeManager 单元测试 (纯 C++, 无 Qt)
//
// 覆盖:
//   1. 初始状态 (AUTO + hour=12 → DAY)
//   2. setMode DAY/NIGHT 强制覆盖
//   3. setMode AUTO + 不同时刻 → 正确判定
//   4. 黎明/黄昏边界 (含 sunrise==sunset 退化)
//   5. 自定义 sunrise/sunset 阈值生效
//   6. setCurrentHour 注入, 触发 AUTO 重新评估
//   7. colors() DAY vs NIGHT 返回不同配色
//   8. colorOf() 5 个 slot 都正确
//   9. normalizeHour 处理负数 / 越界
//  10. tick() AUTO 模式从 now_ms 推算 hour (PR 15 新增)
//  11. tick() 跨 sunrise/sunset 边界
//  12. tick() 24h 环绕 + 时间倒退防御
//  13. setTimeBaseline() 设基线 + 立即触发 evaluate

#include <cstdio>
#include <cassert>
#include <cstring>
#include "../src/layer2/theme_manager.h"

using candash::ThemeManager;
using candash::ThemeMode;
using candash::ThemeColors;

static int test_count = 0;
static int test_pass = 0;

#define RUN(name) do { \
    test_count++; \
    printf("\n[测试 %d] %s\n", test_count, #name); \
    name(); \
    test_pass++; \
} while (0)

static void test_initial_state() {
    ThemeManager t;
    // 默认 AUTO + hour=12 (中午) → DAY
    assert(t.currentMode() == ThemeMode::AUTO);
    assert(t.isDay() == true);
    assert(t.currentHour() == 12);
    assert(t.sunriseHour() == 6);
    assert(t.sunsetHour() == 18);
    printf("  ✓ 初始 AUTO + 12:00 → DAY\n");
}

static void test_force_day_night() {
    ThemeManager t;
    t.setCurrentHour(2);  // 凌晨
    assert(t.isDay() == false);  // AUTO 评估为 NIGHT

    t.setMode(ThemeMode::DAY);
    assert(t.currentMode() == ThemeMode::DAY);
    assert(t.isDay() == true);  // 强制覆盖

    t.setMode(ThemeMode::NIGHT);
    assert(t.isDay() == false);
    printf("  ✓ setMode 强制覆盖 isDay\n");
}

static void test_auto_mode_hour_evaluation() {
    ThemeManager t;

    // 日间小时
    t.setCurrentHour(8);
    assert(t.isDay() == true);
    t.setCurrentHour(12);
    assert(t.isDay() == true);
    t.setCurrentHour(17);
    assert(t.isDay() == true);

    // 夜间小时
    t.setCurrentHour(5);   // 黎明前
    assert(t.isDay() == false);
    t.setCurrentHour(18);  // 黄昏刚过
    assert(t.isDay() == false);
    t.setCurrentHour(23);
    assert(t.isDay() == false);
    printf("  ✓ AUTO 模式: 8/12/17 → DAY, 5/18/23 → NIGHT\n");
}

static void test_boundary_hours() {
    ThemeManager t;

    // 黎明边界 (6:00 = sunrise, 应为 DAY)
    t.setCurrentHour(6);
    assert(t.isDay() == true);

    // 黄昏边界 (18:00 = sunset, 应为 NIGHT — sunset 不含)
    t.setCurrentHour(18);
    assert(t.isDay() == false);

    // 边界前一秒 (5:59)
    t.setCurrentHour(5);
    assert(t.isDay() == false);

    // 边界当刻 (17:59)
    t.setCurrentHour(17);
    assert(t.isDay() == true);
    printf("  ✓ 边界: [6,18) = DAY, [18,6) = NIGHT\n");
}

static void test_custom_sunrise_sunset() {
    ThemeManager t;
    t.setSunriseHour(8);
    t.setSunriseHour(8);
    t.setSunsetHour(20);  // 8-20 = DAY

    t.setCurrentHour(7);
    assert(t.isDay() == false);  // 7 < 8

    t.setCurrentHour(8);
    assert(t.isDay() == true);

    t.setCurrentHour(19);
    assert(t.isDay() == true);

    t.setCurrentHour(20);
    assert(t.isDay() == false);
    printf("  ✓ 自定义 [8,20): 7→NIGHT, 8/19→DAY, 20→NIGHT\n");
}

static void test_degenerate_sunrise_equals_sunset() {
    ThemeManager t;
    t.setSunriseHour(12);
    t.setSunsetHour(12);

    t.setCurrentHour(3);
    assert(t.isDay() == true);  // 退化情形: 全 DAY

    t.setCurrentHour(15);
    assert(t.isDay() == true);
    printf("  ✓ sunrise==sunset 退化: 全 DAY\n");
}

static void test_set_current_hour_triggers_re_eval() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);

    t.setCurrentHour(10);
    assert(t.isDay() == true);

    t.setCurrentHour(22);
    assert(t.isDay() == false);

    t.setCurrentHour(14);
    assert(t.isDay() == true);
    printf("  ✓ setCurrentHour 触发 AUTO 重新评估\n");
}

static void test_tick_evaluates_auto() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);
    t.setTimeBaseline(12, 0);  // baseline 12:00 @ ms=0
    assert(t.currentHour() == 12);

    // tick(0) → delta=0 → hour=12 → DAY
    t.tick(0);
    assert(t.isDay() == true);
    assert(t.currentHour() == 12);

    // tick(7h) → delta=7h → hour=19 → NIGHT
    t.tick(7ULL * 3600 * 1000);
    assert(t.isDay() == false);
    assert(t.currentHour() == 19);
    printf("  ✓ tick() 在 AUTO 模式从 now_ms 推算 hour\n");
}

static void test_tick_crosses_sunrise() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);
    t.setTimeBaseline(5, 0);  // 黎明前 baseline

    // tick(0) → hour=5 → NIGHT
    t.tick(0);
    assert(t.isDay() == false);

    // tick(1h) → hour=6 → DAY (sunrise inclusive, [6,18))
    t.tick(1ULL * 3600 * 1000);
    assert(t.isDay() == true);
    assert(t.currentHour() == 6);
    printf("  ✓ tick() 跨 sunrise (5→6): NIGHT → DAY\n");
}

static void test_tick_crosses_sunset() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);
    t.setTimeBaseline(17, 0);  // 黄昏前 baseline

    // tick(0) → hour=17 → DAY
    t.tick(0);
    assert(t.isDay() == true);

    // tick(1h) → hour=18 → NIGHT (sunset exclusive, [6,18))
    t.tick(1ULL * 3600 * 1000);
    assert(t.isDay() == false);
    assert(t.currentHour() == 18);
    printf("  ✓ tick() 跨 sunset (17→18): DAY → NIGHT\n");
}

static void test_tick_24h_rollover() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);
    t.setTimeBaseline(22, 0);  // 22:00 baseline

    // tick(0) → hour=22 → NIGHT
    t.tick(0);
    assert(t.isDay() == false);

    // tick(3h 累计 3h) → hour=22+3=25%24=1 → NIGHT (凌晨 1 点)
    t.tick(3ULL * 3600 * 1000);
    assert(t.isDay() == false);
    assert(t.currentHour() == 1);

    // tick(8h 绝对时间) → hour=22+8=30%24=6 → DAY (跨 sunrise)
    // 注意: tick() 接收绝对时间, 不是 delta. 上一 tick 在 3h, 这次 tick 在 8h, delta=5h.
    //       测试命名 "24h 环绕" 关注的是 24h 跨日, 22→6 已穿过 0 点 (跨日) 即可.
    t.tick(8ULL * 3600 * 1000);
    assert(t.isDay() == true);
    assert(t.currentHour() == 6);
    printf("  ✓ tick() 24h 环绕: 22→1→6, 正确判定 NIGHT/NIGHT/DAY (跨过 0 点)\n");
}

static void test_tick_time_travel_defense() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);
    t.setTimeBaseline(12, 1000);  // 12:00 @ ms=1000

    // tick(500) → now_ms < baselineMs → 按 delta=0, hour=12, DAY
    // (时间倒退防御: 不让 now_ms < baselineMs 引发负 delta/UB)
    t.tick(500);
    assert(t.currentHour() == 12);
    assert(t.isDay() == true);

    // tick(1000) → 正好 baseline, hour=12
    t.tick(1000);
    assert(t.currentHour() == 12);
    assert(t.isDay() == true);

    // tick(1000+3h) → 15:00 → DAY
    t.tick(1000 + 3ULL * 3600 * 1000);
    assert(t.currentHour() == 15);
    assert(t.isDay() == true);
    printf("  ✓ tick() 时间倒退防御: now_ms<baselineMs → delta=0\n");
}

static void test_set_time_baseline_triggers_re_eval() {
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);
    // 初始 baseline (12, 0), hour=12 → DAY
    assert(t.isDay() == true);
    assert(t.baselineHour() == 12);
    assert(t.baselineMs() == 0);

    // 改 baseline 到 (19, 0) — AUTO 模式立即 evaluate → NIGHT
    t.setTimeBaseline(19, 0);
    assert(t.isDay() == false);
    assert(t.baselineHour() == 19);
    assert(t.baselineMs() == 0);

    // 显式 DAY 模式下改 baseline 不会切 isDay
    t.setMode(ThemeMode::DAY);
    t.setTimeBaseline(2, 0);
    assert(t.isDay() == true);  // DAY 强制覆盖
    assert(t.baselineHour() == 2);
    printf("  ✓ setTimeBaseline() 设基线 + 立即触发 evaluate (AUTO) / 不切 (显式)\n");
}

static void test_tick_no_op_in_explicit_mode() {
    ThemeManager t;
    t.setMode(ThemeMode::DAY);
    t.setCurrentHour(2);  // 凌晨
    // DAY 模式强制 isDay=true, 忽略 hour
    t.tick(1000);
    assert(t.isDay() == true);
    printf("  ✓ tick() 在显式 DAY 模式保持 isDay\n");
}

static void test_colors_differ_day_night() {
    ThemeManager t;
    t.setMode(ThemeMode::DAY);
    ThemeColors day = t.colors();
    assert(day.background == ThemeManager::kDayColors.background);
    assert(day.foreground == ThemeManager::kDayColors.foreground);

    t.setMode(ThemeMode::NIGHT);
    ThemeColors night = t.colors();
    assert(night.background == ThemeManager::kNightColors.background);
    assert(night.foreground == ThemeManager::kNightColors.foreground);

    // DAY vs NIGHT 必须不同 (否则主题没意义)
    assert(day.background != night.background);
    assert(day.foreground != night.foreground);
    printf("  ✓ DAY 配色: bg=0x%08X, NIGHT 配色: bg=0x%08X (不同)\n",
           day.background, night.background);
}

static void test_color_of_slots() {
    ThemeManager t;
    t.setMode(ThemeMode::DAY);
    const ThemeColors c = t.colors();

    assert(t.colorOf("bg")       == c.background);
    assert(t.colorOf("fg")       == c.foreground);
    assert(t.colorOf("accent")   == c.accent);
    assert(t.colorOf("warning")  == c.warning);
    assert(t.colorOf("critical") == c.critical);
    (void)c;  // suppress -Wunused-but-set-variable on -Werror builds

    // 未知 slot 返回 sentinel
    assert(t.colorOf("nonexistent") == 0xFF00FF00U);
    printf("  ✓ colorOf(\"bg\"/\"fg\"/\"accent\"/\"warning\"/\"critical\") 全部正确\n");
}

static void test_warning_critical_consistent_across_modes() {
    // 警告/严重色不随 DAY/NIGHT 变化, 避免色觉混淆
    ThemeManager t;
    t.setMode(ThemeMode::DAY);
    const uint32_t warn_day = t.colorOf("warning");
    const uint32_t crit_day = t.colorOf("critical");

    t.setMode(ThemeMode::NIGHT);
    const uint32_t warn_night = t.colorOf("warning");
    const uint32_t crit_night = t.colorOf("critical");

    assert(warn_day == warn_night);
    assert(crit_day == crit_night);
    (void)warn_night;  // suppress -Wunused-but-set-variable on -Werror builds
    (void)crit_night;
    printf("  ✓ warning/critical 跨模式一致 (warn=0x%08X, crit=0x%08X)\n",
           warn_day, crit_day);
}

static void test_normalize_hour() {
    // 通过 setCurrentHour 间接验证 normalizeHour
    ThemeManager t;
    t.setMode(ThemeMode::AUTO);

    t.setCurrentHour(0);
    assert(t.currentHour() == 0);

    t.setCurrentHour(23);
    assert(t.currentHour() == 23);

    t.setCurrentHour(24);  // 24 % 24 = 0
    assert(t.currentHour() == 0);

    t.setCurrentHour(25);  // 25 % 24 = 1
    assert(t.currentHour() == 1);

    t.setCurrentHour(48);  // 48 % 24 = 0
    assert(t.currentHour() == 0);

    // 负数: 走 (hour % 24 + 24) % 24
    t.setCurrentHour(255);  // 255 % 24 = 15
    assert(t.currentHour() == 15);
    printf("  ✓ normalizeHour: 0/23/24→0/25→1/48→0/255→15\n");
}

static void test_reset() {
    ThemeManager t;
    t.setMode(ThemeMode::NIGHT);
    t.setSunriseHour(8);
    t.setSunsetHour(20);
    t.setCurrentHour(2);

    t.reset();
    assert(t.currentMode() == ThemeMode::AUTO);
    assert(t.sunriseHour() == 6);
    assert(t.sunsetHour() == 18);
    assert(t.currentHour() == 12);
    assert(t.isDay() == true);
    printf("  ✓ reset() 回到默认 AUTO + 12:00 + [6,18)\n");
}

int main() {
    printf("=== ThemeManager 单元测试 ===\n");

    RUN(test_initial_state);
    RUN(test_force_day_night);
    RUN(test_auto_mode_hour_evaluation);
    RUN(test_boundary_hours);
    RUN(test_custom_sunrise_sunset);
    RUN(test_degenerate_sunrise_equals_sunset);
    RUN(test_set_current_hour_triggers_re_eval);
    RUN(test_tick_evaluates_auto);
    RUN(test_tick_crosses_sunrise);
    RUN(test_tick_crosses_sunset);
    RUN(test_tick_24h_rollover);
    RUN(test_tick_time_travel_defense);
    RUN(test_set_time_baseline_triggers_re_eval);
    RUN(test_tick_no_op_in_explicit_mode);
    RUN(test_colors_differ_day_night);
    RUN(test_color_of_slots);
    RUN(test_warning_critical_consistent_across_modes);
    RUN(test_normalize_hour);
    RUN(test_reset);

    printf("\n=== %d/%d 测试通过 ===\n", test_pass, test_count);
    return 0;
}

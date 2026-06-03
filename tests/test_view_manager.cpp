// test_view_manager.cpp
// Layer 2 ViewManager 单元测试 (纯 C++, 无 Qt)
//
// 覆盖:
//   1. 初始状态 (DRIVE 默认)
//   2. setGear(D=3) + tick → DRIVE
//   3. setGear(P=0) + tick (1s+) → SETUP
//   4. setGear(P) 立即查 → 仍是 DRIVE (hysteresis 未满)
//   5. setCharge(1) + tick → CHARGE (高优先级)
//   6. setCharge(1) + setGear(D) → CHARGE (charge 优先于 drive)
//   7. setCharge(0) + setGear(D) + tick → DRIVE
//   8. setCharge(0) + setGear(P) + tick → SETUP
//   9. setHysteresisMs(0) → tick 立即切 (无防抖)
//  10. tick 推进 1.5s 后才允许切换 (debounce 边界)
//  11. alarm_active 不影响 view
//  12. reset() 回到 DRIVE
//  13. setViewForTest() 立即跳 (绕过 hysteresis)
//  14. isDrive/isCharge/isSetup 派生标志
//  15. ViewMode enum 值 + sizeof(ViewSnapshot)
//  16. snapshot() 一次返回全部字段
//  17. R 倒档归为 DRIVE
//  18. S 运动档归为 DRIVE
//  19. N 空档 + 未充电 → SETUP

#include <cstdio>
#include <cassert>
#include <cstring>
#include "../src/layer2/view_manager.h"

using candash::ViewManager;
using candash::ViewMode;
using candash::ViewSnapshot;

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { \
        g_test_passed++; \
        printf("  \xe2\x9c\x93 %s\n", msg); \
    } else { \
        printf("  \xe2\x9c\x97 %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define RUN(name) do { \
    printf("\n[%d] %s:\n", ++g_test_section, #name); \
    name(); \
} while(0)

static int g_test_section = 0;

// ─── 测试用例 ───

static void test_initial_state() {
    ViewManager v;
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "默认 view = DRIVE");
    TEST_ASSERT(v.isDrive(), "isDrive() = true");
    TEST_ASSERT(!v.isCharge(), "isCharge() = false");
    TEST_ASSERT(!v.isSetup(), "isSetup() = false");
}

static void test_drive_view_when_in_drive_gear() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);  // 1s 后
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "D 档 → DRIVE");
}

static void test_setup_view_when_parked() {
    // 测: 第二次切换受 hysteresis 约束 (首次切到 SETUP 是 free, 之后切回 DRIVE 要等 1s)
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);  // 首次 tick: no switch (D==DRIVE)
    v.setGearStatus(ViewManager::kGearPark);
    v.tick(1500);  // 首次 switch (free) → SETUP, lastChangeMs=1500
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "P 档 → SETUP (首次切 free)");
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1600);  // 距 lastChangeMs=1500 仅 100ms < hysteresis(1000) → 不切
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "hysteresis 未满, 仍 SETUP");
    v.tick(2600);  // 距 lastChangeMs=1500 满 1100ms ≥ 1000 → 切 DRIVE
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "hysteresis 满后切到 DRIVE");
}

static void test_hysteresis_holds_immediate_transition() {
    // 测: 第二次切换 (charge→off) 立即发生, 但**再切回** (再次 charge) 受 hysteresis 约束
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);  // 首次 tick: no switch
    v.setChargeStatus(ViewManager::kChargeActive);
    v.tick(1500);  // 首次 switch (free) → CHARGE, lastChangeMs=1500
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "首次 charge 切到 CHARGE");
    v.setChargeStatus(ViewManager::kChargeIdle);
    v.tick(1600);  // 距 lastChangeMs=1500 仅 100ms < hysteresis(1000) → 不切
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "hysteresis 未满, 仍 CHARGE");
    v.tick(2700);  // 距 lastChangeMs=1500 满 1200ms ≥ 1000 → 切 DRIVE
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "hysteresis 满后切到 DRIVE");
}

static void test_charge_view_overrides_drive() {
    // 测: charge 触发首次切到 CHARGE (free), 再次 charge→idle 切回受 hysteresis 约束
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);  // 首次 tick: no switch
    v.setChargeStatus(ViewManager::kChargeActive);
    v.tick(1500);  // 首次 switch (free) → CHARGE
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "charge 触发 → CHARGE");
    TEST_ASSERT(v.isCharge(), "isCharge() = true");
    v.setChargeStatus(ViewManager::kChargeIdle);
    v.tick(1600);  // hysteresis 未满, 不切
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "hysteresis 未满, 仍 CHARGE");
    v.tick(2600);  // 切回 DRIVE
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "charge 结束 → DRIVE");
}

static void test_charge_priority_over_drive() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.setChargeStatus(ViewManager::kChargeActive);
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 1000-0=1000 ≥ hysteresis → 切
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "同时 charge + drive → CHARGE (charge 优先)");
}

static void test_charge_ends_back_to_drive() {
    ViewManager v;
    v.setChargeStatus(ViewManager::kChargeActive);
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 1000-0=1000 ≥ 1000 → 切 CHARGE
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "charge 切到 CHARGE");
    v.setChargeStatus(ViewManager::kChargeIdle);
    v.tick(2500);  // 距 lastChangeMs=1000 满 1500ms ≥ 1000 → 切 DRIVE
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "charge 结束 + D 档 → DRIVE");
}

static void test_charge_ends_back_to_setup() {
    ViewManager v;
    v.setChargeStatus(ViewManager::kChargeActive);
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 切 CHARGE
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "charge → CHARGE");
    v.setChargeStatus(ViewManager::kChargeIdle);
    v.setGearStatus(ViewManager::kGearPark);
    v.tick(2500);  // 距 lastChangeMs=1000 满 1500ms ≥ 1000 → candidate=SETUP, 切
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "charge 结束 + P 档 → SETUP");
}

static void test_hysteresis_zero_immediate_transition() {
    ViewManager v;
    v.setHysteresisMs(0);
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);
    v.setGearStatus(ViewManager::kGearPark);
    v.tick(1100);  // 仅 100ms 后, 但 hysteresis=0 → 立即切
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "hysteresis=0 → 100ms 内也切");
}

static void test_tick_advances_time() {
    ViewManager v;
    v.setHysteresisMs(1000);
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(0);    // 首次 tick: no switch (D==DRIVE)
    v.setGearStatus(ViewManager::kGearPark);
    v.tick(0);    // 首次 switch (free) → SETUP, lastChangeMs=0
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "首次切到 SETUP");
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(500);  // 500 < 1000
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "tick(500) hysteresis 未满, 仍 SETUP");
    v.tick(1500); // 距 lastChangeMs=0 满 1500ms ≥ 1000 → 切
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "tick(1500) 切到 DRIVE");
}

static void test_alarm_does_not_change_view() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.tick(1000);
    v.setAlarmActive(1);  // 告警
    v.tick(1500);
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "alarm 不切 view (独立面板处理)");
    TEST_ASSERT(v.isDrive(), "仍在 DRIVE");
}

static void test_reset_returns_to_drive() {
    ViewManager v;
    v.setChargeStatus(ViewManager::kChargeActive);
    v.tick(0);
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "已切到 CHARGE");
    v.reset();
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "reset → DRIVE");
    TEST_ASSERT(v.snapshot().gear == ViewManager::kGearPark, "reset 清 gear → P");
    TEST_ASSERT(v.snapshot().charge == ViewManager::kChargeIdle, "reset 清 charge → idle");
}

static void test_set_view_for_test_bypasses_hysteresis() {
    ViewManager v;
    v.setHysteresisMs(10000);  // 10s 防抖
    v.tick(0);  // 默认 DRIVE
    v.setViewForTest(ViewMode::CHARGE);
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "setViewForTest 绕过 hysteresis 立即切");
    v.setViewForTest(ViewMode::SETUP);
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "setViewForTest 多次切都立即生效");
}

static void test_derived_flags() {
    ViewManager v;
    v.setViewForTest(ViewMode::DRIVE);
    TEST_ASSERT(v.isDrive() && !v.isCharge() && !v.isSetup(), "DRIVE 派生标志");
    v.setViewForTest(ViewMode::CHARGE);
    TEST_ASSERT(!v.isDrive() && v.isCharge() && !v.isSetup(), "CHARGE 派生标志");
    v.setViewForTest(ViewMode::SETUP);
    TEST_ASSERT(!v.isDrive() && !v.isCharge() && v.isSetup(), "SETUP 派生标志");
}

static void test_view_mode_enum_values() {
    TEST_ASSERT(static_cast<uint8_t>(ViewMode::DRIVE)  == 0, "DRIVE = 0");
    TEST_ASSERT(static_cast<uint8_t>(ViewMode::CHARGE) == 1, "CHARGE = 1");
    TEST_ASSERT(static_cast<uint8_t>(ViewMode::SETUP)  == 2, "SETUP = 2");
}

static void test_snapshot() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearDrive);
    v.setChargeStatus(ViewManager::kChargeActive);
    v.setViewForTest(ViewMode::CHARGE);

    ViewSnapshot snap = v.snapshot();
    TEST_ASSERT(snap.current == static_cast<uint8_t>(ViewMode::CHARGE), "snapshot.current = CHARGE");
    TEST_ASSERT(snap.gear == ViewManager::kGearDrive, "snapshot.gear = D");
    TEST_ASSERT(snap.charge == ViewManager::kChargeActive, "snapshot.charge = active");
    TEST_ASSERT(snap._pad == 0, "snapshot._pad = 0 (对齐)");
    TEST_ASSERT(sizeof(ViewSnapshot) == 4, "ViewSnapshot sizeof = 4 (紧凑)");
}

static void test_reverse_gear_is_drive() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearReverse);
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 1000-0=1000 ≥ hysteresis → 切 DRIVE
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "R 倒档归为 DRIVE");
}

static void test_sport_gear_is_drive() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearSport);
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 切 DRIVE
    TEST_ASSERT(v.currentView() == ViewMode::DRIVE, "S 运动档归为 DRIVE");
}

static void test_neutral_gear_no_charge_is_setup() {
    ViewManager v;
    v.setGearStatus(ViewManager::kGearNeutral);
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 切 SETUP
    TEST_ASSERT(v.currentView() == ViewMode::SETUP, "N 空档 + 未充电 → SETUP");
}

static void test_charge_value_above_1_still_charging() {
    ViewManager v;
    v.setChargeStatus(2);  // 任何 > 0 都视为 charging
    v.tick(1000);  // 首次 tick: lastChangeMs=0. 切 CHARGE
    TEST_ASSERT(v.currentView() == ViewMode::CHARGE, "charge=2 → CHARGE");
}

int main() {
    printf("=== ViewManager 单元测试 ===\n\n");

    RUN(test_initial_state);
    RUN(test_drive_view_when_in_drive_gear);
    RUN(test_setup_view_when_parked);
    RUN(test_hysteresis_holds_immediate_transition);
    RUN(test_charge_view_overrides_drive);
    RUN(test_charge_priority_over_drive);
    RUN(test_charge_ends_back_to_drive);
    RUN(test_charge_ends_back_to_setup);
    RUN(test_hysteresis_zero_immediate_transition);
    RUN(test_tick_advances_time);
    RUN(test_alarm_does_not_change_view);
    RUN(test_reset_returns_to_drive);
    RUN(test_set_view_for_test_bypasses_hysteresis);
    RUN(test_derived_flags);
    RUN(test_view_mode_enum_values);
    RUN(test_snapshot);
    RUN(test_reverse_gear_is_drive);
    RUN(test_sport_gear_is_drive);
    RUN(test_neutral_gear_no_charge_is_setup);
    RUN(test_charge_value_above_1_still_charging);

    printf("\n=== %d/%d 测试通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

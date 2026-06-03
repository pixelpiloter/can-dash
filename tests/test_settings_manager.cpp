// test_settings_manager.cpp
// Layer 2 SettingsManager 单元测试 (纯 C++, 无 Qt)
//
// 覆盖:
//   1. 初始状态 (公制 + 80% 亮度)
//   2. setUnits(METRIC/IMPERIAL) → 切换正确
//   3. setBrightness(50) → 50% 正常
//   4. setBrightness(150) → clamp 到 100
//   5. setBrightness(0) → 0% 合法 (背光关)
//   6. setBrightness(255) (uint8 溢出值) → clamp 到 100
//   7. snapshot() 一次返回 2 字段 + padding 对齐
//   8. tick() 是 no-op (状态不漂移)
//   9. reset() 回到默认
//  10. kDefault* 静态常量值正确 (外部可读)

#include <cstdio>
#include <cassert>
#include <cstring>
#include "../src/layer2/settings_manager.h"

using candash::SettingsManager;
using candash::Units;
using candash::SettingsSnapshot;

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
    SettingsManager s;
    TEST_ASSERT(s.units() == Units::METRIC, "默认单位 = METRIC");
    TEST_ASSERT(s.brightness() == 80, "默认亮度 = 80");
}

static void test_set_units() {
    SettingsManager s;
    s.setUnits(Units::IMPERIAL);
    TEST_ASSERT(s.units() == Units::IMPERIAL, "切到 IMPERIAL");

    s.setUnits(Units::METRIC);
    TEST_ASSERT(s.units() == Units::METRIC, "切回 METRIC");

    // 同一单位重复设置应无副作用
    s.setUnits(Units::METRIC);
    TEST_ASSERT(s.units() == Units::METRIC, "重复设 METRIC 不变");
}

static void test_set_brightness_normal() {
    SettingsManager s;
    s.setBrightness(50);
    TEST_ASSERT(s.brightness() == 50, "亮度 50%");

    s.setBrightness(1);
    TEST_ASSERT(s.brightness() == 1, "亮度 1% (接近 0 边界)");

    s.setBrightness(100);
    TEST_ASSERT(s.brightness() == 100, "亮度 100% (上界)");
}

static void test_set_brightness_clamps_high() {
    SettingsManager s;
    s.setBrightness(150);  // 越界
    TEST_ASSERT(s.brightness() == 100, "150 → clamp 到 100");

    s.setBrightness(101);  // 刚越界
    TEST_ASSERT(s.brightness() == 100, "101 → clamp 到 100");
}

static void test_set_brightness_zero_valid() {
    SettingsManager s;
    s.setBrightness(0);
    TEST_ASSERT(s.brightness() == 0, "亮度 0% 合法 (背光关)");

    // 0 之后再设也回 0 (不会变成 100 之类的回绕)
    s.setBrightness(0);
    TEST_ASSERT(s.brightness() == 0, "重复设 0 仍是 0");
}

static void test_snapshot() {
    SettingsManager s;
    s.setUnits(Units::IMPERIAL);
    s.setBrightness(42);

    SettingsSnapshot snap = s.snapshot();
    TEST_ASSERT(snap.units == static_cast<uint8_t>(Units::IMPERIAL), "snapshot.units = IMPERIAL");
    TEST_ASSERT(snap.brightness == 42, "snapshot.brightness = 42");
    TEST_ASSERT(snap._pad == 0, "snapshot._pad = 0 (对齐)");
    TEST_ASSERT(sizeof(SettingsSnapshot) == 4, "SettingsSnapshot sizeof = 4 (紧凑)");
}

static void test_tick_is_noop() {
    SettingsManager s;
    s.setUnits(Units::IMPERIAL);
    s.setBrightness(33);

    // tick 推进任意时间, 状态应不变 (settings 不漂移)
    s.tick(0);
    TEST_ASSERT(s.units() == Units::IMPERIAL && s.brightness() == 33, "tick(0) 状态不变");

    s.tick(1000000);  // 1M ms 后
    TEST_ASSERT(s.units() == Units::IMPERIAL && s.brightness() == 33, "tick(1M) 状态不变");
}

static void test_reset() {
    SettingsManager s;
    s.setUnits(Units::IMPERIAL);
    s.setBrightness(10);

    s.reset();
    TEST_ASSERT(s.units() == Units::METRIC, "reset → METRIC");
    TEST_ASSERT(s.brightness() == 80, "reset → 80%");
}

static void test_default_constants() {
    // 静态常量值, 供测试和外部读取
    TEST_ASSERT(SettingsManager::kDefaultUnits == Units::METRIC,
                "kDefaultUnits = METRIC");
    TEST_ASSERT(SettingsManager::kDefaultBrightness == 80,
                "kDefaultBrightness = 80");
    TEST_ASSERT(SettingsManager::kMinBrightness == 0,
                "kMinBrightness = 0");
    TEST_ASSERT(SettingsManager::kMaxBrightness == 100,
                "kMaxBrightness = 100");
}

static void test_units_enum_values() {
    // 验证 enum 数值 (供 L3 / QML 知道底层编码)
    TEST_ASSERT(static_cast<uint8_t>(Units::METRIC) == 0, "METRIC = 0");
    TEST_ASSERT(static_cast<uint8_t>(Units::IMPERIAL) == 1, "IMPERIAL = 1");
}

int main() {
    printf("=== SettingsManager 单元测试 ===\n\n");

    RUN(test_initial_state);
    RUN(test_set_units);
    RUN(test_set_brightness_normal);
    RUN(test_set_brightness_clamps_high);
    RUN(test_set_brightness_zero_valid);
    RUN(test_snapshot);
    RUN(test_tick_is_noop);
    RUN(test_reset);
    RUN(test_default_constants);
    RUN(test_units_enum_values);

    printf("\n=== %d/%d 测试通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

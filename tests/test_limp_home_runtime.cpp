// test_limp_home_runtime.cpp
// Layer 2 LimpHomeRuntime 单元测试 (PR 43: 跛行模式)

#include <cstdio>
#include <cstring>
#include <cassert>
#include "../src/layer2/limp_home_runtime.h"
#include "../src/generated/limp_home_def.h"

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { g_test_passed++; printf("  ✓ %s\n", msg); } \
    else { printf("  ✗ %s (line %d)\n", msg, __LINE__); } \
} while(0)

int main() {
    printf("=== LimpHomeRuntime 单元测试 (PR 43) ===\n\n");

    // ─── 测试 1: init 初始状态, 启动时全部信号超时, 至少进入 L1 ───
    printf("[测试 1] init 初始状态\n");
    {
        LimpHomeRuntime rt;
        rt.init(&LIMP_HOME_CONFIG);
        const auto& st = rt.state();
        TEST_ASSERT(st.signalCount == 2, "signalCount=2 (vehicle_speed+motor_rpm)");
        TEST_ASSERT(st.currentLevel == LIMP_LEVEL_NORMAL, "init level=NORMAL (未 tick)");
        TEST_ASSERT(st.signalStatus[0].display_key != nullptr, "signalStatus[0] 有 display_key");
        TEST_ASSERT(std::strcmp(st.signalStatus[0].display_key, "vehicle_speed") == 0, "signalStatus[0]=vehicle_speed");
    }

    // ─── 测试 2: tick 后超时 → L1 (单关键信号超时) ───
    printf("[测试 2] tick 触发 L1 (单信号超时)\n");
    {
        LimpHomeRuntime rt;
        rt.init(&LIMP_HOME_CONFIG);

        // 两个信号都给一帧 (避免启动时全超时)
        rt.onValueChanged("vehicle_speed", 1000);
        rt.onValueChanged("motor_rpm", 1000);

        // 推进时间到 600ms (L1 阈值 500ms, 但 L3 阈值 3000ms — 信号 inTimeout 用 L3 阈值判断)
        rt.tick(1600);  // elapsed = 600ms, 不到 3000ms, 都不超时
        TEST_ASSERT(rt.state().currentLevel == LIMP_LEVEL_NORMAL, "elapsed=600ms < L3=3000ms, 仍 NORMAL");

        // 推进到 3500ms (两个信号都超时)
        rt.tick(4500);
        TEST_ASSERT(rt.state().currentLevel == LIMP_LEVEL_L3, "两个都超时, 进入 L3 (>=2)");

        // 只更新 motor_rpm, vehicle_speed 仍超时
        rt.onValueChanged("motor_rpm", 4500);
        rt.tick(4500 + 100);  // 100ms 后, motor_rpm 100ms (新) < 3000ms, vehicle_speed > 3000ms
        TEST_ASSERT(rt.state().currentLevel == LIMP_LEVEL_L1, "1 个超时 → L1 (>=1)");
        TEST_ASSERT(rt.state().timeoutSignalCount == 1, "timeoutSignalCount=1");
    }

    // ─── 测试 3: 恢复逻辑 (连续有效帧) ───
    printf("[测试 3] 恢复 (连续有效帧)\n");
    {
        LimpHomeRuntime rt;
        rt.init(&LIMP_HOME_CONFIG);
        rt.onValueChanged("vehicle_speed", 1000);
        rt.onValueChanged("motor_rpm", 1000);

        // 触发 L1: vehicle_speed 更新到 4000 (新), motor_rpm 仍 1000 (超时)
        rt.onValueChanged("vehicle_speed", 4000);
        rt.tick(4500);
        TEST_ASSERT(rt.state().currentLevel == LIMP_LEVEL_L1, "1 个超时 (motor_rpm) → L1");

        // 恢复: motor_rpm 连续 3 帧有效 (required_valid_frames=3)
        // tick 内部连续有效帧只对刚恢复的信号 +1, 然后 evaluateLevel 把它重置
        // 简化: 直接给 motor_rpm 多次更新, 让 vehicle_speed 也不超时
        for (int i = 0; i < 5; ++i) {
            rt.onValueChanged("motor_rpm", 5000 + i * 100);
            rt.onValueChanged("vehicle_speed", 5000 + i * 100);
            rt.tick(5000 + (i+1) * 100);
            printf("    [iter %d] level=%d valid=%d tc=%d\n",
                   i, rt.state().currentLevel, rt.state().consecutiveValidFrames, rt.state().timeoutSignalCount);
        }
        TEST_ASSERT(rt.state().currentLevel == LIMP_LEVEL_NORMAL, "5 次有效帧后恢复 NORMAL");
    }

    // ─── 测试 4: query 输出 message ───
    printf("[测试 4] query 消息输出\n");
    {
        LimpHomeRuntime rt;
        rt.init(&LIMP_HOME_CONFIG);
        rt.onValueChanged("vehicle_speed", 1000);
        rt.onValueChanged("motor_rpm", 1000);
        rt.tick(4500);  // 全超时 → L3

        LimpHomeQueryResult q;
        rt.query(q);
        TEST_ASSERT(q.level == LIMP_LEVEL_L3, "query.level=L3");
        TEST_ASSERT(q.active, "query.active=true");
        TEST_ASSERT(std::strcmp(q.messageZh, LIMP_HOME_CONFIG.msg_l3_zh) == 0, "messageZh 跟 config 一致");
        TEST_ASSERT(std::strcmp(q.messageEn, LIMP_HOME_CONFIG.msg_l3_en) == 0, "messageEn 跟 config 一致");
    }

    // ─── 测试 5: 非关键信号忽略 ───
    printf("[测试 5] 非关键信号 onValueChanged 忽略\n");
    {
        LimpHomeRuntime rt;
        rt.init(&LIMP_HOME_CONFIG);
        rt.onValueChanged("brake", 1000);  // 非关键信号
        TEST_ASSERT(rt.state().timeoutSignalCount == 0, "brake 不是关键信号, 不计入 timeoutSignalCount");
    }

    printf("\n=== 总结: %d / %d 通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

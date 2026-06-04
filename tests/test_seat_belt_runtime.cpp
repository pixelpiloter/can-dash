// test_seat_belt_runtime.cpp
// Layer 2 SeatBeltRuntime 单元测试 (PR 23: 升级 39 行 stub 为真实用例)
// 跟 PR 21 (alarm_runtime) / PR 22 (can_converter) 同 TEST_ASSERT 骨架.

#include <cstdio>
#include <cstring>
#include <cassert>
#include "../src/layer2/seat_belt_runtime.h"
#include "../src/generated/seat_belt_def.h"

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { g_test_passed++; printf("  ✓ %s\n", msg); } \
    else { printf("  ✗ %s (line %d)\n", msg, __LINE__); } \
} while(0)

int main() {
    printf("=== SeatBeltRuntime 单元测试 ===\n\n");

    // ─── 测试 1: init 初始状态, 5 座位全部未占未系, 无人报警 ───
    printf("[测试 1] init 初始状态\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const auto& st = rt.states();
        TEST_ASSERT(st.seatCount == 5, "seatCount=5");
        TEST_ASSERT(!st.moving, "moving=false");
        TEST_ASSERT(st.currentSpeed == 0.0f, "currentSpeed=0");
        TEST_ASSERT(!st.speedValid, "speedValid=false");
        for (int i = 0; i < st.seatCount; i++) {
            TEST_ASSERT(!st.seats[i].seatOccupied, "seat[i].occupied=false");
            TEST_ASSERT(!st.seats[i].beltBuckled, "seat[i].buckled=false");
            TEST_ASSERT(!st.seats[i].warning, "seat[i].warning=false");
            TEST_ASSERT(!st.seats[i].hint, "seat[i].hint=false");
        }
    }

    // ─── 测试 2: 速度阈值 5.0 km/h (严格 >) ───
    printf("\n[测试 2] 速度阈值 5.0 km/h (严格 >)\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        rt.updateSpeed(0.0f, true);
        TEST_ASSERT(!rt.states().moving, "speed=0 → moving=false");
        rt.updateSpeed(4.9f, true);
        TEST_ASSERT(!rt.states().moving, "speed=4.9 < 5.0 → moving=false");
        rt.updateSpeed(5.0f, true);
        TEST_ASSERT(!rt.states().moving, "speed=5.0 == 5.0 → moving=false (严格 >)");
        rt.updateSpeed(5.1f, true);
        TEST_ASSERT(rt.states().moving, "speed=5.1 > 5.0 → moving=true");
        rt.updateSpeed(30.0f, true);
        TEST_ASSERT(rt.states().moving, "speed=30 → moving=true");
    }

    // ─── 测试 3: 速度 invalid 强制 not moving (无论速度多大) ───
    printf("\n[测试 3] speed invalid 强制 moving=false\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        rt.updateSpeed(100.0f, false);
        TEST_ASSERT(!rt.states().moving, "speed=100 valid=false → moving=false");
    }

    // ─── 测试 4: CAN 0x2F0 推 driver 占用 (can_id 752) ───
    printf("\n[测试 4] CAN 0x2F0 推 driver 占用\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t data[1] = {0x01};  // bit 0 set
        rt.updateCanFrame(752, data, 1);
        const auto& st = rt.states();
        TEST_ASSERT(st.seats[0].seatOccupied, "driver.occupied=true");
        TEST_ASSERT(!st.seats[0].beltBuckled, "driver.buckled=false (没动 buckle 帧)");
        TEST_ASSERT(st.seats[0].lastUpdateMs > 0, "driver.lastUpdateMs 写入");
        // 752 不应影响其他座位 (rear 座位 seat_occupied_can_id=0 永不匹配)
        TEST_ASSERT(!st.seats[2].seatOccupied, "rear_left.occupied=false");
        TEST_ASSERT(!st.seats[1].seatOccupied, "passenger.occupied=false");
    }

    // ─── 测试 5: CAN 0x3B0 推 driver buckle (can_id 944) ───
    printf("\n[测试 5] CAN 0x3B0 推 driver buckle\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t data[1] = {0x01};
        rt.updateCanFrame(944, data, 1);
        TEST_ASSERT(rt.states().seats[0].beltBuckled, "driver.buckled=true");
        // 944 不应触发其他座位 (passenger 用 945)
        TEST_ASSERT(!rt.states().seats[1].beltBuckled, "passenger.buckled=false");
    }

    // ─── 测试 6: 静止 + 占用 + 未系 → hint=true, warning=false ───
    printf("\n[测试 6] 静止+占+未系 → hint (非 warning)\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateSpeed(0.0f, true);
        const auto& st = rt.states();
        TEST_ASSERT(st.seats[0].seatOccupied, "driver.occupied=true");
        TEST_ASSERT(!st.seats[0].warning, "stationary+未系 → warning=false");
        TEST_ASSERT(st.seats[0].hint, "stationary+占+未系 → hint=true");
    }

    // ─── 测试 7: 行驶 + 占用 + 未系 → warning=true, hint=false ───
    printf("\n[测试 7] 行驶+占+未系 → warning\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateSpeed(30.0f, true);
        const auto& st = rt.states();
        TEST_ASSERT(st.seats[0].warning, "moving+占+未系 → warning=true");
        TEST_ASSERT(!st.seats[0].hint, "moving+占+未系 → hint=false");
    }

    // ─── 测试 8: 行驶 + 占用 + 已系 → 无报警 ───
    printf("\n[测试 8] 行驶+占+已系 → 无报警\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        const uint8_t buck[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateCanFrame(944, buck, 1);
        rt.updateSpeed(30.0f, true);
        const auto& st = rt.states();
        TEST_ASSERT(!st.seats[0].warning, "moving+占+已系 → warning=false");
        TEST_ASSERT(!st.seats[0].hint, "moving+占+已系 → hint=false");
    }

    // ─── 测试 9: 行驶 + 未占 → 无报警 (require_seat_occupied=true) ───
    printf("\n[测试 9] 行驶+未占 → 无报警 (require_occ=true)\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        rt.updateSpeed(30.0f, true);
        const auto& st = rt.states();
        TEST_ASSERT(!st.seats[0].warning, "moving+未占 → warning=false");
        TEST_ASSERT(!st.seats[0].hint, "moving+未占 → hint=false");
    }

    // ─── 测试 10: 状态切换重评估 (静止→行驶) ───
    printf("\n[测试 10] 静止→行驶 触发 re-evaluate\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateSpeed(0.0f, true);
        TEST_ASSERT(rt.states().seats[0].hint, "初始 hint=true (stationary)");
        TEST_ASSERT(!rt.states().seats[0].warning, "初始 warning=false (stationary)");
        rt.updateSpeed(30.0f, true);
        TEST_ASSERT(rt.states().seats[0].warning, "切到 moving → warning=true (re-eval)");
        TEST_ASSERT(!rt.states().seats[0].hint, "切到 moving → hint=false");
    }

    // ─── 测试 11: 行驶中系上安全带 → 取消报警 ───
    printf("\n[测试 11] 行驶中系上 → warning 取消\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateSpeed(30.0f, true);
        TEST_ASSERT(rt.states().seats[0].warning, "初始 warning=true");
        const uint8_t buck[1] = {0x01};
        rt.updateCanFrame(944, buck, 1);
        TEST_ASSERT(!rt.states().seats[0].warning, "系上后 warning=false");
        TEST_ASSERT(!rt.states().seats[0].hint, "系上后 hint=false");
    }

    // ─── 测试 12: query 字段 (单座位 warning) ───
    printf("\n[测试 12] query 字段 (单座位)\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateSpeed(30.0f, true);
        SeatBeltQueryResult r = {};
        rt.query(r);
        TEST_ASSERT(r.anyWarning, "anyWarning=true");
        TEST_ASSERT(r.anyUnbuckled, "anyUnbuckled=true");
        TEST_ASSERT(r.unbuckledCount == 1, "unbuckledCount=1");
        TEST_ASSERT(r.messageZh[0] != '\0', "messageZh 非空");
        TEST_ASSERT(std::strstr(r.messageZh, "请系安全带") != nullptr,
                    "messageZh 含'请系安全带'");
    }

    // ─── 测试 13: query 多座位 warning (driver + passenger) ───
    printf("\n[测试 13] query 字段 (多座位)\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);  // driver
        rt.updateCanFrame(753, occ, 1);  // passenger
        rt.updateSpeed(30.0f, true);
        SeatBeltQueryResult r = {};
        rt.query(r);
        TEST_ASSERT(r.anyWarning, "anyWarning=true");
        TEST_ASSERT(r.unbuckledCount == 2, "unbuckledCount=2");
        TEST_ASSERT(std::strstr(r.messageZh, "+") != nullptr,
                    "多座位消息含 + 分隔符");
    }

    // ─── 测试 14: 全部已系 → query 空消息 ───
    printf("\n[测试 14] query 字段 (无报警)\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        const uint8_t buck[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateCanFrame(944, buck, 1);
        rt.updateSpeed(30.0f, true);
        SeatBeltQueryResult r = {};
        rt.query(r);
        TEST_ASSERT(!r.anyWarning, "anyWarning=false");
        TEST_ASSERT(!r.anyUnbuckled, "anyUnbuckled=false");
        TEST_ASSERT(r.unbuckledCount == 0, "unbuckledCount=0");
        TEST_ASSERT(r.messageZh[0] == '\0', "messageZh 空串");
    }

    // ─── 测试 15: 未知 CAN ID → 无效果 ───
    printf("\n[测试 15] 未知 CAN ID 忽略\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF,
                                 0xFF, 0xFF, 0xFF, 0xFF};
        rt.updateCanFrame(0x999, data, 8);
        const auto& st = rt.states();
        for (int i = 0; i < st.seatCount; i++) {
            TEST_ASSERT(!st.seats[i].seatOccupied, "无座位被占用");
            TEST_ASSERT(!st.seats[i].beltBuckled, "无座位被系上");
        }
    }

    // ─── 测试 16: tick() no-op (L2 业务状态不变) ───
    printf("\n[测试 16] tick() 不影响业务状态\n");
    {
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &SEAT_BELT_CONFIG);
        const uint8_t occ[1] = {0x01};
        rt.updateCanFrame(752, occ, 1);
        rt.updateSpeed(30.0f, true);
        TEST_ASSERT(rt.states().seats[0].warning, "tick 前 warning=true");
        rt.tick(99999);
        TEST_ASSERT(rt.states().seats[0].warning, "tick 后 warning=true (no-op)");
    }

    // ─── 测试 17: require_seat_occupied=false 模式 (本地自定义 config) ───
    printf("\n[测试 17] require_seat_occupied=false 模式\n");
    {
        SeatBeltConfigDef custom = SEAT_BELT_CONFIG;
        custom.require_seat_occupied = false;  // 覆盖默认 true
        SeatBeltRuntime rt;
        rt.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT, &custom);
        rt.updateSpeed(30.0f, true);
        const auto& st = rt.states();
        // require_occ=false → 无人座位也报警 (driver 未占 → 但 buckled=false → 应 warning=true)
        TEST_ASSERT(st.seats[0].warning,
                    "moving+未占+buckled=false+require_occ=false → warning=true");
    }

    // ─── 汇总 ───
    printf("\n=== 测试结果: %d/%d 通过 ===\n", g_test_passed, g_test_count);
    if (g_test_passed != g_test_count) {
        printf("FAILED: %d assertions failed\n", g_test_count - g_test_passed);
        return 1;
    }
    return 0;
}

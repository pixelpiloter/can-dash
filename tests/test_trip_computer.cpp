// test_trip_computer.cpp
// Layer 2 TripComputer 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cassert>
#include "../src/layer2/trip_computer.h"

static void test_initial_state() {
    printf("\n[测试1] 初始状态\n");
    TripComputer t;
    assert(t.tripDistanceKm() == 0.0f);
    assert(t.tripAvgSpeedKmh() == 0.0f);
    assert(t.tripDurationS() == 0);
    assert(!t.isMoving());
    printf("  ✓ 全部为 0\n");
}

static void test_first_tick_only_records_baseline() {
    printf("\n[测试2] 首次 tick 仅记录基准, 不算距离\n");
    TripComputer t;
    t.tick(1000, 50.0f);  // 首次
    // 距离/时长 都不应被计算 (无 dt)
    assert(t.tripDistanceKm() == 0.0f);
    assert(t.tripDurationS() == 0);
    assert(t.isMoving());  // 但 isMoving 应当正确反映
    printf("  ✓ 首次 tick 不算距离, isMoving 正确\n");
}

static void test_constant_speed_60kmh_for_1h() {
    printf("\n[测试3] 60 km/h 匀速 (16ms tick × 5625 次 = 90s = 1.5 km)\n");
    TripComputer t;
    t.tick(0, 60.0f);  // 首次 (t=0, baseline)
    // 模拟 60fps × 90s = 5400 次 tick, 但只跑 5625 次确认 (因为 60kmh * 1.5h = 90km 错!)
    // 重算: 60 km/h × 90s / 3600 = 1.5 km
    const int N = 5625;  // 5625 * 16ms = 90s
    for (int i = 1; i <= N; i++) {
        t.tick(static_cast<uint64_t>(i) * 16, 60.0f);
    }
    // distance = 60 km/h × 90s / 3600 = 1.5 km
    assert(t.tripDistanceKm() > 1.49f && t.tripDistanceKm() < 1.51f);
    assert(t.tripDurationS() == 90);
    assert(t.tripAvgSpeedKmh() > 59.9f && t.tripAvgSpeedKmh() < 60.1f);
    printf("  ✓ distance=%.3fkm, duration=%us, avg=%.2fkmh\n",
           t.tripDistanceKm(), t.tripDurationS(), t.tripAvgSpeedKmh());
}

static void test_stop_doesnt_accumulate() {
    printf("\n[测试4] 停车时不累计距离/时长\n");
    TripComputer t;
    t.tick(0, 60.0f);
    t.tick(1000, 0.0f);   // 立即停车
    t.tick(60000, 0.0f);  // 1 分钟停车
    assert(t.tripDistanceKm() == 0.0f);
    assert(t.tripDurationS() == 0);
    assert(!t.isMoving());
    printf("  ✓ 停车 60s 后 distance=0, duration=0\n");
}

static void test_below_threshold_creep_doesnt_count() {
    printf("\n[测试5] 怠速 0.5 km/h (低于 1.0 阈值) 不算行驶\n");
    TripComputer t;
    t.tick(0, 0.5f);
    t.tick(60000, 0.5f);  // 1 分钟 0.5 km/h
    assert(t.tripDistanceKm() == 0.0f);
    assert(t.tripDurationS() == 0);
    assert(!t.isMoving());
    printf("  ✓ 怠速 1min distance=0\n");
}

static void test_trapezoid_integration() {
    printf("\n[测试6] 梯形积分: 0→100 km/h 线性加速 60s, 16ms tick\n");
    TripComputer t;
    t.tick(0, 0.0f);
    // 60s = 3750 ticks × 16ms, 速度从 0 线性增到 100 km/h
    const int N = 3750;
    const float speed_step = 100.0f / static_cast<float>(N);
    for (int i = 1; i <= N; i++) {
        const uint64_t t_ms = static_cast<uint64_t>(i) * 16;
        const float v = static_cast<float>(i) * speed_step;
        t.tick(t_ms, v);
    }
    // 理论: 0→100 线性 60s, 面积 = 50/3600 * 100 = 0.833 km
    // (50 是 (0+100)/2 平均速度 km/h, 100/3600 是 60s 换 h)
    // 实际 = 0.5 * 100 * 60 / 3600 = 0.833 km
    assert(t.tripDistanceKm() > 0.82f && t.tripDistanceKm() < 0.85f);
    printf("  ✓ 0→100 线性 60s: distance=%.3fkm (期望 ~0.833km)\n",
           t.tripDistanceKm());
}

static void test_reset() {
    printf("\n[测试7] reset() 清零\n");
    TripComputer t;
    t.tick(0, 60.0f);
    t.tick(60000, 60.0f);  // 1 min at 60 km/h = 1 km
    assert(t.tripDistanceKm() > 0.9f);
    t.reset();
    assert(t.tripDistanceKm() == 0.0f);
    assert(t.tripDurationS() == 0);
    assert(t.tripAvgSpeedKmh() == 0.0f);
    assert(!t.isMoving());
    printf("  ✓ reset 后全部清零\n");
}

static void test_time_jump_clamped() {
    printf("\n[测试8] 时间跳变 (>10s) 被 cap, 防止距离暴增\n");
    TripComputer t;
    t.tick(0, 60.0f);  // baseline
    t.tick(16, 60.0f); // 16ms 后, baseline 后第一次正常 tick
    // 模拟挂起 1 小时后唤醒
    t.tick(3600 * 1000, 60.0f);
    // dt 应被 cap 到 10s, distance 应只增加 10s × 60 km/h = 0.167 km
    // 加上 16ms tick 的 16ms × 60 = 0.000267 km ≈ 1.5km 总 (实际应只有 ~0.17km)
    assert(t.tripDistanceKm() < 0.2f);
    assert(t.tripDistanceKm() > 0.15f);
    printf("  ✓ 1h 跳变被 cap 到 10s, distance=%.3fkm (期望 ~0.167km)\n",
           t.tripDistanceKm());
}

// ─── 集成测试: 模拟 ShmDataSource 16ms tick 节奏 ───
// 这些测试不直接调 ShmDataSource (那需要 Qt), 但用相同节奏 (16ms/tick)
// 调 TripComputer, 验证 L2 类在真实使用模式下的行为.
static void test_city_driving_pattern() {
    printf("\n[测试9] 城市驾驶模式: 启停启停 60s\n");
    TripComputer t;
    t.tick(0, 0.0f);  // baseline
    // 60s 内: 起步 (5s) → 巡航 40km/h (20s) → 红灯 (10s) → 起步 → 巡航 (15s) → 停车
    const int TICKS = 3750;  // 60s / 16ms
    for (int i = 1; i <= TICKS; i++) {
        const uint64_t t_ms = static_cast<uint64_t>(i) * 16;
        float speed = 0.0f;
        if (t_ms < 5000) speed = 40.0f * (static_cast<float>(t_ms) / 5000.0f);  // 0→40 加速
        else if (t_ms < 25000) speed = 40.0f;                                   // 巡航
        else if (t_ms < 35000) speed = 40.0f * (1.0f - static_cast<float>(t_ms - 25000) / 10000.0f);  // 40→0
        else if (t_ms < 45000) speed = 0.0f;                                    // 停车
        else if (t_ms < 50000) speed = 30.0f * (static_cast<float>(t_ms - 45000) / 5000.0f);  // 起步 0→30
        else speed = 30.0f;                                                     // 短巡航
        t.tick(t_ms, speed);
    }
    // 估算: 加速段 (0→40, 5s) ≈ 0.028 km
    //       巡航 40kmh × 20s ≈ 0.222 km
    //       减速 40→0 (10s) ≈ 0.056 km
    //       停车不计
    //       加速 0→30 (5s) ≈ 0.021 km
    //       巡航 30kmh × 10s ≈ 0.083 km
    // 合计 ≈ 0.41 km
    assert(t.tripDistanceKm() > 0.35f && t.tripDistanceKm() < 0.5f);
    // 行驶时长 ≈ 55s (停 10s 不计)
    assert(t.tripDurationS() >= 50 && t.tripDurationS() <= 60);
    printf("  ✓ 60s 城市驾驶: distance=%.3fkm, duration=%us, avg=%.1fkmh\n",
           t.tripDistanceKm(), t.tripDurationS(), t.tripAvgSpeedKmh());
}

static void test_highway_steady_5min() {
    printf("\n[测试10] 高速 100 km/h 匀速 5 分钟 = 8.333 km\n");
    TripComputer t;
    t.tick(0, 100.0f);
    // 5min = 300s = 18750 ticks × 16ms
    const int TICKS = 18750;
    for (int i = 1; i <= TICKS; i++) {
        t.tick(static_cast<uint64_t>(i) * 16, 100.0f);
    }
    // 100 km/h × 300s = 100 × 300/3600 = 8.333 km
    assert(t.tripDistanceKm() > 8.32f && t.tripDistanceKm() < 8.34f);
    assert(t.tripDurationS() == 300);
    assert(t.tripAvgSpeedKmh() > 99.9f && t.tripAvgSpeedKmh() < 100.1f);
    printf("  ✓ 100kmh × 5min: distance=%.3fkm, duration=%us, avg=%.1fkmh\n",
           t.tripDistanceKm(), t.tripDurationS(), t.tripAvgSpeedKmh());
}

static void test_reset_mid_trip() {
    printf("\n[测试11] reset() 中途清零, 后续正常累计\n");
    TripComputer t;
    t.tick(0, 60.0f);
    // 跑 30s at 60 kmh = 0.5km
    for (int i = 1; i <= 1875; i++) {
        t.tick(static_cast<uint64_t>(i) * 16, 60.0f);
    }
    assert(t.tripDistanceKm() > 0.49f && t.tripDistanceKm() < 0.51f);
    // reset
    t.reset();
    assert(t.tripDistanceKm() == 0.0f);
    assert(t.tripDurationS() == 0);
    // 再跑 30s, 应该正常累计 (不应把 0.5km 之前的 dt 算进来)
    for (int i = 1; i <= 1875; i++) {
        const uint64_t new_t_ms = 30 * 1000 + static_cast<uint64_t>(i) * 16;
        t.tick(new_t_ms, 60.0f);
    }
    assert(t.tripDistanceKm() > 0.49f && t.tripDistanceKm() < 0.51f);
    printf("  ✓ reset 后再跑 30s, distance=%.3fkm (重置前的 0.5km 不残留)\n",
           t.tripDistanceKm());
}

int main() {
    printf("=== TripComputer 单元测试 ===\n");
    test_initial_state();
    test_first_tick_only_records_baseline();
    test_constant_speed_60kmh_for_1h();
    test_stop_doesnt_accumulate();
    test_below_threshold_creep_doesnt_count();
    test_trapezoid_integration();
    test_reset();
    test_time_jump_clamped();
    test_city_driving_pattern();
    test_highway_steady_5min();
    test_reset_mid_trip();
    printf("\n所有测试通过。\n");
    return 0;
}

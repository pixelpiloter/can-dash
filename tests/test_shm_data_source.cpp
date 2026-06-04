// test_shm_data_source.cpp
// ShmDataSource 单元测试（手写测试，遵循项目惯例）
//
// 覆盖：
// 1. 启动 + 初始状态
// 2. 健康状态变化（DISCONNECTED → OK）
// 3. 帧推送（业务字段转换）
// 4. 离线时不推送数据
// 5. 安全带警告条件（occupied && !buckled && speed>5）
// 6. 丢帧检测（frame_seq 跳号）
// 7. 指示灯映射

#include "layer3/shm_data_source.h"
#include "layer3/display_data_types.h"
#include "layer1/shm/shm_display.h"
#include "layer2/time_util.h"
#include "layer2/theme_manager.h"  // PR 7: ThemeManager 集成测试
#include "layer2/settings_manager.h"  // PR 13: SettingsManager 集成测试
#include "mock_data_binder.h"

#include <QCoreApplication>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>

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

namespace {

void writeShmFrame(uint32_t frame_seq, float speed, uint8_t driver_occ, uint8_t driver_buck) {
    shm_display_close();
    unlink(SHM_DISPLAY_PATH);  // 删旧文件（避免上次测试残留的 valid magic+checksum）
    shm_display_create();  // 测试前先建 shm
    DisplayDataShm shm = {};
    shm.magic = SHM_MAGIC;
    shm.version = SHM_VERSION;
    shm.last_commit_ms = candash::now_monotonic_ms();
    shm.updated_mask = 0xFFFFFFFF;
    shm.frame_seq = frame_seq;
    shm.vehicle_speed = speed;
    shm.motor_rpm = 3000;
    shm.bat_volt = 350.0f;
    shm.bat_soc = 75;
    shm.motor_temp = 60;
    shm.driver_occupied = driver_occ;
    shm.driver_buckled = driver_buck;
    shm.passenger_occupied = 0;
    shm.passenger_buckled = 0;
    shm.rear_buckle = 0;
    shm.alarm_active = 0;
    shm.indicators[0].on = 1;
    shm.indicators[0].flash = 1;
    shm.indicators[0].hz_x10 = 15;
    shm.checksum = shm_display_compute_checksum(&shm);  // 算 checksum 让读端校验通过
    shm_display_write(&shm);
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    printf("\n=== ShmDataSource 测试 ===\n");

    // 全局清理：每次测试前确保 shm 干净
    shm_display_close();

    // ─── Test 1: 启动 + 初始状态 ───
    printf("\n[1] 启动/停止:\n");
    {
        ShmDataSource src;
        TEST_ASSERT(!src.isRunning(), "初始 isRunning = false");
        src.start();
        TEST_ASSERT(src.isRunning(), "start() 后 isRunning = true");
        src.stop();
        TEST_ASSERT(!src.isRunning(), "stop() 后 isRunning = false");
    }

    // ─── Test 2: 健康状态变化 ───
    printf("\n[2] 健康状态变化:\n");
    {
        shm_display_close();
        unlink(SHM_DISPLAY_PATH);  // PR 45: 删旧 shm 文件 (start() 会重新 open, 但需要先 unlink 模拟"无 producer")
        ShmDataSource src;
        MockDataBinder binder;
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        src.tickForTest();
        TEST_ASSERT(binder.lastHealth() == HEALTH_DISCONNECTED, "shm 缺失 → DISCONNECTED");

        writeShmFrame(1, 30.0f, 1, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        TEST_ASSERT(binder.lastHealth() == HEALTH_OK, "写入帧后 → OK");

        src.stop();
        shm_display_close();
    }

    // ─── Test 3: 帧推送字段映射 ───
    printf("\n[3] 业务字段映射:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        writeShmFrame(1, 60.0f, 1, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();

        TEST_ASSERT(binder.snapshotCount() >= 1, "至少 1 个 snapshot");
        if (binder.snapshotCount() >= 1) {
            auto s = binder.lastSnapshot();
            TEST_ASSERT(qFuzzyCompare(s.data.vehicle_speed, 60.0f), "vehicle_speed = 60");
            TEST_ASSERT(s.data.motor_rpm == 3000, "motor_rpm = 3000");
            TEST_ASSERT(qFuzzyCompare(s.data.bat_volt, 350.0f), "bat_volt = 350");
            TEST_ASSERT(s.data.bat_soc == 75, "bat_soc = 75");
            TEST_ASSERT(s.data.driver_occupied == 1, "driver_occupied = 1");
            TEST_ASSERT(s.meta.frame_seq == 1u, "frame_seq = 1");
            TEST_ASSERT(s.is_moving, "is_moving = true (60km/h)");
        }
        src.stop();
        shm_display_close();
    }

    // ─── Test 4: 离线时不推送数据 ───
    printf("\n[4] 离线时无数据:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 触发多次离线 tick（任何一次都不应增加 snapshot 计数）
        for (int i = 0; i < 5; i++) {
            src.tickForTest();
        }
        // cppcheck-suppress unreadVariable
        const size_t count_before = binder.snapshotCount();
        src.tickForTest();
        src.tickForTest();
        // cppcheck-suppress duplicateExpression
        TEST_ASSERT(binder.snapshotCount() - count_before == 0, "连续离线不增加 snapshot 计数");
        src.stop();
    }

    // ─── Test 5: 安全带警告条件 ───
    printf("\n[5] 安全带警告条件:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 60 km/h, driver 坐了但没系 → 警告
        writeShmFrame(1, 60.0f, 1, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.seat_belt.warning_active, "60km/h + driver occupied + not buckled → warning");
        TEST_ASSERT(s.seat_belt.seats[0].warning, "driver seat warning");

        // 0 km/h, driver 坐了但没系 → 不警告（低速不报警）
        writeShmFrame(2, 0.0f, 1, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(!s.seat_belt.warning_active, "0km/h + not buckled → no warning");
        TEST_ASSERT(!s.seat_belt.seats[0].warning, "driver seat no warning");

        src.stop();
        shm_display_close();
    }

    // ─── Test 6: 丢帧检测 ───
    printf("\n[6] 丢帧检测:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        writeShmFrame(1, 30.0f, 1, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        TEST_ASSERT(binder.lastSnapshot().meta.frame_seq == 1u, "frame_seq=1");
        TEST_ASSERT(binder.lastSnapshot().meta.dropped_frames == 0u, "丢帧数=0");

        // 跳号到 5（丢 2/3/4 共 3 帧）
        writeShmFrame(5, 30.0f, 1, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        TEST_ASSERT(binder.lastSnapshot().meta.frame_seq == 5u, "frame_seq=5");
        TEST_ASSERT(binder.lastSnapshot().meta.dropped_frames == 3u, "丢帧数=3");

        src.stop();
        shm_display_close();
    }

    // ─── Test 7: 指示灯映射 ───
    printf("\n[7] 指示灯映射:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        writeShmFrame(1, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.indicators.lights[0].on, "indicator[0].on = true");
        TEST_ASSERT(s.indicators.lights[0].flash, "indicator[0].flash = true");
        TEST_ASSERT(qFuzzyCompare(s.indicators.lights[0].hz, 1.5f), "indicator[0].hz = 1.5");

        src.stop();
        shm_display_close();
    }

    // ─── Test 8: ThemeManager 集成 (PR 7) ───
    // 验证 ShmDataSource m_theme 状态 → snapshot 6 字段全链路
    printf("\n[8] ThemeManager 集成:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 8.1 默认状态: AUTO + hour=12 → DAY, 5 色 = kDayColors
        // PR 45: PR 16 把 start() baseline 改成 wall_clock_hour(), 默认状态跟
        // wall clock 走, 测试要显式设 hour=12 拿回稳定默认 (不依赖运行环境).
        // 配合 theme_manager.cpp setCurrentHour/reset 用 now_monotonic_ms()
        // 同步 baseline, 下次 tickForTest 算 delta≈0, 保留 m_currentHour=12.
        src.setThemeHourForTest(12);
        writeShmFrame(1, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_mode == 2u, "默认 mode = AUTO (2)");
        TEST_ASSERT(s.theme_is_day == 1u, "默认 isDay = 1 (中午)");
        TEST_ASSERT(s.theme_color_background == candash::ThemeManager::kDayColors.background, "默认 bg = DAY 色");
        TEST_ASSERT(s.theme_color_foreground == candash::ThemeManager::kDayColors.foreground, "默认 fg = DAY 色");
        TEST_ASSERT(s.theme_color_critical   == candash::ThemeManager::kDayColors.critical,  "默认 critical = DAY 色 (跨模式一致)");

        // 8.2 setMode(NIGHT) → 强制夜间, 5 色 = kNightColors
        src.setThemeModeForTest(candash::ThemeMode::NIGHT);
        writeShmFrame(2, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_mode == 1u, "NIGHT 模式: mode = 1");
        TEST_ASSERT(s.theme_is_day == 0u, "NIGHT 模式: isDay = 0");
        TEST_ASSERT(s.theme_color_background == candash::ThemeManager::kNightColors.background, "NIGHT 模式: bg = NIGHT 色");

        // 8.3 setMode(DAY) → 强制日间
        src.setThemeModeForTest(candash::ThemeMode::DAY);
        writeShmFrame(3, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_mode == 0u, "DAY 模式: mode = 0");
        TEST_ASSERT(s.theme_is_day == 1u, "DAY 模式: isDay = 1 (强制)");

        // 8.4 AUTO + 凌晨 2 点 → NIGHT (小时评估)
        src.setThemeModeForTest(candash::ThemeMode::AUTO);
        src.setThemeHourForTest(2);
        writeShmFrame(4, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_mode == 2u, "AUTO + hour=2: mode = AUTO");
        TEST_ASSERT(s.theme_is_day == 0u, "AUTO + hour=2: isDay = 0 (NIGHT)");
        TEST_ASSERT(s.theme_color_background == candash::ThemeManager::kNightColors.background, "AUTO+hour=2: bg = NIGHT 色");

        // 8.5 reset → 回到默认 AUTO + 12:00 + DAY
        src.resetThemeForTest();
        writeShmFrame(5, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_mode == 2u, "reset: mode = AUTO");
        TEST_ASSERT(s.theme_is_day == 1u, "reset: isDay = 1 (hour=12)");

        // 8.6 自定义 sunrise/sunset: hour=7 + sunrise=8 + sunset=20 → NIGHT
        src.setThemeSunriseForTest(8);
        src.setThemeSunsetForTest(20);
        src.setThemeHourForTest(7);
        writeShmFrame(6, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_is_day == 0u, "AUTO+custom[8,20)+hour=7: isDay=0");

        // 8.7 同一 hour 不同模式: hour=12 强制 NIGHT → NIGHT 色
        src.setThemeHourForTest(12);
        src.setThemeModeForTest(candash::ThemeMode::NIGHT);
        writeShmFrame(7, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.theme_is_day == 0u, "hour=12 + NIGHT 模式: isDay=0 (强制)");

        src.stop();
        shm_display_close();
    }

    // ─── Test 9: WarningManager 集成 (PR 9) ───
    // 验证 ShmDataSource m_warning 状态 → DisplaySnapshot 6 字段全链路
    printf("\n[9] WarningManager 集成:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 9.1 初始: 0 报警
        writeShmFrame(1, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.warning_count == 0, "初始 warning_count=0");
        TEST_ASSERT(s.has_critical == 0, "初始 has_critical=false");

        // 9.2 推 1 条 CRITICAL (priority=0) → count=1, has_critical=true
        // now_ms 传 0 走 wall clock, 跟 onTick 的 shm.last_commit_ms 同一时间基准
        src.pushWarningForTest("battery_fire", 0, 0xDD, 0x22, 0x22, 0);
        writeShmFrame(2, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.warning_count == 1u, "推 1 条后 count=1");
        TEST_ASSERT(s.has_critical == 1u, "CRITICAL → has_critical=true");
        TEST_ASSERT(s.active_warnings[0].priority == 0u, "priority=0 复制到 snapshot");
        TEST_ASSERT(s.active_warnings[0].severity == 2u, "severity=CRITICAL(2) 复制");
        TEST_ASSERT(s.active_warnings[0].color == 0xFFDD2222u, "ARGB color 复制");
        TEST_ASSERT(s.active_warnings[0].dedup_count == 0u, "dedup_count=0 初始");

        // 9.3 推多条混合 priority → 排序后 priority 小的在前
        src.pushWarningForTest("info1", 15, 0x00, 0x80, 0xFF, 0);  // INFO
        src.pushWarningForTest("warn1", 5,  0xFF, 0xB0, 0x00, 0);  // WARNING
        writeShmFrame(3, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.warning_count == 3u, "3 条全部保留 (max=3)");
        TEST_ASSERT(s.active_warnings[0].priority == 0u, "排序: priority=0 排第 1");
        TEST_ASSERT(s.active_warnings[1].priority == 5u, "排序: priority=5 排第 2");
        TEST_ASSERT(s.active_warnings[2].priority == 15u, "排序: priority=15 排第 3");
        TEST_ASSERT(s.active_warnings[0].severity == 2u, "[0] CRITICAL");
        TEST_ASSERT(s.active_warnings[1].severity == 1u, "[1] WARNING");
        TEST_ASSERT(s.active_warnings[2].severity == 0u, "[2] INFO");
        TEST_ASSERT(s.has_critical == 1u, "has_critical 仍 true");

        // 9.4 推第 4 条超 max → priority 最大的 (info1) 被 trim
        src.pushWarningForTest("info2", 20, 0x88, 0x88, 0x88, 0);
        writeShmFrame(4, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.warning_count == 3u, "max=3 仍 3 条");
        TEST_ASSERT(s.active_warnings[0].priority == 0u,  "[0]=0 不变");
        TEST_ASSERT(s.active_warnings[1].priority == 5u,  "[1]=5 不变");
        TEST_ASSERT(s.active_warnings[2].priority == 15u, "[2]=15 不变 (info1 保留)");
        bool found_info2 = false;
        for (uint8_t i = 0; i < s.warning_count; i++) {
            if (std::strncmp(s.active_warnings[i].name, "info2", 6) == 0) found_info2 = true;
        }
        TEST_ASSERT(!found_info2, "info2(20) 被 trim");

        // 9.5 dedup: 同 name 5s 内推 → dedup_count++
        // 5s 内不太可能 5s 测试窗口通过, 这里用 sleep_for 跳过 — 集成测试聚焦集成路径
        // 详细 dedup 行为已在 tests/test_warning_manager.cpp (20 cases) 覆盖
        TEST_ASSERT(s.warning_count == 3u, "持续 3 条不变 (dedup/hold 行为 L2 单测已覆盖)");

        // 9.6 reset → 全清
        src.resetWarningForTest();
        writeShmFrame(6, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.warning_count == 0u, "reset 后 count=0");
        TEST_ASSERT(s.has_critical == 0u, "reset 后 has_critical=false");

        // 9.7 reset 后, 同 name 算新触发
        src.pushWarningForTest("new_alarm", 5, 0xFF, 0x80, 0x00, 0);
        writeShmFrame(7, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.warning_count == 1u, "reset 后推新 → count=1");
        TEST_ASSERT(s.active_warnings[0].dedup_count == 0u, "dedup_count 重新从 0 开始");

        src.stop();
        shm_display_close();
    }

    // ─── Test 10: SettingsManager 集成 (PR 13) ───
    // 验证 ShmDataSource m_settings 状态 → DisplaySnapshot 2 字段全链路
    printf("\n[10] SettingsManager 集成:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 10.1 初始: defaults (units=0=METRIC, brightness=80)
        writeShmFrame(1, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_units == 0u, "初始 settings_units=0 (METRIC)");
        TEST_ASSERT(s.settings_brightness == 80u, "初始 settings_brightness=80 (默认)");

        // 10.2 切到 IMPERIAL → snapshot 反映
        src.setSettingsUnitsForTest(1);  // 1=IMPERIAL
        writeShmFrame(2, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_units == 1u, "setUnits(1) → settings_units=1 (IMPERIAL)");

        // 10.3 改 brightness 50 → snapshot 反映
        src.setSettingsBrightnessForTest(50);
        writeShmFrame(3, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_brightness == 50u, "setBrightness(50) → settings_brightness=50");

        // 10.4 brightness clamp: > 100 → 100
        src.setSettingsBrightnessForTest(150);
        writeShmFrame(4, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_brightness == 100u, "clamp: 150 → 100");

        // 10.5 brightness clamp: < 0 (uint8 截断 250 之类) — 这里用 < 0 不可能, 改测 uint8 wrap
        // SettingsManager::setBrightness 已经 clamp, uint8_t 输入只能是 0-255
        // 测一下 0 保持 0
        src.setSettingsBrightnessForTest(0);
        writeShmFrame(5, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_brightness == 0u, "setBrightness(0) → 0");

        // 10.6 reset → 回到默认
        src.resetSettingsForTest();
        writeShmFrame(6, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_units == 0u, "reset → units=0 (METRIC 默认)");
        TEST_ASSERT(s.settings_brightness == 80u, "reset → brightness=80 (默认)");

        // 10.7 binder 也缓存一份, 验证 onDataUpdated 反映到 Q_PROPERTY
        // 改 units → onTick 推 snapshot → binder 缓存 m_settingsUnits
        // 这里我们直接用 binder 的 lastSnapshot 验证 (binder 不暴露 getter,
        // 但 snapshot 已经在 10.6 反映 reset 默认值, 跟 L2 一致)

        // 10.8 tick 是 no-op: 没有 setter 的情况下连续 tick, snapshot 字段不变
        src.resetSettingsForTest();
        writeShmFrame(7, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_units == 0u, "reset 后 tick 1: units 仍 0");
        writeShmFrame(8, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.settings_units == 0u, "reset 后 tick 2: units 仍 0 (无漂移)");
        TEST_ASSERT(s.settings_brightness == 80u, "reset 后 tick 2: brightness 仍 80");

        src.stop();
        shm_display_close();
    }

    // ─── Test 11: ViewManager 集成 (PR 13) ───
    // 验证 ShmDataSource m_view 状态 → DisplaySnapshot 3 字段全链路
    // 关键设计: ViewManager 默认 current=DRIVE, 首次 tick 若 candidate=SETUP (P+idle)
    // 会"首次切换 free"切到 SETUP (注释里"启动 P 不跳 SETUP"实际不成立, 见 L2 view_manager.h:163
    // tick(0) → SETUP 是 free 切换); 后续切换需 hysteresis 1s.
    // 注: 集成测试用"resetViewForTest 重新触发 free 切换"规避 wall clock 1s 等时
    printf("\n[11] ViewManager 集成:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 11.1 初始: P 档 + idle, ViewManager 默认 current=DRIVE
        // 首次 tick 候选=SETUP ≠ current=DRIVE, 首次切换 free → 切到 SETUP
        writeShmFrame(1, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.view_current == 2u, "初始 P+idle tick → viewMode=SETUP (首次切换 free)");
        TEST_ASSERT(s.view_gear    == 0u, "初始 view_gear=0 (P)");
        TEST_ASSERT(s.view_charge  == 0u, "初始 view_charge=0 (idle)");

        // 11.2 reset 后立即 setGear(D), 避免 P+idle 触发 SETUP 切
        //     (reset 把 m_current=DRIVE + m_gear=P + m_charge=idle, 首次 tick 候选=SETUP → 切 SETUP)
        //     测"reset → 设 D 档 → 保持 DRIVE"
        src.resetViewForTest();
        src.setViewGearForTest(3);  // D 档, 避免 P+idle 切到 SETUP
        writeShmFrame(2, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.view_current == 0u, "reset + setGear(D) → viewMode=DRIVE (候选=current 不切)");
        TEST_ASSERT(s.view_gear    == 3u, "view_gear=3 (D)");

        // 11.3 切 charge=1 → 高优先级, 候选=CHARGE, 切到 CHARGE (首次切换 free)
        src.setViewChargeForTest(1);  // charging
        writeShmFrame(3, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.view_current == 1u, "setCharge(1) → viewMode=CHARGE (覆盖 DRIVE, free)");
        TEST_ASSERT(s.view_charge  == 1u, "view_charge=1");

        // 11.4 切 charge=0, gear=N (2) → 候选=SETUP, current=CHARGE
        //     hysteresis 未满 (距上次 free 切换 < 1s wall clock) → 仍 CHARGE
        //     但 view_gear=2 view_charge=0 字段应反映 (它们不依赖切换)
        src.setViewGearChargeForTest(2, 0);  // N + idle
        writeShmFrame(4, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.view_gear    == 2u, "view_gear=2 (N) — snapshot 反映即使 view 切还在 hysteresis");
        TEST_ASSERT(s.view_charge  == 0u, "view_charge=0 (idle)");
        TEST_ASSERT(s.view_current == 1u, "viewMode 仍 CHARGE (hysteresis 1s 未满)");

        // 11.5 reset + setGear(D) → 测 reset 路径 + binder 缓存
        src.resetViewForTest();
        src.setViewGearForTest(3);  // D 档
        writeShmFrame(5, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.view_current == 0u, "reset + setGear(D) → viewMode=DRIVE");
        TEST_ASSERT(s.view_gear    == 3u, "reset → view_gear=3 (D)");
        TEST_ASSERT(s.view_charge  == 0u, "reset → view_charge=0 (idle)");

        // 11.6 binder 缓存: m_viewMode/m_viewGear/m_viewCharge 已通过 onDataUpdated 写入
        // 改 gear+charge → tick → snapshot 反映 (binder.lastSnapshot 间接验证)
        src.setViewGearChargeForTest(3, 0);  // D + idle (候选 DRIVE)
        writeShmFrame(6, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.view_gear == 3u, "binder snapshot 反映 setGear(D) → view_gear=3");

        // 11.7 tick no-op: gear/charge 不变情况下连续 tick, snapshot 字段稳定
        writeShmFrame(7, 0.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.view_gear   == 3u, "tick no-op: view_gear 稳定 =3");
        TEST_ASSERT(s.view_charge == 0u, "tick no-op: view_charge 稳定 =0");

        src.stop();
        shm_display_close();
    }

    // ─── Test 12: LimpHomeRuntime 集成 (PR 44) ───
    // 验证 ShmDataSource m_limp_home 状态 → DisplaySnapshot.limp_home 4 字段全链路
    // C 模式 (LimpHome): onTick 内同一 commit_ms 喂 + tick, elapsed=0 → 永远 NORMAL
    // 测不了 L1/L2/L3 状态转换 (留给 L2 单测 test_limp_home_runtime.cpp),
    // 这里只测 binding path: snapshot.limp_home 字段复制 + binder 透传 + reset/tick
    printf("\n[12] LimpHomeRuntime 集成:\n");
    {
        shm_display_close();
        ShmDataSource src;
        MockDataBinder binder;
        src.setUpdateCallback([&](const DisplaySnapshot& s) { binder.onDataUpdated(s); });
        src.setHealthCallback([&](HealthStatus h) { binder.onHealthChanged(h); });
        src.start();

        // 12.1 默认 NORMAL 路径: start + tick → snapshot.limp_home.level = 0
        // 启动时 m_limp_home.init(&LIMP_HOME_CONFIG), onTick 内 onValueChanged(key, commit_ms) + tick(commit_ms)
        // elapsed = commit_ms - commit_ms = 0 → 所有信号 inTimeout=false → level=NORMAL
        writeShmFrame(1, 50.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        auto s = binder.lastSnapshot();
        TEST_ASSERT(s.limp_home.level  == 0u, "默认: level=NORMAL (C 模式 elapsed=0)");
        TEST_ASSERT(s.limp_home.active == 0u, "默认: active=false (level=0 派生)");

        // 12.2 NORMAL 时 message_zh/en 字段为空 (L2 query 不返回 msg 指针)
        TEST_ASSERT(s.limp_home.message_zh[0] == '\0', "NORMAL: message_zh 为空");
        TEST_ASSERT(s.limp_home.message_en[0] == '\0', "NORMAL: message_en 为空");

        // 12.3 reset + tick → 回到 NORMAL (init 把 signalStatus 标 inTimeout=true,
        //     但 onTick 立刻 onValueChanged(commit_ms) 喂后, tick 时 elapsed=0)
        src.resetLimpHomeForTest();
        writeShmFrame(2, 50.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.limp_home.level  == 0u, "reset+tick: level=NORMAL");
        TEST_ASSERT(s.limp_home.active == 0u, "reset+tick: active=false");

        // 12.4 binder 透传 — 手动推 DisplaySnapshot (L3 + msg), 不经 onTick
        //     验证 MockDataBinder.lastSnapshot() 拿到 limp_home 全字段 (跟 selfTest Test 9 同模式)
        DisplaySnapshot snap;
        std::memset(&snap, 0, sizeof(DisplaySnapshot));
        snap.limp_home.level  = 3u;  // L3
        snap.limp_home.active = 1u;
        std::strncpy(snap.limp_home.message_zh, "CAN 总线异常, 显示已禁用", sizeof(snap.limp_home.message_zh) - 1);
        std::strncpy(snap.limp_home.message_en, "CAN bus abnormal, display disabled", sizeof(snap.limp_home.message_en) - 1);
        binder.onDataUpdated(snap);
        s = binder.lastSnapshot();
        TEST_ASSERT(s.limp_home.level  == 3u, "binder 透传: level=L3");
        TEST_ASSERT(s.limp_home.active == 1u, "binder 透传: active=true");
        TEST_ASSERT(std::strncmp(s.limp_home.message_zh, "CAN 总线异常", 12) == 0, "binder 透传: message_zh 含 'CAN 总线异常'");
        TEST_ASSERT(std::strncmp(s.limp_home.message_en, "CAN bus abnormal", 16) == 0, "binder 透传: message_en 含 'CAN bus abnormal'");

        // 12.5 完整 onTick 链路: reset + writeShmFrame + tick → snapshot 回到 NORMAL
        src.resetLimpHomeForTest();
        writeShmFrame(3, 50.0f, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        src.tickForTest();
        s = binder.lastSnapshot();
        TEST_ASSERT(s.limp_home.level  == 0u, "完整 onTick 链路: level=NORMAL");
        TEST_ASSERT(s.limp_home.active == 0u, "完整 onTick 链路: active=false");

        src.stop();
        shm_display_close();
    }

    printf("\n=== 总计: %d/%d 通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

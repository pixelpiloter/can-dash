// test_dashboard_backend.cpp
// DashboardBackend 胶水层测试（手写测试，遵循项目惯例）
//
// 覆盖：
// 1. 默认 init() 启动 ShmDataSource + QtDataBinder
// 2. 注入自定义 MockDataSource
// 3. DataSource 推送时 Binder 收到
// 4. 健康状态透传
// 5. QML 透传接口

#include "layer3/dashboard_backend.h"
#include "layer3/display_data_types.h"
#include "layer3/shm_data_source.h"
#include "layer3/qt_data_binder.h"
#include "mock_data_source.h"

#include <QCoreApplication>
#include <cstdio>
#include <cstring>

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

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    printf("\n=== DashboardBackend 胶水层测试 ===\n");

    // ─── Test 1: 默认 init() 不抛异常 ───
    printf("\n[1] 默认 init:\n");
    {
        DashboardBackend backend;
        backend.init();
        TEST_ASSERT(true, "init() 不抛异常");
    }

    // ─── Test 2: 注入 MockDataSource + 验证转发 ───
    printf("\n[2] MockSource 转发:\n");
    {
        DashboardBackend backend;
        auto source = std::make_unique<MockDataSource>();
        auto* raw_source = source.get();
        auto binder = std::make_unique<QtDataBinder>();
        backend.setDataBinder(std::move(binder));
        backend.setDataSource(std::move(source));

        DisplaySnapshot s;
        s.data.vehicle_speed = 88.0f;
        s.data.bat_volt = 400.0f;
        s.health = HEALTH_OK;
        s.meta.frame_seq = 100;
        raw_source->pushSnapshot(s);

        auto dd = backend.displayData();
        TEST_ASSERT(qFuzzyCompare(dd["vehicle_speed"].toFloat(), 88.0f), "vehicle_speed 透传 = 88");
        TEST_ASSERT(qFuzzyCompare(dd["bat_volt"].toFloat(), 400.0f), "bat_volt 透传 = 400");
        TEST_ASSERT(backend.frameSeq() == 100u, "frameSeq 透传 = 100");
    }

    // ─── Test 3: 健康状态透传 ───
    printf("\n[3] 健康状态透传:\n");
    {
        DashboardBackend backend;
        auto source = std::make_unique<MockDataSource>();
        auto* raw_source = source.get();
        auto binder = std::make_unique<QtDataBinder>();
        backend.setDataBinder(std::move(binder));
        backend.setDataSource(std::move(source));

        raw_source->pushHealth(HEALTH_OK);
        TEST_ASSERT(backend.processorOnline(), "HEALTH_OK → processorOnline");
        TEST_ASSERT(backend.processorStatus() == "ok", "HEALTH_OK → status='ok'");

        raw_source->pushHealth(HEALTH_STALE);
        TEST_ASSERT(!backend.processorOnline(), "HEALTH_STALE → processorOnline=false");
        TEST_ASSERT(backend.processorStatus() == "stale", "HEALTH_STALE → status='stale'");
    }

    // ─── Test 4: QML 透传接口 ───
    printf("\n[4] QML 透传接口:\n");
    {
        DashboardBackend backend;
        auto source = std::make_unique<MockDataSource>();
        auto* raw_source = source.get();
        auto binder = std::make_unique<QtDataBinder>();
        backend.setDataBinder(std::move(binder));
        backend.setDataSource(std::move(source));

        // PR 49b: DisplaySnapshot s; 走 default-init, indicators 字段 (12 灯)
        // 残留栈垃圾 → binder buildIndicatorStates 收到 lights[i].on 随机 true/false
        // → indicatorOn('left_turn_light') 间歇返回 true, 测试 [4] 5/10 失败
        // 修法: 用 s{} 值初始化, 全部字段清零
        DisplaySnapshot s{};
        s.data.vehicle_speed = 50.0f;
        s.health = HEALTH_OK;
        raw_source->pushSnapshot(s);

        TEST_ASSERT(qFuzzyCompare(backend.get("vehicle_speed").toFloat(), 50.0f), "get('vehicle_speed') = 50");
        backend.set("test", 123);  // 不抛
        TEST_ASSERT(backend.tr("nonexistent_key") == "nonexistent_key", "tr('xxx') = 'xxx' (no lang)");
        backend.setLanguage("en_US");  // 不抛
        TEST_ASSERT(!backend.indicatorOn("left_turn_light"), "indicatorOn('left_turn_light') = false (无数据)");
    }

    // ─── Test 5: 默认 QtDataBinder 通过 setLanguage 切换语言 ───
    printf("\n[5] 多语言切换:\n");
    {
        DashboardBackend backend;
        auto source = std::make_unique<MockDataSource>();
        auto* raw_source = source.get();
        auto binder = std::make_unique<QtDataBinder>();
        binder->initLanguage(TRANSLATIONS, TRANSLATION_COUNT);
        backend.setDataBinder(std::move(binder));
        backend.setDataSource(std::move(source));

        backend.setLanguage("en_US");
        // 验证：setLanguage 不抛 + 不影响数据
        DisplaySnapshot s;
        s.data.vehicle_speed = 60.0f;
        raw_source->pushSnapshot(s);
        TEST_ASSERT(qFuzzyCompare(backend.displayData()["vehicle_speed"].toFloat(), 60.0f), "语言切换后数据仍正常");
    }

    printf("\n=== 总计: %d/%d 通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

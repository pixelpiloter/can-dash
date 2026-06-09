// test_rule_engine.cpp
// 规则引擎单元测试 (手写测试, 跟项目惯例一致)
//
// 覆盖:
// 1. yaml 加载 + 表达式编译
// 2. 表达式求值 + setter 调用
// 3. 丢失状态分支
// 4. 编译错误检测
// 5. 多 setter 联动

#include "layer3/rule_engine.h"

#include <QCoreApplication>
#include <QString>
#include <QFile>
#include <QTextStream>
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

namespace {

// 记录 setter 调用的 spy
struct SetterSpy {
    int trip_range_confidence_calls = 0;
    double last_trip_range = -1;
    int view_mode_calls = 0;
    int last_view = -1;
    int indicator_calls = 0;
    int last_ind_id = -1;
    bool last_ind_on = false;
    int alarm_calls = 0;
    QString last_alarm_name;
    int limp_home_calls = 0;
    int last_limp = -1;
};

// 写一份临时 yaml 用于测试
bool writeTempYaml(const QString& path, const QString& content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream out(&f);
    out << content;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    printf("=== ExprRuleEngine tests ===\n");

    // ─── 测试 1: 简单加载 + 编译 ───
    printf("\n[Test 1] Load simple rule\n");
    {
        const QString yaml = "/tmp/test_rule_simple.yaml";
        writeTempYaml(yaml, R"(
derived_metrics:
  - id: "t1"
    expr: |
      set_trip_range_confidence(50);
)");

        candash::ExprRuleEngine engine;
        engine.registerSetter("set_trip_range_confidence", 1,
            [](const std::vector<double>&) { /* spy */ });

        bool ok = engine.loadRules(yaml);
        TEST_ASSERT(ok, "yaml loads with one rule");
        TEST_ASSERT(engine.ruleCount() == 1, "engine has 1 rule");
    }

    // ─── 测试 2: 表达式求值触发 setter ───
    printf("\n[Test 2] Evaluate triggers setter\n");
    {
        const QString yaml = "/tmp/test_rule_eval.yaml";
        writeTempYaml(yaml, R"(
state_machines:
  - id: "t2"
    expr: |
      if (bat_soc < 10) {
        set_trip_range_confidence(20);
      } else {
        set_trip_range_confidence(100);
      }
)");

        SetterSpy spy;
        candash::ExprRuleEngine engine;
        engine.registerSetter("set_trip_range_confidence", 1,
            [&spy](const std::vector<double>& a) {
                spy.trip_range_confidence_calls++;
                spy.last_trip_range = a[0];
            });

        TEST_ASSERT(engine.loadRules(yaml), "load ok");

        // 场景 A: SOC 18 → 100
        QVariantMap ctx1 = { {"bat_soc", 18.0} };
        engine.evaluate(ctx1);
        TEST_ASSERT(spy.trip_range_confidence_calls == 1, "1 call after eval");
        TEST_ASSERT(spy.last_trip_range == 100.0, "SOC=18 → 100");

        // 场景 B: SOC 8 → 20
        QVariantMap ctx2 = { {"bat_soc", 8.0} };
        engine.evaluate(ctx2);
        TEST_ASSERT(spy.trip_range_confidence_calls == 2, "2 calls total");
        TEST_ASSERT(spy.last_trip_range == 20.0, "SOC=8 → 20");
    }

    // ─── 测试 3: 信号丢失分支 ───
    printf("\n[Test 3] Lost state branch\n");
    {
        const QString yaml = "/tmp/test_rule_lost.yaml";
        writeTempYaml(yaml, R"(
alarm_rules:
  - id: "t3"
    expr: |
      if (bat_volt_lost == 1) {
        set_trip_range_confidence(0);
      } else if (bat_volt < 280) {
        set_trip_range_confidence(10);
      } else {
        set_trip_range_confidence(99);
      }
)");

        SetterSpy spy;
        candash::ExprRuleEngine engine;
        engine.registerSetter("set_trip_range_confidence", 1,
            [&spy](const std::vector<double>& a) {
                spy.trip_range_confidence_calls++;
                spy.last_trip_range = a[0];
            });

        engine.loadRules(yaml);

        // 丢失
        engine.evaluate({{"bat_volt", 0.0}, {"bat_volt_lost", 1}});
        TEST_ASSERT(spy.last_trip_range == 0.0, "lost → 0");

        // 欠压
        engine.evaluate({{"bat_volt", 250.0}, {"bat_volt_lost", 0}});
        TEST_ASSERT(spy.last_trip_range == 10.0, "undervolt → 10");

        // 正常
        engine.evaluate({{"bat_volt", 360.0}, {"bat_volt_lost", 0}});
        TEST_ASSERT(spy.last_trip_range == 99.0, "normal → 99");
    }

    // ─── 测试 4: 编译错误检测 ───
    printf("\n[Test 4] Compile error caught\n");
    {
        const QString yaml = "/tmp/test_rule_bad.yaml";
        writeTempYaml(yaml, R"(
derived_metrics:
  - id: "t4"
    expr: |
      this is not a valid expression (((
)");

        candash::ExprRuleEngine engine;
        engine.registerSetter("set_trip_range_confidence", 1, [](auto&){});
        bool ok = engine.loadRules(yaml);
        TEST_ASSERT(!ok, "bad syntax → load fails");
    }

    // ─── 测试 5: 完整 yaml (PR-EXPR-2: derived_rules.yaml 当前只 1 条规则) ───
    printf("\n[Test 5] Full YAML with multiple setters\n");
    {
        SetterSpy spy;
        candash::ExprRuleEngine engine;
        engine.registerSetter("set_trip_range_confidence", 1,
            [&spy](const std::vector<double>& a) {
                spy.last_trip_range = a[0];
            });
        engine.registerSetter("set_view_mode", 1,
            [&spy](const std::vector<double>& a) {
                spy.last_view = (int)a[0];
            });
        engine.registerSetter("set_indicator", 4,
            [&spy](const std::vector<double>& a) {
                spy.last_ind_id = (int)a[0];
                spy.last_ind_on = a[1] != 0;
            });

        bool ok = engine.loadRules("../config/derived_rules.yaml");
        if (ok) {
            printf("  (loaded %d rules from project yaml)\n", engine.ruleCount());
            // PR-EXPR-2: yaml 当前只有 range_confidence 规则
            // 跑一帧 SOC 60% + 速度 0 → conf = 100
            engine.evaluate({
                {"bat_soc", 60.0}, {"bat_volt", 360.0},
                {"vehicle_speed", 0.0}, {"motor_temp", 50.0},
                {"precharge_status", 1}, {"idle_seconds", 0},
                {"bat_soc_lost", 0}, {"bat_volt_lost", 0}, {"vehicle_speed_lost", 0},
                {"driver_occupied", 1}, {"driver_buckled", 1},
                {"passenger_occupied", 0}, {"passenger_buckled", 1},
                {"health", 3}, {"view_current", 0}
            });
            TEST_ASSERT(spy.last_trip_range == 100.0, "SOC 60% → conf 100");
            // PR-EXPR-3 才加 set_view_mode / set_indicator, 当前 yaml 没有对应规则
        } else {
            printf("  (skipped — yaml not found, build from can-dash root)\n");
        }
    }

    printf("\n=== %d/%d passed ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

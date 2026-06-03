// test_vehicle_logic.cpp
// Layer 2 VehicleLogic 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include "../src/layer2/vehicle_logic.h"
#include "../src/layer2/event_bus.h"
#include "../src/generated/vehicle_config.h"  // v3 探针: kDefaultVehicleConfig

// 验证 EventBus 发布的事件
static std::vector<std::string> published_keys;
static void clear_events() { published_keys.clear(); }

int main() {
    printf("=== VehicleLogic 单元测试 ===\n");

    VehicleConfigDef config;
    config.soc_warning_low = 15.0f;
    config.soc_critical_low = 5.0f;
    config.speed_max = 260.0f;
    config.precharge_timeout_ms = 3000;
    config.soc_smoothing_window = 5;

    VehicleLogic logic;
    logic.init(&config);

    // ─── 测试1：初始状态 ───
    printf("\n[测试1] 初始状态\n");
    assert(logic.getSpeed() == 0.0f);
    assert(!logic.isSpeedValid());
    assert(!logic.isReadyGo());
    assert(!logic.isHvActive());
    assert(logic.getPrechargeState() == PRECHARGE_IDLE);
    printf("  ✓ 初始值正确\n");

    // ─── 测试2：车速更新 ───
    printf("\n[测试2] 车速更新\n");
    logic.onSpeedUpdate(50.0f, true);
    assert(logic.getSpeed() == 50.0f);
    assert(logic.isSpeedValid());
    logic.onSpeedUpdate(0.0f, true);
    assert(logic.getSpeed() == 0.0f);
    printf("  ✓ 车速更新正确\n");

    // ─── 测试3：SOC 平滑 ───
    printf("\n[测试3] SOC 平滑\n");
    // 前5次更新填充窗口
    for (int i = 0; i < 5; i++) {
        logic.onSocUpdate(50.0f + i);  // 50, 51, 52, 53, 54
    }
    // 前5次：平均 = (50+51+52+53+54)/5 = 52.0
    assert(logic.getSmoothedSoc() > 51.9f && logic.getSmoothedSoc() < 52.1f);
    printf("  ✓ SOC 平滑窗口正确 (窗口=5)\n");

    // ─── 测试4：SOC 低电量判断 ───
    printf("\n[测试4] SOC 低电量判断\n");
    logic.onSocUpdate(20.0f);  // > 15，不触发低电量
    assert(!logic.isSocLow());
    assert(!logic.isSocCritical());

    logic.onSocUpdate(12.0f);  // 10 < 12 < 15，触发低电量
    assert(logic.isSocLow());
    assert(!logic.isSocCritical());

    logic.onSocUpdate(3.0f);   // < 5，触发临界
    assert(logic.isSocLow());
    assert(logic.isSocCritical());
    printf("  ✓ SOC 阈值判断正确\n");

    // ─── 测试5：驾驶模式字符串 ───
    printf("\n[测试5] 驾驶模式字符串\n");
    assert(strcmp(logic.getDriveModeStr(), "NORMAL") == 0);
    printf("  ✓ 默认驾驶模式 NORMAL\n");

    // ─── 测试6：预充电状态机 ───
    printf("\n[测试6] 预充电状态机\n");
    assert(logic.getPrechargeState() == PRECHARGE_IDLE);

    // 高压上电 → 开始预充电
    logic.onHvStatusUpdate(true);
    assert(logic.getPrechargeState() == PRECHARGE_ACTIVE);

    // 模拟预充电完成（tick 推进时间）
    logic.tick(600);  // 600ms 后
    assert(logic.getPrechargeState() == PRECHARGE_DONE);
    assert(logic.isReadyGo());
    printf("  ✓ 预充电状态机正确（ACTIVE → DONE → ReadyGo）\n");

    // ─── 测试7：预充电超时失败 ───
    printf("\n[测试7] 预充电超时失败\n");
    VehicleLogic logic2;
    logic2.init(&config);
    logic2.onHvStatusUpdate(true);
    // tick 不推进时间 → 超时
    logic2.tick(3000 + 100);  // 超过 3000ms
    assert(logic2.getPrechargeState() == PRECHARGE_FAILED);
    printf("  ✓ 预充电超时检测正确\n");

    // ─── 测试8：高压下电 ───
    printf("\n[测试8] 高压下电\n");
    logic.onHvStatusUpdate(false);
    assert(logic.getPrechargeState() == PRECHARGE_IDLE);
    assert(!logic.isReadyGo());
    assert(!logic.isHvActive());
    printf("  ✓ 高压下电状态正确\n");

    // ─── 测试9：ReadyGo 逻辑 ───
    printf("\n[测试9] ReadyGo 逻辑\n");
    logic.tick(0);
    logic.onHvStatusUpdate(true);
    logic.tick(600);  // 预充电完成
    logic.tick(700);
    logic.onSpeedUpdate(0.1f, true);  // 静止
    logic.tick(800);
    // PRECHARGE_DONE 且 speed < 0.5 → readyGo
    assert(logic.isReadyGo());

    // 行驶中关闭 ReadyGo
    logic.onSpeedUpdate(10.0f, true);  // speed > 5.0
    logic.tick(900);
    assert(!logic.isReadyGo());
    printf("  ✓ ReadyGo 激活/关闭逻辑正确\n");

    // ─── 测试10：v3 探针 — yaml 生成的 kDefaultVehicleConfig 字段一致性 ───
    // 这是 v3 探针的核心断言: 改 yaml 后重新跑此测试,
    // 若任何阈值与 config/vehicle_thresholds.yaml 不一致则 fail.
    printf("\n[测试10] v3 探针: kDefaultVehicleConfig == vehicle_thresholds.yaml\n");
    assert(kDefaultVehicleConfig.soc_warning_low == 10.0f);
    assert(kDefaultVehicleConfig.soc_critical_low == 5.0f);
    assert(kDefaultVehicleConfig.speed_max == 260.0f);
    assert(kDefaultVehicleConfig.precharge_timeout_ms == 3000u);
    assert(kDefaultVehicleConfig.precharge_auto_done_ms == 500u);
    assert(kDefaultVehicleConfig.soc_smoothing_window == 5);
    assert(kDefaultVehicleConfig.readygo_speed_engage_kmh == 0.5f);
    assert(kDefaultVehicleConfig.readygo_speed_disengage_kmh == 5.0f);
    printf("  ✓ 8 个阈值与 yaml 一致 (soc_warn=10, soc_crit=5, speed_max=260,\n");
    printf("    precharge_timeout=3000ms, precharge_auto_done=500ms,\n");
    printf("    soc_window=5, readygo_engage=0.5kmh, readygo_disengage=5.0kmh)\n");

    // ─── 测试11：v3 探针 — init(nullptr) 走 yaml 默认 ───
    printf("\n[测试11] v3 探针: init(nullptr) 应用 yaml 默认值\n");
    VehicleLogic logic3;
    logic3.init(nullptr);  // 不传 config, 走 yaml
    assert(logic3.config().soc_warning_low == 10.0f);
    assert(logic3.config().readygo_speed_disengage_kmh == 5.0f);
    assert(logic3.config().precharge_auto_done_ms == 500u);
    // 跑一遍行为, 验证阈值生效
    logic3.onHvStatusUpdate(true);
    logic3.tick(600);  // > 500ms, 应该走 auto_done
    assert(logic3.getPrechargeState() == PRECHARGE_DONE);
    printf("  ✓ init(nullptr) 行为正确 (走 yaml 默认配置)\n");

    printf("\n所有测试通过。\n");
    return 0;
}

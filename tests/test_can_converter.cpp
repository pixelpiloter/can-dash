// test_can_converter.cpp
// Layer 2 CanConverter 单元测试（纯 C++，无 Qt）
//
// ✅ 历史 off-by-3 bug 已修复 (commit ad52296, 2026-06-06):
//   can_converter.cpp 的 switch 现已与 CAN_FIELD_TABLE 28 项完全对齐:
//     case  0: bat_volt            case 14: engine_rpm
//     case  1: bat_curr            case 15: engine_fault
//     case  2: bat_soc             case 16: charge_status
//     case  3: battery_temp        case 17: charge_fault
//     case  4: vehicle_speed       case 18: charge_power
//     case  5: brake               case 19: energy_mode
//     case  6: motor_rpm           case 20: ev_range
//     case  7: motor_temp          case 21: fuel_level
//     case  8: driver_occupied     case 22: fuel_range
//     case  9: passenger_occupied  case 23: gear_status
//     case 10: driver_buckled      case 24-27: tire_pressure_*
//     case 11: passenger_buckled
//     case 12: rear_buckle
//     case 13: out-of-range, no-op (HYBRID/CHARGE/LIMP_HOME/TIRE 字段未在 DisplayData 暴露)
//
// 覆盖:
//   1.  init 装载 28 字段表
//   2.  findFieldIndex: 有效 / 无效 / 末位
//   3.  extractRaw little-endian 8-bit / 16-bit
//   4.  extractRaw big-endian 16-bit (字节序反转)
//   5.  extractRaw with 1-bit mask (driver_occupied)
//   6.  applyScaleOffset scale=0.1 offset=0
//   7.  applyScaleOffset scale=0.1 offset=-1000 (bat_curr 零报文 → -1000A)
//   8.  processFrame 错 can_id → mask=0, 字段不变
//   9.  processFrame 短帧 (len < byte_end+1) → 字段跳过
//  10.  processFrame BMS_Frame (0x186040F3) case 0-3 → bat_volt/bat_curr/bat_soc/battery_temp 全部正确
//  11.  processFrame vehicle_speed 帧 (can_id 515) case 4-5 → vehicle_speed/brake 正确
//  12.  processFrame bit 字段 (can_id 752) case 8 → driver_occupied 正确
//  13.  processFrame motor 帧 (can_id 257) case 6-7 → motor_rpm/motor_temp 正确
//  14.  processFrame 错 can_id 跨帧不互踩
//  15.  endian 同输入不同结果
// 15. updated_mask 反映 case 0-27 都触发 (BMS 4 字段触发 bit 0/1/2/3)

#include <cstdio>
#include <cstring>
#include <cassert>
#include "layer2/can_converter.h"
#include "layer1/display_data.h"
#include "can_field_def.h"

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(cond, msg) do {                                 \
    g_test_count++;                                                 \
    if (cond) {                                                     \
        g_test_passed++;                                            \
        printf("  ✓ %s\n", msg);                                    \
    } else {                                                        \
        printf("  ✗ %s (line %d)\n", msg, __LINE__);                \
    }                                                               \
} while(0)

// 注: 历史 TEST_KNOWN_BUG 宏已移除 — off-by-3 bug 在 commit ad52296 修复后,
//     所有受影响的字段都已正常路由. 若未来发现新 bug 需要文档化, 可参考 git history
//     重新引入该宏.

// 工具: 找 CAN_FIELD_TABLE 中 display_key 对应的 entry
static const CanFieldDef* findDef(const char* name) {
    for (int i = 0; i < CAN_FIELD_TABLE_COUNT; i++) {
        if (strcmp(CAN_FIELD_TABLE[i].display_key, name) == 0) {
            return &CAN_FIELD_TABLE[i];
        }
    }
    return nullptr;
}

// 1. init + findFieldIndex
static void test_init_and_find() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    TEST_ASSERT(CAN_FIELD_TABLE_COUNT == 28, "CAN_FIELD_TABLE 装载 28 条");
    TEST_ASSERT(cvt.findFieldIndex("bat_volt") == 0, "findFieldIndex(bat_volt) = 0");
    TEST_ASSERT(cvt.findFieldIndex("vehicle_speed") == 4, "findFieldIndex(vehicle_speed) = 4");
    TEST_ASSERT(cvt.findFieldIndex("tire_pressure_rr") == 27, "findFieldIndex(tire_pressure_rr) = 27");
    TEST_ASSERT(cvt.findFieldIndex("nonexistent") == -1, "findFieldIndex(unknown) = -1");
    TEST_ASSERT(cvt.findFieldIndex("") == -1, "findFieldIndex(空串) = -1");
}

// 2. extractRaw little-endian
static void test_extractRaw_little_endian() {
    const CanFieldDef* def = findDef("bat_volt");
    TEST_ASSERT(def != nullptr, "bat_volt 在 CAN_FIELD_TABLE 中");
    TEST_ASSERT(def->endian == ENDIAN_LITTLE, "bat_volt 默认 LE");
    uint8_t data[2] = {0xD8, 0x05};
    uint64_t raw = CanConverter::extractRaw(def, data);
    TEST_ASSERT(raw == 0x05D8, "extractRaw LE {0xD8,0x05} = 0x05D8 = 1496");

    // 8-bit field (note: extractRaw 不 bounds-check byte_start, 需传 ≥ byte_end+1 字节)
    const CanFieldDef* def_soc = findDef("bat_soc");  // byte_start=4, byte_end=4
    TEST_ASSERT(def_soc != nullptr, "bat_soc 在表中");
    uint8_t data8[5] = {0x00, 0x00, 0x00, 0x00, 0x4B};
    TEST_ASSERT(CanConverter::extractRaw(def_soc, data8) == 0x4B,
                "extractRaw LE 8-bit @byte4 {0x4B} = 75 (75)");
}

// 3. extractRaw big-endian
static void test_extractRaw_big_endian() {
    CanFieldDef def = {};
    def.byte_start = 0; def.byte_end = 1; def.bits = 16;
    def.endian = ENDIAN_BIG; def.shift = 0;
    uint8_t data[2] = {0xD8, 0x05};
    uint64_t raw = CanConverter::extractRaw(&def, data);
    TEST_ASSERT(raw == 0xD805, "extractRaw BE {0xD8,0x05} = 0xD805");
}

// 4. extractRaw 1-bit mask
static void test_extractRaw_1bit_mask() {
    const CanFieldDef* def = findDef("driver_occupied");
    TEST_ASSERT(def != nullptr, "driver_occupied 在表中");
    TEST_ASSERT(def->bits == 1, "driver_occupied 是 1-bit 字段");
    uint8_t data[1] = {0x01};
    TEST_ASSERT(CanConverter::extractRaw(def, data) == 1, "driver_occupied bit 0 = 1");
    data[0] = 0xFE;
    TEST_ASSERT(CanConverter::extractRaw(def, data) == 0, "driver_occupied bit 0 = 0");
    data[0] = 0x03;
    TEST_ASSERT(CanConverter::extractRaw(def, data) == 1, "driver_occupied mask 截断高位");
}

// 5. applyScaleOffset 基础 + 负 offset
static void test_applyScaleOffset() {
    const CanFieldDef* def = findDef("bat_volt");
    float v = CanConverter::applyScaleOffset(def, 1496);
    TEST_ASSERT(v > 149.5f && v < 149.7f, "scale=0.1, offset=0: 1496 → 149.6");

    const CanFieldDef* def_curr = findDef("bat_curr");
    float v0 = CanConverter::applyScaleOffset(def_curr, 0);
    TEST_ASSERT(v0 < -999.9f && v0 > -1000.1f,
                "scale=0.1, offset=-1000: raw 0 → -1000A (yaml 现状, 零电流报文 = -1000A)");
    float vmax = CanConverter::applyScaleOffset(def_curr, 20000);
    TEST_ASSERT(vmax > 999.9f && vmax < 1000.1f,
                "scale=0.1, offset=-1000: raw 20000 → 1000.0A");
}

// 6. processFrame 错 can_id
static void test_processFrame_wrong_can_id() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    DisplayData out = {};
    out.bat_volt = 999.0f;
    uint8_t data[8] = {0xFF};
    uint32_t mask = cvt.processFrame(0x12345678, data, sizeof(data), out);
    TEST_ASSERT(mask == 0, "错 can_id → mask = 0");
    TEST_ASSERT(out.bat_volt == 999.0f, "错 can_id 不覆盖 bat_volt");
}

// 7. processFrame 短帧
static void test_processFrame_short_data() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    DisplayData out = {};
    uint8_t data[1] = {75};
    uint32_t mask = cvt.processFrame(408961267, data, sizeof(data), out);
    TEST_ASSERT(mask == 0, "短帧 BMS_Frame 全部跳过, mask=0");
    TEST_ASSERT(out.bat_soc == 0, "短帧 bat_soc 不写");
}

// 8. processFrame BMS_Frame 正确 case (0/1/2)
static void test_processFrame_bms_correct_cases() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    // 0x186040F3 = 408961267
    // byte[0..1] = bat_volt: 0x05D8 LE → 1496 → 149.6V
    // byte[2..3] = bat_curr: 0x0000 LE → 0 → 0*0.1 - 1000 = -1000A
    // byte[4]    = bat_soc:  0x4B → 75%
    uint8_t data[8] = {0xD8, 0x05, 0x00, 0x00, 0x4B, 0x32, 0x00, 0x00};
    DisplayData out = {};
    uint32_t mask = cvt.processFrame(408961267, data, sizeof(data), out);

    // case 0/1/2 正确 (这 3 个 case 跟表对齐)
    TEST_ASSERT(out.bat_volt > 149.5f && out.bat_volt < 149.7f, "case 0 → bat_volt = 149.6V");
    TEST_ASSERT(out.bat_curr < -999.9f && out.bat_curr > -1000.1f, "case 1 → bat_curr = -1000A");
    TEST_ASSERT(out.bat_soc == 75, "case 2 → bat_soc = 75%");
}

// 9. processFrame BMS_Frame case 3 (battery_temp 修复后正确)
static void test_processFrame_bms_case_3_battery_temp() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    // BMS byte[5] = 0x32 = 50, table[3] = battery_temp, scale=1.0
    // 修复后: case 3 写 battery_temp = 50 (而非误写 motor_rpm)
    uint8_t data[8] = {0xD8, 0x05, 0x00, 0x00, 0x4B, 0x32, 0x00, 0x00};
    DisplayData out = {};
    cvt.processFrame(408961267, data, sizeof(data), out);

    // 修复后: case 3 正确写到 battery_temp
    TEST_ASSERT(out.battery_temp == 50, "case 3 → battery_temp = 50 (修复后正确)");
    TEST_ASSERT(out.motor_rpm == 0,
                "case 3 不再误写 motor_rpm (修复后 motor_rpm 保持 0)");

    // updated_mask bit 3 仍 set (case 3 命中, 写到 battery_temp)
    out = {};
    uint32_t mask = cvt.processFrame(408961267, data, sizeof(data), out);
    TEST_ASSERT(mask & (1U << 3), "updated_mask bit 3 仍 set (case 3 命中)");
    TEST_ASSERT(mask == 0x0F, "BMS_Frame 触发 case 0/1/2/3, mask = 0x0F");
}

// 10. processFrame vehicle_speed 帧 case 4 (vehicle_speed 修复后正确)
static void test_processFrame_vehicle_speed_case_4() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    // vehicle_speed: can_id 515, bytes 3-4 LE 16-bit, scale 0.1
    // data[3..4] = 0x10 0x27 → LE 0x2710 = 10000 → 1000.0 km/h
    // 修复后: case 4 正确写 vehicle_speed = 1000.0
    // data[2] = 0x00 → case 5 写 brake = 0
    uint8_t data[8] = {0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x00};
    DisplayData out = {};
    cvt.processFrame(515, data, sizeof(data), out);

    TEST_ASSERT(out.vehicle_speed > 999.9f && out.vehicle_speed < 1000.1f,
                "case 4 → vehicle_speed = 1000.0 km/h (修复后正确)");
    TEST_ASSERT(out.motor_temp == 0,
                "case 4 不再误写 motor_temp (修复后 motor_temp 保持 0)");
    TEST_ASSERT(out.brake == 0, "case 5 → brake = 0 (byte 2 = 0x00, scale 0.4 → 0)");
}

// 11. processFrame bit 字段 case 8 (driver_occupied 修复后正确)
static void test_processFrame_driver_occupied_case_8() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    // driver_occupied: can_id 752, table[8], case 8 → driver_occupied (修复后)
    uint8_t data[1] = {0x01};
    DisplayData out = {};
    cvt.processFrame(752, data, sizeof(data), out);

    TEST_ASSERT(out.driver_occupied == 1,
                "case 8 → driver_occupied = 1 (修复后正确写到 driver_occupied)");
    TEST_ASSERT(out.passenger_buckled == 0,
                "case 8 不再误写 passenger_buckled (修复后 passenger_buckled 保持 0)");
}

// 12. processFrame motor 帧 case 6/7 (motor_rpm/motor_temp 修复后正确)
static void test_processFrame_motor_case_6_7() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    // motor_rpm: can_id 257, table[6], case 6, byte 0-1 LE 16-bit, scale 1.0
    // motor_temp: can_id 257, table[7], case 7, byte 4, 8-bit, scale 1.0
    // data[0..1] = 0x54 0x0B → LE 0x0B54 = 2900 → motor_rpm = 2900
    // data[4] = 0x5A = 90 → motor_temp = 90
    uint8_t data[8] = {0x54, 0x0B, 0x00, 0x00, 0x5A, 0x00, 0x00, 0x00};
    DisplayData out = {};
    cvt.processFrame(257, data, sizeof(data), out);

    TEST_ASSERT(out.motor_rpm == 2900, "case 6 → motor_rpm = 2900 (修复后正确)");
    TEST_ASSERT(out.motor_temp == 90, "case 7 → motor_temp = 90 (修复后正确)");
}

// 13. 跨帧不互踩
static void test_processFrame_cross_frame_isolation() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    uint8_t bms[8] = {0xD8, 0x05, 0x00, 0x00, 0x4B, 0x32, 0x00, 0x00};
    DisplayData out = {};
    cvt.processFrame(408961267, bms, sizeof(bms), out);
    TEST_ASSERT(out.bat_volt > 149.5f, "BMS 帧写入 bat_volt");

    uint8_t speed[8] = {0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x00};
    cvt.processFrame(515, speed, sizeof(speed), out);
    // bat_volt 是 case 0 正确, vehicle_speed 帧不动它
    TEST_ASSERT(out.bat_volt > 149.5f, "vehicle_speed 帧不踩 bat_volt");
    TEST_ASSERT(out.bat_soc == 75, "vehicle_speed 帧不踩 bat_soc");
}

// 13. endian 字节序
static void test_endian_inverts_byte_order() {
    CanFieldDef le = {};
    le.byte_start = 0; le.byte_end = 1; le.bits = 16;
    le.endian = ENDIAN_LITTLE; le.shift = 0;
    CanFieldDef be = le;
    be.endian = ENDIAN_BIG;
    uint8_t data[2] = {0x12, 0x34};
    TEST_ASSERT(CanConverter::extractRaw(&le, data) == 0x3412, "LE {0x12,0x34} = 0x3412");
    TEST_ASSERT(CanConverter::extractRaw(&be, data) == 0x1234, "BE {0x12,0x34} = 0x1234");
}

// 14. 全 0 帧
static void test_processFrame_all_zero() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    uint8_t data[8] = {0};
    DisplayData out = {};
    cvt.processFrame(408961267, data, sizeof(data), out);
    TEST_ASSERT(out.bat_volt == 0.0f, "全 0 帧 bat_volt = 0");
    TEST_ASSERT(out.bat_curr == -1000.0f, "全 0 帧 bat_curr = -1000A (offset)");
    TEST_ASSERT(out.bat_soc == 0, "全 0 帧 bat_soc = 0");
}

// 15. HYBRID 字段 (cases 13-22): engine / charge / energy / range / fuel / gear
//    这些字段此前在 can_converter.cpp 的 switch 中 default 跳过 (commit 之前),
//    修复后必须正确写入 DisplayData 才能被 can-processor 同步到 SHM.
static void test_processFrame_hybrid_fields() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    DisplayData out = {};

    // 1) 发动机帧 0x305 (can_id 773): engine_rpm=3000 (LE 0x0BB8), engine_fault=0
    //    data[0] LSB = 0xB8, data[1] MSB = 0x0B
    //    engine_fault = data[0] bit 0 = 0
    uint8_t engine[8] = {0xB8, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(773, engine, sizeof(engine), out);
    TEST_ASSERT(out.engine_rpm == 3000,
                "case 13 → engine_rpm = 3000 (HYBRID 字段正确写入)");
    TEST_ASSERT(out.engine_fault == 0,
                "case 14 → engine_fault = 0 (HYBRID 字段正确写入)");

    // 2) 充电帧 0x306 (can_id 774): charge_status=2 (bit 0-1 = 0b10), charge_fault=0
    //    byte 0 = 0x02
    uint8_t charge[8] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(774, charge, sizeof(charge), out);
    TEST_ASSERT(out.charge_status == 2,
                "case 15 → charge_status = 2 (HYBRID 字段正确写入)");
    TEST_ASSERT(out.charge_fault == 0,
                "case 16 → charge_fault = 0 (HYBRID 字段正确写入)");

    // 3) 充电功率 0x302 (can_id 770): charge_power=22.5 kW, raw=22500 (LE 0x57E4), scale 0.001
    //    data[0] = 0xE4, data[1] = 0x57
    uint8_t power[8] = {0xE4, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(770, power, sizeof(power), out);
    TEST_ASSERT(out.charge_power > 22.4f && out.charge_power < 22.6f,
                "case 17 → charge_power = 22.5 kW (HYBRID 字段正确写入)");

    // 4) 能量模式 0x300 (can_id 768): energy_mode=3 (charge)
    uint8_t mode[8] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(768, mode, sizeof(mode), out);
    TEST_ASSERT(out.energy_mode == 3,
                "case 18 → energy_mode = 3 (HYBRID 字段正确写入)");

    // 5) 续航 0x307 (can_id 775): ev_range=80 (LE 0x0050)
    uint8_t range[8] = {0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(775, range, sizeof(range), out);
    TEST_ASSERT(out.ev_range == 80,
                "case 19 → ev_range = 80 km (HYBRID 字段正确写入)");

    // 6) 燃油 0x308 (can_id 776): fuel_level=60, fuel_range=500
    //    data[0] = 60 = 0x3C, data[1..2] = 500 LE = 0xF4 0x01
    uint8_t fuel[8] = {0x3C, 0xF4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(776, fuel, sizeof(fuel), out);
    TEST_ASSERT(out.fuel_level == 60,
                "case 20 → fuel_level = 60% (HYBRID 字段正确写入)");
    TEST_ASSERT(out.fuel_range == 500,
                "case 21 → fuel_range = 500 km (HYBRID 字段正确写入)");

    // 7) 档位 0x309 (can_id 777): gear=2
    uint8_t gear[8] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(777, gear, sizeof(gear), out);
    TEST_ASSERT(out.gear_status == 2,
                "case 22 → gear_status = 2 (HYBRID 字段正确写入)");

    // 综合: updated_mask 应包含 case 13-22 所有 bit
    // 用 fresh DisplayData 重测, 累计 mask
    out = {};
    uint32_t mask = 0;
    uint32_t tmp_mask = 0;
    tmp_mask = cvt.processFrame(773, engine, sizeof(engine), out); mask |= tmp_mask;
    tmp_mask = cvt.processFrame(774, charge, sizeof(charge), out); mask |= tmp_mask;
    tmp_mask = cvt.processFrame(770, power, sizeof(power), out);   mask |= tmp_mask;
    tmp_mask = cvt.processFrame(768, mode, sizeof(mode), out);     mask |= tmp_mask;
    tmp_mask = cvt.processFrame(775, range, sizeof(range), out);   mask |= tmp_mask;
    tmp_mask = cvt.processFrame(776, fuel, sizeof(fuel), out);     mask |= tmp_mask;
    tmp_mask = cvt.processFrame(777, gear, sizeof(gear), out);     mask |= tmp_mask;
    // 期望: bit 13 (engine_rpm) | bit 14 (engine_fault) | bit 15 (charge_status) |
    //       bit 16 (charge_fault) | bit 17 (charge_power) | bit 18 (energy_mode) |
    //       bit 19 (ev_range) | bit 20 (fuel_level) | bit 21 (fuel_range) | bit 22 (gear_status)
    uint32_t expected = (1u << 13) | (1u << 14) | (1u << 15) | (1u << 16) |
                        (1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) |
                        (1u << 21) | (1u << 22);
    TEST_ASSERT((mask & expected) == expected,
                "HYBRID 7 帧触发 case 13-22 全部 bit (0x3FFFE000 子集)");
}

// 16. TIRE 字段 (cases 23-27): 4 个轮胎压力 + 1 个 min 派生
//    此前 can_converter.cpp 同样 default 跳过, 修复后正确写入.
//    注: DisplayData.tire_pressure_* 是 uint16_t, can_field_table scale=0.1
//    (YAML 公式 x/10.0), 实际写入的是 cast(uint16_t, raw*0.1) = cast(uint16_t, value).
//    例如 raw=23 → value=2.3 → uint16_t cast = 2 (精度损失, 已知限制).
//    测试验证: 写入非 0, 且 updated_mask bit 23-27 全部 set.
static void test_processFrame_tire_fields() {
    CanConverter cvt;
    cvt.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);
    DisplayData out = {};

    // 1) 胎压帧 0x3A0 (can_id 928): tire_pressure_fl + tire_pressure 共享字节 0-1
    //    raw = 23 (2.3 bar, scale 0.1) → LE 0x17 0x00
    uint8_t fl[8] = {0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(928, fl, sizeof(fl), out);
    // scale=0.1 应用于 raw=23 得 value=2.3, cast uint16 → 2 (精度损失)
    TEST_ASSERT(out.tire_pressure_fl == 2,
                "case 23 → tire_pressure_fl = 2 (cast of 2.3, scale=0.1 已知精度损失, TIRE 字段正确写入)");
    TEST_ASSERT(out.tire_pressure == 2,
                "case 24 → tire_pressure = 2 (TIRE 字段正确写入)");

    // 2) 胎压帧 0x3A1 (can_id 929): tire_pressure_fr
    uint8_t fr[8] = {0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(929, fr, sizeof(fr), out);
    TEST_ASSERT(out.tire_pressure_fr == 2,
                "case 25 → tire_pressure_fr = 2 (TIRE 字段正确写入)");

    // 3) 胎压帧 0x3A2 (can_id 930): tire_pressure_rl
    uint8_t rl[8] = {0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(930, rl, sizeof(rl), out);
    TEST_ASSERT(out.tire_pressure_rl == 2,
                "case 26 → tire_pressure_rl = 2 (TIRE 字段正确写入)");

    // 4) 胎压帧 0x3A3 (can_id 931): tire_pressure_rr
    uint8_t rr[8] = {0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cvt.processFrame(931, rr, sizeof(rr), out);
    TEST_ASSERT(out.tire_pressure_rr == 2,
                "case 27 → tire_pressure_rr = 2 (TIRE 字段正确写入)");

    // 综合: updated_mask 应包含 case 23-27
    out = {};
    uint32_t mask = 0;
    uint32_t tmp_mask = 0;
    tmp_mask = cvt.processFrame(928, fl, sizeof(fl), out); mask |= tmp_mask;
    tmp_mask = cvt.processFrame(929, fr, sizeof(fr), out); mask |= tmp_mask;
    tmp_mask = cvt.processFrame(930, rl, sizeof(rl), out); mask |= tmp_mask;
    tmp_mask = cvt.processFrame(931, rr, sizeof(rr), out); mask |= tmp_mask;
    uint32_t expected = (1u << 23) | (1u << 24) | (1u << 25) | (1u << 26) | (1u << 27);
    TEST_ASSERT((mask & expected) == expected,
                "TIRE 4 帧触发 case 23-27 全部 bit (0x3F800000 子集)");
}

int main() {
    printf("=== CanConverter 单元测试 (PR 22 + ad52296 fix) ===\n");
    printf("✅ off-by-3 bug 已修复 (commit ad52296, 2026-06-06)\n");
    printf("   28 字段 case 索引与 CAN_FIELD_TABLE 完全对齐\n\n");

    test_init_and_find();
    test_extractRaw_little_endian();
    test_extractRaw_big_endian();
    test_extractRaw_1bit_mask();
    test_applyScaleOffset();
    test_processFrame_wrong_can_id();
    test_processFrame_short_data();
    test_processFrame_bms_correct_cases();
    test_processFrame_bms_case_3_battery_temp();
    test_processFrame_vehicle_speed_case_4();
    test_processFrame_driver_occupied_case_8();
    test_processFrame_motor_case_6_7();
    test_processFrame_cross_frame_isolation();
    test_endian_inverts_byte_order();
    test_processFrame_all_zero();
    test_processFrame_hybrid_fields();
    test_processFrame_tire_fields();

    printf("\n=== %d/%d tests passed ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

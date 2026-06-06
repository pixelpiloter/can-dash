
// can-processor main.cpp
// 双进程 can-dash 的"数据端"：从 CAN 总线（或仿真 socket）收帧，
// 解析后写入共享内存供 can-dash 读。
//
// 启动参数：
//   默认（无参数）       : sim-socket，等待 can_sim/engine.py 连接
//   --transport=socketcan  : Linux SocketCAN（生产环境）
//   --can-if=can0          : SocketCAN 接口名（默认 can0）
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "layer1/shm/shm_display.h"
#include "layer2/can_converter.h"
#include "layer2/alarm_runtime.h"
#include "layer2/indicator_runtime.h"
#include "layer2/seat_belt_runtime.h"
#include "layer2/time_util.h"
#include "layer2/transport/transport_factory.h"
#include "can_field_def.h"
#include "alarm_rule_def.h"
#include "indicator_def.h"
#include "seat_belt_def.h"

#define TICK_PERIOD_MS 100

// 内存屏障，确保共享内存写入对 reader 进程可见
#define SHM_BARRIER() __sync_synchronize()

static CanConverter g_converter;
static AlarmRuntime* g_alarmRuntime = nullptr;
static IndicatorRuntime* g_indicatorRuntime = nullptr;

// DisplayData字段 → ShmFieldIndex 的映射表（按CAN_FIELD_TABLE顺序）
// DisplayData字段顺序: bat_volt, bat_curr, bat_soc, battery_temp, vehicle_speed, brake,
//                      motor_rpm, motor_temp, driver_occupied, passenger_occupied,
//                      driver_buckled, passenger_buckled, rear_buckle,
//                      engine_rpm, engine_fault, charge_status, charge_fault, charge_power,
//                      energy_mode, ev_range, fuel_level, fuel_range, gear_status,
//                      tire_pressure_fl, tire_pressure, tire_pressure_fr, tire_pressure_rl, tire_pressure_rr
static const ShmFieldIndex FIELD_TO_SHM[] = {
    SHM_FIELD_BAT_VOLT,           // [0] bat_volt
    SHM_FIELD_BAT_CURR,           // [1] bat_curr
    SHM_FIELD_BAT_SOC,            // [2] bat_soc
    SHM_FIELD_BATTERY_TEMP,       // [3] battery_temp
    SHM_FIELD_VEHICLE_SPEED,      // [4] vehicle_speed
    SHM_FIELD_BRAKE,              // [5] brake
    SHM_FIELD_MOTOR_RPM,          // [6] motor_rpm
    SHM_FIELD_MOTOR_TEMP,         // [7] motor_temp
    SHM_FIELD_DRIVER_OCCUPIED,    // [8] driver_occupied
    SHM_FIELD_PASSENGER_OCCUPIED, // [9] passenger_occupied
    SHM_FIELD_DRIVER_BUCKLED,     // [10] driver_buckled
    SHM_FIELD_PASSENGER_BUCKLED,  // [11] passenger_buckled
    SHM_FIELD_REAR_BUCKLE,        // [12] rear_buckle
    SHM_FIELD_ENGINE_RPM,         // [13] engine_rpm
    SHM_FIELD_ENGINE_FAULT,        // [14] engine_fault
    SHM_FIELD_CHARGE_STATUS,      // [15] charge_status
    SHM_FIELD_COUNT,              // [16] charge_fault (not mapped to SHM)
    SHM_FIELD_CHARGE_POWER,       // [17] charge_power
    SHM_FIELD_ENERGY_MODE,        // [18] energy_mode
    SHM_FIELD_EV_RANGE,            // [19] ev_range
    SHM_FIELD_FUEL_LEVEL,         // [20] fuel_level
    SHM_FIELD_FUEL_RANGE,         // [21] fuel_range
    SHM_FIELD_GEAR_STATUS,        // [22] gear_status
    // [23-27] tire_pressure fields - not mapped to SHM
    SHM_FIELD_COUNT,              // [23] tire_pressure_fl
    SHM_FIELD_COUNT,              // [24] tire_pressure
    SHM_FIELD_COUNT,              // [25] tire_pressure_fr
    SHM_FIELD_COUNT,              // [26] tire_pressure_rl
    SHM_FIELD_COUNT,              // [27] tire_pressure_rr
};
static const int FIELD_TO_SHM_COUNT = sizeof(FIELD_TO_SHM) / sizeof(FIELD_TO_SHM[0]);

// ─── 报警回调 ────────────────────────────────────────────
static void alarm_on_indicator(const char* widget_id, bool on, bool flash,
                                float flash_hz, void* /*user*/) {
    if (strcmp(widget_id, "left_turn_light") == 0)
        shm_display_set_indicator(IND_LEFT_TURN, on, flash, flash_hz);
    else if (strcmp(widget_id, "right_turn_light") == 0)
        shm_display_set_indicator(IND_RIGHT_TURN, on, flash, flash_hz);
    else if (strcmp(widget_id, "park_brake_light") == 0)
        shm_display_set_indicator(IND_PARK_BRAKE, on, flash, flash_hz);
    else if (strcmp(widget_id, "ready_go_light") == 0)
        shm_display_set_indicator(IND_READY_GO, on, flash, flash_hz);
    else if (strcmp(widget_id, "bat_warn_light") == 0)
        shm_display_set_indicator(IND_BAT_WARN, on, flash, flash_hz);
    else if (strcmp(widget_id, "check_engine_light") == 0)
        shm_display_set_indicator(IND_ENGINE, on, flash, flash_hz);
    else if (strcmp(widget_id, "high_voltage_light") == 0)
        shm_display_set_indicator(IND_HIGH_VOLT, on, flash, flash_hz);
    else if (strcmp(widget_id, "fog_light") == 0)
        shm_display_set_indicator(IND_FOG_LIGHT, on, flash, flash_hz);
    else if (strcmp(widget_id, "tire_pressure_light") == 0)
        shm_display_set_indicator(IND_TIRE_PRESSURE, on, flash, flash_hz);
    else if (strcmp(widget_id, "seatbelt_warning") == 0)
        shm_display_set_indicator(IND_SEATBELT, on, flash, flash_hz);
}

static void alarm_on_text(const char* text_zh, const char* text_en, void* /*user*/) {  // NOLINT(bugprone-easily-swappable-parameters)
    (void)text_en;
    shm_display_set_alarm(text_zh);
}

static void alarm_on_state(const char* /*alarm_name*/, bool active, void* /*user*/) {
    if (!active) shm_display_set_alarm(NULL);
}

static void indicator_on_state(const char* id, bool on, bool flash,
                               float hz, void* /*user*/) {
    alarm_on_indicator(id, on, flash, hz, NULL);
}

// ─── 从 DisplayData 按索引取值写入共享内存 ───────────────
static void sync_field_to_shm(int field_idx, const DisplayData* dd) {
    if (field_idx < 0 || field_idx >= FIELD_TO_SHM_COUNT) return;
    ShmFieldIndex sid = FIELD_TO_SHM[field_idx];
    if (sid == SHM_FIELD_COUNT) return; // unmapped field (e.g. tire_pressure)
    switch (field_idx) {
        case 0: shm_display_set_float(sid, dd->bat_volt); break;
        case 1: shm_display_set_float(sid, dd->bat_curr); break;
        case 2: shm_display_set_uint8(sid, dd->bat_soc); break;
        case 3: shm_display_set_uint8(sid, (uint8_t)dd->battery_temp); break; // int8_t -> uint8_t
        case 4: shm_display_set_float(sid, dd->vehicle_speed); break;
        case 5: shm_display_set_uint8(sid, dd->brake); break;
        case 6: shm_display_set_float(sid, dd->motor_rpm); break;
        case 7: shm_display_set_uint8(sid, dd->motor_temp); break;
        case 8: shm_display_set_uint8(sid, dd->driver_occupied); break;
        case 9: shm_display_set_uint8(sid, dd->passenger_occupied); break;
        case 10: shm_display_set_uint8(sid, dd->driver_buckled); break;
        case 11: shm_display_set_uint8(sid, dd->passenger_buckled); break;
        case 12: shm_display_set_uint8(sid, dd->rear_buckle); break;
        case 13: shm_display_set_uint16(sid, dd->engine_rpm); break;
        case 14: shm_display_set_uint8(sid, dd->engine_fault); break;
        case 15: shm_display_set_uint8(sid, dd->charge_status); break;
        case 16: /* charge_fault: no SHM field, skip */ break;
        case 17: shm_display_set_float(sid, dd->charge_power); break;
        case 18: shm_display_set_uint8(sid, dd->energy_mode); break;
        case 19: shm_display_set_uint16(sid, dd->ev_range); break;
        case 20: shm_display_set_uint8(sid, dd->fuel_level); break;
        case 21: shm_display_set_uint16(sid, dd->fuel_range); break;
        case 22: shm_display_set_uint8(sid, dd->gear_status); break;
        // case 23-27: tire_pressure fields - not mapped to SHM
    }
}

// ─── 处理一个CAN帧 ───────────────────────────────────────
static void process_can_frame(uint32_t can_id, const uint8_t* data, size_t len) {
    DisplayData dd = {};
    uint32_t mask = g_converter.processFrame(can_id, data, len, dd);
    if (mask == 0) return;

    // 遍历每一位掩码
    for (int i = 0; i < CAN_FIELD_TABLE_COUNT && i < FIELD_TO_SHM_COUNT; i++) {
        if (mask & (1U << i)) {
            sync_field_to_shm(i, &dd);
        }
    }

    // 通知报警runtime
    if (g_alarmRuntime) {
        for (int i = 0; i < CAN_FIELD_TABLE_COUNT; i++) {
            if (mask & (1U << i)) {
                const char* key = CAN_FIELD_TABLE[i].display_key;
                float value = 0.0f;
                switch (i) {
                    case 0: value = dd.bat_volt; break;
                    case 1: value = dd.bat_curr; break;
                    case 2: value = dd.bat_soc; break;
                    case 3: value = dd.battery_temp; break;
                    case 4: value = dd.vehicle_speed; break;
                    case 5: value = dd.brake; break;
                    case 6: value = dd.motor_rpm; break;
                    case 7: value = dd.motor_temp; break;
                    case 8: value = dd.driver_occupied; break;
                    case 9: value = dd.passenger_occupied; break;
                    case 10: value = dd.driver_buckled; break;
                    case 11: value = dd.passenger_buckled; break;
                    case 12: value = dd.rear_buckle; break;
                    case 13: value = dd.engine_rpm; break;
                    case 14: value = dd.engine_fault; break;
                    case 15: value = dd.charge_status; break;
                    case 16: value = dd.charge_fault; break;
                    case 17: value = dd.charge_power; break;
                    case 18: value = dd.energy_mode; break;
                    case 19: value = dd.ev_range; break;
                    case 20: value = dd.fuel_level; break;
                    case 21: value = dd.fuel_range; break;
                    case 22: value = dd.gear_status; break;
                    case 23: value = dd.tire_pressure_fl; break;        // tire_pressure_fl
                    case 24: value = dd.tire_pressure; break;           // tire_pressure (tire_pressure_low alarm 用)
                    case 25: value = dd.tire_pressure_fr; break;        // tire_pressure_fr
                    case 26: value = dd.tire_pressure_rl; break;        // tire_pressure_rl
                    case 27: value = dd.tire_pressure_rr; break;        // tire_pressure_rr
                    default: break;
                }
                g_alarmRuntime->onValueChanged(key, value);
            }
        }
    }
}

int main(int argc, char** argv) {
    // 解析 CLI 参数（包含 --help 退出）
    const candash::transport::TransportConfig cfg =
        candash::transport::TransportFactory::parseArgs(argc, argv);

    printf("[Processor] Starting with transport: ");
    if (cfg.type == candash::transport::TransportType::kSocketCan) {
        printf("socketcan (if=%s)\n", cfg.can_if_name);
    } else {
        printf("sim-socket\n");
    }

    if (shm_display_create() < 0) {
        fprintf(stderr, "[Processor] shm_create failed\n"); return 1;
    }
    printf("[Processor] Shared memory: %s\n", SHM_DISPLAY_PATH);

    g_converter.init(CAN_FIELD_TABLE, CAN_FIELD_TABLE_COUNT);

    AlarmCallbacks alarm_cb = {};
    alarm_cb.onIndicatorUpdate = alarm_on_indicator;
    alarm_cb.onAlarmTextUpdate = alarm_on_text;
    alarm_cb.onAlarmStateChanged = alarm_on_state;
    AlarmRuntime alarmRuntime(alarm_cb);
    alarmRuntime.init(ALARM_RULE_TABLE, ALARM_RULE_TABLE_COUNT,
                      ALARM_ACTION_TABLE, ALARM_ACTION_TABLE_COUNT);
    g_alarmRuntime = &alarmRuntime;

    IndicatorCallbacks ind_cb = {};
    ind_cb.onStateChange = indicator_on_state;
    IndicatorRuntime indRuntime(ind_cb);
    indRuntime.init(INDICATOR_TABLE, INDICATOR_TABLE_COUNT);
    g_indicatorRuntime = &indRuntime;

    SeatBeltRuntime seatBeltRuntime;
    seatBeltRuntime.init(SEAT_POSITION_TABLE, SEAT_POSITION_TABLE_COUNT,
                         &SEAT_BELT_CONFIG);

    // 创建并打开传输层
    auto transport = candash::transport::TransportFactory::create(cfg);
    if (!transport) {
        fprintf(stderr, "[Processor] transport create failed\n");
        return 1;
    }
    if (!transport->open()) {
        fprintf(stderr, "[Processor] transport open failed (name=%s)\n",
                transport->name());
        return 1;
    }
    printf("[Processor] Transport: %s\n", transport->name());

    uint8_t rx_data[candash::transport::kCanMaxDlc] = {};
    uint64_t last_tick_ms = 0;
    printf("[Processor] Ready.\n");

    // 主循环：读帧 + 周期 tick + 按需提交共享内存
    // TODO(SIGINT) : 当前仅 SIGKILL 可退出，未来加信号优雅关闭
    while (1) {
        uint32_t can_id = 0;
        uint8_t dlc = 0;
        if (transport->readFrame(can_id, dlc, rx_data, sizeof(rx_data),
                                 TICK_PERIOD_MS)) {
            process_can_frame(can_id, rx_data, dlc);
        }

        uint64_t tick_ms = candash::now_monotonic_ms();
        if (tick_ms - last_tick_ms >= TICK_PERIOD_MS) {
            alarmRuntime.tick(tick_ms);
            indRuntime.tick(tick_ms);
            if (transport->isReady()) {
                SHM_BARRIER();
                shm_display_commit();
            }
            last_tick_ms = tick_ms;
        }
    }

    transport->close();
    shm_display_close();
    printf("[Processor] Exiting.\n");
    return 0;
}

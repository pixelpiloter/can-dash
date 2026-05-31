
// can-processor main.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include "layer1/shm/shm_display.h"
#include "layer2/can_converter.h"
#include "layer2/alarm_runtime.h"
#include "layer2/indicator_runtime.h"
#include "layer2/seat_belt_runtime.h"
#include "can_field_def.h"
#include "alarm_rule_def.h"
#include "indicator_def.h"
#include "seat_belt_def.h"

#define RX_BUFFER_SIZE 256

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

static void alarm_on_text(const char* text_zh, const char* text_en, void* /*user*/) {
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
                    default: break;
                }
                g_alarmRuntime->onValueChanged(key, value);
            }
        }
    }
}

// ─── Unix Socket 服务器 ────────────────────────────────────
static int setup_socket(void) {
    unlink(SOCKET_PATH);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 3) < 0) { close(fd); return -1; }
    return fd;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("[Processor] Starting...\n");

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

    int listen_fd = setup_socket();
    if (listen_fd < 0) { fprintf(stderr, "[Processor] socket failed\n"); return 1; }
    printf("[Processor] Socket: %s\n", SOCKET_PATH);

    uint8_t rx_buf[RX_BUFFER_SIZE];
    int rx_len = 0;
    int client_fd = -1;
    uint64_t tick_ms = 0;

    printf("[Processor] Ready.\n");

    while (1) {
        struct pollfd pfd[2] = {};
        pfd[0].fd = listen_fd; pfd[0].events = POLLIN;
        if (client_fd >= 0) { pfd[1].fd = client_fd; pfd[1].events = POLLIN; }

        int n = poll(pfd, client_fd >= 0 ? 2 : 1, 100);
        if (n < 0) break;

        if (pfd[0].revents & POLLIN) {
            if (client_fd >= 0) close(client_fd);
            client_fd = accept(listen_fd, NULL, NULL);
            rx_len = 0;
            printf("[Processor] Client connected\n");
        }

        if (client_fd >= 0 && pfd[1].revents & POLLIN) {
            uint8_t buf[64];
            int r = read(client_fd, buf, sizeof(buf));
            if (r <= 0) { close(client_fd); client_fd = -1; rx_len = 0; continue; }

            for (int i = 0; i < r; i++) {
                if (rx_len < RX_BUFFER_SIZE) rx_buf[rx_len++] = buf[i];
                if (rx_len >= 5) {
                    uint8_t dlc = rx_buf[4];
                    int frame_len = 5 + dlc;
                    if (rx_len >= frame_len) {
                        uint32_t can_id = rx_buf[0]
                                        | ((uint32_t)rx_buf[1] << 8)
                                        | ((uint32_t)rx_buf[2] << 16)
                                        | ((uint32_t)rx_buf[3] << 24);
                        process_can_frame(can_id, &rx_buf[5], dlc);
                        rx_len -= frame_len;
                        if (rx_len > 0)
                            memmove(rx_buf, &rx_buf[frame_len], rx_len);
                    }
                }
            }
        }

        tick_ms += 100;
        alarmRuntime.tick(tick_ms);
        indRuntime.tick(tick_ms);

        if (client_fd >= 0) shm_display_commit();
    }

    if (client_fd >= 0) close(client_fd);
    close(listen_fd);
    shm_display_close();
    printf("[Processor] Exiting.\n");
    return 0;
}

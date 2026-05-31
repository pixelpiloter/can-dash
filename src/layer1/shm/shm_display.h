// shm_display.h
// Layer 1: 共享内存显示数据结构（纯C，无Qt，无动态内存）

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHM_DISPLAY_PATH  "/dev/shm/can_display"
#define SOCKET_PATH       "/tmp/can_processor_socket"

typedef enum {
    SHM_FIELD_TIMESTAMP = 0,
    SHM_FIELD_MOTOR_RPM,
    SHM_FIELD_VEHICLE_SPEED,
    SHM_FIELD_BAT_VOLT,
    SHM_FIELD_BAT_CURR,
    SHM_FIELD_BAT_SOC,
    SHM_FIELD_MOTOR_TEMP,
    SHM_FIELD_BRAKE,
    SHM_FIELD_DRIVER_OCCUPIED,
    SHM_FIELD_PASSENGER_OCCUPIED,
    SHM_FIELD_DRIVER_BUCKLED,
    SHM_FIELD_PASSENGER_BUCKLED,
    SHM_FIELD_REAR_BUCKLE,
    // HYBRID fields (indices 12-18)
    SHM_FIELD_BATTERY_TEMP,
    SHM_FIELD_ENERGY_MODE,
    SHM_FIELD_FUEL_LEVEL,
    SHM_FIELD_FUEL_RANGE,
    SHM_FIELD_CHARGE_POWER,
    SHM_FIELD_CHARGE_STATUS,
    SHM_FIELD_EV_RANGE,
    SHM_FIELD_ENGINE_RPM,
    SHM_FIELD_ENGINE_FAULT,
    SHM_FIELD_GEAR_STATUS,
    SHM_FIELD_COUNT
} ShmFieldIndex;

#define SHM_INDICATOR_COUNT 12
typedef struct {
    uint8_t on;
    uint8_t flash;
    uint8_t hz_x10;
    uint8_t _pad;
} ShmIndicatorSlot;

#define SHM_ALARM_TEXT_LEN 128

typedef struct {
    uint64_t  timestamp;
    uint32_t  updated_mask;
    // Basic fields (same as before)
    float     motor_rpm;
    float     vehicle_speed;
    float     bat_volt;
    float     bat_curr;
    uint8_t   bat_soc;
    uint8_t   motor_temp;
    uint8_t   brake;
    uint8_t   driver_occupied;
    uint8_t   passenger_occupied;
    uint8_t   driver_buckled;
    uint8_t   passenger_buckled;
    uint8_t   rear_buckle;
    // HYBRID fields (indices 12+)
    uint8_t   battery_temp;      // SHM_FIELD_BATTERY_TEMP (index 13)
    uint8_t   energy_mode;       // SHM_FIELD_ENERGY_MODE (index 14)
    uint8_t   fuel_level;        // SHM_FIELD_FUEL_LEVEL  (index 15)
    uint16_t  fuel_range;        // SHM_FIELD_FUEL_RANGE  (index 16)
    float     charge_power;       // SHM_FIELD_CHARGE_POWER (index 17)
    uint8_t   charge_status;     // SHM_FIELD_CHARGE_STATUS (index 18)
    uint16_t  ev_range;          // SHM_FIELD_EV_RANGE    (index 19)
    uint16_t  engine_rpm;        // SHM_FIELD_ENGINE_RPM  (index 20)
    uint8_t   engine_fault;      // SHM_FIELD_ENGINE_FAULT (index 21)
    uint8_t   gear_status;       // SHM_FIELD_GEAR_STATUS (index 22)
    uint8_t   _reserved2[3];     // padding to 8-byte alignment
    // Backlight control (REQ-SYS-003)
    uint8_t   backlight_timeout_seconds; // LCD背光超时时间(秒)
    uint8_t   backlight_state;            // 0=正常, 1=暗, 2=关闭
    uint8_t   alarm_active;
    char      alarm_message_zh[SHM_ALARM_TEXT_LEN];
    ShmIndicatorSlot indicators[SHM_INDICATOR_COUNT];
    uint8_t   _padding[235];     // remaining space to 512 bytes
} DisplayDataShm;


typedef enum {
    IND_LEFT_TURN = 0, IND_RIGHT_TURN = 1, IND_PARK_BRAKE = 2,
    IND_READY_GO = 3, IND_BAT_WARN = 4, IND_ENGINE = 5,
    IND_HIGH_VOLT = 6, IND_FOG_LIGHT = 7, IND_SEATBELT = 8,
    IND_TIRE_PRESSURE = 9, IND_COUNT
} ShmIndicatorId;

// processor端
int shm_display_create(void);
void shm_display_write(const DisplayDataShm* data);
void shm_display_set_float(ShmFieldIndex idx, float value);
void shm_display_set_uint8(ShmFieldIndex idx, uint8_t value);
void shm_display_set_uint16(ShmFieldIndex idx, uint16_t value);
void shm_display_set_alarm(const char* msg_zh);
void shm_display_set_indicator(ShmIndicatorId id, int on, int flash, float hz);
void shm_display_set_backlight_state(uint8_t state);
void shm_display_mark_updated(ShmFieldIndex idx);
void shm_display_commit(void);

// dash端
int shm_display_open(void);
uint64_t shm_display_read(DisplayDataShm* out_data);
uint64_t shm_display_poll(uint64_t last_timestamp);
void shm_display_close(void);

#ifdef __cplusplus
}
#endif

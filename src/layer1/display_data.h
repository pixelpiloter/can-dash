// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/can_ids.yaml

#pragma once
#include <stdint.h>
#include <stdbool.h>

// 显示数据结构（按领域拆分）
typedef struct {
    float         bat_volt;   // unit: V
    float         bat_curr;   // unit: A
    uint8_t       bat_soc;   // unit: %
    int8_t        battery_temp;   // unit: °C
    float         vehicle_speed;   // unit: km/h
    uint8_t       brake;   // unit: %
    int16_t       motor_rpm;   // unit: rpm
    uint8_t       motor_temp;   // unit: °C
    uint8_t       driver_occupied;   // unit: 
    uint8_t       passenger_occupied;   // unit: 
    uint8_t       driver_buckled;   // unit: 
    uint8_t       passenger_buckled;   // unit: 
    uint8_t       rear_buckle;   // unit: 
    uint16_t      engine_rpm;   // unit: rpm
    uint8_t       engine_fault;   // unit: 
    uint8_t       charge_status;   // unit: 
    uint8_t       charge_fault;   // unit: 
    float         charge_power;   // unit: kW
    uint8_t       energy_mode;   // unit: 
    uint16_t      ev_range;   // unit: km
    uint8_t       fuel_level;   // unit: %
    uint16_t      fuel_range;   // unit: km
    uint8_t       gear_status;   // unit: 
} DisplayData;


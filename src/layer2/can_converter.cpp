// can_converter.cpp
#include "can_converter.h"
#include "../generated/can_field_def.h"
#include <cstdio>

CanConverter::CanConverter() = default;

void CanConverter::init(const CanFieldDef* table, int table_count) {
    m_table = table;
    m_tableCount = table_count;
}

uint32_t CanConverter::processFrame(uint32_t can_id, const uint8_t* data,
                                    size_t len, DisplayData& out) {
    uint32_t updated_mask = 0;

    for (int i = 0; i < m_tableCount; i++) {
        const CanFieldDef* def = &m_table[i];
        if (def->can_id != can_id) continue;
        if (def->byte_end >= (int)len) continue;

        uint64_t raw = extractRaw(def, data);
        float value = applyScaleOffset(def, raw);

        // 根据字段索引写入 DisplayData
        // 索引顺序与 can_field_table.cpp 完全一致（28 项）
        // ⚠️ 严格遵守 skill 顶部 "switch-case 字段索引错位" 章节:
        //   每个 case 的字段名必须与 CAN_FIELD_TABLE[i].display_key 一致,
        //   任何 off-by-N 都会让 can-processor 写错 SHM 字段, QML 显示 0.
        // 严格自检: 28 个 case 必须全部覆盖, 顺序与 can_field_table.cpp 完全一致.
        switch (i) {
            case  0: out.bat_volt = value; break;                       // bat_volt
            case  1: out.bat_curr = value; break;                       // bat_curr
            case  2: out.bat_soc = (uint8_t)value; break;              // bat_soc
            case  3: out.battery_temp = (int8_t)value; break;           // battery_temp
            case  4: out.vehicle_speed = value; break;                  // vehicle_speed
            case  5: out.brake = (uint8_t)value; break;                 // brake
            case  6: out.motor_rpm = (int16_t)value; break;             // motor_rpm
            case  7: out.motor_temp = (uint8_t)value; break;            // motor_temp
            case  8: out.driver_occupied = (uint8_t)value; break;       // driver_occupied
            case  9: out.passenger_occupied = (uint8_t)value; break;     // passenger_occupied
            case 10: out.driver_buckled = (uint8_t)value; break;         // driver_buckled
            case 11: out.passenger_buckled = (uint8_t)value; break;      // passenger_buckled
            case 12: out.rear_buckle = (uint8_t)value; break;            // rear_buckle
            case 13: out.engine_rpm = (uint16_t)value; break;            // engine_rpm
            case 14: out.engine_fault = (uint8_t)value; break;           // engine_fault
            case 15: out.charge_status = (uint8_t)value; break;          // charge_status
            case 16: out.charge_fault = (uint8_t)value; break;           // charge_fault
            case 17: out.charge_power = value; break;                    // charge_power
            case 18: out.energy_mode = (uint8_t)value; break;            // energy_mode
            case 19: out.ev_range = (uint16_t)value; break;              // ev_range
            case 20: out.fuel_level = (uint8_t)value; break;             // fuel_level
            case 21: out.fuel_range = (uint16_t)value; break;            // fuel_range
            case 22: out.gear_status = (uint8_t)value; break;            // gear_status
            case 23: out.tire_pressure_fl = (uint16_t)value; break;      // tire_pressure_fl
            case 24: out.tire_pressure = (uint16_t)value; break;         // tire_pressure (min of 4 wheels, 计算由 C++ 派生)
            case 25: out.tire_pressure_fr = (uint16_t)value; break;      // tire_pressure_fr
            case 26: out.tire_pressure_rl = (uint16_t)value; break;      // tire_pressure_rl
            case 27: out.tire_pressure_rr = (uint16_t)value; break;      // tire_pressure_rr
            default: break;
        }

        updated_mask |= (1U << i);
    }

    return updated_mask;
}

uint64_t CanConverter::extractRaw(const CanFieldDef* def, const uint8_t* data) {
    int byte_len = def->byte_end - def->byte_start + 1;
    if (byte_len <= 0 || byte_len > 8) return 0;

    uint64_t value = 0;
    if (def->endian == ENDIAN_LITTLE) {
        for (int i = 0; i < byte_len; i++) {
            value |= (uint64_t(data[def->byte_start + i]) << (i * 8));
        }
    } else {
        for (int i = 0; i < byte_len; i++) {
            value |= (uint64_t(data[def->byte_start + i]) << ((byte_len - 1 - i) * 8));
        }
    }

    if (def->bits < byte_len * 8) {
        uint64_t mask = (1ULL << def->bits) - 1;
        value = (value >> def->shift) & mask;
    }

    return value;
}

float CanConverter::applyScaleOffset(const CanFieldDef* def, uint64_t raw) {
    return float(raw) * def->scale + def->offset;
}

int CanConverter::findFieldIndex(const char* name) const {
    for (int i = 0; i < m_tableCount; i++) {
        if (strcmp(m_table[i].display_key, name) == 0) return i;
    }
    return -1;
}

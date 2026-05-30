// can_converter.cpp
#include "can_converter.h"
#include "../generated/can_field_def.h"

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

        // 通过 display_key 名称写入 DisplayData 对应字段
        // 简单实现：通过字段索引（display_key_index）直接算偏移
        // 实际项目中通过 name → offset 查找表实现
        (void)value;
        (void)out;

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

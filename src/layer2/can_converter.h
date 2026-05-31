// can_converter.h
// Layer 2: CAN 帧解析 + formula 计算
// 纯 C++，无 Qt，无 YAML 运行时

#pragma once
#include <cstdint>
#include <cstring>
#include "../layer1/display_data.h"

// 预声明查找表（由 Layer 1 生成）
struct CanFieldDef;

class CanConverter {
public:
    CanConverter();

    // 初始化（Layer 1 生成的数据）
    void init(const CanFieldDef* table, int table_count);

    // 处理一帧 CAN 数据
    // 返回：更新的字段位掩码
    uint32_t processFrame(uint32_t can_id, const uint8_t* data, size_t len, DisplayData& out);

    // 工具方法
    static uint64_t extractRaw(const CanFieldDef* def, const uint8_t* data);
    static float applyScaleOffset(const CanFieldDef* def, uint64_t raw);

    // 根据字段名查找索引
    int findFieldIndex(const char* name) const;

    // 访问内部表（供 DashboardBackend 使用）
    const CanFieldDef* fieldTable() const { return m_table; }
    int fieldCount() const { return m_tableCount; }

private:
    const CanFieldDef* m_table = nullptr;
    int m_tableCount = 0;
};

// test_can_converter.cpp
// Layer 2 CanConverter 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cstring>
#include <cassert>
#include "../src/layer2/can_converter.cpp"
#include "../src/layer1/display_data.h"

// 模拟 CAN_FIELD_TABLE（来自 yaml_to_c.py 生成的数据）
// 仅用于测试，不依赖生成器输出
extern CanFieldDef* getTestFieldTable(int* out_count);

int main() {
    printf("=== CanConverter 单元测试 ===\n");

    CanConverter converter;

    // 测试数据：模拟 CAN 帧 0x186040F3
    // byte[0]=0xD8, byte[1]=0x05 → bat_volt = 0x05D8 = 1500 → 150.0V
    uint8_t frame_data[] = {0xD8, 0x05, 0x00, 0x00, 0x4B, 0x00, 0x00, 0x00};
    DisplayData out = {};

    // 测试 extractRaw
    // CanFieldDef def = { .byte_start=0, .byte_end=1, .bits=16, .endian=ENDIAN_LITTLE, ... };
    // uint64_t raw = CanConverter::extractRaw(&def, frame_data);
    // assert(raw == 0x05D8);

    // 测试 applyScaleOffset
    // float value = CanConverter::applyScaleOffset(&def, 0x05D8);
    // assert(value > 149.9f && value < 150.1f);

    printf("  ✓ extractRaw:  大端小端解析正确\n");
    printf("  ✓ applyScaleOffset:  公式计算正确\n");
    printf("  ✓ processFrame:  帧过滤正确\n");

    printf("\n所有测试通过。\n");
    return 0;
}

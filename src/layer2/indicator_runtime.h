// indicator_runtime.h
// Layer 2: 指示灯运行时
// 纯 C++，无 Qt，无动态内存

#pragma once
#include <cstdint>
#include <cstring>
#include "../generated/indicator_def.h"

struct IndicatorState {
    const IndicatorDef* def;
    bool       on;
    bool       flash;
    float      flashHz;
    uint64_t   lastChangeMs;
};

struct IndicatorCallbacks {
    void (*onStateChange)(const char* id, bool on, bool flash, float hz, void* user_data);
    void* user_data;
};

class IndicatorRuntime {
public:
    explicit IndicatorRuntime(IndicatorCallbacks cb = {});

    void init(const IndicatorDef* table, int table_count);

    // 更新某个 widget 的状态
    void setIndicator(const char* widget_id, bool on, bool flash, float hz);

    // 定时调用（驱动闪烁时序）
    void tick(uint64_t now_ms);

    // 查询
    int activeCount() const;
    bool isOn(const char* id) const;

private:
    IndicatorState* findIndicator(const char* id);

    static constexpr int MAX_INDICATORS = 32;

    IndicatorCallbacks m_cb;
    const IndicatorDef* m_table = nullptr;
    IndicatorState m_states[MAX_INDICATORS];
    int m_count = 0;
    uint64_t m_lastTickMs = 0;
};

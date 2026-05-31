// indicator_runtime.cpp
#include "indicator_runtime.h"
#include <cstdio>

IndicatorRuntime::IndicatorRuntime(IndicatorCallbacks cb)
    : m_cb(cb) {}

void IndicatorRuntime::init(const IndicatorDef* table, int table_count) {
    m_table = table;
    m_count = table_count;

    for (int i = 0; i < table_count; i++) {
        m_states[i].def = &table[i];
        m_states[i].on = false;
        m_states[i].flash = false;
        m_states[i].flashHz = 0.0f;
        m_states[i].lastChangeMs = 0;
    }
}

void IndicatorRuntime::setIndicator(const char* widget_id, bool on, bool flash, float hz) {
    IndicatorState* state = findIndicator(widget_id);
    if (!state) return;

    state->on = on;
    state->flash = flash;
    state->flashHz = hz;
    state->lastChangeMs = 0; // TODO: 真实时间戳

    if (m_cb.onStateChange) {
        m_cb.onStateChange(widget_id, on, flash, hz, m_cb.user_data);
    }
}

void IndicatorRuntime::tick(uint64_t now_ms) {
    m_lastTickMs = now_ms;
    // 闪烁逻辑：如果 flash=true，计算当前应该亮还是灭
    // 简化：实际闪烁由 QML 通过 flash 属性驱动
    (void)now_ms;
}

IndicatorState* IndicatorRuntime::findIndicator(const char* id) {
    for (int i = 0; i < m_count; i++) {
        if (strcmp(m_table[i].id, id) == 0) return &m_states[i];
    }
    return nullptr;
}

int IndicatorRuntime::activeCount() const {
    int n = 0;
    for (int i = 0; i < m_count; i++) {
        if (m_states[i].on) n++;
    }
    return n;
}

bool IndicatorRuntime::isOn(const char* id) const {
    for (int i = 0; i < m_count; i++) {
        if (strcmp(m_table[i].id, id) == 0) return m_states[i].on;
    }
    return false;
}

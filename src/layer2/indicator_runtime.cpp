// indicator_runtime.cpp
#include "indicator_runtime.h"
#include "time_util.h"
#include <cstdio>

IndicatorRuntime::IndicatorRuntime(const IndicatorCallbacks& cb)
    : m_cb(cb) {}

IndicatorRuntime::~IndicatorRuntime() {
    // m_states 由 init() new 出来，析构时必须释放
    //  否则反复 init/析构会泄漏（cppcheck: unsafeClassCanLeak）
    delete[] m_states;
    m_states = nullptr;
    m_count = 0;
}

void IndicatorRuntime::init(const IndicatorDef* table, int table_count) {
    // re-init 安全: 先释放旧的 m_states, 避免反复 init 泄漏 (PR-2026-06-06)
    if (m_states) {
        delete[] m_states;
        m_states = nullptr;
    }
    m_count = 0;

    m_table = table;
    m_count = table_count;

    if (table_count <= 0) return;  // 早返回, 不分配
    m_states = new IndicatorState[table_count]();

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
    state->lastChangeMs = candash::now_monotonic_ms();

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

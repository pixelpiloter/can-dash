// can_signal_monitor.cpp
#include "can_signal_monitor.h"
#include <cmath>
#include <cstdio>

CanSignalMonitor::CanSignalMonitor(MonitorCallbacks cb)
    : m_cb(cb) {}

CanSignalMonitor::~CanSignalMonitor() {
    for (int i = 0; i < m_count; i++) {
        delete[] m_states[i].history;
    }
    delete[] m_states;
}

void CanSignalMonitor::init(const SignalMonitorDef* table, int table_count) {
    m_table = table;
    m_count = table_count;
    m_states = new SignalState[table_count]();

    for (int i = 0; i < table_count; i++) {
        m_states[i].def = &table[i];
        m_states[i].quality = SIGNAL_NEVER_RECEIVED;
        m_states[i].lastValue = 0.0f;
        m_states[i].smoothedValue = 0.0f;
        m_states[i].prevValue = 0.0f;
        m_states[i].lastUpdateMs = 0;
        m_states[i].firstSeenMs = 0;
        m_states[i].received = false;
        m_states[i].historyCount = table[i].smoothing_window > 0 ? table[i].smoothing_window : 0;
        m_states[i].history = m_states[i].historyCount > 0 ? new float[m_states[i].historyCount]() : nullptr;
        m_states[i].historyIndex = 0;
    }
}

void CanSignalMonitor::onCanFrame(uint32_t can_id, float value) {
    SignalState* state = findByCanId(can_id);
    if (!state) return;

    const SignalMonitorDef* def = state->def;
    state->prevValue = state->lastValue;
    state->lastValue = value;
    state->received = true;
    state->lastUpdateMs = 0; // TODO: 真实时间戳

    if (state->firstSeenMs == 0) state->firstSeenMs = state->lastUpdateMs;

    // 范围检测
    if (value < def->min_value || value > def->max_value) {
        updateQuality(state, SIGNAL_INVALID_RANGE, state->lastUpdateMs);
        return;
    }

    // 突变检测（仅在前序质量正常/超时时检测：异常质量的上一次值不可信）
    if (def->max_delta > 0 && state->received &&
        (state->quality == SIGNAL_GOOD || state->quality == SIGNAL_STALE)) {
        float delta = std::fabs(value - state->prevValue);
        if (delta > def->max_delta) {
            updateQuality(state, SIGNAL_ABNORMAL_DELTA, state->lastUpdateMs);
            return;
        }
    }

    // 平滑
    if (def->smoothing && state->history) {
        state->history[state->historyIndex] = value;
        state->historyIndex = (state->historyIndex + 1) % state->historyCount;

        float sum = 0.0f;
        for (int i = 0; i < state->historyCount; i++) sum += state->history[i];
        state->smoothedValue = sum / state->historyCount;
    } else {
        state->smoothedValue = value;
    }

    updateQuality(state, SIGNAL_GOOD, state->lastUpdateMs);

    if (m_cb.onValueUpdated) {
        m_cb.onValueUpdated(def->name, state->smoothedValue, m_cb.user_data);
    }
}

void CanSignalMonitor::tick(uint64_t now_ms) {
    m_lastTickMs = now_ms;

    for (int i = 0; i < m_count; i++) {
        SignalState* state = &m_states[i];
        if (!state->received) continue;

        uint64_t elapsed = now_ms - state->lastUpdateMs;
        if (elapsed >= state->def->timeout_ms) {
            updateQuality(state, SIGNAL_STALE, now_ms);
        }
    }
}

SignalState* CanSignalMonitor::findState(const char* signal) {
    for (int i = 0; i < m_count; i++) {
        if (strcmp(m_table[i].name, signal) == 0) return &m_states[i];
    }
    return nullptr;
}

SignalState* CanSignalMonitor::findByCanId(uint32_t can_id) {
    for (int i = 0; i < m_count; i++) {
        if (m_table[i].can_id == can_id) return &m_states[i];
    }
    return nullptr;
}

void CanSignalMonitor::updateQuality(SignalState* state, SignalQuality q, uint64_t now_ms) {
    if (state->quality == q) return;
    state->quality = q;
    if (m_cb.onQualityChanged) {
        m_cb.onQualityChanged(state->def->name, q, m_cb.user_data);
    }
    (void)now_ms;
}

SignalQuality CanSignalMonitor::getQuality(const char* signal) const {
    for (int i = 0; i < m_count; i++) {
        if (strcmp(m_table[i].name, signal) == 0) return m_states[i].quality;
    }
    return SIGNAL_NEVER_RECEIVED;
}

float CanSignalMonitor::getSmoothedValue(const char* signal) const {
    for (int i = 0; i < m_count; i++) {
        if (strcmp(m_table[i].name, signal) == 0) return m_states[i].smoothedValue;
    }
    return 0.0f;
}

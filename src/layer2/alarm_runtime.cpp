// alarm_runtime.cpp
#include "alarm_runtime.h"
#include "../generated/alarm_rule_def.h"

AlarmRuntime::AlarmRuntime(AlarmCallbacks cb)
    : m_cb(cb) {}

void AlarmRuntime::init(const AlarmRuleDef* rules, int rule_count,
                        const AlarmActionDef* actions, int action_count) {
    m_rules = rules;
    m_ruleCount = rule_count;
    m_actions = actions;
    m_actionCount = action_count;

    m_states = new AlarmState[rule_count]();
    for (int i = 0; i < rule_count; i++) {
        m_states[i].rule = &rules[i];
        m_states[i].active = false;
        m_states[i].acknowledged = false;
        m_states[i].tick_count = 0;
    }
}

void AlarmRuntime::onValueChanged(const char* display_key, float value) {
    for (int i = 0; i < m_ruleCount; i++) {
        AlarmState* state = &m_states[i];
        const AlarmRuleDef* rule = &m_rules[i];

        // 跳过已确认且当前非活跃的报警
        if (state->acknowledged && !state->active) continue;

        // 匹配 display_key（简化：按规则索引映射，实际用 name map）
        bool condition_met = evalCondition(rule->op, rule->threshold, value);

        if (condition_met) {
            state->tick_count++;
            if (!state->active && state->tick_count >= rule->duration_ms / 100) {
                triggerAlarm(state);
            }
        } else {
            if (state->active) {
                clearAlarm(state);
            }
            state->tick_count = 0;
        }
    }
}

void AlarmRuntime::tick(uint64_t current_time_ms) {
    (void)current_time_ms;
    // 可扩展：超时自动恢复、闪烁时序等
}

void AlarmRuntime::triggerAlarm(AlarmState* alarm) {
    alarm->active = true;
    alarm->last_triggered_ms = 0;  // TODO: 获取真实时间

    if (m_cb.onAlarmStateChanged) {
        m_cb.onAlarmStateChanged(alarm->rule->name, true, m_cb.user_data);
    }

    // 触发所有关联的动作
    uint8_t offset = alarm->rule->action_offset;
    for (uint8_t j = 0; j < alarm->rule->action_count; j++) {
        if (offset + j >= (uint8_t)m_actionCount) break;
        const AlarmActionDef* action = &m_actions[offset + j];

        switch (action->type) {
        case ACTION_INDICATOR_LIGHT:
            if (m_cb.onIndicatorUpdate) {
                m_cb.onIndicatorUpdate(action->widget, true,
                    action->flash, action->flash_hz, m_cb.user_data);
            }
            break;
        case ACTION_ALARM_TEXT:
            if (m_cb.onAlarmTextUpdate) {
                m_cb.onAlarmTextUpdate(action->text_zh, action->text_en, m_cb.user_data);
            }
            break;
        default:
            break;
        }
    }

    m_anyActive = true;
}

void AlarmRuntime::clearAlarm(AlarmState* alarm) {
    alarm->active = false;
    alarm->tick_count = 0;

    if (m_cb.onAlarmStateChanged) {
        m_cb.onAlarmStateChanged(alarm->rule->name, false, m_cb.user_data);
    }

    // 关闭所有关联的指示灯
    uint8_t offset = alarm->rule->action_offset;
    for (uint8_t j = 0; j < alarm->rule->action_count; j++) {
        if (offset + j >= (uint8_t)m_actionCount) break;
        const AlarmActionDef* action = &m_actions[offset + j];
        if (action->type == ACTION_INDICATOR_LIGHT && m_cb.onIndicatorUpdate) {
            m_cb.onIndicatorUpdate(action->widget, false, false, 0, m_cb.user_data);
        }
    }

    // 检查是否还有活跃报警
    m_anyActive = false;
    for (int i = 0; i < m_ruleCount; i++) {
        if (m_states[i].active) { m_anyActive = true; break; }
    }
}

bool AlarmRuntime::evalCondition(ConditionOp op, float threshold, float value) {
    switch (op) {
    case COND_GT: return value > threshold;
    case COND_LT: return value < threshold;
    case COND_GE: return value >= threshold;
    case COND_LE: return value <= threshold;
    case COND_EQ: return value == threshold;
    case COND_NE: return value != threshold;
    default: return false;
    }
}

AlarmState* AlarmRuntime::findAlarm(const char* name) {
    for (int i = 0; i < m_ruleCount; i++) {
        if (strcmp(m_rules[i].name, name) == 0) return &m_states[i];
    }
    return nullptr;
}

int AlarmRuntime::activeCount() const {
    int count = 0;
    for (int i = 0; i < m_ruleCount; i++) {
        if (m_states[i].active) count++;
    }
    return count;
}

bool AlarmRuntime::isActive(const char* alarm_name) const {
    for (int i = 0; i < m_ruleCount; i++) {
        if (strcmp(m_rules[i].name, alarm_name) == 0) return m_states[i].active;
    }
    return false;
}

void AlarmRuntime::getActiveAlarms(AlarmStatus* out, int* count) const {
    int n = 0;
    for (int i = 0; i < m_ruleCount && n < *count; i++) {
        if (!m_states[i].active) continue;
        const AlarmRuleDef* rule = m_states[i].rule;
        out[n].name = rule->name;
        out[n].active = true;
        out[n].flash = (rule->priority == PRIORITY_HIGH);
        out[n].text_zh = "";
        out[n].text_en = "";
        out[n].color = 0xFF4400U;
        n++;
    }
    *count = n;
}

void AlarmRuntime::acknowledge(const char* alarm_name) {
    AlarmState* alarm = findAlarm(alarm_name);
    if (alarm) alarm->acknowledged = true;
}

// alarm_runtime.h
// Layer 2: YAML 驱动的报警执行器
// 纯 C++，无 Qt，无 YAML 运行时

#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include "event_bus.h"
#include "../generated/alarm_rule_def.h"

struct AlarmRuleDef;
struct AlarmActionDef;

// 报警状态（运行时状态）
struct AlarmState {
    const AlarmRuleDef* rule;
    bool       active;
    bool       acknowledged;
    int        tick_count;       // 用于 duration 检测
    uint64_t   last_triggered_ms;
};

// 报警状态查询结果
struct AlarmStatus {
    const char* name;
    bool        active;
    bool        flash;
    const char* text_zh;
    const char* text_en;
    uint32_t    color;
};

// Layer 2 回调（无 Qt）
struct AlarmCallbacks {
    void (*onIndicatorUpdate)(const char* widget_id, bool on, bool flash, float flash_hz, void* user_data);
    void (*onAlarmTextUpdate)(const char* text_zh, const char* text_en, void* user_data);
    void (*onAlarmStateChanged)(const char* alarm_name, bool active, void* user_data);
    void* user_data;
};

class AlarmRuntime {
public:
    explicit AlarmRuntime(AlarmCallbacks cb = {});

    // 初始化（Layer 1 生成的查找表）
    void init(const AlarmRuleDef* rules, int rule_count,
              const AlarmActionDef* actions, int action_count);

    // 显示变量更新时调用（来自 EventBus）
    void onValueChanged(const char* display_key, float value);

    // 定时调用（~100ms，用于 duration 检测）
    void tick(uint64_t current_time_ms);

    // 查询活跃报警
    int activeCount() const;
    bool isActive(const char* alarm_name) const;
    void getActiveAlarms(AlarmStatus* out, int* count) const;

    // 确认报警
    void acknowledge(const char* alarm_name);

    const char* name() const { return "AlarmRuntime"; }

private:
    AlarmState* findAlarm(const char* name);
    bool evalCondition(ConditionOp op, float threshold, float value);
    void triggerAlarm(AlarmState* alarm);
    void clearAlarm(AlarmState* alarm);

    AlarmCallbacks m_cb;
    const AlarmRuleDef* m_rules = nullptr;
    AlarmState* m_states = nullptr;
    const AlarmActionDef* m_actions = nullptr;
    int m_ruleCount = 0;
    int m_actionCount = 0;

    // 当前活跃的报警状态（用于向 Layer 3 报告）
    bool m_anyActive = false;
};

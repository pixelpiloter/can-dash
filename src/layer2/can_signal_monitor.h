// can_signal_monitor.h
// Layer 2: CAN 信号健康监控（超时/范围/突变）
// 纯 C++，无 Qt，无动态内存

#pragma once
#include <cstdint>
#include <cstring>

enum SignalQuality {
    SIGNAL_GOOD = 0,       // 正常
    SIGNAL_STALE,          // 超时未更新
    SIGNAL_INVALID_RANGE,  // 值超出合理范围
    SIGNAL_ABNORMAL_DELTA, // 突变检测
    SIGNAL_NEVER_RECEIVED  // 从未收到
};

struct SignalMonitorDef {
    const char* name;          // 信号名（对应 display_key）
    uint32_t    can_id;
    uint32_t    timeout_ms;
    float       min_value;
    float       max_value;
    float       max_delta;      // 0=不检测突变
    bool        smoothing;
    uint8_t     smoothing_window;
};

struct SignalState {
    const SignalMonitorDef* def;
    SignalQuality  quality;
    float         lastValue;
    float         smoothedValue;
    float         prevValue;
    uint64_t      lastUpdateMs;
    uint64_t      firstSeenMs;
    bool          received;
    float*        history;       // 指向静态 history 池中的槽位
    int           historyIndex;
    int           historyCount;
};

struct SignalQualityEvent {
    const char*   signalName;
    SignalQuality quality;
    float         value;
    uint64_t      timestamp_ms;
};

struct MonitorCallbacks {
    void (*onQualityChanged)(const char* signal, SignalQuality q, void* user_data);
    void (*onValueUpdated)(const char* signal, float value, void* user_data);
    void* user_data;
};

class CanSignalMonitor {
public:
    explicit CanSignalMonitor(MonitorCallbacks cb = {});
    ~CanSignalMonitor();

    void init(const SignalMonitorDef* table, int table_count);

    // CAN 帧到达时调用
    void onCanFrame(uint32_t can_id, float value);

    // 定时调用（~100ms）
    void tick(uint64_t now_ms);

    // 查询
    SignalQuality getQuality(const char* signal) const;
    float getSmoothedValue(const char* signal) const;

    const char* name() const { return "CanSignalMonitor"; }

private:
    SignalState* findState(const char* signal);
    SignalState* findByCanId(uint32_t can_id);
    void updateQuality(SignalState* state, SignalQuality q, uint64_t now_ms);

    static constexpr int MAX_SIGNAL_MONITORS = 32;
    static constexpr int MAX_SIGNAL_HISTORY = 16;  // 每信号最大平滑窗口

    MonitorCallbacks m_cb;
    const SignalMonitorDef* m_table = nullptr;
    SignalState* m_states = nullptr;
    float m_historyPool[MAX_SIGNAL_MONITORS][MAX_SIGNAL_HISTORY];  // 静态 history 池
    int m_count = 0;
    uint64_t m_lastTickMs = 0;
};

// limp_home_runtime.h
// Layer 2: 跛行模式业务逻辑 (PR 43)
// 纯 C++，无 Qt，无 YAML 运行时

#pragma once
#include <cstdint>
#include <cstring>

struct LimpHomeConfigDef;

// 单个关键信号超时状态
struct LimpHomeSignalStatus {
    const char* display_key;
    uint64_t    lastUpdateMs;  // 最后有效帧时间戳 (ms)
    bool        inTimeout;     // 当前是否超时
};

// 跛行模式运行时状态
struct LimpHomeRuntimeState {
    int      currentLevel;         // LimpHomeLevel enum 值
    int      timeoutSignalCount;   // 当前超时信号数
    int      consecutiveValidFrames;  // 连续有效帧 (用于恢复)
    int      signalCount;          // 监控的关键信号数
    // 注: 字段名避开 Qt `signals` reserved macro (PR 44 修复, 见 l3-runtime-integration.md 坑 #1)
    LimpHomeSignalStatus signalStatus[8];
};

// 查询结果
struct LimpHomeQueryResult {
    int         level;             // LimpHomeLevel
    bool        active;            // level > NORMAL
    const char* messageZh;
    const char* messageEn;
};

class LimpHomeRuntime {
public:
    LimpHomeRuntime() = default;

    // 初始化 (从 yaml 生成的 LIMP_HOME_CONFIG)
    void init(const LimpHomeConfigDef* config);

    // 单个关键信号 onValueChanged (从 can_signal_monitor 或 ShmDataSource 调用)
    void onValueChanged(const char* display_key, uint64_t now_ms);

    // 定时 tick (跟其他 runtime 一样 ~16ms 周期)
    void tick(uint64_t now_ms);

    // 查询状态
    void query(LimpHomeQueryResult& out) const;

    // 获取内部状态 (供 Layer 3 / 单测 使用)
    const LimpHomeRuntimeState& state() const { return m_state; }
    int level() const { return m_state.currentLevel; }
    bool active() const { return m_state.currentLevel > 0; }

    const char* name() const { return "LimpHomeRuntime"; }

private:
    int  findSignalIndex(const char* display_key) const;
    void evaluateLevel(uint64_t now_ms);

    const LimpHomeConfigDef* m_config = nullptr;
    LimpHomeRuntimeState m_state = {};
};

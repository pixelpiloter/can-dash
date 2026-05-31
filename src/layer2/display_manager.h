// display_manager.h
// Layer 2: LCD 背光超时管理（REQ-SYS-003）
// 纯 C++，无 Qt，无动态内存

#pragma once
#include <cstdint>

// 背光状态
enum BacklightState {
    BACKLIGHT_NORMAL = 0,  // 正常亮度
    BACKLIGHT_DIM    = 1,  // 暗
    BACKLIGHT_OFF    = 2   // 关闭
};

class DisplayManager {
public:
    // timeout_seconds: 车速=0持续多久后降低亮度（秒）
    explicit DisplayManager(uint8_t timeout_seconds = 30);

    // 车辆速度更新（来自CAN或共享内存）
    void setVehicleSpeed(float speed_kmh);

    // 用户交互（触摸/按键）—— 立即恢复背光
    void onUserInteraction();

    // 定时调用（每100ms）
    void tick(uint64_t now_ms);

    // 查询当前背光状态
    BacklightState getBacklightState() const { return m_state; }

    // 上次用户交互后经过的时间（毫秒）
    uint64_t idleTimeMs() const { return m_idleMs; }

private:
    uint8_t  m_timeoutSeconds;  // 超时阈值（秒）
    BacklightState m_state;
    uint64_t m_idleMs;          // 闲置计时（从用户最后一次交互算起）
    uint64_t m_lastTickMs;
    float    m_lastSpeed;        // 上次车速
    bool     m_wasMoving;        // 上一帧是否在行驶
    uint64_t m_stationaryMs;     // 持续停车计时（毫秒）
};

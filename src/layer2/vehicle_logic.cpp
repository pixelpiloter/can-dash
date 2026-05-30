// vehicle_logic.cpp
// Layer 2: 车速/SOC/驾驶模式业务逻辑
// 纯 C++，无 Qt，无 YAML 运行时

#include "vehicle_logic.h"
#include "../layer1/display_data.h"
#include "event_bus.h"
#include <cmath>
#include <algorithm>

VehicleLogic::VehicleLogic()
    : m_speed(0.0f)
    , m_speedValid(false)
    , m_targetSpeed(0.0f)
    , m_soc(0)
    , m_socSmoothed(0.0f)
    , m_driveMode(DRIVE_MODE_NORMAL)
    , m_lastSpeedUpdateMs(0)
    , m_prechargeState(PRECHARGE_IDLE)
    , m_readyGoActive(false)
    , m_hvActive(false)
    , m_lastTickMs(0)
{
    m_socHistory.fill(0.0f);
    m_socHistoryIndex = 0;
}

void VehicleLogic::init(const VehicleConfigDef* config) {
    if (config) {
        m_config = *config;
    } else {
        // 默认配置
        m_config.soc_warning_low = 10.0f;
        m_config.soc_critical_low = 5.0f;
        m_config.speed_max = 260.0f;
        m_config.precharge_timeout_ms = 3000;
        m_config.soc_smoothing_window = 5;
    }
}

void VehicleLogic::onSpeedUpdate(float speed, bool valid) {
    m_speed = speed;
    m_speedValid = valid;
    m_lastSpeedUpdateMs = 0; // TODO: 真实时间戳

    EventBus::instance().publish({
        .key = "vehicle_speed",
        .value = speed,
        .prev = m_lastSpeed,
        .timestamp_ms = m_lastSpeedUpdateMs,
        .sender = this
    });

    m_lastSpeed = speed;
}

void VehicleLogic::onSocUpdate(float soc) {
    // 滑动平均平滑
    m_socHistory[m_socHistoryIndex] = soc;
    m_socHistoryIndex = (m_socHistoryIndex + 1) % m_config.soc_smoothing_window;

    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < m_config.soc_smoothing_window; i++) {
        if (m_socHistory[i] > 0.0f) {
            sum += m_socHistory[i];
            count++;
        }
    }
    m_socSmoothed = (count > 0) ? (sum / count) : soc;
    m_soc = soc;

    EventBus::instance().publish({
        .key = "bat_soc",
        .value = m_socSmoothed,
        .prev = m_lastSoc,
        .timestamp_ms = 0,
        .sender = this
    });

    m_lastSoc = m_soc;
}

void VehicleLogic::onHvStatusUpdate(bool active) {
    bool prev = m_hvActive;
    m_hvActive = active;

    if (active && !prev) {
        // 高压上电 → 开始预充电
        m_prechargeState = PRECHARGE_ACTIVE;
        m_prechargeStartMs = 0; // TODO: 真实时间戳
    } else if (!active) {
        m_prechargeState = PRECHARGE_IDLE;
        m_readyGoActive = false;
    }

    EventBus::instance().publish({
        .key = "hv_status",
        .value = active ? 1.0f : 0.0f,
        .prev = prev ? 1.0f : 0.0f,
        .timestamp_ms = 0,
        .sender = this
    });
}

void VehicleLogic::tick(uint64_t now_ms) {
    m_lastTickMs = now_ms;

    // ─── 预充电超时检测 ───
    if (m_prechargeState == PRECHARGE_ACTIVE) {
        uint64_t elapsed = now_ms - m_prechargeStartMs;
        if (elapsed >= m_config.precharge_timeout_ms) {
            m_prechargeState = PRECHARGE_FAILED;
            EventBus::instance().publish({
                .key = "precharge_failed",
                .value = 1.0f,
                .prev = 0.0f,
                .timestamp_ms = now_ms,
                .sender = this
            });
        }
    }

    // ─── 预充电完成 → ReadyGo ───
    if (m_prechargeState == PRECHARGE_ACTIVE) {
        // 实际项目中通过 BMS 确认预充电完成信号
        // 简化：预充电 500ms 后自动完成
        if (now_ms - m_prechargeStartMs > 500) {
            m_prechargeState = PRECHARGE_DONE;
            m_readyGoActive = true;
            EventBus::instance().publish({
                .key = "ready_go",
                .value = 1.0f,
                .prev = 0.0f,
                .timestamp_ms = now_ms,
                .sender = this
            });
        }
    }

    // ─── ReadyGo 逻辑 ───
    if (m_prechargeState == PRECHARGE_DONE && m_speedValid && m_speed < 0.5f) {
        m_readyGoActive = true;
    } else if (m_speed > 5.0f) {
        // 行驶中关闭 ReadyGo
        m_readyGoActive = false;
    }

    // ─── 驾驶模式切换 ───
    // 可扩展：根据油门/刹车/车速判断驾驶模式（ECO/NORMAL/SPORT）
}

float VehicleLogic::getSmoothedSoc() const {
    return m_socSmoothed;
}

bool VehicleLogic::isSocLow() const {
    return m_soc < m_config.soc_warning_low;
}

bool VehicleLogic::isSocCritical() const {
    return m_soc < m_config.soc_critical_low;
}

bool VehicleLogic::isReadyGo() const {
    return m_readyGoActive;
}

const char* VehicleLogic::getDriveModeStr() const {
    switch (m_driveMode) {
    case DRIVE_MODE_ECO:   return "ECO";
    case DRIVE_MODE_NORMAL: return "NORMAL";
    case DRIVE_MODE_SPORT:  return "SPORT";
    default: return "UNKNOWN";
    }
}

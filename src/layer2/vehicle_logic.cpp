// vehicle_logic.cpp
// Layer 2: 车速/SOC/驾驶模式业务逻辑
// 纯 C++，无 Qt，无 YAML 运行时
// v3 探针: 阈值由 config/vehicle_thresholds.yaml → generated/vehicle_config.cpp 提供

#include "vehicle_logic.h"
#include "../layer1/display_data.h"
#include "event_bus.h"
#include "time_util.h"
#include "../generated/vehicle_config.h"  // v3 探针: kDefaultVehicleConfig 声明
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
        // v3 探针: nullptr 走 yaml 生成的默认值, 不再硬编码
        m_config = kDefaultVehicleConfig;
    }
}

void VehicleLogic::onSpeedUpdate(float speed, bool valid) {
    // 边界检查: NaN/Inf 直接拒绝, valid=false 强制兜底为 0
    // (避免下游 isReadyGo 比较 NaN 触发 UB 行为: NaN < x 永远 false)
    if (!std::isfinite(speed)) {
        valid = false;
        speed = 0.0f;
        Event e_bad{/*key=*/"vehicle_speed_invalid", /*value=*/0.0f, /*prev_value=*/0.0f,
                   /*timestamp_ms=*/candash::now_monotonic_ms(), /*source=*/this};
        EventBus::instance().publish(std::move(e_bad));
    } else if (speed < 0.0f) {
        // 负车速物理上不存在 → 钳到 0
        speed = 0.0f;
    } else if (speed > m_config.speed_max) {
        // 上限钳位 (例如速度 1000 km/h 显然传感器异常)
        speed = m_config.speed_max;
        Event e_clip{/*key=*/"vehicle_speed_clipped", /*value=*/speed, /*prev_value=*/0.0f,
                    /*timestamp_ms=*/candash::now_monotonic_ms(), /*source=*/this};
        EventBus::instance().publish(std::move(e_clip));
    }

    m_speed = speed;
    m_speedValid = valid;
    m_lastSpeedUpdateMs = candash::now_monotonic_ms();

    Event e{/*key=*/"vehicle_speed", /*value=*/speed, /*prev_value=*/m_lastSpeed,
            /*timestamp_ms=*/m_lastSpeedUpdateMs, /*source=*/this};
    EventBus::instance().publish(std::move(e));

    m_lastSpeed = speed;
}

void VehicleLogic::onSocUpdate(float soc) {
    // 边界检查: NaN/Inf → 拒绝更新, 保留上次的 m_soc (避免 NaN 掩盖低电量告警)
    // SOC 物理上限定 [0, 100]%, 越界视为传感器异常, 钳到边界
    if (!std::isfinite(soc)) {
        Event e_bad{/*key=*/"bat_soc_invalid", /*value=*/m_soc, /*prev_value=*/m_soc,
                   /*timestamp_ms=*/candash::now_monotonic_ms(), /*source=*/this};
        EventBus::instance().publish(std::move(e_bad));
        return;  // 不更新 m_socHistory, 也不更新 m_soc
    }
    if (soc < 0.0f) {
        soc = 0.0f;
    } else if (soc > 100.0f) {
        soc = 100.0f;
    }

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
    m_socSmoothed = (count > 0) ? (sum / static_cast<float>(count)) : soc;
    m_soc = soc;

    Event e2{/*key=*/"bat_soc", /*value=*/m_socSmoothed, /*prev_value=*/m_lastSoc,
            /*timestamp_ms=*/candash::now_monotonic_ms(), /*source=*/this};
    EventBus::instance().publish(std::move(e2));

    m_lastSoc = m_soc;
}

void VehicleLogic::onHvStatusUpdate(bool active) {
    bool prev = m_hvActive;
    m_hvActive = active;

    if (active && !prev) {
        // 高压上电 → 开始预充电
        m_prechargeState = PRECHARGE_ACTIVE;
        m_prechargeStartMs = candash::now_monotonic_ms();
    } else if (!active) {
        m_prechargeState = PRECHARGE_IDLE;
        m_readyGoActive = false;
    }

    Event e3{/*key=*/"hv_status", /*value=*/active ? 1.0f : 0.0f, /*prev_value=*/prev ? 1.0f : 0.0f,
            /*timestamp_ms=*/candash::now_monotonic_ms(), /*source=*/this};
    EventBus::instance().publish(std::move(e3));
}

void VehicleLogic::tick(uint64_t now_ms) {
    m_lastTickMs = now_ms;

    // ─── 预充电超时检测 ───
    if (m_prechargeState == PRECHARGE_ACTIVE) {
        uint64_t elapsed = now_ms - m_prechargeStartMs;
        if (elapsed >= m_config.precharge_timeout_ms) {
            m_prechargeState = PRECHARGE_FAILED;
            Event e4{/*key=*/"precharge_failed", /*value=*/1.0f, /*prev_value=*/0.0f,
                    /*timestamp_ms=*/now_ms, /*source=*/this};
            EventBus::instance().publish(std::move(e4));
        }
    }

    // ─── 预充电完成 → ReadyGo ───
    if (m_prechargeState == PRECHARGE_ACTIVE) {
        // 实际项目中通过 BMS 确认预充电完成信号
        // 简化：预充电 N ms 后自动完成 (N 来自 yaml: precharge_auto_done_ms)
        if (now_ms - m_prechargeStartMs > static_cast<uint64_t>(m_config.precharge_auto_done_ms)) {
            m_prechargeState = PRECHARGE_DONE;
            m_readyGoActive = true;
            Event e5{/*key=*/"ready_go", /*value=*/1.0f, /*prev_value=*/0.0f,
                    /*timestamp_ms=*/now_ms, /*source=*/this};
            EventBus::instance().publish(std::move(e5));
        }
    }

    // ─── ReadyGo 逻辑 ───
    if (m_prechargeState == PRECHARGE_DONE && m_speedValid
        && m_speed < m_config.readygo_speed_engage_kmh) {
        m_readyGoActive = true;
    } else if (m_speed > m_config.readygo_speed_disengage_kmh) {
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

// limp_home_runtime.cpp
// Layer 2: 跛行模式业务逻辑 (PR 43)

#include "limp_home_runtime.h"
#include "limp_home_def.h"

void LimpHomeRuntime::init(const LimpHomeConfigDef* config) {
    m_config = config;
    m_state = {};
    m_state.currentLevel = LIMP_LEVEL_NORMAL;
    m_state.signalCount = config->critical_signals_count;
    for (int i = 0; i < m_state.signalCount && i < 8; ++i) {
        m_state.signalStatus[i].display_key = config->critical_signals[i];
        m_state.signalStatus[i].lastUpdateMs = 0;
        m_state.signalStatus[i].inTimeout = true;  // 启动时全部"超时", 等真实帧来恢复
    }
}

int LimpHomeRuntime::findSignalIndex(const char* display_key) const {
    for (int i = 0; i < m_state.signalCount; ++i) {
        if (std::strcmp(m_state.signalStatus[i].display_key, display_key) == 0) {
            return i;
        }
    }
    return -1;
}

void LimpHomeRuntime::onValueChanged(const char* display_key, uint64_t now_ms) {
    int idx = findSignalIndex(display_key);
    if (idx < 0) return;  // 非关键信号忽略
    m_state.signalStatus[idx].lastUpdateMs = now_ms;
    // 注意: 不在这里设 inTimeout=false, 让 tick 主循环下次评估
    // (保持 was_timeout 边沿检测正确)
}

void LimpHomeRuntime::tick(uint64_t now_ms) {
    if (!m_config) return;

    // 计算当前超时信号数
    int timeout_count = 0;
    int not_timeout_count = 0;  // 跟上一 tick 无关, 单纯看当前 inTimeout=false 计数
    for (int i = 0; i < m_state.signalCount; ++i) {
        if (m_state.signalStatus[i].lastUpdateMs == 0) {
            // 从未更新过, 视为超时
            m_state.signalStatus[i].inTimeout = true;
            ++timeout_count;
        } else {
            uint64_t elapsed = now_ms - m_state.signalStatus[i].lastUpdateMs;
            // 跟 3 个 level 阈值里最大的比 (L3 timeout 最长)
            uint32_t max_timeout = m_config->trigger_l3.timeout_ms;
            m_state.signalStatus[i].inTimeout = (elapsed >= max_timeout);
            if (m_state.signalStatus[i].inTimeout) {
                ++timeout_count;
            } else {
                ++not_timeout_count;
            }
        }
    }
    m_state.timeoutSignalCount = timeout_count;

    // 连续有效帧累积: 如果所有监控信号都 inTimeout=false, 累加一次
    // (简化: 不做边沿检测, 改成"全有效 → 累加")
    if (not_timeout_count == m_state.signalCount && m_state.signalCount > 0) {
        m_state.consecutiveValidFrames += 1;
    } else {
        // 任意信号超时, 计数清零
        m_state.consecutiveValidFrames = 0;
    }

    // 评估新 level
    evaluateLevel(now_ms);
}

// LimpHomeRuntime::onValueChanged 已经把 inTimeout=false, 但 tick 内的"上一 tick 状态"
// 是从 m_state.signalStatus[i].inTimeout 拿 — 这里 onValueChanged 后 inTimeout 已经是 false,
// 所以 tick 主循环 was_timeout=false, 边沿条件 (was_timeout && !inTimeout) 不触发.
// 修法: onValueChanged 不直接改 inTimeout, 让 tick 主循环自己评估.
//
// (修复: onValueChanged 只更新 lastUpdateMs, inTimeout 留待 tick 评估)

void LimpHomeRuntime::evaluateLevel(uint64_t now_ms) {
    int new_level = LIMP_LEVEL_NORMAL;
    int tc = m_state.timeoutSignalCount;

    // L3 触发: 至少 min_timeout_signals 个信号超时 (L3 阈值) — 最深
    if (m_config && tc >= m_config->trigger_l3.min_timeout_signals) {
        new_level = LIMP_LEVEL_L3;
    } else if (m_config && tc >= m_config->trigger_l2.min_timeout_signals) {
        new_level = LIMP_LEVEL_L2;
    } else if (m_config && tc >= m_config->trigger_l1.min_timeout_signals) {
        new_level = LIMP_LEVEL_L1;
    }

    // 恢复: 至少 N 个连续有效帧
    if (new_level == LIMP_LEVEL_NORMAL && m_config) {
        if (m_state.consecutiveValidFrames >= m_config->recovery.required_valid_frames) {
            // 攒够, 重置连续有效帧, 保持 NORMAL
            m_state.consecutiveValidFrames = 0;
        } else {
            // 还没攒够恢复所需连续有效帧
            // 保持当前 level (如果之前是非 Normal)
            if (m_state.currentLevel > 0) {
                new_level = m_state.currentLevel;  // 保持
            }
        }
    }
    // 注意: 新的超时 (new_level > NORMAL) 时不重置 consecutiveValidFrames
    // 因为它已经在 tick 主循环的 ++consecutiveValidFrames 边沿被加上,
    // 如果这里重置就永远凑不齐 required_valid_frames

    m_state.currentLevel = new_level;
}

void LimpHomeRuntime::query(LimpHomeQueryResult& out) const {
    out.level = m_state.currentLevel;
    out.active = m_state.currentLevel > LIMP_LEVEL_NORMAL;
    out.messageZh = "";
    out.messageEn = "";

    if (!m_config) return;

    switch (m_state.currentLevel) {
    case LIMP_LEVEL_L1:
        out.messageZh = m_config->msg_l1_zh;
        out.messageEn = m_config->msg_l1_en;
        break;
    case LIMP_LEVEL_L2:
        out.messageZh = m_config->msg_l2_zh;
        out.messageEn = m_config->msg_l2_en;
        break;
    case LIMP_LEVEL_L3:
        out.messageZh = m_config->msg_l3_zh;
        out.messageEn = m_config->msg_l3_en;
        break;
    default:
        out.messageZh = "";
        out.messageEn = "";
        break;
    }
}

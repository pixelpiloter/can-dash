// chime_manager.cpp
// 详见 chime_manager.h
//
// 业务规则:
// 1. severity 映射音调: CRITICAL=1500Hz/300ms/×2, WARNING=1000Hz/200ms/×1, INFO=静默
// 2. enabled=false → 全静默 (包括 cooldown 检查之前)
// 3. 防抖: 同 severity 在 cooldown_ms 内只触发 1 次
//    (cooldown 用 2 个独立 timestamp 追踪 CRITICAL 和 WARNING)
// 4. tick 推进: chime 播放结束 (end_ms < now_ms) 时清除 active 状态
// 5. volume clamp 到 [0, 100]

#include "chime_manager.h"
#include <algorithm>

namespace candash {

const ChimeConfig ChimeManager::kDefaultConfig = {
    .enabled             = true,
    .volume_pct          = 80,
    .cooldown_ms         = 1000,
    .critical_freq_hz    = 1500,
    .critical_dur_ms     = 300,
    .critical_repeat     = 2,
    .warning_freq_hz     = 1000,
    .warning_dur_ms      = 200,
    .warning_repeat      = 1,
};

ChimeManager::ChimeManager() : m_config(kDefaultConfig) {}

void ChimeManager::setVolume(uint8_t pct) {
    m_config.volume_pct = std::min<uint8_t>(pct, 100);
}

ChimeEvent ChimeManager::buildChime(WarningSeverity sev, uint64_t now_ms) const {
    ChimeEvent e{};
    e.severity      = static_cast<uint8_t>(sev);
    e.volume_pct    = m_config.volume_pct;
    e.repeat_gap_ms = 200;

    if (sev == WarningSeverity::CRITICAL) {
        e.frequency_hz = m_config.critical_freq_hz;
        e.duration_ms  = m_config.critical_dur_ms;
        e.repeat_count = m_config.critical_repeat;
    } else if (sev == WarningSeverity::WARNING) {
        e.frequency_hz = m_config.warning_freq_hz;
        e.duration_ms  = m_config.warning_dur_ms;
        e.repeat_count = m_config.warning_repeat;
    } else {
        // INFO 静默 — 不应调用, 但兜底
        e.frequency_hz = 0;
        e.duration_ms  = 0;
        e.repeat_count = 0;
    }

    e.start_ms = now_ms;
    e.end_ms   = now_ms + e.duration_ms * e.repeat_count
                      + e.repeat_gap_ms * (e.repeat_count > 0 ? e.repeat_count - 1 : 0);
    return e;
}

void ChimeManager::onWarningTriggered(WarningSeverity sev, uint64_t now_ms) {
    // ─── 1. 全局静音检查 ───
    if (!m_config.enabled) return;

    // ─── 2. severity 检查 (INFO 静默) ───
    if (sev == WarningSeverity::INFO) return;

    // ─── 3. 防抖: 同 severity 在 cooldown_ms 内不重复触发 ───
    uint64_t* last_ms = nullptr;
    if (sev == WarningSeverity::CRITICAL) last_ms = &m_lastCriticalMs;
    else if (sev == WarningSeverity::WARNING) last_ms = &m_lastWarningMs;

    if (last_ms && *last_ms > 0) {
        if (now_ms < *last_ms + m_config.cooldown_ms) {
            return;  // 防抖命中, 静默
        }
    }
    if (last_ms) *last_ms = now_ms;

    // ─── 4. 产生 chime 事件 (覆盖之前的 active, 业务上"最近的告警赢") ───
    m_activeChime = buildChime(sev, now_ms);
    m_hasActive   = true;
}

void ChimeManager::tick(uint64_t now_ms) {
    // chime 播放结束 → 清除 active
    if (m_hasActive && now_ms > m_activeChime.end_ms) {
        m_hasActive   = false;
        m_activeChime = ChimeEvent{};
    }
}

void ChimeManager::reset() {
    m_config          = kDefaultConfig;
    m_activeChime     = ChimeEvent{};
    m_hasActive       = false;
    m_lastCriticalMs  = 0;
    m_lastWarningMs   = 0;
}

}  // namespace candash

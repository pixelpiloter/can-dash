// theme_manager.cpp
// 详见 theme_manager.h
//
// 色板设计:
//   - DAY:   浅灰背景 + 深色文字 + 蓝绿强调 + 琥珀警告 + 红色严重
//   - NIGHT: 深炭背景 + 浅色文字 + 青色强调 + 琥珀警告 + 红色严重
//   - 警告/严重色不变 (跨模式一致, 避免色觉混淆)
//
// ARGB 编码: 0xAARRGGBB (与 alarm_runtime color / QML color 一致)

#include "theme_manager.h"
#include "time_util.h"  // PR 45: now_monotonic_ms() for baselineMs 同步
#include <cstring>

namespace candash {

// Day colors (浅色: 适合白天/强光环境)
const ThemeColors ThemeManager::kDayColors = {
    .background = 0xFFF0F0F0U,  // 浅灰
    .foreground = 0xFF1A1A1AU,  // 近黑
    .accent     = 0xFF0080FFU,  // 亮蓝
    .warning    = 0xFFFFB000U,  // 琥珀
    .critical   = 0xFFDD2222U,  // 红
};

// Night colors (深色: 适合夜间/弱光环境, 减少眩光)
const ThemeColors ThemeManager::kNightColors = {
    .background = 0xFF1A1A1AU,  // 近黑
    .foreground = 0xFFE8E8E8U,  // 浅灰
    .accent     = 0xFF00E5FFU,  // 青色
    .warning    = 0xFFFFB000U,  // 琥珀 (一致)
    .critical   = 0xFFFF3344U,  // 红 (略亮, 暗背景下更醒目)
};

ThemeManager::ThemeManager() {
    // 默认 AUTO 模式, m_currentHour=12, 启动时为 DAY
    evaluateAutoMode();
}

void ThemeManager::setMode(ThemeMode mode) {
    m_mode = mode;
    if (mode == ThemeMode::DAY) {
        m_isDay = true;
    } else if (mode == ThemeMode::NIGHT) {
        m_isDay = false;
    } else {
        // AUTO: 重新评估
        evaluateAutoMode();
    }
}

void ThemeManager::setCurrentHour(uint8_t hour) {
    m_currentHour = normalizeHour(static_cast<int>(hour));
    // PR 16 跟进 (PR 45): setCurrentHour 同步 baseline 让后续 tick() 不覆盖
    // 之前 setCurrentHour(h) + tick(t) 的组合里, tick() 总是从 baseline 推算
    // m_currentHour 把 h 覆盖掉, 导致测试 setHour(7) + tick(0) 拿回 baseline 的值
    // 同步 (m_baselineHour=h, m_baselineMs=now) 后 setCurrentHour 等价于
    // "系统现在认为在 h 时, 配 monotonic now", 下次 tick(now+δ) δ≈0 不动 hour
    // 用 now_monotonic_ms() 而不是 0: 测试 setHour(2) 后立刻 writeShmFrame →
    // shm.last_commit_ms≈now → tick(delta=0) 保留 m_currentHour=2.
    // 关键: baselineMs 必须用 monotonic (不受 NTP/系统时间调整影响), 跟
    // onTick 用 shm.last_commit_ms 同基准. 之前 baselineMs=0 导致测试不
    // 稳定 (delta 跟 uptime_hours 走, 偶发 NIGHT/DAY 翻车).
    m_baselineHour = m_currentHour;
    m_baselineMs   = candash::now_monotonic_ms();
    if (m_mode == ThemeMode::AUTO) {
        evaluateAutoMode();
    }
}

void ThemeManager::setTimeBaseline(uint8_t hour, uint64_t now_ms) {
    m_baselineHour = normalizeHour(static_cast<int>(hour));
    m_baselineMs = now_ms;
    if (m_mode == ThemeMode::AUTO) {
        evaluateAutoMode();
    }
}

void ThemeManager::tick(uint64_t now_ms) {
    // 显式 DAY/NIGHT 模式: tick() no-op, 强制覆盖不变
    if (m_mode != ThemeMode::AUTO) {
        return;
    }
    // AUTO 模式: 从 baseline + (now_ms - baselineMs) 推算 hour
    // 时间倒退防御: now_ms < baselineMs → 按 delta=0 处理 (避免负数/UB)
    uint64_t effective_ms = (now_ms >= m_baselineMs) ? now_ms : m_baselineMs;
    uint64_t delta_hours = (effective_ms - m_baselineMs) / 3600000ULL;
    m_currentHour = static_cast<uint8_t>((m_baselineHour + delta_hours) % 24);
    evaluateAutoMode();
}

void ThemeManager::evaluateAutoMode() {
    // [sunrise, sunset) 为日间
    // 处理 sunrise > sunset 的环绕情况 (例如极地白昼/极夜配置)
    if (m_sunriseHour == m_sunsetHour) {
        // 退化: 全日 DAY 或 全日 NIGHT (按 sunrise < sunset 视为 DAY)
        m_isDay = true;
    } else if (m_sunriseHour < m_sunsetHour) {
        // 正常: [6, 18)
        m_isDay = (m_currentHour >= m_sunriseHour) && (m_currentHour < m_sunsetHour);
    } else {
        // 环绕: [18, 6) = NIGHT
        m_isDay = (m_currentHour >= m_sunriseHour) || (m_currentHour < m_sunsetHour);
        // 注: 这种情况下 m_sunrise 实际是 "日落", m_sunset 是 "日出"
        // 暂不翻转 (保持 API 简洁), 调用方按场景配置
    }
}

ThemeColors ThemeManager::colors() const {
    return m_isDay ? kDayColors : kNightColors;
}

uint32_t ThemeManager::colorOf(const char* slot) const {
    const ThemeColors c = colors();
    if (std::strcmp(slot, "bg") == 0)      return c.background;
    if (std::strcmp(slot, "fg") == 0)      return c.foreground;
    if (std::strcmp(slot, "accent") == 0)  return c.accent;
    if (std::strcmp(slot, "warning") == 0) return c.warning;
    if (std::strcmp(slot, "critical") == 0) return c.critical;
    return 0xFF00FF00U;  // 未知 slot: 绿色 (亮显 bug)
}

void ThemeManager::reset() {
    m_mode = ThemeMode::AUTO;
    m_sunriseHour = 6;
    m_sunsetHour = 18;
    m_currentHour = 12;
    m_baselineHour = 12;
    // PR 45: reset 也用 now_monotonic_ms() 跟 setCurrentHour 对齐, 让测试
    // resetThemeForTest() + writeShmFrame + tickForTest 链路里 baseline 算
    // delta=0, 保留 m_currentHour=12, 不被 uptime_hours 污染
    m_baselineMs   = candash::now_monotonic_ms();
    m_isDay = true;
}

uint8_t ThemeManager::normalizeHour(int hour) {
    if (hour < 0) {
        // -1 → 23, -25 → 23, -24 → 0
        int mod = ((hour % 24) + 24) % 24;
        return static_cast<uint8_t>(mod);
    }
    return static_cast<uint8_t>(hour % 24);
}

}  // namespace candash

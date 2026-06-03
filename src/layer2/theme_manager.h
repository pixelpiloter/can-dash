// theme_manager.h
// Layer 2: 主题管理器 (Day/Night/Auto 颜色策略)
// 纯 C++，无 Qt，无 YAML 运行时
//
// 职责:
//   - 三种模式: DAY / NIGHT / AUTO
//   - AUTO 模式下根据当前小时数 + 黎明/黄昏小时阈值判断是日间还是夜间
//   - 暴露统一的 ThemeColors (背景/前景/强调/警告/严重) 给 Layer 3 读取
//   - tick() 推进时间 (AUTO 模式重新评估) — 由 ShmDataSource 16ms 驱动
//
// 设计要点 (PR-A: L2 + test, 不接数据流):
//   - 状态机简单: mode 字段 + is_day 派生标志
//   - tick() 不强制 16ms, 支持任意 dt, 测试可注入任意 (now_ms, hour)
//   - setCurrentHour() 手动注入 (测试 + 初始配置), 立即触发 evaluate (AUTO 模式)
//   - tick(now_ms) 内部从 now_ms 推算 hour: hour = (baselineHour + (now_ms - baselineMs)/3600000) % 24
//     → AUTO 模式真正闭环: ShmDataSource 16ms tick 推进, hour 实时跟随时间
//     → 显式 DAY/NIGHT 模式 tick() no-op (强制覆盖不变)
//   - setTimeBaseline(hour, ms) 设基线: 部署时 ShmDataSource 启动时调
//     (用 wall clock hour + candash::now_monotonic_ms())
//   - 时间倒退防御: now_ms < baselineMs → 按 delta=0 处理 (避免负数/UB)
//   - 默认值: baseline (12:00, ms=0), 启动时为 DAY
//
// 复用现有模式 (参照 TripComputer):
//   - 纯 C++ 类, 状态自包含, 无 Qt
//   - tick() 由上游按 16ms 节奏调用
//   - reset() 回到默认 DAY 模式
//
// 不在本 PR 范围 (后续 PR-B 接入数据流, PR-C 改 QML):
//   - 不修改 DisplaySnapshot / QtDataBinder / QML
//   - 不与 VehicleLogic 联动 (无 CAN 信号输入)

#pragma once

#include <cstdint>

namespace candash {

// 主题模式
enum class ThemeMode : uint8_t {
    DAY = 0,    // 强制日间
    NIGHT = 1,  // 强制夜间
    AUTO = 2    // 根据 hour 自动切换
};

// 5 色板: 背景/前景/强调/警告/严重
// ARGB 格式 (与 alarm_runtime 的 color 字段一致)
struct ThemeColors {
    uint32_t background;  // 主背景
    uint32_t foreground;  // 主文字
    uint32_t accent;      // 强调 (高亮数字/指针)
    uint32_t warning;     // 警告 (黄色)
    uint32_t critical;    // 严重 (红色)
};

class ThemeManager {
public:
    ThemeManager();

    // ─── 模式控制 ───
    void setMode(ThemeMode mode);
    ThemeMode currentMode() const { return m_mode; }
    bool isDay() const { return m_isDay; }

    // ─── AUTO 阈值 ───
    // 默认: 06:00-18:00 为日间, 其余为夜间
    void setSunriseHour(uint8_t hour) { m_sunriseHour = hour; }
    void setSunsetHour(uint8_t hour) { m_sunsetHour = hour; }
    uint8_t sunriseHour() const { return m_sunriseHour; }
    uint8_t sunsetHour() const { return m_sunsetHour; }

    // ─── 时间注入 ───
    // 当前小时 (0-23). 默认 12 (中午 = DAY)
    void setCurrentHour(uint8_t hour);
    uint8_t currentHour() const { return m_currentHour; }

    // ─── 时间基线 (tick 推算 hour 用) ───
    // 设基线: 启动时 (baselineHour, baselineMs) = (wall clock hour, monotonic ms)
    // tick(now_ms) → hour = (baselineHour + (now_ms - baselineMs)/3600000) % 24
    // 默认 (12, 0): tick(0) = 12:00, tick(3.6M) = 13:00
    // 重复调 = 重设基线 (ShmDataSource 启动时调一次即可)
    void setTimeBaseline(uint8_t hour, uint64_t now_ms);
    uint8_t baselineHour() const { return m_baselineHour; }
    uint64_t baselineMs() const { return m_baselineMs; }

    // ─── tick ───
    // AUTO 模式: 推算 hour = baseline + (now_ms - baselineMs)/3600000, 重新评估 DAY/NIGHT
    // 显式 DAY/NIGHT 模式: no-op (isDay 不变)
    // 时间倒退: now_ms < baselineMs → 按 delta=0 处理
    void tick(uint64_t now_ms);

    // ─── 颜色查询 ───
    ThemeColors colors() const;
    uint32_t colorOf(const char* slot) const;  // "bg" / "fg" / "accent" / "warning" / "critical"

    // ─── 重置 ───
    void reset();

    // 预设配色 (测试/外部可读)
    static const ThemeColors kDayColors;
    static const ThemeColors kNightColors;

private:
    void evaluateAutoMode();
    static uint8_t normalizeHour(int hour);  // 把 hour mod 24 限制到 [0, 23]

    ThemeMode m_mode = ThemeMode::AUTO;
    bool      m_isDay = true;     // 派生: 当前是否为日间
    uint8_t   m_sunriseHour = 6;
    uint8_t   m_sunsetHour  = 18;
    uint8_t   m_currentHour = 12; // 默认中午, 启动时为 DAY
    uint8_t   m_baselineHour = 12;  // tick() 推算 hour 用: 启动时刻的 wall clock hour
    uint64_t  m_baselineMs   = 0;   // 启动时刻的 monotonic ms (默认 0 便于测试)
};

}  // namespace candash

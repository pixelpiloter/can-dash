// chime_manager.h
// Layer 2: 声音提示 (chime) 管理器
// 纯 C++，无 Qt，无音频文件依赖 (L2 只发事件, 实际播放由 QML / Kanzi 端负责)
//
// 职责:
//   - 接收告警触发 (WarningSeverity), 按业务规则决定是否产生 chime 事件
//   - 暴露 ChimeEvent 给 Layer 3 (QML 用 QtMultimedia 播放 / 将来 Kanzi 端用 platform API)
//   - 全局静音开关 + 音量控制 + 防抖
//
// 设计要点 (PR 11: L2 + test, 不接数据流):
//   - 状态机简单: 最多 1 个 active chime (车载 HMI 不会同时播多个)
//   - tick() 推进, 处理 chime 播放结束 (active 状态清除)
//   - severity 映射: CRITICAL 触发高频重音, WARNING 中频, INFO 静默
//   - 防抖: 同 severity 在 cooldown_ms 内只触发 1 次, 避免告警风暴刷屏
//   - 全局 enabled=false → 全静默 (驾驶员按静音键)
//   - volume 0-100, 自动 clamp
//
// 复用现有模式 (参照 ThemeManager / TripComputer / WarningManager / SettingsManager):
//   - 纯 C++ 类, 状态自包含, 无 Qt
//   - tick() 由上游按需调用
//   - reset() 回到默认状态
//   - kDefaultConfig 静态常量供测试和外部读取
//
// 不在本 PR 范围 (后续 PR 接入数据流 + QML 实际播放):
//   - 不修改 DisplaySnapshot / QtDataBinder / QML
//   - 不实际播放音频 (需要 QtMultimedia 依赖)
//   - 不联动 SettingsManager (音量各自独立, 后续可加联动)

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// 复用 WarningManager 的 WarningSeverity
#include "warning_manager.h"

namespace candash {

// chime 事件 (Layer 3 消费, QML 端用 QSoundEffect / QMediaPlayer 播放)
struct ChimeEvent {
    uint8_t  severity;       // 0=INFO, 1=WARNING, 2=CRITICAL
    uint8_t  _pad0[1];
    uint16_t frequency_hz;   // 800 / 1200 / 1500 等 (正弦波频率)
    uint16_t duration_ms;    // 100-500
    uint8_t  volume_pct;     // 0-100
    uint8_t  repeat_count;   // 1-3 (重播次数, 间隔由 repeat_gap_ms 决定)
    uint16_t repeat_gap_ms;  // 重复间隔, 默认 200
    uint64_t start_ms;       // monotonic ms, 播放开始时刻
    uint64_t end_ms;         // monotonic ms, 播放结束时刻 (start + duration × repeat + gaps)
};

// chime 配置 (运行时可调, 测试可注入)
struct ChimeConfig {
    bool     enabled       = true;
    uint8_t  volume_pct    = 80;    // 0-100
    uint32_t cooldown_ms   = 1000;  // 同 severity 防抖窗口

    // severity → 音调参数
    uint16_t critical_freq_hz   = 1500;  // CRITICAL 用高频
    uint16_t critical_dur_ms    = 300;
    uint8_t  critical_repeat    = 2;
    uint16_t warning_freq_hz    = 1000;  // WARNING 中频
    uint16_t warning_dur_ms     = 200;
    uint8_t  warning_repeat     = 1;
    // INFO 不触发 chime (静默)
};

class ChimeManager {
public:
    ChimeManager();

    // ─── 业务入口 ───
    // 接收告警触发, 根据 severity + 防抖 + 静音决定是否产生 chime
    // now_ms: monotonic ms, 测试可注入
    void onWarningTriggered(WarningSeverity sev, uint64_t now_ms);

    // 时间推进 — 处理 active chime 结束 (end_ms < now_ms 时清除)
    void tick(uint64_t now_ms);

    // ─── 查询 ───
    // 当前是否有 active chime
    bool hasActiveChime() const { return m_hasActive; }

    // 当前 chime 详情 (Layer 3 binder 读取, 实际播放由 UI 框架负责)
    const ChimeEvent& activeChime() const { return m_activeChime; }

    // ─── 配置 ───
    void setEnabled(bool e)        { m_config.enabled = e; }
    bool enabled() const           { return m_config.enabled; }

    void setVolume(uint8_t pct);   // 自动 clamp 到 [0, 100]
    uint8_t volume() const         { return m_config.volume_pct; }

    void setCooldownMs(uint32_t ms){ m_config.cooldown_ms = ms; }
    uint32_t cooldownMs() const    { return m_config.cooldown_ms; }

    void setConfig(const ChimeConfig& c) { m_config = c; }
    const ChimeConfig& config() const     { return m_config; }

    // ─── 重置 ───
    void reset();

    // 预设配置 (测试/外部可读)
    static const ChimeConfig kDefaultConfig;

private:
    // 按 severity 构造 chime 事件
    ChimeEvent buildChime(WarningSeverity sev, uint64_t now_ms) const;

    ChimeConfig m_config;
    ChimeEvent  m_activeChime{};
    bool        m_hasActive = false;
    // 防抖表: severity → 上次触发时间
    uint64_t    m_lastCriticalMs = 0;
    uint64_t    m_lastWarningMs  = 0;
};

}  // namespace candash

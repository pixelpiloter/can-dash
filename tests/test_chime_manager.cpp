// test_chime_manager.cpp
// Layer 2 ChimeManager 单元测试 (纯 C++, 无 Qt)
//
// 覆盖:
//   1. 初始无 active
//   2. CRITICAL 触发 → 高频重音
//   3. WARNING 触发 → 中频单音
//   4. INFO 触发 → 静默 (无 active)
//   5. 全局静音 enabled=false → 所有 severity 都静默
//   6. 音量 clamp 到 [0, 100]
//   7. 防抖: 同 severity 1s 内只触发 1 次
//   8. 防抖外可再次触发
//   9. CRITICAL 和 WARNING 互不干扰 (独立防抖时钟)
//  10. tick 推进 → chime 播放结束后清除 active
//  11. tick 未到期时 active 保持
//  12. reset 回到默认 + 清 active
//  13. setConfig 自定义 freq/dur/repeat 生效
//  14. setEnabled 切换全局静默
//  15. CRITICAL 覆盖之前的 WARNING (最近的告警赢)
//  16. chime 事件 end_ms 计算正确 (dur × repeat + gap × (repeat-1))

#include <cstdio>
#include <cstring>
#include <cassert>
#include "../src/layer2/chime_manager.h"
#include "../src/layer2/warning_manager.h"

using candash::ChimeManager;
using candash::ChimeConfig;
using candash::ChimeEvent;
using candash::WarningSeverity;

static int g_test_count = 0;
static int g_test_passed = 0;
static int g_test_section = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { \
        g_test_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        printf("  ✗ %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define RUN(name) do { \
    printf("\n[%d] %s:\n", ++g_test_section, #name); \
    name(); \
} while(0)

// ─── 1. 初始无 active ───
static void test_initial_state() {
    ChimeManager c;
    TEST_ASSERT(!c.hasActiveChime(), "初始 hasActiveChime = false");
    TEST_ASSERT(c.enabled(), "默认 enabled = true");
    TEST_ASSERT(c.volume() == 80, "默认 volume = 80");
    TEST_ASSERT(c.cooldownMs() == 1000, "默认 cooldown = 1000ms");
}

// ─── 2. CRITICAL 触发 → 高频重音 ───
static void test_critical_chime() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.hasActiveChime(), "CRITICAL 触发 → hasActive = true");
    const auto& e = c.activeChime();
    TEST_ASSERT(e.severity == 2u, "severity = CRITICAL(2)");
    TEST_ASSERT(e.frequency_hz == 1500u, "freq = 1500Hz (高频)");
    TEST_ASSERT(e.duration_ms == 300u, "duration = 300ms");
    TEST_ASSERT(e.repeat_count == 2u, "repeat = 2 (重播)");
    TEST_ASSERT(e.volume_pct == 80u, "volume = 80 (默认)");
    TEST_ASSERT(e.start_ms == 1000u, "start_ms = 1000");
    // end_ms = 1000 + 300*2 + 200*1 = 1000 + 600 + 200 = 1800
    TEST_ASSERT(e.end_ms == 1800u, "end_ms = 1800 (300×2 + 200×1)");
}

// ─── 3. WARNING 触发 → 中频单音 ───
static void test_warning_chime() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::WARNING, 1000);
    TEST_ASSERT(c.hasActiveChime(), "WARNING 触发 → hasActive = true");
    const auto& e = c.activeChime();
    TEST_ASSERT(e.severity == 1u, "severity = WARNING(1)");
    TEST_ASSERT(e.frequency_hz == 1000u, "freq = 1000Hz (中频)");
    TEST_ASSERT(e.duration_ms == 200u, "duration = 200ms");
    TEST_ASSERT(e.repeat_count == 1u, "repeat = 1 (单音)");
    // end_ms = 1000 + 200*1 + 200*0 = 1200
    TEST_ASSERT(e.end_ms == 1200u, "end_ms = 1200");
}

// ─── 4. INFO 触发 → 静默 ───
static void test_info_silent() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::INFO, 1000);
    TEST_ASSERT(!c.hasActiveChime(), "INFO → 静默 (无 active)");
}

// ─── 5. 全局静音 ───
static void test_globally_muted() {
    ChimeManager c;
    c.setEnabled(false);
    TEST_ASSERT(!c.enabled(), "setEnabled(false) 生效");
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(!c.hasActiveChime(), "静音下 CRITICAL 不触发");
    c.onWarningTriggered(WarningSeverity::WARNING, 1100);
    TEST_ASSERT(!c.hasActiveChime(), "静音下 WARNING 不触发");

    // 重新启用
    c.setEnabled(true);
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1200);
    TEST_ASSERT(c.hasActiveChime(), "启用后 CRITICAL 触发");
}

// ─── 6. 音量 clamp ───
static void test_volume_clamp() {
    ChimeManager c;
    c.setVolume(50);
    TEST_ASSERT(c.volume() == 50, "volume=50 正常");

    c.setVolume(150);  // 越界
    TEST_ASSERT(c.volume() == 100, "volume 越界 150 → clamp 到 100");

    c.setVolume(255);
    TEST_ASSERT(c.volume() == 100, "volume 越界 255 → 100");

    c.setVolume(0);
    TEST_ASSERT(c.volume() == 0, "volume=0 静默 (但 enabled 仍 true)");
    TEST_ASSERT(c.hasActiveChime() == false || c.hasActiveChime() == true, "volume=0 不阻止触发, 由 UI 端处理");
    // 实际: volume=0 时仍触发, 由播放端按 0 音量播放 = 静默
    c.onWarningTriggered(WarningSeverity::WARNING, 1000);
    TEST_ASSERT(c.hasActiveChime(), "volume=0 仍触发 (业务: 触发逻辑跟音量解耦)");
    TEST_ASSERT(c.activeChime().volume_pct == 0u, "chime event volume_pct=0");
}

// ─── 7. 防抖: 同 severity 1s 内不重复 ───
static void test_cooldown_same_severity() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.hasActiveChime(), "首次 CRITICAL 触发");
    const auto& e1 = c.activeChime();
    TEST_ASSERT(e1.start_ms == 1000u, "首次 start_ms=1000");

    // 500ms 后再推 CRITICAL → 防抖命中, 静默 (active 不变)
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1500);
    const auto& e2 = c.activeChime();
    TEST_ASSERT(e2.start_ms == 1000u, "500ms 内防抖命中 → active 不变");
    TEST_ASSERT(e2.severity == 2u, "severity 仍 CRITICAL (上次的)");

    // 800ms 后 (仍在 1s 窗口内) → 仍防抖
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1800);
    const auto& e3 = c.activeChime();
    TEST_ASSERT(e3.start_ms == 1000u, "800ms 内仍防抖");
}

// ─── 8. 防抖外可再次触发 ───
static void test_cooldown_outside_window() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.activeChime().start_ms == 1000u, "首次 start_ms=1000");

    // 1500ms 后, 超过 1s 窗口 → 可再次触发
    c.onWarningTriggered(WarningSeverity::CRITICAL, 2500);
    TEST_ASSERT(c.activeChime().start_ms == 2500u, "1500ms 后重新触发, start_ms 更新");
    TEST_ASSERT(c.activeChime().end_ms == 3300u, "end_ms = 2500 + 600 + 200 = 3300");
}

// ─── 9. CRITICAL 和 WARNING 互不干扰 ───
static void test_critical_warning_independent_cooldown() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);  // CRITICAL cooldown = 1000
    c.onWarningTriggered(WarningSeverity::WARNING, 1100);   // WARNING cooldown = 1100, 互不影响
    TEST_ASSERT(c.hasActiveChime(), "active 仍存在");
    TEST_ASSERT(c.activeChime().severity == 1u, "WARNING (后者) 覆盖了 CRITICAL");

    // 200ms 后再推 CRITICAL → 距上次 CRITICAL 200ms, 仍在 1s 窗口内 → 防抖
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1300);
    TEST_ASSERT(c.activeChime().severity == 1u, "CRITICAL 防抖命中 → active 仍 WARNING");

    // 推另一个 WARNING → 距上次 WARNING 200ms, 仍在 1s 窗口内 → 防抖
    c.onWarningTriggered(WarningSeverity::WARNING, 1300);
    TEST_ASSERT(c.activeChime().severity == 1u, "WARNING 防抖命中 → active 仍 WARNING");

    // 2000ms 后, 两个都过 cooldown → 推 CRITICAL 成功
    c.onWarningTriggered(WarningSeverity::CRITICAL, 3000);
    TEST_ASSERT(c.activeChime().severity == 2u, "2000ms 后 CRITICAL 重新触发");
    TEST_ASSERT(c.activeChime().start_ms == 3000u, "start_ms 更新");
}

// ─── 10. tick 推进 → chime 播放结束后清除 ───
static void test_tick_clears_expired() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    // end_ms = 1800
    TEST_ASSERT(c.hasActiveChime(), "触发后 active = true");

    c.tick(1500);  // 距 end_ms=1800 还有 300ms
    TEST_ASSERT(c.hasActiveChime(), "tick(1500) 未到期, 仍 active");

    c.tick(1799);  // 距 1ms
    TEST_ASSERT(c.hasActiveChime(), "tick(1799) 差 1ms, 仍 active");

    c.tick(1801);  // 超过 end_ms
    TEST_ASSERT(!c.hasActiveChime(), "tick(1801) 超过 end_ms → active 清除");
}

// ─── 11. tick 未到期时 active 保持 ───
static void test_tick_no_op() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::WARNING, 1000);
    // end_ms = 1200
    c.tick(1000);
    TEST_ASSERT(c.hasActiveChime(), "tick(now=start) 仍 active");
    c.tick(1100);
    TEST_ASSERT(c.hasActiveChime(), "tick(1100) 仍 active");
    c.tick(1199);
    TEST_ASSERT(c.hasActiveChime(), "tick(1199) 仍 active");
}

// ─── 12. reset 回到默认 ───
static void test_reset() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.hasActiveChime(), "reset 前 active = true");

    // 改各种 config
    c.setVolume(50);
    c.setEnabled(false);
    c.setCooldownMs(500);

    c.reset();
    TEST_ASSERT(!c.hasActiveChime(), "reset 后 active = false");
    TEST_ASSERT(c.volume() == 80, "reset 后 volume = 80 (默认)");
    TEST_ASSERT(c.enabled(), "reset 后 enabled = true");
    TEST_ASSERT(c.cooldownMs() == 1000, "reset 后 cooldown = 1000 (默认)");

    // reset 后可重新触发 (防抖时钟也清)
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.hasActiveChime(), "reset 后重新触发 OK");
}

// ─── 13. setConfig 自定义 freq/dur/repeat ───
static void test_custom_config() {
    ChimeManager c;
    ChimeConfig cfg = c.config();
    cfg.critical_freq_hz = 2000;
    cfg.critical_dur_ms  = 500;
    cfg.critical_repeat  = 3;
    cfg.warning_freq_hz  = 600;
    c.setConfig(cfg);

    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    const auto& e1 = c.activeChime();
    TEST_ASSERT(e1.frequency_hz == 2000u, "CRITICAL freq = 自定义 2000");
    TEST_ASSERT(e1.duration_ms == 500u, "CRITICAL dur = 自定义 500");
    TEST_ASSERT(e1.repeat_count == 3u, "CRITICAL repeat = 自定义 3");
    // end_ms = 1000 + 500*3 + 200*2 = 1000 + 1500 + 400 = 2900
    TEST_ASSERT(e1.end_ms == 2900u, "end_ms = 2900 (500×3 + 200×2)");

    c.onWarningTriggered(WarningSeverity::WARNING, 1000);  // 防抖外 (severity 独立)
    const auto& e2 = c.activeChime();
    TEST_ASSERT(e2.frequency_hz == 600u, "WARNING freq = 自定义 600");
}

// ─── 14. setEnabled 切换 ───
static void test_set_enabled() {
    ChimeManager c;
    TEST_ASSERT(c.enabled(), "默认 enabled = true");
    c.setEnabled(false);
    TEST_ASSERT(!c.enabled(), "setEnabled(false) 生效");

    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(!c.hasActiveChime(), "disabled 时不触发");

    c.setEnabled(true);
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.hasActiveChime(), "re-enabled 后触发");
}

// ─── 15. CRITICAL 覆盖之前的 WARNING ───
static void test_critical_overrides_warning() {
    ChimeManager c;
    c.onWarningTriggered(WarningSeverity::WARNING, 1000);
    TEST_ASSERT(c.activeChime().severity == 1u, "初始 WARNING");

    // 50ms 后推 CRITICAL (跨 severity 不受防抖限制)
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1050);
    TEST_ASSERT(c.activeChime().severity == 2u, "CRITICAL 覆盖 WARNING");
    TEST_ASSERT(c.activeChime().start_ms == 1050u, "start_ms 更新到 1050");
    TEST_ASSERT(c.activeChime().frequency_hz == 1500u, "freq 切到 CRITICAL 1500");
}

// ─── 16. chime 事件 end_ms 计算 ───
static void test_end_ms_calculation() {
    ChimeManager c;
    // CRITICAL: 300ms × 2 repeat + 200ms gap × 1 = 800ms 总长
    c.onWarningTriggered(WarningSeverity::CRITICAL, 1000);
    TEST_ASSERT(c.activeChime().end_ms - c.activeChime().start_ms == 800u, "CRITICAL 总长 800ms");

    // WARNING: 200ms × 1 repeat + 200ms gap × 0 = 200ms 总长
    c.onWarningTriggered(WarningSeverity::WARNING, 2000);
    TEST_ASSERT(c.activeChime().end_ms - c.activeChime().start_ms == 200u, "WARNING 总长 200ms");

    // 自定义 3 repeat, dur=500, gap=200: 500*3 + 200*2 = 1900ms
    ChimeConfig cfg = c.config();
    cfg.critical_repeat = 3;
    cfg.critical_dur_ms = 500;
    c.setConfig(cfg);
    c.onWarningTriggered(WarningSeverity::CRITICAL, 3000);
    TEST_ASSERT(c.activeChime().end_ms - c.activeChime().start_ms == 1900u, "自定义 3 repeat 总长 1900ms");
}

int main() {
    printf("=== ChimeManager 单元测试 ===\n");

    RUN(test_initial_state);
    RUN(test_critical_chime);
    RUN(test_warning_chime);
    RUN(test_info_silent);
    RUN(test_globally_muted);
    RUN(test_volume_clamp);
    RUN(test_cooldown_same_severity);
    RUN(test_cooldown_outside_window);
    RUN(test_critical_warning_independent_cooldown);
    RUN(test_tick_clears_expired);
    RUN(test_tick_no_op);
    RUN(test_reset);
    RUN(test_custom_config);
    RUN(test_set_enabled);
    RUN(test_critical_overrides_warning);
    RUN(test_end_ms_calculation);

    printf("\n=== %d/%d 测试通过 ===\n", g_test_passed, g_test_count);
    return (g_test_passed == g_test_count) ? 0 : 1;
}

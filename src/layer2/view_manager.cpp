// view_manager.cpp
// 详见 view_manager.h

#include "view_manager.h"

namespace candash {

ViewManager::ViewManager() {
    // 默认值由成员初始化列表给定 (DRIVE + 1s hysteresis)
    // 此处无额外逻辑, 保持简洁
}

void ViewManager::setGearStatus(uint8_t gear) {
    m_gear = gear;
    // 不在 setter 内立即切, 让 tick 统一处理 (受 hysteresis 约束)
    // tick 会重算 candidate 并应用
}

void ViewManager::setChargeStatus(uint8_t charge) {
    m_charge = charge;
}

void ViewManager::tick(uint64_t now_ms) {
    ViewMode candidate = computeCandidate();
    if (candidate != m_current) {
        tryTransition(now_ms, candidate, /*force=*/false);
    }
}

ViewSnapshot ViewManager::snapshot() const {
    ViewSnapshot s{};
    s.current = static_cast<uint8_t>(m_current);
    s.gear    = m_gear;
    s.charge  = m_charge;
    s._pad    = 0;
    return s;
}

void ViewManager::reset() {
    m_current      = kDefaultView;
    m_pending      = kDefaultView;
    m_lastChangeMs = UINT64_MAX;  // 重置为 "从未切换" sentinel
    m_gear         = kGearPark;
    m_charge       = kChargeIdle;
    // 不重置 m_hysteresisMs, 配置独立于瞬时状态
}

void ViewManager::setViewForTest(ViewMode v) {
    tryTransition(0, v, /*force=*/true);
}

ViewMode ViewManager::computeCandidate() const {
    // 优先级 1: 充电中 → CHARGE (覆盖一切)
    if (m_charge > kChargeIdle) {
        return ViewMode::CHARGE;
    }
    // 优先级 2: 行驶档 (D / R / S) → DRIVE
    //   注意: R 倒档也归为 DRIVE (用户实际在驾驶, 只是方向反了)
    if (m_gear == kGearDrive || m_gear == kGearReverse || m_gear == kGearSport) {
        return ViewMode::DRIVE;
    }
    // 优先级 3: P / N + 未充电 → SETUP
    return ViewMode::SETUP;
}

void ViewManager::tryTransition(uint64_t now_ms, ViewMode candidate, bool force) {
    m_pending = candidate;
    if (m_current == candidate) {
        return;  // 已在目标态, 无操作
    }
    if (force || m_hysteresisMs == 0) {
        m_current      = candidate;
        m_lastChangeMs = now_ms;
        return;
    }
    // 正常路径: 距上次切换满 hysteresis 才切
    // 语义: 首次切换 (lastChangeMs=UINT64_MAX sentinel) 立即执行, 后续切换需 hysteresis
    // 防御: 时间倒流 (now_ms < m_lastChangeMs) 时不切, 等下个 tick 重试
    if (m_lastChangeMs == UINT64_MAX ||
        (now_ms >= m_lastChangeMs &&
         (now_ms - m_lastChangeMs) >= m_hysteresisMs)) {
        m_current      = candidate;
        m_lastChangeMs = now_ms;
    }
    // 否则保留 m_current, 等下次 tick 重试
}

}  // namespace candash

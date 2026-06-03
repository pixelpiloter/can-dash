// warning_manager.cpp
// 详见 warning_manager.h
//
// 5 个业务规则实现细节:
//
// 1. 严重度分级 (severityFromPriority):
//    priority == 0    → CRITICAL (红, 快闪, 后续 PR 接 chime)
//    priority 1-9     → WARNING  (黄, 慢闪)
//    priority >= 10   → INFO     (蓝, 不闪)
//
// 2. 去重 (dedup): 同 name 在 dedup_window_ms 内只"真"加入 active 一次
//    但仍记 dedup_count, 角标显示 N 次
//    (注意: 防抖后无论是否真加入, 都更新 m_debounce 防止高频)
//
// 3. 防抖 (debounce): 防抖窗口内的连续 pushAlarm 视为同一次事件
//    区别于去重: 防抖是"还在抖动就合并", 去重是"已显示过就不重复显示"
//
// 4. hold: 报警源消失 (pushAlarm 不再来) 后, active 保留 hold_ms 再清除
//    避免闪烁 (短促消失又出现时, UI 不抖动)
//
// 5. max_active: 超过 max_active 时, 丢 priority 最大的 (即最不严重的)
//    排序按 priority 数字小在前, 保持确定性

#include "warning_manager.h"
#include <algorithm>
#include <cstring>

namespace candash {

const WarningConfig WarningManager::kDefaultConfig = {
    .dedup_window_ms = 5000,
    .debounce_ms     = 100,
    .hold_ms         = 3000,
    .max_active      = 3,
};

WarningManager::WarningManager() : m_config(kDefaultConfig) {}

// 严重度判定: priority 数字小=严重
WarningSeverity WarningManager::severityFromPriority(uint32_t priority) {
    if (priority == 0) return WarningSeverity::CRITICAL;
    if (priority < 10) return WarningSeverity::WARNING;
    return WarningSeverity::INFO;
}

void WarningManager::pushAlarm(const AlarmEvent& evt, uint64_t now_ms) {
    // ─── 3. 防抖: 抖动窗口内只算 1 次 push ───
    // 防抖命中: 算"被合并", 但 dedup_count 仍 ++ (反映触发频度, 便于 UI 角标)
    // last_seen_ms 必须刷新, 防止 hold 期间被错误清除
    std::string name_key(evt.name);
    auto deb_it = m_debounce.find(name_key);
    if (deb_it != m_debounce.end()) {
        if (now_ms < deb_it->second + m_config.debounce_ms) {
            deb_it->second = now_ms;
            for (auto& aw : m_active) {
                if (std::strncmp(aw.name, evt.name, sizeof(aw.name)) == 0) {
                    aw.dedup_count++;
                    aw.last_seen_ms = now_ms;
                    return;
                }
            }
            return;  // 完全静默
        }
    }
    m_debounce[name_key] = now_ms;

    // ─── 2. 去重: dedup_window_ms 内不重复加入 ───
    auto ded_it = m_dedup.find(name_key);
    if (ded_it != m_dedup.end()) {
        if (now_ms < ded_it->second + m_config.dedup_window_ms) {
            // 去重命中: 在已显示告警上加 dedup_count 计数
            for (auto& aw : m_active) {
                if (std::strncmp(aw.name, evt.name, sizeof(aw.name)) == 0) {
                    aw.dedup_count++;
                    aw.last_seen_ms = now_ms;
                    return;
                }
            }
            // 已不在 active (可能因 trim 被丢), 仍累计 dedup_count 但不重新加入
            // 简化: 直接丢弃 (实际项目里可以维护一个"hidden" 计数表)
            return;
        }
    }

    // ─── 新增或更新 active ───
    ActiveWarning aw{};
    std::strncpy(aw.name, evt.name, sizeof(aw.name) - 1);
    std::strncpy(aw.text_zh, evt.text_zh, sizeof(aw.text_zh) - 1);
    std::strncpy(aw.text_en, evt.text_en, sizeof(aw.text_en) - 1);
    aw.priority      = evt.priority;
    aw.severity      = static_cast<uint8_t>(severityFromPriority(evt.priority));
    aw.color         = 0xFF000000U |  // 不透明 alpha
                       (static_cast<uint32_t>(evt.color_r) << 16) |
                       (static_cast<uint32_t>(evt.color_g) << 8)  |
                       (static_cast<uint32_t>(evt.color_b));
    aw.first_seen_ms = now_ms;
    aw.last_seen_ms  = now_ms;
    aw.dedup_count   = 0;

    // 找同名 active 是否已存在 → 复用 first_seen_ms, 累加 dedup_count
    bool found = false;
    for (auto& existing : m_active) {
        if (std::strncmp(existing.name, evt.name, sizeof(existing.name)) == 0) {
            existing.last_seen_ms = now_ms;
            existing.dedup_count++;
            existing.priority = aw.priority;  // priority 可能动态变化
            existing.severity = aw.severity;
            existing.color = aw.color;
            found = true;
            break;
        }
    }
    if (!found) {
        m_active.push_back(aw);
    }

    m_dedup[name_key] = now_ms;

    // ─── 1. 严重度排序 + 5. max_active 截断 ───
    sortByPriority();
    trimToMax();
}

void WarningManager::tick(uint64_t now_ms) {
    // ─── 4. hold: 过期清理 ───
    removeExpired(now_ms);
    sortByPriority();
    trimToMax();
}

bool WarningManager::hasCritical() const {
    for (const auto& aw : m_active) {
        if (aw.severity == static_cast<uint8_t>(WarningSeverity::CRITICAL)) {
            return true;
        }
    }
    return false;
}

bool WarningManager::isActive(const char* name) const {
    if (!name) return false;
    for (const auto& aw : m_active) {
        if (std::strncmp(aw.name, name, sizeof(aw.name)) == 0) {
            return true;
        }
    }
    return false;
}

void WarningManager::reset() {
    m_active.clear();
    m_dedup.clear();
    m_debounce.clear();
    m_config = kDefaultConfig;
}

void WarningManager::removeExpired(uint64_t now_ms) {
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
            [this, now_ms](const ActiveWarning& aw) {
                return (now_ms > aw.last_seen_ms + m_config.hold_ms);
            }),
        m_active.end()
    );
}

void WarningManager::sortByPriority() {
    std::sort(m_active.begin(), m_active.end(),
        [](const ActiveWarning& a, const ActiveWarning& b) {
            return a.priority < b.priority;  // 数字小在前
        });
}

void WarningManager::trimToMax() {
    if (m_active.size() > m_config.max_active) {
        m_active.resize(m_config.max_active);  // 截断末尾 (priority 大的)
    }
}

}  // namespace candash

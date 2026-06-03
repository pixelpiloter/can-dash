// settings_manager.cpp
// 详见 settings_manager.h

#include "settings_manager.h"

namespace candash {

SettingsManager::SettingsManager() {
    // 默认值由成员初始化列表给定 (METRIC + 80)
    // 此处无额外逻辑, 保持简洁
}

void SettingsManager::setUnits(Units u) {
    m_units = u;
}

void SettingsManager::setBrightness(uint8_t pct) {
    // clamp 到 [kMinBrightness, kMaxBrightness]
    if (pct < kMinBrightness) {
        m_brightness = kMinBrightness;
    } else if (pct > kMaxBrightness) {
        m_brightness = kMaxBrightness;
    } else {
        m_brightness = pct;
    }
}

SettingsSnapshot SettingsManager::snapshot() const {
    SettingsSnapshot s{};
    s.units      = static_cast<uint8_t>(m_units);
    s.brightness = m_brightness;
    s._pad       = 0;
    return s;
}

void SettingsManager::tick(uint64_t now_ms) {
    (void)now_ms;  // settings 不随时间漂移, 保留接口对齐
}

void SettingsManager::reset() {
    m_units      = kDefaultUnits;
    m_brightness = kDefaultBrightness;
}

}  // namespace candash

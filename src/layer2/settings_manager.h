// settings_manager.h
// Layer 2: 用户偏好设置管理器 (单位 / 亮度)
// 纯 C++，无 Qt，无 YAML 运行时
//
// 职责:
//   - 存储: 单位制 (公制/英制) + 屏幕亮度 (0-100)
//   - 暴露 getter/setter 给 Layer 3 binder 读取
//   - brightness 自动 clamp 到 [0, 100]
//
// 设计要点 (PR 10: L2 + test, 不接数据流):
//   - 状态机简单: 两个标量字段, 无 tick 副作用
//   - tick() 保留接口与 ThemeManager/TripComputer 对齐, 实际 no-op
//     (settings 不会随时间漂移, 后续 PR 可在此加 time-of-day 联动)
//   - reset() 回到默认 (公制 + 亮度 80)
//   - 默认值在 kDefaultUnits / kDefaultBrightness 静态常量, 外部可读
//
// 复用现有模式 (参照 ThemeManager / TripComputer / WarningManager):
//   - 纯 C++ 类, 状态自包含, 无 Qt
//   - tick() 由上游按需调用 (本类 no-op)
//   - reset() 回到默认状态
//   - kDefault* 静态常量供测试和外部读取
//
// 不在本 PR 范围 (后续 PR 接入数据流, PR 改 QML):
//   - 不修改 DisplaySnapshot / QtDataBinder / QML
//   - 不持久化到文件 (后续 PR 加 loadFromConfig / saveToConfig)
//   - 不联动 language / theme (LanguageManager / ThemeManager 各自独立)

#pragma once

#include <cstdint>

namespace candash {

// 单位制
enum class Units : uint8_t {
    METRIC   = 0,  // km/h, km, kWh/100km, °C
    IMPERIAL = 1,  // mph, mi, mpg, °F
};

// 当前 L2 支持的设置 (供 Layer 3 / QML 知道有哪些字段可读)
struct SettingsSnapshot {
    uint8_t units;       // Units 枚举
    uint8_t brightness;  // 0-100
    uint16_t _pad;       // 对齐到 4 字节
};

class SettingsManager {
public:
    SettingsManager();

    // ─── 单位制 ───
    void setUnits(Units u);
    Units units() const { return m_units; }

    // ─── 亮度 (0-100, 自动 clamp) ───
    void setBrightness(uint8_t pct);
    uint8_t brightness() const { return m_brightness; }

    // ─── 一次性导出 (供 Layer 3 binder 一次性读取, 避免多次 getter 调用) ───
    SettingsSnapshot snapshot() const;

    // ─── tick ───
    // 保留接口与 L2 模式对齐, 当前 no-op
    void tick(uint64_t now_ms);

    // ─── 重置 ───
    void reset();

    // 默认值 (外部可读, 跨进程配置回退)
    static const Units   kDefaultUnits      = Units::METRIC;
    static const uint8_t kDefaultBrightness = 80;
    static const uint8_t kMinBrightness     = 0;
    static const uint8_t kMaxBrightness     = 100;

private:
    Units    m_units      = kDefaultUnits;
    uint8_t  m_brightness = kDefaultBrightness;
};

}  // namespace candash

// warning_manager.h
// Layer 2: 业务告警去重/防抖/排序管理器
// 纯 C++，无 Qt，无 YAML 运行时
//
// 职责:
//   - 接收 AlarmRuntime 上报的 AlarmEvent
//   - 5 个业务规则: 严重度分级 / 去重 / 防抖 / hold / max_active
//   - 暴露 ActiveWarning 列表给 Layer 3 读取 (QML / Kanzi 都行)
//
// 设计要点 (PR 8: L2 + test, 不接数据流):
//   - 状态机简单: active 列表 + dedup 表
//   - tick() 不强制 16ms, 支持任意 dt, 测试可注入任意 (now_ms)
//   - pushAlarm(AlarmEvent, now_ms) 是唯一入口, 走 applyRules()
//   - 排序稳定: priority 数字小在前 (与 AlarmRuntime 约定一致, priority=0 最高)
//   - 默认配置保守: 5s dedup + 100ms debounce + 3s hold + 3 max_active
//
// 复用现有模式 (参照 ThemeManager / TripComputer):
//   - 纯 C++ 类, 状态自包含, 无 Qt
//   - tick() 由上游按需调用
//   - reset() 回到默认状态
//   - kDefaultConfig 静态常量供测试和外部读取
//
// 不在本 PR 范围 (后续 PR-9 接数据流, PR-10 改 QML):
//   - 不修改 DisplaySnapshot / QtDataBinder / QML
//   - 不联动 chime (后续 PR)
//   - 不持久化报警历史

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>

namespace candash {

// 报警事件输入 (来自 AlarmRuntime 或 can-processor)
// ⚠️ 与 src/layer3/display_data_types.h 的 AlarmEvent 字段保持一致
// (L2 独立定义一份, 避免 L2 依赖 L3 头 — Layer 严格单向)
struct AlarmEvent {
    char     name[32];
    char     text_zh[128];
    char     text_en[128];
    uint8_t  priority;            // 0=最高
    uint8_t  color_r;
    uint8_t  color_g;
    uint8_t  color_b;
    uint8_t  _pad[1];
};

// 严重度 (派生自 priority 范围, 方便 QML 端按严重度过滤)
enum class WarningSeverity : uint8_t {
    INFO     = 0,  // 蓝色提示, 不闪烁
    WARNING  = 1,  // 黄色警告, 慢闪
    CRITICAL = 2,  // 红色严重, 快闪 + chime
};

// 告警配置 (运行时可调, 测试可注入)
struct WarningConfig {
    uint32_t dedup_window_ms = 5000;  // 同 name 5s 内只显示 1 次
    uint32_t debounce_ms     = 100;   // 100ms 内连续同 name 只算 1 次
    uint32_t hold_ms         = 3000;  // 消失后保留 3s
    uint8_t  max_active      = 3;     // 最多同时 3 条
};

// 当前活动告警 (Layer 3 binder 读取此结构填 Q_PROPERTY / QVariantMap)
struct ActiveWarning {
    char     name[32];
    char     text_zh[128];
    char     text_en[128];
    uint8_t  severity;     // WarningSeverity
    uint8_t  _pad[3];
    uint32_t priority;     // 数字小=严重
    uint32_t color;        // ARGB
    uint64_t first_seen_ms;
    uint64_t last_seen_ms;
    uint32_t dedup_count;  // 累计被去重次数 (供 QML 角标用)
};

class WarningManager {
public:
    WarningManager();

    // ─── 业务入口 ───
    // 推入一条报警事件 (来自 AlarmRuntime 的活跃列表或事件回调)
    // now_ms: monotonic ms, 测试可注入任意值
    void pushAlarm(const AlarmEvent& evt, uint64_t now_ms);

    // 时间推进 — 处理 hold 过期, 清理超时的 active
    void tick(uint64_t now_ms);

    // ─── 查询 ───
    // 当前活动告警 (按 priority 升序, 即最严重在前)
    const std::vector<ActiveWarning>& activeWarnings() const { return m_active; }

    // 是否有 CRITICAL (priority 数值最小的那条; 简化: 是否有 priority=0)
    bool hasCritical() const;

    // 活动告警数量
    size_t activeCount() const { return m_active.size(); }

    // 按 name 查找是否在 active 列表
    bool isActive(const char* name) const;

    // ─── 配置 ───
    void setConfig(const WarningConfig& c) { m_config = c; }
    const WarningConfig& config() const { return m_config; }

    // ─── 重置 ───
    void reset();

    // 预设配置 (测试/外部可读)
    static const WarningConfig kDefaultConfig;

private:
    // 应用业务规则, 决定 evt 是新增 / 更新 / 去重
    // 返回 true 表示已加入或更新到 m_active; false 表示被去重
    bool applyRules(const AlarmEvent& evt, uint64_t now_ms, ActiveWarning& out);

    // 把告警从 active 中移除 (hold 过期调用)
    void removeExpired(uint64_t now_ms);

    // 按 priority 重新排序 (priority 小的在前)
    void sortByPriority();

    // 截断到 max_active (低 priority 的先丢)
    void trimToMax();

    // 严重度判定 (从 priority 数值)
    static WarningSeverity severityFromPriority(uint32_t priority);

    WarningConfig m_config;
    std::vector<ActiveWarning> m_active;
    // 去重表: name -> last_seen_ms (最后一次"真实"进入 active 的时刻)
    std::unordered_map<std::string, uint64_t> m_dedup;
    // 防抖表: name -> last_push_ms (最近一次 pushAlarm 的时刻, 不论是否真加入)
    std::unordered_map<std::string, uint64_t> m_debounce;
};

}  // namespace candash

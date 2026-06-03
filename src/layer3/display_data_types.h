// display_data_types.h
// 跨 UI 框架的数据类型（Qt / Kanzi / 未来其它）
// ⚠️ 不依赖任何 UI 框架头文件
//
// 设计原则：
// - 28 个业务字段（来自 config/can_ids.yaml，编译期生成）
// - 帧元数据：timestamp_ms / frame_seq / updated_mask
// - 健康状态：OK / WAITING / STALE / DISCONNECTED
// - 业务事件：alarm 列表 / seat_belt 状态 / indicator 状态（独立结构，binder 层映射到 UI）

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── 健康状态（4 态：处理器心跳监测）──────────────────────
typedef enum {
    HEALTH_DISCONNECTED = 0,  // shm 不可用（路径错 / ABI 不匹配）
    HEALTH_WAITING,           // 已打开 shm，尚未收到第一帧
    HEALTH_STALE,             // 心跳超时（>500ms 未 commit）
    HEALTH_OK                 // 心跳新鲜
} HealthStatus;

inline const char* health_status_str(HealthStatus h) {
    switch (h) {
        case HEALTH_OK:           return "ok";
        case HEALTH_WAITING:      return "waiting";
        case HEALTH_STALE:        return "stale";
        case HEALTH_DISCONNECTED: return "disconnected";
    }
    return "unknown";
}

// ─── 报警事件（来自 can-processor 报警 runtime）────────────
typedef struct {
    char     name[32];            // 报警名称（如 "bat_overvolt"）
    char     text_zh[128];        // 中文文案
    char     text_en[128];        // 英文文案
    uint8_t  priority;            // 0=最高, 255=最低
    uint8_t  color_r, color_g, color_b;  // 报警颜色（RGB）
    uint8_t  _pad;
} AlarmEvent;

// ─── 安全带状态（5 座位）──────────────────────────────────
typedef struct {
    bool occupied;                // 座位是否有人
    bool buckled;                 // 是否系安全带
    bool warning;                 // 是否触发警告（occupied && !buckled && speed>5）
} SeatState;

#define SEAT_COUNT 5
typedef struct {
    SeatState seats[SEAT_COUNT];  // 0=driver, 1=passenger, 2=rear_left, 3=rear_center, 4=rear_right
    bool warning_active;          // 至少 1 个座位触发警告
} SeatBeltState;

// ─── 指示灯状态（12 个槽位）────────────────────────────────
typedef struct {
    bool    on;                   // 是否亮
    bool    flash;                // 是否闪烁
    float   hz;                   // 闪烁频率（Hz）
} IndicatorState;

#define DISPLAY_INDICATOR_COUNT 12
typedef struct {
    IndicatorState lights[DISPLAY_INDICATOR_COUNT];
} IndicatorStates;

// ─── 帧元数据（来自 shm v1.2 协议）───────────────────────
typedef struct {
    uint64_t timestamp_ms;        // monotonic ms（processor commit 时间）
    uint32_t frame_seq;           // 帧序号（单调递增，丢帧检测用）
    uint32_t updated_mask;        // bit[i]=1 表示字段 i 已更新
    uint32_t dropped_frames;      // 累计丢帧数（cur_seq - last_seq - 1）
} FrameMetadata;

// ─── 显示数据快照（28 业务字段，编译期由 yaml_to_c.py 生成）──
// ⚠️ display_data.h 由 yaml_to_c.py 生成，这里仅 re-export
#include "../layer1/display_data.h"   // DisplayData

// ─── 完整数据快照（DataSource 推送给 Binder 的数据结构）───
typedef struct {
    DisplayData      data;            // 28 业务字段
    FrameMetadata    meta;            // 帧元数据
    HealthStatus     health;          // 健康状态
    AlarmEvent       alarms[8];       // 当前活跃报警（最多 8 条）
    uint8_t          alarm_count;     // alarms 数组有效长度
    SeatBeltState    seat_belt;       // 安全带状态
    IndicatorStates  indicators;      // 指示灯状态
    bool             is_moving;       // vehicle_speed > 1.0 km/h

    // ─── 派生指标（由 ShmDataSource 在 onTick() 中计算, 不来自 shm）───
    // TripComputer 算小计里程/平均车速/行驶时长, 是 v3 探针的"配置驱动"思路
    // 在数据流链路上的延伸: 把 cpp 业务逻辑 → 派生 → QML
    float            trip_distance_km;     // 累计行驶里程 (km)
    float            trip_avg_speed_kmh;   // 平均车速 (km/h)
    uint32_t         trip_duration_s;      // 累计行驶时长 (s)
    bool             trip_is_moving;       // 当前是否在行驶 (派生, 不读 is_moving)

    // PR 4: 能耗 + 续航可信度
    float            trip_energy_kwh;          // 累计放电能量 (kWh)
    float            trip_efficiency_kwh100km; // 百公里电耗 (kWh/100km), < 0.5km 时为 0
    float            trip_range_confidence_pct; // 续航可信度 (0-100%), 默认 100

    // ─── 主题 (PR 7) ───
    // 模式 + 5 色: ARGB 格式 (与 alarm_runtime 的 color 字段一致)
    // 由 ShmDataSource 在 onTick() 中从 m_theme 取出填入
    uint8_t          theme_mode;       // 0=DAY, 1=NIGHT, 2=AUTO
    uint8_t          theme_is_day;     // 派生: 当前是否日间 (0/1)
    uint8_t          _theme_pad[2];
    uint32_t         theme_color_background;  // 主背景 ARGB
    uint32_t         theme_color_foreground;  // 主文字 ARGB
    uint32_t         theme_color_accent;      // 强调 ARGB
    uint32_t         theme_color_warning;     // 警告 ARGB
    uint32_t         theme_color_critical;    // 严重 ARGB
} DisplaySnapshot;

#ifdef __cplusplus
}
#endif

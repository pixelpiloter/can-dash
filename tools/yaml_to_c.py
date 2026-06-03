#!/usr/bin/env python3
"""
YAML → C 代码生成器
运行时机：make 之前必须运行

Usage:
    python tools/yaml_to_c.py
"""

import json
import yaml
import sys
from pathlib import Path
from typing import Any, Dict, List

SRC_GENERATED = Path(__file__).parent.parent / "src" / "generated"
CONFIG_DIR = Path(__file__).parent.parent / "config"
SRC_LAYER1 = Path(__file__).parent.parent / "src" / "layer1"


def load_yaml(name: str) -> Dict:
    path = CONFIG_DIR / name
    if not path.exists():
        print(f"WARNING: {path} not found, skipping")
        return {}
    with open(path) as f:
        return yaml.safe_load(f)


def gen_display_data_h(can_data: Dict) -> str:
    """生成 display_data.h"""
    fields = []
    for src in can_data.get("can_sources", []):
        for f in src.get("fields", []):
            ctype = {
                "float": "float",
                "uint8": "uint8_t",
                "uint16": "uint16_t",
                "uint32": "uint32_t",
                "int8": "int8_t",
                "int16": "int16_t",
            }.get(f.get("type", "float"), "float")
            fields.append(f"    {ctype:12s}  {f['name']};   // unit: {f.get('unit','')}")

    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/can_ids.yaml

#pragma once
#include <stdint.h>
#include <stdbool.h>

// 显示数据结构（按领域拆分）
typedef struct {{
{chr(10).join(fields)}
}} DisplayData;

"""


def gen_can_field_def_h(can_data: Dict) -> str:
    """生成 can_field_def.h"""
    return """// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/can_ids.yaml

#pragma once
#include <stdint.h>

typedef enum {{
    ENDIAN_LITTLE,
    ENDIAN_BIG
}} Endian;

typedef struct {{
    uint32_t  can_id;
    uint8_t   byte_start;
    uint8_t   byte_end;
    uint8_t   bits;
    Endian     endian;
    int        shift;
    float      scale;
    float      offset;
    const char* display_key;   // 关联的显示变量名
}} CanFieldDef;

extern const CanFieldDef CAN_FIELD_TABLE[];
extern const int CAN_FIELD_TABLE_COUNT;
"""


def gen_can_field_table_c(can_data: Dict) -> str:
    """生成 can_field_table.c"""
    entries = []
    idx = 0
    for src in can_data.get("can_sources", []):
        can_id = src["can_id"]
        for f in src.get("fields", []):
            byte_start = f["byte"][0] if isinstance(f["byte"], list) else f["byte"]
            byte_end = f["byte"][-1] if isinstance(f["byte"], list) else f["byte"]
            bits = f.get("bits", 8 * (byte_end - byte_start + 1))
            endian = f.get("endian", "little")
            formula = f.get("formula", "x")
            scale, offset = extract_scale_offset(formula)
            endian_str = f"ENDIAN_{endian.upper()}"

            entries.append(
                f"    {{{can_id}, {byte_start}, {byte_end}, {bits}, {endian_str}, "
                f"0, {scale}f, {offset}f, \"{f['name']}\"}}"
            )
            idx += 1

    entries_str = ",\n".join(entries)
    count = idx
    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/can_ids.yaml

#include "can_field_def.h"

const CanFieldDef CAN_FIELD_TABLE[] = {{
{entries_str}
}};

const int CAN_FIELD_TABLE_COUNT = {count};
"""


def gen_alarm_rule_def_h(alarm_data: Dict) -> str:
    """生成 alarm_rule_def.h"""
    return """// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/alarm_rules.yaml

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {{
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM = 1,
    PRIORITY_HIGH = 2
}} AlarmPriority;

typedef enum {{
    COND_GT, COND_LT, COND_GE, COND_LE, COND_EQ, COND_NE
}} ConditionOp;

// 报警规则定义（编译时常量）
typedef struct {{
    const char*  name;              // 报警名
    uint8_t     display_key_index; // display_keys 索引
    ConditionOp op;                // 条件操作符
    float       threshold;          // 阈值
    AlarmPriority priority;        // 优先级
    uint32_t    duration_ms;       // 持续时间（防抖）
    uint8_t     action_count;      // 动作数量
    uint8_t     action_offset;     // 动作表起始索引
}} AlarmRuleDef;

// 报警动作定义
typedef enum {{
    ACTION_INDICATOR_LIGHT,
    ACTION_ALARM_TEXT,
    ACTION_ALARM_IMAGE,
    ACTION_SOUND
}} AlarmActionType;

typedef struct {{
    AlarmActionType type;
    const char*     widget;       // 关联的 widget id（indicator_light 用）
    const char*     text_zh;     // 报警中文文字
    const char*     text_en;      // 报警英文文字
    uint8_t         flash;        // 是否闪烁
    float           flash_hz;     // 闪烁频率
    uint16_t        font_size;    // 字体大小
    uint32_t        color;        // ARGB 颜色
    const char*     image;        // 图片路径
}} AlarmActionDef;

extern const AlarmRuleDef ALARM_RULE_TABLE[];
extern const int ALARM_RULE_TABLE_COUNT;

extern const AlarmActionDef ALARM_ACTION_TABLE[];
extern const int ALARM_ACTION_TABLE_COUNT;

// display_key 全局字符串表（由 yaml_to_c.py 从 can_ids.yaml + alarm_rules.yaml
// 聚合生成，所有规则的 display_key_index 都引用此表）。
// 用途：AlarmRuntime::onValueChanged() 用 strcmp 过滤，避免每个 value 变化
// 都遍历所有规则。
extern const char* const DISPLAY_KEY_TABLE[];
extern const int DISPLAY_KEY_TABLE_COUNT;
"""


def gen_alarm_rule_table_c(alarm_data: Dict, can_data: Dict) -> str:
    """生成 alarm_rule_table.c（同时生成 DISPLAY_KEY_TABLE）"""
    rules = alarm_data.get("alarm_rules", [])

    # 收集所有 display_key：先 can_ids.yaml 里的所有 field name，
    # 再 alarm_rules.yaml 里实际用到的 key（去重保序）
    keys = []
    seen = set()
    for src in can_data.get("can_sources", []):
        for f in src.get("fields", []):
            k = f.get("name", "")
            if k and k not in seen:
                seen.add(k); keys.append(k)
    for r in rules:
        k = r.get("display_key", "")
        if k and k not in seen:
            seen.add(k); keys.append(k)
    key_index = {k: i for i, k in enumerate(keys)}

    rule_entries = []
    action_entries = []
    action_offset = 0

    for i, rule in enumerate(rules):
        op_str = cond_to_op(rule.get("condition", "value > 0"))
        dk = rule.get("display_key", "")
        dk_idx = key_index.get(dk, 0)
        priority_str = f"PRIORITY_{rule.get('priority', 'medium').upper()}"

        rule_entries.append(
            f"    {{\"{rule['name']}\", /* display_key */ {dk_idx}, "
            f"COND_{op_str}, {extract_threshold(rule.get('condition', 'value > 0'))}f, "
            f"{priority_str}, {rule.get('duration_ms', 0)}, "
            f"{len(rule.get('actions', []))}, {action_offset}}}"
        )

        for action in rule.get("actions", []):
            action_type_map = {
                "indicator_light": "ACTION_INDICATOR_LIGHT",
                "alarm_text": "ACTION_ALARM_TEXT",
                "alarm_image": "ACTION_ALARM_IMAGE",
            }
            action_type = action_type_map.get(action.get("type", ""), "ACTION_INDICATOR_LIGHT")
            widget = action.get("widget", "null")
            text_zh = action.get("text_zh", "")
            text_en = action.get("text_en", "")
            flash = "1" if action.get("flash", False) else "0"
            flash_hz = action.get("flash_hz", 2.0)
            font_size = action.get("font_size", 24)
            color = hex_color(action.get("color", "#FF4400"))
            image = action.get("image", "null")

            action_entries.append(
                f"    {{ACTION_{action_type}, \"{widget}\", \"{text_zh}\", \"{text_en}\", "
                f"{flash}, {flash_hz}f, {font_size}, {color}U, \"{image}\"}}"
            )
            action_offset += 1

    rule_str = ",\n".join(rule_entries)
    action_str = ",\n".join(action_entries)
    key_str = ",\n".join(f'    "{k}"' for k in keys)

    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/alarm_rules.yaml / can_ids.yaml

#include "alarm_rule_def.h"

const char* const DISPLAY_KEY_TABLE[] = {{
{key_str}
}};
const int DISPLAY_KEY_TABLE_COUNT = {len(keys)};

const AlarmRuleDef ALARM_RULE_TABLE[] = {{
{rule_str}
}};

const int ALARM_RULE_TABLE_COUNT = {len(rules)};

const AlarmActionDef ALARM_ACTION_TABLE[] = {{
{action_str}
}};

const int ALARM_ACTION_TABLE_COUNT = {len(action_entries)};
"""


def gen_seat_belt_def_h(seat_data: Dict) -> str:
    """生成 seat_belt_def.h"""
    return """// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/seat_belt.yaml

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {{
    const char*  id;               // 座位 ID
    const char*  label_zh;        // 中文标签
    const char*  label_en;        // 英文标签
    const char*  icon_buckled;    // 已系图标
    const char*  icon_unbuckled;  // 未系图标
    const char*  icon_empty;      // 无人图标
    uint32_t     seat_occupied_can_id;
    uint8_t      seat_occupied_bit;
    uint32_t     buckle_can_id;
    uint8_t      buckle_bit;
    uint8_t      sort_priority;
}} SeatPositionDef;

typedef struct {{
    float  speed_threshold;        // 报警速度阈值 (km/h)
    bool   require_seat_occupied; // 是否需要检测到座位有人才报警
    const char* msg_single_zh;
    const char* msg_multiple_zh;
    const char* msg_single_en;
    const char* msg_multiple_en;
}} SeatBeltConfigDef;

extern const SeatPositionDef SEAT_POSITION_TABLE[];
extern const int SEAT_POSITION_TABLE_COUNT;

extern const SeatBeltConfigDef SEAT_BELT_CONFIG;
"""


def gen_seat_belt_table_c(seat_data: Dict) -> str:
    """生成 seat_belt_table.c"""
    cfg = seat_data.get("seat_belt", {})
    positions = cfg.get("positions", [])
    trigger = cfg.get("trigger", {})
    msgs = cfg.get("messages", {})

    pos_entries = []
    for pos in positions:
        can_ids = pos.get("can_ids", {})
        occ_id = can_ids.get("occupied", {}).get("id", 0)
        occ_bit = can_ids.get("occupied", {}).get("bit", 0)
        buc_id = can_ids.get("buckle", {}).get("id", 0)
        buc_bit = can_ids.get("buckle", {}).get("bit", 0)

        pos_entries.append(
            f"    {{\"{pos['id']}\", \"{pos['label_zh']}\", \"{pos['label_en']}\", "
            f"\"{pos.get('icon_buckled', '')}\", \"{pos.get('icon_unbuckled', '')}\", "
            f"\"{pos.get('icon_empty', '')}\", "
            f"{occ_id}, {occ_bit}, {buc_id}, {buc_bit}, {pos.get('sort_priority', 0)}}}"
        )

    pos_str = ",\n".join(pos_entries)
    config_str = (
        f"    {trigger.get('speed_threshold', 5.0)}f, "
        f"{'true' if trigger.get('require_seat_occupied', True) else 'false'}, "
        f"\"{msgs.get('single_zh', '{position}请系安全带')}\", "
        f"\"{msgs.get('multiple_zh', '{positions}请系安全带')}\", "
        f"\"{msgs.get('single_en', '{position} please buckle up')}\", "
        f"\"{msgs.get('multiple_en', '{positions} please buckle up')}\""
    )

    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/seat_belt.yaml

#include "seat_belt_def.h"

const SeatPositionDef SEAT_POSITION_TABLE[] = {{
{pos_str}
}};

const int SEAT_POSITION_TABLE_COUNT = {len(positions)};

const SeatBeltConfigDef SEAT_BELT_CONFIG = {{
{config_str}
}};
"""


def gen_indicator_def_h(indicator_data: Dict) -> str:
    """生成 indicator_def.h"""
    return """// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/indicators.yaml

#pragma once
#include <stdint.h>

typedef struct {{
    const char*  id;           // 指示灯唯一 ID
    const char*  type;         // light / text / image
    const char*  image_on;     // 点亮图片
    const char*  image_off;    // 熄灭图片
    int16_t     x;
    int16_t     y;
    uint16_t    width;
    uint16_t    height;
    bool        flash_on_fault; // 故障时自动闪烁
}} IndicatorDef;

extern const IndicatorDef INDICATOR_TABLE[];
extern const int INDICATOR_TABLE_COUNT;
"""


def gen_indicator_table_c(indicator_data: Dict) -> str:
    """生成 indicator_table.c"""
    entries = []
    for ind in indicator_data.get("indicators", []):
        pos = ind.get("position", {})
        size = ind.get("size", {})
        flash = "true" if ind.get("flash_on_fault", False) else "false"
        entries.append(
            f"    {{\"{ind['id']}\", \"{ind.get('type', 'light')}\", "
            f"\"{ind.get('image_on', '')}\", \"{ind.get('image_off', '')}\", "
            f"{pos.get('x', 0)}, {pos.get('y', 0)}, "
            f"{size.get('width', 60)}, {size.get('height', 60)}, "
            f"{flash}}}"
        )

    entries_str = ",\n".join(entries)
    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/indicators.yaml

#include "indicator_def.h"

const IndicatorDef INDICATOR_TABLE[] = {{
{entries_str}
}};

const int INDICATOR_TABLE_COUNT = {len(entries)};
"""


def gen_vehicle_config_h(threshold_data: Dict) -> str:
    """生成 vehicle_config.h (v3 探针)

    仅声明 kDefaultVehicleConfig, 定义在 .cpp 里.
    这是与其他生成代码一致的 idiom (alarm_rule_def.h + alarm_rule_table.cpp).
    """
    return """// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/vehicle_thresholds.yaml
//
// v3 探针产物: 车辆业务阈值编译期常量声明
// 定义见 vehicle_config.cpp (由 yaml_to_c.py 同步生成, 加入 GENERATED_SOURCES)

#pragma once
#include "vehicle_logic.h"

extern const VehicleConfigDef kDefaultVehicleConfig;
"""


def gen_vehicle_config_c(threshold_data: Dict) -> str:
    """生成 vehicle_config.cpp (v3 探针)

    从 config/vehicle_thresholds.yaml 读取阈值,
    输出 kDefaultVehicleConfig 全局常量供 VehicleLogic::init(nullptr) 使用.

    设计: 编译期常量 (无运行时 yaml 解析), 复用现有 yaml_to_c.py 基础设施.
    """
    t = threshold_data.get("vehicle_thresholds", {})

    # 类型安全: yaml.safe_load 已解析为正确类型 (float/int), 无需再转
    # 但加 static_cast 满足 -Wsign-conversion -Wconversion
    soc_warn = float(t.get("soc_warning_low", 10.0))
    soc_crit = float(t.get("soc_critical_low", 5.0))
    speed_max = float(t.get("speed_max", 260.0))
    pre_timeout = int(t.get("precharge_timeout_ms", 3000))
    pre_auto_done = int(t.get("precharge_auto_done_ms", 500))
    soc_window = int(t.get("soc_smoothing_window", 5))
    rg_engage = float(t.get("readygo_speed_engage_kmh", 0.5))
    rg_disengage = float(t.get("readygo_speed_disengage_kmh", 5.0))

    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/vehicle_thresholds.yaml
//
// v3 探针产物: 把车辆业务阈值从 C++ 硬编码移入 yaml, 改阈值不碰 cpp
// 字段含义见 config/vehicle_thresholds.yaml 注释
//
// 符号发射 2 个必要条件:
// 1. `extern` — C++ 中 namespace scope `const` 缺省是 internal linkage
//    (符号被 mangle 为 _ZL... 而 header 里 `extern const` 期待 external linkage, 会链接失败)
// 2. __attribute__((used)) — 否则 GCC -O3/-flto 会把未被本 TU 引用的 const 优化掉

#include "vehicle_logic.h"

__attribute__((used)) extern const VehicleConfigDef kDefaultVehicleConfig = {{
    /*  soc_warning_low           */ {soc_warn!r}f,
    /*  soc_critical_low          */ {soc_crit!r}f,
    /*  speed_max                 */ {speed_max!r}f,
    /*  precharge_timeout_ms      */ static_cast<uint32_t>({pre_timeout}),
    /*  soc_smoothing_window      */ {soc_window},
    /*  precharge_auto_done_ms    */ static_cast<uint32_t>({pre_auto_done}),
    /*  readygo_speed_engage_kmh  */ {rg_engage!r}f,
    /*  readygo_speed_disengage_kmh */ {rg_disengage!r}f,
}};
"""


def gen_signal_def_h(signal_data: Dict) -> str:
    """生成 signal_def.h"""
    return """// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/can_signal_status.yaml

#pragma once
#include <stdint.h>

typedef struct {{
    const char*  name;           // 信号名
    uint32_t     can_id;
    uint32_t     timeout_ms;      // 超时阈值
    float        min_value;       // 合法范围最小值
    float        max_value;       // 合法范围最大值
    float        max_delta;       // 最大变化量（突变检测）
    bool         smoothing;       // 是否启用平滑
    uint8_t      smoothing_window; // 平滑窗口大小
}} SignalDef;

extern const SignalDef SIGNAL_TABLE[];
extern const int SIGNAL_TABLE_COUNT;
"""


def gen_signal_table_c(signal_data: Dict) -> str:
    """生成 signal_table.c"""
    entries = []
    for sig in signal_data.get("signals", []):
        validity = sig.get("validity", {})
        smoothing = "true" if sig.get("smoothing", False) else "false"
        entries.append(
            f"    {{\"{sig['name']}\", {sig.get('can_id', 0)}, "
            f"{sig.get('timeout_ms', 500)}, "
            f"{validity.get('min', 0.0)}f, {validity.get('max', 0.0)}f, "
            f"{validity.get('max_delta', 0.0)}f, "
            f"{smoothing}, {sig.get('smoothing_window', 5)}}}"
        )

    entries_str = ",\n".join(entries)
    return f"""// ⚠️ 此文件由 tools/yaml_to_c.py 自动生成
// ⚠️ 请勿手动修改，修改请改 config/can_signal_status.yaml

#include "signal_def.h"

const SignalDef SIGNAL_TABLE[] = {{
{entries_str}
}};

const int SIGNAL_TABLE_COUNT = {len(entries)};
"""


def generate_all():
    print("Loading YAML configs...")
    can_data = load_yaml("can_ids.yaml")
    alarm_data = load_yaml("alarm_rules.yaml")
    seat_data = load_yaml("seat_belt.yaml")
    indicator_data = load_yaml("indicators.yaml")
    signal_data = load_yaml("can_signal_status.yaml")
    threshold_data = load_yaml("vehicle_thresholds.yaml")  # v3 探针

    SRC_GENERATED.mkdir(parents=True, exist_ok=True)
    print(f"Generating files in {SRC_GENERATED}...")

    # Layer 1: C struct 定义头文件（存 layer1/）
    layer1_dir = SRC_LAYER1
    layer1_dir.mkdir(parents=True, exist_ok=True)

    # display_data.h → layer1/
    with open(layer1_dir / "display_data.h", "w") as f:
        f.write(gen_display_data_h(can_data))
    print("  ✓ src/layer1/display_data.h")

    # generated/ 下的查找表
    files_generated = [
        ("can_field_def.h", None),
        ("can_field_table.cpp", can_data),
        ("alarm_rule_def.h", None),
        ("alarm_rule_table.cpp", alarm_data),
        ("seat_belt_def.h", None),
        ("seat_belt_table.cpp", seat_data),
        ("indicator_def.h", None),
        ("indicator_table.cpp", indicator_data),
        ("signal_def.h", None),
        ("signal_table.cpp", signal_data),
        ("vehicle_config.h", threshold_data),  # v3 探针
        ("vehicle_config.cpp", threshold_data),  # v3 探针
    ]

    generators = {
        "can_field_def.h": lambda _: gen_can_field_def_h(can_data),
        "can_field_table.cpp": lambda d: gen_can_field_table_c(d),
        "alarm_rule_def.h": lambda _: gen_alarm_rule_def_h(alarm_data),
        "alarm_rule_table.cpp": lambda d: gen_alarm_rule_table_c(d, can_data),
        "seat_belt_def.h": lambda _: gen_seat_belt_def_h(seat_data),
        "seat_belt_table.cpp": lambda d: gen_seat_belt_table_c(d),
        "indicator_def.h": lambda _: gen_indicator_def_h(indicator_data),
        "indicator_table.cpp": lambda d: gen_indicator_table_c(d),
        "signal_def.h": lambda _: gen_signal_def_h(signal_data),
        "signal_table.cpp": lambda d: gen_signal_table_c(d),
        "vehicle_config.h": lambda d: gen_vehicle_config_h(d),  # v3 探针
        "vehicle_config.cpp": lambda d: gen_vehicle_config_c(d),  # v3 探针
    }

    for fname, data in files_generated:
        content = generators[fname](data)
        with open(SRC_GENERATED / fname, "w") as f:
            f.write(content)
        print(f"  ✓ src/generated/{fname}")

    print(f"\n生成完成。共 {len(files_generated)} 个文件。")
    print("请运行 make 编译。")


# ── 辅助函数 ──────────────────────────────────────────────

def extract_scale_offset(formula: str) -> tuple:
    """从 formula 提取 scale 和 offset，假设格式为 'x / k * k + b - b'"""
    import re
    formula = formula.strip()
    # 简单处理：y = x / s + o 或 y = x * s + o
    m = re.match(r"x\s*/\s*([0-9.]+)", formula)
    if m:
        scale = 1.0 / float(m.group(1))
        return scale, 0.0
    m = re.match(r"x\s*\*\s*([0-9.]+)", formula)
    if m:
        return float(m.group(1)), 0.0
    m = re.match(r"\(x\s*-\s*([0-9.]+)\)\s*/\s*([0-9.]+)", formula)
    if m:
        offset = float(m.group(1))
        scale = 1.0 / float(m.group(2))
        return scale, -offset * scale
    return 1.0, 0.0


def cond_to_op(condition: str) -> str:
    """条件字符串转为 ConditionOp"""
    import re
    m = re.search(r"(>=|<=|==|!=|>|<)", condition.strip())
    if not m:
        return "GT"
    op = m.group(1)
    return {"==": "EQ", "!=": "NE", ">": "GT", "<": "LT", ">=": "GE", "<=": "LE"}.get(op, "GT")


def extract_threshold(condition: str) -> str:
    """从条件字符串提取阈值"""
    import re
    m = re.search(r"(-?[0-9.]+)", condition)
    return m.group(1) if m else "0"


def hex_color(color: str) -> str:
    """#RRGGBB → 0xRRGGBB"""
    c = color.lstrip("#")
    return "0x" + c.upper()


if __name__ == "__main__":
    generate_all()

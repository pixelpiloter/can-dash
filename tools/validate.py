#!/usr/bin/env python3
"""
YAML 配置校验器
运行时机：yaml_to_c.py 之前必须运行

检查内容：
1. JSON Schema 校验（字段类型/格式/枚举值）
2. 跨文件引用校验（display_key / widget id / can_id 等）

Usage:
    python tools/validate.py
"""

import json
import sys
import re
import yaml
from pathlib import Path
from typing import Any, Dict, List, Optional
from dataclasses import dataclass, field

# jsonschema 是可选依赖, 没装也能跑 (只是 schema 校验降级为 warning)
try:
    import jsonschema  # type: ignore
    _HAS_JSONSCHEMA = True
except ImportError:
    _HAS_JSONSCHEMA = False

CONFIG_DIR = Path(__file__).parent.parent / "config"
SCHEMA_DIR = Path(__file__).parent.parent / "schemas"


@dataclass
class ValidationError:
    file: str
    path: str
    message: str
    suggestion: Optional[str] = None


@dataclass
class ValidationResult:
    errors: List[ValidationError] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def add_error(self, file: str, path: str, msg: str, suggestion: str = None):
        self.errors.append(ValidationError(file, path, msg, suggestion))

    def add_warning(self, msg: str):
        self.warnings.append(msg)

    def ok(self) -> bool:
        return len(self.errors) == 0


def load_yaml(name: str) -> Dict:
    path = CONFIG_DIR / name
    if not path.exists():
        return {}
    with open(path) as f:
        return yaml.safe_load(f)


def validate_alarm_rules(result: ValidationResult):
    """校验 alarm_rules.yaml"""
    data = load_yaml("alarm_rules.yaml")
    can_data = load_yaml("can_ids.yaml")
    indicator_data = load_yaml("indicators.yaml")

    # 提取 can_ids.yaml 中定义的字段名
    defined_keys = set()
    for src in can_data.get("can_sources", []):
        for f in src.get("fields", []):
            defined_keys.add(f["name"])

    # 提取 indicators.yaml 中定义的 widget id
    defined_widgets = set()
    for ind in indicator_data.get("indicators", []):
        defined_widgets.add(ind["id"])

    alarm_rules = data.get("alarm_rules", [])
    if not alarm_rules:
        result.add_warning("alarm_rules.yaml: alarm_rules 列表为空")

    for rule in alarm_rules:
        name = rule.get("name", "<unnamed>")

        # display_key 校验
        dk = rule.get("display_key")
        if dk and dk not in defined_keys:
            result.add_error(
                "alarm_rules.yaml",
                f"alarm_rules[{name}].display_key",
                f"display_key='{dk}' 在 can_ids.yaml 中未定义",
                f"可用值: {sorted(defined_keys)}"
            )

        # condition 格式校验
        cond = rule.get("condition", "")
        if not re.match(r"^value\s*(>=|<=|==|!=|>|<)\s*-?[0-9.]+$", cond.strip()):
            result.add_error(
                "alarm_rules.yaml",
                f"alarm_rules[{name}].condition",
                f"condition 格式错误: '{cond}'",
                "正确格式: 'value > 420' 或 'value < 10'"
            )

        # priority 枚举校验
        priority = rule.get("priority")
        if priority not in ("high", "medium", "low"):
            result.add_error(
                "alarm_rules.yaml",
                f"alarm_rules[{name}].priority",
                f"priority='{priority}' 必须是 high/medium/low 之一",
            )

        # actions 校验
        actions = rule.get("actions", [])
        if not actions:
            result.add_error(
                "alarm_rules.yaml",
                f"alarm_rules[{name}].actions",
                f"报警 '{name}' 没有定义任何 actions",
            )

        for j, action in enumerate(actions):
            action_type = action.get("type")
            if action_type not in ("indicator_light", "alarm_text", "alarm_image", "sound"):
                result.add_error(
                    "alarm_rules.yaml",
                    f"alarm_rules[{name}].actions[{j}].type",
                    f"type='{action_type}' 必须是 indicator_light/alarm_text/alarm_image/sound 之一",
                )

            if action_type == "indicator_light":
                widget = action.get("widget")
                if widget and widget not in defined_widgets:
                    result.add_error(
                        "alarm_rules.yaml",
                        f"alarm_rules[{name}].actions[{j}].widget",
                        f"widget='{widget}' 在 indicators.yaml 中未定义",
                        f"可用值: {sorted(defined_widgets)}"
                    )
                if not action.get("flash") and not action.get("widget"):
                    result.add_warning(
                        f"alarm_rules[{name}].actions[{j}]: indicator_light 建议设置 flash"
                    )

            if action_type == "alarm_text":
                if not action.get("text_zh"):
                    result.add_error(
                        "alarm_rules.yaml",
                        f"alarm_rules[{name}].actions[{j}].text_zh",
                        "alarm_text 类型必须设置 text_zh"
                    )
                color = action.get("color", "#FF4400")
                if not re.match(r"^#[0-9A-Fa-f]{6}$", color):
                    result.add_error(
                        "alarm_rules.yaml",
                        f"alarm_rules[{name}].actions[{j}].color",
                        f"color='{color}' 格式错误",
                        "正确格式: #RRGGBB 如 #FF4400"
                    )


def validate_can_ids(result: ValidationResult):
    """校验 can_ids.yaml"""
    data = load_yaml("can_ids.yaml")
    can_sources = data.get("can_sources", [])
    if not can_sources:
        result.add_warning("can_ids.yaml: can_sources 列表为空")

    defined_names = set()
    for src in can_sources:
        name = src.get("name", "<unnamed>")
        can_id = src.get("can_id")

        if not can_id:
            result.add_error("can_ids.yaml", f"can_sources[{name}].can_id", "can_id 未定义")
            continue

        # 检查 can_id 唯一性
        if can_id in defined_names:
            result.add_error("can_ids.yaml", f"can_sources[{name}].can_id",
                           f"can_id 0x{can_id:X} 重复定义")
        defined_names.add(can_id)

        fields = src.get("fields", [])
        field_names = set()
        for f in fields:
            fname = f.get("name")
            if not fname:
                result.add_error("can_ids.yaml", f"can_sources[{name}].fields",
                               "field.name 未定义")
                continue

            if fname in field_names:
                result.add_error("can_ids.yaml", f"can_sources[{name}].fields[{fname}].name",
                               f"字段名 '{fname}' 重复定义")
            field_names.add(fname)

            # byte 范围校验
            byte = f.get("byte")
            if byte is None:
                result.add_error("can_ids.yaml", f"can_sources[{name}].fields[{fname}].byte",
                               "byte 未定义")
            elif isinstance(byte, list):
                if len(byte) != 2 or byte[0] > byte[1] or byte[1] > 7:
                    result.add_error("can_ids.yaml", f"can_sources[{name}].fields[{fname}].byte",
                                   f"byte 范围错误: {byte}，应为 [start, end] 且 end <= 7")
            elif not isinstance(byte, int):
                result.add_error("can_ids.yaml", f"can_sources[{name}].fields[{fname}].byte",
                               f"byte 类型错误: {type(byte).__name__}，应为 int 或 [int,int]")


def validate_seat_belt(result: ValidationResult):
    """校验 seat_belt.yaml"""
    data = load_yaml("seat_belt.yaml")
    can_data = load_yaml("can_ids.yaml")

    defined_can_ids = {src["can_id"] for src in can_data.get("can_sources", [])}

    seat_belt = data.get("seat_belt", {})
    positions = seat_belt.get("positions", [])
    if not positions:
        result.add_warning("seat_belt.yaml: positions 列表为空")

    trigger = seat_belt.get("trigger", {})
    if "speed_threshold" not in trigger:
        result.add_warning("seat_belt.yaml: trigger.speed_threshold 未定义，使用默认值 5.0")

    for pos in positions:
        pid = pos.get("id", "<unnamed>")

        # CAN ID 跨文件引用校验
        can_ids = pos.get("can_ids", {})
        for key, val in can_ids.items():
            if isinstance(val, dict):
                cid = val.get("id", 0)
            else:
                cid = val
            if cid and cid not in defined_can_ids:
                result.add_error(
                    "seat_belt.yaml",
                    f"seat_belt.positions[{pid}].can_ids.{key}",
                    f"CAN ID 0x{cid:X} 在 can_ids.yaml 中未定义"
                )


def validate_indicators(result: ValidationResult):
    """校验 indicators.yaml"""
    data = load_yaml("indicators.yaml")
    indicators = data.get("indicators", [])
    if not indicators:
        result.add_warning("indicators.yaml: indicators 列表为空")

    ids = set()
    for ind in indicators:
        iid = ind.get("id")
        if not iid:
            result.add_error("indicators.yaml", "indicators[?].id", "id 未定义")
            continue
        if iid in ids:
            result.add_error("indicators.yaml", f"indicators[{iid}].id", f"id '{iid}' 重复定义")
        ids.add(iid)


def validate_display_layout(result: ValidationResult):
    """校验 display_layout.yaml"""
    data = load_yaml("display_layout.yaml")
    layout = data.get("display", {})
    if not layout:
        result.add_warning("display_layout.yaml: display 节点为空")

    pages = layout.get("pages", [])
    for page in pages:
        components = page.get("layers", page.get("components", []))
        for comp in components:
            comp_id = comp.get("id")
            if not comp_id:
                result.add_error("display_layout.yaml", "components[?].id", "component id 未定义")


def validate_vehicle_thresholds(result: ValidationResult):
    """校验 vehicle_thresholds.yaml (v3 探针)

    检查:
    1. JSON Schema (字段类型 + 取值范围) — 借 jsonschema 库
    2. 业务逻辑不变量 (warning > critical, disengage > engage, etc.)
    """
    data = load_yaml("vehicle_thresholds.yaml")
    thresholds = data.get("vehicle_thresholds")
    if not thresholds:
        result.add_error(
            "vehicle_thresholds.yaml", "vehicle_thresholds",
            "vehicle_thresholds 节点缺失或为空"
        )
        return

    # ── JSON Schema 校验 ──
    schema_path = SCHEMA_DIR / "vehicle_thresholds.schema.json"
    if schema_path.exists():
        if not _HAS_JSONSCHEMA:
            result.add_warning(
                "jsonschema 未安装, 跳过 schema 校验 (pip install jsonschema)"
            )
        else:
            try:
                with open(schema_path) as f:
                    schema = json.load(f)
                jsonschema.validate(data, schema)
            except jsonschema.ValidationError as e:
                result.add_error(
                    "vehicle_thresholds.yaml",
                    f"vehicle_thresholds.{list(e.absolute_path)}",
                    e.message
                )
    else:
        result.add_warning(
            f"schemas/vehicle_thresholds.schema.json 不存在, 跳过 schema 校验"
        )

    # ── 业务逻辑不变量 (yaml 单独表达, schema 写不下的语义) ──
    if "soc_warning_low" in thresholds and "soc_critical_low" in thresholds:
        if thresholds["soc_warning_low"] <= thresholds["soc_critical_low"]:
            result.add_error(
                "vehicle_thresholds.yaml", "vehicle_thresholds.soc_warning_low",
                f"soc_warning_low ({thresholds['soc_warning_low']}) 必须 > "
                f"soc_critical_low ({thresholds['soc_critical_low']})",
                "warning 等级应高于 critical, 否则告警永远不触发"
            )

    if "readygo_speed_engage_kmh" in thresholds and "readygo_speed_disengage_kmh" in thresholds:
        if thresholds["readygo_speed_engage_kmh"] >= thresholds["readygo_speed_disengage_kmh"]:
            result.add_error(
                "vehicle_thresholds.yaml", "vehicle_thresholds.readygo_speed_engage_kmh",
                f"engage ({thresholds['readygo_speed_engage_kmh']}) 必须 < "
                f"disengage ({thresholds['readygo_speed_disengage_kmh']})",
                "否则 ReadyGo 会出现 engage=disengage 时立刻关闭的死循环"
            )

    if "precharge_auto_done_ms" in thresholds and "precharge_timeout_ms" in thresholds:
        if thresholds["precharge_auto_done_ms"] >= thresholds["precharge_timeout_ms"]:
            result.add_error(
                "vehicle_thresholds.yaml", "vehicle_thresholds.precharge_auto_done_ms",
                f"auto_done ({thresholds['precharge_auto_done_ms']}ms) 必须 < "
                f"timeout ({thresholds['precharge_timeout_ms']}ms)",
                "否则预充电永远先 timeout 再 done, 永远走 FAILED 路径"
            )


def validate_cross_references(result: ValidationResult):
    """跨文件引用校验"""
    alarm_data = load_yaml("alarm_rules.yaml")
    indicator_data = load_yaml("indicators.yaml")

    defined_widgets = {ind["id"] for ind in indicator_data.get("indicators", [])}
    used_widgets = set()

    for rule in alarm_data.get("alarm_rules", []):
        for action in rule.get("actions", []):
            if action.get("type") == "indicator_light":
                w = action.get("widget")
                if w:
                    used_widgets.add(w)

    undefined = used_widgets - defined_widgets
    for w in undefined:
        result.add_error(
            "alarm_rules.yaml",
            f"actions.widget='{w}'",
            f"widget '{w}' 在 alarm_rules 中使用但未在 indicators.yaml 中定义",
            f"请在 indicators.yaml 中添加 id: '{w}' 的定义"
        )


def validate_display_key_three_way(result: ValidationResult):
    """display_key 三处一致性校验：can_ids.yaml ↔ dashboard_backend_qt.cpp ↔ QML"""
    # 1. 收集 can_ids.yaml 定义的 display_key 列表
    can_data = load_yaml("can_ids.yaml")
    defined_keys = set()
    for src in can_data.get("can_sources", []):
        for f in src.get("fields", []):
            defined_keys.add(f["name"])

    # 2. 收集 C++ (dashboard_backend_qt.cpp) 中 Q_PROPERTY 引用的 key
    project_root = Path(__file__).parent.parent
    backend_cpp = project_root / "src" / "layer3" / "dashboard_backend_qt.cpp"
    cpp_keys = set()
    if backend_cpp.exists():
        content = backend_cpp.read_text(encoding="utf-8")
        # 匹配 Q_PROPERTY 声明中的属性名 + READ getter 模式
        for m in re.finditer(r"Q_PROPERTY\s+\w+\s+(\w+)\s+READ", content):
            cpp_keys.add(m.group(1))

    # 3. 收集 QML 中 Connections / property 引用
    qml_dir = project_root / "src" / "ui"
    qml_keys = set()
    if qml_dir.exists():
        for qml_file in qml_dir.glob("*.qml"):
            content = qml_file.read_text(encoding="utf-8")
            # 匹配 property real xxx / property bool xxx / property int xxx
            for m in re.finditer(r"property\s+(?:real|int|bool|string)\s+(\w+)", content):
                qml_keys.add(m.group(1))
            # 匹配 Connections { target: backend; function onXxxChanged() {...} } 模式
            for m in re.finditer(r"function\s+on(\w+)Changed\s*\(", content):
                qml_keys.add(m.group(1))

    # 4. 报告：can_ids 定义但 C++ 缺失的 key
    if backend_cpp.exists():
        missing_in_cpp = defined_keys - cpp_keys
        for k in sorted(missing_in_cpp):
            result.add_warning(
                f"display_key '{k}' 在 can_ids.yaml 中定义，但 dashboard_backend_qt.cpp 未暴露为 Q_PROPERTY"
            )

    # 5. 报告：C++ 有 Q_PROPERTY 但 can_ids 未定义
    if backend_cpp.exists():
        extra_in_cpp = cpp_keys - defined_keys
        for k in sorted(extra_in_cpp):
            result.add_error(
                "dashboard_backend_qt.cpp",
                f"Q_PROPERTY '{k}'",
                f"Q_PROPERTY '{k}' 在 dashboard_backend_qt.cpp 中声明但 can_ids.yaml 未定义",
                f"在 can_ids.yaml 的对应 can_source.fields 中添加 name: '{k}'"
            )

    # 6. 报告：QML 引用但 can_ids 未定义
    # 注：QML 属性常为 camelCase（vehicleSpeed），can_ids 多为 snake_case（vehicle_speed），
    #     二者通过 C++ Q_PROPERTY 桥接。因此这里只做 WARNING（不阻断），
    #     真正的一致性由 C++ ↔ YAML 这一层把关。
    if qml_dir.exists():
        # 构造 snake_case 集合用于模糊匹配
        camel_to_snake = {k: re.sub(r"(?<!^)(?=[A-Z])", "_", k).lower() for k in qml_keys}
        defined_snake = {k.lower() for k in defined_keys}
        for k in sorted(qml_keys):
            # 排除 QML 内置属性
            if k in ("color", "text", "width", "height", "x", "y", "z", "opacity",
                     "visible", "enabled", "anchors", "source", "radius", "border",
                     "running", "value", "from", "to", "duration", "easing",
                     "horizontalAlignment", "verticalAlignment", "wrapMode",
                     "elide", "font", "smooth", "antialiasing", "transform"):
                continue
            # 模糊匹配：camelCase → snake_case
            snake = camel_to_snake.get(k, k)
            if snake.lower() not in defined_snake:
                result.add_warning(
                    f"QML property/onXxxChanged '{k}'（→ snake_case: '{snake}'）"
                    f" 不在 can_ids.yaml 中。请确认："
                    f"(1) C++ Q_PROPERTY '{snake}' 是否已声明；"
                    f"(2) can_ids.yaml 是否缺该字段"
                )


def validate_all() -> ValidationResult:
    result = ValidationResult()
    validate_alarm_rules(result)
    validate_can_ids(result)
    validate_seat_belt(result)
    validate_indicators(result)
    validate_display_layout(result)
    validate_vehicle_thresholds(result)  # v3 探针
    validate_cross_references(result)
    validate_display_key_three_way(result)
    return result


def main():
    print("=" * 60)
    print("CAN-Dash YAML 配置校验")
    print("=" * 60)

    result = validate_all()

    # 打印警告
    for w in result.warnings:
        print(f"  ⚠️  {w}")

    # 打印错误
    if result.errors:
        print(f"\n❌ 校验失败，共 {len(result.errors)} 个错误:\n")
        for err in result.errors:
            print(f"  📄 {err.file}")
            print(f"     路径: {err.path}")
            print(f"     错误: {err.message}")
            if err.suggestion:
                print(f"     建议: {err.suggestion}")
            print()

        print("请修复上述错误后重新运行 validate.py")
        print("确认无误后运行: python tools/yaml_to_c.py")
        sys.exit(1)
    else:
        print(f"\n✅ 校验通过（{len(result.warnings)} 个警告）")
        print("下一步: python tools/yaml_to_c.py")
        sys.exit(0)


if __name__ == "__main__":
    main()

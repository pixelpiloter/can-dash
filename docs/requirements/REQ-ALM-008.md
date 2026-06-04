#REQ-ALM-008|发动机驱动模式指示灯控制 (Engine Boost Active)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: High
**来源**:   alarm_rules.yaml (engine_boost_active) / REQ-HYBRID-001.md
**创建日期**: 2026-05-31
**实现版本**: PR 25 (2026-06-04) — alarm_rules.yaml:engine_boost_active (L117)

---

## 1. 概述

### 1.1 需求描述
当 energy_mode == 2 (发动机单独驱动) 时，点亮发动机指示灯，熄灭纯电和混动指示灯。

### 1.2 背景与动机
发动机直驱模式需要与其他模式明确区分，参考比亚迪DM-i车型的能量流显示逻辑。

---

## 2. 功能需求

### 2.1 触发条件
- energy_mode == 2 (ENGINE_ONLY)
- 持续时间 ≥ 100ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| energy_mode | CAN总线 (0x300, byte 0) | uint8 | 0=EV, 1=HYBRID, 2=ENGINE_ONLY, 3=CHARGE |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| engine_run_light | 指示灯常亮 | widget id: engine_run_light |
| ev_mode_light | 指示灯熄灭 | widget id: ev_mode_light |
| hybrid_mode_light | 指示灯熄灭 | widget id: hybrid_mode_light |
| energy_flow_light | 指示灯常亮 | widget id: energy_flow_light |

### 2.4 处理逻辑
```
[energy_mode == 2] → 持续 100ms → 模式控制
    ├── engine_run_light ON（白色/橙色）
    ├── ev_mode_light OFF
    ├── hybrid_mode_light OFF
    └── energy_flow_light ON（表示能量流动）
```

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 2 |
| 优先级 | alarm_rules.yaml | priority | low |
| 防抖 | alarm_rules.yaml | duration_ms | 100 |
| 指示灯 | alarm_rules.yaml | widget | engine_run_light, energy_flow_light |

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` (engine_boost_active 规则, L117) |
| 生成文件 | `src/generated/alarm_rule_table.cpp` (ALARM_RULE_TABLE 索引 7) |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (`onValueChanged("energy_mode", 2)`) |
| QML组件 | `src/ui/IndicatorLight.qml` (ev_mode_light / hybrid_mode_light / engine_run_light / energy_flow_light) |
| 验证日期 | 2026-06-04 |
| 验证结果 | 18/18 ctest pass (含 alarm_rule_table 18 条规则) |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 2.0 | 状态同步 Approved → Implemented (PR 25, engine_boost_active 规则已实现, 修 PR 24 留下的"三角矛盾" — INDEX 标题/文件标题/规则名现已一致) | can-dash-jd-autopilot |

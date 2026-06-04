#REQ-ALM-009|充电模式指示灯控制 (Charge Mode Active)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   alarm_rules.yaml (charge_mode_active) / REQ-HYBRID-001.md
**创建日期**: 2026-05-31
**实现版本**: PR 25 (2026-06-04) — alarm_rules.yaml:charge_mode_active (L136)

---

## 1. 概述

### 1.1 需求描述
当 energy_mode == 3 (充电模式) 时，能量流指示灯以 2Hz 频率闪烁，表示正在充电。

### 1.2 背景与动机
充电模式（动能回收+外接电源）需要明显区分于行驶模式。

---

## 2. 功能需求

### 2.1 触发条件
- energy_mode == 3 (CHARGE_MODE)
- 持续时间 ≥ 100ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| energy_mode | CAN总线 (0x300, byte 0) | uint8 | 0=EV, 1=HYBRID, 2=ENGINE_ONLY, 3=CHARGE |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| energy_flow_light | 指示灯闪烁 | widget id: energy_flow_light, 2Hz |

### 2.4 处理逻辑
```
[energy_mode == 3] → 持续 100ms → 充电模式
    └── energy_flow_light 闪烁（2Hz，黄色）
```

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 3 |
| 优先级 | alarm_rules.yaml | priority | low |
| 防抖 | alarm_rules.yaml | duration_ms | 100 |
| 闪烁频率 | alarm_rules.yaml | flash_hz | 2 |
| 指示灯 | alarm_rules.yaml | widget | energy_flow_light |

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` (charge_mode_active 规则, L136) |
| 生成文件 | `src/generated/alarm_rule_table.cpp` (ALARM_RULE_TABLE 索引 8) |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (`onValueChanged("energy_mode", 3)`) |
| QML组件 | `src/ui/IndicatorLight.qml` (energy_flow_light) |
| 验证日期 | 2026-06-04 |
| 验证结果 | 18/18 ctest pass (含 alarm_rule_table 18 条规则) |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 2.0 | 状态同步 Approved → Implemented (PR 25, charge_mode_active 规则已实现) | can-dash-jd-autopilot |

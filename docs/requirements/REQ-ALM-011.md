#REQ-ALM-011|充电异常报警
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   alarm_rules.yaml (charge_fault_alarm) / REQ-HYBRID-001.md
**创建日期**: 2026-05-31
**实现版本**: PR 25 (2026-06-04) — alarm_rules.yaml:charge_fault_alarm (L163)

---

## 1. 概述

### 1.1 需求描述
当充电故障标志位为 1 时，触发充电异常报警横幅，charge_fault_light 闪烁（2Hz）。

### 1.2 背景与动机
充电过程中发生故障需要立即提醒驾驶员，以防止电池过充或其他安全问题。

### 1.3 相关需求
- REQ-SIG-XXX: charge_fault 信号

---

## 2. 功能需求

### 2.1 触发条件
- charge_fault == 1
- 持续时间 ≥ 500ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| charge_fault | CAN总线 (0x306, bit 2) | bool | 0=正常, 1=故障 |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| alarm_text | QML 报警横幅 | QString: "充电异常！" |
| alarm_text_en | QML 报警横幅 | QString: "CHARGE FAULT" |
| indicator | charge_fault_light | widget id: charge_fault_light, 2Hz闪烁 |

### 2.4 处理逻辑
```
[charge_fault == 1] → 持续 500ms → 触发充电故障报警
    ├── charge_fault_light 闪烁（2Hz，红色）
    └── 报警横幅显示（红色）
```

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 1 |
| 优先级 | alarm_rules.yaml | priority | high |
| 防抖 | alarm_rules.yaml | duration_ms | 500 |
| 闪烁频率 | alarm_rules.yaml | flash_hz | 2 |
| 指示灯 | alarm_rules.yaml | widget | charge_fault_light |

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` (charge_fault_alarm 规则, L163) |
| 生成文件 | `src/generated/alarm_rule_table.cpp` (ALARM_RULE_TABLE 索引 10) |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (`onValueChanged("charge_fault", 1)`) |
| QML组件 | `src/ui/AlarmBanner.qml` (charge_fault_light + 报警横幅) |
| 验证日期 | 2026-06-04 |
| 验证结果 | 18/18 ctest pass (含 alarm_rule_table 18 条规则) |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 2.0 | 状态同步 Approved → Implemented (PR 25, charge_fault_alarm 规则已实现, 修正 PR 24 INDEX 行号引用错位 — 之前 010 误指 charge_fault_alarm 实际属于 011) | can-dash-jd-autopilot |

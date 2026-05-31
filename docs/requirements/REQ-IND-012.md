#REQ-IND-012|充电故障指示灯 (Charge Fault Light)
=========================================

**状态**:   Approved
**类型**:   Safety
**优先级**: High
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (charge_fault_alarm)
**创建日期**: 2026-05-31
**实现版本**: v1.0

---

## 1. 概述

### 1.1 需求描述
充电故障指示灯在充电异常（charge_fault == 1）时闪烁（2Hz），提醒驾驶员充电系统存在问题。

### 1.2 背景与动机
充电故障可能涉及电池安全，需要高优先级提醒驾驶员。

### 1.3 相关需求
- REQ-ALM-011: charge_fault_alarm (触发逻辑)
- REQ-SIG-XXX: charge_fault 信号

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | charge_fault_light |
| 类型 | light |
| 颜色 | 红色 |
| 位置 | x=450, y=260 |
| 图片(亮) | charge_fault_red.png |
| 图片(暗) | charge_fault_red_dim.png |
| 尺寸 | 50x50 |
| flash_on_fault | true |

### 2.2 状态切换逻辑

| 条件 | 显示状态 | 闪烁 |
|------|---------|------|
| charge_fault == 1 | image_on (红色) | 是 (2Hz) |
| charge_fault == 0 | image_off (暗) | 否 |
| 信号超时 | image_off (暗) | 否 |

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 闪烁频率 | alarm_rules.yaml | flash_hz | 2 |
| 触发条件 | alarm_rules.yaml | condition | value == 1 |
| 防抖 | alarm_rules.yaml | duration_ms | 500 |

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` |
| 控制规则 | `config/alarm_rules.yaml` (charge_fault_alarm) |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

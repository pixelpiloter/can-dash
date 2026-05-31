#REQ-IND-009|发动机运行指示灯 (Engine Run Light)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (engine_boost_active / engine_fault_alarm)
**创建日期**: 2026-05-31
**实现版本**: v1.0

---

## 1. 概述

### 1.1 需求描述
发动机运行指示灯用于显示发动机是否处于工作状态。当 energy_mode == 2 (发动机驱动) 或 engine_fault == 1 (故障) 时触发不同状态。

### 1.2 背景与动机
驾驶员需要知道发动机是否启动，尤其在EV模式下发动机熄灭的体验需要明确标识。

### 1.3 相关需求
- REQ-ALM-008: engine_boost_active (发动机驱动模式)
- REQ-ALM-010: engine_fault_alarm (发动机故障)
- REQ-SIG-XXX: engine_rpm, engine_fault 信号

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | engine_run_light |
| 类型 | light |
| 颜色 | 白色 (#FFFFFF) |
| 位置 | x=540, y=260 |
| 图片(亮) | engine_run_white.png |
| 图片(暗) | engine_run_dim.png |
| 尺寸 | 60x60 |

### 2.2 状态切换逻辑

| 条件 | 显示状态 | 闪烁 |
|------|---------|------|
| energy_mode == 2 | image_on (白色常亮) | 否 |
| engine_fault == 1 | image_on (红色) | 是 (2Hz) |
| 其他模式 | image_off (暗) | 否 |
| 信号超时 | image_off (暗) | 否 |

### 2.3 优先级
故障状态 (engine_fault == 1) 优先级最高，覆盖正常发动机运行状态。

---

## 3. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` |
| 控制规则 | `config/alarm_rules.yaml` (engine_boost_active, engine_fault_alarm) |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

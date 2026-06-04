#REQ-IND-006|高压指示灯 (High Voltage Light)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有)
**创建日期**: 2026-05-31
**实现版本**: indicators.yaml:high_voltage_light (L32) + IndicatorLight.qml

---

## 1. 概述

### 1.1 需求描述
高压指示灯用于显示车辆高压系统（如电池）是否处于工作状态。混动车型需明确显示高压状态以满足 GB 18384-2020 安全标识要求。

### 1.2 背景与动机
新能源汽车高压系统（>60V）需要明确标识，防止人员触电风险。GB 18384-2020 要求B级电压部件有高压警示标志。

### 1.3 相关需求
- REQ-IND-001: 电池警告指示灯
- REQ-SIG-001: bat_volt 信号

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | high_voltage_light |
| 类型 | light |
| 颜色 | 橙色 (#FF8800) |
| 位置 | x=20, y=20 |
| 图片(亮) | hv_orange.png |
| 图片(暗) | hv_dim.png |
| 尺寸 | 50x50 |

### 2.2 状态切换逻辑

| 条件 | 显示状态 |
|------|---------|
| 高压系统激活 | image_on (橙色亮) |
| 高压系统关闭 | image_off (暗) |
| 信号超时 | image_off (暗) |

### 2.3 边界条件与异常处理
- bat_volt 信号超时 (>1000ms) → 熄灭（安全优先）
- 显示位置固定，不随界面缩放

---

## 3. 非功能需求

### 3.1 性能要求
- 状态切换延迟: ≤ 100ms

### 3.2 安全性需求
- ISO 26262 ASIL B（高压系统相关）
- 安全相关指示灯必须实时反映系统状态

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` (high_voltage_light L32, 50x50, 左上角位置, orange #FF8800) |
| QML组件 | `src/ui/IndicatorLight.qml` (通用 light 渲染, image_on/image_off 切换) |
| 状态信号 | `bat_volt` (REQ-SIG-001) — 高于阈值激活, 来自 can_ids.yaml BMS 帧 |
| 关联 L2 组件 | `src/layer2/can_converter.cpp` (信号桥接) + `src/layer2/alarm_runtime.cpp` (可选告警联动) |
| QML 显示位置 | `src/ui/DashboardMain.qml` 左上角, 始终可见, 不随视图切换 |
| 验证日期 | 2026-06-04 |
| 验证结果 | ctest 18/18 pass (含 IndicatorRuntimeTest 105 行覆盖 onValueChanged / signal mapping); yaml_to_c.py 生成的 indicator_runtime 数据表 0 错 |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 元数据头部 + §4 实现追踪同步: 状态 Approved → Implemented, 实现版本 + indicators.yaml 行号 + 状态信号 + 关联 L2 + QML 位置 + 验证日期/结果 (PR 33) | can-dash-jd-autopilot |

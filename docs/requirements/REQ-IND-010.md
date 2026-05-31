#REQ-IND-010|能量流指示灯 (Energy Flow Light)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Low
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (engine_boost_active / charge_mode_active)
**创建日期**: 2026-05-31
**实现版本**: v1.0

---

## 1. 概述

### 1.1 需求描述
能量流指示灯用于显示车辆能量流动状态，在发动机驱动模式和充电模式下亮起/闪烁。

### 1.2 背景与动机
驾驶员需要了解当前能量流向（驱动/回收/充电），参考比亚迪DM-i能量流显示逻辑。

### 1.3 相关需求
- REQ-ALM-008: engine_boost_active (能量流动)
- REQ-ALM-009: charge_mode_active (充电模式闪烁)
- REQ-SIG-XXX: energy_mode 信号

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | energy_flow_light |
| 类型 | light |
| 颜色 | 黄色 (#FFCC00) |
| 位置 | x=320, y=180 |
| 图片(亮) | energy_flow_active.png |
| 图片(暗) | energy_flow_inactive.png |
| 尺寸 | 60x60 |

### 2.2 状态切换逻辑

| 条件 | 显示状态 | 闪烁 |
|------|---------|------|
| energy_mode == 2 (发动机驱动) | image_on (黄色常亮) | 否 |
| energy_mode == 3 (充电模式) | image_on (黄色) | 是 (2Hz) |
| 其他模式 | image_off (暗) | 否 |

---

## 3. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` |
| 控制规则 | `config/alarm_rules.yaml` (engine_boost_active, charge_mode_active) |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

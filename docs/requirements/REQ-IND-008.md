#REQ-IND-008|混动模式指示灯 (Hybrid Mode Light)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (hybrid_mode_active)
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 需求描述
混动模式指示灯在车辆处于混动模式（energy_mode == 1）时亮起，表示电机和发动机共同驱动。

### 1.2 背景与动机
混动模式需要与纯电模式和发动机模式明确区分。

### 1.3 相关需求
- REQ-ALM-007: hybrid_mode_active (触发逻辑)
- REQ-SIG-XXX: energy_mode 信号

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | hybrid_mode_light |
| 类型 | light |
| 颜色 | 蓝色 (#0066FF) |
| 位置 | x=260, y=180 |
| 图片(亮) | hybrid_mode_blue.png |
| 图片(暗) | hybrid_mode_dim.png |
| 尺寸 | 60x60 |

### 2.2 状态切换逻辑

| 条件 | 显示状态 |
|------|---------|
| energy_mode == 1 | image_on (蓝色亮) |
| energy_mode != 1 | image_off (暗) |
| 信号超时 | image_off (暗) |

---

## 3. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` |
| 控制规则 | `config/alarm_rules.yaml` (hybrid_mode_active) |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

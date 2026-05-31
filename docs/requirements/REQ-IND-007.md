#REQ-IND-007|纯电模式指示灯 (EV Mode Light)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (ev_mode_active)
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 需求描述
纯电模式指示灯在车辆处于EV模式（energy_mode == 0）时亮起，告知驾驶员当前为纯电驱动。

### 1.2 背景与动机
EV模式指示是PHEV/HEV车型仪表盘的标准功能，参考比亚迪秦Plus DM-i。

### 1.3 相关需求
- REQ-ALM-006: ev_mode_active (触发逻辑)
- REQ-SIG-XXX: energy_mode 信号

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | ev_mode_light |
| 类型 | light |
| 颜色 | 绿色 (#00FF00) |
| 位置 | x=200, y=180 |
| 图片(亮) | ev_mode_green.png |
| 图片(暗) | ev_mode_dim.png |
| 尺寸 | 60x60 |

### 2.2 状态切换逻辑

| 条件 | 显示状态 |
|------|---------|
| energy_mode == 0 | image_on (绿色亮) |
| energy_mode != 0 | image_off (暗) |
| 信号超时 | image_off (暗) |

### 2.3 闪烁逻辑
- 默认常亮（energy_mode == 0 时）
- 闪烁由 alarm_rules.yaml 中的 ev_mode_active 规则控制（当前配置为不闪烁）

---

## 3. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` |
| 控制规则 | `config/alarm_rules.yaml` (ev_mode_active) |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

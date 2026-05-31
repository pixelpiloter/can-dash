#REQ-IND-011|充电中指示灯 (Charge Light)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有)
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 需求描述
充电中指示灯在车辆正在充电时亮起（蓝色），告知驾驶员当前处于充电状态。

### 1.2 背景与动机
充电状态显示是PHEV车型仪表盘必备功能，参考比亚迪秦Plus DM-i充电界面。

### 1.3 相关需求
- REQ-SIG-XXX: charge_status 信号
- REQ-HYBRID-001: 充电状态显示基线

---

## 2. 功能需求

### 2.1 指示灯定义

| 字段 | 值 |
|------|-----|
| ID | charge_light |
| 类型 | light |
| 颜色 | 蓝色 |
| 位置 | x=400, y=260 |
| 图片(亮) | charge_blue.png |
| 图片(暗) | charge_blue_dim.png |
| 尺寸 | 50x50 |

### 2.2 状态切换逻辑

| 条件 | 显示状态 |
|------|---------|
| charge_status != 0 (充电中) | image_on (蓝色亮) |
| charge_status == 0 (未充电) | image_off (暗) |
| 信号超时 | image_off (暗) |

---

## 3. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/indicators.yaml` |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

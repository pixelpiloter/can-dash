#REQ-IND-011|充电中指示灯 (Charge Light)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (charge_mode_active L136)
**创建日期**: 2026-05-31
**实现版本**: indicators.yaml:charge_light (L71) + alarm_rules.yaml:charge_mode_active (L136)

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
| 实现文件 | `config/indicators.yaml` (charge_light L71, 50x50, x=400/y=260 位置, 蓝色) |
| 控制规则 | `config/alarm_rules.yaml` (charge_mode_active L136: energy_mode==3 触发) |
| QML组件 | `src/ui/IndicatorLight.qml` (image_on = charge_blue.png 亮, image_off = charge_blue_dim.png 暗) |
| 状态信号 | `energy_mode` (REQ-SIG-018) — 模式 3 触发, 等价 charge_status != 0 条件 |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (onValueChanged("energy_mode", 3) 触发 charge_mode_active) + `src/layer2/indicator_runtime.cpp` (亮/灭渲染) |
| QML 显示位置 | `src/ui/DashboardMain.qml` 状态灯区 (400,260, 跟 charge_fault_light L78 紧邻) |
| 验证日期 | 2026-06-04 |
| 验证结果 | ctest 18/18 pass (AlarmRuntimeTest 验证 charge_mode_active 规则 + IndicatorRuntimeTest 验证映射); yaml_to_c.py 生成的 ALARM_RULE_TABLE 含 charge_mode_active, INDICATOR_TABLE 含 charge_light |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 元数据头部 + §3 实现追踪同步: 状态 Approved → Implemented, 实现版本 + 行号 + 状态信号 + 关联 L2 + QML 位置 + 验证 (PR 33) | can-dash-jd-autopilot |

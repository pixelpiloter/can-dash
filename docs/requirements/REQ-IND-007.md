#REQ-IND-007|纯电模式指示灯 (EV Mode Light)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (ev_mode_active L85)
**创建日期**: 2026-05-31
**实现版本**: indicators.yaml:ev_mode_light (L47) + alarm_rules.yaml:ev_mode_active (L85)

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
| 实现文件 | `config/indicators.yaml` (ev_mode_light L47, 60x60, x=200/y=180 位置, green #00FF00) |
| 控制规则 | `config/alarm_rules.yaml` (ev_mode_active L85: energy_mode==0 触发) |
| QML组件 | `src/ui/IndicatorLight.qml` (image_on = ev_mode_green.png 亮, image_off = ev_mode_dim.png 暗) |
| 状态信号 | `energy_mode` (REQ-SIG-018) — 来自 can_ids.yaml VCU 帧 byte 3 |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (onValueChanged("energy_mode", v) 触发 ev_mode_active 评估) + `src/layer2/indicator_runtime.cpp` (亮/灭渲染) |
| QML 显示位置 | `src/ui/DashboardMain.qml` 中央 mode 选择区 (200,180 附近, 跟 hybrid_mode_light 紧邻) |
| 验证日期 | 2026-06-04 |
| 验证结果 | ctest 18/18 pass (含 AlarmRuntimeTest 验证 ev_mode_active 规则 + IndicatorRuntimeTest 验证映射); yaml_to_c.py 生成的 ALARM_RULE_TABLE 含 ev_mode_active, 生成的 INDICATOR_TABLE 含 ev_mode_light |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 元数据头部 + §3 实现追踪同步: 状态 Approved → Implemented, 实现版本 + 行号 + 状态信号 + 关联 L2 + QML 位置 + 验证 (PR 33) | can-dash-jd-autopilot |

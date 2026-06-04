#REQ-IND-008|混动模式指示灯 (Hybrid Mode Light)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (hybrid_mode_active L101)
**创建日期**: 2026-05-31
**实现版本**: indicators.yaml:hybrid_mode_light (L55) + alarm_rules.yaml:hybrid_mode_active (L101)

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
| 实现文件 | `config/indicators.yaml` (hybrid_mode_light L55, 60x60, x=260/y=180 位置, blue #0066FF) |
| 控制规则 | `config/alarm_rules.yaml` (hybrid_mode_active L101: energy_mode==1 触发) |
| QML组件 | `src/ui/IndicatorLight.qml` (image_on = hybrid_mode_blue.png 亮, image_off = hybrid_mode_dim.png 暗) |
| 状态信号 | `energy_mode` (REQ-SIG-018) — 来自 can_ids.yaml VCU 帧 byte 3 |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (onValueChanged("energy_mode", v) 触发 hybrid_mode_active 评估) + `src/layer2/indicator_runtime.cpp` (亮/灭渲染) |
| QML 显示位置 | `src/ui/DashboardMain.qml` 中央 mode 选择区 (260,180 附近, 紧邻 ev_mode_light / engine_run_light) |
| 验证日期 | 2026-06-04 |
| 验证结果 | ctest 18/18 pass (AlarmRuntimeTest 验证 hybrid_mode_active 规则 + IndicatorRuntimeTest 验证映射); yaml_to_c.py 生成的 ALARM_RULE_TABLE 含 hybrid_mode_active, INDICATOR_TABLE 含 hybrid_mode_light |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 元数据头部 + §3 实现追踪同步: 状态 Approved → Implemented, 实现版本 + 行号 + 状态信号 + 关联 L2 + QML 位置 + 验证 (PR 33) | can-dash-jd-autopilot |

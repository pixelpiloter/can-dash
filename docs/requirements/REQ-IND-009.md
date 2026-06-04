#REQ-IND-009|发动机运行指示灯 (Engine Run Light)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   indicators.yaml (已有) / alarm_rules.yaml (engine_boost_active L117 / engine_fault_alarm L147)
**创建日期**: 2026-05-31
**实现版本**: indicators.yaml:engine_run_light (L39) + alarm_rules.yaml:engine_boost_active (L117) + engine_fault_alarm (L147)

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
| 实现文件 | `config/indicators.yaml` (engine_run_light L39, 60x60, x=540/y=260 位置, white #FFFFFF) |
| 控制规则 | `config/alarm_rules.yaml` (engine_boost_active L117: energy_mode==2 触发 + engine_fault_alarm L147: engine_fault==1 触发, 优先级最高) |
| QML组件 | `src/ui/IndicatorLight.qml` (image_on = engine_run_white.png 亮, image_off = engine_run_dim.png 暗) |
| 状态信号 | `energy_mode` (REQ-SIG-018, 模式 2) + `engine_fault` (REQ-ALM-010 关联信号) |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (双规则评估, engine_fault 覆盖) + `src/layer2/indicator_runtime.cpp` (亮/灭/闪烁渲染) |
| QML 显示位置 | `src/ui/DashboardMain.qml` 故障灯区 (540,260, 跟 bat_warn_light 等并排) |
| 验证日期 | 2026-06-04 |
| 验证结果 | ctest 18/18 pass (AlarmRuntimeTest 验证 2 条 engine_* 规则 + IndicatorRuntimeTest 验证映射); yaml_to_c.py 生成的 ALARM_RULE_TABLE 含 engine_boost_active + engine_fault_alarm |

---

## 4. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 元数据头部 + §3 实现追踪同步: 状态 Approved → Implemented, 实现版本 + 行号 + 状态信号 + 关联 L2 + QML 位置 + 验证 (PR 33) | can-dash-jd-autopilot |

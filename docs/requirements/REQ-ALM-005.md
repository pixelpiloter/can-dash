#REQ-ALM-005|胎压低报警
=========================================

**状态**:   Proposed
**类型**:   Functional, Safety
**优先级**: Critical
**来源**:   用户需求（2026-06-01）
**创建日期**: 2026-06-01
**实现版本**: v1.0

---

## 1. 概述

### 1.1 需求描述
当任意轮胎的胎压低于 1.8 bar 时，仪表盘立即显示报警横幅并点亮胎压警告指示灯。

### 1.2 背景与动机
汽车安全标准要求驾驶员实时了解轮胎状态，胎压过低会导致操控性下降、制动距离增加甚至爆胎。此需求为法规强制要求。

### 1.3 相关需求
- REQ-IND-005: 高压指示灯（相关指示灯）
- REQ-SYS-001: CAN总线超时检测（信号丢失时显示"---"）

---

## 2. 功能需求

### 2.1 触发条件
- 胎压传感器信号 < 1.8 bar
- 信号持续低压超过 200ms（防抖）

### 2.2 输入

| 字段 | 来源 | 格式 | 正常范围 |
|------|------|------|---------|
| tire_pressure_fl | CAN总线 (0x3A0) | float (bar) | 2.0~2.9 bar |
| tire_pressure_fr | CAN总线 (0x3A1) | float (bar) | 2.0~2.9 bar |
| tire_pressure_rl | CAN总线 (0x3A2) | float (bar) | 2.0~2.9 bar |
| tire_pressure_rr | CAN总线 (0x3A3) | float (bar) | 2.0~2.9 bar |

### 2.3 输出

| 字段 | 目标 | 格式 |
|------|------|------|
| alarm_text | QML 报警横幅 | QString: "胎压低，请检查轮胎！" |
| alarm_text_en | QML 报警横幅 | QString: "LOW TIRE PRESSURE" |
| tire_warn_light | indicators.yaml | widget id: tire_pressure_light |
| alarm_priority | 报警优先级 | high |

### 2.4 处理逻辑

```
[CAN帧 0x3A0~0x3A3]
    │
    ▼
[CanConverter.processFrame] → 提取4个胎压值
    │
    ▼
[AlarmRuntime.onValueChanged] → 遍历 alarm_rules
    │
    ▼
[条件: value < 1.8] → 持续 200ms → 触发报警
    │
    ├── 点亮 tire_pressure_light（红色，闪烁 2Hz）
    └── 显示报警横幅 "胎压低，请检查轮胎！"
```

### 2.5 边界条件与异常处理

| 场景 | 处理 |
|------|------|
| 胎压信号超时 (>500ms) | 显示 "---"，不触发报警 |
| 胎压值 > 3.5 bar | 视为传感器故障，忽略本次更新 |
| 报警触发后胎压恢复正常 | 200ms 防抖后自动清除报警 |
| CAN总线断开 | 显示所有信号 "---"，清除所有报警 |

---

## 3. 非功能需求

### 3.1 性能要求
- 响应延迟: ≤ 100ms（信号变化到UI更新）
- 报警防抖: 200ms
- 指示灯闪烁频率: 2Hz (±0.1Hz)

### 3.2 安全性需求
- ISO 26262 ASIL B
- 传感器丢失时仪表必须显示"---"，不能显示旧值
- 报警清除必须基于信号实际恢复，不能仅靠超时

### 3.3 可靠性需求
- 连续运行 10000 小时无故障
- CAN总线断开后自动重连，无需重启应用

---

## 4. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 报警阈值 | alarm_rules.yaml | condition | value < 1.8 |
| 防抖时间 | alarm_rules.yaml | duration_ms | 200 |
| 优先级 | alarm_rules.yaml | priority | high |
| 指示灯 | alarm_rules.yaml | widget | tire_pressure_light |
| 信号超时 | can_signal_status.yaml | timeout_ms | 500 |
| 信号范围 | can_signal_status.yaml | min | 0.0 |
| 信号范围 | can_signal_status.yaml | max | 3.5 |
| CAN ID (FL) | can_ids.yaml | can_id | 0x3A0 |
| CAN ID (FR) | can_ids.yaml | can_id | 0x3A1 |
| CAN ID (RL) | can_ids.yaml | can_id | 0x3A2 |
| CAN ID (RR) | can_ids.yaml | can_id | 0x3A3 |

---

## 5. 测试用例

### TC-ALM-005-01|正常触发报警
- **前置条件**: 4个胎压信号均正常，值为 2.3 bar
- **输入**: 模拟 CAN 帧：0x3A0 data = [0x00, 0x17] (即 2.3 bar, x/10)
- **预期输出**: 200ms 后报警横幅显示 + tire_pressure_light 亮（闪烁）
- **通过标准**: PASS（报警横幅出现时间 ≤ 200ms）

### TC-ALM-005-02|报警自动清除
- **前置条件**: 报警已触发（胎压=1.5 bar）
- **输入**: 恢复正常：0x3A0 data = [0x00, 0x1F] (3.1 bar)
- **预期输出**: 200ms 后报警清除，指示灯熄灭
- **通过标准**: PASS（清除延迟 ≤ 200ms）

### TC-ALM-005-03|信号超时显示"---"
- **前置条件**: 胎压信号正常
- **输入**: 停止发送 0x3A0 帧，保持 600ms
- **预期输出**: 胎压显示 "---"，报警清除
- **通过标准**: PASS（500ms 后开始显示 "---"）

### TC-ALM-005-04|防抖验证
- **前置条件**: 胎压正常（2.3 bar）
- **输入**: 发送异常值（1.5 bar）持续 100ms，然后恢复正常（2.3 bar）
- **预期输出**: 不触发报警（100ms < 200ms 防抖）
- **通过标准**: PASS（无报警横幅出现）

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-ALM-005-01 | 正常触发 | FL=1.5bar | 报警+指示灯 | Approved |
| TC-ALM-005-02 | 自动清除 | 恢复正常 | 清除报警 | Approved |
| TC-ALM-005-03 | 信号超时 | 无帧600ms | 显示"---" | Approved |
| TC-ALM-005-04 | 防抖验证 | 异常100ms | 不报警 | Approved |

---

## 6. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/can_ids.yaml`, `config/alarm_rules.yaml`, `config/indicators.yaml` |
| 生成代码 | `src/generated/can_field_def.h`, `src/generated/alarm_rule_def.h` |
| QML组件 | `src/ui/AlarmBanner.qml`, `src/ui/IndicatorLight.qml` |
| 单元测试 | `tests/test_alarm_runtime.cpp` |
| 集成测试 | `can_sim/engine.py` (需新增 tire_pressure 仿真) |
| 验证日期 | - |
| 验证结果 | - |

---

## 7. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-06-01 | 1.0 | 初始创建 | requirements-document-agent |

#REQ-ALM-010|发动机故障报警
=========================================

**状态**:   Approved
**类型**:   Safety
**优先级**: High
**来源**:   alarm_rules.yaml (已有) / REQ-HYBRID-001.md
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 需求描述
当发动机故障标志位为 1 时，触发发动机故障报警横幅，engine_run_light 闪烁（2Hz）。

### 1.2 背景与动机
发动机故障可能影响行车安全，需要立即提醒驾驶员。

### 1.3 相关需求
- REQ-SIG-XXX: engine_fault 信号

---

## 2. 功能需求

### 2.1 触发条件
- engine_fault == 1
- 持续时间 ≥ 200ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| engine_fault | CAN总线 (0x305, byte 0, bit 0) | bool | 0=正常, 1=故障 |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| alarm_text | QML 报警横幅 | QString: "发动机故障！" |
| alarm_text_en | QML 报警横幅 | QString: "ENGINE FAULT" |
| indicator | engine_run_light | widget id: engine_run_light, 2Hz闪烁 |

### 2.4 处理逻辑
```
[engine_fault == 1] → 持续 200ms → 触发故障报警
    ├── engine_run_light 闪烁（2Hz）
    └── 报警横幅显示（红色）
```

### 2.5 边界条件与异常处理
- 故障清除 (engine_fault == 0) → 200ms 防抖后清除报警
- 信号超时 → 显示 "---"，不触发报警

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 1 |
| 优先级 | alarm_rules.yaml | priority | high |
| 防抖 | alarm_rules.yaml | duration_ms | 200 |
| 闪烁频率 | alarm_rules.yaml | flash_hz | 2 |
| 指示灯 | alarm_rules.yaml | widget | engine_run_light |

---

## 4. 测试用例

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-ALM-010-01 | 触发故障 | engine_fault=1 持续 250ms | 报警横幅+2Hz闪烁 | Approved |
| TC-ALM-010-02 | 清除故障 | engine_fault恢复到0 | 200ms后清除 | Approved |

---

## 5. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` |
| 生成代码 | `src/generated/alarm_rule_def.h` |
| QML组件 | `src/ui/AlarmBanner.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 6. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

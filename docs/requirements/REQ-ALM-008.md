#REQ-ALM-008|发动机驱动模式指示灯控制 (Engine Boost Active)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Low
**来源**:   alarm_rules.yaml (已有) / REQ-HYBRID-001.md
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 需求描述
当 energy_mode == 2 (发动机单独驱动) 时，点亮发动机指示灯，熄灭纯电和混动指示灯。

### 1.2 背景与动机
发动机直驱模式需要与其他模式明确区分，参考比亚迪DM-i车型的能量流显示逻辑。

---

## 2. 功能需求

### 2.1 触发条件
- energy_mode == 2 (ENGINE_ONLY)
- 持续时间 ≥ 100ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| energy_mode | CAN总线 (0x300, byte 0) | uint8 | 0=EV, 1=HYBRID, 2=ENGINE_ONLY, 3=CHARGE |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| engine_run_light | 指示灯常亮 | widget id: engine_run_light |
| ev_mode_light | 指示灯熄灭 | widget id: ev_mode_light |
| hybrid_mode_light | 指示灯熄灭 | widget id: hybrid_mode_light |
| energy_flow_light | 指示灯常亮 | widget id: energy_flow_light |

### 2.4 处理逻辑
```
[energy_mode == 2] → 持续 100ms → 模式控制
    ├── engine_run_light ON（白色/橙色）
    ├── ev_mode_light OFF
    ├── hybrid_mode_light OFF
    └── energy_flow_light ON（表示能量流动）
```

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 2 |
| 优先级 | alarm_rules.yaml | priority | low |
| 防抖 | alarm_rules.yaml | duration_ms | 100 |
| 指示灯 | alarm_rules.yaml | widget | engine_run_light, energy_flow_light |

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` |
| QML组件 | `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |

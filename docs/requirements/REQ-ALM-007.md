#REQ-ALM-007|混动模式指示灯控制 (Hybrid Mode Active)
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
当 energy_mode == 1 (混动模式) 时，点亮混动模式指示灯，熄灭纯电和发动机指示灯。

### 1.2 背景与动机
混动模式下电机和发动机同时参与驱动，需要明确标识当前状态。

### 1.3 相关需求
- REQ-ALM-006: 纯电模式指示 (ev_mode_active)
- REQ-ALM-008: 发动机驱动指示 (engine_boost_active)

---

## 2. 功能需求

### 2.1 触发条件
- energy_mode == 1 (HYBRID模式)
- 持续时间 ≥ 100ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| energy_mode | CAN总线 (0x300, byte 0) | uint8 | 0=EV, 1=HYBRID, 2=ENGINE_ONLY, 3=CHARGE |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| hybrid_mode_light | 指示灯常亮 | widget id: hybrid_mode_light (蓝色) |
| ev_mode_light | 指示灯熄灭 | widget id: ev_mode_light |
| engine_run_light | 指示灯熄灭 | widget id: engine_run_light |

### 2.4 处理逻辑
```
[energy_mode == 1] → 持续 100ms → 模式控制
    ├── hybrid_mode_light ON（蓝色，常亮）
    ├── ev_mode_light OFF
    └── engine_run_light OFF
```

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 1 |
| 优先级 | alarm_rules.yaml | priority | low |
| 防抖 | alarm_rules.yaml | duration_ms | 100 |
| 指示灯 | alarm_rules.yaml | widget | hybrid_mode_light |

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

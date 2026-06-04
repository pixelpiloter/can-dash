#REQ-ALM-006|纯电模式指示灯控制 (EV Mode Active)
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: High
**来源**:   alarm_rules.yaml (ev_mode_active) / REQ-HYBRID-001.md
**创建日期**: 2026-05-31
**实现版本**: PR 25 (2026-06-04) — alarm_rules.yaml:ev_mode_active (L85)

---

## 1. 概述

### 1.1 需求描述
当 energy_mode == 0 (EV模式) 时，点亮纯电模式指示灯，熄灭发动机和混动指示灯。

### 1.2 背景与动机
驾驶员需要清晰知道当前车辆的能量模式状态。EV模式指示是混动车型仪表盘的标准功能。

### 1.3 相关需求
- REQ-ALM-007: 混动模式指示 (hybrid_mode_active)
- REQ-ALM-008: 发动机驱动指示 (engine_boost_active)
- REQ-SIG-XXX: energy_mode 信号

---

## 2. 功能需求

### 2.1 触发条件
- energy_mode == 0 (EV模式)
- 持续时间 ≥ 100ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| energy_mode | CAN总线 (0x300, byte 0) | uint8 | 0=EV, 1=HYBRID, 2=ENGINE_ONLY, 3=CHARGE |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| ev_mode_light | 指示灯常亮 | widget id: ev_mode_light |
| hybrid_mode_light | 指示灯熄灭 | widget id: hybrid_mode_light |
| engine_run_light | 指示灯熄灭 | widget id: engine_run_light |

### 2.4 处理逻辑
```
[energy_mode == 0] → 持续 100ms → 模式控制
    ├── ev_mode_light ON（绿色，常亮）
    ├── hybrid_mode_light OFF
    └── engine_run_light OFF
```

### 2.5 边界条件与异常处理
- energy_mode 信号超时 → 所有模式灯熄灭（不确定状态）
- 切换到其他模式 → 对应规则触发，覆盖此规则

---

## 3. 非功能需求

### 3.1 性能要求
- 响应延迟: ≤ 100ms

---

## 4. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value == 0 |
| 优先级 | alarm_rules.yaml | priority | low |
| 防抖 | alarm_rules.yaml | duration_ms | 100 |
| 指示灯1 | alarm_rules.yaml | widget | ev_mode_light |
| 指示灯2 | alarm_rules.yaml | widget | hybrid_mode_light |
| 指示灯3 | alarm_rules.yaml | widget | engine_run_light |

---

## 5. 测试用例

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-ALM-006-01 | EV模式 | energy_mode=0 | ev_mode_light亮，其他灭 | Implemented |

---

## 6. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` (ev_mode_active 规则, L85) |
| 生成文件 | `src/generated/alarm_rule_table.cpp` (ALARM_RULE_TABLE 索引 5) |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (`onValueChanged("energy_mode", 0)`) |
| QML组件 | `src/ui/IndicatorLight.qml` (ev_mode_light / hybrid_mode_light / engine_run_light) |
| 验证日期 | 2026-06-04 |
| 验证结果 | 18/18 ctest pass (含 alarm_rule_table 18 条规则) |

---

## 7. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 2.0 | 状态同步 Approved → Implemented (PR 25, ev_mode_active 规则已实现) | can-dash-jd-autopilot |

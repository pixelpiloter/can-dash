#REQ-ALM-003|电量严重不足报警 (SOC<8%)
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
当电池 SOC 低于 8% 时，触发严重报警横幅，指示灯快速闪烁（3Hz），提醒驾驶员立即充电。

### 1.2 背景与动机
极度低电量会严重影响用户体验并可能触发电池保护机制。此阈值为行业通用临界值。

### 1.3 相关需求
- REQ-ALM-003 自身 = `soc_critical_low` (电量严重不足报警，SOC<8%, duration 300ms)
- REQ-SIG-002: `soc_critical_low` 关联信号定义
- REQ-HYBRID-001: SOC < 8% 触发严重报警 (跨条目)
- REQ-ALM-012: 10% 阈值的 "电量低报警" (`bat_soc_low` L37), 跟本条分层触发 (PR 27 拆分)

---

## 2. 功能需求

### 2.1 触发条件
- bat_soc < 8
- 持续时间 ≥ 300ms（防抖）

### 2.2 输入
| 字段 | 来源 | 格式 | 正常范围 |
|------|------|------|---------|
| bat_soc | CAN总线 (0x186040F3, byte 4) | uint8 % | 0~100% |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| alarm_text | QML 报警横幅 | QString: "电量严重不足，请立即充电！" |
| alarm_text_en | QML 报警横幅 | QString: "CRITICAL: EXTREMELY LOW BATTERY" |
| indicator | soc_warn_light | widget id: soc_warn_light |
| flash_hz | 闪烁频率 | 3 Hz |

### 2.4 处理逻辑
```
[bat_soc < 8] → 持续 300ms → 触发 soc_critical_low
    ├── soc_warn_light 闪烁（3Hz，红色）
    └── 报警横幅显示（红色，大字体）
```

### 2.5 边界条件与异常处理
- bat_soc 信号超时 → 显示 "---"，不触发此报警
- SOC 恢复到 ≥ 10% → 300ms 防抖后清除报警

---

## 3. 非功能需求

### 3.1 性能要求
- 响应延迟: ≤ 300ms（含 300ms 防抖）
- 闪烁频率: 3Hz (±0.1Hz)

### 3.2 安全性需求
- ISO 26262 ASIL B
- 电池严重低电量属于安全相关（可能导致行驶中断）

---

## 4. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value < 8 |
| 优先级 | alarm_rules.yaml | priority | high |
| 防抖 | alarm_rules.yaml | duration_ms | 300 |
| 闪烁频率 | alarm_rules.yaml | flash_hz | 3 |
| 指示灯 | alarm_rules.yaml | widget | soc_warn_light |

---

## 5. 测试用例

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-ALM-003-01 | 触发严重低电 | SOC=7% 持续 350ms | 报警横幅+3Hz闪烁 | Approved |
| TC-ALM-003-02 | 自动清除 | SOC恢复到12% | 300ms后清除 | Approved |

---

## 6. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` |
| 生成代码 | `src/generated/alarm_rule_def.h` |
| QML组件 | `src/ui/AlarmBanner.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 7. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建（从 alarm_rules.yaml 补充） | requirements-document-agent |
| 2026-06-04 | 1.1 | 修 1.3 节"相关需求"自引用: 原本混引 `bat_soc_low` (10% 规则) 跟本 .md 描述的 8% 严重规则, 改成清晰指 `soc_critical_low`. 三角矛盾决策 (a/b/c) 选 (b) 改 INDEX 对齐本文件 (PR 26) | can-dash-jd-autopilot |
| 2026-06-04 | 1.2 | 1.3 节去掉 PR 26 的 "待 PR 27 另立 012" 注, 改成对 REQ-ALM-012 的明确引用 (PR 27 拆分完成) | can-dash-jd-autopilot |

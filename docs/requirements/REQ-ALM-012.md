#REQ-ALM-012|电量低报警 (SOC<10%)
=========================================

**状态**:   Approved
**类型**:   Safety
**优先级**: Medium
**来源**:   alarm_rules.yaml (bat_soc_low) / REQ-HYBRID-001.md
**创建日期**: 2026-06-04
**实现版本**: alarm_rules.yaml:bat_soc_low (L37), 状态待 PR 28 同步

---

## 1. 概述

### 1.1 需求描述
当电池 SOC 低于 10% 时，触发黄色报警横幅，电量警告指示灯以 1Hz 闪烁，提醒驾驶员尽快充电。

### 1.2 背景与动机
中等低电量是车主日常场景（长时间停车未充电、长途行驶后），需要提前提醒避免进入严重低电（<8%）状态。10% 阈值为行业通用"低电警告"临界值。

### 1.3 跟 REQ-ALM-003 关系（分层报警）
- **本条 (REQ-ALM-012)**: 10% 阈值, 黄色横幅 + 1Hz 闪烁, 优先级 medium
- **REQ-ALM-003**: 8% 阈值, 红色横幅 + 3Hz 闪烁, 优先级 high (严重报警)
- 两条 alarm rule 独立但互不冲突: SOC 进入 [8%, 10%) 时仅 012 触发, 进入 <8% 时 012 仍触发且 003 叠加触发 (指示灯闪烁 3Hz 覆盖 1Hz, 横幅两段文本都显示)

---

## 2. 功能需求

### 2.1 触发条件
- bat_soc < 10
- 持续时间 ≥ 1000ms（防抖，比 003 长避免误报）

### 2.2 输入
| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| bat_soc | CAN总线 (0x186040F3, byte 4) | uint8 % | 0~100% |

### 2.3 输出
| 字段 | 目标 | 格式 |
|------|------|------|
| alarm_text | QML 报警横幅 | QString: "电量低，请充电" |
| alarm_text_en | QML 报警横幅 | QString: "LOW BATTERY" |
| indicator | soc_warn_light | widget id: soc_warn_light, flash=1Hz, color=#FFAA00 (黄) |

### 2.4 处理逻辑
```
[bat_soc < 10] → 持续 1000ms → 触发 bat_soc_low
    ├── soc_warn_light 闪烁 (1Hz, 黄色)
    └── 报警横幅显示 (黄色, 中字体)
```

### 2.5 边界条件与异常处理
- bat_soc 信号超时 → 显示 "---"，不触发本报警
- SOC 恢复到 ≥ 12% → 1000ms 防抖后清除报警
- 当 SOC 进入 <8% 时: 本条 (012) 仍 active, REQ-ALM-003 叠加触发 (3Hz 闪烁覆盖 1Hz, 横幅两段文本都显示)

---

## 3. 非功能需求

### 3.1 性能要求
- 响应延迟: ≤ 1000ms (含 1000ms 防抖)
- 闪烁频率: 1Hz (±0.1Hz)

### 3.2 安全性需求
- ISO 26262 ASIL A (提示类, 非安全相关)

---

## 4. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 条件 | alarm_rules.yaml | condition | value < 10 |
| 优先级 | alarm_rules.yaml | priority | medium |
| 防抖 | alarm_rules.yaml | duration_ms | 1000 |
| 闪烁频率 | alarm_rules.yaml | flash_hz | 1 |
| 指示灯 | alarm_rules.yaml | widget | soc_warn_light |
| 横幅颜色 | alarm_rules.yaml | color | #FFAA00 (黄) |

---

## 5. 测试用例

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-ALM-012-01 | 触发低电警告 | SOC=9% 持续 1100ms | 黄横幅+1Hz 闪烁 | Proposed |
| TC-ALM-012-02 | 自动清除 | SOC 恢复到 13% | 1000ms 后清除 | Proposed |
| TC-ALM-012-03 | 跟 003 分层触发 | SOC=7% | 012 + 003 同时 active (3Hz 覆盖 1Hz) | Proposed |
| TC-ALM-012-04 | 防抖边界 | SOC=9% 持续 900ms 然后 11% | 不触发 (防抖未达 1000ms) | Proposed |

---

## 6. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/alarm_rules.yaml` (bat_soc_low 规则, L37) |
| 生成文件 | `src/generated/alarm_rule_table.cpp` (ALARM_RULE_TABLE 索引 2) |
| 关联 L2 组件 | `src/layer2/alarm_runtime.cpp` (`onValueChanged("bat_soc", 9)` 触发) |
| QML组件 | `src/ui/AlarmBanner.qml` (黄色横幅) / `src/ui/IndicatorLight.qml` (1Hz 闪烁) |
| 验证日期 | - |
| 验证结果 | - |

---

## 7. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-06-04 | 1.0 | 初始创建, 从 REQ-ALM-003 拆分 (PR 27). 历史: 10% 规则 `bat_soc_low` (L37) 早就在 alarm_rules.yaml, 但长期跟 REQ-ALM-003 (8% 严重规则) 共用编号. PR 26 把 003 重新指向 8% 严重规则后, 本条单独成档. | can-dash-jd-autopilot |

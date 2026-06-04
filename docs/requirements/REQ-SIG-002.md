#REQ-SIG-002|电池SOC信号 (bat_soc)
=========================================

**状态**:   Approved
**类型**:   Functional
**优先级**: Critical
**来源**:   can_ids.yaml (已有) / INDEX
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 信号描述
电池荷电状态（State of Charge）信号，表示电池剩余电量百分比。

### 1.2 相关需求
- REQ-ALM-003: soc_critical_low (SOC < 8%)
- REQ-ALM-012: bat_soc_low (SOC < 10%, PR 27 新立, 原本误引 REQ-ALM-004)
- REQ-SIG-001: bat_volt (同属BMS)

---

## 2. 信号规格

### 2.1 CAN 帧信息
| 字段 | 值 |
|------|-----|
| CAN ID | 0x186040F3 |
| 来源 | BMS |
| 周期 | 100ms |

### 2.2 数据字段
| 字段 | 值 |
|------|-----|
| 名称 | bat_soc |
| 字节位置 | byte 4 |
| 位宽 | 8 bits |
| 类型 | uint8 |
| 公式 | x |
| 单位 | % |

### 2.3 有效范围
| 字段 | 值 |
|------|-----|
| 正常范围 | 0% ~ 100% |
| 低电量阈值 | < 10% |
| 严重低电量阈值 | < 8% |

### 2.4 显示格式
- 格式: "{:.0f}%" 或图形化电池条
- 超时显示: "---"

---

## 3. 信号处理
- 使能平滑滤波: smoothing_window = 5
- 防止SOC显示跳动

---

## 4. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| CAN ID | can_ids.yaml | can_id | 0x186040F3 |
| 字节 | can_ids.yaml | byte | 4 |
| 公式 | can_ids.yaml | formula | x |
| 单位 | can_ids.yaml | unit | % |
| 平滑 | can_signal_status.yaml | smoothing | true |
| 平滑窗口 | can_signal_status.yaml | smoothing_window | 5 |

---

## 5. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 | `config/can_ids.yaml` |
| 监控配置 | `config/can_signal_status.yaml` |
| 生成代码 | `src/generated/can_field_def.h` |
| 验证日期 | - |
| 验证结果 | - |

---

## 6. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 1.2 节修 bat_soc_low 引用: 原本误引 REQ-ALM-004, 改成 REQ-ALM-012 (PR 27 新立) | can-dash-jd-autopilot |

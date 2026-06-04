#REQ-HYBRID-006|充电功率显示
=========================================

**状态**:   Implemented
**类型**:   Functional
**优先级**: Medium
**来源**:   can_ids.yaml (CHG_POWER) / REQ-HYBRID-001.md §2.8.1
**创建日期**: 2026-06-04
**实现版本**: can_ids.yaml:CHG_POWER (L167) + src/ui/EnergyFlowDiagram.qml:L255-256 (chargePower 文本+颜色阈值 充绿/放蓝)

---

## 1. 概述

### 1.1 需求描述
在能量流图中显示当前充电功率 (charge_power) 数值, 区分充电 (正功率, 外部 → 电池) 和放电 (负功率, 电池 → 电机) 两种流向, 用不同颜色提示用户.

### 1.2 背景与动机
充电功率是混动车 / 纯电车仪表盘的关键信息. 用户在快充桩充电时需要确认功率是否符合预期 (e.g. 期望 60kW 实测 45kW → 桩或车端有损耗). 放电时显示负功率, 让用户直观感知能量消耗速率.

### 1.3 相关需求
- REQ-HYBRID-001 §2.8.1: 充电信号定义 (charge_status / charge_fault / charge_power)
- REQ-SIG-017: 充电功率信号 (charge_power) — 已 Implemented (PR 38 docs sync)
- REQ-IND-011: 充电中指示灯 (charge_light) — 已 Implemented (PR 33 docs sync)

---

## 2. 功能需求

### 2.1 信号定义
| 字段 | CAN ID | 字节 | 类型 | 单位 | 公式 | 范围 |
|------|--------|------|------|------|------|------|
| charge_power | 0x302 (CHG_POWER) | [0,1] (2 bytes, little endian) | float | kW | x * 0.001 | -300 ~ +300 |

### 2.2 显示要求
- 格式: `"{+/-}{charge_power:.1f} kW"` (正功率前加 `+` 号, 负功率自带 `-` 号)
- 颜色阈值: `>= 0` 绿色 (#00FF88, colorCharge) / `< 0` 蓝色 (#00AAFF, colorDischarge)
- 显示位置: EnergyFlowDiagram.qml "功率" Text 控件 (电池图标下方)
- 字体: Roboto Mono 12px (跟项目其他 power/efficiency 显示保持一致)

### 2.3 联动行为
- `charge_status == 0` (未充电) 时 charge_power 通常为 0 或负 (车辆用电), 显示 `+0.0 kW` 绿色 (技术上不该发生, 但 EnergyFlowDiagram 不判断 charge_status, 仅看 chargePower 符号)
- `charge_status == 1/2` (慢充/快充) 时 charge_power 为正, 显示 `+60.0 kW` 绿色
- 车辆行驶 (charge_status == 0 + 车辆 drive) 时 charge_power 为负, 显示 `-15.0 kW` 蓝色
- 制动回收 (brake > threshold) 时 charge_power 为负, 同样蓝色 (跟行驶放电同色, 不区分)

---

## 3. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| CAN ID | can_ids.yaml | can_id | 0x302 |
| 字节范围 | can_ids.yaml | byte | [0, 1] |
| 位宽 | can_ids.yaml | bits | 16 |
| 字节序 | can_ids.yaml | endian | little |
| 类型 | can_ids.yaml | type | float |
| 公式 | can_ids.yaml | formula | x * 0.001 |
| 单位 | can_ids.yaml | unit | kW |
| 周期 | can_ids.yaml | period_ms | 100 |

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件 (信号) | `config/can_ids.yaml` (CHG_POWER L167, charge_power field L171) |
| 生成代码 | `src/generated/can_field_def.h` |
| 数据流入口 | `src/layer3/shm_data_source.cpp:L329` (`out.data.charge_power = shm.charge_power`) |
| 数据流出 | `src/layer3/qt_data_binder.cpp:L208` (`m["charge_power"] = d.charge_power`) |
| QML 绑定 | `src/ui/DashboardMain.qml:L266` (`chargePower: dashboard.displayData["charge_power"] || 0`) |
| QML 显示 | `src/ui/EnergyFlowDiagram.qml` L255-256 — `text: (root.chargePower >= 0 ? "+" : "") + root.chargePower.toFixed(1) + " kW"` + 颜色阈值 (`>= 0` 绿, `< 0` 蓝) |
| 验证日期 | 2026-06-04 |
| 验证结果 | 16/16 ctest pass (L1+L2 无回归);  EnergyFlowDiagram.qml L255-256 实际内容验证 (`chargePower.toFixed(1)` + 颜色阈值);  数据通路 5 段行号验证 (can_ids.yaml:L171 → shm_data_source.cpp:L329 → qt_data_binder.cpp:L208 → DashboardMain.qml:L266 → EnergyFlowDiagram.qml:L255-256) |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-06-04 | 1.0 | 初始创建 (PR 40 新建 .md, 跟 PR 36 IND 1-5 + SYS-001 同形状) | can-dash-jd-autopilot |

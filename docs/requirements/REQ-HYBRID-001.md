#REQ-HYBRID-001|混动汽车仪表盘特有功能需求基线
=========================================

**状态**:   Proposed
**类型**:   Functional, Safety
**优先级**: High
**来源**:   竞品分析（比亚迪秦Plus DM-i/岚图梦想家/丰田混动）+ 行业标准调研（2026-05-31）
**创建日期**: 2026-05-31
**实现版本**: -

---

## 1. 概述

### 1.1 需求描述
定义 PHEV/HEV 混动车型仪表盘的特有功能基线，包括能量流显示、SOC/电量管理、发动机启停状态、纯电续航里程、燃油续航里程、档位显示、以及相关的报警规则和指示灯。

### 1.2 背景与动机
当前 `can-dash` 的 CAN 配置已覆盖电池（bat_volt/bat_curr/bat_soc）、电机（motor_rpm/motor_temp）、充电（charge_status/charge_power/charge_fault）和能量模式（energy_mode）信号。但缺少混动车型特有的燃油管理、纯电续航、电池温度、档位显示等功能。本文档整合竞品调研结果和国标要求，建立混动车型仪表盘需求的完整基线。

### 1.3 相关需求
- REQ-ALM-001/002/003/004: 电池/电机已有报警（本文补充混动特有报警）
- REQ-SIG-001~007: 已有 CAN 信号（本文新增混动专用 CAN 信号）
- REQ-IND-001~005: 已有指示灯（本文新增混动特有指示灯）

---

## 2. 功能需求

### 2.1 能量流显示（Energy Flow）

#### 2.1.1 显示模式定义
混动仪表盘需同时显示三种能量流的动态状态：

| 能量流状态 | energy_mode 值 | 显示图标 | 颜色 |
|-----------|---------------|---------|------|
| 纯电驱动 | 0 (EV) | ev_mode_light | 绿色 |
| 混动驱动 | 1 (HYBRID) | hybrid_mode_light | 蓝色 |
| 发动机驱动 | 2 (ENGINE_ONLY) | engine_run_light | 白色/橙色 |
| 充电模式 | 3 (CHARGE_MODE) | energy_flow_light | 黄色闪烁 |
| 能量回收 | 动能回收中 | energy_flow_light（反向） | 绿色 |

> **注**: `energy_mode` 信号已存在于 `can_ids.yaml`（0x300），alarm_rules.yaml 中已配置对应报警规则。本文档基线确认该信号满足能量流显示需求。

#### 2.1.2 能量回收指示
- 制动能量回收时，`energy_flow_light` 以 2Hz 频率闪烁（黄色/绿色）
- 回收功率 > 0 时，显示当前回收功率值（kW）

---

### 2.2 SOC 电池电量显示

#### 2.2.1 信号定义

| 字段 | 来源 | 格式 | 单位 | 正常范围 |
|------|------|------|------|---------|
| bat_soc | CAN总线 (0x186040F3, byte 4) | uint8 | % | 0~100% |

> **注**: `bat_soc` 已存在于 can_ids.yaml。

#### 2.2.2 显示逻辑
- SOC ≥ 20%: 正常显示（白色/绿色）
- SOC 10%~19%: 显示黄色低电量警告
- SOC < 10%: 显示红色极低电量报警（已有 `bat_soc_low` 规则，duration 1000ms）
- SOC < 8%: 触发严重报警 `soc_critical_low`（已有规则）

#### 2.2.3 比亚迪秦Plus DM-i 参考
- SOC 可在仪表盘上以百分比+图形方式显示
- 能量流图可在中控屏调出（仪表盘显示简化版）
- SOC 目标点可在中控屏设置（DM-i 保电逻辑）

---

### 2.3 纯电续航里程（EV Range）

#### 2.3.1 信号定义（需新增）

| 字段 | CAN ID（待定） | 格式 | 单位 | 正常范围 |
|------|---------------|------|------|---------|
| ev_range | 0x307 (建议) | uint16 | km | 0~300 km |

#### 2.3.2 显示要求
- 单位：km（公里），显示整数
- 分辨率：1 km
- 当无法计算时（如 SOC=0）：显示 "---"
- 低于 5 km 时触发提示报警

---

### 2.4 燃油续航里程（Fuel Range）

#### 2.4.1 信号定义（需新增）

| 字段 | CAN ID（待定） | 格式 | 单位 | 正常范围 |
|------|---------------|------|------|---------|
| fuel_level | 0x308 (建议) | uint8 | % | 0~100% |
| fuel_range | 从燃油表物理量换算 | uint16 | km | 0~1000 km |

#### 2.4.2 显示要求
- 燃油表显示剩余油量百分比（%）
- 燃油续航里程从油量估算
- 燃油 < 15% 时点亮 `fuel_low_light`（黄色）
- 燃油 < 5% 时触发报警

---

### 2.5 电池温度显示与报警

#### 2.5.1 信号定义（需新增）

| 字段 | CAN ID（待定） | 格式 | 单位 | 正常范围 |
|------|---------------|------|------|---------|
| battery_temp | 0x186040F3 (byte 5) | int8 | °C | -40~85°C |

> **注**: `motor_temp` 已存在（MCU, 0x101, byte 4），但电池温度与电机温度不同，需独立信号。

#### 2.5.2 报警规则

| 报警名称 | 条件 | 优先级 | 动作 |
|---------|------|--------|------|
| bat_temp_high | battery_temp > 65°C | High | 报警横幅 + `bat_warn_light` 闪烁 |
| bat_temp_low | battery_temp < -10°C | Medium | 提示横幅 |
| bat_temp_critical | battery_temp > 75°C | Critical | 强制报警横幅 + 降功率提示 |

---

### 2.6 发动机启停状态

#### 2.6.1 信号定义

| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| engine_rpm | CAN总线 (0x305) | uint16 rpm | 发动机转速 |
| engine_fault | CAN总线 (0x305, byte 0, bit 0) | bool | 发动机故障标志 |

> **注**: 两信号均已存在于 can_ids.yaml。

#### 2.6.2 显示要求
- 发动机启动时（engine_rpm > 0 且 energy_mode != 0）：`engine_run_light` 常亮
- 纯电模式时（energy_mode == 0）：`engine_run_light` 熄灭
- 发动机故障（engine_fault == 1）：已有 `engine_fault_alarm` 规则

---

### 2.7 档位显示（Gear Status）

#### 2.7.1 信号定义（需新增）

| 字段 | CAN ID（待定） | 格式 | 说明 |
|------|---------------|------|------|
| gear_status | 0x309 (建议) | uint8 | 0=P, 1=R, 2=N, 3=D, 4=S(运动) |

#### 2.7.2 显示要求
- 档位以文字或图标形式显示（P/R/N/D）
- 运动模式(S)可作为D的子模式显示

---

### 2.8 充电状态显示

#### 2.8.1 信号定义

| 字段 | 来源 | 格式 | 说明 |
|------|------|------|------|
| charge_status | CAN总线 (0x306) | uint8 (2bit) | 0=未充电, 1=慢充, 2=快充, 3=预约 |
| charge_fault | CAN总线 (0x306, bit 2) | bool | 充电故障 |
| charge_power | CAN总线 (0x302) | float kW | 当前充电功率 |

> **注**: 三信号均已存在于 can_ids.yaml。

#### 2.8.2 显示要求
- 充电中（charge_status != 0）：`charge_light` 亮（蓝色）
- 充电故障（charge_fault == 1）：`charge_fault_light` 闪烁，报警横幅
- 显示充电功率和预计时间

---

### 2.9 READY 指示灯

所有新能源车型（HEV/PHEV/BEV）在启动时必须显示 READY 指示灯，表示车辆已完成自检，可以行驶。

| 字段 | 触发条件 | 指示灯 | 颜色 |
|------|---------|--------|------|
| ready_indicator | 车辆上电完成，自检通过 | `ready_go_light` | 绿色 |

> **注**: `ready_go_light` 已存在于 indicators.yaml（position: x=780, y=260）。

---

## 3. 非功能需求

### 3.1 法规与安全标准

| 标准 | 版本 | 适用范围 | 要求摘要 |
|------|------|---------|---------|
| GB 4094-2016 | 现行 | M/N类汽车 | 操纵件/指示器/信号装置标志，颜色定义（红/黄/绿/蓝/白） |
| GB 18384-2020 | 现行 | 电动汽车（PHEV/HEV/BEV） | 绝缘电阻监测报警（声/光），B级电压标记 |
| GB/T 4094.2-2017 | 现行 | 电动汽车特有标志 | EV/HEV特有图标（能量流、充电等） |
| ISO 26262:2018 | 国际 | 安全相关E/E系统 | ASIL等级评估，仪表显示属于QM~ASIL B |
| ISO 15005 | 参考 | 仪表盘显示 | 响应时间、亮度等通用要求 |

**GB 18384-2020 关键要求**：
- 绝缘电阻低于限值时，必须通过仪表声/光报警提示驾驶员
- B级电压（>60V DC或>30V AC）部件需有高压警示标志

### 3.2 性能要求

| 指标 | 要求 |
|------|------|
| 信号响应延迟 | ≤ 100ms（CAN信号变化到UI更新） |
| 报警触发延迟 | ≤ 200ms（含防抖） |
| 指示灯切换延迟 | ≤ 100ms |
| 画面刷新率 | ≥ 30 FPS |
| 信号超时检测 | ≤ 500ms |

### 3.3 安全性需求
- ISO 26262 ASIL B（安全相关显示：电池报警、READY指示、发动机故障）
- 传感器/信号丢失时，仪表必须显示 "---"，不得显示旧值
- 报警清除必须基于信号实际恢复，不得仅靠超时

---

## 4. 配置参数

### 4.1 已有信号（can_ids.yaml 已有）

| 字段 | YAML文件 | CAN ID | 说明 |
|------|---------|--------|------|
| energy_mode | can_ids.yaml | 0x300 | 0=EV, 1=HYBRID, 2=ENGINE_ONLY, 3=CHARGE |
| engine_rpm | can_ids.yaml | 0x305 | 发动机转速 |
| engine_fault | can_ids.yaml | 0x305 | 发动机故障 |
| charge_status | can_ids.yaml | 0x306 | 充电状态 |
| charge_fault | can_ids.yaml | 0x306 | 充电故障 |
| charge_power | can_ids.yaml | 0x302 | 充电功率(kW) |
| bat_soc | can_ids.yaml | 0x186040F3 | 电池SOC(%) |

### 4.2 需新增CAN信号（建议配置）

| 字段 | 建议CAN ID | 字节位置 | 类型 | 单位 | 说明 |
|------|-----------|---------|------|------|------|
| ev_range | 0x307 | [0,1] | uint16 | km | 纯电续航里程 |
| fuel_level | 0x308 | 0 | uint8 | % | 燃油剩余量 |
| fuel_range | 0x308 | [1,2] | uint16 | km | 燃油续航里程 |
| battery_temp | 0x186040F3 | 5 | int8 | °C | 电池温度 |
| gear_status | 0x309 | 0 | uint8 | - | 档位 P/R/N/D/S |

### 4.3 报警规则新增项

| 报警名称 | YAML字段 | 条件 | 优先级 | widget |
|---------|---------|------|--------|--------|
| fuel_low | alarm_rules.yaml | fuel_level < 15 | medium | fuel_low_light |
| fuel_critical | alarm_rules.yaml | fuel_level < 5 | high | fuel_low_light |
| ev_range_low | alarm_rules.yaml | ev_range < 5 | medium | ev_range_warn_light |
| bat_temp_high | alarm_rules.yaml | battery_temp > 65 | high | bat_warn_light |
| bat_temp_critical | alarm_rules.yaml | battery_temp > 75 | critical | bat_warn_light |

### 4.4 需新增指示灯

| ID | 图标文件 | 颜色 | 位置（示例） | 说明 |
|----|---------|------|------------|------|
| fuel_low_light | fuel_low_yellow.png / fuel_low_dim.png | 黄 | x=600, y=300 | 燃油低警告 |
| ev_range_warn_light | ev_range_warn.png | 黄 | x=660, y=300 | 纯电续航不足 |

---

## 5. 测试用例

### TC-HYBRID-001|EV模式指示灯正确显示
- **前置条件**: 车辆处于纯电行驶模式
- **输入**: CAN帧 energy_mode = 0 (EV)
- **预期输出**: ev_mode_light 常亮，hybrid_mode_light 熄灭，engine_run_light 熄灭
- **通过标准**: PASS（指示灯状态与能量模式一致）

### TC-HYBRID-002|混动模式能量流显示
- **前置条件**: 车辆处于混动驱动模式
- **输入**: CAN帧 energy_mode = 1 (HYBRID)
- **预期输出**: hybrid_mode_light 常亮，ev_mode_light 熄灭
- **通过标准**: PASS

### TC-HYBRID-003|充电中指示灯
- **前置条件**: 车辆正在直流快充
- **输入**: CAN帧 charge_status = 2, charge_power = 60.0
- **预期输出**: charge_light 常亮（蓝），显示充电功率 "60.0 kW"
- **通过标准**: PASS

### TC-HYBRID-004|燃油低报警触发
- **前置条件**: 燃油正常（fuel_level = 20%）
- **输入**: CAN帧 fuel_level = 14%（持续 500ms）
- **预期输出**: fuel_low_light 点亮（黄色）
- **通过标准**: PASS

### TC-HYBRID-005|电池温度过高报警
- **前置条件**: 电池温度正常（battery_temp = 45°C）
- **输入**: CAN帧 battery_temp = 68°C（持续 300ms）
- **预期输出**: bat_warn_light 闪烁（2Hz），报警横幅 "电池温度过高"
- **通过标准**: PASS

### TC-HYBRID-006|纯电续航不足提示
- **前置条件**: 纯电续航正常（ev_range = 20km）
- **输入**: CAN帧 ev_range = 4（持续 1000ms）
- **预期输出**: ev_range_warn_light 亮，提示文字 "纯电续航不足，请切换混动模式"
- **通过标准**: PASS

### TC-HYBRID-007|READY灯上电显示
- **前置条件**: 车辆钥匙ON，整车自检通过
- **输入**: 车辆上电完成信号
- **预期输出**: ready_go_light 亮（绿色）
- **通过标准**: PASS

### TC-HYBRID-008|充电故障报警
- **前置条件**: 正常充电中（charge_status=1, charge_fault=0）
- **输入**: CAN帧 charge_fault = 1
- **预期输出**: charge_fault_light 闪烁（2Hz），报警横幅 "充电异常！"
- **通过标准**: PASS

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-HYBRID-01 | EV模式显示 | energy_mode=0 | EV灯亮 | Proposed |
| TC-HYBRID-02 | 混动模式显示 | energy_mode=1 | 混动灯亮 | Proposed |
| TC-HYBRID-03 | 充电中显示 | charge_status=2 | 充电灯亮 | Proposed |
| TC-HYBRID-04 | 燃油低报警 | fuel_level=14% | 燃油低灯亮 | Proposed |
| TC-HYBRID-05 | 电池温度过高 | battery_temp=68°C | 电池警告灯+横幅 | Proposed |
| TC-HYBRID-06 | 纯电续航不足 | ev_range=4km | 续航不足提示 | Proposed |
| TC-HYBRID-07 | READY灯 | 上电完成 | READY灯亮 | Proposed |
| TC-HYBRID-08 | 充电故障 | charge_fault=1 | 充电故障灯+横幅 | Proposed |

---

## 6. 实现追踪

| 字段 | 值 |
|------|-----|
| 实现文件（已有） | `config/can_ids.yaml`, `config/alarm_rules.yaml`, `config/indicators.yaml` |
| 实现文件（新增） | `config/can_ids.yaml`（新增4个信号）, `config/alarm_rules.yaml`（新增5条规则）, `config/indicators.yaml`（新增2个指示灯） |
| 生成代码 | `src/generated/can_field_def.h`, `src/generated/alarm_rule_def.h` |
| QML组件 | `src/ui/AlarmBanner.qml`, `src/ui/IndicatorLight.qml` |
| 验证日期 | - |
| 验证结果 | - |

---

## 7. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建：基于竞品调研（比亚迪秦Plus DM-i/岚图梦想家/丰田混动）和 GB 4094-2016 / GB 18384-2020 / ISO 26262 标准 | requirements-document-agent |

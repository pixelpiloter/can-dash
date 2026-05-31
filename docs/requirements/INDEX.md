# CAN-Dash 需求索引

最后更新: 2026-05-31

## 统计

| 类别 | 总数 | Approved | Implemented | Verified |
|------|------|----------|-------------|----------|
| ALM (报警) | 5 | 4 | 0 | 0 |
| HYBRID (混动特有) | 1 | 0 | 0 | 0 |
| IND (指示灯) | 10 | 10 | 0 | 0 |
| SIG (CAN信号) | 12 | 12 | 0 | 0 |
| UI (界面) | 3 | 3 | 0 | 0 |
| SYS (系统) | 1 | 1 | 0 | 0 |
| **合计** | **22** | **20** | **0** | **0 |

---

## 需求列表

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-ALM-001 | 电池过压报警 | Safety | High | Approved | - |
| REQ-ALM-002 | 电池欠压报警 | Safety | High | Approved | - |
| REQ-ALM-003 | 电量低报警 (SOC<10%) | Safety | Medium | Approved | - |
| REQ-ALM-004 | 电机温度过高报警 | Safety | High | Approved | - |
| REQ-IND-001 | 电池警告指示灯 | Functional | High | Approved | - |
| REQ-IND-002 | 电量低指示灯 | Functional | Medium | Approved | - |
| REQ-IND-003 | 电机温度警告指示灯 | Functional | High | Approved | - |
| REQ-IND-004 | Ready/Go 指示灯 | Functional | Medium | Approved | - |
| REQ-IND-005 | 高压指示灯 | Functional | Medium | Approved | - |
| REQ-SIG-001 | 电池电压信号 (bat_volt) | Functional | Critical | Approved | - |
| REQ-SIG-002 | 电池电流信号 (bat_curr) | Functional | High | Approved | - |
| REQ-SIG-003 | 电池SOC信号 (bat_soc) | Functional | Critical | Approved | - |
| REQ-SIG-004 | 车速信号 (vehicle_speed) | Functional | Critical | Approved | - |
| REQ-SIG-005 | 制动信号 (brake) | Functional | High | Approved | - |
| REQ-SIG-006 | 电机转速信号 (motor_rpm) | Functional | High | Approved | - |
| REQ-SIG-007 | 电机温度信号 (motor_temp) | Functional | High | Approved | - |
| REQ-UI-001 | 多语言切换 (zh_CN / en_US) | UI | Medium | Approved | - |
| REQ-UI-002 | 报警横幅 (AlarmBanner) | UI | High | Approved | - |
| REQ-UI-003 | 仪表表盘 (GaugeCanvas) | UI | Critical | Approved | - |
| REQ-SYS-001 | CAN总线超时检测 | Reliability | High | Approved | - |
| REQ-ALM-005 | 胎压低报警 | Safety, Functional | Critical | Proposed | - |
| REQ-HYBRID-001 | 混动汽车仪表盘特有功能需求基线 | Functional, Safety | High | Proposed | - |

---

## 待规划需求

| ID | 标题 | 来源 | 优先级 | 状态 |
|----|------|------|--------|------|
| REQ-SIG-008 | 胎压信号 (tire_pressure) | 用户需求 | High | Proposed |

---

## 需求文档

每个需求的详细规格文档位于本目录：

- `REQ-ALM-001.md`
- `REQ-ALM-002.md`
- ...

---

## 生命周期状态

```
Proposed → Approved → Implemented → Verified
    ↓           ↓            ↓
 Rejected   Rejected     Rejected
```

## 更新规则

1. 新增需求 → 分配 ID → 写 REQ-XXX.md → 更新本 INDEX.md
2. 需求实现 → 更新 `实现版本` 列 → 更新 `状态` 为 Implemented
3. 需求验收 → 更新 `状态` 为 Verified + 验证日期
4. 需求拒绝 → 更新 `状态` 为 Rejected

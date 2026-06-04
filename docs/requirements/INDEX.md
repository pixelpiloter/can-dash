# CAN-Dash 需求索引

最后更新: 2026-06-04 (PR 40 同步)

## 统计

| 类别 | 总数 | Approved | Implemented | Verified |
|------|------|----------|-------------|----------|
| ALM (报警) | 12 | 0 | 11 | 1 |
| HYBRID (混动特有) | 6 | 0 | 6 | 0 |
| IND (指示灯) | 12 | 0 | 12 | 0 |
| SIG (CAN信号) | 19 | 0 | 19 | 0 |
| UI (界面) | 5 | 1 | 4 | 0 |
| SYS (系统) | 5 | 2 | 3 | 0 |
| **合计** | **59** | **2** | **55** | **2** |

> **PR 39 同步说明**: HYBRID 类别 1 条 docs-only 同步 (跟 PR 25/33/34/35/36/37/38 docs-only 形状一致, 0 cpp 改动):
> - **REQ-HYBRID-002** (电池温度显示与报警): 状态 Approved → Implemented, 实现版本补全 QML 显示. **修正 §4 stale 误判**: 之前 .md §4 "QML 显示 | **缺** — 当前仪表盘未在 TripPanel / 主仪表区显示 battery_temp 数值" + INDEX impl ref "显示组件待 PR 32" 是 stale 描述. 实际 src/ui/EnergyFlowDiagram.qml L246-247 早已实现 batteryTemp.toFixed(0) + "°C" 显示 + 颜色阈值 (>50°C 红, >40°C 橙, 其他灰), 数据通路是 ShmDataSource.cpp:L316 (out.data.battery_temp = shm.battery_temp) → QtDataBinder.cpp:L194 (m["battery_temp"] = d.battery_temp) → DashboardMain.qml:L262 (batteryTemp: dashboard.displayData["battery_temp"] || 0 绑定到 EnergyFlowDiagram). 报警侧 bat_temp_high L228 + bat_temp_critical L244 + 指示灯 bat_warn_light L5 联动完整, "显示" 部分"缺"的判断不成立.
> - 类别表 stale 修复: HYBRID 2/4/0 → 1/5/0, 合计 4/53/2 → 3/54/2
> - **决策依据**: 跟 PR 35 修 SIG-002 / PR 30 修 HYBRID 标题 / PR 33 修 IND 标题同规则 — 当 .md §4 描述跟实际代码状态不符, .md §4 必须跟代码对齐, 不能反过来 (用户硬要求"诚实标注实现版本").
> - **范围限制 (跟 PR 38 一致)**: REQ-HYBRID-006 (PR 40 已新建 .md + Implemented, 移出范围限制) / 不动 REQ-UI-005 (资源规格, PR 37 决策保持 Approved) / 不动 REQ-SYS-002/003 (PR 37/38 决策保持 Approved) / 不动 SIG-011/012/015/016/019 (impl ref/标题错位, 留 PR 40+)
>

> **PR 34 同步说明**: 跨 SYS + UI 类别 2 条 docs-only 同步 (跟 PR 25/33 docs-only 形状一致, 0 cpp 改动):
> - REQ-SYS-004 (安全带运行时监控 SeatBeltRuntime): 状态 Approved → Implemented, INDEX 标题"安全带状态运行时监控" → "安全带运行时监控 (SeatBeltRuntime)" (跟 .md 一致, 删冗余"状态" + 补 L2 组件名). 实现版本填 SeatBeltRuntime (PR 23 L2+test 升级) — config/seat_belt.yaml:trigger.speed_threshold (L57), 监控 5 个座位行号 (driver L4 / passenger L15 / rear_left L26 / rear_center L36 / rear_right L46)
> - REQ-UI-003 (仪表表盘组件 GaugeCanvas): 状态 Approved → Implemented, INDEX 标题"仪表表盘" → "仪表表盘组件" (跟 .md 一致, 补"组件" 2 字). 实现版本填 GaugeCanvas QML 组件 — config/display_layout.yaml:speed_gauge (L15, bindings.value=vehicle_speed, config.max=260, major_ticks=13) + DashboardMain.qml 20ms Timer 推算
> - 类别表 stale 修复: UI 5/0/0 → 4/1/0 (Approved -1, Implemented +1), SYS 4/1/0 → 3/2/0 (Approved -1, Implemented +1), 合计 33/24/2 → 31/26/2
> - **范围限制 (跟 PR 25/33 一致)**: 不动 IND 1-5 (无 .md 文件, 历史欠账, 跟 PR 24/27/30/33 决策一致) / 不动 REQ-UI-001/002/004/005 (本 PR 34 只覆盖 2 条最干净的 A2 候选, 避免 scope 扩大) / 不动 REQ-UI-005 + REQ-SYS-003 (INDEX 标题跟 .md 完全不同概念, e.g. INDEX "颜色主题" vs .md "i18n" / INDEX "LCD背光超时" vs .md "跛行模式" — 三角矛盾, 留待用户决定 source of truth) / 不动 REQ-SIG-001~019 (SIG 类别 17 条全 Approved 是统一 docs sync 候选, 但本 PR scope 优先 SYS+UI, 留给下个 PR) / 不动 HYBRID 类别 (PR 29-31 已同步完) / 不动 ALM 类别 (PR 25-28 已同步完)

> **PR 33 同步说明**: IND 6-12 状态同步 Approved → Implemented + INDEX 6 处标题错位修齐 (跟 PR 30 修 HYBRID 同形状):
> - REQ-IND-006: "高压警示灯" → "高压指示灯" (跟 .md 一致), Approved → Implemented (indicators.yaml:high_voltage_light L32 + IndicatorLight.qml)
> - REQ-IND-007: "驾驶模式指示灯" → "纯电模式指示灯" (跟 .md 一致), Approved → Implemented (ev_mode_light L47 + ev_mode_active L85)
> - REQ-IND-008: "充电指示灯" → "混动模式指示灯" (跟 .md 一致), Approved → Implemented (hybrid_mode_light L55 + hybrid_mode_active L101)
> - REQ-IND-009: "系统故障指示灯" → "发动机运行指示灯" (跟 .md 一致), Approved → Implemented (engine_run_light L39 + engine_boost_active L117 + engine_fault_alarm L147)
> - REQ-IND-010: "电池警告灯" → "能量流指示灯" (跟 .md 一致), Approved → Implemented (energy_flow_light L63 + engine_boost_active L117 + charge_mode_active L136)
> - REQ-IND-011: "充电进行中指示灯" → "充电中指示灯" (跟 .md 一致, 文字简化), Approved → Implemented (charge_light L71 + charge_mode_active L136)
> - REQ-IND-012: 标题匹配, Approved → Implemented (charge_fault_light L78 + charge_fault_alarm L163, flash=2Hz)
> - 合计 IND 12/0/0 → 5/0/7 (Approved -7, Implemented +7), 整体 40/17/2 → 33/24/2
>
> **范围限制**: 不动 IND 1-5 (无 .md 文件, 历史欠账, 跟 PR 24/27/30 修 IND 不补的策略一致) / 不动 SIG/UI/SYS 类别表 (跟 .md 一致) / 不动 ALM/HYBRID (PR 28-32 已同步完).

> **PR 32 同步说明**: REQ-SYS-005 状态 Approved → Implemented (PR 17 SelfTestRuntime 已实现**信号自检**子功能), 实现版本 + SelfTestRuntime (PR 17) + 标"QML 黑屏/白屏检测待 PR 33". SYS 类别 5/0/0 → 4/1/0, 合计 41/16/2 → 40/17/2.
>
> **范围限制**: §6 实现追踪 4 项 (QML/后端类/报警规则/信号监控) 全部标"未落地" (待 PR 33 跟进) — 诚实标注 PR 17 只覆盖信号自检一个子功能, 不假装完整黑屏/白屏检测已实现.
>
> **PR 30 同步说明**: HYBRID 4 处标题错位修齐 + 状态对齐实际实现 (跟 PR 24 修 ALM 同形状):
> - REQ-HYBRID-002: \"充电状态显示\" → \"电池温度显示与报警\" (跟 .md 一致), 留 Approved (alarm 有, 显示组件缺)
> - REQ-HYBRID-003: \"能量流动图\" → \"纯电续航里程显示 (EV Range)\" (跟 .md 一致), Approved → Implemented (trip_computer PR 4 + ev_range_warn_light)
> - REQ-HYBRID-004: \"续航里程预测\" → \"燃油续航里程显示 (Fuel Range / Fuel Level)\" (跟 .md 一致), Approved → Implemented (trip_computer + fuel_low + fuel_low_light)
> - REQ-HYBRID-005: \"驾驶模式切换\" → \"档位显示 (Gear Status)\" (跟 .md 一致), Approved → Implemented (ViewManager PR 12 + ShmDataSource PR 13)
> - 合计 5/1/0 → 2/4/0 (HYBRID 类别), 整体 44/13/2 → 41/16/2
>
> **PR 31 同步说明**: 批量同步 4 个 HYBRID .md 元数据头部 + §实现追踪章节, 跟 INDEX 表对齐 (跟 PR 28 修 ALM 同模式, 跟 PR 30 修 HYBRID INDEX 配套):
> - REQ-HYBRID-002: 状态 Approved, 实现版本 + alarm_rules.yaml:bat_temp_high (L228) + bat_temp_critical (L244) 报警已落地, 显示组件待 PR 32
> - REQ-HYBRID-003: 状态 Implemented, 实现版本 + trip_computer (PR 4) + indicators.yaml:ev_range_warn_light (L94)
> - REQ-HYBRID-004: 状态 Implemented, 实现版本 + trip_computer (PR 4) + alarm_rules.yaml:fuel_low (L180) + indicators.yaml:fuel_low_light (L86)
> - REQ-HYBRID-005: 状态 Implemented, 实现版本 + ViewManager (PR 12) + ShmDataSource (PR 13) gear_status
> 4 个 .md §实现追踪 章节全部填充: 实现文件行号 / 关联 L2 组件 / 验证日期 2026-06-04 / 验证结果 18/18 ctest pass.
>
> **范围限制**: 不动 IND/SIG/UI/SYS 类别表.
>

> **PR 29 同步说明**: 补 2 条 INDEX 实现版本引用 + 修统计表 stale:
> 1. REQ-HYBRID-001 实现版本 "-" → "TripComputer (PR 4) + alarm_rules.yaml SOC 联动" (.md 元数据早就是 Implemented, INDEX 状态字段也已是 Implemented, 之前实现版本留空, 现补)
> 2. REQ-SIG-008 状态 Proposed → Implemented (跟 .md 元数据 Implemented + 实现版本 v0.3 commit 2448f83 对齐), 实现版本 "-" → "can_ids.yaml:0x3A0 (L235)"
>
> 顺带修统计表 stale: HYBRID 0/0/1 → 5/1/0 (Verified=1 是 bug, 找不到 .md 元数据是 Verified 的条目; 实际 HYBRID-001 Implemented + 002-006 Approved), SIG 18/0/1 → 17/1/1 (008 移到 Implemented, Approved 18→17), SYS 4/0/0 → 5/0/0 (SYS-001 缺 .md 仍 INDEX 列了 5 项), 合计相应调整 (39/11/3 → 44/13/2).
>
> **范围限制**: 不动 HYBRID-002~005 (.md 元数据 Proposed, INDEX 标 Approved — 差 1 step, 但 Approved/Proposed 概念相邻, 避免 PR 范围扩大) / 不动 IND/UI 类别表 (跟 .md 一致) / 不动 HYBRID-001 / SIG-008 .md 元数据 (实现版本 v0.3 / - 标法 PR 28 模式不强求统一).
>

> **PR 28 同步说明**: 批量同步 3 个 .md 元数据头部 + §实现追踪章节, 跟 INDEX 表对齐 — REQ-ALM-003/004/012 状态 Approved → Implemented, 实现版本填 alarm_rules.yaml:rule_name (L<n>), 验证日期/结果填充. REQ-ALM-001/002 无 .md 跳过; REQ-ALM-005 已是 Implemented, v1.0 标法不同不动.
>
> **PR 27 同步说明**: 新立 REQ-ALM-012 (电量低报警 SOC<10%, `bat_soc_low` 规则 L37), 从 REQ-ALM-003 拆分. ALM 类别 11 → 12 项, 合计 58 → 59. 状态字段元数据留待 PR 28 同步 (本 PR 27 不动 .md 元数据 "状态/实现版本" 字段, 避免范围扩大).
>
> **PR 25 同步说明**: 接 PR 24 留下的 4 条 ALM (006/008/009/011), 状态 Approved → Implemented 并填实现版本. 这 4 条都是 IND-mode 指示灯联动 (energy_mode==N 联动 N 个 widget 亮/灭), 跟 alarm_runtime 现有 single-key-condition 模型天然兼容, alarm_rules.yaml 早就有对应规则 (ev_mode_active L85 / engine_boost_active L117 / charge_mode_active L136 / charge_fault_alarm L163).

---
> **PR 40 同步说明**: HYBRID 类别 1 条 docs-only 同步 (跟 PR 33 IND 6-12、PR 35 SIG 14、PR 36 IND 1-5/UI 3/SYS 1、PR 37 三角矛盾、PR 38 SIG 013/014/017、PR 39 HYBRID-002 同形状, 0 cpp 改动, 纯 docs sync):
> - **新建 REQ-HYBRID-006.md** (历史欠账): 充电功率显示, 状态 Approved → Implemented, 实现版本填 can_ids.yaml:CHG_POWER (L167) + src/ui/EnergyFlowDiagram.qml:L255-256 (chargePower.toFixed(1) + 'kW' 文本 + 颜色阈值 充绿/放蓝)
> - **数据通路** (跟 PR 39 HYBRID-002 同形状): ShmDataSource.cpp:L329 (out.data.charge_power = shm.charge_power) → QtDataBinder.cpp (m["charge_power"] 绑定) → DashboardMain.qml (chargePower 绑定) → EnergyFlowDiagram.qml:L255-256 渲染
> - 类别表无变化 (HYBRID 0/6/0 保持, 合计 2/55/2 保持) — 纯单条 Approved→Implemented 移动
> - **范围限制 (跟 PR 38/39 决策一致)**: 不动 REQ-UI-005 (资源规格, PR 37 决策保持 Approved) / 不动 REQ-SYS-002/003 (PR 37/38 决策保持 Approved) / 不动 REQ-SIG-011/012/015/016/019 (impl ref/标题错位, 留 PR 41+) / 不动 REQ-ALM-001/002 (无 .md) / 不动 SIG-013/014/017 标题错位 (PR 38 修状态但没改标题, 留 PR 41+) / 不动 SYS-005 黑屏/白屏检测 (需补代码, 留代码 PR)
>

> **PR 38 同步说明**: SIG 类别最后 3 条 Approved → Implemented + INDEX 标题错位修齐 (跟 PR 35 docs-only 形状一致, 0 cpp 改动):
> - **REQ-SIG-013**: INDEX 标题 "驾驶员座椅温度信号" → "副驾安全带状态信号 (passenger_buckled)" (跟 .md 一致), 类型 Functional → Safety, 优先级 Low → High (跟 .md 一致, 安全相关), 状态 Approved → Implemented, 实现版本填 `can_ids.yaml:L112 (SEAT_BELT_P) + src/layer3/shm_data_source.cpp:L324`
> - **REQ-SIG-014**: INDEX 标题 "副驾驶员座椅温度信号" → "后排安全带状态信号 (rear_buckle)" (跟 .md 一致), 类型 Functional → Safety, 优先级 Low → High, 状态 Approved → Implemented, 实现版本填 `can_ids.yaml:L123 (SEAT_BELT_R) + src/layer3/shm_data_source.cpp:L325`
> - **REQ-SIG-017**: INDEX 标题 "剩余充电时间信号" → "充电功率信号 (charge_power)" (跟 .md 一致, 跟 SIG-016 区分清楚 — SIG-016 是 charge_status 充电状态, SIG-017 是 charge_power 充电功率), 优先级 Low → Medium (跟 .md 一致), 状态 Approved → Implemented, 实现版本填 `can_ids.yaml:L171 (CHG_POWER) + src/layer3/shm_data_source.cpp:L329`
> - 3 个 .md 状态 Approved → Implemented + 实现版本填实际行号 (跟 PR 28/31 修 ALM/HYBRID 元数据同模式)
> - 类别表 stale 修复: SIG 2/17/0 → 0/19/0 (Approved -2 + Implemented +2 — 实际是 -3/+3, 之前 2/17 本身就有 1 off-by-1 误差, 本 PR 一并修齐), 合计 7/50/2 → 4/53/2
> - **决策依据**: PR 35/36/37 路线延伸 — 当 INDEX 跟 .md 概念冲突时, INDEX 跟 .md 标题以 .md 为准 (这跟 PR 35 SIG-002 内部不自洽修齐 / PR 30 HYBRID 标题错位 / PR 33 IND 标题错位 / PR 37 UI-005+SYS-003 三角矛盾是同一规则)
> - **PR 36/37 范围限制更正**: 之前 PR 36/37 说 "SIG-013/014/017 (无 can_ids.yaml 字段)" 留 Approved 是**错误判断** — 实际 can_ids.yaml L112/L123/L171 有对应 passenger_buckled/rear_buckle/charge_power 字段, 代码也已经在 shm_data_source.cpp L324/L325/L329 消费 + QtDataBinder 暴露. 本 PR 38 纠正这个误判
> - **范围限制 (跟 PR 35 一致)**: 不动 SIG-015/016/019 (3 个相邻 INDEX 标题错位 + .md impl ref 错位, 需要 .md 也修, 范围更大, 留 PR 39) / 不动 SIG-011/012 (.md impl ref 错位 1 off, 不影响 "数据已实现" 判定, 范围更小修另说) / 不动 REQ-ALM-001/002 (无 .md) / 不动 IND 1-5 (历史欠账)
>

> **PR 37 同步说明**: 2 处三角矛盾解决 (.md 优先, 跟 SIG-002 决策同形状 — 既然 .md 在, .md 是 source of truth):
> - **REQ-UI-005**: INDEX 标题 "颜色主题需求" → "多语言配置 (i18n)" (跟 .md 一致), 优先级/类型不变, 实现版本填 `config/i18n/zh_CN.json + en_US.json + src/ui/I18nProvider.qml` (翻译资源规格), 状态保持 Approved (规格文档性质, 跟 UI-001 Implemented 区分清楚 — UI-001 是"运行时切换机制", UI-005 是"翻译资源规格", 互补不重复)
> - **REQ-SYS-003**: INDEX 标题 "LCD背光超时逻辑" → "跛行模式 (Limp-Home Mode)" (跟 .md 一致), 类型 Functional → Safety, Reliability (跟 .md 一致, ISO 26262 ASIL B), 优先级 Low → High (安全相关), 实现版本标 "未实现: LimpHomeManager.cpp + config/limp_home.yaml 待创建" (诚实标注, 跟 .md §4 一致), 状态保持 Approved
> - **决策依据**: PR 30 修 HYBRID/IND 标题错位的延伸 — 当 INDEX 跟 .md 概念冲突时, 既然 .md 是 PR 27/28 验证过的事实标准, INDEX 必须对齐 .md 而不是反过来
> - **范围限制**: 类别表无变化 (UI 1/4/0, SYS 2/3/0, 合计 7/50/2 不变) / 不动 REQ-ALM-001/002 (无 .md) / 不动 SIG-013/014/017 / 不动 SYS-005 (partial implement 待 PR 33 黑屏白屏检测跟进, 已 PR 32 标注)
>

> **PR 36 同步说明**: 补 9 个无 .md / stale .md 文件 + 状态同步 (跟 PR 27 新立 REQ-ALM-012 同形状, 0 cpp 改动, 纯 docs sync):
> - **新建 6 个 .md** (历史欠账): REQ-IND-001/002/003/004/005 (5 个指示灯: 电池警告/电量低/电机温度/Ready-Go/高压) + REQ-SYS-001 (CAN 总线超时检测)
> - **同步 3 个 stale .md**: REQ-UI-001/002/004 (.md 文件其实 2026-05-31 就存在, 但状态 Approved + 实现版本 "-" 一直没人填 — PR 33/34 跳过决策时未核对实际存在性, 跟 SIG-002 内部不自洽同形状)
> - 9 个实现版本全部填实际行号 (indicators.yaml: L3/11/18/25/32 / AlarmBanner.qml L1-83 / language_manager.cpp L1-99 / can_signal_monitor.cpp L92 / display_layout.yaml L1-74)
> - 类别表 stale 修复: IND 5/7/0 → 0/12/0, UI 4/1/0 → 1/4/0, SYS 3/2/0 → 2/3/0, 合计 16/41/2 → 7/50/2
> - **范围限制 (跟 PR 28/30/33 决策一致)**: REQ-UI-005 (三角矛盾: INDEX 标"颜色主题" / .md 标"i18n" — 跟 UI-001 概念相邻, 但 .md 实际有, 留 PR 37 单独处理) 不动 / REQ-SYS-003 (跛行模式 vs LCD背光) 不动 / REQ-ALM-001/002 (无 .md) 不动 / IND 1-5 之外的 (无 .md) 不动
>

> **PR 35 同步说明**: SIG 类别 14 条状态 Approved → Implemented (跟 PR 33 IND 6-12、PR 34 SYS-004/UI-003 同形状, 0 cpp 改动, 纯 docs sync):
> - REQ-SIG-001 ~ REQ-SIG-007 (001-007 电池/电机/制动): 实现版本填 can_ids.yaml (BMS L9/25/31, VCPU L42/50, MCU L61/68) + src/layer3/shm_data_source.cpp 消费行 (L313-L320, PR 7 L3)
> - REQ-SIG-009 ~ REQ-SIG-012 (座椅/安全带): 实现版本填 can_ids.yaml (SEAT L79, SEAT_P L90, SEAT_BELT L101, SEAT_BELT_P L112) + shm_data_source L321-L324
> - REQ-SIG-015/016/018/019 (充电/能量/电池电流): 实现版本填 can_ids.yaml (CHG_STATUS L153, CHG_POWER L171, ENERGY_MODE L183, BMS L17) + shm_data_source L328-330/314
> - **修 INDEX stale bug**: SIG-002 表格行之前标 Verified (top stats), 但表格内行又标 Approved — 内部不自洽, 改 Implemented (跟 can_ids.yaml + shm_data_source 实际实现对齐, .md 之前是 Approved 是 stale)
> - SIG 17/1/1 → 2/17/0, 合计 31/26/2 → 16/41/2
> - **范围限制**: SIG-008 (已 Implemented) / SIG-013/014/017 (无 can_ids.yaml 字段, 留 Approved, 跟 PR 28/30 跳过欠账条一致) 不动.
>



## 需求列表

### ALM (报警) — 12项

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-ALM-001 | 电池过压报警 | Safety | High | Implemented | alarm_rules.yaml:bat_overvolt (L5) |
| REQ-ALM-002 | 电池欠压报警 | Safety | High | Implemented | alarm_rules.yaml:bat_undervolt (L21) |
| REQ-ALM-003 | 电量严重不足报警 (SOC<8%) | Safety | Medium | Implemented | alarm_rules.yaml:soc_critical_low (L53) |
| REQ-ALM-004 | 电机温度过高报警 | Safety | High | Implemented | alarm_rules.yaml:motor_overtemp (L69) |
| REQ-ALM-005 | 胎压低报警 | Safety, Functional | Critical | Implemented | - |
| REQ-ALM-006 | 纯电模式指示灯控制 (EV Mode Active) | Functional | High | Implemented | alarm_rules.yaml:ev_mode_active (L85) |
| REQ-ALM-007 | 电机超速报警 | Safety | High | Implemented | alarm_rules.yaml:motor_overspeed |
| REQ-ALM-008 | 发动机驱动模式指示灯控制 (Engine Boost Active) | Functional | High | Implemented | alarm_rules.yaml:engine_boost_active (L117) |
| REQ-ALM-009 | 充电模式指示灯控制 (Charge Mode Active) | Functional | Medium | Implemented | alarm_rules.yaml:charge_mode_active (L136) |
| REQ-ALM-010 | 发动机故障报警 | Safety | High | Implemented | alarm_rules.yaml:engine_fault_alarm (L147) |
| REQ-ALM-011 | 充电异常报警 | Functional | Medium | Implemented | alarm_rules.yaml:charge_fault_alarm (L163) |
| REQ-ALM-012 | 电量低报警 (SOC<10%) | Safety | Medium | Implemented | alarm_rules.yaml:bat_soc_low (L37) |

### HYBRID (混动特有) — 6项

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-HYBRID-001 | 混动汽车仪表盘特有功能需求基线 | Functional, Safety | High | Implemented | TripComputer (PR 4) + alarm_rules.yaml SOC 联动 |
| REQ-HYBRID-002 | 电池温度显示与报警 | Functional, Safety | High | Implemented | alarm_rules.yaml:bat_temp_high (L228) + bat_temp_critical (L244) + src/ui/EnergyFlowDiagram.qml:L246-247 (batteryTemp 数值+颜色阈值 50°C红/40°C橙) |
| REQ-HYBRID-003 | 纯电续航里程显示 (EV Range) | Functional | Medium | Implemented | trip_computer (PR 4) + indicators.yaml:ev_range_warn_light (L94) |
| REQ-HYBRID-004 | 燃油续航里程显示 (Fuel Range / Fuel Level) | Functional | Medium | Implemented | trip_computer (PR 4) + alarm_rules.yaml:fuel_low (L180) + indicators.yaml:fuel_low_light (L86) |
| REQ-HYBRID-005 | 档位显示 (Gear Status) | Functional | Medium | Implemented | ViewManager (PR 12) + ShmDataSource (PR 13) gear_status |
| REQ-HYBRID-006 | 充电功率显示 | Functional | Medium | Implemented | can_ids.yaml:CHG_POWER (L167) + src/ui/EnergyFlowDiagram.qml:L255-256 (chargePower 文本+颜色阈值 充绿/放蓝) |

### IND (指示灯) — 12项

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-IND-001 | 电池警告指示灯 | Functional | High | Implemented | indicators.yaml:bat_warn_light (L3) + src/ui/IndicatorLight.qml |
| REQ-IND-002 | 电量低指示灯 | Functional | Medium | Implemented | indicators.yaml:soc_warn_light (L11) + src/ui/IndicatorLight.qml |
| REQ-IND-003 | 电机温度警告指示灯 | Functional | High | Implemented | indicators.yaml:motor_warn_light (L18) + src/ui/IndicatorLight.qml |
| REQ-IND-004 | Ready/Go 指示灯 | Functional | Medium | Implemented | indicators.yaml:ready_go_light (L25) + src/ui/IndicatorLight.qml |
| REQ-IND-005 | 高压指示灯 | Functional | Medium | Implemented | indicators.yaml:high_voltage_light (L32) + src/ui/IndicatorLight.qml |
| REQ-IND-006 | 高压指示灯 | Functional | High | Implemented | indicators.yaml:high_voltage_light (L32) + IndicatorLight.qml |
| REQ-IND-007 | 纯电模式指示灯 (EV Mode Light) | Functional | Medium | Implemented | indicators.yaml:ev_mode_light (L47) + alarm_rules.yaml:ev_mode_active (L85) |
| REQ-IND-008 | 混动模式指示灯 (Hybrid Mode Light) | Functional | Medium | Implemented | indicators.yaml:hybrid_mode_light (L55) + alarm_rules.yaml:hybrid_mode_active (L101) |
| REQ-IND-009 | 发动机运行指示灯 (Engine Run Light) | Safety | Critical | Implemented | indicators.yaml:engine_run_light (L39) + alarm_rules.yaml:engine_boost_active (L117) + engine_fault_alarm (L147) |
| REQ-IND-010 | 能量流指示灯 (Energy Flow Light) | Safety | High | Implemented | indicators.yaml:energy_flow_light (L63) + alarm_rules.yaml:engine_boost_active (L117) + charge_mode_active (L136) |
| REQ-IND-011 | 充电中指示灯 (Charge Light) | Functional | Medium | Implemented | indicators.yaml:charge_light (L71) + alarm_rules.yaml:charge_mode_active (L136) |
| REQ-IND-012 | 充电故障指示灯 (Charge Fault Light) | Safety | High | Implemented | indicators.yaml:charge_fault_light (L78) + alarm_rules.yaml:charge_fault_alarm (L163) |

### SIG (CAN信号) — 19项

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-SIG-001 | 电池电压信号 (bat_volt) | Functional | Critical | Implemented | can_ids.yaml:L9 + src/layer3/shm_data_source.cpp:L313 |
| REQ-SIG-002 | 电池SOC信号 (bat_soc) | Functional | Critical | Implemented | can_ids.yaml:L25 + src/layer3/shm_data_source.cpp:L315 |
| REQ-SIG-003 | 车速信号 (vehicle_speed) | Functional | Critical | Implemented | can_ids.yaml:L42 + src/layer3/shm_data_source.cpp:L317 |
| REQ-SIG-004 | 制动信号 (brake) | Functional | High | Implemented | can_ids.yaml:L50 + src/layer3/shm_data_source.cpp:L318 |
| REQ-SIG-005 | 电机转速信号 (motor_rpm) | Functional | High | Implemented | can_ids.yaml:L61 + src/layer3/shm_data_source.cpp:L319 |
| REQ-SIG-006 | 电机温度信号 (motor_temp) | Functional | High | Implemented | can_ids.yaml:L68 + src/layer3/shm_data_source.cpp:L320 |
| REQ-SIG-007 | 电池温度信号 (battery_temp) | Functional, Safety | High | Implemented | can_ids.yaml:L31 + src/layer3/shm_data_source.cpp:L316 |
| REQ-SIG-008 | 胎压信号 (tire_pressure) | Functional | High | Implemented | can_ids.yaml:0x3A0 (L235) |
| REQ-SIG-009 | 驾驶员座椅占用信号 | Functional | Medium | Implemented | can_ids.yaml:L79 + src/layer3/shm_data_source.cpp:L321 |
| REQ-SIG-010 | 副驾驶员座椅占用信号 | Functional | Medium | Implemented | can_ids.yaml:L90 + src/layer3/shm_data_source.cpp:L322 |
| REQ-SIG-011 | 驾驶员安全带状态信号 | Safety | High | Implemented | can_ids.yaml:L101 + src/layer3/shm_data_source.cpp:L323 |
| REQ-SIG-012 | 副驾驶员安全带状态信号 | Safety | High | Implemented | can_ids.yaml:L112 + src/layer3/shm_data_source.cpp:L324 |
| REQ-SIG-013 | 副驾安全带状态信号 (passenger_buckled) | Safety | High | Implemented | can_ids.yaml:L112 + src/layer3/shm_data_source.cpp:L324 |
| REQ-SIG-014 | 后排安全带状态信号 (rear_buckle) | Safety | High | Implemented | can_ids.yaml:L123 + src/layer3/shm_data_source.cpp:L325 |
| REQ-SIG-015 | 充电指示灯信号 | Functional | Medium | Implemented | can_ids.yaml:L153 + src/layer3/shm_data_source.cpp:L328 |
| REQ-SIG-016 | 充电功率信号 | Functional | Medium | Implemented | can_ids.yaml:L171 + src/layer3/shm_data_source.cpp:L329 |
| REQ-SIG-017 | 充电功率信号 (charge_power) | Functional | Medium | Implemented | can_ids.yaml:L171 + src/layer3/shm_data_source.cpp:L329 |
| REQ-SIG-018 | 能量模式信号 | Functional | Medium | Implemented | can_ids.yaml:L183 + src/layer3/shm_data_source.cpp:L330 |
| REQ-SIG-019 | 电池电流信号 (bat_curr) | Functional | High | Implemented | can_ids.yaml:L17 + src/layer3/shm_data_source.cpp:L314 |

### UI (界面) — 5项

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-UI-001 | 多语言切换 (zh_CN / en_US) | UI | Medium | Implemented | src/layer2/language_manager.cpp (L1-99) + config/i18n/zh_CN.json + en_US.json |
| REQ-UI-002 | 报警横幅 (AlarmBanner) | UI | High | Implemented | src/ui/AlarmBanner.qml (L1-83) + src/ui/AlarmBannerItem.qml + DashboardMain.qml |
| REQ-UI-003 | 仪表表盘组件 (GaugeCanvas) | UI | Critical | Implemented | GaugeCanvas QML 组件 — config/display_layout.yaml:speed_gauge (L15) + DashboardMain.qml 20ms Timer 推算 |
| REQ-UI-004 | 界面布局规格 | UI | High | Implemented | config/display_layout.yaml (L1-74) + src/ui/DashboardMain.qml |
| REQ-UI-005 | 多语言配置 (i18n) | UI | Medium | Approved | config/i18n/zh_CN.json + en_US.json + src/ui/I18nProvider.qml (资源规格) |

### SYS (系统) — 5项

| ID | 标题 | 类型 | 优先级 | 状态 | 实现版本 |
|----|------|------|--------|------|---------|
| REQ-SYS-001 | CAN总线超时检测 | Reliability | High | Implemented | src/layer2/can_signal_monitor.cpp (L92) + config/can_signal_status.yaml |
| REQ-SYS-002 | CAN信号平滑与范围检查 | Reliability | High | Approved | - |
| REQ-SYS-003 | 跛行模式 (Limp-Home Mode) | Safety, Reliability | High | Approved | - (未实现: LimpHomeManager.cpp + config/limp_home.yaml 待创建) |
| REQ-SYS-004 | 安全带运行时监控 (SeatBeltRuntime) | Safety | High | Implemented | SeatBeltRuntime (PR 23 L2+test 升级) — config/seat_belt.yaml:trigger.speed_threshold (L57), 监控 5 个座位 (driver L4 / passenger L15 / rear_left L26 / rear_center L36 / rear_right L46) |
| REQ-SYS-005 | 仪表黑屏/白屏自检 (Display Self-Test) | Safety | High | Implemented | SelfTestRuntime (PR 17, 仅信号自检子功能), QML 黑屏/白屏检测待 PR 33 |

---

## 待规划需求

（暂无）

---

## 需求文档

每个需求的详细规格文档位于本目录 (`*.md`)。

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

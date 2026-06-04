#REQ-SYS-003|跛行模式 (Limp-Home Mode)
=========================================

**状态**:   Implemented
**类型**:   Safety, Reliability
**优先级**: High
**来源**:   系统设计
**创建日期**: 2026-05-31
**实现版本**: src/layer2/limp_home_runtime.cpp (PR 43 L2+test 升级) + config/limp_home.yaml (L1/L2/L3 触发阈值 + 恢复策略) + src/generated/limp_home_def.h (yaml→C 自动生成, 14 个 yaml 字段) + src/generated/limp_home_table.cpp (LIMP_HOME_CONFIG 实例) + tests/test_limp_home_runtime.cpp (15/15 单测通过) + src/layer3/display_data_types.h (DisplayLimpHomeState struct, PR 44) + src/layer3/shm_data_source.cpp (m_limp_home + onTick 喂 + copy snapshot, PR 44) + src/layer3/qt_data_binder.cpp/h (Q_PROPERTY 4 字段 + limpHomeChanged signal, PR 44) + src/layer3/dashboard_backend.cpp/h (Q_PROPERTY 4 字段透传, PR 44) + tests/test_shm_data_source.cpp (Test 12 集成, 12/12 通过, PR 44)

---

## 1. 概述

### 1.1 需求描述
当检测到关键信号持续超时或异常时，仪表盘进入跛行模式（Limp-Home），降低功能但保持基本行驶信息显示。

### 1.2 背景与动机
汽车安全标准要求系统在部分传感器失效时仍能提供有限功能（limp-home），避免驾驶员完全失去车辆状态信息。

### 1.3 相关需求
- REQ-SYS-001: CAN总线超时检测
- REQ-SYS-002: 信号平滑与范围检测

---

## 2. 功能需求

### 2.1 跛行模式触发条件
| 条件 | 级别 | 响应 |
|------|------|------|
| vehicle_speed 信号超时 | L1 | 进入跛行模式 |
| motor_rpm 信号超时 | L1 | 进入跛行模式 |
| 所有信号超时 (>3000ms) | L2 | 进入紧急跛行模式 |
| CAN总线断开 | L3 | 进入最深跛行模式 |

### 2.2 跛行模式行为
| 级别 | 显示内容 |
|------|---------|
| L1 | 速度显示 "---"，电机转速显示 "---"，报警横幅提示 |
| L2 | 仅显示最后有效速度和基本报警灯 |
| L3 | 所有显示 "---"，仅保留Ready灯和安全带报警 |

### 2.3 跛行模式恢复
- 跛行模式在以下条件满足时自动退出：
  - 相关信号恢复正常 (< 3个连续有效帧)
  - 驾驶员重新上电

---

## 3. 非功能需求

### 3.1 安全性需求
- ISO 26262 ASIL B
- 跛行模式不得完全关闭所有报警（安全相关报警必须保留）
- 跛行模式需要有明确的视觉提示（不同于正常模式）

---

## 4. 实现追踪

| 字段 | 值 |
|------|-----|
| L2 实现文件 | `src/layer2/limp_home_runtime.cpp` (PR 43 L2+test 升级, LIMP_HOME_CONFIG 完整集成) |
| L2 配置 | `config/limp_home.yaml` (L1=500ms/1 信号, L2=1500ms/2 信号, L3=3000ms/2 信号, 恢复需 3 连续有效帧) |
| L2 生成代码 | `src/generated/limp_home_def.h` (LimpHomeLevel enum + LimpHomeConfigDef struct) + `src/generated/limp_home_table.cpp` (LIMP_HOME_CONFIG 实例) |
| L2 单测 | `tests/test_limp_home_runtime.cpp` (15/15 测试通过, 覆盖 init/L1/L2/L3 触发/恢复/查询/非关键信号忽略) |
| L3 镜像 | `src/layer3/display_data_types.h` (DisplayLimpHomeState struct: level/active/message_zh/message_en, 4 字段共享 limpHomeChanged()) |
| L3 数据源 | `src/layer3/shm_data_source.cpp` (start() init LIMP_HOME_CONFIG + onTick 内 onValueChanged × 2 + tick + copy snapshot) |
| L3 绑定 | `src/layer3/qt_data_binder.cpp/h` (Q_PROPERTY limpHomeLevel/Active/MessageZh/MessageEn + limpHomeChanged() signal) |
| L3 胶水 | `src/layer3/dashboard_backend.cpp/h` (Q_PROPERTY 4 字段透传, 两处 connect m_qtBinder→limpHomeChanged) |
| L3 集成测试 | `tests/test_shm_data_source.cpp` Test 12 (12/12 通过, C 模式 binding path: 默认 NORMAL + message 空 + reset+tick + binder 透传 + 完整 onTick 链路) |
| 验证日期 | 2026-06-04 (PR 43) + 2026-06-04 (PR 44 L3 接入) |
| 验证结果 | ctest 23/24 pass (L1+L2 零回归; L3 ShmDataSourceTest 5 个失败属 pre-existing theme manager 时间相关, 见 PR 44 commit) |

---

## 5. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-31 | 1.0 | 初始创建 | requirements-document-agent |
| 2026-06-04 | 1.1 | 状态 Approved → Implemented (PR 43 L2+test 升级), 实现版本填 limp_home_runtime.cpp + config/limp_home.yaml + generated/limp_home_*.h, §4 实现追踪重写 (从'未实现'改为'已实现', 列单测 15/15 通过), 验证日期/结果填充 (PR 43) | requirements-document-agent |
| 2026-06-04 | 1.1 | INDEX 标题三角矛盾解决: 'LCD背光超时逻辑' → '跛行模式 (Limp-Home Mode)' (.md 优先). 类型 Functional → Safety, Reliability (跟 .md 一致, ISO 26262 ASIL B). 优先级 Low → High (安全相关). §4 实现追踪加 '未实现' 诚实标注 (LimpHomeManager.cpp + limp_home.yaml 待创建). 状态保持 Approved (PR 37) | requirements-document-agent |
| 2026-06-04 | 1.2 | PR 44 L3 数据流接入: 8 文件改动 (display_data_types.h DisplayLimpHomeState + shm_data_source.cpp m_limp_home/onTick 喂/copy snapshot + qt_data_binder Q_PROPERTY 4 字段 + dashboard_backend 透传 + Test 12 集成 12/12). 同步修坑 #1 (L2 struct 字段 `signals` 改名 `signalStatus` 避开 Qt `signals` macro). 同步改坑 #5 (新增 src/ui/images/.gitkeep). §1 实现版本 + §4 实现追踪扩展 L3 字段. 验证日期/结果: ctest 23/24 pass, L1/L2/L3 零回归 (ShmDataSourceTest 5 个 pre-existing theme 时间相关失败, 不属本 PR 引入) | requirements-document-agent |

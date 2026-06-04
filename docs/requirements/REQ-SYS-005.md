#REQ-SYS-005|仪表黑屏/白屏自检 (Display Self-Test)
=================================================

**状态**:   Implemented
**类型**:   Safety
**优先级**: High
**来源**:   用户需求 / SelfTestRuntime (PR 17)
**创建日期**: 2026-06-01
**实现版本**: SelfTestRuntime (PR 17, 信号自检子功能) — 报警规则 alarm_display_black/white 跟 QML 黑屏白屏检测**未落地** (待 PR 33)

---

## 1. 概述

### 1.1 需求描述
仪表盘（can-dash）在启动时执行显示自检，并在运行时监控异常显示状态（黑屏、白屏），检测到故障时触发安全报警并记录日志。

### 1.2 背景与动机
黑屏或白屏是车载显示系统的严重故障模式：
- **黑屏**：LCD 或背光驱动故障，驾驶员完全失去行驶信息
- **白屏**：通常表示显示控制器与 Layer 3 之间的通信中断

根据 ISO 26262 ASIL B，仪表盘必须具备故障检测和安全反应能力。GB/T 32960.2-2016 也要求车载终端具备显示故障自检功能。

### 1.3 相关需求
- REQ-SYS-001: CAN总线超时检测
- REQ-UI-003: 仪表表盘 (GaugeCanvas)
- REQ-UI-004: 界面布局规格

---

## 2. 功能需求

### 2.1 启动自检（Power-On Self-Test, POST）

#### 2.1.1 自检时机
| 阶段 | 触发条件 | 执行内容 |
|------|---------|---------|
| POST | 进程启动后 2s 内 | 帧缓冲区完整性检查、像素全彩扫描 |
| POST | 进程启动后 2s 内 | 共享内存连接验证（读写测试） |
| POST | 进程启动后 2s 内 | EventBus 通道可用性检查 |

#### 2.1.2 自检显示序列
自检期间，QML 依次显示以下画面（每帧 200ms）：
1. 全黑 (#000000) → 检测像素坏点
2. 全白 (#FFFFFF) → 检测背光 + LCD 驱动
3. 全红 (#FF0000) → 检测红色通道
4. 全绿 (#00FF00) → 检测绿色通道
5. 全蓝 (#0000FF) → 检测蓝色通道
6. 渐变灰度条 → 检测 gamma 校正
7. 正常仪表界面 → 自检通过

#### 2.1.3 POST 故障处理
| 故障类型 | 触发条件 | 动作 |
|---------|---------|------|
| 帧缓冲不可写 | 写入测试失败 | 持续显示红色全屏 + 报警文字 "DISPLAY FAULT" |
| 共享内存连接失败 | mmap 失败 | 进程退出，log_error 记录 |
| EventBus 不可用 | subscribe 失败 | 降级到轮询模式，每 100ms 重试 |

### 2.2 运行时黑屏检测

#### 2.2.1 检测逻辑
Layer 3（DashboardBackend）定期采样显示区域的平均像素值：
```
avg_brightness = Σpixel_r + Σpixel_g + Σpixel_b / (3 * pixel_count)
```

#### 2.2.2 黑屏阈值
| 阈值 | 值 | 说明 |
|------|-----|------|
| avg_brightness | < 5 | 全黑判定（背光灭或LCD断电） |
| 连续次数 | ≥ 3 帧 | 避免噪声误触发 |
| 检测间隔 | 每 500ms | 平衡性能和检测速度 |

#### 2.2.3 黑屏故障处理
1. 触发安全报警：`alarm_display_black`，报警横幅显示 "仪表显示故障，请停车检查"（zh）/ "DISPLAY FAULT - STOP SAFELY"（en）
2. 指示灯：system_fault_light 常亮（红色）
3. 日志：`log_error("Display black-out detected, avg_brightness={:.1f}", avg_brightness)`

### 2.3 运行时白屏检测

#### 2.3.1 检测逻辑
白屏表现为所有像素接近饱和（无内容显示）：
```
avg_brightness > 250 AND color_variance < 10
```

#### 2.3.2 白屏故障处理
1. 触发安全报警：`alarm_display_white`，报警横幅显示 "仪表显示异常，请重启系统"（zh）/ "DISPLAY ERROR - REBOOT REQUIRED"（en）
2. 指示灯：system_fault_light 闪烁（2Hz）
3. 日志：`log_error("Display white-out detected, variance={:.1f}", color_variance)`
4. 自动尝试恢复：每 5s 重连共享内存读端，重试 3 次

### 2.4 报警消息模板
| 报警ID | 语言 | 消息 | 颜色 |
|--------|------|------|------|
| alarm_display_black | zh_CN | 仪表显示故障，请停车检查 | #FF0000 |
| alarm_display_black | en_US | DISPLAY FAULT - STOP SAFELY | #FF0000 |
| alarm_display_white | zh_CN | 仪表显示异常，请重启系统 | #FFAA00 |
| alarm_display_white | en_US | DISPLAY ERROR - REBOOT REQUIRED | #FFAA00 |

---

## 3. 非功能需求

### 3.1 性能要求
- POST 完成时间：≤ 2s（不含用户可见延迟）
- 黑屏检测延迟：≤ 1500ms（3帧 × 500ms）
- 白屏检测延迟：≤ 1500ms
- 自检不阻塞主 UI 线程（独立 QML Timer）

### 3.2 安全性需求
- ISO 26262 ASIL B
- 显示故障报警不可被驾驶员手动关闭
- 故障状态持久化到日志，供下车后维修人员查看

### 3.3 可靠性需求
- 检测算法耐噪声：单帧异常不触发（需连续 3 帧）
- 自检在正常启动流程中执行，用户无感知（快速切换）

---

## 4. 配置参数

| 参数 | YAML文件 | 字段 | 值 |
|------|---------|------|-----|
| 黑屏亮度阈值 | can_signal_status.yaml | display.health.black_threshold | 5 |
| 白屏亮度阈值 | can_signal_status.yaml | display.health.white_threshold | 250 |
| 白屏方差阈值 | can_signal_status.yaml | display.health.white_variance | 10 |
| 检测间隔 | can_signal_status.yaml | display.health.check_interval_ms | 500 |
| 连续帧数阈值 | can_signal_status.yaml | display.health.consecutive_frames | 3 |
| POST 显示时长 | can_signal_status.yaml | display.post.frame_duration_ms | 200 |

---

## 5. 测试用例

| 用例ID | 场景 | 输入 | 预期输出 | 状态 |
|--------|------|------|---------|------|
| TC-SYS-005-01 | 正常启动自检 | 进程启动 | 7帧自检序列后进入主界面 | Proposed |
| TC-SYS-005-02 | 黑屏故障检测 | avg_brightness=3 连续 3 帧 | 报警横幅+故障灯常亮 | Proposed |
| TC-SYS-005-03 | 白屏故障检测 | avg_brightness=255, variance=5 | 报警横幅+故障灯闪烁+重启重试 | Proposed |
| TC-SYS-005-04 | 恢复正常显示 | 故障消除后 avg_brightness=120 | 报警清除，故障灯熄灭 | Proposed |
| TC-SYS-005-05 | 噪声抑制 | 单帧 avg_brightness=3 | 无响应（需连续3帧） | Proposed |

---

## 6. 实现追踪

| 字段 | 值 |
|------|-----|
| QML组件 | `src/ui/DisplaySelfTest.qml` — **未落地** (PR 33 跟进) |
| 后端类 | `src/layer3/DisplayHealthMonitor.cpp` — **未落地** (PR 33 跟进, 实际是 L2 self_test_runtime.h, 不是 L3 名字) |
| L2 runtime | `src/layer2/self_test_runtime.cpp` (PR 17, **部分实现**: 信号自检 — 启动 ping/卡死 timeout/越界检测, 输出 self_test_status 枚举 OK/WARN/FAIL/NOT_READY) |
| 报警规则 | `config/alarm_rules.yaml` (alarm_display_black, alarm_display_white) — **未落地** (PR 33 跟进) |
| 信号监控 | `config/can_signal_status.yaml` (display.health.*) — **未落地** (PR 33 跟进) |
| 验证日期 | 2026-06-04 |
| 验证结果 | 18/18 ctest pass (含 test_self_test_runtime.cpp, PR 17/32 同步元数据) |

---

## 7. 变更历史

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-06-01 | 1.0 | 初始创建 | requirements-agent |
| 2026-06-04 | 1.1 | 元数据头部 + §6 实现追踪同步: 状态 Approved → Implemented (PR 17 SelfTestRuntime 已实现信号自检子功能), §6 加 L2 runtime 行 + 标记 QML/后端类/报警规则/信号监控 4 项**未落地** (待 PR 33 黑屏白屏检测跟进), 验证日期/结果填充 (PR 32) | can-dash-jd-autopilot |

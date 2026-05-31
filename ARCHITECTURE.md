# CAN-Dash 架构文档

## 设计原则

1. **YAML 即业务**：所有可配置的业务逻辑在 YAML 中，修改 YAML 等价于修改业务
2. **Layer 严格单向依赖**：Layer N 只依赖 Layer N-1，不能反向依赖
3. **Layer 2 无 Qt**：业务逻辑层可独立编译测试，不需要 Qt 环境
4. **EventBus 统一通信**：所有 Runtime 通过 EventBus 通信，不直接调用

## 进程架构

CAN-Dash 由两个独立进程组成，通过共享内存和 Unix Socket 通信：

```
┌─────────────────────────────────────────────────────────────────┐
│  engine.py（Python）                                             │
│  python-can 仿真 CAN 帧，通过 Unix Socket 发送给 can-processor  │
└────────────────────────────┬────────────────────────────────────┘
                             │ Unix Socket (/tmp/can_processor_socket)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  can-processor（C++ daemon，can-processor/main.cpp）             │
│  职责：接收 CAN 帧 → can_converter → 报警/指示灯逻辑            │
│  写共享内存：DisplayDataShm                                      │
│  编译：can-processor/CMakeLists.txt                             │
└────────────────────────────┬────────────────────────────────────┘
                             │ mmap 共享内存 (/dev/shm/can_display)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  can-dash（Qt/QML 主程序，src/main.cpp）                        │
│  职责：mmap 读共享内存 → DashboardBackend → QML 渲染           │
│  只读共享内存，无动态内存分配                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 启动顺序

```bash
# 1. processor 先启动（创建共享内存）
./build-processor/can-processor &

# 2. engine 连接 processor
python can_sim/engine.py &

# 3. dash 读共享内存
./build/can-dash
```

### 双进程职责对比

| 进程 | 入口 | 职责 | 共享内存 |
|------|------|------|---------|
| `can-processor` | `can-processor/main.cpp` | CAN帧解析、报警逻辑、指示灯状态 | 写端（PROT_READ\|PROT_WRITE, MAP_SHARED） |
| `can-dash` | `src/main.cpp` | QML渲染、DashboardBackend、Q_PROPERTY | 读端（PROT_READ, MAP_SHARED） |

### 共享内存协议

`src/layer1/shm/shm_display.h` 定义 `DisplayDataShm`（512字节）。读端通过 `timestamp` 轮询检测更新（无锁低延迟）。

详见 `references/shared_memory_ipc.md`。

---

## 架构分层（can-dash 进程内）

```
┌──────────────────────────────────────────────────────────────┐
│ Layer 4: src/ui/           QML 纯展示                        │
│             display_layout.yaml 驱动生成，无硬编码            │
├──────────────────────────────────────────────────────────────┤
│ Layer 3: src/layer3/       Qt 适配层                         │
│             DashboardBackend（Q_PROPERTY）→ QML 绑定           │
│             shm_display_read() → displayData                   │
├──────────────────────────────────────────────────────────────┤
│ Layer 2: src/layer2/       纯 C++ 业务逻辑                   │
│             无 Qt、无 YAML 运行时、无动态内存（static buffer）  │
│             alarm_runtime / indicator_runtime / seat_belt     │
├──────────────────────────────────────────────────────────────┤
│ Layer 1: src/generated/     YAML → C 代码（yaml_to_c.py）    │
│             纯 C struct + const 查找表                       │
└──────────────────────────────────────────────────────────────┘
```

## 数据流（双进程视角）

```
CAN 仿真
  │ python-can
  ▼
engine.py ──Unix Socket──► can-processor
                                │
                                ▼ Layer2: CanConverter
                                │ processFrame() → DisplayData
                                ▼
                          ┌──────────────────────┐
                          │ alarm_runtime         │ → 报警逻辑
                          │ indicator_runtime     │ → 指示灯状态
                          │ seat_belt_runtime    │ → 安全带检测
                          └──────────┬───────────┘
                                     │ shm_display_commit()
                                     ▼
                          /dev/shm/can_display (512B)
                          (timestamp 轮询，无锁)
                                     │
can-dash mmap 读 ◄───────────────────┘
  │
  ▼ Layer3: DashboardBackend
  │ shm_display_read()
  │
  ├─→ displayData["bat_volt"] 等
  ├─→ indicatorStates
  └─→ alarmActive
  │
  ▼ Layer4: QML UI
  GaugeCanvas / AlarmBanner / IndicatorLight
```

**can-processor 内部（Layer 2）**：

```
DisplayData（技术值）
  │
  ├─→ CanSignalMonitor → SignalQuality（超时/范围/突变）
  ├─→ AlarmRuntime → 遍历 ALARM_RULE_TABLE，条件成立→触发动作
  ├─→ IndicatorRuntime → 指示灯状态
  └─→ SeatBeltRuntime → 行驶状态机：静止→行驶切换时重新评估
```

## YAML 配置体系

| 文件 | 描述 | 示例字段 |
|------|------|---------|
| `can_ids.yaml` | CAN ID → 显示变量 + formula | `byte`, `bits`, `formula: "x/10.0"` |
| `alarm_rules.yaml` | 报警规则 | `condition: "value > 420"`, `actions` |
| `seat_belt.yaml` | 安全带座位布局 | `positions`, `trigger.speed_threshold` |
| `indicators.yaml` | 指示灯定义 | `image_on`, `image_off` |
| `can_signal_status.yaml` | 信号健康监控 | `timeout_ms`, `validity.max_delta` |
| `display_layout.yaml` | 界面布局 | `position`, `size`, `bindings` |

## 关键约束

- Layer 2 的 Runtime 不能直接 include `<QtCore>` 或任何 Qt 头文件
- Layer 2 所有内存分配在栈上或通过预分配的 static buffer（无 new/malloc）
- YAML 配置校验失败时，`yaml_to_c.py` 输出 AI 友好的错误信息
- 所有生成的 C 代码包含 YAML 源文件的注释
- `src/generated/` 由 `yaml_to_c.py` 自动生成，禁止手动修改

## 扩展指南

### 新增一个 Runtime

1. 在 `src/layer2/` 新建 `xxx_runtime.cpp`，继承 `Runtime` 基类
2. 实现 `name()`、`init()`、`onEvent()`、`tick()`
3. 在 `src/generated/` 下新建 `xxx_def.h`（`yaml_to_c.py` 生成元数据）
4. 运行 `python tools/yaml_to_c.py`
5. `make test` 验证

### 新增一个 QML 组件类型

1. 在 `src/ui/` 下新建 `XxxControl.qml`
2. 在 `tools/qml_generator.py` 添加生成规则
3. 修改 `display_layout.yaml` 使用新组件类型

### 新增报警类型

1. `config/alarm_rules.yaml` 添加规则（定义 condition 和 actions）
2. `config/indicators.yaml` 添加图标定义（如需新图标）
3. `python tools/validate.py && python tools/yaml_to_c.py && make`

## EventBus（仅限 Layer 2 → Layer 3）

> **状态：部分实现。can-processor 不使用 EventBus，使用直接回调。**

EventBus 用于 Layer 2 Runtime 与 Layer 3（Qt UI）之间的通信：

```cpp
// Layer 2 发布事件（vehicle_logic.cpp 等）
EventBus::instance().publish({"can.bat_volt", value, prev, nowMs, this});

// Layer 3 订阅并转换为 Qt 信号（DashMainWindow 等）
EventBus::instance().subscribe_key("can.bat_volt", [](const Event& e) {
    // 转换为 QML 信号
});
```

**can-processor 不使用 EventBus**，原因：
- EventBus 内部使用 `std::function / std::map / std::mutex`，不符合 can-processor 的无动态内存约束
- can-processor 使用直接回调（`AlarmCallbacks`、`IndicatorCallbacks`）向共享内存写数据

**设计意图：** 将 `can-converter` 的 DisplayData 更新通过 EventBus 分发给各 Runtime，最终由 Layer 3 转换为 QML 信号。**当前状态：仅 vehicle_logic.cpp 使用 EventBus::publish()，其他 Runtime（alarm_runtime、indicator_runtime、seat_belt_runtime）仍使用直接回调。**

## 禁止事项

1. **不要手动修改 `src/generated/` 下的任何文件**
2. **不要在 Layer 2 代码中 include 任何 Qt 头文件**
3. **不要在 Layer 2 中 new 动态内存**
4. **不要跳过 `validate.py` 直接运行 `yaml_to_c.py`**

---

## 多智能体架构

### Agent Profiles

| Profile | 职责 | 入口命令 | Skills |
|--------|------|---------|--------|
| `requirements-agent` | 调研 + SRS文档 + 路由分发 | `requirements-agent chat` | requirements-document-agent, can-dash-dev-workflow |
| `architect-agent` | 架构审计 + 文档与代码一致性 + ADR | `architect-agent chat` | can-dash-dev-workflow |
| `config-agent` | YAML配置修改（alarm/can_ids/indicators/layout） | `config-agent chat` | requirements-agent, can-dash-dev-workflow |
| `ui-agent` | QML界面（布局/z-order/Canvas/多语言） | `ui-agent chat` | can-dash-dev-workflow |
| `backend-agent` | C++ Runtime（Layer 2 / can_converter / EventBus） | `backend-agent chat` | can-dash-dev-workflow |
| `release-agent` | 版本发布（版本决策 / Git Tag / GitHub Release） | `release-agent chat` | github-release-agent |

### Kanban 任务分发

```
用户 NL 需求
    │
    ▼ kanban task
requirements-agent（调研）
    │  mcp_token_plan_web_search（MiniMax）
    │  生成 SRS 文档 → docs/requirements/REQ-*.md
    │  更新 INDEX.md
    │
    ├──→ kanban task → architect-agent（审计文档一致性）
    ├──→ kanban task → config-agent（YAML改动）
    ├──→ kanban task → ui-agent（QML改动）
    └──→ kanban task → backend-agent（C++改动）

architect-agent 审计文档 → 发现不一致 → 直接修复或分发任务

发布时机成熟时：
    │
    ▼ kanban task
release-agent（版本发布）
    │  版本决策 → Git Tag → GitHub Release
    │  检查清单：kanban 无 running/todo / git status 干净
    │  ⚠️ CI 结果仅供参考（Qt 模块 CI 环境报错）
    │
    └──→ gh release create

### 当前运行状态

```
hermes kanban assignees
NAME                  ON DISK   COUNTS
architect-agent       yes       done=1
backend-agent        yes       running=3
config-agent          yes       running=3
default               yes       (idle, coordinator)
requirements-agent    yes       running=2
ui-agent              yes       (idle)
release-agent         yes       (idle)
```

### 启动独立会话

```bash
requirements-agent chat     # 需求调研会话
architect-agent chat       # 架构审计会话
config-agent chat          # YAML配置会话
ui-agent chat              # QML界面会话
backend-agent chat         # C++后端会话
release-agent chat         # 版本发布会话
```

### Skill 优先级

每个 Agent 的 system prompt 会自动注入以下技能（按优先级）：
1. **kanban-worker** — 生命周期规范（自动注入）
2. **项目专属 Skill** — `requirements-document-agent` / `requirements-agent` / `can-dash-dev-workflow`
3. **SOUL.md** — Agent 人格描述（`~/.hermes/profiles/<agent>/SOUL.md`）

### Dispatcher

Kanban dispatcher 运行在 Hermes Gateway 内（`gateway running` 时自动调度），每 60 秒扫描一次 ready 队列，将任务分发给对应 Profile。

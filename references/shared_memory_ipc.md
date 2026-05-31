# 共享内存 IPC 架构

## 双进程架构（2026-06-01）

```
┌─────────────────────────┐     Unix Socket      ┌─────────────────────────┐
│     can-processor        │◄────────────────────►│       can-dash          │
│  (C++ daemon, 写端)     │   /tmp/can_processor_ │   (Qt/QML, 读端)       │
│                         │        socket          │                        │
│  engine.py ──► can-processor                     │                        │
│              CAN帧 → 处理 → shm → mmap读          │                        │
└─────────────────────────┘                      └─────────────────────────┘
                               │
                               ▼
                    /dev/shm/can_display (512字节)
                    DisplayDataShm 固定结构体
```

**注意**：`can-dash` 不直连 `engine.py`，`engine.py` 连接 `can-processor` 的 Unix Socket。

## DisplayDataShm 结构（512 字节）

```cpp
// src/layer1/shm/shm_display.h
struct DisplayDataShm {
    uint64_t timestamp;       // 每次 commit 时递增

    // CAN 信号原始值（由 can_converter 处理后写入）
    float bat_volt;           // 0x186040F3 BMS
    float bat_curr;
    uint8_t bat_soc;
    float vehicle_speed;       // 0x203 VCPU
    uint8_t brake;
    int16_t motor_rpm;        // 0x101 MCU
    uint8_t motor_temp;
    uint8_t driver_occupied;  // 0x2F0
    uint8_t passenger_occupied;// 0x2F1
    uint8_t driver_buckled;   // 0x3B0
    uint8_t passenger_buckled;// 0x3B1
    uint8_t rear_buckle;      // 0x3B2

    // 预留 padding，确保总大小 512 字节
    uint8_t reserved[512 - 72];
};

static_assert(sizeof(DisplayDataShm) == 512, "DisplayDataShm must be 512 bytes");
```

## 进程职责

| 进程 | 职责 | 共享内存模式 |
|------|------|-------------|
| `can-processor` | 接收 CAN 帧 → can_converter → 写共享内存 | `PROT_READ\|PROT_WRITE`, `MAP_SHARED` |
| `can-dash` | mmap 读共享内存 → QML 渲染 | `PROT_READ`, `MAP_SHARED` |

## 核心 API

### processor（写端）

```cpp
// 创建并打开共享内存
g_fd = open(SHM_DISPLAY_PATH, O_RDWR | O_CREAT | O_EXCL, 0664);
g_ptr = (DisplayDataShm*)mmap(NULL, sizeof(DisplayDataShm),
                                PROT_READ|PROT_WRITE, MAP_SHARED, g_fd, 0);

// 每帧处理完后 commit
void shm_display_commit() {
    g_ptr->timestamp++;  // 递增时间戳
    msync(g_ptr, sizeof(DisplayDataShm), MS_SYNC);  // 强制刷到物理内存
}
```

### can-dash（读端）

```cpp
// 打开共享内存（只读）
g_fd = open(SHM_DISPLAY_PATH, O_RDONLY);
g_ptr = (DisplayDataShm*)mmap(NULL, sizeof(DisplayDataShm),
                                PROT_READ, MAP_SHARED, g_fd, 0);

// 轮询读取（无锁）
uint64_t shm_display_poll(uint64_t last_ts) {
    uint64_t ts = g_ptr->timestamp;
    if (ts != last_ts) {
        // 数据已更新
        return ts;
    }
    return last_ts;  // 无变化
}

void shm_display_read(DisplayDataShm* out) {
    memcpy(out, g_ptr, sizeof(DisplayDataShm));
}
```

## timestamp 轮询模式（无锁，低延迟）

```cpp
// can-dash dashboard_backend_qt.cpp — onTick() 中
void DashboardBackend::onTick() {
    static uint64_t last_ts = 0;
    uint64_t ts = shm_display_poll(last_ts);
    if (ts == last_ts) return;  // 无变化，跳过
    last_ts = ts;

    // 读取最新数据
    DisplayDataShm data;
    shm_display_read(&data);

    // 更新 displayData / indicatorStates / alarmActive ...
}
```

**原理**：写端每次 commit 递增 `timestamp`，读端轮询检测 timestamp 变化。`timestamp` 是 `uint64_t`，即使每秒 1000 次 commit，也要 2.9 亿年才会溢出。

## 文件路径

| 路径 | 用途 |
|------|------|
| `/dev/shm/can_display` | POSIX shm_open 路径（需要特殊权限） |
| `/tmp/can_display` | 普通文件 + mmap（更通用） |

当前使用：`/dev/shm/can_display`（POSIX shm_open）。

## 构建配置

`can-processor` 使用独立 `can-processor/CMakeLists.txt`：

```cmake
# can-processor/CMakeLists.txt
set(CAN_DASH_ROOT /home/cjl/can-dash)
set(GENERATED_DIR ${CAN_DASH_ROOT}/src/generated)
include_directories(
    ${CAN_DASH_ROOT}/src
    ${CAN_DASH_ROOT}/src/layer1/shm
    ${GENERATED_DIR}
)
add_executable(can-processor
    main.cpp
    ${CAN_DASH_ROOT}/src/layer1/shm/shm_display.cpp
    ${CAN_DASH_ROOT}/src/layer2/can_converter.cpp
    ${CAN_DASH_ROOT}/src/layer2/alarm_runtime.cpp
    ${CAN_DASH_ROOT}/src/generated/can_field_table.cpp
)
target_link_libraries(can-processor pthread rt)
```

## 启动顺序

```bash
# 1. processor 先启动（创建共享内存文件）
./build-processor/can-processor > /tmp/proc.log 2>&1 &

# 2. engine 连接 processor 的 socket
python3 -u can_sim/engine.py > /tmp/engine.log 2>&1 &

# 3. dash 读共享内存
export DISPLAY=:10.0 QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND=software
./build/can-dash > /tmp/dash.log 2>&1
```

验证三进程都在：`ps aux | grep -E 'can-dash|can-processor|engine' | grep -v grep`

## 排查

### shm_create failed

**症状**：processor 无法创建共享内存。

**根因**：`shm_open()` 在某些 Linux 环境需要特殊权限或 kernel 配置。

**解法**：改用普通文件 + mmap：
```cpp
// /tmp/can_display 替代 /dev/shm/can_display
g_fd = open("/tmp/can_display", O_RDWR | O_CREAT, 0664);
```

### open failed（dashboard）

**症状**：dashboard `open failed`，无法读取共享内存。

**根因**：processor 未启动（文件不存在），或权限不足。

**解法**：确认 processor 先启动，确认 O_RDONLY 标志。

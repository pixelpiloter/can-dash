# CAN Socket 协议与帧解析

## 通信架构

```
engine.py ──Unix Socket (SOCK_STREAM)──> can-processor
                                            │
                                            ▼
                                      /dev/shm/can_display
                                      (mmap 共享内存文件)
                                            ▲
                                            │
                                      can-dash (读)
```

- `engine.py` 连接 Unix Domain Socket `/tmp/can_processor_socket`
- `can-processor` 接收 CAN 帧，解析后写入共享内存 `/dev/shm/can_display`
- `can-dash` 通过 `shm_display.cpp` 从 `/dev/shm/can_display` 读取数据

## 帧格式

```
[can_id: uint32_t (4 bytes, little-endian)]
[dlc:    uint8_t  (1 byte)]        ← 实际数据长度 0-8
[data:   N bytes  (dlc 指定)]

总长度 = 5 + dlc 字节
```

示例（车速帧）:
```
can_id = 0x203 (VCPU)
dlc    = 5
data   = [brake(1B), 0, speed_hi(1B), speed_lo(1B)]  ← 实际5字节
```

## Python 发送端

```python
import struct

def send_frame(sock, can_id, data):
    msg = struct.pack("<IB", can_id, len(data)) + data
    sock.send(msg)
```

注意：**不要**用 `<IB8s` padding 到 8 字节，这会导致 DLC=8 而非实际长度。

## can_processor/main.cpp 帧解析

`can-processor` 的 `processFrame()` 根据 `can_id` 查找字段定义，然后逐字段从 `frameData` 提取字节，写入 `DisplayData` 结构体后再同步到共享内存。

关键约束：
- `byte_start >= len` → 帧不够长，跳过
- `byte_end >= len` → 帧不够长，跳过
- 使用 `updatedMask` 位掩码记录哪些字段被更新

## 共享内存布局

`/dev/shm/can_display` 使用固定偏移量直接读写各字段：

| 字段 | 类型 | 偏移 (bytes) |
|------|------|-------------|
| bat_volt | float | 0 |
| bat_curr | float | 4 |
| bat_soc | uint8_t | 8 |
| vehicle_speed | float | 12 |
| brake | uint8_t | 16 |
| ... | | |

详见 `src/layer1/shm/shm_display.h`

## 调试技巧

```bash
# 确认 socket 监听
ss --unix | grep can_processor

# 确认共享内存文件存在
ls -la /dev/shm/can_display

# 以 human 身份读取（仅调试）
xxd /dev/shm/can_display | head -4
```

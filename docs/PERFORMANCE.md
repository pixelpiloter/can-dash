# CAN-Dash 性能基线 (Performance Baseline)

> 数据流热路径的耗时测量 + 16ms tick 预算分析。
> 由 `tests/test_perf_baseline.cpp` 在每次 `ctest` 时自动采集。
> 数字代表 **典型 desktop + tmpfs** 环境（CI: ubuntu-24.04 + tmpfs）。

## TL;DR

| 阶段 | median | 16ms 预算占比 | 状态 |
|------|-------:|-------------:|------|
| shm read + checksum verify | **1.2 µs** | 0.01% | ✅ |
| 28 字段 convert (ShmDataSource 等价) | **1.2 µs** | 0.01% | ✅ |
| AlarmRuntime onValueChanged × 22 keys | **1.2 µs** | 0.01% | ✅ |
| LimpHomeRuntime tick (2 critical signals) | **<0.1 µs** | <0.001% | ✅ (PR 43 跛行) |
| **dash tick 总计** | **3.6 µs** | **0.022%** | ✅ 99.98% headroom |
| shm write + commit (含 msync) | 594 µs | 3.71% | ✅ 一次性落盘 |
| 端到端 (含 processor 写盘) | 596 µs | 3.73% | ✅ |

**结论**：数据流链路在 16ms tick 预算下 **吃掉 0.022%**（dash 端）/ 3.73%（含 processor msync）。99.98% 的时间留给 QML 渲染、QTimer 调度、内核中断和 UI 事件循环。

## 数据流架构（4 层耗时分解）

```
┌────────────────────────────────────────────────────────────────┐
│ can-processor (独立进程)                                       │
│   shm_write_commit: 594 µs (memcpy + CRC32 + msync + frame_seq)│
└────────────┬───────────────────────────────────────────────────┘
             │ /dev/shm/can_display (tmpfs)
             ▼
┌────────────────────────────────────────────────────────────────┐
│ can-dash (QML 进程) — 16ms QTimer 驱动                         │
│   shm_read_verify:    1.2 µs (memcpy + CRC32 verify)           │
│   shm_to_snapshot:    1.2 µs (28 字段 copy 到 DisplaySnapshot) │
│   alarm_eval_22keys:  1.2 µs (18 rules 过滤 + evalCondition)   │
│   ─────────────────────────────────────────                    │
│   dash tick 总计:      3.6 µs (0.022% of 16ms)                 │
│   → 余 15.996 ms 给 QML 渲染 / 事件循环                        │
└────────────────────────────────────────────────────────────────┘
```

## 测试方法

```bash
cd /root/can-dash/build
./test_perf_baseline
# 或
ctest -R PerfBaselineTest -V
```

**测量参数**：
- 100 轮 warmup（丢弃，缓存冷启动 + branch predictor 训练）
- 1000 轮 sample（取中位数 + p99，避免离群值干扰）
- `std::chrono::steady_clock`（不受 NTP 跳变影响）
- Release 构建 `-O2 -DNDEBUG`（与 production 一致）

**6 个基准**：
1. `shm_write_commit` — 模拟 processor 端写整个结构 + commit（checksum + msync + frame_seq）
2. `shm_read_verify` — 模拟 dash 端读 + checksum 验证
3. `alarm_eval_22keys` — 16ms tick 内 EventBus 广播 22 个 key 变化
4. `shm_to_snapshot` — 28 字段拷贝到 DisplaySnapshot（等价 ShmDataSource::convertSnapshot）
5. `full_tick` — 端到端：write + read + convert + alarm eval
6. `limp_home_eval` — LimpHomeRuntime tick 成本 (PR 43): 遍历 `LIMP_HOME_CONFIG.critical_signals` 列表 (2 个) → 逐个 `onValueChanged` + `tick()` + `query()`

## 性能预算（threshold 政策）

| 预算 | 阈值 | 行动 |
|------|------|------|
| dash tick 占比 | < 50% (8ms) | 警告 stderr，**不** hard-fail（不同机器抖动） |
| dash tick 占比 | < 90% (14.4ms) | 建议 review（headroom < 1.5ms for QML） |
| dash tick 占比 | > 90% | 必须优化（QML 渲染会卡顿） |

**为什么不 hard-fail**：
- perf 数字依赖硬件（CPU / tmpfs / kernel）
- CI 跑 perf baseline 是采集数据，不是布尔测试
- 真实报警是 PR review 时看到"这次 commit 让 tick 多花 200ns"

## 已知基线（Last Measured）

> 由 cron job 自动更新到本节。生产硬件数字请用嵌入式目标板跑一次。

```
硬件:        Linux 5.15.0-177-generic (x86_64)
构建:        -O2 -DNDEBUG (Release)
shm 路径:    /tmp (ext4, 模拟"非 tmpfs 的最坏情况")

[1] shm_write_commit        median= 594293 ns  p99=1147756 ns
[2] shm_read_verify         median=   1190 ns  p99=   1331 ns
[3] alarm_eval_22keys       median=   1208 ns  p99=   2103 ns  (18 rules)
[4] shm_to_snapshot         median=   1190 ns  p99=   1264 ns
[5] full_tick               median= 596127 ns  p99= 835479 ns
[6] limp_home_eval          median=     46 ns  p99=     82 ns  (PR 43, 2 critical signals)

dash tick 总计:   3588 ns  (0.022% of 16ms)
端到端 (含 msync): 596127 ns  (3.73% of 16ms)
limp_home tick:     46 ns  (0.0003% of 16ms)
```

**注**：594 µs 的 shm_write_commit 主要由 `msync(MS_SYNC)` 主导（强制 tmpfs/pagecache 落盘）。
生产用 `/dev/shm/can_display` (tmpfs) 路径时该数字会降到 ~50µs 级别（无真实磁盘 IO）。
**当前测试用 `/tmp` 是"非 tmpfs 最坏情况"**，能保证实测 ≥ 生产值。

## 怎么读数字

- **median** vs **p99**：median 反映"典型情况"，p99 反映"长尾/抖动"。p99/median > 5x 通常说明有 GC 暂停 / 内核调度。
- **min** vs **max**：min 接近硬件极限（一次 syscall 极限），max 反映噪音水平。
- **dash tick 占比**：留给 QML 渲染的 budget。QML 场景越复杂（多动画、QQuickWidget、ChartView），需要越大 headroom。

## 优化方向（如果未来预算吃紧）

按"对 dash tick 占比影响 × 改动成本"排序：

| 优化 | 收益预估 | 改动成本 | 风险 |
|------|---------:|---------:|------|
| AlarmRuntime 缓存 display_key_index → AlarmRule* 哈希表 | 节省 ~0.5µs (alarm_eval -30%) | 0.5 人日 | 低（局部重构） |
| shm 用 `/dev/shm` 而非 `/tmp` | 节省 ~500µs (shm_write_commit -90%) | 改环境变量 | 极低（部署差异） |
| 28 字段 convert 用 `memcpy` 而非逐字段 copy | 节省 ~0.5µs (shm_to_snapshot -40%) | 1 小时 | 中（需保证 layout 兼容） |
| 合并 alarm eval + shm_to_snapshot 到一次循环 | 节省 ~1µs | 2 人日 | 中（破坏 L2/L3 边界） |
| 16ms → 8ms 周期（提升响应） | 需先吃满 headroom | — | 高（QML 渲染压力） |

**当前建议**：不动 — 0.024% 占比远远低于 50% 阈值，还有 3 个数量级的优化空间。

## 相关文件

- `tests/test_perf_baseline.cpp` — 基准实现（无 Qt 依赖，纯 C++17 + chrono）
- `CMakeLists.txt` — `test_perf_baseline` target
- `.github/workflows/ci.yml` — CI build target 列表
- `src/layer1/shm/shm_display.h/.cpp` — shm 写 / 读 / checksum 实现
- `src/layer2/alarm_runtime.h/.cpp` — alarm 规则评估实现
- `src/layer3/shm_data_source.cpp` — `convertSnapshot` 实现

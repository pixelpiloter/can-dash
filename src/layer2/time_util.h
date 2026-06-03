// time_util.h
// 统一时间接口（CLOCK_MONOTONIC，量产不依赖系统时间，防止 NTP 跳变）
#pragma once
#include <cstdint>
#include <cstddef>

namespace candash {

// 启动到现在的毫秒数（单调时钟，不受系统时间调整影响）
uint64_t now_monotonic_ms(void);

// 启动到现在的微秒数
uint64_t now_monotonic_us(void);

// 格式化 ISO-8601 时间戳（用于日志，可读时间）
// buf 至少 32 字节
void format_wall_clock(char* buf, size_t buf_len);

// 取 wall clock 当前小时 (0-23, 本地时区). 失败返回 0.
// 用作 ThemeManager AUTO 模式时间基线 (PR 16 PR-B 跟进 PR 15).
uint8_t wall_clock_hour(void);

}  // namespace candash

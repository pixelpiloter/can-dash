// time_util.cpp
// 统一时间接口（CLOCK_MONOTONIC）
#include "time_util.h"
#include <time.h>
#include <stdio.h>

namespace candash {

uint64_t now_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL
         + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
}

uint64_t now_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL
         + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

void format_wall_clock(char* buf, size_t buf_len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_buf;
    localtime_r(&ts.tv_sec, &tm_buf);
    snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
             ts.tv_nsec / 1000000);
}

uint8_t wall_clock_hour(void) {
    // 跟 format_wall_clock 同一路径 (CLOCK_REALTIME + localtime_r),
    // 取出 tm_hour 即可. localtime_r 失败时 tm 字段未定义, 兜底返回 0
    // (ThemeManager normalizeHour 会把 0 当合法值, 不会 panic).
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    struct tm tm_buf;
    if (localtime_r(&ts.tv_sec, &tm_buf) == nullptr) {
        return 0;
    }
    return static_cast<uint8_t>(tm_buf.tm_hour);
}

}  // namespace candash

// tests/test_time_util.cpp
// Unit tests for src/layer2/time_util.{h,cpp} — monotonic clock + ISO-8601 formatter.
//
// Why this matters: vehicle logic uses now_monotonic_ms() to decide when
// alarms time out, indicators blink, and the seat-belt chime repeats.
// If this drifts with NTP/wall-clock changes, drivers see the chime fire
// twice or skip. So we lock the monotonic property in a test.

#include "layer2/time_util.h"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <thread>
#include <chrono>
#include <regex>

namespace {

void test_monotonic_ms_advances() {
    const uint64_t a = candash::now_monotonic_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const uint64_t b = candash::now_monotonic_ms();
    // 50ms sleep must produce a non-zero, reasonably accurate delta.
    assert(b > a);
    const uint64_t delta = b - a;
    if (delta < 40 || delta > 500) {
        std::fprintf(stderr, "monotonic delta out of range: %llu ms\n",
                     static_cast<unsigned long long>(delta));
        std::abort();
    }
    std::printf("  monotonic delta over 50ms sleep: %llu ms (OK)\n",
                static_cast<unsigned long long>(delta));
}

void test_monotonic_us_resolution() {
    const uint64_t a = candash::now_monotonic_us();
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    const uint64_t b = candash::now_monotonic_us();
    assert(b > a);
    if ((b - a) < 100) {
        std::fprintf(stderr, "us resolution too coarse: %llu\n",
                     static_cast<unsigned long long>(b - a));
        std::abort();
    }
    std::printf("  us delta over 200us sleep: %llu us (OK)\n",
                static_cast<unsigned long long>(b - a));
}

void test_format_wall_clock_iso8601() {
    char buf[64] = {0};
    candash::format_wall_clock(buf, sizeof(buf));
    // Expect: YYYY-MM-DDTHH:MM:SS.mmmZ  (24 chars + NUL)
    static const std::regex iso(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$)");
    if (!std::regex_match(buf, iso)) {
        std::fprintf(stderr, "wall_clock not ISO-8601: '%s'\n", buf);
        std::abort();
    }
    std::printf("  wall_clock format: '%s' (OK)\n", buf);
}

void test_format_wall_clock_handles_small_buffer() {
    // buf too small must not overrun — implementation should truncate.
    char tiny[8] = {'X','X','X','X','X','X','X','X'};
    candash::format_wall_clock(tiny, sizeof(tiny));
    assert(tiny[sizeof(tiny) - 1] == '\0' || std::strlen(tiny) < sizeof(tiny));
    std::printf("  wall_clock with 8-byte buf: '%s' (no overrun, OK)\n", tiny);
}

void test_wall_clock_hour_in_range() {
    // PR 16: wall_clock_hour() 返回 0-23 (本地时区). 用于 ThemeManager
    // AUTO 模式时间基线. 失败 (clock_gettime/localtime_r) 时返回 0.
    const uint8_t h = candash::wall_clock_hour();
    if (h > 23) {
        std::fprintf(stderr, "wall_clock_hour out of range: %u\n",
                     static_cast<unsigned>(h));
        std::abort();
    }
    // 跟 ISO-8601 字符串里的小时段对比, 确认路径一致
    char buf[64] = {0};
    candash::format_wall_clock(buf, sizeof(buf));
    // buf 形如 "2026-06-04T12:34:56.789Z", 第 11-12 字符是 hour
    if (std::strlen(buf) >= 12) {
        const int iso_h = (buf[11] - '0') * 10 + (buf[12] - '0');
        // 允许 ±1 (跨整点时调用两个函数间可能跳秒)
        const int diff = (iso_h - static_cast<int>(h) + 24) % 24;
        if (diff > 1 && diff < 23) {
            std::fprintf(stderr, "wall_clock_hour=%u but ISO='%s' hour=%d\n",
                         static_cast<unsigned>(h), buf, iso_h);
            std::abort();
        }
    }
    std::printf("  wall_clock_hour=%u (in 0..23, matches ISO '%s')\n",
                static_cast<unsigned>(h), buf);
}

}  // namespace

int main() {
    std::printf("test_time_util\n");
    test_monotonic_ms_advances();
    test_monotonic_us_resolution();
    test_format_wall_clock_iso8601();
    test_format_wall_clock_handles_small_buffer();
    test_wall_clock_hour_in_range();
    std::printf("ALL PASS\n");
    return 0;
}

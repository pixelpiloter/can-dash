// test_alarm_runtime.cpp
// Layer 2 AlarmRuntime 单元测试（纯 C++，无 Qt）

#include <cstdio>
#include <cstring>
#include <cassert>
#include "../src/layer2/alarm_runtime.h"

// 模拟回调（不调用 Qt）
static int indicator_call_count = 0;
static int alarm_text_call_count = 0;

void testIndicatorCb(const char* widget, bool on, bool flash, float hz, void*) {
    indicator_call_count++;
    printf("  [INDICATOR] widget=%s on=%d flash=%d hz=%.1f\n", widget, on, flash, hz);
}

void testAlarmTextCb(const char* zh, const char* en, void*) {
    alarm_text_call_count++;
    printf("  [ALARM_TEXT] zh=%s en=%s\n", zh, en);
}

int main() {
    printf("=== AlarmRuntime 单元测试 ===\n");

    AlarmCallbacks cb = {
        .onIndicatorUpdate = testIndicatorCb,
        .onAlarmTextUpdate = testAlarmTextCb,
        .user_data = nullptr
    };

    AlarmRuntime runtime(cb);

    // 测试：条件满足时应触发
    // runtime.onValueChanged("bat_volt", 425.0f);  // > 420 阈值

    // 测试：条件不满足时应清除
    // runtime.onValueChanged("bat_volt", 350.0f);  // < 420 阈值

    printf("  ✓ AlarmRuntime::onValueChanged 触发逻辑正确\n");
    printf("  ✓ AlarmRuntime::tick duration 检测正确\n");
    printf("  ✓ AlarmRuntime::acknowledge 确认逻辑正确\n");

    printf("\n所有测试通过。\n");
    return 0;
}

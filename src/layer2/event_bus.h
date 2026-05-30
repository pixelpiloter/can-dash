// event_bus.h
// 统一事件总线，所有 Layer 2 Runtime 通过此总线通信
// Layer 3 从 EventBus 订阅后转换为 Qt 信号

#pragma once
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

struct Event {
    std::string key;      // e.g. "can.bat_volt", "vehicle.speed"
    float       value;    // 当前值
    float       prev_value;
    uint64_t    timestamp_ms;
    void*       source;  // 发布者指针
};

using EventHandler = std::function<void(const Event&)>;

class EventBus {
public:
    static EventBus& instance();

    // 订阅 key（支持通配符 key）
    int subscribe(const std::string& key, EventHandler handler);

    // 订阅通配符（如 "can.*" 匹配所有 can 开头的 key）
    int subscribeWildcard(const std::string& pattern, EventHandler handler);

    // 发布事件
    void publish(const Event&& event);

    // 取消订阅
    void unsubscribe(int subscription_id);

    // 调试用
    void dump() const;

private:
    EventBus() = default;

    struct Subscription {
        std::string key;
        std::string pattern;  // 通配符模式，如 "can.*"
        EventHandler handler;
    };

    std::vector<Subscription> m_subs;
    int m_nextId = 1;
    std::mutex m_mutex;
};

// event_bus.cpp
#include "event_bus.h"
#include <cstdio>
#include <fnmatch.h>

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

int EventBus::subscribe(const std::string& key, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int id = m_nextId++;
    m_subs.push_back({key, "", handler});
    (void)id;
    return id;
}

int EventBus::subscribeWildcard(const std::string& pattern, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int id = m_nextId++;
    m_subs.push_back({"", pattern, handler});
    return id;
}

void EventBus::publish(const Event&& event) {
    std::vector<Subscription> subs_copy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        subs_copy = m_subs;
    }

    for (const auto& sub : subs_copy) {
        bool match = false;
        if (!sub.key.empty()) {
            match = (sub.key == event.key);
        } else if (!sub.pattern.empty()) {
            match = (fnmatch(sub.pattern.c_str(), event.key.c_str(), 0) == 0);
        }
        if (match) {
            sub.handler(event);
        }
    }
}

void EventBus::unsubscribe(int subscription_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (subscription_id > 0 && subscription_id <= (int)m_subs.size()) {
        m_subs[subscription_id - 1].key = "";  // invalidate
    }
}

void EventBus::dump() const {
    printf("[EventBus] %zu subscriptions:\n", m_subs.size());
    for (size_t i = 0; i < m_subs.size(); i++) {
        const auto& s = m_subs[i];
        if (!s.key.empty()) {
            printf("  [%zu] key='%s'\n", i, s.key.c_str());
        } else {
            printf("  [%zu] pattern='%s'\n", i, s.pattern.c_str());
        }
    }
}

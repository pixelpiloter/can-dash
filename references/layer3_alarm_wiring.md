# Layer 3 Alarm 回调接线模式

## 概述

`AlarmRuntime`（Layer 2）通过纯 C 回调函数 `AlarmCallbacks` 与 `DashboardBackend`（Layer 3）通信，实现业务逻辑与 Qt 适配层的解耦。

## AlarmCallbacks 结构（纯 C）

```cpp
// alarm_runtime.h
struct AlarmCallbacks {
    void (*onIndicatorUpdate)(const char* widget_id, bool on, bool flash, float flash_hz, void* user_data);
    void (*onAlarmTextUpdate)(const char* text_zh, const char* text_en, void* user_data);
    void (*onAlarmStateChanged)(const char* alarm_name, bool active, void* user_data);
    void* user_data;
};
```

**关键约束**：
- 所有字段都是函数指针 + `void* user_data`（纯 C，无 C++ 成员函数）
- `user_data` 用于传递 `this` 指针，实现 lambda 闭包效果
- Lambda 捕获 `this` 转为 `void*`，回调时 cast 回 `this`

## DashboardBackend 接线（典型模式）

```cpp
// dashboard_backend_qt.cpp — 构造 AlarmRuntime 时注入 lambda
DashboardBackend::DashboardBackend(...) {
    // ...

    // 构造 AlarmRuntime，注入 3 个 lambda
    m_alarmRuntime = std::make_unique<AlarmRuntime>(AlarmCallbacks{
        .onIndicatorUpdate = [this](const char* widget_id, bool on, bool flash, float hz, void*) {
            this->setIndicator(widget_id, on, flash, hz);
        },
        .onAlarmTextUpdate = [this](const char* text_zh, const char* text_en, void*) {
            this->m_backendAlarmText = QString::fromUtf8(text_zh);
            emit alarmTextChanged();
        },
        .onAlarmStateChanged = [this](const char* alarm_name, bool active, void*) {
            this->m_backendAlarmActive = active;
            emit alarmActiveChanged();
        },
        .user_data = nullptr
    });

    // 初始化（从 generated/ 读取配置表）
    m_alarmRuntime->init(g_alarm_rule_table, ALARM_RULE_COUNT);
}
```

## Lambda user_data 模式

当需要跨回调访问成员变量时，使用 lambda 闭包捕获 `this`：

```cpp
// 正确：lambda 捕获 this
[](const char* widget_id, bool on, bool flash, float hz, void*) {
    // 这里可以访问外层 DashboardBackend 的 m_xxx
}

// 错误：在 lambda 内部使用未捕获的 this
// （除非显式捕获）
[this](const char* widget_id, bool on, bool flash, float hz, void*) {
    // 显式捕获 this
}
```

## 回调触发流程

```
AlarmRuntime::tick()
    ↓ 检测到条件满足
    ↓ 调用 m_cb.onIndicatorUpdate("bat_warn_light", true, true, 2.0f, nullptr)
    ↓ DashboardBackend lambda 接收
    ↓ 调用 setIndicator() / emit alarmTextChanged()
    ↓ QML Connections 接收
    ↓ 界面更新
```

## 常见错误

### 1. DashboardBackend 未传入回调 lambda

**症状**：`AlarmRuntime` 的 `tick()` 正常调用，但 QML 报警灯永远不亮。

**根因**：`DashboardBackend` 构造 `AlarmRuntime` 时没有提供回调 lambda，`m_cb` 全为 `nullptr`。

```cpp
// 错误：默认构造 AlarmCallbacks，成员全为 nullptr
m_alarmRuntime = std::make_unique<AlarmRuntime>(AlarmCallbacks{});

// 正确：提供有效的 lambda
m_alarmRuntime = std::make_unique<AlarmRuntime>(AlarmCallbacks{
    .onIndicatorUpdate = lambda,
    // ...
});
```

### 2. AlarmCallbacks 字段遗漏

**症状**：编译报错 `incomplete struct` 或链接报错 `undefined reference`。

```cpp
// 错误：只提供了部分字段（C++ 聚合初始化要求全提供或全不提供）
AlarmCallbacks cb = {
    .onIndicatorUpdate = lambda,
    // .onAlarmTextUpdate 遗漏
    // .onAlarmStateChanged 遗漏
};

// 正确：提供全部字段
AlarmCallbacks cb = {
    .onIndicatorUpdate = lambda,
    .onAlarmTextUpdate = lambda2,
    .onAlarmStateChanged = lambda3,
    .user_data = nullptr
};
```

### 3. lambda 捕获 this 但 user_data 非空

**症状**：程序运行正常但 crash 在回调中（访问了已析构的对象）。

```cpp
// 错误：lambda 捕获了 this，但 user_data 传了非空值
AlarmCallbacks cb = {
    .onIndicatorUpdate = [this](...) { /* uses this */ },
    .user_data = this  // ← 矛盾，lambda 已经捕获了，不需要 user_data
};
```

## 在 QML 层观察报警状态

```qml
// DashboardMain.qml
Connections {
    target: dashboard
    function onAlarmActiveChanged() {
        console.log("alarmActive:", dashboard.alarmActive)
    }
    function onAlarmTextChanged() {
        console.log("alarmText:", dashboard.alarmText)
    }
}
```

## 报警状态 Q_PROPERTY

```cpp
// dashboard_backend_qt.h
Q_PROPERTY(bool alarmActive READ alarmActive NOTIFY alarmActiveChanged)
Q_PROPERTY(QString alarmText READ alarmText NOTIFY alarmTextChanged)

bool alarmActive() const { return m_backendAlarmActive; }
QString alarmText() const { return m_backendAlarmText; }
```

## 调试技巧

在 `AlarmRuntime::tick()` 的条件判断处加临时日志：

```cpp
// alarm_runtime.cpp — tick() 内
if (shouldActivate(now_ms)) {
    qDebug() << "[Alarm] Activating:" << def->name
             << "widget:" << def->actions[0].widget_id;
    if (m_cb.onIndicatorUpdate) {
        m_cb.onIndicatorUpdate(def->actions[0].widget_id, true, true, 2.0f, m_cb.user_data);
    }
}
```

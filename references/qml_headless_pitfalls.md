# QML Headless / VMware 环境崩溃与调试

## 已知 Headless Crash 场景

### QInputMethod::inputDirection() SIGSEGV

**症状**：`DashboardBackend::init() done` 后，在 `QQuickText::componentComplete()` 内段错误。

**根因**：headless/offscreen 环境下 Text 组件完成时调用输入法模块，但 `QGuiApplication` 没有输入法上下文。

**解法**：设置 `QT_IM_MODULE=minimal`：
```bash
export QT_IM_MODULE=minimal
./build/can-dash
```

### QCoreApplication 导致 SIGSEGV

**症状**：程序启动后 `DashboardBackend::init() done` 正常输出，但随后段错误（exit 139）。

**根因**：`QQmlApplicationEngine` 需要 GUI 应用上下文，`QCoreApplication` 不提供。

**解法**：使用 `QGuiApplication`（HEAD 版本已正确使用）：
```cpp
// 错误
#include <QCoreApplication>
QCoreApplication app(argc, argv);

// 正确
#include <QGuiApplication>
QGuiApplication app(argc, argv);
```

## VMware 环境 Qt6 段错误（OpenGL 驱动）

**症状**：在 VMware 虚拟机中运行 Qt6 应用时 `QQmlApplicationEngine` 初始化阶段段错误，但相同二进制在物理机正常。

**根因**：VMware 虚拟显卡的 OpenGL 硬件加速驱动不稳定。

**解法**：强制软件渲染 + 强制 X11 平台：
```bash
export QT_QUICK_BACKEND=software
export QT_OPENGL=software
export QT_QPA_PLATFORM=xcb
./build/can-dash
```

## Ubuntu 22.04 Wayland 会话下 xcb 无窗口

**症状**：在 VMware + Ubuntu 22.04 (Wayland) 环境下，程序日志显示 QML 加载成功，但 `xwininfo -root -tree` 看不到任何 X11 窗口。

**根因**：Ubuntu 22.04 默认使用 Wayland 桌面（gdm-wayland-session）。Qt 的 `xcb` 平台插件依赖 X11/Xorg，在纯 Wayland 会话下窗口由 Wayland compositor 管理，xcb 窗口无法正确嵌入。

**解法**：
1. **方案一（推荐）**：在 GDM 登录界面切换到 Xorg 会话。注销后点击用户名，输入密码前点击右下角齿轮，选择 "Ubuntu on Xorg"。
2. **方案二**：通过 SSH 远程运行（DISPLAY 指向 MobaXterm 等 X 服务器）。
3. **方案三**：安装 Qt Wayland 平台插件（配置复杂，不推荐）。

**诊断命令**：
```bash
# 查看当前会话类型
loginctl show-session 3 | grep Type=

# 查看 Qt 选择了哪个平台插件
QT_DEBUG_PLUGINS=1 ./can-dash 2>&1 | grep "qt.core.plugin.library" | grep platforms

# 查看所有窗口（本地 Xorg :0）
xwininfo -root -display :0 -tree

# 查看 SSH 远程 X 服务器上的窗口
xwininfo -root -display localhost:12 -tree
```

## Headless X11 转发运行

**发现（2026-06-01）**：VMware NAT 环境有 X11 server（`DISPLAY=:10.0`），可以通过 `DISPLAY=:10.0` 从 headless SSH 终端启动 Qt Quick 应用。

```bash
# 查找可用的 X display
xauth list   # 找 MIT-MAGIC-COOKIE
xdpyinfo -display :10.0  # 验证 X server 可用

# 从 headless 启动 can-dash（连接到远程 X server）
export DISPLAY=:10.0
export QT_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins
export QML2_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml
export QT_QUICK_BACKEND=software
export QT_OPENGL=software
export QT_QPA_PLATFORM=xcb
./build/can-dash &
```

**关键**：`xauth` 的 MIT-MAGIC-COOKIE 必须在 `$HOME/.Xauthority` 中存在才能认证。可以直接 `export XAUTHORITY=$HOME/.Xauthority`。

**验证 headless 是否成功**：`xwininfo -root -display :10.0 -tree | grep -i can` 应能看到窗口。

## 已知 QML 报错（headless 启动后）

- `dashboard.tr is not a function` — `tr` 未注册为 Q_INVOKABLE
- `dashboard.indicatorOn is not a function` — 方法不存在
- `lang=undefined` — 语言未设置

这些不影响基础数据显示（车速指针、SOC 条等），但文本翻译功能暂不可用。

## `inputMethodHints` 属性错误

**症状**：`Cannot assign to non-existent property "inputMethodHints"`。

**根因**：`inputMethodHints` 是 `TextInput`（可编辑）的属性，**不是** `Text`（只读）的属性。在 Text 组件上加 `inputMethodHints` 会报错。

**解法**：不要在 `Text` 组件上加 `inputMethodHints`。

## QML Text style 属性错误

**症状**：`Unable to assign undefined to QQuickText::TextStyle`。

**根因**：Qt6 QML 的 `Text` 类型没有 `style` 和 `styleColor` 属性。

**解法**：
```qml
// 错误
Text {
    style: Text.Normal
    styleColor: "#00FF88"
}

// 正确
Text {
    color: "#00FF88"
    font.weight: Font.Bold
}
```

## binary 0 字节问题

**症状**：`cmake --build` 报 "[100%] Built target can-dash" 但 binary 大小为 0。

**根因**：cmake 认为 target 已构建但实际跳过了 link。

**解法**：`rm -rf build && cmake -B build` 强制重建。

## 验证 headless 成功

```bash
# 确认进程在跑且占用 CPU
ps aux | grep can-dash

# 确认 X11 窗口存在
xwininfo -root -display :10.0 -tree | grep -i can
```

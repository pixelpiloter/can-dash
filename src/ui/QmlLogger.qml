// QmlLogger.qml - 日志单例组件
// 功能：QML端实时日志，支持ring buffer、级别过滤、C++端qInstallMessageHandler桥接
// 路径: qml/QmlLogger.qml
import QtQuick 2.15

Item {
    id: root

    // ─── 启动时向 C++ 桥接注册 ───
    Component.onCompleted: {
        if (typeof qmlLoggerBridge !== "undefined") {
            qmlLoggerBridge.registerQmlLogger(root)
        }
    }

    // ─── 环形缓冲区 (30条) ───
    property int bufferSize: 30
    property int headIndex: 0
    property int count: 0

    // 日志存储：ListModel 用于 ListView 绑定
    ListModel {
        id: logModel
    }

    // ─── 级别定义 ───
    readonly property string lvlDebug: "DEBUG"
    readonly property string lvlInfo:  "INFO"
    readonly property string lvlWarn:  "WARN"
    readonly property string lvlError: "ERROR"

    // ─── 颜色映射 ───
    function levelColor(level) {
        switch (level) {
            case lvlDebug: return "#888888"
            case lvlInfo:  return "#FFFFFF"
            case lvlWarn:  return "#FFAA00"
            case lvlError: return "#FF4400"
            default:       return "#CCCCCC"
        }
    }

    // ─── 内部添加一条日志 ───
    function addEntry(level, source, message) {
        var now = new Date()
        var ts = Qt.formatDateTime(now, "HH:mm:ss.zzz")

        // ring buffer: 替换最旧的条目
        if (count < bufferSize) {
            logModel.append({ ts: ts, level: level, source: source, message: message })
            count = count + 1
        } else {
            var idx = headIndex % bufferSize
            logModel.set(idx, { ts: ts, level: level, source: source, message: message })
            headIndex = (headIndex + 1) % bufferSize
        }
    }

    // ─── Q_INVOKABLE API ───
    function info(source, message) {
        addEntry(lvlInfo, source, message)
    }

    function warn(source, message) {
        addEntry(lvlWarn, source, message)
    }

    function error(source, message) {
        addEntry(lvlError, source, message)
    }

    function debug(source, message) {
        addEntry(lvlDebug, source, message)
    }

    // 清除所有日志
    function clearLogs() {
        logModel.clear()
        headIndex = 0
        count = 0
    }

    // 返回最近N条日志 (供外部读取)
    function getLogs() {
        var logs = []
        var start = (count < bufferSize) ? 0 : headIndex
        for (var i = 0; i < count; i++) {
            var idx = (start + i) % bufferSize
            if (idx < logModel.count) {
                var item = logModel.get(idx)
                logs.push({ ts: item.ts, level: item.level, source: item.source, message: item.message })
            }
        }
        return logs
    }

    // ─── 暴露给C++的静态桥接方法 ─────────────────────────────────────────────
    // C++ qInstallMessageHandler 调用 QmlLogger.addLog(level, source, message)
    // 该方法由 DashboardBackend 通过 QML 上下文暴露
    function _cppAddLog(level, source, message) {
        addEntry(level, source, message)
    }
}

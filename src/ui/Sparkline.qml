// Sparkline.qml - 轻量级历史曲线（QML Canvas 60s 滑动窗口）
//
// 用法：
//   Sparkline {
//       width: 280; height: 90
//       sourceValue: dashboard.displayData["vehicle_speed"]
//       minValue: 0; maxValue: 260
//       lineColor: "#00FF88"
//       fillColor: "#00332255"
//       title: "速度 60s"
//   }
//
// 数据采样：内部 200ms 采样一次 → 60s = 300 个点
// 渲染：每 200ms 触发一次 requestPaint()
import QtQuick 2.15

Item {
    id: root
    width: 280
    height: 90

    // ─── 输入参数 ───
    property real sourceValue: 0
    property real minValue: 0
    property real maxValue: 100
    property color lineColor: "#00FF88"
    property color fillColor: "#2200FF88"
    property color gridColor: "#22FFFFFF"
    property string title: ""
    property string unit: ""
    property int sampleIntervalMs: 200     // 200ms 采样 → 60s 窗口 300 点
    property int windowSeconds: 60

    // ─── 内部状态 ───
    property var samples: []               // 环形缓冲 [{t, v}, ...]
    property real currentValue: 0

    // 限长（保留 windowSeconds/sampleIntervalMs*1000 个点）
    property int maxSamples: Math.ceil(windowSeconds * 1000 / sampleIntervalMs)

    // ─── 采样定时器 ───
    Timer {
        id: sampleTimer
        interval: root.sampleIntervalMs
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            root.currentValue = root.sourceValue
            var s = root.samples.slice()
            s.push(root.sourceValue)
            while (s.length > root.maxSamples) s.shift()
            root.samples = s
        }
    }

    // ─── 画布 ───
    Canvas {
        id: sparklineCanvas
        anchors.fill: parent
        antialiasing: true
        renderTarget: Canvas.FramebufferObject

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var w = width
            var h = height
            var padTop = 4
            var padBottom = 18   // 留出标题/单位
            var padLeft = 4
            var padRight = 4
            var plotW = w - padLeft - padRight
            var plotH = h - padTop - padBottom

            // 网格线（横线 4 等分）
            ctx.strokeStyle = root.gridColor
            ctx.lineWidth = 1
            for (var i = 1; i < 4; i++) {
                var y = padTop + plotH * i / 4
                ctx.beginPath()
                ctx.moveTo(padLeft, y)
                ctx.lineTo(padLeft + plotW, y)
                ctx.stroke()
            }

            var s = root.samples
            if (!s || s.length < 2) return

            // 数值归一化
            var range = root.maxValue - root.minValue
            if (range <= 0) range = 1
            function norm(v) {
                var n = (v - root.minValue) / range
                if (n < 0) n = 0
                if (n > 1) n = 1
                return n
            }

            var n = s.length
            var dx = plotW / (root.maxSamples - 1)

            // 填充区域
            ctx.beginPath()
            ctx.moveTo(padLeft, padTop + plotH)
            for (var k = 0; k < n; k++) {
                var px = padLeft + k * dx
                var py = padTop + plotH * (1 - norm(s[k]))
                ctx.lineTo(px, py)
            }
            ctx.lineTo(padLeft + (n - 1) * dx, padTop + plotH)
            ctx.closePath()
            ctx.fillStyle = root.fillColor
            ctx.fill()

            // 折线
            ctx.beginPath()
            for (var j = 0; j < n; j++) {
                var x2 = padLeft + j * dx
                var y2 = padTop + plotH * (1 - norm(s[j]))
                if (j === 0) ctx.moveTo(x2, y2)
                else ctx.lineTo(x2, y2)
            }
            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = 1.6
            ctx.lineJoin = "round"
            ctx.stroke()

            // 当前值圆点
            var lastY = padTop + plotH * (1 - norm(s[n - 1]))
            var lastX = padLeft + (n - 1) * dx
            ctx.beginPath()
            ctx.arc(lastX, lastY, 2.5, 0, Math.PI * 2)
            ctx.fillStyle = root.lineColor
            ctx.fill()
        }
    }

    // ─── 标题 + 当前值 ───
    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.bottomMargin: 2
        text: root.title
        color: "#888888"
        font.pixelSize: 11
        font.family: dashboard ? dashboard.currentFont : "sans-serif"
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 4
        anchors.bottomMargin: 2
        text: root.currentValue.toFixed(0) + (root.unit ? " " + root.unit : "")
        color: root.lineColor
        font.pixelSize: 12
        font.weight: Font.Bold
        font.family: "Roboto Mono, monospace"
    }

    // 触发重绘（独立于采样）
    Timer {
        id: renderTimer
        interval: root.sampleIntervalMs
        running: true
        repeat: true
        onTriggered: sparklineCanvas.requestPaint()
    }
}

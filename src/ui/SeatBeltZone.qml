// SeatBeltZone.qml - 安全带监控（REQ-SYS-004）
// 5座位安全带状态显示，支持 debounce 和 AlarmBanner 集成
import QtQuick 2.15

Item {
    id: root
    width: 420
    height: 90

    // ─── Canvas 绘制5个座位图标 ───
    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        // 闪烁相位（0=亮, 1=暗），由 Timer 驱动
        property real flashPhase: 0.0
        property bool isFlashing: false

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var seatW = Math.min(w / 5 - 8, 68)
            var seatH = Math.min(h - 20, 62)
            var gap = 8
            var totalW = 5 * seatW + 4 * gap
            var startX = (w - totalW) / 2

            ctx.clearRect(0, 0, w, h)

            var lang = dashboard.currentLanguage || "zh_CN"
            var isZh = (lang === "zh_CN")

            for (var i = 0; i < 5; i++) {
                var seat = dashboard.seatIconStates[i] || {}
                var def = seatDefs[i]
                var x = startX + i * (seatW + gap)
                var y = 2

                var occupied = !!seat.occupied
                var buckled = !!seat.buckled
                var warning = !!seat.warning

                // 背景色（闪烁时 alpha 变化）
                var alpha = (isFlashing && warning) ? (0.3 + 0.7 * flashPhase) : 1.0
                var bgColor
                if (!occupied) {
                    bgColor = "rgba(51,51,51," + alpha + ")"
                } else if (warning) {
                    bgColor = "rgba(170,17,0," + alpha + ")"
                } else if (buckled) {
                    bgColor = "rgba(0,102,34," + alpha + ")"
                } else {
                    bgColor = "rgba(51,51,51," + alpha + ")"
                }

                // 圆角矩形背景
                _drawRoundRectPath(ctx, x, y, seatW, seatH, 6)
                ctx.fillStyle = bgColor
                ctx.fill()

                // 边框
                _drawRoundRectPath(ctx, x, y, seatW, seatH, 6)
                ctx.strokeStyle = warning ? "rgba(255,68,0," + alpha + ")" : "rgba(60,60,60," + alpha + ")"
                ctx.lineWidth = warning ? 2 : 1
                ctx.stroke()

                // Buckle 状态图标（✓ / ! / —）
                var iconText = !occupied ? "—" : (buckled ? "✓" : "!")
                var iconColor = !occupied ? "rgba(85,85,85," + alpha + ")" : (buckled ? "rgba(0,255,136," + alpha + ")" : "rgba(255,68,0," + alpha + ")")
                ctx.font = "bold " + Math.round(seatH * 0.38) + "px sans-serif"
                ctx.fillStyle = iconColor
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText(iconText, x + seatW / 2, y + seatH * 0.48)

                // 座位标签
                var label = isZh ? def.labelZh : def.labelEn
                ctx.font = "bold 9px sans-serif"
                ctx.fillStyle = "rgba(136,136,136," + alpha + ")"
                ctx.textAlign = "center"
                ctx.textBaseline = "bottom"
                ctx.fillText(label, x + seatW / 2, y + seatH + 14)
            }
        }

        function _drawRoundRectPath(ctx, x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.lineTo(x + w - r, y)
            ctx.quadraticCurveTo(x + w, y, x + w, y + r)
            ctx.lineTo(x + w, y + h - r)
            ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
            ctx.lineTo(x + r, y + h)
            ctx.quadraticCurveTo(x, y + h, x, y + h - r)
            ctx.lineTo(x, y + r)
            ctx.quadraticCurveTo(x, y, x + r, y)
            ctx.closePath()
        }
    }

    // ─── 2Hz 闪烁 Timer（驱动 flashPhase）───
    // 2Hz = 500ms 周期，250ms 半周期
    Timer {
        id: flashTimer
        interval: 250
        running: _hasWarning && dashboard.isMoving
        repeat: true
        onTriggered: {
            canvas.flashPhase = (canvas.flashPhase === 0.0) ? 1.0 : 0.0
            canvas.isFlashing = true
            canvas.requestPaint()
        }
    }

    // 监听 seatIconStates 变化 → 重绘
    Connections {
        target: dashboard
        function onSeatBeltWarningChanged() {
            _updateWarningState()
            canvas.requestPaint()
            _updateIndicator()
        }
    }

    // ─── 监听 isMoving 变化（影响闪烁条件）───
    Connections {
        target: dashboard
        function onMovingChanged() {
            if (!dashboard.isMoving) {
                canvas.flashPhase = 0.0
                canvas.isFlashing = false
            }
            canvas.requestPaint()
        }
    }

    // ─── 当前是否有 warning ───
    property bool _hasWarning: false

    function _updateWarningState() {
        var states = dashboard.seatIconStates
        _hasWarning = states.some(function(s) { return !!s.warning })
    }

    // ─── 触发 AlarmBanner（通过 dashboard 指示灯系统）───
    function _updateIndicator() {
        dashboard.setIndicator("seatbelt_warning", _hasWarning)
    }

    // ─── 初始化时读取一次 ───
    Component.onCompleted: {
        _updateWarningState()
        _updateIndicator()
        canvas.requestPaint()
    }

    // ─── 座位定义 ───
    readonly property var seatDefs: [
        { id: "driver",      labelZh: "主驾", labelEn: "DRIVER" },
        { id: "passenger",   labelZh: "副驾", labelEn: "PASSENGER" },
        { id: "rear_left",   labelZh: "后左", labelEn: "REAR L" },
        { id: "rear_center", labelZh: "后中", labelEn: "REAR C" },
        { id: "rear_right",  labelZh: "后右", labelEn: "REAR R" }
    ]
}

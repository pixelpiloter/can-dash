// SpeedGauge.qml - 车速表专用组件
//   范围: 0~260 km/h
//   弧度: 240° (起始 210°, 终止 330°)
//   红色警戒区: 220 km/h 起
//   指针颜色: 绿色(正常) / 黄色(≥180) / 红色(≥220)
//   超时显示: vehicle_speed === 0 持续 >1s → 显示 "---"，指针归零
import QtQuick 2.15

Item {
    id: root
    property real value: 0          // 外部绑定：dashboard displayData["vehicle_speed"]
    property real minValue: 0
    property real maxValue: 260
    property string unit: "km/h"
    property int majorTickCount: 13
    property int minorTicksPerMajor: 5
    property real startAngleDeg: 210
    property real endAngleDeg: 330
    property string dialColor: "#1a2a1a"
    property string needleColorNormal:  "#00FF88"
    property string needleColorWarning: "#FFAA00"
    property string needleColorDanger:  "#FF2200"
    property string labelColor: "#88FF88"
    property real warningValue: 180
    property real dangerValue:  220

    width: 580
    height: 580

    // ─── 超时检测：value === 0 持续 >1s ───
    property int _zeroCount: 0
    property bool _timedOut: false
    property real _displayValue: 0

    onValueChanged: {
        if (value === 0 || value < 0.5) {
            // 保持 _displayValue 为 0（指针归零）
        } else {
            _displayValue = value
        }
    }

    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: {
            if (value === 0 || value < 0.5) {
                _zeroCount++
            } else {
                _zeroCount = 0
                _timedOut = false
            }
            if (_zeroCount > 10 && !_timedOut) {
                _timedOut = true
            }
        }
    }

    // ─── 指针颜色（动态）───
    function computeNeedleColor(v) {
        if (v >= dangerValue)  return needleColorDanger
        if (v >= warningValue) return needleColorWarning
        return needleColorNormal
    }
    property string currentNeedleColor: computeNeedleColor(_timedOut ? 0 : value)

    // ─── 底层：表盘背景 + 刻度 + 红色警戒区 ───
    Canvas {
        id: bgCanvas
        anchors.fill: parent
        antialiasing: true
        smooth: true
        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            var outerR = Math.min(cx, cy) - 4

            ctx.clearRect(0, 0, w, h)

            // 外环发光
            var glow = ctx.createRadialGradient(cx, cy, outerR - 10, cx, cy, outerR + 15)
            glow.addColorStop(0, needleColorNormal + "22")
            glow.addColorStop(1, "transparent")
            ctx.beginPath()
            ctx.arc(cx, cy, outerR + 15, 0, 2 * Math.PI)
            ctx.fillStyle = glow
            ctx.fill()

            // 表盘外圈
            ctx.beginPath()
            ctx.arc(cx, cy, outerR, 0, 2 * Math.PI)
            ctx.fillStyle = "#0d0d0d"
            ctx.fill()

            // 表盘主体
            var bgGrad = ctx.createRadialGradient(cx, cy - outerR * 0.3, outerR * 0.1, cx, cy, outerR)
            bgGrad.addColorStop(0, dialColor)
            bgGrad.addColorStop(0.7, "#0a0a0a")
            bgGrad.addColorStop(1, "#050505")
            ctx.beginPath()
            ctx.arc(cx, cy, outerR - 2, 0, 2 * Math.PI)
            ctx.fillStyle = bgGrad
            ctx.fill()

            // 内圈
            var innerBgGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, outerR * 0.72)
            innerBgGrad.addColorStop(0, "#111111")
            innerBgGrad.addColorStop(1, "#0a0a0a")
            ctx.beginPath()
            ctx.arc(cx, cy, outerR * 0.72, 0, 2 * Math.PI)
            ctx.fillStyle = innerBgGrad
            ctx.fill()

            // 刻度线 + 数字
            var startAngle = startAngleDeg * Math.PI / 180
            var endAngle = endAngleDeg * Math.PI / 180
            var angleRange = endAngle - startAngle
            var totalMinorTicks = majorTickCount * minorTicksPerMajor

            for (var i = 0; i <= totalMinorTicks; i++) {
                var t = i / totalMinorTicks
                var angle = startAngle + t * angleRange
                var isMajor = (i % minorTicksPerMajor === 0)
                var tickOuter = outerR - 6
                var tickInner = tickOuter - (isMajor ? 18 : 10)
                var tickW = isMajor ? 2.0 : 1.0

                var cos = Math.cos(angle)
                var sin = Math.sin(angle)

                ctx.beginPath()
                ctx.moveTo(cx + tickInner * cos, cy + tickInner * sin)
                ctx.lineTo(cx + tickOuter * cos, cy + tickOuter * sin)
                ctx.strokeStyle = isMajor ? "#FFFFFF" : "#555555"
                ctx.lineWidth = tickW
                ctx.lineCap = "round"
                ctx.stroke()

                if (isMajor) {
                    var majorIndex = i / minorTicksPerMajor
                    var labelValue = minValue + (majorIndex / (majorTickCount)) * (maxValue - minValue)
                    var labelR = tickInner - 14
                    var lx = cx + labelR * cos
                    var ly = cy + labelR * sin + 4

                    ctx.font = "bold " + Math.round(outerR * 0.07) + "px Roboto Mono, monospace"
                    ctx.fillStyle = labelColor
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(labelValue.toFixed(0), lx, ly)
                }
            }

            // ─── 红色警戒区弧形 (220 km/h 起) ───
            var redZoneStart = 220
            if (redZoneStart > minValue && redZoneStart < maxValue) {
                var tStart = (redZoneStart - minValue) / (maxValue - minValue)
                var arcStart = startAngleDeg + tStart * angleRange
                var arcEnd   = endAngleDeg

                var arcR = outerR - 30
                ctx.beginPath()
                ctx.arc(cx, cy, arcR, arcStart * Math.PI / 180, arcEnd * Math.PI / 180)
                ctx.strokeStyle = "#FF220088"
                ctx.lineWidth = 6
                ctx.lineCap = "round"
                ctx.stroke()

                // 填充半透明红色扇形
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.arc(cx, cy, arcR + 8, arcStart * Math.PI / 180, arcEnd * Math.PI / 180)
                ctx.closePath()
                var redFill = ctx.createRadialGradient(cx, cy, arcR - 5, cx, cy, arcR + 10)
                redFill.addColorStop(0, "#FF220015")
                redFill.addColorStop(1, "#FF220000")
                ctx.fillStyle = redFill
                ctx.fill()
            }

            // 中心圆帽
            ctx.beginPath()
            ctx.arc(cx, cy, outerR * 0.055, 0, 2 * Math.PI)
            var capGrad = ctx.createRadialGradient(cx - 2, cy - 2, 0, cx, cy, outerR * 0.055)
            capGrad.addColorStop(0, "#444444")
            capGrad.addColorStop(1, "#111111")
            ctx.fillStyle = capGrad
            ctx.fill()

            // 中心点
            ctx.beginPath()
            ctx.arc(cx, cy, outerR * 0.022, 0, 2 * Math.PI)
            ctx.fillStyle = needleColorNormal
            ctx.fill()
        }
    }

    // ─── 中层：指针 ───
    Canvas {
        id: needleCanvas
        anchors.fill: parent
        antialiasing: true
        smooth: true

        Connections {
            target: root
            function onValueChanged() { needleCanvas.requestPaint() }
        }

        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            var outerR = Math.min(cx, cy) - 4
            var needleLen = outerR - 35

            ctx.clearRect(0, 0, w, h)

            var v = _timedOut ? 0 : value
            var t = Math.max(0, Math.min(1, (v - minValue) / (maxValue - minValue)))
            var needleAngle = startAngleDeg + t * (endAngleDeg - startAngleDeg)
            var rad = needleAngle * Math.PI / 180

            var needleCol = currentNeedleColor

            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(rad)
            ctx.shadowColor = needleCol
            ctx.shadowBlur = 10

            ctx.beginPath()
            ctx.moveTo(needleLen, 0)
            ctx.lineTo(-8, -5)
            ctx.lineTo(-8,  5)
            ctx.closePath()

            var needleGrad = ctx.createLinearGradient(0, 0, -8, 0)
            needleGrad.addColorStop(0, needleCol)
            needleGrad.addColorStop(1, needleCol + "aa")
            ctx.fillStyle = needleGrad
            ctx.fill()
            ctx.restore()
        }
    }

    // ─── 顶层：数值 + 单位 ───
    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.width * 0.16
        Text {
            id: speedValueText
            anchors.horizontalCenter: parent.horizontalCenter
            text: _timedOut ? "---" : Math.round(_timedOut ? 0 : value).toString()
            color: {
                if (_timedOut) return "#666666"
                if (value >= 220) return "#FF2200"
                if (value >= 180) return "#FFAA00"
                return "#FFFFFF"
            }
            font.family: "Roboto Mono, monospace"
            font.pixelSize: Math.round(root.width * 0.20)
            font.weight: Font.Bold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: unit
            color: "#888888"
            font.family: "Roboto Mono, monospace"
            font.pixelSize: Math.round(root.width * 0.08)
            font.weight: Font.Bold
        }
    }

    onWidthChanged:  bgCanvas.requestPaint()
    onHeightChanged: bgCanvas.requestPaint()
}

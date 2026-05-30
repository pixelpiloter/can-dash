// GaugeCanvas.qml - 高质量汽车仪表盘 Canvas 组件
import QtQuick 2.15

Item {
    id: root
    property real value: 0
    property real minValue: 0
    property real maxValue: 260
    property string unit: "km/h"
    property int majorTickCount: 13
    property int minorTicksPerMajor: 5
    property real startAngleDeg: 135
    property real endAngleDeg: 405
    property string dialColor: "#1a2a1a"
    property string needleColor: "#00FF88"
    property string labelColor: "#88FF88"

    width: 320
    height: 320

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        smooth: true

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            var outerR = Math.min(cx, cy) - 4
            var innerR = outerR - 35

            ctx.clearRect(0, 0, w, h)

            // 1. 外环发光
            var glow = ctx.createRadialGradient(cx, cy, outerR - 10, cx, cy, outerR + 20)
            glow.addColorStop(0, needleColor + "33")
            glow.addColorStop(1, "transparent")
            ctx.beginPath()
            ctx.arc(cx, cy, outerR + 20, 0, 2 * Math.PI)
            ctx.fillStyle = glow
            ctx.fill()

            // 2. 表盘外圈（深色）
            ctx.beginPath()
            ctx.arc(cx, cy, outerR, 0, 2 * Math.PI)
            ctx.fillStyle = "#0d0d0d"
            ctx.fill()

            // 3. 表盘主体（径向渐变）
            var bgGrad = ctx.createRadialGradient(cx, cy - outerR * 0.3, outerR * 0.1, cx, cy, outerR)
            bgGrad.addColorStop(0, dialColor)
            bgGrad.addColorStop(0.7, "#0a0a0a")
            bgGrad.addColorStop(1, "#050505")
            ctx.beginPath()
            ctx.arc(cx, cy, outerR - 2, 0, 2 * Math.PI)
            ctx.fillStyle = bgGrad
            ctx.fill()

            // 4. 内圈（指针区域背景）
            var innerBgGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, outerR * 0.75)
            innerBgGrad.addColorStop(0, "#111111")
            innerBgGrad.addColorStop(1, "#0a0a0a")
            ctx.beginPath()
            ctx.arc(cx, cy, outerR * 0.75, 0, 2 * Math.PI)
            ctx.fillStyle = innerBgGrad
            ctx.fill()

            // 5. 刻度线
            var startAngle = startAngleDeg * Math.PI / 180
            var endAngle = endAngleDeg * Math.PI / 180
            var angleRange = endAngle - startAngle
            var totalMinorTicks = majorTickCount * minorTicksPerMajor

            for (var i = 0; i <= totalMinorTicks; i++) {
                var t = i / totalMinorTicks
                var angle = startAngle + t * angleRange
                var isMajor = (i % minorTicksPerMajor === 0)
                var tickOuter = outerR - 6
                var tickInner = tickOuter - (isMajor ? 22 : 12)
                var tickW = isMajor ? 2.5 : 1.0

                var cos = Math.cos(angle)
                var sin = Math.sin(angle)

                ctx.beginPath()
                ctx.moveTo(cx + tickInner * cos, cy + tickInner * sin)
                ctx.lineTo(cx + tickOuter * cos, cy + tickOuter * sin)
                ctx.strokeStyle = isMajor ? "#FFFFFF" : "#555555"
                ctx.lineWidth = tickW
                ctx.lineCap = "round"
                ctx.stroke()

                // 大刻度数字
                if (isMajor) {
                    var majorIndex = i / minorTicksPerMajor
                    var labelValue = minValue + (majorIndex / (majorTickCount)) * (maxValue - minValue)
                    var labelR = tickInner - 18
                    var lx = cx + labelR * cos
                    var ly = cy + labelR * sin + 5

                    ctx.font = "bold " + Math.round(outerR * 0.075) + "px Roboto Mono, monospace"
                    ctx.fillStyle = labelColor
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(labelValue.toFixed(0), lx, ly)
                }
            }

            // 6. 指针
            var ratio = Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))
            var needleAngle = startAngle + ratio * angleRange
            var needleLen = outerR - 40

            // 指针发光
            ctx.save()
            ctx.shadowColor = needleColor
            ctx.shadowBlur = 12

            // 指针主体（细长三角形）
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.lineTo(
                cx + needleLen * Math.cos(needleAngle - Math.PI / 2),
                cy + needleLen * Math.sin(needleAngle - Math.PI / 2)
            )
            ctx.lineTo(
                cx + needleLen * Math.cos(needleAngle),
                cy + needleLen * Math.sin(needleAngle)
            )
            ctx.lineTo(
                cx + needleLen * Math.cos(needleAngle + Math.PI / 2),
                cy + needleLen * Math.sin(needleAngle + Math.PI / 2)
            )
            ctx.closePath()
            var needleGrad = ctx.createLinearGradient(
                cx, cy - needleLen, cx, cy
            )
            needleGrad.addColorStop(0, needleColor)
            needleGrad.addColorStop(1, needleColor + "88")
            ctx.fillStyle = needleGrad
            ctx.fill()
            ctx.restore()

            // 7. 中心圆帽
            ctx.beginPath()
            ctx.arc(cx, cy, outerR * 0.06, 0, 2 * Math.PI)
            var capGrad = ctx.createRadialGradient(cx - 3, cy - 3, 0, cx, cy, outerR * 0.06)
            capGrad.addColorStop(0, "#444444")
            capGrad.addColorStop(1, "#111111")
            ctx.fillStyle = capGrad
            ctx.fill()

            // 中心点
            ctx.beginPath()
            ctx.arc(cx, cy, outerR * 0.025, 0, 2 * Math.PI)
            ctx.fillStyle = needleColor
            ctx.fill()

            // 8. 底部数值显示
            var valText = value.toFixed(0)
            ctx.font = "bold " + Math.round(outerR * 0.22) + "px Roboto Mono, monospace"
            ctx.fillStyle = "#FFFFFF"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.shadowColor = needleColor
            ctx.shadowBlur = 8
            ctx.fillText(valText, cx, cy + outerR * 0.38)
            ctx.shadowBlur = 0

            // 9. 单位文字
            ctx.font = "bold " + Math.round(outerR * 0.09) + "px Roboto Mono, monospace"
            ctx.fillStyle = "#888888"
            ctx.fillText(unit, cx, cy + outerR * 0.56)
        }
    }

    onValueChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}

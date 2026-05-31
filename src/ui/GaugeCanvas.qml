// GaugeCanvas.qml - 汽车仪表盘，两层结构：
//   底层 Canvas：画表盘背景+刻度（静态，只画一次）
//   中层 Canvas：画指针（value 变化时重绘）
//   顶层 QML Text：显示数值+单位（value 变化时更新）
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

    width: 280
    height: 280

    // ============================================================
    // 底层：表盘背景 + 刻度（静态，只画一次）
    // ============================================================
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
            glow.addColorStop(0, needleColor + "22")
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
            ctx.fillStyle = needleColor
            ctx.fill()
        }
    }

    // ============================================================
    // 中层：指针（独立 Canvas，value 变化时重绘）
    // ============================================================
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

            var t = Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))
            var needleAngle = startAngleDeg + t * (endAngleDeg - startAngleDeg)
            var rad = needleAngle * Math.PI / 180

            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(rad)
            ctx.shadowColor = needleColor
            ctx.shadowBlur = 10

            // 指针三角形：顶点朝右（0°），rotate后指向刻度
            ctx.beginPath()
            ctx.moveTo(needleLen, 0)
            ctx.lineTo(-8, -5)
            ctx.lineTo(-8,  5)
            ctx.closePath()

            var needleGrad = ctx.createLinearGradient(0, 0, -8, 0)
            needleGrad.addColorStop(0, needleColor)
            needleGrad.addColorStop(1, needleColor + "aa")
            ctx.fillStyle = needleGrad
            ctx.fill()
            ctx.restore()
        }
    }

    // ============================================================
    // 顶层：数值 + 单位（独立 Text，value 变化时更新）
    // ============================================================
    // 指针底部在 cy+5，文字放在 cy+outerR*0.52 以下，避让指针
    Column {
        id: valueDisplay
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.width * 0.16
        Text {
            id: valueText
            anchors.horizontalCenter: parent.horizontalCenter
            text: value.toFixed(0)
            color: "#FFFFFF"
            font.family: "Roboto Mono, monospace"
            font.pixelSize: Math.round(root.width * 0.20)
            font.weight: Font.Bold
        }
        Text {
            id: unitText
            anchors.horizontalCenter: parent.horizontalCenter
            text: unit
            color: "#888888"
            font.family: "Roboto Mono, monospace"
            font.pixelSize: Math.round(root.width * 0.08)
            font.weight: Font.Bold
        }
    }

    onWidthChanged: bgCanvas.requestPaint()
    onHeightChanged: bgCanvas.requestPaint()
}

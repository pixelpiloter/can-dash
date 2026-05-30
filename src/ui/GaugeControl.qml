// GaugeControl.qml - 圆形表盘组件
import QtQuick 2.15

Item {
    id: root
    property float value: 0
    property real minValue: 0
    property real maxValue: 260
    property string unit: "km/h"
    property int majorTicks: 13

    width: 320
    height: 320

    // 表盘背景
    Canvas {
        id: gaugeCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var cx = width / 2
            var cy = height / 2
            var radius = Math.min(cx, cy) - 10

            ctx.clearRect(0, 0, width, height)

            // 外圈
            ctx.beginPath()
            ctx.arc(cx, cy, radius, 0, 2 * Math.PI)
            ctx.strokeStyle = "#333333"
            ctx.lineWidth = 3
            ctx.stroke()

            // 刻度线
            var startAngle = 135 * Math.PI / 180
            var endAngle = 405 * Math.PI / 180
            var angleRange = endAngle - startAngle
            var tickCount = majorTicks
            for (var i = 0; i <= tickCount; i++) {
                var angle = startAngle + (i / tickCount) * angleRange
                var innerR = radius - (i % 5 === 0 ? 15 : 8)
                var outerR = radius - 3
                ctx.beginPath()
                ctx.moveTo(cx + innerR * Math.cos(angle), cy + innerR * Math.sin(angle))
                ctx.lineTo(cx + outerR * Math.cos(angle), cy + outerR * Math.sin(angle))
                ctx.strokeStyle = i % 5 === 0 ? "#FFFFFF" : "#666666"
                ctx.lineWidth = i % 5 === 0 ? 2 : 1
                ctx.stroke()
            }

            // 指针
            var ratio = Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))
            var needleAngle = startAngle + ratio * angleRange
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.lineTo(cx + (radius - 30) * Math.cos(needleAngle),
                       cy + (radius - 30) * Math.sin(needleAngle))
            ctx.strokeStyle = "#FF4400"
            ctx.lineWidth = 3
            ctx.stroke()

            // 中心圆
            ctx.beginPath()
            ctx.arc(cx, cy, 8, 0, 2 * Math.PI)
            ctx.fillStyle = "#FF4400"
            ctx.fill()
        }
    }

    // 当前数值
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        text: value.toFixed(0)
        color: "#FFFFFF"
        font.pixelSize: 48
        font.weight: Font.Bold
    }

    // 单位
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        text: unit
        color: "#888888"
        font.pixelSize: 16
    }

    onValueChanged: gaugeCanvas.requestPaint()
}

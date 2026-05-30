// WarningLight.qml - Canvas 绘制汽车警告灯
import QtQuick 2.15

Item {
    id: root
    property bool active: false
    property bool flash: false
    property string warnType: "bat"  // "bat" or "seatbelt"
    property real flashHz: 2.0

    width: 50
    height: 50

    Timer {
        id: flashTimer
        interval: flash ? (1000 / (flashHz * 2)) : 100
        running: root.flash && root.active
        repeat: true
        onTriggered: flashAnim.running = !flashAnim.running
    }

    SequentialAnimation {
        id: flashAnim
        running: false
        NumberAnimation { target: glowRect; property: "opacity"; from: 1.0; to: 0.1; duration: 250 }
        NumberAnimation { target: glowRect; property: "opacity"; from: 0.1; to: 1.0; duration: 250 }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var cx = width / 2
            var cy = height / 2
            var r = Math.min(cx, cy) - 2

            ctx.clearRect(0, 0, width, height)

            var isBat = warnType === "bat"
            var onColor = isBat ? [255, 55, 0] : [255, 165, 0]   // red / amber
            var offColor = isBat ? [60, 25, 0] : [65, 45, 0]

            var col = active ? onColor : offColor
            var alpha = active ? 255 : 180

            // 外圈发光
            if (active) {
                var glow = ctx.createRadialGradient(cx, cy, r * 0.7, cx, cy, r * 1.2)
                glow.addColorStop(0, "rgba(" + col[0] + "," + col[1] + "," + col[2] + ",0.35)")
                glow.addColorStop(1, "rgba(0,0,0,0)")
                ctx.beginPath()
                ctx.arc(cx, cy, r * 1.2, 0, 2 * Math.PI)
                ctx.fillStyle = glow
                ctx.fill()
            }

            // 金属边框
            var metalGrad = ctx.createLinearGradient(cx - r, cy - r, cx + r, cy + r)
            metalGrad.addColorStop(0, "#555555")
            metalGrad.addColorStop(0.3, "#333333")
            metalGrad.addColorStop(0.7, "#1a1a1a")
            metalGrad.addColorStop(1, "#444444")
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, 2 * Math.PI)
            ctx.fillStyle = metalGrad
            ctx.fill()

            // 镜头背景
            var lensGrad = ctx.createRadialGradient(cx - r*0.2, cy - r*0.2, 0, cx, cy, r * 0.85)
            if (active) {
                lensGrad.addColorStop(0, "rgba(" + Math.min(255,col[0]+80) + "," + Math.min(255,col[1]+80) + "," + Math.min(255,col[2]+80) + ",255)")
                lensGrad.addColorStop(0.6, "rgba(" + col[0] + "," + col[1] + "," + col[2] + ",255)")
                lensGrad.addColorStop(1, "rgba(" + Math.max(0,col[0]-40) + "," + Math.max(0,col[1]-40) + "," + Math.max(0,col[2]-40) + ",255)")
            } else {
                lensGrad.addColorStop(0, "#3a3a3a")
                lensGrad.addColorStop(1, "#1a1a1a")
            }
            ctx.beginPath()
            ctx.arc(cx, cy, r * 0.85, 0, 2 * Math.PI)
            ctx.fillStyle = lensGrad
            ctx.fill()

            // 画符号
            ctx.font = "bold " + Math.round(r * 0.8) + "px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            if (active) {
                ctx.shadowColor = "rgb(" + col[0] + "," + col[1] + "," + col[2] + ")"
                ctx.shadowBlur = 8
            }
            ctx.fillStyle = active ? "#FFFFFF" : "#666666"
            if (isBat) {
                // 电池符号：红色! 感叹号
                ctx.fillText("!", cx, cy + 2)
            } else {
                // 安全带：感叹号
                ctx.fillText("!", cx, cy + 2)
            }
            ctx.shadowBlur = 0
        }
    }

    Rectangle {
        id: glowRect
        anchors.centerIn: parent
        width: parent.width * 0.6
        height: parent.height * 0.6
        radius: width / 2
        color: warnType === "bat" ? "#FF3700" : "#FFA500"
        opacity: 0
    }

    onActiveChanged: {
        canvas.requestPaint()
        if (!active) glowRect.opacity = 0
    }
    onWarnTypeChanged: canvas.requestPaint()
}

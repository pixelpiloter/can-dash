// BarIndicator.qml - 竖向进度条组件
import QtQuick 2.15

Item {
    id: root
    property float value: 0
    property real minValue: 0
    property real maxValue: 100
    property string direction: "bottom_up"  // bottom_up or top_down
    property color barColor: "#00FF88"
    property color backgroundColor: "#1A1A1A"

    width: 40
    height: 180

    // 背景
    Rectangle {
        id: barBg
        anchors.fill: parent
        color: backgroundColor
        radius: 4
    }

    // 填充
    Rectangle {
        id: barFill
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: direction === "bottom_up" ? parent.bottom : undefined
        anchors.top: direction === "top_down" ? parent.top : undefined

        height: {
            var ratio = Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))
            return ratio * parent.height
        }
        color: barColor
        radius: 4

        // 颜色渐变（低电量变红）
        gradient: Gradient {
            GradientStop { position: 0.0; color: barColor }
            GradientStop { position: 1.0; color: value < 20 ? "#FF2200" : barColor }
        }
    }

    // 数值文字
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        text: value.toFixed(0)
        color: "#FFFFFF"
        font.pixelSize: 14
        font.weight: Font.Bold
        visible: barFill.height > 30
    }
}

// WarningLight.qml - 报警灯组件
import QtQuick 2.15

Item {
    id: root
    property bool active: false
    property bool flash: false
    property string imageOn: ""
    property string imageOff: ""
    property real flashHz: 2.0

    width: 60
    height: 60

    // 闪烁定时器
    Timer {
        id: flashTimer
        interval: flash ? (1000 / (flashHz * 2)) : 100
        running: root.flash && root.active
        repeat: true
        onTriggered: imageContainer.opacity = imageContainer.opacity === 1.0 ? 0.0 : 1.0
    }

    // 图片容器
    Item {
        id: imageContainer
        anchors.fill: parent
        opacity: 1.0

        // 非激活状态
        Image {
            anchors.fill: parent
            source: imageOff
            visible: !root.active
            fillMode: Image.PreserveAspectFit
        }

        // 激活状态
        Image {
            anchors.fill: parent
            source: imageOn
            visible: root.active
            fillMode: Image.PreserveAspectFit
        }
    }

    onActiveChanged: {
        if (!active) imageContainer.opacity = 1.0
    }
}

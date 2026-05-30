// SeatBeltZone.qml - 安全带状态显示区域
import QtQuick 2.15
import QtQuick.Layouts 1.3

Item {
    id: root
    property bool isMoving: false
    property variant seatIconStates: []

    width: 400
    height: 80

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 8

        Repeater {
            model: seatIconStates.length

            Item {
                id: seatItem
                width: 60
                height: 70

                // 座位图标（简化文字版）
                Rectangle {
                    id: seatBg
                    anchors.fill: parent
                    radius: 4
                    color: seatIconStates[index].warning ? "#FF2200" :
                           seatIconStates[index].hint ? "#FFAA00" :
                           seatIconStates[index].buckled ? "#00AA44" : "#333333"

                    // 闪烁动画（行驶中未系）
                    SequentialAnimation on opacity {
                        running: root.isMoving && seatIconStates[index].warning
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.2; duration: 250 }
                        NumberAnimation { from: 0.2; to: 1.0; duration: 250 }
                    }
                }

                // 座位标签
                Text {
                    anchors.centerIn: parent
                    text: {
                        var seat = seatIconStates[index]
                        if (!seat.occupied) return "—"
                        return seat.buckled ? "✓" : "!"
                    }
                    color: "#FFFFFF"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }

                // 座位名称
                Text {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: seatIconStates[index].id || ""
                    color: "#888888"
                    font.pixelSize: 10
                }
            }
        }
    }
}

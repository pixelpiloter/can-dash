// MetricCell.qml - 单指标格（用于 DerivedMetrics 面板）
//
// 显示一个小标签 + 大数值 + 单位
//   MetricCell {
//       width: 100; height: 36
//       label: "AVG SPD"
//       value: "62.3"
//       unit: "km/h"
//       valueColor: "#00FF88"
//   }
import QtQuick 2.15

Item {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""
    property color valueColor: "#FFFFFF"

    Column {
        anchors.fill: parent
        spacing: 0
        Text {
            text: root.label
            color: "#666666"
            font.pixelSize: 9
            font.letterSpacing: 1
            font.family: "Roboto Mono, monospace"
        }
        Row {
            spacing: 3
            Text {
                text: root.value
                color: root.valueColor
                font.pixelSize: 18
                font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
            Text {
                anchors.baseline: parent.children[0].baseline
                text: root.unit
                color: "#888888"
                font.pixelSize: 10
                font.family: "Roboto Mono, monospace"
            }
        }
    }
}

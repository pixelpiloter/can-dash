// DashboardMain.qml - 1920x720 汽车数字仪表盘
import QtQuick 2.15
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import CanDash 1.0

ApplicationWindow {
    id: root
    width: 1920
    height: 720
    visible: true
    title: "CAN-Dash 1920x720"
    color: "#000000"

    // 背景
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#0a0a0a" }
            GradientStop { position: 0.3; color: "#111116" }
            GradientStop { position: 0.7; color: "#111116" }
            GradientStop { position: 1.0; color: "#0a0a0a" }
        }
    }

    // 顶部警告灯条
    Rectangle {
        id: warningBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 50
        color: "#CC000000"

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 30

            WarningLight {
                id: batWarn
                width: 50; height: 50
                active: dashboard.alarmActive
                warnType: "bat"
            }
            WarningLight {
                id: seatWarn
                width: 50; height: 50
                active: dashboard.seatIconStates.some(function(s){return s.warning})
                warnType: "seatbelt"
            }
        }
    }

    // ===== 左侧：转速表 (RPM) =====
    GaugeCanvas {
        id: tachGauge
        x: 80
        y: 180
        width: 500
        height: 500
        minValue: 0
        maxValue: 8000
        value: dashboard.get("rpm") || 0
        unit: "RPM"
        dialColor: "#1a3a5c"
        needleColor: "#00AAFF"
        labelColor: "#88CCFF"
        majorTickCount: 8
        minorTicksPerMajor: 5
        startAngleDeg: 135
        endAngleDeg: 405
    }

    // ===== 中央：车速表 (大) =====
    GaugeCanvas {
        id: speedGauge
        anchors.horizontalCenter: parent.horizontalCenter
        y: 130
        width: 700
        height: 700
        minValue: 0
        maxValue: 260
        value: dashboard.get("vehicle_speed") || 0
        unit: "km/h"
        dialColor: "#1a2a1a"
        needleColor: "#00FF88"
        labelColor: "#88FF88"
        majorTickCount: 13
        minorTicksPerMajor: 5
        startAngleDeg: 135
        endAngleDeg: 405
    }

    // ===== 右侧：电池电压 + SOC =====
    Column {
        x: 1550
        y: 200
        spacing: 20

        // 电池电压大字
        Rectangle {
            width: 250; height: 80
            color: "#1a1a1a"
            radius: 8
            border.color: "#333333"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: (dashboard.get("bat_volt") || 0).toFixed(1) + " V"
                color: dashboard.get("bat_volt") > 14.0 ? "#00FF88" : dashboard.get("bat_volt") > 12.0 ? "#FFAA00" : "#FF4400"
                font.pixelSize: 36
                font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
        }

        // SOC 进度条
        Rectangle {
            width: 250; height: 30
            color: "#1a1a1a"
            radius: 6
            border.color: "#333333"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * ((dashboard.get("bat_soc") || 0) / 100)
                radius: 6
                gradient: Gradient {
                    GradientStop { position: 0; color: "#00FF88" }
                    GradientStop { position: 0.5; color: "#AAFF00" }
                    GradientStop { position: 1; color: dashboard.get("bat_soc") < 20 ? "#FF2200" : "#00FF88" }
                }
            }

            Text {
                anchors.centerIn: parent
                text: "SOC " + (dashboard.get("bat_soc") || 0).toFixed(0) + "%"
                color: "#FFFFFF"
                font.pixelSize: 14
                font.weight: Font.Bold
            }
        }

        // 行驶状态
        Rectangle {
            width: 250; height: 80
            color: "#1a1a1a"
            radius: 8
            border.color: dashboard.isMoving ? "#00AA44" : "#333333"
            border.width: dashboard.isMoving ? 2 : 1

            Column {
                anchors.centerIn: parent
                Text {
                    text: dashboard.isMoving ? "行驶中" : "停车"
                    color: dashboard.isMoving ? "#00FF88" : "#666666"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: dashboard.isMoving ? "⚡ 正常" : "◇ 待机"
                    color: "#888888"
                    font.pixelSize: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // 安全带状态
        Rectangle {
            width: 250; height: 120
            color: "#1a1a1a"
            radius: 8
            border.color: "#333333"

            Row {
                anchors.centerIn: parent
                spacing: 10
                Repeater {
                    model: dashboard.seatIconStates.length
                    Rectangle {
                        width: 50; height: 80
                        radius: 4
                        color: {
                            var s = dashboard.seatIconStates[index]
                            if (!s) return "#333333"
                            if (s.warning) return "#FF2200"
                            if (s.hint) return "#FFAA00"
                            if (s.buckled) return "#00AA44"
                            return "#333333"
                        }

                        Column {
                            anchors.centerIn: parent
                            Text {
                                text: {
                                    var s = dashboard.seatIconStates[index]
                                    if (!s || !s.occupied) return "—"
                                    return s.buckled ? "✓" : "!"
                                }
                                color: "#FFFFFF"
                                font.pixelSize: 20
                                font.weight: Font.Bold
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: {
                                    var s = dashboard.seatIconStates[index]
                                    return s ? (s.id || "") : ""
                                }
                                color: "#888888"
                                font.pixelSize: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        SequentialAnimation on opacity {
                            running: root.visible && dashboard.isMoving && dashboard.seatIconStates[index] && dashboard.seatIconStates[index].warning
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.2; duration: 250 }
                            NumberAnimation { from: 0.2; to: 1.0; duration: 250 }
                        }
                    }
                }
            }
        }
    }

    // ===== 底部状态栏 =====
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 55
        color: "#AA000000"
        border.color: "#333333"
        border.width: 1

        Row {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 60

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "⏱ " + Qt.application.applicationVersion
                color: "#666666"
                font.pixelSize: 14
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "CAN-Dash"
                color: "#444444"
                font.pixelSize: 14
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 10; height: 10
                radius: 5
                color: dashboard.alarmActive ? "#FF4400" : "#00AA44"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: dashboard.alarmActive ? ("⚠ " + dashboard.alarmMessageZh) : "系统正常"
                color: dashboard.alarmActive ? "#FF4400" : "#00AA44"
                font.pixelSize: 16
                font.weight: Font.Bold
            }
        }
    }

    // ===== 报警横幅（最高优先级） =====
    Rectangle {
        id: alarmBanner
        anchors.horizontalCenter: parent.horizontalCenter
        y: 60
        visible: dashboard.alarmActive
        width: 600; height: 60
        color: "#CC000000"
        radius: 8
        border.color: "#FF4400"
        border.width: 2

        Text {
            anchors.centerIn: parent
            text: dashboard.alarmMessageZh
            color: "#FF4400"
            font.pixelSize: 28
            font.weight: Font.Bold
        }
    }

    Component.onCompleted: {
        console.log("DashboardMain.qml loaded - 1920x720")
    }
}

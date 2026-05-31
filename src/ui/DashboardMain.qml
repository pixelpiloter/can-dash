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

    // ---- 平滑滤波状态 ----
    property real rawSpeed: 0
    property real rawRpm: 0
    property real displaySpeed: 0
    property real displayRpm: 0

    // ---- 20ms 定时器：积分更新显示值（独立于 CAN 帧）----
    Timer {
        id: displayTimer
        interval: 20
        running: true
        repeat: true
        onTriggered: {
            var alpha = 0.22
            displaySpeed = displaySpeed + (rawSpeed - displaySpeed) * alpha
            displayRpm = displayRpm + (rawRpm - displayRpm) * alpha
            speedGauge.value = displaySpeed
            rpmGauge.value = displayRpm
        }
    }

    // ---- 监听 CAN 数据：只更新原始值 ----
    Connections {
        target: dashboard
        function onDisplayDataChanged() {
            rawSpeed = dashboard.displayData["vehicle_speed"] || 0
            rawRpm = dashboard.displayData["rpm"] || 0
            var v = Math.round((dashboard.displayData["bat_volt"] || 0) * 10) / 10
            var soc = Math.round(dashboard.displayData["bat_soc"] || 0)
            console.log("QML: spd=" + rawSpeed + " rpm=" + rawRpm + " v=" + v + " soc=" + soc)
            batVoltText.text = v.toFixed(1) + " V"
            batVoltText.color = v > 14.0 ? "#00FF88" : v > 12.0 ? "#FFAA00" : "#FF4400"
            socBar.width = batPanel.width * (soc / 100)
            socBar.color = soc < 20 ? "#FF2200" : "#00FF88"
            socText.text = "SOC " + soc + "%"
        }
    }

    // ---- 背景 ----
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

    // ---- 顶部警告灯条 ----
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
        id: rpmGauge
        x: 60
        y: 180
        width: 380
        height: 380
        minValue: 0
        maxValue: 8000
        value: 0
        unit: "RPM"
        dialColor: "#1a3a5c"
        needleColor: "#00AAFF"
        labelColor: "#88CCFF"
        majorTickCount: 8
        minorTicksPerMajor: 5
        startAngleDeg: 135
        endAngleDeg: 405
    }

    // ===== 中央：车速表 =====
    GaugeCanvas {
        id: speedGauge
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        y: -30
        width: 580
        height: 580
        minValue: 0
        maxValue: 260
        value: 0
        unit: "km/h"
        dialColor: "#1a2a1a"
        needleColor: "#00FF88"
        labelColor: "#88FF88"
        majorTickCount: 13
        minorTicksPerMajor: 5
        startAngleDeg: 135
        endAngleDeg: 405
    }

    // ===== 右侧：电池 + SOC + 行驶状态 + 安全带 =====
    Column {
        x: 1420
        y: 180
        spacing: 16

        // 电池电压大字
        Rectangle {
            width: 220; height: 70
            color: "#1a1a1a"
            radius: 8
            border.color: "#333333"
            border.width: 1

            Text {
                id: batVoltText
                anchors.centerIn: parent
                text: (dashboard.displayData["bat_volt"] || 0).toFixed(1) + " V"
                color: "#00FF88"
                font.pixelSize: 32
                font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
        }

        // SOC 进度条
        Rectangle {
            id: batPanel
            width: 220; height: 28
            color: "#1a1a1a"
            radius: 6
            border.color: "#333333"

            Rectangle {
                id: socBar
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * ((dashboard.displayData["bat_soc"] || 0) / 100)
                radius: 6
                color: "#00FF88"
            }

            Text {
                id: socText
                anchors.centerIn: parent
                text: "SOC " + (dashboard.displayData["bat_soc"] || 0).toFixed(0) + "%"
                color: "#FFFFFF"
                font.pixelSize: 13
                font.weight: Font.Bold
            }
        }

        // 行驶状态
        Rectangle {
            width: 220; height: 70
            color: "#1a1a1a"
            radius: 8
            border.color: dashboard.isMoving ? "#00AA44" : "#333333"
            border.width: dashboard.isMoving ? 2 : 1

            Column {
                anchors.centerIn: parent
                Text {
                    text: dashboard.isMoving ? "行驶中" : "停车"
                    color: dashboard.isMoving ? "#00FF88" : "#666666"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: dashboard.isMoving ? "⚡ 正常" : "◇ 待机"
                    color: "#888888"
                    font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // 安全带状态
        Rectangle {
            width: 220; height: 110
            color: "#1a1a1a"
            radius: 8
            border.color: "#333333"

            Row {
                anchors.centerIn: parent
                spacing: 8
                Repeater {
                    model: dashboard.seatIconStates.length
                    Rectangle {
                        width: 44; height: 75
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
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: {
                                    var s = dashboard.seatIconStates[index]
                                    return s ? (s.id || "") : ""
                                }
                                color: "#888888"
                                font.pixelSize: 9
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

    // ===== 报警横幅 =====
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

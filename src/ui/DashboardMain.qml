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
    title: dashboard.tr("app.title")
    color: "#000000"

    // ─── 平滑滤波状态 ───
    property real rawSpeed: 0
    property real rawRpm: 0
    property real displaySpeed: 0
    property real displayRpm: 0

    // ─── 20ms 定时器 ───
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

    // ─── 监听 CAN 数据 ───
    Connections {
        target: dashboard
        function onDisplayDataChanged() {
            var dd = dashboard.displayData
            rawSpeed = dd["vehicle_speed"] || 0
            rawRpm = dd["motor_rpm"] !== undefined ? dd["motor_rpm"] : 0

            var v = Math.round((dd["bat_volt"] || 0) * 10) / 10
            var soc = Math.round(dd["bat_soc"] || 0)
            var temp = dd["motor_temp"] !== undefined ? dd["motor_temp"] : 0

            batVoltText.text = v.toFixed(1) + " " + dashboard.tr("unit.voltage")
            batVoltText.color = v > 14.0 ? "#00FF88" : v > 12.0 ? "#FFAA00" : "#FF4400"

            socBar.width = batPanel.width * (soc / 100)
            socBar.color = soc < 20 ? "#FF2200" : "#00FF88"
            socText.text = dashboard.tr("battery.soc") + " " + soc + dashboard.tr("unit.soc")

            motorTempText.text = temp + dashboard.tr("unit.temperature")

            // 指示灯逻辑
            dashboard.setIndicator("park_brake_light", !dashboard.isMoving)
            dashboard.setIndicator("ready_go_light", dashboard.isMoving)
        }
    }

    // ─── 背景 ───
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

    // ─── 顶部指示灯条（高度 80，包含语言切换在右侧）───
    Rectangle {
        id: indicatorBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 80
        color: "#CC000000"
        z: 5

        // 左侧指示灯
        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 300
            spacing: 14
            IndicatorLight { id: leftTurnLight;    width: 55; height: 55; symbol: "turn_left";     on: dashboard.indicatorOn("left_turn_light");      flash: true;  flashHz: 1.5 }
            IndicatorLight { id: rightTurnLight;   width: 55; height: 55; symbol: "turn_right";    on: dashboard.indicatorOn("right_turn_light");     flash: true;  flashHz: 1.5 }
            Rectangle { width: 2; height: 50; color: "#333333" }
            IndicatorLight { id: batWarnLight;     width: 55; height: 55; symbol: "bat";           on: dashboard.alarmActive && dashboard.alarmMessageZh.indexOf("压") >= 0; flash: true; flashHz: 2 }
            IndicatorLight { id: parkBrakeLight;   width: 55; height: 55; symbol: "park";          on: dashboard.indicatorOn("park_brake_light");    flash: false }
            IndicatorLight { id: readyLight;       width: 55; height: 55; symbol: "ready";         on: dashboard.indicatorOn("ready_go_light");     flash: false }
            IndicatorLight { id: tireLight;        width: 55; height: 55; symbol: "tire";          on: dashboard.alarmActive && dashboard.alarmMessageZh.indexOf("胎压") >= 0; flash: true; flashHz: 2 }
            IndicatorLight { id: engineLight;      width: 55; height: 55; symbol: "check_engine"; on: dashboard.indicatorOn("check_engine_light"); flash: true;  flashHz: 1 }
            IndicatorLight { id: highVoltLight;    width: 55; height: 55; symbol: "high_volt";    on: dashboard.indicatorOn("high_voltage_light");  flash: false }
            IndicatorLight { id: fogLight;         width: 55; height: 55; symbol: "fog";           on: dashboard.indicatorOn("fog_light");          flash: false }
        }

        // 右侧语言切换（在 indicatorBar 内，z=10 确保在最上层）
        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 20
            spacing: 6
            z: 10
            Rectangle {
                width: 50; height: 34; radius: 6
                color: dashboard.currentLanguage === "zh_CN" ? "#FF660000" : "#00000000"
                border.color: dashboard.currentLanguage === "zh_CN" ? "#FFAA00" : "#444444"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "中文"
                    color: dashboard.currentLanguage === "zh_CN" ? "#FFAA00" : "#888888"
                    font.pixelSize: 13; font.weight: Font.Bold
                }
                MouseArea { anchors.fill: parent; onClicked: dashboard.setLanguage("zh_CN") }
            }
            Rectangle {
                width: 50; height: 34; radius: 6
                color: dashboard.currentLanguage === "en_US" ? "#FF660000" : "#00000000"
                border.color: dashboard.currentLanguage === "en_US" ? "#FFAA00" : "#444444"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "EN"
                    color: dashboard.currentLanguage === "en_US" ? "#FFAA00" : "#888888"
                    font.pixelSize: 13; font.weight: Font.Bold
                }
                MouseArea { anchors.fill: parent; onClicked: dashboard.setLanguage("en_US") }
            }
        }
    }

    // ─── 左侧：转速表 ───
    GaugeCanvas {
        id: rpmGauge
        x: 60; y: 180
        width: 380; height: 380
        minValue: 0; maxValue: 8000
        value: 0
        unit: dashboard.tr("unit.rpm")
        dialColor: "#1a3a5c"
        needleColor: "#00AAFF"
        labelColor: "#88CCFF"
        majorTickCount: 8
        minorTicksPerMajor: 5
        startAngleDeg: 135
        endAngleDeg: 405
    }

    // ─── 中央：车速表 ───
    GaugeCanvas {
        id: speedGauge
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        y: -30
        width: 580; height: 580
        minValue: 0; maxValue: 260
        value: 0
        unit: dashboard.tr("unit.speed")
        dialColor: "#1a2a1a"
        needleColor: "#00FF88"
        labelColor: "#88FF88"
        majorTickCount: 13
        minorTicksPerMajor: 5
        startAngleDeg: 135
        endAngleDeg: 405
    }

    // ─── 右侧：电池 + SOC + 行驶状态 + 温度 ───
    Column {
        x: 1420; y: 180
        spacing: 16

        Rectangle {
            width: 220; height: 70
            color: "#1a1a1a"; radius: 8
            border.color: "#333333"; border.width: 1
            Text {
                id: batVoltText
                anchors.centerIn: parent
                text: (dashboard.displayData["bat_volt"] || 0).toFixed(1) + " V"
                color: "#00FF88"; font.pixelSize: 30; font.weight: Font.Bold
                font.family: dashboard.currentFont
            }
        }

        Rectangle {
            id: batPanel
            width: 220; height: 28
            color: "#1a1a1a"; radius: 6
            border.color: "#333333"
            Rectangle {
                id: socBar
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: parent.width * ((dashboard.displayData["bat_soc"] || 0) / 100)
                radius: 6
                color: "#00FF88"
            }
            Text {
                id: socText
                anchors.centerIn: parent
                text: "SOC " + (dashboard.displayData["bat_soc"] || 0).toFixed(0) + "%"
                color: "#FFFFFF"; font.pixelSize: 13; font.weight: Font.Bold
            }
        }

        Rectangle {
            width: 220; height: 70
            color: "#1a1a1a"; radius: 8
            border.color: dashboard.isMoving ? "#00AA44" : "#333333"
            border.width: dashboard.isMoving ? 2 : 1
            Column { anchors.centerIn: parent; spacing: 2
                Text {
                    text: dashboard.isMoving ? dashboard.tr("status.driving") : dashboard.tr("status.parked")
                    color: dashboard.isMoving ? "#00FF88" : "#666666"; font.pixelSize: 22; font.weight: Font.Bold
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: dashboard.isMoving ? (dashboard.tr("status.normal") + " ⚡") : dashboard.tr("status.standby") + " ◇"
                    color: "#888888"; font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Rectangle {
            width: 220; height: 60
            color: "#1a1a1a"; radius: 8; border.color: "#333333"
            Row { anchors.centerIn: parent; spacing: 8
                Text {
                    id: motorTempText
                    text: (dashboard.displayData["motor_temp"] || 0) + dashboard.tr("unit.temperature")
                    color: "#FFAA00"; font.pixelSize: 24; font.weight: Font.Bold
                }
            }
        }

        // 安全带状态
        Rectangle {
            width: 220; height: 110
            color: "#1a1a1a"; radius: 8; border.color: "#333333"
            Row { anchors.centerIn: parent; spacing: 8
                Repeater {
                    model: dashboard.seatIconStates.length
                    Rectangle {
                        width: 44; height: 75; radius: 4
                        color: {
                            var s = dashboard.seatIconStates[index]
                            if (!s) return "#333333"
                            if (s.warning) return "#FF2200"
                            if (s.hint) return "#FFAA00"
                            if (s.buckled) return "#00AA44"
                            return "#333333"
                        }
                        Column { anchors.centerIn: parent; spacing: 2
                            Text {
                                text: {
                                    var s = dashboard.seatIconStates[index]
                                    if (!s || !s.occupied) return "—"
                                    return s.buckled ? "✓" : "!"
                                }
                                color: "#FFFFFF"; font.pixelSize: 18; font.weight: Font.Bold
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text: { var s = dashboard.seatIconStates[index]; return s ? (s.id || "") : "" }
                                color: "#888888"; font.pixelSize: 9
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

    // ─── 报警横幅（z=10，在表盘之上）───
    Rectangle {
        id: alarmBanner
        z: 10
        anchors.horizontalCenter: parent.horizontalCenter
        y: 88
        visible: dashboard.alarmActive
        width: 620; height: 60
        color: "#DD000000"; radius: 8
        border.color: "#FF4400"; border.width: 2
        Text {
            anchors.centerIn: parent
            text: dashboard.alarmActive ? ("⚠ " + dashboard.alarmMessageZh) : ""
            color: "#FF4400"; font.pixelSize: 30; font.weight: Font.Bold
        }
    }

    // ─── 底部状态栏 ───
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 55
        color: "#AA000000"; border.color: "#333333"; border.width: 1
        Row { anchors.fill: parent; anchors.margins: 10; spacing: 60
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "⏱ " + dashboard.tr("app.version")
                color: "#666666"; font.pixelSize: 14
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "CAN-Dash"
                color: "#444444"; font.pixelSize: 14
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 10; height: 10; radius: 5
                color: dashboard.alarmActive ? "#FF4400" : "#00AA44"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: dashboard.alarmActive ? ("⚠ " + dashboard.alarmMessageZh) : dashboard.tr("alarm.system_normal")
                color: dashboard.alarmActive ? "#FF4400" : "#00AA44"
                font.pixelSize: 16; font.weight: Font.Bold
            }
        }
    }

    Component.onCompleted: {
        console.log("DashboardMain.qml loaded - 1920x720 lang=" + dashboard.currentLanguage)
    }
}

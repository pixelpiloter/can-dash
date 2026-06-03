// TripPanel.qml - 行程数据面板 (PR 3)
//
// 显示由 C++ TripComputer 算出的派生指标。
// 数据流: shm.vehicle_speed → ShmDataSource::onTick
//        → m_trip.tick(commit_ts, speed)
//        → DisplaySnapshot{trip_distance_km, trip_avg_speed_kmh, trip_duration_s, trip_moving}
//        → QtDataBinder 4 Q_PROPERTY → DashboardBackend 透传 → QML
//
// 4 个字段：
//   TRIP    小计里程 (km)
//   TIME    行驶时长 (mm:ss)
//   AVG     均速 (km/h, 停车时不计)
//   MOVING  当前是否在行驶（停车时 AVG 显示 0.0 但 TIME 仍累计）
//
// 右侧 reset 按钮调 dashboard.resetTrip()，由 DashboardBackend
// 通过 dynamic_cast 拿 ShmDataSource 具体指针反向调用。
//
// 与 QML 端 DerivedMetrics.qml 的关系：那个自己算 ODO/AVG/电耗/续航，
// 本面板只显示 C++ 域逻辑结果，TODO 后续 PR 4 切走 QML 端计算。
import QtQuick 2.15
import QtQuick.Controls 2.5

Item {
    id: root
    width: 280
    height: 90

    // ─── 背景 ───
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 6
        border.color: "#333333"
        border.width: 1
    }

    // ─── 标题栏 + reset 按钮 ───
    Row {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 6
        height: 16
        spacing: 4

        Text {
            text: "TRIP"
            color: "#888888"
            font.pixelSize: 9
            font.letterSpacing: 1
            font.family: "Roboto Mono, monospace"
            anchors.verticalCenter: parent.verticalCenter
        }

        Item { width: 1; height: 1 }  // 弹性间距

        // 行驶状态点 (停车 = 灰, 行驶 = 绿)
        Rectangle {
            width: 6; height: 6; radius: 3
            color: dashboard.tripIsMoving ? "#00FF88" : "#666666"
            anchors.verticalCenter: parent.verticalCenter
        }

        // 重置按钮
        Rectangle {
            id: resetBtn
            width: 38; height: 16
            color: resetMa.containsMouse ? "#FF4400" : "#552200"
            radius: 2
            border.color: "#FF4400"
            border.width: 1
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: "RESET"
                color: resetMa.containsMouse ? "#FFFFFF" : "#FFAA88"
                font.pixelSize: 8
                font.family: "Roboto Mono, monospace"
            }
            MouseArea {
                id: resetMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: dashboard.resetTrip()
            }
        }
    }

    // ─── 3 个指标 ───
    Row {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        spacing: 8

        MetricCell {
            width: (parent.width - 16) / 3; height: 50
            label: "DIST"
            value: dashboard.tripDistanceKm.toFixed(2)
            unit: "km"
            valueColor: "#88CCFF"
        }
        MetricCell {
            width: (parent.width - 16) / 3; height: 50
            label: "TIME"
            // mm:ss 格式, >1h 显示 h:mm:ss
            value: {
                var s = dashboard.tripDurationS
                if (s >= 3600) {
                    var h = Math.floor(s / 3600)
                    var m = Math.floor((s % 3600) / 60)
                    var sec = s % 60
                    return h + ":" + (m < 10 ? "0" : "") + m + ":" + (sec < 10 ? "0" : "") + sec
                }
                var m2 = Math.floor(s / 60)
                var sec2 = s % 60
                return m2 + ":" + (sec2 < 10 ? "0" : "") + sec2
            }
            unit: ""
            valueColor: "#FFAA00"
        }
        MetricCell {
            width: (parent.width - 16) / 3; height: 50
            label: "AVG"
            value: dashboard.tripAvgSpeedKmh.toFixed(1)
            unit: "km/h"
            // 停车时灰色, 行驶时绿色
            valueColor: dashboard.tripIsMoving ? "#00FF88" : "#666666"
        }
    }
}

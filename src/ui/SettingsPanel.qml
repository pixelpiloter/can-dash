// SettingsPanel.qml - 用户偏好设置面板 (PR 19)
//
// 显示 SettingsManager 当前用户偏好 + 触屏控制入口。
// 数据流:
//   shm.settings_units / shm.settings_brightness
//   → ShmDataSource 同步 m_settings 实例 (PR 13)
//   → DisplaySnapshot.settings_units + settings_brightness
//   → QtDataBinder 2 Q_PROPERTY (settingsUnits / settingsBrightness)
//   → DashboardBackend 透传 → 本面板
//
// 面板元素 (3 段, 横向 1 行 高度 20):
//   1. UNITS 段: METRIC/IMPERIAL 切换 (单位: km/h ↔ mph, km ↔ mi, °C ↔ °F)
//   2. BRIGHT 段: 亮度循环 (0 → 25 → 50 → 75 → 100 → 0, 屏幕亮度占空比)
//   3. RESET 段: 重置到默认 (METRIC + 80%)
//
// 交互: 每段独立 MouseArea, 点击调对应 Q_INVOKABLE (setSettingsUnits/
// setSettingsBrightness/resetSettings), 16ms 内 L3 推 snapshot → Q_PROPERTY
// 更新 → 文本/颜色反映新值
//
// 注: QML 端没有"应用单位换算"逻辑 (距离 km/mi 切换), 那部分需要单独的
// "派生 unit-formatted" 视图层 (PR 后续), 本面板只暴露 raw 偏好
// 入口让用户能设置, 真实应用留给 TripPanel 后续 PR.
import QtQuick 2.15
import QtQuick.Controls 2.15
import CanDash 1.0

Item {
    id: root
    width: 320
    height: 20

    // ─── 背景 ───
    Rectangle {
        anchors.fill: parent
        radius: 3
        color: "#1a1a1a"
        border.color: "#333333"
        border.width: 1
    }

    // ─── 3 段横向布局 ───
    Row {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 0

        // ─── 段 1: UNITS ───
        Rectangle {
            id: unitsSegment
            width: (parent.width - 8) / 3
            height: parent.height
            color: unitsArea.containsMouse ? "#2a2a2a" : "transparent"
            border.color: "#444444"
            border.width: 0

            // 右分隔线
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: parent.height - 4
                color: "#333333"
            }

            Text {
                anchors.centerIn: parent
                text: "UNITS " + (dashboard.settingsUnits === 0 ? "METRIC" : "IMPERIAL")
                color: dashboard.settingsUnits === 0 ? "#88CCFF" : "#FFAA88"
                font.pixelSize: 10
                font.family: "Roboto Mono, monospace"
            }

            MouseArea {
                id: unitsArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    // toggle METRIC (0) ↔ IMPERIAL (1)
                    dashboard.setSettingsUnits(dashboard.settingsUnits === 0 ? 1 : 0)
                }
            }
        }

        // ─── 段 2: BRIGHT ───
        Rectangle {
            id: brightSegment
            width: (parent.width - 8) / 3
            height: parent.height
            color: brightArea.containsMouse ? "#2a2a2a" : "transparent"

            // 右分隔线
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: parent.height - 4
                color: "#333333"
            }

            // 进度条背景 (按 brightness 比例填充, 直观显示亮度级别)
            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                height: 2
                color: "#333333"

                Rectangle {
                    height: parent.height
                    width: parent.width * (dashboard.settingsBrightness / 100.0)
                    color: dashboard.settingsBrightness === 0 ? "#D32F2F" :
                           dashboard.settingsBrightness < 30 ? "#FFC107" : "#00AA44"
                }
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: "☀ " + dashboard.settingsBrightness + "%"
                color: dashboard.settingsBrightness === 0 ? "#D32F2F" : "#FFAA00"
                font.pixelSize: 10
                font.family: "Roboto Mono, monospace"
            }

            MouseArea {
                id: brightArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    // cycle 0 → 25 → 50 → 75 → 100 → 0
                    var b = dashboard.settingsBrightness
                    if (b >= 100)      dashboard.setSettingsBrightness(0)
                    else if (b >= 75)  dashboard.setSettingsBrightness(100)
                    else if (b >= 50)  dashboard.setSettingsBrightness(75)
                    else if (b >= 25)  dashboard.setSettingsBrightness(50)
                    else               dashboard.setSettingsBrightness(25)
                }
            }
        }

        // ─── 段 3: RESET ───
        Rectangle {
            id: resetSegment
            width: (parent.width - 8) / 3
            height: parent.height
            color: resetArea.containsMouse ? "#2a2a2a" : "transparent"

            Text {
                anchors.centerIn: parent
                text: "RESET"
                color: resetArea.containsMouse ? "#FF6600" : "#888888"
                font.pixelSize: 10
                font.family: "Roboto Mono, monospace"
            }

            MouseArea {
                id: resetArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    dashboard.resetSettings()
                }
            }
        }
    }
}

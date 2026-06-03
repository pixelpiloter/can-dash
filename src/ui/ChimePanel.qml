// ChimePanel.qml - 声音提示面板 (PR 14)
//
// 显示 ChimeManager 当前状态 + 静音/音量控制。
// 数据流: m_warning 当前 severity (CRITICAL > WARNING > INFO)
//        → m_chime.onWarningTriggered() (防抖 + 静音)
//        → m_chime.tick() (end_ms 超期清除)
//        → DisplaySnapshot.chime {has_active, severity, frequency_hz,
//                                  duration_ms, repeat_count, volume_pct}
//        → QtDataBinder 6 Q_PROPERTY (chimeActive/severity/freq/duration/repeat/volume)
//        → DashboardBackend 透传 → QML
//
// 面板元素 (4 段):
//   1. 顶部状态点: 静音灰 / 静默绿 / 播放黄 (WARNING) 或红 (CRITICAL)
//   2. 当前事件: FREQ / DURATION / REPEAT / VOLUME
//   3. 静音 toggle: 调 dashboard.setChimeEnabled()
//   4. 音量 slider: 调 dashboard.setChimeVolume()
//
// 实际音频播放需要 QtMultimedia (QSoundEffect), 暂用 console beep 占位
// 未来 PR: 替换 beep 为 .wav 文件 + 调 QSoundEffect.play()
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: 320
    height: 110

    // ─── 背景 ───
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 6
        border.color: chimeActiveGlow.color
        border.width: 2
    }

    // 边框颜色随 active 状态变化 (灰 → 绿/黄/红)
    Rectangle {
        id: chimeActiveGlow
        anchors.fill: parent
        color: "transparent"
        // computed property: 动态边框色
        property color computedColor: {
            if (!dashboard.chimeActive) {
                return "#333333"  // 静默
            }
            if (dashboard.chimeSeverity === 2) {
                return "#D32F2F"  // CRITICAL 红
            }
            if (dashboard.chimeSeverity === 1) {
                return "#FFC107"  // WARNING 黄
            }
            return "#444444"
        }
    }

    // ─── 标题栏 + 静音按钮 ───
    Row {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 8

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "CHIME"
            color: "#888888"
            font.pixelSize: 11
            font.letterSpacing: 1
            font.family: "Roboto Mono, monospace"
        }
    }

    // 静音按钮 (右上)
    Rectangle {
        id: muteBtn
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        width: 64
        height: 22
        radius: 3
        color: muteArea.containsMouse ? "#333333" : "#222222"
        border.color: dashboard.chimeVolumePct === 0 ? "#D32F2F" : "#555555"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: dashboard.chimeVolumePct === 0 ? "MUTED" : "MUTE"
            color: dashboard.chimeVolumePct === 0 ? "#D32F2F" : "#AAAAAA"
            font.pixelSize: 10
            font.family: "Roboto Mono, monospace"
        }

        MouseArea {
            id: muteArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                // toggle 静音
                dashboard.setChimeVolume(dashboard.chimeVolumePct === 0 ? 80 : 0)
            }
        }
    }

    // ─── 状态点 (左上, 紧跟标题) ───
    Rectangle {
        id: statusDot
        x: 8
        y: 32
        width: 10
        height: 10
        radius: 5
        color: {
            if (!dashboard.chimeActive) return "#444444"      // 静默
            if (dashboard.chimeSeverity === 2) return "#D32F2F" // CRITICAL
            if (dashboard.chimeSeverity === 1) return "#FFC107" // WARNING
            return "#1976D2"                                    // INFO
        }

        // CRITICAL 时脉冲闪烁 (暗示紧急)
        SequentialAnimation on opacity {
            running: dashboard.chimeActive && dashboard.chimeSeverity === 2
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.3; duration: 200 }
            NumberAnimation { from: 0.3; to: 1.0; duration: 200 }
        }
    }

    // ─── 当前事件字段 (4 个, 2x2 网格) ───
    Column {
        x: 24
        y: 28
        spacing: 2
        width: parent.width - 32

        Row {
            spacing: 16
            Text { text: "FREQ";   color: "#666666"; font.pixelSize: 9; font.family: "Roboto Mono, monospace" }
            Text {
                text: dashboard.chimeActive ? (dashboard.chimeFrequencyHz + " Hz") : "—"
                color: dashboard.chimeActive ? "#FFAA00" : "#444444"
                font.pixelSize: 14; font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
            Text { text: "DUR";    color: "#666666"; font.pixelSize: 9; font.family: "Roboto Mono, monospace" }
            Text {
                text: dashboard.chimeActive ? (dashboard.chimeDurationMs + " ms") : "—"
                color: dashboard.chimeActive ? "#FFAA00" : "#444444"
                font.pixelSize: 14; font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
        }
        Row {
            spacing: 16
            Text { text: "REPEAT"; color: "#666666"; font.pixelSize: 9; font.family: "Roboto Mono, monospace" }
            Text {
                text: dashboard.chimeActive ? ("× " + dashboard.chimeRepeatCount) : "—"
                color: dashboard.chimeActive ? "#FFAA00" : "#444444"
                font.pixelSize: 14; font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
            Text { text: "VOL";    color: "#666666"; font.pixelSize: 9; font.family: "Roboto Mono, monospace" }
            Text {
                text: dashboard.chimeVolumePct + "%"
                color: dashboard.chimeVolumePct === 0 ? "#D32F2F" : "#FFAA00"
                font.pixelSize: 14; font.weight: Font.Bold
                font.family: "Roboto Mono, monospace"
            }
        }
    }

    // ─── 音量滑条 (底部) ───
    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        spacing: 6

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "0"
            color: "#666666"
            font.pixelSize: 9
            font.family: "Roboto Mono, monospace"
        }

        Rectangle {
            id: sliderTrack
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 60
            height: 4
            color: "#333333"
            radius: 2

            // 填充条 (按 volume_pct 比例)
            Rectangle {
                height: parent.height
                width: parent.width * (dashboard.chimeVolumePct / 100.0)
                color: dashboard.chimeVolumePct === 0 ? "#D32F2F" : "#FFAA00"
                radius: 2
            }

            MouseArea {
                anchors.fill: parent
                onPressed: (mouse) => { updateVolume(mouse.x) }
                onPositionChanged: (mouse) => {
                    if (pressed) updateVolume(mouse.x)
                }
                function updateVolume(x) {
                    var pct = Math.round((Math.max(0, Math.min(width, x)) / width) * 100)
                    dashboard.setChimeVolume(pct)
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "100"
            color: "#666666"
            font.pixelSize: 9
            font.family: "Roboto Mono, monospace"
        }
    }
}

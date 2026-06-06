// LogPanel.qml - 可折叠日志面板
// 功能：显示最近30条日志，支持级别过滤，半透明滑动面板
// 路径: qml/LogPanel.qml
import QtQuick 2.15
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

Item {
    id: root

    // ─── 可见性状态 ───
    property bool visible_: false

    // ─── 级别过滤状态 ───
    property bool filterDebug: true
    property bool filterInfo:  true
    property bool filterWarn:  true
    property bool filterError: true

    // ─── 面板高度 ───
    readonly property int panelHeight: 200

    // ─── z-order: 低于报警横幅 (9999)，高于仪表 ───
    z: 15

    // ─── 滑动动画 ───
    y: visible_ ? (parent.height - panelHeight) : parent.height

    Behavior on y {
        NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
    }

    // ─── 面板本体 ───
    Rectangle {
        anchors.fill: parent
        color: "#CC0A0A14"   // 半透明深蓝黑
        border.color: "#33AAAAFF"
        border.width: 1

        Column {
            anchors.fill: parent
            spacing: 0

            // ─── 工具栏 ───
            Rectangle {
                width: parent.width
                height: 36
                color: "#AA0D0D20"
                border.color: "#22AAAAFF"
                border.width: 0
                border.direction: BorderBottom

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    spacing: 16

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "LOG"
                        color: "#88CCFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        font.family: "Roboto Mono, monospace"
                    }

                    Rectangle { width: 1; height: 20; color: "#33AAAAFF" }

                    // DEBUG checkbox
                    CheckBox {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "DEBUG"
                        checked: filterDebug
                        onCheckedChanged: filterDebug = checked
                        indicator.width: 14; indicator.height: 14
                        contentItem: Text {
                            text: parent.text
                            color: "#888888"
                            leftPadding: parent.indicator.width + 4
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 11
                        }
                    }

                    // INFO checkbox
                    CheckBox {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "INFO"
                        checked: filterInfo
                        onCheckedChanged: filterInfo = checked
                        indicator.width: 14; indicator.height: 14
                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            leftPadding: parent.indicator.width + 4
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 11
                        }
                    }

                    // WARN checkbox
                    CheckBox {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "WARN"
                        checked: filterWarn
                        onCheckedChanged: filterWarn = checked
                        indicator.width: 14; indicator.height: 14
                        contentItem: Text {
                            text: parent.text
                            color: "#FFAA00"
                            leftPadding: parent.indicator.width + 4
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 11
                        }
                    }

                    // ERROR checkbox
                    CheckBox {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "ERROR"
                        checked: filterError
                        onCheckedChanged: filterError = checked
                        indicator.width: 14; indicator.height: 14
                        contentItem: Text {
                            text: parent.text
                            color: "#FF4400"
                            leftPadding: parent.indicator.width + 4
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 11
                        }
                    }

                    Rectangle { width: 1; height: 20; color: "#33AAAAFF" }

                    // Clear button
                    Button {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 60; height: 24
                        Text {
                            anchors.centerIn: parent
                            text: "CLEAR"
                            color: "#FF6644"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#33FF6644" : "#00000000"
                            border.color: "#33FF6644"
                            border.width: 1
                            radius: 3
                        }
                        onClicked: QmlLogger.clearLogs()
                    }

                    // log count
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: QmlLogger.count + " entries"
                        color: "#556677"
                        font.pixelSize: 11
                        font.family: "Roboto Mono, monospace"
                    }
                }

                // 拖动条（可折叠）
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - 4
                    width: 60; height: 3
                    color: "#33AAAAFF"
                    radius: 2
                    MouseArea {
                        anchors.fill: parent
                        onClicked: visible_ = false
                    }
                }
            }

            // ─── 日志列表 ───
            ListView {
                id: logListView
                width: parent.width
                height: parent.height - 36
                clip: true
                model: QmlLogger.logModel
                spacing: 1

                // 自动滚动到底部（最新日志）
                onCountChanged: {
                    if (count > 0) {
                        Qt.callLater(function() {
                            logListView.positionViewAtEnd()
                        })
                    }
                }

                // 过滤委托
                property var levelFilters: ({
                    "DEBUG": filterDebug,
                    "INFO":  filterInfo,
                    "WARN":  filterWarn,
                    "ERROR": filterError
                })

                delegate: Rectangle {
                    width: logListView.width
                    height: 22
                    color: "#08080800"

                    // 级别过滤
                    visible: {
                        var f = logListView.levelFilters
                        var lvl = model.level
                        if (lvl === "DEBUG" && !f["DEBUG"]) return false
                        if (lvl === "INFO"  && !f["INFO"])  return false
                        if (lvl === "WARN"  && !f["WARN"])  return false
                        if (lvl === "ERROR" && !f["ERROR"]) return false
                        return true
                    }

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        spacing: 8

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "[" + model.ts + "]"
                            color: "#556677"
                            font.pixelSize: 11
                            font.family: "Roboto Mono, monospace"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "[" + model.level + "]"
                            color: QmlLogger.levelColor(model.level)
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            font.family: "Roboto Mono, monospace"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "[" + model.source + "]"
                            color: "#4488AA"
                            font.pixelSize: 11
                            font.family: "Roboto Mono, monospace"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: model.message
                            color: QmlLogger.levelColor(model.level)
                            font.pixelSize: 11
                            font.family: "Roboto Mono, monospace"
                            elide: Text.ElideRight
                            width: root.width - 320
                        }
                    }
                }
            }
        }
    }
}

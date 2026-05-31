// AlarmBanner.qml - 报警横幅组件（主容器）
// 功能：最多同时显示3条报警，按优先级排列；滑动进入(200ms ease-out) → 显示3秒 → 淡出(300ms)
// z-order: 9999 (最高层)
import QtQuick 2.15
import QtQuick.Controls 2.5

Item {
    id: root
    anchors.horizontalCenter: parent.horizontalCenter
    y: 88
    z: 9999
    width: 620
    height: implicitHeight
    clip: false

    readonly property int maxAlarms: 3

    // 报警数据模型
    ListModel {
        id: alarmModel
    }

    // ─── 监听 dashboard.alarmActive 变化 ─────────────────────────────────────
    Connections {
        target: dashboard
        function onAlarmActiveChanged() {
            if (dashboard.alarmActive) {
                addAlarmsFromList()
            } else {
                alarmModel.clear()
            }
        }
    }

    // 从 backend 的 alarmList 添加报警到本地模型
    function addAlarmsFromList() {
        var list = dashboard.alarmList
        if (!list || list.length === 0) return

        for (var i = 0; i < Math.min(list.length, maxAlarms); i++) {
            var alarm = list[i]
            // 防重复：相同 text_zh 不重复添加
            var exists = false
            for (var j = 0; j < alarmModel.count; j++) {
                if (alarmModel.get(j).text_zh === (alarm.text_zh || "")) {
                    exists = true; break
                }
            }
            if (!exists) {
                alarmModel.append({
                    text_zh: alarm.text_zh || "",
                    text_en: alarm.text_en || "",
                    color: alarm.color || "#FF4400",
                    font_size: alarm.font_size || 28
                })
            }
        }
    }

    // ─── ListView ──────────────────────────────────────────────────────────────
    Column {
        id: column
        anchors.fill: parent
        spacing: 8

        Repeater {
            model: alarmModel
            delegate: AlarmBannerItem {
                mTextZh: model.text_zh
                mTextEn: model.text_en
                mColor: model.color
                mFontSize: model.font_size
                mModel: alarmModel
                mIndex: index
                width: 620
            }
        }
    }

    // 无报警时不可见
    visible: alarmModel.count > 0
    implicitHeight: visible ? (alarmModel.count * 64 + (Math.max(0, alarmModel.count - 1) * 8)) : 0
}

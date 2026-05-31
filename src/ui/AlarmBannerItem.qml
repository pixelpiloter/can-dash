// AlarmBannerItem.qml - 单条报警项代理
// 由 AlarmBanner.qml 的 Repeater 使用
import QtQuick 2.15

Rectangle {
    id: root

    property string mTextZh: ""
    property string mTextEn: ""
    property string mColor: "#FF4400"
    property int mFontSize: 28
    property ListModel mModel: null
    property int mIndex: -1

    width: 620
    height: 56
    radius: 8
    color: "#DD000000"
    border.color: mColor
    border.width: 2

    // 初始状态：隐藏，等待进入动画
    y: -64
    opacity: 0

    // ─── 报警文字（根据语言自动切换）────────────────────────────────────────
    Row {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12
        anchors.verticalCenter: parent.verticalCenter

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "⚠"
            color: mColor
            font.pixelSize: mFontSize + 4
            font.weight: Font.Bold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: dashboard.currentLanguage === "en_US"
                ? (mTextEn || mTextZh)
                : (mTextZh || mTextEn)
            color: mColor
            font.pixelSize: mFontSize
            font.weight: Font.Bold
            font.family: dashboard.currentFont
            verticalAlignment: Text.AlignVCenter
        }
    }

    // ─── 动画：进入(200ms) → 停留(3s) → 退出(300ms) → 从模型移除 ───────────

    // 阶段1：进入动画 - 滑动 + 淡入 (200ms ease-out)
    SequentialAnimation {
        id: enterSeq
        running: true
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "y"
                from: -64; to: 0
                duration: 200
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "opacity"
                from: 0; to: 1
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
        onStopped: {
            // 进入动画结束 → 启动3秒计时器
            holdTimer.start()
        }
    }

    // 阶段2：停留3秒
    Timer {
        id: holdTimer
        interval: 3000
        repeat: false
        onTriggered: exitSeq.start()
    }

    // 阶段3：退出动画 - 淡出 (300ms)
    SequentialAnimation {
        id: exitSeq
        NumberAnimation {
            target: root
            property: "opacity"
            from: 1; to: 0
            duration: 300
            easing.type: Easing.InCubic
        }
        onStopped: {
            // 退出动画结束 → 从模型移除
            if (mModel && mIndex >= 0 && mIndex < mModel.count) {
                // 再次检查 text_zh 防止误删
                if (mModel.get(mIndex) && mModel.get(mIndex).text_zh === mTextZh) {
                    mModel.remove(mIndex)
                }
            }
        }
    }
}

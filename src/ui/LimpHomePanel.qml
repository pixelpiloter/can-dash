// LimpHomePanel.qml - 跛行模式提示面板 (PR 46: SYS-003 QML 端入口)
//
// 功能: 当 LimpHomeRuntime 进入 L1/L2/L3 跛行模式时, 在仪表盘顶部显示级别 + 提示文案
// 数据流 (PR 44 L3 数据流接入):
//   L2 limp_home_runtime → DisplayLimpHomeState snapshot
//   → QtDataBinder Q_PROPERTY (limpHomeLevel/Active/MessageZh/MessageEn)
//   → DashboardBackend 透传 → 本面板
//
// 颜色映射:
//   L1 (轻度, 1 个关键信号超时): 黄色 #FFAA00
//   L2 (紧急, 2+ 信号超时):     橙色 #FF6600
//   L3 (最深, CAN 总线断开近似): 红色 #FF2200
//
// 闪烁: L1 不闪 (轻度), L2/L3 闪烁 1Hz (紧急, 跟 indicator flashHz=1 一致)
//
// 交互: 只读显示, 不接受触屏操作 (跛行模式下的用户操作由 ChimePanel/SettingsPanel 接管)
// 位置: DashboardMain.qml 顶部居中, AlarmBanner 下方 (y=180)
//
// 注: PR 43 L2+test 升级 + PR 44 L3 数据流接入 + 本 PR 46 QML 端入口
//     = SYS-003 跛行模式端到端完成 (init → tick → 触发 → 显示)
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: 480
    height: 50

    // 内部状态: 整面板的最终透明度
    // 组合 = 基础透明度 (0/1) × 闪烁衰减 (L2/L3)
    property real flashOpacity: 1.0
    opacity: dashboard.limpHomeActive ? flashOpacity : 0.0

    // 显隐过渡 (200ms 淡入, 避免硬切)
    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    // 闪烁动画 (L2/L3 闪烁 1Hz, L1 不闪)
    SequentialAnimation on flashOpacity {
        running: dashboard.limpHomeActive && dashboard.limpHomeLevel >= 2
        loops: Animation.Infinite
        NumberAnimation { from: 1.0; to: 0.4; duration: 500 }
        NumberAnimation { from: 0.4; to: 1.0; duration: 500 }
    }

    // ─── 背景框 (颜色随 level 变化) ───
    Rectangle {
        anchors.fill: parent
        radius: 6
        color: {
            if (dashboard.limpHomeLevel === 1) return "#33FFAA00"      // L1 半透黄
            if (dashboard.limpHomeLevel === 2) return "#44FF6600"      // L2 半透橙
            if (dashboard.limpHomeLevel === 3) return "#55FF2200"      // L3 半透红
            return "#00000000"                                          // fallback
        }
        border.color: {
            if (dashboard.limpHomeLevel === 1) return "#FFAA00"        // L1 黄边
            if (dashboard.limpHomeLevel === 2) return "#FF6600"        // L2 橙边
            if (dashboard.limpHomeLevel === 3) return "#FF2200"        // L3 红边
            return "#888888"                                            // fallback
        }
        border.width: 2
    }

    // ─── 内容行 (图标 + 级别 + 文案) ───
    Row {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        // 警告图标 ⚠ (大字号, 颜色随 level)
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "⚠"
            color: {
                if (dashboard.limpHomeLevel === 1) return "#FFAA00"
                if (dashboard.limpHomeLevel === 2) return "#FF6600"
                if (dashboard.limpHomeLevel === 3) return "#FF2200"
                return "#888888"
            }
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        // 文案 (中英文根据 dashboard.currentLanguage 切换)
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 50
            text: {
                if (dashboard.limpHomeLevel === 0) return ""            // NORMAL, 整面板不可见
                var msg = dashboard.currentLanguage === "zh_CN"
                          ? dashboard.limpHomeMessageZh
                          : dashboard.limpHomeMessageEn
                return msg
            }
            color: "#FFFFFF"
            font.pixelSize: 18
            font.weight: Font.Bold
            elide: Text.ElideRight
            wrapMode: Text.WordWrap
            maximumLineCount: 2
        }
    }
}

// DerivedMetrics.qml - 派生指标面板
//
// 计算：
//   1. 平均车速（60s 滑动平均）
//   2. 累计里程（积分 vehicle_speed × dt）
//   3. 百公里电耗（放电能量 / 累计里程 × 100）
//   4. 续航可信度（实际 SOC 消耗 vs 显示 ev_range 的比例）
//
// 所有计算在 QML 端完成，0 C++ 改动：
//   - 100ms 采样环
//   - 滑动窗口用环形 buffer
//   - 单位转换纯 QML
import QtQuick 2.15

Item {
    id: root
    width: 280
    height: 90

    // ─── 输入 ───
    property var dataSource: dashboard.displayData
    property int sampleIntervalMs: 100
    property int avgWindowSeconds: 60

    // ─── 内部状态 ───
    property real _lastTickMs: 0
    property real _avgSum: 0
    property int _avgCount: 0
    property real _avgSpeedKmh: 0

    property real _odometerKm: 0
    property real _energyKWh: 0    // 累计放电能量（kWh, 正值）
    property real _efficiency: 0    // kWh / 100km

    property real _lastSoc: -1
    property real _startSoc: -1
    property real _startEvRange: 0
    property real _rangeConfidence: 100  // 0-100%

    // ─── 采样定时器（100ms）───
    Timer {
        id: metricTimer
        interval: root.sampleIntervalMs
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var dd = root.dataSource
            if (!dd) return
            var now = Date.now()
            var dt = (root._lastTickMs > 0) ? (now - root._lastTickMs) / 1000.0 : 0
            root._lastTickMs = now

            var speed = +dd["vehicle_speed"] || 0
            var soc = +dd["bat_soc"] || 0
            var volt = +dd["bat_volt"] || 0
            var curr = +dd["bat_curr"] || 0
            var evRange = +dd["ev_range"] || 0

            // ── 1. 平均车速（60s 滑动）──
            root._avgSum += speed
            root._avgCount += 1
            // 简单衰减式（60s 后旧样本权重 ~e^-1 ≈ 0.37）
            if (root._avgCount > root.avgWindowSeconds * (1000 / root.sampleIntervalMs)) {
                root._avgSum = root._avgSum * 0.995
                root._avgCount = Math.floor(root._avgCount * 0.995)
            }
            if (root._avgCount > 0) {
                root._avgSpeedKmh = root._avgSum / root._avgCount
            }

            // ── 2. 累计里程（积分 km/h × s → km）──
            if (dt > 0 && dt < 5) {  // 防止暂停后 dt 暴涨
                root._odometerKm += speed * dt / 3600.0
            }

            // ── 3. 累计放电能量（bat_curr > 0 = 放电）──
            if (dt > 0 && dt < 5 && curr > 0 && volt > 0) {
                // kW = V × A / 1000; kWh = kW × h
                root._energyKWh += (volt * curr / 1000.0) * (dt / 3600.0)
            }

            // ── 4. 百公里电耗 ──
            if (root._odometerKm > 0.5) {  // 起步 500m 后才显示
                root._efficiency = root._energyKWh / root._odometerKm * 100.0
            } else {
                root._efficiency = 0
            }

            // ── 5. 续航可信度 ──
            if (root._lastSoc < 0) {
                root._lastSoc = soc
                root._startSoc = soc
                root._startEvRange = evRange
            } else {
                var socDelta = root._startSoc - soc   // SOC 下降量
                var rangeDelta = root._startEvRange - evRange  // 显示续航下降
                if (socDelta > 1) {  // SOC 变化 > 1% 才计算
                    // 实际 km/SOC% = 累计里程 / SOC 下降
                    var actualKmPerPct = root._odometerKm / socDelta
                    // 显示 km/SOC% = 显示续航 / 总 SOC
                    var displayedKmPerPct = root._startEvRange / Math.max(root._startSoc, 1)
                    if (displayedKmPerPct > 0) {
                        // 可信度 = min(100, actual/displayed × 100)
                        root._rangeConfidence = Math.min(100, actualKmPerPct / displayedKmPerPct * 100)
                    }
                }
            }
            root._lastSoc = soc
        }
    }

    // ─── 背景 ───
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 6
        border.color: "#333333"
        border.width: 1
    }

    // ─── 4 个指标 2×2 网格 ───
    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 2

        Row {
            spacing: 12
            width: parent.width
            MetricCell {
                width: (parent.width - 12) / 2; height: 36
                label: "AVG SPD"; value: root._avgSpeedKmh.toFixed(1); unit: "km/h"
                valueColor: "#00FF88"
            }
            MetricCell {
                width: (parent.width - 12) / 2; height: 36
                label: "ODO"; value: root._odometerKm.toFixed(2); unit: "km"
                valueColor: "#88CCFF"
            }
        }

        Row {
            spacing: 12
            width: parent.width
            MetricCell {
                width: (parent.width - 12) / 2; height: 36
                label: "EFF"; value: root._efficiency.toFixed(1); unit: "kWh/100km"
                valueColor: "#FFAA00"
            }
            MetricCell {
                width: (parent.width - 12) / 2; height: 36
                label: "RANGE"; value: root._rangeConfidence.toFixed(0); unit: "%"
                valueColor: root._rangeConfidence >= 80 ? "#00FF88" :
                            root._rangeConfidence >= 50 ? "#FFAA00" : "#FF4400"
            }
        }
    }
}

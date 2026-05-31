# QML z-ordering 与层叠问题

## 默认层叠顺序

QML 默认按声明顺序层叠——后声明的元素在上层（z-index 更低）。

在同一个 parent 内，声明顺序决定覆盖关系：
```qml
Item {
    Rectangle { id: rect1 }  // 先声明，在底层
    Rectangle { id: rect2 }  // 后声明，在上层 → 覆盖 rect1
}
```

## z 属性控制层叠

QML 的 `z` 属性可以显式控制层叠顺序：
- `z: 0`（默认）按声明顺序
- `z: 10` → 在 z=0 元素之上
- `z: 20` → 在 z=10 元素之上

```qml
Item {
    Rectangle { z: 0 }  // 底层
    Rectangle { z: 10 } // 中层
    Rectangle { z: 20 } // 顶层
}
```

## 典型问题：报警横幅被表盘遮挡

**症状**：报警横幅 `alarmBanner` 在 `y: 88` 位置，但被中央车速表盘 `GaugeCanvas` 盖住。

**根因**：
1. `GaugeCanvas` 使用 `anchors.verticalCenter: parent.verticalCenter`（580x580 大表盘覆盖整个中间区域）
2. `alarmBanner` 的 `y: 88` 仍在表盘范围内
3. 默认层叠：后声明的元素在上层，若 `alarmBanner` 在 `GaugeCanvas` 之前声明，则 `GaugeCanvas` 在上层

**解法**：对 `alarmBanner` 设置 `z: 10`：
```qml
Rectangle {
    id: alarmBanner
    y: 88
    z: 10  // ← 在 GaugeCanvas（z=0）之上
    // ...
}
```

## 正确层叠示例（DashboardMain.qml）

```qml
ApplicationWindow {
    id: root
    width: 1920
    height: 720

    // ─── 底层：仪表盘表盘 ───
    GaugeCanvas {
        id: speedGauge
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        y: -30
        width: 580
        height: 580
        // z: 0（默认，在底层）
    }

    GaugeCanvas {
        id: rpmGauge
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 400
        height: 400
        // z: 0
    }

    // ─── 中层：报警横幅 ───
    Rectangle {
        id: alarmBanner
        anchors.top: parent.top
        anchors.topMargin: 88
        anchors.horizontalCenter: parent.horizontalCenter
        z: 10  // ← 在表盘之上
        // ...
    }

    // ─── 顶层：指示灯条 + 语言切换 ───
    Row {
        id: indicatorBar
        anchors.top: parent.top
        anchors.topMargin: 10
        // z: 0（默认，在 alarmBanner 之下）
    }

    Rectangle {
        id: langSwitch
        anchors.top: parent.top
        anchors.topMargin: 90
        anchors.right: parent.right
        anchors.rightMargin: 14
        z: 20  // ← 最顶层
    }
}
```

## 常见踩坑

### 1. 误以为 `anchors` 控制 z-order

`anchors` 只控制位置，不控制层叠。设置了 `anchors.verticalCenter` 的元素仍可能覆盖其他元素。

### 2. 只在某个条件下才设置 z

```qml
// 错误：条件 z 无效
Rectangle {
    z: condition ? 10 : 0  // z 必须是常量，不支持绑定
}

// 正确：始终设置足够高的 z
Rectangle {
    z: 10
}
```

### 3. parent 内的 z 与跨 parent 的 z

z 只在同一 parent 内比较。不同 parent 的子元素各自层叠。

```qml
Item {
    Rectangle { z: 100 }  // 在 parent 内最高
}
Item {
    Rectangle { z: 1 }   // 在自己的 parent 内是最高
}
// 两个 Rectangle 可能重叠，取决于它们的父 Item 的层叠顺序
```

## 调试技巧

用 `QQuickCanvas` 的 `dumpItemTree()` 或在 QML 中加可见边框调试：

```qml
Rectangle {
    id: alarmBanner
    border.color: "red"
    border.width: 2
    z: 10
}
```

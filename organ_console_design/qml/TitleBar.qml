import QtQuick 2.15
import QtQuick.Window 2.15

// 自绘标题栏：打开文件（含最近文件下拉）、保存、系统状态按钮（含浮层）、
// 中央标题、录制 / 停止 / 播放暂停、设置、窗口按钮
Item {
    id: root
    height: 40

    signal openRequested()
    signal recentSelected(string path)
    signal closeFileRequested()
    signal saveRequested()
    signal playRequested()
    signal stopRequested()
    signal recordRequested()
    signal settingsRequested()
    signal minimizeRequested()
    signal maximizeRequested()
    signal closeRequested()

    Theme { id: theme }

    readonly property bool busy: console.playing || console.paused
    readonly property var recentList: console.recentFiles

    component BarButton: Rectangle {
        property string label: ""
        property string iconKind: "none"   // none | dot | square | play | pause
        property bool highlighted: false
        property color iconColor: theme.text
        signal clicked()

        width: contentRow.implicitWidth + 20
        height: 26
        radius: 3
        color: highlighted ? Qt.rgba(0.133, 0.827, 0.933, 0.10)
                           : (btnMouse.containsMouse ? theme.panelHover : "transparent")
        border.color: highlighted ? theme.accent : theme.border
        border.width: 1
        opacity: enabled ? 1.0 : 0.35

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 6

            Item {
                width: 10
                height: 10
                visible: iconKind !== "none"
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    visible: iconKind === "dot"
                    color: iconColor
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    visible: iconKind === "square"
                    color: iconColor
                }
                Canvas {
                    anchors.fill: parent
                    visible: iconKind === "play"
                    onVisibleChanged: if (visible) requestPaint()
                    Component.onCompleted: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.fillStyle = iconColor;
                        ctx.beginPath();
                        ctx.moveTo(1, 0);
                        ctx.lineTo(9, 5);
                        ctx.lineTo(1, 10);
                        ctx.closePath();
                        ctx.fill();
                    }
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 3
                    visible: iconKind === "pause"
                    Rectangle { width: 3; height: 10; color: iconColor }
                    Rectangle { width: 3; height: 10; color: iconColor }
                }
            }

            Text {
                text: label
                color: theme.text
                font.pixelSize: 12
                visible: label.length > 0
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }

    component WinButton: Rectangle {
        property string kind: "min"
        signal clicked()

        width: 34
        height: 26
        radius: 3
        color: wcMouse.containsMouse ? (kind === "close" ? "#C42B1C" : theme.panelHover) : "transparent"

        Rectangle {
            visible: kind === "min"
            width: 10
            height: 1
            color: theme.text
            anchors.centerIn: parent
        }
        Rectangle {
            visible: kind === "max"
            width: 10
            height: 10
            color: "transparent"
            border.color: theme.text
            border.width: 1
            anchors.centerIn: parent
        }
        Text {
            visible: kind === "close"
            text: "×"
            color: theme.text
            font.pixelSize: 16
            anchors.centerIn: parent
        }
        MouseArea {
            id: wcMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.panel
    }
    MouseArea {
        anchors.fill: parent
        onPressed: Window.window.startSystemMove()
        onDoubleClicked: root.maximizeRequested()
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: theme.border
    }

    // 左：打开文件 / 保存 / 状态
    Row {
        id: leftRow
        z: 2
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Rectangle {
            id: openSplit
            width: 112
            height: 26
            radius: 3
            color: openMainMouse.containsMouse || openArrowMouse.containsMouse
                   ? theme.panelHover : "transparent"
            border.color: theme.border
            border.width: 1

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: "打开文件"
                color: theme.text
                font.pixelSize: 12
            }
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 25
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 14
                color: theme.border
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "▾"
                color: theme.text
                font.pixelSize: 11
            }
            MouseArea {
                id: openMainMouse
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - 26
                hoverEnabled: true
                onClicked: {
                    recentPopup.visible = false;
                    statusPopup.visible = false;
                    root.openRequested();
                }
            }
            MouseArea {
                id: openArrowMouse
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 26
                hoverEnabled: true
                onClicked: {
                    recentPopup.visible = !recentPopup.visible;
                    statusPopup.visible = false;
                }
            }
        }

        BarButton {
            label: "保存"
            enabled: console.canSave && !console.recording && !root.busy
            onClicked: root.saveRequested()
        }

        Rectangle {
            id: statusBtn
            width: statusRow.implicitWidth + 18
            height: 26
            radius: 3
            color: statusMouse.containsMouse ? theme.panelHover : "transparent"
            border.color: theme.border
            border.width: 1

            Row {
                id: statusRow
                anchors.centerIn: parent
                spacing: 6
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: (console.serialConnected && console.boardOnline)
                           ? theme.okGreen
                           : (console.serialConnected ? theme.warnYellow : theme.errRed)
                }
                Text {
                    text: (console.serialConnected && console.boardOnline)
                          ? "系统正常"
                          : (console.serialConnected ? "主板未应答" : "串口未连接")
                    color: (console.serialConnected && console.boardOnline)
                           ? theme.secondary : theme.errRed
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "▾"
                    color: theme.secondary
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: statusMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    statusPopup.visible = !statusPopup.visible;
                    recentPopup.visible = false;
                }
            }
        }
    }

    // 中：标题（文件名 + 关闭文件）
    Row {
        anchors.centerIn: parent
        spacing: 6

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: {
                var name = console.currentFile;
                if (name && name.length > 0) {
                    var slash = Math.max(name.lastIndexOf("/"), name.lastIndexOf("\\"));
                    name = name.substring(slash + 1);
                    return "Organ Console — " + name;
                }
                return "Organ Console";
            }
            color: theme.text
            font.pixelSize: 13
        }

        Rectangle {
            visible: console.fileLoaded
            width: 18
            height: 18
            radius: 3
            anchors.verticalCenter: parent.verticalCenter
            color: closeFileMouse.containsMouse ? theme.panelHover : "transparent"

            Text {
                anchors.centerIn: parent
                text: "×"
                color: theme.secondary
                font.pixelSize: 12
            }
            MouseArea {
                id: closeFileMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.closeFileRequested()
            }
        }
    }

    // 右：走带 + 设置 + 窗口
    Row {
        id: rightRow
        z: 2
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        BarButton {
            label: "录制"
            iconKind: "dot"
            iconColor: theme.recordRed
            highlighted: console.recording
            enabled: !console.playing && !console.paused
            onClicked: root.recordRequested()
        }
        BarButton {
            label: "停止"
            iconKind: "square"
            enabled: console.fileLoaded || console.recording || root.busy
            onClicked: root.stopRequested()
        }
        BarButton {
            label: console.playing && !console.paused ? "暂停" : "播放"
            iconKind: console.playing && !console.paused ? "pause" : "play"
            highlighted: root.busy
            enabled: console.fileLoaded && !console.recording
            onClicked: root.playRequested()
        }
        BarButton {
            label: "设置"
            onClicked: root.settingsRequested()
        }
        Item { width: 10; height: 1 }
        WinButton { kind: "min"; onClicked: root.minimizeRequested() }
        WinButton { kind: "max"; onClicked: root.maximizeRequested() }
        WinButton { kind: "close"; onClicked: root.closeRequested() }
    }

    // 最近文件下拉
    Rectangle {
        id: recentPopup
        visible: false
        z: 100
        x: openSplit.mapToItem(root, 0, 0).x
        y: root.height + 4
        width: 320
        height: recentColumn.implicitHeight + 12
        color: theme.panel
        border.color: theme.border
        border.width: 1
        radius: 4

        Column {
            id: recentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6
            spacing: 2

            Text {
                visible: root.recentList.length === 0
                height: 28
                width: recentColumn.width
                text: "暂无最近文件"
                color: theme.secondary
                font.pixelSize: 12
                leftPadding: 8
                verticalAlignment: Text.AlignVCenter
            }

            Repeater {
                model: root.recentList
                delegate: Rectangle {
                    width: recentColumn.width
                    height: 30
                    radius: 3
                    color: itemMouse.containsMouse ? theme.panelHover : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            var name = modelData;
                            var slash = Math.max(name.lastIndexOf("/"), name.lastIndexOf("\\"));
                            return slash >= 0 ? name.substring(slash + 1) : name;
                        }
                        color: theme.text
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            recentPopup.visible = false;
                            root.recentSelected(modelData);
                        }
                    }
                }
            }

            Rectangle {
                visible: console.fileLoaded
                width: recentColumn.width
                height: 1
                color: theme.border
            }
            Rectangle {
                visible: console.fileLoaded
                width: recentColumn.width
                height: 30
                radius: 3
                color: closeItemMouse.containsMouse ? theme.panelHover : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "关闭文件"
                    color: theme.secondary
                    font.pixelSize: 12
                }
                MouseArea {
                    id: closeItemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        recentPopup.visible = false;
                        root.closeFileRequested();
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: {}   // 吞掉点击，避免穿透到标题栏拖动
        }
    }

    // 系统状态浮层
    Rectangle {
        id: statusPopup
        visible: false
        z: 100
        x: statusBtn.mapToItem(root, 0, 0).x
        y: root.height + 4
        width: 340
        height: statusColumn.implicitHeight + 16
        color: theme.panel
        border.color: theme.border
        border.width: 1
        radius: 4

        Column {
            id: statusColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 7

            Row {
                width: statusColumn.width
                spacing: 8
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: (console.serialConnected && console.boardOnline)
                           ? theme.okGreen : theme.errRed
                }
                Text {
                    text: (console.serialConnected && console.boardOnline)
                          ? "系统正常"
                          : (console.serialConnected ? "主板没有应答" : "串口未连接")
                    color: (console.serialConnected && console.boardOnline)
                           ? theme.okGreen : theme.errRed
                    font.pixelSize: 12
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle { width: statusColumn.width; height: 1; color: theme.border }

            Repeater {
                model: [
                    { "name": "ORGAN 主板",
                      "value": console.boardOnline ? "在线" : "离线",
                      "level": console.boardOnline ? 0 : 1 },
                    { "name": "串口",
                      "value": console.serialConnected
                               ? (console.linkedPort + " · " + console.baudRate)
                               : "未连接",
                      "level": console.serialConnected ? 0 : 2 },
                    { "name": "MIDI 输入",
                      "value": console.midiInputOpen ? "已连接" : "未连接",
                      "level": console.midiInputOpen ? 0 : 1 },
                    { "name": "虚拟演奏",
                      "value": console.virtualPlayback ? "开启" : "关闭",
                      "level": 0 },
                    { "name": "通道数",
                      "value": String(console.channelCount),
                      "level": 0 }
                ]
                delegate: Row {
                    width: statusColumn.width
                    spacing: 8
                    Rectangle {
                        width: 7
                        height: 7
                        radius: 3.5
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData.level === 0 ? theme.okGreen
                             : modelData.level === 1 ? theme.warnYellow : theme.errRed
                    }
                    Text {
                        text: modelData.name
                        color: theme.text
                        font.pixelSize: 12
                        width: 92
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.value
                        color: theme.secondary
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Rectangle {
                width: statusColumn.width
                height: 1
                color: theme.border
                visible: console.lastError.length > 0
            }
            Text {
                width: statusColumn.width
                visible: console.lastError.length > 0
                text: "最近错误：" + console.lastError
                color: theme.errRed
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: {}
        }
    }
}

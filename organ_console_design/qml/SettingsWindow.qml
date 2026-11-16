import QtQuick 2.15
import QtQuick.Window 2.15

// 设置窗口：硬件连接 / 键位映射 / 运行日志 / 关于
// 播放或录制期间只置灰关键项，日志与关于始终可用。
Window {
    id: win
    width: 820
    height: 580
    minimumWidth: 680
    minimumHeight: 440
    title: "设置"
    color: "#0F1216"
    flags: Qt.Window | Qt.FramelessWindowHint

    Theme { id: theme }

    property int currentIndex: 0
    property bool locked: console.playing || console.paused || console.recording

    component SectionLabel: Text {
        color: theme.text
        font.pixelSize: 15
        font.bold: true
    }
    component SubLabel: Text {
        color: theme.secondary
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
    component PlainButton: Rectangle {
        property string label: ""
        property bool highlighted: false
        signal clicked()
        width: Math.max(64, pbText.implicitWidth + 20)
        height: 26
        radius: 3
        color: highlighted ? Qt.rgba(0.133, 0.827, 0.933, 0.12)
                           : (pbMouse.containsMouse ? theme.panelHover : theme.panel)
        border.color: highlighted ? theme.accent : theme.border
        border.width: 1
        opacity: enabled ? 1.0 : 0.4
        Text {
            id: pbText
            anchors.centerIn: parent
            text: parent.label
            color: parent.highlighted ? theme.accent : theme.text
            font.pixelSize: 12
        }
        MouseArea {
            id: pbMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }

    Column {
        anchors.fill: parent

        // 标题栏
        Item {
            width: parent.width
            height: 40
            Rectangle { anchors.fill: parent; color: theme.panel }
            MouseArea {
                anchors.fill: parent
                onPressed: win.startSystemMove()
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "设置"
                color: theme.text
                font.pixelSize: 13
            }
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 26
                radius: 3
                color: closeMouse.containsMouse ? "#C42B1C" : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: theme.text
                    font.pixelSize: 16
                }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: win.close()
                }
            }
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: theme.border
            }
        }

        Row {
            width: parent.width
            height: parent.height - 40

            // 侧边栏
            Rectangle {
                width: 172
                height: parent.height
                color: theme.panel
                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    Repeater {
                        model: ["硬件连接", "键位映射", "运行日志", "关于"]
                        delegate: Rectangle {
                            width: parent.width
                            height: 36
                            radius: 4
                            color: win.currentIndex === index ? Qt.rgba(0.133, 0.827, 0.933, 0.12)
                                 : sideMouse.containsMouse ? theme.panelHover : "transparent"
                            border.color: win.currentIndex === index ? theme.accent : "transparent"
                            border.width: 1
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                color: win.currentIndex === index ? theme.accent : theme.text
                                font.pixelSize: 13
                            }
                            MouseArea {
                                id: sideMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: win.currentIndex = index
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: parent.height
                    width: 1
                    color: theme.border
                }
            }

            Item {
                width: parent.width - 172
                height: parent.height
                Loader {
                    anchors.fill: parent
                    anchors.margins: 18
                    sourceComponent: [hardwarePage, mappingPage, logPage, aboutPage][win.currentIndex]
                }
            }
        }
    }

    // ---------------------------------------------------------- 硬件连接
    Component {
        id: hardwarePage
        Item {
            id: hwPage

            property bool busy: win.locked
            property var devices: console.midiInputDevices
            property var ports: console.serialPortNames
            property int midiIndex: -1
            property int baudRow: -1
            property Item baudAnchor: null

            function toggleMidi(index) {
                if (busy) return;
                if (console.midiInputOpen) {
                    if (midiIndex === index) {
                        console.disconnectMidi();
                        midiIndex = -1;
                        return;
                    }
                    console.disconnectMidi();
                }
                if (console.connectMidi(index)) midiIndex = index;
            }

            function togglePort(portName) {
                if (busy) return;
                if (console.serialConnected && console.linkedPort === portName) {
                    console.disconnectSerial();
                } else {
                    console.connectSerial(portName, console.baudRate);
                }
            }

            Flickable {
                id: hwFlick
                anchors.fill: parent
                contentHeight: hwColumn.implicitHeight + 8
                clip: true

                Column {
                    id: hwColumn
                    width: hwFlick.width
                    spacing: 12

                    SectionLabel { text: "硬件连接" }
                    SubLabel {
                        width: parent.width
                        text: "双击设备或串口即可连接 / 断开。播放或录制期间连接项会被锁定。"
                    }

                    Row {
                        spacing: 10
                        Text { text: "MIDI 设备"; color: theme.secondary; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        PlainButton {
                            label: "刷新"
                            enabled: !hwPage.busy
                            onClicked: console.refreshMidiDevices()
                        }
                    }

                    Text {
                        visible: hwPage.devices.length === 0
                        text: "未发现 MIDI 输入设备。"
                        color: theme.secondary
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: hwPage.devices
                        delegate: Rectangle {
                            width: hwColumn.width
                            height: 38
                            radius: 3
                            property bool connected: console.midiInputOpen && hwPage.midiIndex === index
                            color: connected ? Qt.rgba(0.133, 0.827, 0.933, 0.10) : theme.panel
                            border.color: connected ? theme.accent : theme.border
                            border.width: 1
                            opacity: hwPage.busy ? 0.55 : 1.0

                            Rectangle {
                                width: 3
                                height: parent.height
                                color: connected ? theme.accent : "transparent"
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                color: theme.text
                                font.pixelSize: 12
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                text: connected ? "已连接" : "未连接"
                                color: connected ? theme.okGreen : theme.secondary
                                font.pixelSize: 11
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !hwPage.busy
                                onDoubleClicked: hwPage.toggleMidi(index)
                            }
                        }
                    }

                    Item { width: 1; height: 4 }
                    Row {
                        spacing: 10
                        Text { text: "串口"; color: theme.secondary; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        PlainButton {
                            label: "刷新"
                            enabled: !hwPage.busy
                            onClicked: console.refreshSerialPorts()
                        }
                        Text {
                            text: "波特率 " + console.baudRate
                            color: theme.secondary
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        visible: hwPage.ports.length === 0
                        text: "未发现串口设备。"
                        color: theme.secondary
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: hwPage.ports
                        delegate: Rectangle {
                            id: portRow
                            width: hwColumn.width
                            height: 38
                            radius: 3
                            property string portName: modelData
                            property bool connected: console.serialConnected && console.linkedPort === portName
                            color: connected ? Qt.rgba(0.133, 0.827, 0.933, 0.10) : theme.panel
                            border.color: connected ? theme.accent : theme.border
                            border.width: 1
                            opacity: hwPage.busy ? 0.55 : 1.0

                            Rectangle {
                                width: 3
                                height: parent.height
                                color: connected ? theme.accent : "transparent"
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                text: portRow.portName
                                color: theme.text
                                font.pixelSize: 12
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 108
                                anchors.verticalCenter: parent.verticalCenter
                                text: portRow.connected
                                      ? (console.boardOnline ? "主板在线" : "未应答")
                                      : "未连接"
                                color: portRow.connected
                                       ? (console.boardOnline ? theme.okGreen : theme.warnYellow)
                                       : theme.secondary
                                font.pixelSize: 11
                            }

                            Rectangle {
                                id: baudBox
                                width: 82
                                height: 24
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 3
                                color: theme.bg
                                border.color: theme.border
                                border.width: 1
                                opacity: hwPage.busy ? 0.4 : 1.0
                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: console.baudRate
                                    color: theme.text
                                    font.pixelSize: 11
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "▾"
                                    color: theme.secondary
                                    font.pixelSize: 10
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !hwPage.busy
                                    onClicked: {
                                        if (hwPage.baudRow === index && hwPage.baudAnchor !== null) {
                                            hwPage.baudRow = -1;
                                            hwPage.baudAnchor = null;
                                        } else {
                                            hwPage.baudRow = index;
                                            hwPage.baudAnchor = baudBox;
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.left: parent.left
                                anchors.right: baudBox.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                enabled: !hwPage.busy
                                onDoubleClicked: hwPage.togglePort(portRow.portName)
                            }
                        }
                    }

                    Item { width: 1; height: 6 }
                    Rectangle { width: hwColumn.width; height: 1; color: theme.border }

                    Row {
                        spacing: 10
                        Text { text: "虚拟演奏"; color: theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        Rectangle {
                            width: 44
                            height: 22
                            radius: 11
                            opacity: hwPage.busy ? 0.4 : 1.0
                            color: console.virtualPlayback ? theme.accent : theme.border
                            Rectangle {
                                width: 16
                                height: 16
                                radius: 8
                                anchors.verticalCenter: parent.verticalCenter
                                x: console.virtualPlayback ? parent.width - 19 : 3
                                color: theme.bg
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !hwPage.busy
                                onClicked: console.setVirtualPlayback(!console.virtualPlayback)
                            }
                        }
                        Text {
                            text: "没有接主板时用它试听（不发声到硬件）"
                            color: theme.secondary
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        width: hwColumn.width
                        visible: console.lastError.length > 0
                        text: console.lastError
                        color: theme.errRed
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // 波特率弹层（页面级，避免被 Flickable 裁掉）
            Rectangle {
                id: baudPopup
                visible: hwPage.baudAnchor !== null
                z: 200
                width: 82
                height: baudCol.implicitHeight + 4
                color: theme.panel
                border.color: theme.border
                border.width: 1
                radius: 3
                x: hwPage.baudAnchor !== null
                   ? Math.min(Math.max(0, hwPage.baudAnchor.mapToItem(hwPage, 0, 0).x), hwPage.width - width)
                   : 0
                y: hwPage.baudAnchor !== null
                   ? hwPage.baudAnchor.mapToItem(hwPage, 0, 0).y + hwPage.baudAnchor.height + 2
                   : 0

                Column {
                    id: baudCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 2
                    Repeater {
                        model: ["9600", "19200", "38400", "57600", "115200", "230400"]
                        delegate: Rectangle {
                            width: baudCol.width
                            height: 22
                            radius: 2
                            color: baudItemMouse.containsMouse ? theme.panelHover : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: theme.text
                                font.pixelSize: 11
                            }
                            MouseArea {
                                id: baudItemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    console.setBaudRate(parseInt(modelData, 10));
                                    hwPage.baudRow = -1;
                                    hwPage.baudAnchor = null;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------- 键位映射
    Component {
        id: mappingPage
        Item {
            id: mapPage

            property bool cascadeMode: false
            property string errorText: ""
            property bool mappingLocked: win.locked || console.midiInputOpen
            property int noteRow: -1
            property int noteChannel: 0
            property int noteCurrent: 0
            property Item noteAnchor: null

            function noteLabel(n) {
                var names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
                return names[n % 12] + (Math.floor(n / 12) - 1);
            }

            Column {
                width: parent.width
                height: parent.height
                spacing: 10

                SectionLabel { text: "键位映射" }
                SubLabel {
                    width: parent.width
                    text: "每个通道对应一个音高。播放、录制或 MIDI 输入开启时不可修改。"
                }

                Row {
                    spacing: 8
                    PlainButton {
                        label: "单键模式"
                        highlighted: !mapPage.cascadeMode
                        enabled: !mapPage.mappingLocked
                        onClicked: mapPage.cascadeMode = false
                    }
                    PlainButton {
                        label: "跟随模式"
                        highlighted: mapPage.cascadeMode
                        enabled: !mapPage.mappingLocked
                        onClicked: mapPage.cascadeMode = true
                    }
                    Text {
                        text: mapPage.cascadeMode
                              ? "改一个通道，其后通道依次顺移"
                              : "只改当前通道"
                        color: theme.secondary
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    width: parent.width
                    visible: mapPage.errorText.length > 0
                    text: mapPage.errorText
                    color: theme.errRed
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    width: parent.width
                    height: 26
                    color: theme.panel
                    border.color: theme.border
                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 60
                        Text { text: "通道"; color: theme.secondary; font.pixelSize: 11; width: 90 }
                        Text { text: "音高"; color: theme.secondary; font.pixelSize: 11; width: 140 }
                        Text { text: "音高选择"; color: theme.secondary; font.pixelSize: 11 }
                    }
                }

                ListView {
                    id: mapList
                    width: parent.width
                    height: parent.height - 196
                    clip: true
                    model: console.mappingRows
                    opacity: mapPage.mappingLocked ? 0.5 : 1.0
                    onContentYChanged: {
                        mapPage.noteRow = -1;
                        mapPage.noteAnchor = null;
                    }

                    delegate: Rectangle {
                        width: mapList.width
                        height: 34
                        color: index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.02)

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 90
                            text: "CH " + (modelData.channel < 10 ? "0" : "") + modelData.channel
                            color: theme.secondary
                            font.pixelSize: 12
                            font.family: "Consolas"
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 100
                            anchors.verticalCenter: parent.verticalCenter
                            width: 140
                            text: modelData.noteName + "  (" + modelData.midiNote + ")"
                            color: theme.text
                            font.pixelSize: 12
                            font.family: "Consolas"
                        }

                        Rectangle {
                            id: noteBox
                            width: 132
                            height: 24
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 3
                            color: theme.bg
                            border.color: (mapPage.noteRow === index) ? theme.accent : theme.border
                            border.width: 1
                            opacity: mapPage.mappingLocked ? 0.4 : 1.0

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.noteName + "  (" + modelData.midiNote + ")"
                                color: theme.text
                                font.pixelSize: 11
                                font.family: "Consolas"
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: "▾"
                                color: theme.secondary
                                font.pixelSize: 10
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !mapPage.mappingLocked
                                onClicked: {
                                    if (mapPage.noteRow === index && mapPage.noteAnchor !== null) {
                                        mapPage.noteRow = -1;
                                        mapPage.noteAnchor = null;
                                    } else {
                                        mapPage.noteRow = index;
                                        mapPage.noteChannel = modelData.channel;
                                        mapPage.noteCurrent = modelData.midiNote;
                                        mapPage.noteAnchor = noteBox;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 音高选择弹层（页面级，避免被裁剪）
            Rectangle {
                id: notePopup
                visible: mapPage.noteAnchor !== null
                z: 200
                width: 190
                height: Math.min(300, noteList.contentHeight + 4)
                color: theme.panel
                border.color: theme.border
                border.width: 1
                radius: 3
                x: {
                    if (mapPage.noteAnchor === null) return 0;
                    var px = mapPage.noteAnchor.mapToItem(mapPage, 0, 0).x
                             + mapPage.noteAnchor.width - width;
                    return Math.max(0, Math.min(px, mapPage.width - width));
                }
                y: {
                    if (mapPage.noteAnchor === null) return 0;
                    var py = mapPage.noteAnchor.mapToItem(mapPage, 0, 0).y;
                    var below = py + mapPage.noteAnchor.height + 2;
                    if (below + height <= mapPage.height) return below;
                    return Math.max(0, py - height - 2);
                }

                ListView {
                    id: noteList
                    anchors.fill: parent
                    anchors.margins: 2
                    clip: true
                    model: 128
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: Rectangle {
                        width: noteList.width
                        height: 22
                        radius: 2
                        color: noteItemMouse.containsMouse
                               ? theme.panelHover
                               : (index === mapPage.noteCurrent
                                  ? Qt.rgba(0.133, 0.827, 0.933, 0.14) : "transparent")
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: mapPage.noteLabel(index) + "   " + index
                            color: index === mapPage.noteCurrent ? theme.accent : theme.text
                            font.pixelSize: 11
                            font.family: "Consolas"
                        }
                        MouseArea {
                            id: noteItemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                var err = console.setMappingNote(mapPage.noteChannel, index,
                                                                 mapPage.cascadeMode);
                                mapPage.errorText = err.length > 0 ? err : "";
                                if (err.length === 0) {
                                    mapPage.noteRow = -1;
                                    mapPage.noteAnchor = null;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------- 运行日志
    Component {
        id: logPage
        Item {
            id: logRoot
            Column {
                width: parent.width
                height: parent.height
                spacing: 12

                Row {
                    width: parent.width
                    spacing: 10
                    SectionLabel { text: "运行日志"; anchors.verticalCenter: parent.verticalCenter }
                    Item { width: parent.width - 220; height: 1 }
                    PlainButton {
                        label: "清空"
                        onClicked: console.clearLog()
                    }
                }

                Rectangle {
                    width: parent.width
                    height: parent.height - 60
                    color: theme.panel
                    border.color: theme.border
                    radius: 4
                    clip: true

                    ListView {
                        id: logList
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        model: console.logLines
                        spacing: 2
                        onCountChanged: positionViewAtEnd()

                        delegate: Text {
                            width: logList.width
                            text: modelData
                            color: theme.secondary
                            font.family: "Consolas"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: console.logLines.length === 0
                        text: "暂无日志"
                        color: theme.secondary
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------- 关于
    Component {
        id: aboutPage
        Column {
            spacing: 12
            SectionLabel { text: "Organ Console" }
            SubLabel { width: parent.width; text: "版本 0.2.0（接入版）" }
            SubLabel {
                width: parent.width
                text: "一款面向音管风琴控制台的前端：实时弹奏可视化、录制、OCF 播放与 MIDI 导入。\n\n"
                      + "内部统一使用 OCF 位图帧作为曲目数据；MIDI 只作为外部交换格式（导入转换、导出反推音高）。"
            }
            SubLabel {
                width: parent.width
                text: "通道数、起始音与串口等参数保存在工程目录的 setting.txt 中，界面只读通道数。"
            }
        }
    }
}

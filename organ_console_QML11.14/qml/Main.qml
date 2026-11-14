import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Organ 1.0
import "components"

ApplicationWindow {
    id: root
    visible: true
    width: 1440
    height: 900
    minimumWidth: 1120
    minimumHeight: 720
    title: "Organ Console · Control Studio"
    color: "#F3F7FC"

    property int selectedPage: 0
    property string toastText: ""
    property var recentLogs: []
    readonly property color navy: "#244B70"
    readonly property color navyDeep: "#183A5D"
    readonly property color sky: "#69B7E8"
    readonly property color paleSky: "#EAF5FC"
    readonly property color ink: "#18334E"
    readonly property color muted: "#74879A"
    readonly property color green: "#DDF3E8"
    readonly property color greenInk: "#28724E"
    readonly property color yellow: "#FFF2CF"
    readonly property color line: "#E2EAF2"
    readonly property var pageNames: ["控制总览", "实时 MIDI", "自动演奏", "直接控制", "键位映射", "OCF 转换", "设置", "运行日志"]
    readonly property var navItems: [
        { title: "控制总览", glyph: "⌂" },
        { title: "实时 MIDI", glyph: "♫" },
        { title: "自动演奏", glyph: "▶" },
        { title: "直接控制", glyph: "▦" },
        { title: "键位映射", glyph: "⌘" },
        { title: "OCF 转换", glyph: "⇄" },
        { title: "设置", glyph: "⚙" },
        { title: "运行日志", glyph: "☷" }
    ]

    function selectPage(index) {
        selectedPage = index
        const mode = index >= 1 && index <= 3 ? index - 1 : -1
        console.activateMode(mode)
    }

    function showToast(message) {
        toastText = message
        toastTimer.restart()
    }

    function formatMs(value) {
        const total = Math.max(0, Math.floor(Number(value)))
        const minutes = Math.floor(total / 60000)
        const seconds = Math.floor((total % 60000) / 1000)
        const millis = total % 1000
        return minutes.toString().padStart(2, "0") + ":"
                + seconds.toString().padStart(2, "0") + "."
                + millis.toString().padStart(3, "0")
    }

    Timer {
        id: toastTimer
        interval: 2800
        onTriggered: root.toastText = ""
    }

    function refreshRecentLogs() {
        const all = console.logLines
        const start = Math.max(0, all.length - 6)
        root.recentLogs = all.slice(start)
    }

    Connections {
        target: console
        function onConversionFinished(success, path, error) {
            if (success)
                root.showToast("OCF 已生成并载入：" + path.split(/[\\/]/).pop())
            else if (error.length > 0)
                root.showToast(error)
        }
        function onLogLinesChanged() {
            root.refreshRecentLogs()
        }
    }

    Component.onCompleted: root.refreshRecentLogs()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 236
            Layout.minimumWidth: 236
            Layout.fillHeight: true
            color: root.navyDeep

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.topMargin: 24
                anchors.bottomMargin: 20
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 34
                    spacing: 12
                    Rectangle {
                        width: 42
                        height: 42
                        radius: 14
                        color: "#4F9ED3"
                        Text {
                            anchors.centerIn: parent
                            text: "♫"
                            color: "white"
                            font.pixelSize: 24
                            font.weight: Font.Bold
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: "ORGAN CONSOLE"
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }
                        Text {
                            text: "CONTROL STUDIO"
                            color: "#AFC7DA"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            font.letterSpacing: 1.6
                        }
                    }
                }

                Text {
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 10
                    text: "工作区"
                    color: "#91ADC5"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: root.navItems
                    delegate: Button {
                        id: navButton
                        required property var modelData
                        required property int index
                        text: modelData.title
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        Layout.bottomMargin: 5
                        flat: true
                        Accessible.name: modelData.title
                        onClicked: root.selectPage(index)

                        background: Rectangle {
                            radius: 12
                            color: root.selectedPage === navButton.index ? "#315B80"
                                                                          : navButton.hovered ? "#284D70" : "transparent"
                            border.color: root.selectedPage === navButton.index ? "#4B86B3" : "transparent"
                            border.width: 1
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 13
                            anchors.rightMargin: 12
                            spacing: 12
                            Text {
                                text: navButton.modelData.glyph
                                color: root.selectedPage === navButton.index ? "#FFFFFF" : "#B7CBDC"
                                font.pixelSize: 19
                                Layout.preferredWidth: 22
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                text: navButton.modelData.title
                                color: root.selectedPage === navButton.index ? "#FFFFFF" : "#D1DEEA"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                font.weight: root.selectedPage === navButton.index ? Font.DemiBold : Font.Normal
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: root.sky
                                visible: root.selectedPage === navButton.index
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 94
                    radius: 14
                    color: "#21486B"
                    border.color: "#39668B"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: console.serialConnected ? "#73D7A8" : "#F4C66A"
                            }
                            Text {
                                text: console.serialConnected ? "主板已连接" : "主板未连接"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            text: console.serialConnected ? console.linkedPort + "  ·  " + console.baudRate : "插入主板后自动连接"
                            color: "#B7CCDD"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                Text {
                    Layout.topMargin: 15
                    Layout.leftMargin: 4
                    text: "v1.0  ·  OCF / 32-BIT"
                    color: "#89A5BD"
                    font.family: "Segoe UI"
                    font.pixelSize: 9
                    font.letterSpacing: 1.1
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#F3F7FC"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 82
                    color: "#FFFFFF"
                    border.color: root.line
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 28
                        anchors.rightMargin: 28
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: root.pageNames[root.selectedPage]
                                color: root.ink
                                font.family: "Segoe UI"
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: "ORGAN CONSOLE  /  " + root.pageNames[root.selectedPage].toUpperCase()
                                color: root.muted
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                font.letterSpacing: 1.35
                            }
                        }

                        Text {
                            text: console.serialConnected ? "已连接 " + console.linkedPort : "正在查找主板"
                            color: root.muted
                            font.pixelSize: 12
                        }
                        Rectangle {
                            radius: 14
                            color: console.serialConnected ? root.green : root.yellow
                            Layout.preferredWidth: 94
                            Layout.preferredHeight: 34
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 7
                                Rectangle {
                                    width: 7
                                    height: 7
                                    radius: 4
                                    color: console.serialConnected ? "#48A878" : "#D49A2E"
                                }
                                Text {
                                    text: console.serialConnected ? "ONLINE" : "OFFLINE"
                                    color: console.serialConnected ? root.greenInk : "#936B1D"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    font.letterSpacing: 0.7
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    id: pageScroller
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    StackLayout {
                        id: pageStack
                        width: pageScroller.availableWidth
                        // StackLayout 没有 currentItem（那是 StackView 的），用 children[currentIndex]。
                        height: {
                            const item = (currentIndex >= 0 && currentIndex < children.length)
                                          ? children[currentIndex] : null
                            if (!item)
                                return 0
                            const h = item.implicitHeight
                            if (root.selectedPage === 2)
                                return Math.max(h, pageScroller.height)
                            return h
                        }
                        currentIndex: root.selectedPage

                        // Overview
                        Item {
                            implicitHeight: overviewBody.implicitHeight + 56
                            ColumnLayout {
                                id: overviewBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 18
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 228
                                    radius: 22
                                    clip: true
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#EAF6FD" }
                                        GradientStop { position: 1.0; color: "#D8ECF8" }
                                    }
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 28
                                        anchors.rightMargin: 24
                                        anchors.topMargin: 22
                                        anchors.bottomMargin: 20
                                        spacing: 24
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 10
                                            Text {
                                                text: "MIDI CONTROL  ·  LIVE & PLAYBACK"
                                                color: "#4E88B3"
                                                font.family: "Segoe UI"
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                                font.letterSpacing: 1.8
                                            }
                                            Text {
                                                text: "让每个音符，\n准确到达。"
                                                color: root.navyDeep
                                                font.family: "Segoe UI"
                                                font.pixelSize: 31
                                                font.weight: Font.DemiBold
                                                lineHeight: 1.05
                                            }
                                            Text {
                                                text: "统一管理 MIDI 输入、位图流回放与键位映射。"
                                                color: "#54728D"
                                                font.family: "Segoe UI"
                                                font.pixelSize: 13
                                            }
                                            RowLayout {
                                                spacing: 10
                                                ActionButton {
                                                    text: "打开自动演奏"
                                                    Layout.preferredWidth: 142
                                                    fill: root.navy
                                                    onClicked: root.selectPage(2)
                                                }
                                                ActionButton {
                                                    text: "查看键位映射"
                                                    Layout.preferredWidth: 136
                                                    fill: "#FFFFFF"
                                                    ink: root.navy
                                                    outline: "#BFD7E8"
                                                    onClicked: root.selectPage(4)
                                                }
                                            }
                                        }
                                        Item {
                                            Layout.preferredWidth: 310
                                            Layout.fillHeight: true
                                            Rectangle {
                                                anchors.right: parent.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 265
                                                height: 170
                                                radius: 24
                                                color: "#C5E2F3"
                                            }
                                            Row {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                anchors.bottom: parent.bottom
                                                anchors.bottomMargin: 9
                                                spacing: 9
                                                Repeater {
                                                    model: [76, 126, 94, 151, 110, 138, 82, 120]
                                                    delegate: Rectangle {
                                                        required property int modelData
                                                        required property int index
                                                        width: 19
                                                        height: modelData
                                                        radius: 9
                                                        color: index % 3 === 0 ? "#4F93C4" : "#2B5A81"
                                                        opacity: 0.92
                                                        anchors.bottom: parent.bottom
                                                    }
                                                }
                                            }
                                            Rectangle {
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.rightMargin: 18
                                                anchors.topMargin: 8
                                                width: 38
                                                height: 38
                                                radius: 19
                                                color: "#FFF2CF"
                                                Text { anchors.centerIn: parent; text: "♪"; color: "#9B762B"; font.pixelSize: 21 }
                                            }
                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.bottom: parent.bottom
                                                anchors.leftMargin: 20
                                                width: 30
                                                height: 30
                                                radius: 15
                                                color: "#DDF3E8"
                                                Text { anchors.centerIn: parent; text: "•"; color: "#388B61"; font.pixelSize: 24 }
                                            }
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 3
                                    columnSpacing: 16
                                    rowSpacing: 16
                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 124
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { text: "◉"; color: root.sky; font.pixelSize: 23 }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text { text: "串口链路"; color: root.muted; font.pixelSize: 11 }
                                                Text {
                                                    text: console.serialConnected ? "已连接" : "等待连接"
                                                    color: root.ink; font.pixelSize: 19; font.weight: Font.DemiBold
                                                }
                                            }
                                            Rectangle {
                                                width: 10; height: 10; radius: 5
                                                color: console.serialConnected ? "#58B783" : "#E5B044"
                                            }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: console.serialConnected ? "ST 位图帧可发送" : "插入主板后自动连接"
                                            color: root.muted; font.pixelSize: 10
                                        }
                                    }
                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 124
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { text: "♫"; color: "#64A782"; font.pixelSize: 23 }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text { text: "MIDI 输入"; color: root.muted; font.pixelSize: 11 }
                                                Text {
                                                    text: console.midiInputOpen ? "设备已打开" : "未启动"
                                                    color: root.ink; font.pixelSize: 19; font.weight: Font.DemiBold
                                                }
                                            }
                                            Rectangle { width: 10; height: 10; radius: 5; color: console.midiInputOpen ? "#58B783" : "#C8D4DF" }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: console.midiInputDevices.length > 0 ? console.midiInputDevices.length + " 个输入设备可用" : "连接 USB MIDI 键盘后刷新设备"
                                            color: root.muted; font.pixelSize: 10
                                        }
                                    }
                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 124
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { text: "▤"; color: "#C79A3C"; font.pixelSize: 23 }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text { text: "当前曲目"; color: root.muted; font.pixelSize: 11 }
                                                Text {
                                                    text: console.currentFile.length > 0 ? console.currentFile.split(/[\\/]/).pop() : "尚未载入"
                                                    color: root.ink; font.pixelSize: 16; font.weight: Font.DemiBold
                                                    elide: Text.ElideMiddle; Layout.fillWidth: true
                                                }
                                            }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: console.currentFile.length > 0 ? root.formatMs(console.durationMs) + " 总时长" : "支持 MIDI 与 OCF 位图流"
                                            color: root.muted; font.pixelSize: 10
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 250
                                    spacing: 16
                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.preferredWidth: 2
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { text: "最近活动"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                            Item { Layout.fillWidth: true }
                                            ActionButton {
                                                text: "全部日志 →"
                                                fill: "#EDF5FB"; ink: root.navy
                                                implicitHeight: 34; Layout.preferredWidth: 98
                                                onClicked: root.selectPage(7)
                                            }
                                        }
                                        ListView {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            spacing: 6
                                            model: root.recentLogs
                                            delegate: RowLayout {
                                                required property string modelData
                                                required property int index
                                                width: ListView.view.width
                                                height: 25
                                                Rectangle { width: 6; height: 6; radius: 3; color: index === 0 ? root.sky : "#B9CBD9" }
                                                Text { text: modelData; color: root.muted; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.preferredWidth: 1
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 38
                                            radius: 11
                                            color: root.paleSky
                                            RowLayout {
                                                anchors.fill: parent; anchors.leftMargin: 12; spacing: 8
                                                Text { text: "✦"; color: "#4F91C0"; font.pixelSize: 16 }
                                                Text { text: "快速开始"; color: root.navy; font.pixelSize: 12; font.weight: Font.DemiBold }
                                            }
                                        }
                                        Text { text: "1  插入主板，等待自动连接"; color: root.ink; font.pixelSize: 12 }
                                        Text { text: "2  载入 MIDI 或 OCF"; color: root.ink; font.pixelSize: 12 }
                                        Text { text: "3  播放并观察位图回报"; color: root.ink; font.pixelSize: 12 }
                                        ActionButton {
                                            text: "选择演奏文件"
                                            fill: root.navy
                                            Layout.fillWidth: true
                                            onClicked: console.chooseControlFile()
                                        }
                                    }
                                }
                            }
                        }

                        // Realtime MIDI
                        Item {
                            implicitHeight: realtimeBody.implicitHeight + 56
                            ColumnLayout {
                                id: realtimeBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 18
                                Text { text: "把外接键盘接入控制链路"; color: root.ink; font.pixelSize: 23; font.weight: Font.DemiBold }
                                Text { text: "选择系统 MIDI 输入设备；音高按键位映射表转换为通道状态。"; color: root.muted; font.pixelSize: 12 }
                                RowLayout {
                                    Layout.fillWidth: true; Layout.preferredHeight: 280; spacing: 16
                                    SurfaceCard {
                                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 3
                                        Text { text: "输入设备"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                        Text { text: "Windows MIDI / winmm"; color: root.muted; font.pixelSize: 11 }
                                        ComboBox {
                                            id: midiInputSelect
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 46
                                            model: console.midiInputDevices
                                            displayText: currentText.length > 0 ? currentText : "未发现 MIDI 输入设备"
                                            enabled: !console.midiInputOpen
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true; spacing: 10
                                            ActionButton {
                                                text: "刷新设备"
                                                fill: "#EDF5FB"; ink: root.navy
                                                Layout.preferredWidth: 115
                                                onClicked: console.refreshMidiInputs()
                                            }
                                            ActionButton {
                                                text: console.midiInputOpen ? "关闭输入" : "打开输入"
                                                Layout.fillWidth: true
                                                fill: console.midiInputOpen ? "#EAF4EE" : root.navy
                                                ink: console.midiInputOpen ? root.greenInk : "#FFFFFF"
                                                onClicked: console.toggleMidiInput(midiInputSelect.currentIndex)
                                            }
                                        }
                                    }
                                    SurfaceCard {
                                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 2
                                        Text { text: "信号状态"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                        Rectangle {
                                            Layout.fillWidth: true; Layout.preferredHeight: 104
                                            radius: 14; color: console.midiInputOpen ? root.green : root.paleSky
                                            RowLayout {
                                                anchors.fill: parent; anchors.margins: 17; spacing: 12
                                                Rectangle { width: 36; height: 36; radius: 18; color: "#FFFFFF"; Text { anchors.centerIn: parent; text: "♫"; color: root.navy; font.pixelSize: 19 } }
                                                ColumnLayout {
                                                    Layout.fillWidth: true; spacing: 4
                                                    Text { text: console.midiInputOpen ? "正在监听 MIDI" : "等待输入设备"; color: root.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                                                    Text { text: console.serialConnected ? "串口链路已就绪" : "插入主板后会自动连接"; color: root.muted; font.pixelSize: 10 }
                                                }
                                                Rectangle { width: 9; height: 9; radius: 5; color: console.midiInputOpen ? "#48A878" : "#B8C7D4" }
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true; Layout.preferredHeight: 108
                                            radius: 14; color: "#F8FAFC"; border.color: root.line
                                            ColumnLayout {
                                                anchors.fill: parent; anchors.margins: 16; spacing: 7
                                                Text { text: "当前映射摘要"; color: root.muted; font.pixelSize: 10; font.weight: Font.DemiBold }
                                                Text { text: console.mappingRows.length > 0 ? ("通道 1  ·  MIDI " + console.mappingRows[0].midiNote + "  ·  " + console.mappingRows[0].noteName) : ""; color: root.ink; font.pixelSize: 12 }
                                                Text { text: console.mappingRows.length > 0 ? ("通道 " + console.channelCount + "  ·  MIDI " + console.mappingRows[console.mappingRows.length - 1].midiNote + "  ·  " + console.mappingRows[console.mappingRows.length - 1].noteName) : ""; color: root.ink; font.pixelSize: 12 }
                                            }
                                        }
                                    }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 92
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout {
                                            Layout.fillWidth: true; spacing: 3
                                            Text { text: "录制演奏"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                            Text { text: console.recording ? "正在记录。停止后保存为 MIDI。" : "实时 MIDI 与直接按键都会写入。"; color: root.muted; font.pixelSize: 10 }
                                        }
                                        Rectangle {
                                            radius: 10; color: console.recording ? "#F8D7D7" : root.paleSky
                                            Layout.preferredWidth: 92; Layout.preferredHeight: 28
                                            Text { anchors.centerIn: parent; text: console.recording ? "REC" : "IDLE"; color: console.recording ? "#9B3B3B" : root.navy; font.pixelSize: 10; font.weight: Font.Bold }
                                        }
                                        ActionButton {
                                            text: console.recording ? "停止并保存" : "开始录制"
                                            fill: console.recording ? "#F4D2D2" : root.navy
                                            ink: console.recording ? "#8E3030" : "#FFFFFF"
                                            Layout.preferredWidth: 140
                                            onClicked: {
                                                const message = console.toggleRecording()
                                                if (message.length > 0)
                                                    root.showToast(message)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Autoplay：不用 ColumnLayout fillHeight，避免在 ScrollView/StackLayout 里被压成 0。
                        Item {
                            id: autoPage
                            // 宽度跟 StackLayout，高度 = 顶栏 + 控制卡 + 瀑布流
                            width: pageStack.width
                            implicitWidth: width
                            implicitHeight: waterfallFrame.y + waterfallFrame.height + 28

                            readonly property real contentW: Math.max(200, width - 56)

                            RowLayout {
                                id: autoHeader
                                x: 28
                                y: 28
                                width: autoPage.contentW
                                height: 48
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        text: "自动演奏"
                                        color: root.ink
                                        font.pixelSize: 22
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: "载入 MIDI 或 OCF 后按时间轴播放。"
                                        color: root.muted
                                        font.pixelSize: 11
                                    }
                                }
                                CheckBox {
                                    id: virtualPlayBox
                                    text: "虚拟演奏"
                                    checked: console.virtualPlayback
                                    onToggled: console.setVirtualPlayback(checked)
                                    contentItem: Text {
                                        text: virtualPlayBox.text
                                        color: root.navy
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        leftPadding: virtualPlayBox.indicator.width + 8
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    indicator: Rectangle {
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        x: virtualPlayBox.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: virtualPlayBox.checked ? root.navy : "#FFFFFF"
                                        border.color: virtualPlayBox.checked ? root.navy : "#C5D4E2"
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            color: "#FFFFFF"
                                            font.pixelSize: 11
                                            font.weight: Font.Bold
                                            visible: virtualPlayBox.checked
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: "勾选后可不连主板串口，仅预览瀑布流演奏"
                                    ToolTip.delay: 400
                                }
                                ActionButton {
                                    text: "打开 MIDI / OCF"
                                    fill: "#EDF5FB"
                                    ink: root.navy
                                    Layout.preferredWidth: 150
                                    onClicked: console.chooseControlFile()
                                }
                            }

                            Rectangle {
                                id: autoControls
                                x: 28
                                y: autoHeader.y + autoHeader.height + 16
                                width: autoPage.contentW
                                height: 235
                                color: "#FFFFFF"
                                radius: 18
                                border.color: "#E1EAF3"
                                border.width: 1
                                clip: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 20
                                    spacing: 12

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: "当前曲目"; color: root.muted; font.pixelSize: 11 }
                                        Item { Layout.fillWidth: true }
                                        Rectangle {
                                            radius: 10
                                            color: console.currentFile.toLowerCase().endsWith(".ocf") ? root.green : root.paleSky
                                            Layout.preferredHeight: 28
                                            Layout.preferredWidth: 82
                                            Text {
                                                anchors.centerIn: parent
                                                text: console.currentFile.toLowerCase().endsWith(".ocf") ? "OCF STREAM" : "MIDI FILE"
                                                color: console.currentFile.toLowerCase().endsWith(".ocf") ? root.greenInk : root.navy
                                                font.pixelSize: 9
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: console.currentFile.length > 0
                                              ? console.currentFile.split(/[\\/]/).pop()
                                              : "尚未载入文件"
                                        color: root.ink
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideMiddle
                                    }
                                    ProgressBar {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: Math.max(1, console.durationMs)
                                        value: console.positionMs
                                        background: Rectangle { implicitHeight: 7; radius: 4; color: "#E8EFF5" }
                                        contentItem: Item {
                                            Rectangle {
                                                width: parent.width * (console.durationMs > 0 ? console.positionMs / console.durationMs : 0)
                                                height: 7
                                                radius: 4
                                                color: root.sky
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10
                                        ActionButton { text: "▶  播放"; fill: root.navy; Layout.preferredWidth: 112; onClicked: console.play() }
                                        ActionButton { text: "Ⅱ  暂停"; fill: "#EDF5FB"; ink: root.navy; Layout.preferredWidth: 112; onClicked: console.pause() }
                                        ActionButton { text: "■  停止"; fill: "#F2F5F8"; ink: root.navy; Layout.preferredWidth: 112; onClicked: console.stop() }
                                        Item { Layout.fillWidth: true }
                                        Text {
                                            text: root.formatMs(console.positionMs) + "  /  " + root.formatMs(console.durationMs)
                                            color: root.muted
                                            font.family: "Consolas"
                                            font.pixelSize: 11
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Text { text: "速度"; color: root.muted; font.pixelSize: 11 }
                                        Slider {
                                            Layout.fillWidth: true
                                            from: 25
                                            to: 200
                                            stepSize: 1
                                            value: console.tempoPercent
                                            onMoved: console.setTempo(Math.round(value))
                                        }
                                        Text {
                                            text: (console.tempoPercent / 100).toFixed(2) + "×"
                                            color: root.navy
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            Layout.preferredWidth: 46
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: waterfallFrame
                                x: 28
                                y: autoControls.y + autoControls.height + 16
                                width: autoPage.contentW
                                height: Math.max(320, Math.max(0, pageScroller.height - y - 28))
                                color: "#E7F1F8"
                                radius: 18
                                border.color: "#D5E4EF"
                                border.width: 1
                                clip: true

                                WaterfallItem {
                                    id: autoWaterfall
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    notes: console.waterfallNotes
                                    channelCount: console.channelCount
                                    playing: console.playing
                                    positionMs: console.positionMs
                                }

                                Timer {
                                    interval: 16
                                    running: console.playing
                                    repeat: true
                                    onTriggered: autoWaterfall.positionMs = console.livePositionMs()
                                }
                                Connections {
                                    target: console
                                    function onPlaybackPositionChanged() { autoWaterfall.positionMs = console.livePositionMs() }
                                    function onPlaybackStateChanged() { autoWaterfall.positionMs = console.livePositionMs() }
                                    function onFileStateChanged() { autoWaterfall.positionMs = console.livePositionMs() }
                                }
                            }
                        }

                        // Direct control
                        Item {
                            implicitHeight: directBody.implicitHeight + 56
                            ColumnLayout {
                                id: directBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 18
                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 3
                                        Text { text: "直接控制"; color: root.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                        Text { text: "按住键位发送 ON，松开后发送 OFF。"; color: root.muted; font.pixelSize: 11 }
                                    }
                                    ActionButton { text: "ALL OFF"; fill: "#FFF2CF"; ink: "#8F6B20"; Layout.preferredWidth: 110; onClicked: console.allOff() }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: "通道面板"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                        Item { Layout.fillWidth: true }
                                        Text { text: "当前 " + console.channelCount + " 键  ·  N1–N4 每板 8 路"; color: root.muted; font.pixelSize: 10 }
                                    }
                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: console.channelCount > 16 ? 8 : 4; rowSpacing: 12; columnSpacing: 12
                                        Repeater {
                                            model: console.channelCount
                                            delegate: ActionButton {
                                                required property int index
                                                Layout.fillWidth: true
                                                Layout.fillHeight: false
                                                Layout.preferredHeight: 92
                                                implicitHeight: 92
                                                text: "KEY  " + (index + 1) + "\nBOARD " + (index < 8 ? "N1" : index < 16 ? "N2" : index < 24 ? "N3" : "N4")
                                                fill: down ? "#DDF3E8" : "#F8FBFD"
                                                ink: down ? root.greenInk : root.navy
                                                outline: down ? "#86C9A5" : "#DCE7F0"
                                                cornerRadius: 16
                                                onPressed: console.pressChannel(index + 1)
                                                onReleased: console.releaseChannel(index + 1)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Mapping
                        Item {
                            implicitHeight: mapBody.implicitHeight + 56
                            ColumnLayout {
                                id: mapBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 16
                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 3
                                        Text { text: "键位映射"; color: root.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                        Text { text: "实时 MIDI 与 MIDI 自动播放共用这张音高表。"; color: root.muted; font.pixelSize: 11 }
                                    }
                                    ColumnLayout {
                                        spacing: 4
                                        Text { text: "键位数"; color: root.muted; font.pixelSize: 10 }
                                        SpinBox {
                                            from: 1; to: 32; editable: true
                                            value: console.channelCount
                                            onValueModified: {
                                                const error = console.setChannelCount(value)
                                                if (error.length > 0) {
                                                    root.showToast(error)
                                                    value = console.channelCount
                                                }
                                            }
                                        }
                                    }
                                    ActionButton {
                                        text: "恢复默认半音阶"
                                        fill: "#EDF5FB"; ink: root.navy; Layout.preferredWidth: 158
                                        onClicked: console.resetMapping()
                                    }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        Text { text: "通道"; color: root.muted; font.pixelSize: 10; Layout.preferredWidth: 100 }
                                        Text { text: "控制板"; color: root.muted; font.pixelSize: 10; Layout.preferredWidth: 110 }
                                        Text { text: "MIDI 音高"; color: root.muted; font.pixelSize: 10; Layout.preferredWidth: 170 }
                                        Text { text: "音名"; color: root.muted; font.pixelSize: 10; Layout.fillWidth: true }
                                    }
                                    // 固定行高自上而下排列；页面高度不够时由外层 ScrollView 滚动。
                                    ListView {
                                        id: mappingList
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Math.max(console.channelCount, 1) * 44
                                        clip: true
                                        interactive: false
                                        spacing: 6
                                        model: console.mappingRows
                                        delegate: Item {
                                            required property var modelData
                                            width: ListView.view ? ListView.view.width : 0
                                            height: 38
                                            RowLayout {
                                                anchors.fill: parent
                                                spacing: 12
                                                Rectangle {
                                                    Layout.preferredWidth: 100
                                                    Layout.preferredHeight: 29
                                                    radius: 9
                                                    color: root.paleSky
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "通道 " + modelData.channel
                                                        color: root.navy
                                                        font.pixelSize: 10
                                                        font.weight: Font.DemiBold
                                                    }
                                                }
                                                Rectangle {
                                                    Layout.preferredWidth: 110
                                                    Layout.preferredHeight: 29
                                                    radius: 9
                                                    color: modelData.board === "N1" ? "#E8F3FA" : "#EEF0F8"
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: modelData.board
                                                        color: root.navy
                                                        font.pixelSize: 10
                                                        font.weight: Font.DemiBold
                                                    }
                                                }
                                                SpinBox {
                                                    Layout.preferredWidth: 150
                                                    from: 0
                                                    to: 127
                                                    editable: true
                                                    value: modelData.midiNote
                                                    onValueModified: {
                                                        const error = console.setMappingNote(modelData.channel, value)
                                                        if (error.length > 0) {
                                                            root.showToast(error)
                                                            value = modelData.midiNote
                                                        }
                                                    }
                                                }
                                                Text {
                                                    text: modelData.noteName
                                                    color: root.ink
                                                    font.family: "Segoe UI"
                                                    font.pixelSize: 12
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // OCF conversion
                        Item {
                            implicitHeight: ocfBody.implicitHeight + 56
                            ColumnLayout {
                                id: ocfBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 3
                                    Text { text: "MIDI → OCF 转换"; color: root.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                    Text { text: "按聚合窗把 MIDI 收成位图流。窗口静默后输出完整快照，回放不再二次聚合。"; color: root.muted; font.pixelSize: 11 }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true; Layout.preferredHeight: 250
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: "转换参数"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                        Item { Layout.fillWidth: true }
                                        Rectangle {
                                            radius: 10; color: "#FFF5DB"; Layout.preferredHeight: 28; Layout.preferredWidth: 100
                                            Text { anchors.centerIn: parent; text: console.converting ? "CONVERTING" : "32-BIT OCF"; color: "#9A7527"; font.pixelSize: 9; font.weight: Font.Bold }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 12
                                        ColumnLayout {
                                            Layout.fillWidth: true; spacing: 6
                                            Text { text: "映射方式"; color: root.muted; font.pixelSize: 10 }
                                            ComboBox { id: mappingMode; Layout.fillWidth: true; model: ["范围外忽略", "取余循环"] }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 140; spacing: 6
                                            Text { text: "起始音高"; color: root.muted; font.pixelSize: 10 }
                                            SpinBox { id: rangeStart; from: 0; to: 127; value: 60; editable: true; Layout.fillWidth: true }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 140; spacing: 6
                                            Text { text: "结束音高（含）"; color: root.muted; font.pixelSize: 10 }
                                            SpinBox { id: rangeEnd; from: 0; to: 127; value: 91; editable: true; Layout.fillWidth: true }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 145; spacing: 6
                                            Text { text: "聚合窗（ms）"; color: root.muted; font.pixelSize: 10 }
                                            SpinBox { id: aggregationWindow; from: 1; to: 1000; value: 30; editable: true; Layout.fillWidth: true }
                                        }
                                    }
                                    Text { text: "范围首尾都包含，跨度不能超过当前键位数 " + console.channelCount + "。"; color: root.muted; font.pixelSize: 10 }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 10
                                        Item { Layout.fillWidth: true }
                                        ActionButton {
                                            text: console.converting ? "正在转换…" : "选择 MIDI 并转换"
                                            fill: root.navy; Layout.preferredWidth: 172
                                            enabled: !console.converting
                                            onClicked: console.convertMidiFile(mappingMode.currentIndex === 0 ? "range" : "wrap",
                                                                                rangeStart.value, rangeEnd.value,
                                                                                aggregationWindow.value)
                                        }
                                        ActionButton {
                                            text: "查看 OCF 文本"
                                            fill: root.paleSky; ink: root.navy; Layout.preferredWidth: 148
                                            onClicked: {
                                                const dump = console.readOcfText("")
                                                if (dump.length === 0)
                                                    return
                                                if (dump.startsWith("OCF 解析失败")) {
                                                    root.showToast(dump)
                                                    return
                                                }
                                                ocfText.text = dump
                                                ocfDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Settings
                        Item {
                            implicitHeight: settingsBody.implicitHeight + 56
                            ColumnLayout {
                                id: settingsBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 3
                                    Text { text: "设置"; color: root.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                    Text { text: "连接参数和录制参数。插入主板后按这里的波特率自动握手。"; color: root.muted; font.pixelSize: 11 }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true; Layout.preferredHeight: 150
                                    Text { text: "串口"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                    Text { text: console.serialConnected ? ("当前主板 " + console.linkedPort) : "尚未发现主板"; color: root.muted; font.pixelSize: 11 }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 12
                                        Text { text: "波特率"; color: root.muted; font.pixelSize: 11 }
                                        ComboBox {
                                            id: baudSetting
                                            Layout.preferredWidth: 160
                                            model: ["115200", "57600", "9600", "921600"]
                                            currentIndex: Math.max(0, model.indexOf(String(console.baudRate)))
                                            onActivated: console.setBaudRate(Number(currentText))
                                        }
                                    }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true; Layout.preferredHeight: 170
                                    Text { text: "录制"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                    Text { text: "同一聚合窗内的音写成同时按下。同一键短于消抖时间的来回抖动会被丢掉。"; color: root.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 18
                                        ColumnLayout {
                                            Layout.preferredWidth: 180; spacing: 6
                                            Text { text: "聚合窗（ms）"; color: root.muted; font.pixelSize: 10 }
                                            SpinBox {
                                                from: 1; to: 100; value: console.recordWindowMs; editable: true; Layout.fillWidth: true
                                                onValueModified: console.setRecordWindowMs(value)
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 180; spacing: 6
                                            Text { text: "消抖（ms）"; color: root.muted; font.pixelSize: 10 }
                                            SpinBox {
                                                from: 0; to: 50; value: console.recordDebounceMs; editable: true; Layout.fillWidth: true
                                                onValueModified: console.setRecordDebounceMs(value)
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                    }
                                }
                            }
                        }

                        // Logs
                        Item {
                            implicitHeight: logBody.implicitHeight + 56
                            ColumnLayout {
                                id: logBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 16
                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 3
                                        Text { text: "运行日志"; color: root.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                        Text { text: "串口、MIDI、转换与位图帧活动。"; color: root.muted; font.pixelSize: 11 }
                                    }
                                    Rectangle { radius: 10; color: root.paleSky; Layout.preferredWidth: 90; Layout.preferredHeight: 30; Text { anchors.centerIn: parent; text: console.logLines.length + " 条记录"; color: root.navy; font.pixelSize: 10; font.weight: Font.DemiBold } }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 560
                                    ListView {
                                        id: logList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.preferredHeight: 500
                                        clip: true; spacing: 2
                                        model: console.logLines
                                        delegate: Rectangle {
                                            required property string modelData
                                            required property int index
                                            width: ListView.view.width
                                            height: 36
                                            radius: 8
                                            color: index % 2 === 0 ? "#F8FAFC" : "#FFFFFF"
                                            RowLayout {
                                                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10
                                                Rectangle { width: 5; height: 5; radius: 3; color: modelData.indexOf("错误") >= 0 ? "#D98770" : root.sky }
                                                Text { text: modelData; color: root.ink; font.family: "Consolas"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                            }
                            Connections {
                                target: console
                                function onLogLinesChanged() {
                                    if (root.selectedPage === 7)
                                        logList.positionViewAtEnd()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: ocfDialog
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 80, 920)
        height: Math.min(root.height - 80, 650)
        title: "OCF 文本解析"
        standardButtons: Dialog.Close
        background: Rectangle { color: "#FFFFFF"; radius: 18; border.color: root.line }
        contentItem: ScrollView {
            clip: true
            TextArea {
                id: ocfText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.NoWrap
                textFormat: TextEdit.PlainText
                font.family: "Consolas"
                font.pixelSize: 11
                color: root.ink
            }
        }
    }

    Rectangle {
        visible: root.toastText.length > 0
        z: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(parent.width - 80, toastLabel.implicitWidth + 36)
        height: Math.max(48, toastLabel.implicitHeight + 22)
        radius: 14
        color: root.navyDeep
        opacity: 0.97
        Text {
            id: toastLabel
            anchors.fill: parent
            anchors.margins: 16
            text: root.toastText
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}

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

    Connections {
        target: console
        function onConversionFinished(success, path, error) {
            if (success)
                root.showToast("OCF 已生成并载入：" + path.split(/[\\/]/).pop())
            else if (error.length > 0)
                root.showToast(error)
        }
    }

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
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }
                        Text {
                            text: "CONTROL STUDIO"
                            color: "#AFC7DA"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.5
                        }
                    }
                }

                Text {
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 10
                    text: "工作区"
                    color: "#A8C2D8"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }

                Repeater {
                    model: root.navItems
                    delegate: Button {
                        id: navButton
                        required property var modelData
                        required property int index
                        text: modelData.title
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
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
                                color: root.selectedPage === navButton.index ? "#FFFFFF" : "#C4D7E6"
                                font.pixelSize: 21
                                Layout.preferredWidth: 24
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                text: navButton.modelData.title
                                color: root.selectedPage === navButton.index ? "#FFFFFF" : "#DCE8F2"
                                font.family: "Segoe UI"
                                font.pixelSize: 15
                                font.weight: root.selectedPage === navButton.index ? Font.Bold : Font.DemiBold
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
                                font.pixelSize: 13
                                font.weight: Font.Bold
                            }
                        }
                        Text {
                            text: console.serialConnected ? console.linkedPort + "  ·  " + console.baudRate : "插入主板后自动连接"
                            color: "#C4D8E8"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                Text {
                    Layout.topMargin: 15
                    Layout.leftMargin: 4
                    text: "v1.0  ·  OCF / 32-BIT"
                    color: "#9FB8CD"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.letterSpacing: 1.0
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
                        OverviewPage {
                            appRoot: root
                        }

                        // Realtime MIDI
                        Item {
                            id: realtimePage
                            readonly property var midiEvent: console.lastMidiEvent
                            readonly property bool hasMidiEvent: midiEvent
                                && midiEvent.note !== undefined
                            readonly property int activeCount: {
                                let count = 0
                                for (let i = 0; i < console.channelCount; ++i) {
                                    if (((console.activeChannels >>> i) & 1) === 1)
                                        ++count
                                }
                                return count
                            }
                            implicitHeight: realtimeBody.implicitHeight + 56

                            ColumnLayout {
                                id: realtimeBody
                                width: parent.width
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 28
                                spacing: 16

                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        Text { text: "实时 MIDI"; color: root.ink; font.pixelSize: 24; font.weight: Font.DemiBold }
                                        Text { text: "把外接键盘接入控制链路，映射结果与通道状态会实时反馈。"; color: root.muted; font.pixelSize: 12 }
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: 128
                                        Layout.preferredHeight: 36
                                        radius: 18
                                        color: console.midiInputOpen ? "#E5F6EE" : "#EEF4F9"
                                        RowLayout {
                                            anchors.centerIn: parent
                                            spacing: 7
                                            Rectangle {
                                                width: 7; height: 7; radius: 4
                                                color: console.midiInputOpen ? "#48A878" : "#B8C7D4"
                                            }
                                            Text {
                                                text: console.midiInputOpen ? "MIDI LIVE" : "MIDI IDLE"
                                                color: console.midiInputOpen ? "#28724E" : "#5D7488"
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                                font.letterSpacing: 0.7
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 300
                                    spacing: 16

                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.preferredWidth: 3

                                        RowLayout {
                                            Layout.fillWidth: true
                                            ColumnLayout {
                                                spacing: 3
                                                Text { text: "输入设备"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                                                Text { text: "Windows MIDI / winmm"; color: root.muted; font.pixelSize: 11 }
                                            }
                                            Item { Layout.fillWidth: true }
                                            Rectangle {
                                                Layout.preferredWidth: 86; Layout.preferredHeight: 26; radius: 13
                                                color: "#EDF5FB"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: console.midiInputDevices.length + " DEV"
                                                    color: "#4F7C9E"
                                                    font.pixelSize: 9
                                                    font.weight: Font.Bold
                                                    font.letterSpacing: 0.5
                                                }
                                            }
                                        }

                                        AppComboBox {
                                            id: midiInputSelect
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 50
                                            font.pixelSize: 14
                                            model: console.midiInputDevices
                                            displayText: currentText.length > 0 ? currentText : "未发现 MIDI 输入设备"
                                            enabled: !console.midiInputOpen
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 10
                                            ActionButton {
                                                text: "刷新设备"
                                                fill: "#EDF5FB"; ink: root.navy
                                                implicitHeight: 46
                                                Layout.preferredWidth: 132
                                                onClicked: console.refreshMidiInputs()
                                            }
                                            ActionButton {
                                                text: console.midiInputOpen ? "关闭输入" : "打开输入"
                                                Layout.fillWidth: true
                                                implicitHeight: 46
                                                fill: console.midiInputOpen ? "#EAF4EE" : root.navy
                                                ink: console.midiInputOpen ? root.greenInk : "#FFFFFF"
                                                onClicked: console.toggleMidiInput(midiInputSelect.currentIndex)
                                            }
                                        }

                                        Item { Layout.fillHeight: true }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "打开后，按键会立即映射为通道状态；未连接主板时也能看到通道反馈。"
                                            color: root.muted
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    SurfaceCard {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.preferredWidth: 2

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { text: "最近音符"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                                            Item { Layout.fillWidth: true }
                                            Rectangle {
                                                Layout.preferredWidth: 84; Layout.preferredHeight: 26; radius: 13
                                                color: !realtimePage.hasMidiEvent
                                                    ? "#EEF4F9"
                                                    : realtimePage.midiEvent.on ? "#E5F6EE" : "#EEF4F9"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: !realtimePage.hasMidiEvent
                                                        ? "WAITING"
                                                        : realtimePage.midiEvent.on ? "NOTE ON" : "NOTE OFF"
                                                    color: !realtimePage.hasMidiEvent
                                                        ? "#5D7488"
                                                        : realtimePage.midiEvent.on ? "#28724E" : "#5D7488"
                                                    font.pixelSize: 9
                                                    font.weight: Font.Bold
                                                    font.letterSpacing: 0.5
                                                }
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            radius: 16
                                            color: console.midiInputOpen ? "#F4FAFE" : "#F8FAFC"
                                            border.width: 1
                                            border.color: root.line

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 18
                                                spacing: 14

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 2
                                                    Text {
                                                        text: realtimePage.hasMidiEvent ? realtimePage.midiEvent.name : "—"
                                                        color: !realtimePage.hasMidiEvent
                                                            ? root.muted
                                                            : realtimePage.midiEvent.on ? root.navyDeep : root.muted
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 46
                                                        font.weight: Font.DemiBold
                                                    }
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: realtimePage.hasMidiEvent
                                                            ? "MIDI " + realtimePage.midiEvent.note
                                                                + "  ·  力度 " + realtimePage.midiEvent.velocity
                                                            : "等待外部 MIDI 输入"
                                                        color: root.ink
                                                        font.family: "Consolas"
                                                        font.pixelSize: 12
                                                    }
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: !realtimePage.hasMidiEvent
                                                            ? "弹奏键盘后显示最近事件"
                                                            : realtimePage.midiEvent.channel > 0
                                                                ? "映射通道 " + realtimePage.midiEvent.channel
                                                                    + "  ·  " + (realtimePage.midiEvent.on ? "按下" : "松开")
                                                                : "该音高未映射到通道"
                                                        color: root.muted
                                                        font.pixelSize: 11
                                                    }
                                                }

                                                Rectangle {
                                                    Layout.preferredWidth: 66
                                                    Layout.preferredHeight: 66
                                                    radius: 20
                                                    color: console.midiInputOpen ? "#EAF5FC" : "#F1F5F9"
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "♫"
                                                        color: console.midiInputOpen ? "#4F9ED3" : "#AAB8C5"
                                                        font.pixelSize: 30
                                                        font.weight: Font.DemiBold
                                                    }
                                                }
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "最近一次外部 MIDI 事件，与通道监听同步更新。"
                                            color: root.muted
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                SurfaceCard {
                                    Layout.fillWidth: true

                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout {
                                            spacing: 3
                                            Text { text: "通道监听"; color: root.ink; font.pixelSize: 16; font.weight: Font.DemiBold }
                                            Text { text: "按下的通道会实时点亮，反馈来自当前映射表。"; color: root.muted; font.pixelSize: 10 }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Rectangle {
                                            Layout.preferredWidth: 118; Layout.preferredHeight: 26; radius: 13
                                            color: "#EDF5FB"
                                            Text {
                                                anchors.centerIn: parent
                                                text: console.channelCount + " CH · " + realtimePage.activeCount + " 按下"
                                                color: "#4F7C9E"
                                                font.pixelSize: 9
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 8
                                        columnSpacing: 8
                                        rowSpacing: 8

                                        Repeater {
                                            model: console.mappingRows
                                            delegate: Rectangle {
                                                id: channelPad
                                                required property var modelData

                                                readonly property bool active: ((console.activeChannels >>> (modelData.channel - 1)) & 1) === 1
                                                readonly property color boardAccent: modelData.board === "N1" ? "#3F8FC4"
                                                    : modelData.board === "N2" ? "#6B7FB8"
                                                    : modelData.board === "N3" ? "#4C9A73" : "#B48635"

                                                Layout.fillWidth: true
                                                Layout.preferredWidth: 1
                                                Layout.preferredHeight: 44
                                                radius: 10
                                                color: active ? boardAccent : "#F8FBFE"
                                                border.width: 1
                                                border.color: active ? boardAccent : "#E6EEF5"

                                                Behavior on color { ColorAnimation { duration: 100 } }
                                                Behavior on border.color { ColorAnimation { duration: 100 } }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    spacing: 6
                                                    Text {
                                                        text: String(modelData.channel).padStart(2, "0")
                                                        color: channelPad.active ? "#FFFFFF" : root.navy
                                                        font.family: "Consolas"
                                                        font.pixelSize: 11
                                                        font.weight: Font.Bold
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        text: modelData.noteName
                                                        color: channelPad.active ? "#FFFFFF" : root.muted
                                                        font.pixelSize: 11
                                                        font.weight: Font.DemiBold
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                SurfaceCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 110

                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: "录制演奏"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                                            Text { text: console.recording ? "正在记录。停止后保存为 MIDI。" : "实时 MIDI 与直接按键都会写入。"; color: root.muted; font.pixelSize: 11 }
                                        }
                                        Rectangle {
                                            Layout.preferredWidth: 100; Layout.preferredHeight: 32; radius: 16
                                            color: console.recording ? "#F8D7D7" : root.paleSky
                                            Text {
                                                anchors.centerIn: parent
                                                text: console.recording ? "REC" : "IDLE"
                                                color: console.recording ? "#9B3B3B" : root.navy
                                                font.pixelSize: 11
                                                font.weight: Font.Bold
                                            }
                                        }
                                        ActionButton {
                                            text: console.recording ? "停止并保存" : "开始录制"
                                            fill: console.recording ? "#F4D2D2" : root.navy
                                            ink: console.recording ? "#8E3030" : "#FFFFFF"
                                            implicitHeight: 48
                                            Layout.preferredWidth: 170
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
                                        AppSlider {
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
                                        Text { text: "按住键位发送 ON，松开后发送 OFF；也支持电脑键盘 1–0 / Q–P / A–L / Z–C。"; color: root.muted; font.pixelSize: 11 }
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
                                        spacing: 3
                                        Text { text: "键位映射"; color: root.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                        Text { text: "实时 MIDI 与 MIDI 自动播放共用这张音高表。"; color: root.muted; font.pixelSize: 11 }
                                    }
                                    Item { Layout.fillWidth: true }
                                    ColumnLayout {
                                        spacing: 4
                                        Text { text: "键位数"; color: root.muted; font.pixelSize: 10 }
                                        AppSpinBox {
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
                                        Layout.alignment: Qt.AlignBottom
                                        onClicked: console.resetMapping()
                                    }
                                }

                                SurfaceCard {
                                    Layout.fillWidth: true

                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout {
                                            spacing: 3
                                            Text { text: "通道映射"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                                            Text { text: "每行两个通道，直接编辑 MIDI 音高。"; color: root.muted; font.pixelSize: 10 }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Rectangle {
                                            Layout.preferredWidth: 118
                                            Layout.preferredHeight: 26
                                            radius: 13
                                            color: "#EDF5FB"
                                            Text {
                                                anchors.centerIn: parent
                                                text: console.channelCount + " CH · 2 / ROW"
                                                color: "#4F7C9E"
                                                font.family: "Segoe UI"
                                                font.pixelSize: 8
                                                font.weight: Font.Bold
                                                font.letterSpacing: 0.6
                                            }
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        columnSpacing: 12
                                        rowSpacing: 8

                                        Repeater {
                                            model: console.mappingRows
                                            delegate: Rectangle {
                                                id: mapCard
                                                required property var modelData

                                                readonly property string board: modelData.board
                                                readonly property color boardTint: board === "N1" ? "#EAF5FC"
                                                    : board === "N2" ? "#EEF0FA"
                                                    : board === "N3" ? "#EDF7F0" : "#FDF4E6"
                                                readonly property color boardAccent: board === "N1" ? "#3F8FC4"
                                                    : board === "N2" ? "#6B7FB8"
                                                    : board === "N3" ? "#4C9A73" : "#B48635"

                                                Layout.fillWidth: true
                                                Layout.preferredWidth: 1
                                                Layout.minimumWidth: 0
                                                Layout.preferredHeight: 68
                                                radius: 14
                                                color: cardHover.hovered ? "#FBFDFF" : "#F8FBFE"
                                                border.width: 1
                                                border.color: cardHover.hovered ? "#C7DEEE" : "#E6EEF5"

                                                HoverHandler { id: cardHover }
                                                Behavior on border.color { ColorAnimation { duration: 120 } }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 9
                                                    spacing: 12

                                                    Rectangle {
                                                        Layout.preferredWidth: 40
                                                        Layout.preferredHeight: 40
                                                        Layout.alignment: Qt.AlignVCenter
                                                        radius: 12
                                                        color: mapCard.boardTint

                                                        ColumnLayout {
                                                            anchors.centerIn: parent
                                                            spacing: 0
                                                            Text {
                                                                text: String(mapCard.modelData.channel).padStart(2, "0")
                                                                color: mapCard.boardAccent
                                                                font.family: "Consolas"
                                                                font.pixelSize: 15
                                                                font.weight: Font.Bold
                                                            }
                                                            Text {
                                                                text: mapCard.board
                                                                color: mapCard.boardAccent
                                                                opacity: 0.75
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 8
                                                                font.weight: Font.Bold
                                                                font.letterSpacing: 0.8
                                                            }
                                                        }
                                                    }

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 14

                                                        ColumnLayout {
                                                            spacing: 3
                                                            Text { text: "MIDI 音高"; color: root.muted; font.pixelSize: 9 }
                                                            AppSpinBox {
                                                                Layout.preferredWidth: 120
                                                                Layout.preferredHeight: 38
                                                                from: 0
                                                                to: 127
                                                                editable: true
                                                                value: mapCard.modelData.midiNote
                                                                onValueModified: {
                                                                    const error = console.setMappingNote(mapCard.modelData.channel, value)
                                                                    if (error.length > 0) {
                                                                        root.showToast(error)
                                                                        value = mapCard.modelData.midiNote
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        RowLayout {
                                                            Layout.fillWidth: true
                                                            Layout.alignment: Qt.AlignVCenter
                                                            spacing: 8
                                                            Rectangle {
                                                                Layout.fillWidth: true
                                                                Layout.preferredHeight: 1
                                                                color: "#DCE8F2"
                                                            }
                                                            Text {
                                                                text: "→"
                                                                color: "#AFC4D5"
                                                                font.pixelSize: 11
                                                            }
                                                        }

                                                        ColumnLayout {
                                                            Layout.alignment: Qt.AlignVCenter
                                                            spacing: 1
                                                            Text {
                                                                text: mapCard.modelData.noteName
                                                                color: mapCard.boardAccent
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 20
                                                                font.weight: Font.DemiBold
                                                            }
                                                            Text {
                                                                text: "NOTE"
                                                                color: root.muted
                                                                opacity: 0.75
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 8
                                                                font.weight: Font.Bold
                                                                font.letterSpacing: 1.0
                                                            }
                                                        }
                                                    }
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
                                            AppComboBox { id: mappingMode; Layout.fillWidth: true; model: ["范围外忽略", "取余循环"] }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 140; spacing: 6
                                            Text { text: "起始音高"; color: root.muted; font.pixelSize: 10 }
                                            AppSpinBox { id: rangeStart; from: 0; to: 127; value: 60; editable: true; Layout.fillWidth: true }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 140; spacing: 6
                                            Text { text: "结束音高（含）"; color: root.muted; font.pixelSize: 10 }
                                            AppSpinBox { id: rangeEnd; from: 0; to: 127; value: 91; editable: true; Layout.fillWidth: true }
                                        }
                                        ColumnLayout {
                                            Layout.preferredWidth: 145; spacing: 6
                                            Text { text: "聚合窗（ms）"; color: root.muted; font.pixelSize: 10 }
                                            AppSpinBox { id: aggregationWindow; from: 1; to: 1000; value: 30; editable: true; Layout.fillWidth: true }
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
                                    Text { text: "设置"; color: root.ink; font.pixelSize: 24; font.weight: Font.DemiBold }
                                    Text { text: "连接参数和录制参数。插入主板后按这里的波特率自动握手。"; color: root.muted; font.pixelSize: 12 }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true; Layout.preferredHeight: 220

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Rectangle {
                                            width: 44; height: 44; radius: 13; color: "#EAF5FC"
                                            Text { anchors.centerIn: parent; text: "◉"; color: "#3F8FC4"; font.pixelSize: 20; font.weight: Font.DemiBold }
                                        }
                                        ColumnLayout {
                                            spacing: 2
                                            Text { text: "串口连接"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                                            Text { text: console.serialConnected ? ("当前主板 " + console.linkedPort) : "尚未发现主板"; color: root.muted; font.pixelSize: 12 }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Rectangle {
                                            Layout.preferredWidth: 92; Layout.preferredHeight: 28; radius: 14
                                            color: console.serialConnected ? "#E5F6EE" : "#FFF5DC"
                                            Text {
                                                anchors.centerIn: parent
                                                text: console.serialConnected ? "ONLINE" : "AUTO SCAN"
                                                color: console.serialConnected ? "#28724E" : "#936B1D"
                                                font.pixelSize: 9; font.weight: Font.Bold; font.letterSpacing: 0.6
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "主板通过 USB 串口连接；上位机按当前波特率自动探测并握手。"
                                        color: root.muted
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 16
                                        ColumnLayout {
                                            spacing: 6
                                            Text { text: "波特率"; color: root.muted; font.pixelSize: 12 }
                                            AppComboBox {
                                                id: baudSetting
                                                Layout.preferredWidth: 220
                                                Layout.preferredHeight: 48
                                                font.pixelSize: 14
                                                model: ["115200", "57600", "9600", "921600"]
                                                currentIndex: Math.max(0, model.indexOf(String(console.baudRate)))
                                                onActivated: console.setBaudRate(Number(currentText))
                                            }
                                        }
                                        ActionButton {
                                            text: "重新扫描串口"
                                            fill: "#EDF5FB"; ink: root.navy
                                            implicitHeight: 48
                                            Layout.preferredWidth: 168
                                            Layout.alignment: Qt.AlignBottom
                                            onClicked: console.refreshSerialPorts()
                                        }
                                        Item { Layout.fillWidth: true }
                                    }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true; Layout.preferredHeight: 290

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Rectangle {
                                            width: 44; height: 44; radius: 13; color: "#FDEDEA"
                                            Text { anchors.centerIn: parent; text: "●"; color: "#D98770"; font.pixelSize: 18 }
                                        }
                                        ColumnLayout {
                                            spacing: 2
                                            Text { text: "录制参数"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                                            Text { text: "实时 MIDI 与直接控制共用同一套量化参数"; color: root.muted; font.pixelSize: 12 }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Rectangle {
                                            Layout.preferredWidth: 92; Layout.preferredHeight: 28; radius: 14; color: "#FDEDEA"
                                            Text { anchors.centerIn: parent; text: console.recording ? "RECORDING" : "READY"; color: "#9B3B3B"; font.pixelSize: 9; font.weight: Font.Bold; font.letterSpacing: 0.6 }
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "同一聚合窗内的音写成同时按下；同一键短于消抖时间的来回抖动会被丢掉。"
                                        color: root.muted
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 20
                                        ColumnLayout {
                                            spacing: 6
                                            Text { text: "聚合窗（ms）"; color: root.muted; font.pixelSize: 12 }
                                            AppSpinBox {
                                                Layout.preferredWidth: 220
                                                Layout.preferredHeight: 48
                                                font.pixelSize: 13
                                                from: 1; to: 100; value: console.recordWindowMs; editable: true
                                                onValueModified: console.setRecordWindowMs(value)
                                            }
                                        }
                                        ColumnLayout {
                                            spacing: 6
                                            Text { text: "消抖（ms）"; color: root.muted; font.pixelSize: 12 }
                                            AppSpinBox {
                                                Layout.preferredWidth: 220
                                                Layout.preferredHeight: 48
                                                font.pixelSize: 13
                                                from: 0; to: 50; value: console.recordDebounceMs; editable: true
                                                onValueModified: console.setRecordDebounceMs(value)
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: "推荐值：聚合窗 30 ms · 消抖 20 ms"; color: root.muted; font.pixelSize: 11 }
                                        Item { Layout.fillWidth: true }
                                        ActionButton {
                                            text: "恢复推荐值"
                                            fill: "#EDF5FB"; ink: root.navy
                                            implicitHeight: 46
                                            Layout.preferredWidth: 152
                                            onClicked: {
                                                console.setRecordWindowMs(30)
                                                console.setRecordDebounceMs(20)
                                            }
                                        }
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
                                        Text { text: "运行日志"; color: root.ink; font.pixelSize: 24; font.weight: Font.DemiBold }
                                        Text { text: "串口、MIDI、转换与位图帧活动。"; color: root.muted; font.pixelSize: 12 }
                                    }
                                    Item { Layout.fillWidth: true }
                                    Rectangle {
                                        Layout.preferredWidth: 112; Layout.preferredHeight: 36; radius: 18
                                        color: root.paleSky
                                        Text { anchors.centerIn: parent; text: console.logLines.length + " 条记录"; color: root.navy; font.pixelSize: 12; font.weight: Font.DemiBold }
                                    }
                                    ActionButton {
                                        text: "定位最新"
                                        fill: "#EDF5FB"; ink: root.navy
                                        implicitHeight: 40
                                        Layout.preferredWidth: 120
                                        onClicked: logList.positionViewAtEnd()
                                    }
                                }
                                SurfaceCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 620
                                    ListView {
                                        id: logList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.preferredHeight: 560
                                        clip: true
                                        spacing: 3
                                        model: console.logLines
                                        delegate: Rectangle {
                                            required property string modelData
                                            required property int index
                                            width: ListView.view.width
                                            height: 46
                                            radius: 9
                                            color: index % 2 === 0 ? "#F8FAFC" : "#FFFFFF"
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 14
                                                anchors.rightMargin: 14
                                                spacing: 12
                                                Rectangle { width: 7; height: 7; radius: 4; color: modelData.indexOf("错误") >= 0 ? "#D98770" : root.sky }
                                                Text {
                                                    text: modelData
                                                    color: root.ink
                                                    font.family: "Consolas"
                                                    font.pixelSize: 13
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                                Text {
                                                    text: String(index + 1).padStart(3, "0")
                                                    color: "#C3D1DE"
                                                    font.family: "Consolas"
                                                    font.pixelSize: 10
                                                }
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
                padding: 12
                font.family: "Consolas"
                font.pixelSize: 11
                color: root.ink
                selectionColor: "#69B7E8"
                selectedTextColor: "#FFFFFF"

                background: Rectangle {
                    color: "#FFFFFF"
                    radius: 10
                    border.width: 1
                    border.color: root.line
                }
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

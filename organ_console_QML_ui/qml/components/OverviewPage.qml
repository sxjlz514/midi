import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * 控制总览页。
 * 视觉结构：Hero（信号监视）+ 一体化状态带 + 活动流 / 快捷操作宫格。
 * 只负责展示与导航，串口/MIDI 状态全部来自 MainWindow 的 console。
 */
Item {
    id: page
    required property var appRoot

    readonly property bool hasFile: console.currentFile.length > 0
    readonly property string fileName: hasFile
        ? console.currentFile.split(/[\\/]/).pop()
        : "尚未载入"
    readonly property bool fileIsOcf: hasFile
        && console.currentFile.toLowerCase().endsWith(".ocf")
    readonly property string fileState: !hasFile
        ? "EMPTY"
        : console.playing
            ? "PLAYING"
            : console.paused
                ? "PAUSED"
                : fileIsOcf ? "OCF" : "MIDI"
    readonly property color fileAccent: fileIsOcf ? "#C79A3C" : "#4F9ED3"

    implicitHeight: body.implicitHeight + 56

    component StatusSegment: Rectangle {
        id: segment
        property string eyebrow: ""
        property string valueText: ""
        property string detailText: ""
        property string glyph: "◉"
        property color accent: "#4F9ED3"
        property color iconBackground: "#EAF5FC"
        property bool online: false
        signal activated()

        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 14
        color: segmentArea.containsMouse ? "#F7FBFE" : "transparent"

        Behavior on color { ColorAnimation { duration: 120 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 13
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 12

            Rectangle {
                width: 42
                height: 42
                radius: 13
                color: segment.iconBackground
                Text {
                    anchors.centerIn: parent
                    text: segment.glyph
                    color: segment.accent
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        text: segment.eyebrow
                        color: "#8798A8"
                        font.family: "Segoe UI"
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.9
                    }
                    Rectangle {
                        width: 5
                        height: 5
                        radius: 3
                        color: segment.accent
                        opacity: segment.online ? 1.0 : 0.55
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: segment.valueText
                    color: "#18334E"
                    font.family: "Segoe UI"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: segment.detailText
                    color: "#74879A"
                    font.family: "Segoe UI"
                    font.pixelSize: 9
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                width: 26
                height: 26
                radius: 13
                color: segmentArea.containsMouse ? segment.iconBackground : "#F2F7FB"
                Text {
                    anchors.centerIn: parent
                    text: "›"
                    color: segment.accent
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }
        }

        MouseArea {
            id: segmentArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: segment.activated()
        }
    }

    component ActionTile: Rectangle {
        id: tile
        property string glyph: "＋"
        property string title: ""
        property string subtitle: ""
        property color accent: "#CDE6F6"
        signal activated()

        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 14
        color: tileArea.containsMouse ? "#26FFFFFF" : "#10FFFFFF"
        border.width: 1
        border.color: tileArea.containsMouse ? "#40FFFFFF" : "#1FFFFFFF"

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    width: 52
                    height: 52
                    radius: 15
                    color: Qt.rgba(tile.accent.r, tile.accent.g, tile.accent.b,
                                   tileArea.containsMouse ? 0.22 : 0.13)
                    Text {
                        anchors.centerIn: parent
                        text: tile.glyph
                        color: tile.accent
                        font.pixelSize: 23
                        font.weight: Font.Bold
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "›"
                    color: tileArea.containsMouse ? "#FFFFFF" : "#6F9CBB"
                    font.pixelSize: 20
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                text: tile.title
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 19
                font.weight: Font.Bold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                text: tile.subtitle
                color: "#B9D2E3"
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        MouseArea {
            id: tileArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tile.activated()
        }
    }

    ColumnLayout {
        id: body
        width: parent.width
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 28
        spacing: 18

        // ── Hero ────────────────────────────────────────────────────────
        Rectangle {
            id: hero
            Layout.fillWidth: true
            Layout.preferredHeight: 264
            radius: 24
            clip: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0F2C47" }
                GradientStop { position: 0.58; color: "#1A456A" }
                GradientStop { position: 1.0; color: "#2B6490" }
            }

            Rectangle {
                width: 460
                height: 460
                radius: 230
                x: hero.width - 270
                y: -260
                color: "#2E7BAE"
                opacity: 0.30
            }
            Rectangle {
                width: 320
                height: 320
                radius: 160
                x: hero.width - 440
                y: 160
                color: "#69B7E8"
                opacity: 0.08
            }
            Text {
                x: hero.width - 470
                y: -60
                text: "♫"
                color: "#FFFFFF"
                opacity: 0.032
                font.pixelSize: 250
                font.weight: Font.Bold
            }
            Repeater {
                model: 10
                delegate: Rectangle {
                    required property int index
                    x: hero.width - 470 + index * 52
                    y: 0
                    width: 1
                    height: hero.height
                    color: "#FFFFFF"
                    opacity: 0.032
                }
            }
            Rectangle {
                width: hero.width
                height: 1
                y: hero.height - 1
                color: "#FFFFFF"
                opacity: 0.08
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 30
                anchors.rightMargin: 24
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                spacing: 22

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    RowLayout {
                        spacing: 9

                        Rectangle {
                            Layout.preferredWidth: overviewBadge.implicitWidth + 20
                            Layout.preferredHeight: 26
                            radius: 13
                            color: "#14FFFFFF"
                            border.width: 1
                            border.color: "#24FFFFFF"
                            Text {
                                id: overviewBadge
                                anchors.centerIn: parent
                                text: "CONTROL / OVERVIEW"
                                color: "#BFDDF0"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 1.3
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: statusText.implicitWidth + 28
                            Layout.preferredHeight: 26
                            radius: 13
                            color: console.serialConnected ? "#1E5A47" : "#5C4A22"
                            border.width: 1
                            border.color: console.serialConnected ? "#2E7A5E" : "#7A622E"
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Rectangle {
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: console.serialConnected ? "#6FE0B0" : "#F4C66A"
                                }
                                Text {
                                    id: statusText
                                    text: console.serialConnected ? "系统在线" : "等待主板"
                                    color: console.serialConnected ? "#A7E8C9" : "#F2D697"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Item { Layout.preferredHeight: 20 }

                    Text {
                        text: "让每个音符，"
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 35
                        font.weight: Font.DemiBold
                        lineHeight: 0.95
                    }
                    Text {
                        text: "准确到达。"
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 35
                        font.weight: Font.DemiBold
                        lineHeight: 0.95
                    }

                    Item { Layout.preferredHeight: 9 }

                    Text {
                        text: "统一管理 MIDI 输入、位图流回放与键位映射。"
                        color: "#B7D2E5"
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        Layout.fillWidth: true
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        spacing: 10
                        ActionButton {
                            text: "▶   打开自动演奏"
                            fill: "#69B7E8"
                            ink: "#102B42"
                            Layout.preferredWidth: 166
                            onClicked: appRoot.selectPage(2)
                        }
                        ActionButton {
                            text: "＋   载入 MIDI / OCF"
                            fill: "#FFFFFF"
                            ink: "#183A5D"
                            Layout.preferredWidth: 178
                            onClicked: console.chooseControlFile()
                        }
                    }
                }

                // 右侧信号监视面板
                Rectangle {
                    Layout.preferredWidth: 336
                    Layout.fillHeight: true
                    radius: 18
                    color: "#12FFFFFF"
                    border.width: 1
                    border.color: "#24FFFFFF"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 9

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            Text {
                                text: "SIGNAL MONITOR"
                                color: "#A9C5D9"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                width: 7
                                height: 7
                                radius: 4
                                color: console.serialConnected ? "#6FE0B0" : "#F4C66A"
                            }
                            Text {
                                text: console.serialConnected ? "ONLINE" : "STANDBY"
                                color: console.serialConnected ? "#9FE6C4" : "#E8CE8A"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 0.6
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14

                            ColumnLayout {
                                spacing: 0
                                Text {
                                    text: console.channelCount
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 42
                                    font.weight: Font.DemiBold
                                    lineHeight: 0.9
                                }
                                Text {
                                    text: "ACTIVE CHANNELS"
                                    color: "#8FB6CE"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 8
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 1.0
                                }
                            }

                            Item { Layout.fillWidth: true }

                            ColumnLayout {
                                spacing: 1
                                Text {
                                    text: "4 ms"
                                    color: "#D8ECF8"
                                    font.family: "Consolas"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: "WINDOW"
                                    color: "#8FB6CE"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 8
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.preferredHeight: 30
                                color: "#2EFFFFFF"
                            }

                            ColumnLayout {
                                spacing: 1
                                Text {
                                    text: "32"
                                    color: "#D8ECF8"
                                    font.family: "Consolas"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: "BITMAP"
                                    color: "#8FB6CE"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 8
                                }
                            }
                        }

                        Row {
                            Layout.topMargin: 2
                            spacing: 3
                            Repeater {
                                model: 32
                                delegate: Item {
                                    required property int index
                                    width: 6
                                    height: 26
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: 6
                                        height: index < console.channelCount ? 26 : 10
                                        radius: 3
                                        color: index < console.channelCount ? "#69B7E8" : "#FFFFFF"
                                        opacity: index < console.channelCount ? 0.88 : 0.10
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: "#FFFFFF"
                            opacity: 0.10
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Text {
                                text: "MIDI IN"
                                color: "#8FB6CE"
                                font.family: "Segoe UI"
                                font.pixelSize: 8
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: console.midiInputOpen ? "#6FE0B0" : "#71899C"
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "UART"
                                color: "#8FB6CE"
                                font.family: "Segoe UI"
                                font.pixelSize: 8
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: console.serialConnected ? "#6FE0B0" : "#71899C"
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "CAN OUT"
                                color: "#8FB6CE"
                                font.family: "Segoe UI"
                                font.pixelSize: 8
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: console.serialConnected ? "#6FE0B0" : "#71899C"
                            }
                        }
                    }
                }
            }
        }

        // ── 一体化状态带 ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            radius: 18
            color: "#FFFFFF"
            border.width: 1
            border.color: "#E2EAF2"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                StatusSegment {
                    eyebrow: "01 / SERIAL LINK"
                    valueText: console.serialConnected ? "主板在线" : "等待连接"
                    detailText: console.serialConnected
                        ? console.linkedPort + " · " + console.baudRate + " baud"
                        : "插入主板后自动扫描"
                    glyph: "◉"
                    accent: console.serialConnected ? "#3E9B70" : "#D7A23F"
                    iconBackground: console.serialConnected ? "#E8F6EF" : "#FFF6E2"
                    online: console.serialConnected
                    onActivated: appRoot.selectPage(6)
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 56
                    color: "#E8EFF5"
                }

                StatusSegment {
                    eyebrow: "02 / MIDI INPUT"
                    valueText: console.midiInputOpen ? "正在监听" : "未启动"
                    detailText: console.midiInputDevices.length > 0
                        ? console.midiInputDevices.length + " 个输入设备可用"
                        : "连接 USB MIDI 键盘后刷新"
                    glyph: "♫"
                    accent: console.midiInputOpen ? "#3E9B70" : "#4F9ED3"
                    iconBackground: console.midiInputOpen ? "#E8F6EF" : "#EAF5FC"
                    online: console.midiInputOpen
                    onActivated: appRoot.selectPage(1)
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 56
                    color: "#E8EFF5"
                }

                StatusSegment {
                    eyebrow: "03 / CURRENT SCORE"
                    valueText: page.fileName
                    detailText: page.hasFile
                        ? appRoot.formatMs(console.durationMs) + " · "
                            + (page.fileIsOcf ? "位图流" : "MIDI 文件")
                        : "支持 MIDI / OCF 位图流"
                    glyph: "▤"
                    accent: page.fileAccent
                    iconBackground: page.fileIsOcf ? "#FFF6E2" : "#EAF5FC"
                    online: page.hasFile
                    onActivated: console.chooseControlFile()
                }
            }
        }

        // ── 快捷操作 ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 292
            radius: 20
            clip: true
            border.width: 1
            border.color: "#2A5A80"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#173A5C" }
                GradientStop { position: 1.0; color: "#215A83" }
            }

            Rectangle {
                width: 300
                height: 300
                radius: 150
                x: parent.width - 180
                y: -170
                color: "#69B7E8"
                opacity: 0.10
            }
            Text {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 30
                anchors.bottomMargin: 8
                text: "♫"
                color: "#FFFFFF"
                opacity: 0.03
                font.pixelSize: 220
                font.weight: Font.Bold
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 3
                        Text {
                            text: "快捷操作"
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 19
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: "常用入口，一步直达。"
                            color: "#A9C5D9"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 26
                        radius: 13
                        color: "#14FFFFFF"
                        border.width: 1
                        border.color: "#26FFFFFF"
                        Text {
                            anchors.centerIn: parent
                            text: "4 ACTIONS"
                            color: "#BFDDF0"
                            font.family: "Segoe UI"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                            font.letterSpacing: 1.0
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    ActionTile {
                        glyph: "＋"
                        title: "载入文件"
                        subtitle: "MIDI / OCF"
                        onActivated: console.chooseControlFile()
                    }
                    ActionTile {
                        glyph: "▶"
                        title: "自动演奏"
                        subtitle: "播放与瀑布流"
                        onActivated: appRoot.selectPage(2)
                    }
                    ActionTile {
                        glyph: "♫"
                        title: "实时 MIDI"
                        subtitle: "外接键盘输入"
                        onActivated: appRoot.selectPage(1)
                    }
                    ActionTile {
                        glyph: "■"
                        title: "全部关闭"
                        subtitle: "停止并归零"
                        accent: "#F0B49E"
                        onActivated: console.allOff()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Repeater {
                        model: [
                            { label: "键位映射", page: 4 },
                            { label: "运行日志", page: 7 }
                        ]
                        delegate: Text {
                            required property var modelData
                            text: modelData.label
                            color: linkArea.containsMouse ? "#FFFFFF" : "#9FC5DD"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            MouseArea {
                                id: linkArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appRoot.selectPage(modelData.page)
                            }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "v1.0"
                        color: "#6E9ABB"
                        font.family: "Consolas"
                        font.pixelSize: 9
                    }
                }
            }
        }
    }
}

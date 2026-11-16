import QtQuick 2.15

// MIDI → OCF 转换参数弹窗（内存转换，不落盘）
Item {
    id: root

    property bool shown: false
    visible: shown
    property int defaultStart: 60
    property int defaultEnd: 91
    property int channelLimit: 16
    property string fileName: ""

    signal accepted(int rangeStart, int rangeEnd, bool wrap, int windowMs)
    signal canceled()

    Theme { id: theme }

    component StepButton: Rectangle {
        property string label: "-"
        signal clicked()
        width: 26
        height: 26
        radius: 3
        color: sbMouse.containsMouse ? theme.panelHover : theme.bg
        border.color: theme.border
        Text {
            anchors.centerIn: parent
            text: parent.label
            color: theme.text
            font.pixelSize: 13
        }
        MouseArea {
            id: sbMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }

    property int startValue: defaultStart
    property int endValue: defaultEnd
    property bool wrapMode: false
    property string windowText: "30"
    property string errorText: ""

    onShownChanged: {
        if (shown) {
            startValue = defaultStart;
            endValue = defaultEnd;
            wrapMode = false;
            windowText = "30";
            errorText = "";
            startInput.text = String(defaultStart);
            endInput.text = String(defaultEnd);
            windowInput.text = "30";
        }
    }

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        width: 440
        height: cardColumn.implicitHeight + 40
        anchors.centerIn: parent
        color: theme.panel
        border.color: theme.border
        border.width: 1
        radius: 4

        Column {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 14

            Text {
                text: "转换为 OCF"
                color: theme.text
                font.pixelSize: 16
                font.bold: true
            }
            Text {
                width: cardColumn.width
                text: "已选择「" + root.fileName + "」。转换参数如下，确认后在内存中播放，不写入磁盘。"
                color: theme.secondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            // 起点
            Row {
                spacing: 8
                height: 30
                Text {
                    text: "起点音"
                    color: theme.text
                    font.pixelSize: 12
                    width: 64
                    anchors.verticalCenter: parent.verticalCenter
                }
                StepButton { label: "-"; onClicked: { root.startValue = Math.max(0, root.startValue - 1); startInput.text = String(root.startValue); } }

                Rectangle {
                    width: 70
                    height: 26
                    color: theme.bg
                    border.color: theme.border
                    border.width: 1
                    TextInput {
                        id: startInput
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        verticalAlignment: TextInput.AlignVCenter
                        color: theme.text
                        font.pixelSize: 12
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: {
                            var v = parseInt(text, 10);
                            if (!isNaN(v)) root.startValue = Math.max(0, Math.min(127, v));
                            text = String(root.startValue);
                        }
                    }
                }
                StepButton { label: "+"; onClicked: { root.startValue = Math.min(127, root.startValue + 1); startInput.text = String(root.startValue); } }
            }

            // 终点
            Row {
                spacing: 8
                height: 30
                Text {
                    text: "结束音"
                    color: theme.text
                    font.pixelSize: 12
                    width: 64
                    anchors.verticalCenter: parent.verticalCenter
                }
                StepButton { label: "-"; onClicked: { root.endValue = Math.max(0, root.endValue - 1); endInput.text = String(root.endValue); } }
                Rectangle {
                    width: 70
                    height: 26
                    color: theme.bg
                    border.color: theme.border
                    border.width: 1
                    TextInput {
                        id: endInput
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        verticalAlignment: TextInput.AlignVCenter
                        color: theme.text
                        font.pixelSize: 12
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: {
                            var v = parseInt(text, 10);
                            if (!isNaN(v)) root.endValue = Math.max(0, Math.min(127, v));
                            text = String(root.endValue);
                        }
                    }
                }
                StepButton { label: "+"; onClicked: { root.endValue = Math.min(127, root.endValue + 1); endInput.text = String(root.endValue); } }
            }

            // 映射模式
            Row {
                spacing: 8
                height: 30
                Text {
                    text: "范围外"
                    color: theme.text
                    font.pixelSize: 12
                    width: 64
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    width: 74
                    height: 26
                    radius: 3
                    color: !root.wrapMode ? Qt.rgba(0.133, 0.827, 0.933, 0.12) : theme.bg
                    border.color: !root.wrapMode ? theme.accent : theme.border
                    Text {
                        anchors.centerIn: parent
                        text: "忽略"
                        color: !root.wrapMode ? theme.accent : theme.secondary
                        font.pixelSize: 11
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.wrapMode = false }
                }
                Rectangle {
                    width: 74
                    height: 26
                    radius: 3
                    color: root.wrapMode ? Qt.rgba(0.133, 0.827, 0.933, 0.12) : theme.bg
                    border.color: root.wrapMode ? theme.accent : theme.border
                    Text {
                        anchors.centerIn: parent
                        text: "循环"
                        color: root.wrapMode ? theme.accent : theme.secondary
                        font.pixelSize: 11
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.wrapMode = true }
                }
            }

            // 聚合窗
            Row {
                spacing: 8
                height: 30
                Text {
                    text: "聚合窗"
                    color: theme.text
                    font.pixelSize: 12
                    width: 64
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    width: 70
                    height: 26
                    color: theme.bg
                    border.color: theme.border
                    border.width: 1
                    TextInput {
                        id: windowInput
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        verticalAlignment: TextInput.AlignVCenter
                        color: theme.text
                        font.pixelSize: 12
                        inputMethodHints: Qt.ImhDigitsOnly
                    }
                }
                Text {
                    text: "ms"
                    color: theme.secondary
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: cardColumn.width
                text: "跨度 " + (root.endValue - root.startValue + 1)
                      + "，本机可用键位 " + root.channelLimit
                color: (root.endValue - root.startValue + 1) > root.channelLimit
                       ? theme.errRed : theme.secondary
                font.pixelSize: 11
            }

            Text {
                width: cardColumn.width
                visible: root.errorText.length > 0
                text: root.errorText
                color: theme.errRed
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Row {
                spacing: 10
                anchors.right: parent.right

                Rectangle {
                    width: 76
                    height: 28
                    radius: 3
                    color: cancelMouse.containsMouse ? theme.panelHover : "transparent"
                    border.color: theme.border
                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: theme.text
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: cancelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.canceled()
                    }
                }
                Rectangle {
                    width: 96
                    height: 28
                    radius: 3
                    color: okMouse.containsMouse ? "#1AA7BF" : theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: "转换并播放"
                        color: "#0F1216"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: okMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.submit()
                    }
                }
            }
        }
    }

    function submit() {
        var start;
        var end;
        if (startInput.text.length > 0) start = parseInt(startInput.text, 10);
        else start = startValue;
        if (endInput.text.length > 0) end = parseInt(endInput.text, 10);
        else end = endValue;
        var win = parseInt(windowInput.text, 10);

        if (isNaN(start) || isNaN(end) || start < 0 || end > 127 || start > end) {
            errorText = "起点和结束音必须在 0..127 且起点不大于结束。";
            return;
        }
        if ((end - start + 1) > channelLimit) {
            errorText = "跨度超过本机键位数 " + channelLimit + "。";
            return;
        }
        if (isNaN(win) || win < 1 || win > 65535) {
            errorText = "聚合窗需在 1..65535 ms。";
            return;
        }
        errorText = "";
        root.accepted(start, end, root.wrapMode, win);
    }
}

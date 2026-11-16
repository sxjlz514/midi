import QtQuick 2.15

// 通用确认弹窗（未保存数据提示等）
Item {
    id: root

    property bool shown: false
    visible: shown
    property string title: "提示"
    property string message: ""
    property string confirmLabel: "确定"
    property string cancelLabel: "取消"

    signal confirmed()
    signal canceled()

    Theme { id: theme }

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        width: 380
        height: body.implicitHeight + 36
        anchors.centerIn: parent
        color: theme.panel
        border.color: theme.border
        border.width: 1
        radius: 4

        Column {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 14

            Text {
                text: root.title
                color: theme.text
                font.pixelSize: 15
                font.bold: true
            }
            Text {
                width: body.width
                text: root.message
                color: theme.secondary
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
                    color: cm.containsMouse ? theme.panelHover : "transparent"
                    border.color: theme.border
                    Text {
                        anchors.centerIn: parent
                        text: root.cancelLabel
                        color: theme.text
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: cm
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.canceled()
                    }
                }
                Rectangle {
                    width: 86
                    height: 28
                    radius: 3
                    color: om.containsMouse ? "#1AA7BF" : theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: root.confirmLabel
                        color: "#0F1216"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: om
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.confirmed()
                    }
                }
            }
        }
    }
}

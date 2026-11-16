import QtQuick 2.15

// 顶部进度条：标题栏下方、音符区上方
// - 播放 / 暂停：显示播放进度与总时长
// - 录制：右侧显示已录制时长
Item {
    id: root
    height: 24

    property bool active: false        // 播放或暂停中
    property bool recording: false
    property real positionMs: 0
    property real durationMs: 0
    property real recordElapsedMs: 0

    Theme { id: theme }

    function fmt(ms) {
        var sec = Math.max(0, Math.floor(ms / 1000));
        var m = Math.floor(sec / 60);
        var s = sec % 60;
        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
    }

    Rectangle { anchors.fill: parent; color: theme.panel }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: theme.border
    }

    property real ratio: durationMs > 0 ? Math.min(1, Math.max(0, positionMs / durationMs)) : 0

    Rectangle {
        visible: root.active && root.durationMs > 0
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * root.ratio
        color: Qt.rgba(0.133, 0.827, 0.933, 0.15)
    }
    Rectangle {
        visible: root.active && root.durationMs > 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: Math.max(0, parent.width * root.ratio - 2)
        width: 2
        color: theme.accent
    }

    Row {
        visible: root.recording
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: theme.recordRed
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "录制中"
            color: theme.text
            font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 11
        color: theme.secondary
        text: root.recording ? "已录制 " + fmt(root.recordElapsedMs)
            : (root.active ? fmt(root.positionMs) + " / " + fmt(root.durationMs) : "")
    }
}

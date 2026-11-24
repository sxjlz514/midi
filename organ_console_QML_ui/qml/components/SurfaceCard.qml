import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    default property alias contentData: content.data

    color: "#FFFFFF"
    radius: 18
    border.color: "#E1EAF3"
    border.width: 1
    // 用内容算出隐含高度；父布局若指定 preferredHeight / fillHeight 仍可覆盖。
    implicitHeight: content.implicitHeight + 40
    clip: true

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14
    }
}

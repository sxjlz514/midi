import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: card
    default property alias contentData: content.data

    color: "#FFFFFF"
    radius: 18
    border.color: "#E1EAF3"
    border.width: 1

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14
    }
}

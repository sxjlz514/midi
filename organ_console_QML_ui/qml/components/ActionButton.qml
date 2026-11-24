import QtQuick
import QtQuick.Controls

Button {
    id: control

    property color fill: "#2F78B7"
    property color ink: "#FFFFFF"
    property color outline: "transparent"
    property int cornerRadius: 12

    implicitHeight: 44
    font.family: "Segoe UI"
    font.pixelSize: 14
    font.weight: Font.DemiBold

    background: Rectangle {
        radius: control.cornerRadius
        color: control.down ? Qt.darker(control.fill, 1.06) : control.fill
        border.color: control.outline
        border.width: control.outline.a > 0 ? 1 : 0
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    contentItem: Text {
        text: control.text
        color: control.ink
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}

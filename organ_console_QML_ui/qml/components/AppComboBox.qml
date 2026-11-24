import QtQuick
import QtQuick.Controls

/**
 * 统一参数下拉框。
 * 白底圆角、蓝色聚焦边框；弹出列表也使用浅色主题。
 */
ComboBox {
    id: control

    implicitWidth: 160
    implicitHeight: 40

    font.family: "Segoe UI"
    font.pixelSize: 12

    background: Rectangle {
        radius: 10
        color: control.enabled ? "#FFFFFF" : "#F4F8FB"
        border.width: control.activeFocus || control.popup.visible ? 2 : 1
        border.color: !control.enabled
            ? "#DCE7F0"
            : control.activeFocus || control.popup.visible
                ? "#69B7E8"
                : control.hovered ? "#B9D4E6" : "#DCE7F0"

        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: 34
        text: control.displayText
        font: control.font
        color: control.enabled ? "#18334E" : "#93A5B5"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - 28
        y: 0
        width: 20
        height: control.height
        text: control.popup.visible ? "⌃" : "⌄"
        color: control.enabled ? "#4F7C9E" : "#AAB8C5"
        font.family: "Segoe UI"
        font.pixelSize: 17
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 280)
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 12
            color: "#FFFFFF"
            border.width: 1
            border.color: "#DCE7F0"
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 2

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    delegate: ItemDelegate {
        id: item
        width: control.width - 12
        height: 38
        highlighted: control.highlightedIndex === index
        hoverEnabled: true

        background: Rectangle {
            radius: 8
            color: item.highlighted
                ? "#EAF5FC"
                : item.hovered ? "#F5FAFD" : "transparent"
        }

        contentItem: Text {
            leftPadding: 10
            rightPadding: 10
            text: modelData
            font.family: control.font.family
            font.pixelSize: control.font.pixelSize
            color: item.highlighted ? "#244B70" : "#18334E"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}

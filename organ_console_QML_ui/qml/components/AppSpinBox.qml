import QtQuick
import QtQuick.Controls

/**
 * 统一参数数字输入框。
 * 白底、圆角、蓝色聚焦边框，右侧提供上下步进按钮。
 */
SpinBox {
    id: control

    implicitWidth: 118
    implicitHeight: 40
    editable: true

    font.family: "Consolas"
    font.pixelSize: 12
    font.weight: Font.DemiBold

    readonly property bool editingFocus: contentItem
        && typeof contentItem.activeFocus !== "undefined"
        && contentItem.activeFocus

    background: Rectangle {
        radius: 10
        color: control.enabled ? "#FFFFFF" : "#F4F8FB"
        border.width: control.activeFocus || control.editingFocus ? 2 : 1
        border.color: !control.enabled
            ? "#DCE7F0"
            : control.activeFocus || control.editingFocus
                ? "#69B7E8"
                : control.hovered ? "#B9D4E6" : "#DCE7F0"

        Behavior on border.color { ColorAnimation { duration: 120 } }

        Rectangle {
            visible: control.editable
            x: control.width - 30
            y: 7
            width: 1
            height: Math.max(0, control.height - 14)
            color: control.enabled ? "#EAF1F7" : "#EDF2F6"
        }
    }

    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: control.enabled ? "#18334E" : "#93A5B5"
        selectionColor: "#69B7E8"
        selectedTextColor: "#FFFFFF"
        horizontalAlignment: TextInput.AlignLeft
        verticalAlignment: TextInput.AlignVCenter
        leftPadding: 12
        rightPadding: 38
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true

        onEditingFinished: {
            if (text.length === 0) {
                return
            }
            const parsed = control.valueFromText(text, control.locale)
            if (!isNaN(parsed)) {
                control.value = Math.max(control.from, Math.min(control.to, parsed))
            }
        }
    }

    up.indicator: Rectangle {
        visible: control.editable
        x: control.width - 26
        y: 5
        width: 22
        height: Math.max(10, (control.height - 12) / 2)
        radius: 5
        color: !control.enabled
            ? "transparent"
            : control.up.pressed ? "#DCEBF7"
            : control.up.hovered ? "#EDF5FB" : "transparent"

        Text {
            anchors.centerIn: parent
            text: "▲"
            color: control.enabled ? "#4F7C9E" : "#AAB8C5"
            font.pixelSize: 10
        }
    }

    down.indicator: Rectangle {
        visible: control.editable
        x: control.width - 26
        y: control.height - 5 - height
        width: 22
        height: Math.max(10, (control.height - 12) / 2)
        radius: 5
        color: !control.enabled
            ? "transparent"
            : control.down.pressed ? "#DCEBF7"
            : control.down.hovered ? "#EDF5FB" : "transparent"

        Text {
            anchors.centerIn: parent
            text: "▼"
            color: control.enabled ? "#4F7C9E" : "#AAB8C5"
            font.pixelSize: 10
        }
    }
}

import QtQuick
import QtQuick.Controls

/**
 * 统一参数滑杆。
 * 浅色轨道 + 天蓝进度 + 白色圆形手柄。
 */
Slider {
    id: control

    implicitHeight: 30

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 6
        radius: 3
        color: control.enabled ? "#E8EFF5" : "#F0F4F8"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: control.enabled ? "#69B7E8" : "#B9CBD9"
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition
            * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 18
        height: 18
        radius: 9
        color: control.pressed ? "#69B7E8" : "#FFFFFF"
        border.width: 2
        border.color: control.enabled
            ? (control.pressed ? "#4F9ED3" : "#69B7E8")
            : "#B9CBD9"

        Behavior on color { ColorAnimation { duration: 100 } }
    }
}

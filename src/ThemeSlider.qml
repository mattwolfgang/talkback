import QtQuick
import QtQuick.Controls

// The Omarchy shell's slider (Ui/PanelSlider.qml, MIT): a thin track filled
// in the foreground color, and a round knob that grows under the pointer.
Slider {
    id: control

    readonly property int trackHeight: 4
    readonly property int knobSize: 14

    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    implicitHeight: 28

    background: Rectangle {
        x: control.leftPadding + control.knobSize / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth - control.knobSize
        height: control.trackHeight
        radius: height / 2
        color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, theme.selectedFill)

        Rectangle {
            // Placed by x, which mirroring leaves alone, so a right-to-left
            // layout grows the fill from the right itself.
            x: control.mirrored ? parent.width - width : 0
            width: parent.width * control.position
            height: parent.height
            radius: parent.radius
            color: theme.foreground
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.knobSize
        height: control.knobSize
        radius: width / 2
        color: theme.foreground
        border.width: 2
        border.color: theme.background
        scale: control.hovered || control.pressed ? 1.15 : 1
        Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}

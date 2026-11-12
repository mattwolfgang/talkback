import QtQuick
import QtQuick.Controls

// The Omarchy shell's button (Ui/Button.qml, MIT): flat at rest, a faint
// foreground fill on hover and press, and a border only while it's under the
// pointer or keyboard, unless it's bordered. Primary is the action an area
// exists for, drawn in the accent.
AbstractButton {
    id: control

    property bool primary: false
    property bool selected: false
    property bool bordered: primary

    readonly property color tint: primary ? theme.accent : theme.foreground
    readonly property bool hot: hovered || visualFocus

    function shade(alpha) {
        return Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, alpha)
    }

    hoverEnabled: true
    // A click shouldn't take the arrow keys away from the voice list.
    focusPolicy: Qt.TabFocus
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6
    implicitWidth: label.implicitWidth + leftPadding + rightPadding + 2
    implicitHeight: Math.max(28, label.implicitHeight + topPadding + bottomPadding + 2)
    opacity: enabled ? 1 : 0.4

    // Held down, the button gives a little, like the shell's.
    scale: pressed ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    contentItem: Text {
        id: label
        text: control.text
        color: control.selected ? theme.foreground : control.tint
        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
        font.bold: control.selected || control.primary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: theme.radius
        color: control.pressed ? control.shade(theme.pressedFill)
             : control.hot ? control.shade(theme.hoverFill)
             : control.selected ? control.shade(theme.selectedFill)
             : "transparent"
        border.width: control.hot || control.bordered ? 1 : 0
        border.color: control.primary ? theme.accent
                    : control.hot ? control.shade(theme.hoverBorder)
                    : control.shade(theme.normalBorder)
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}

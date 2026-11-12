import QtQuick
import QtQuick.Controls

// The Omarchy shell's text field (Ui/TextField.qml, MIT): a faint fill and a
// thin border that changes with hover and focus.
TextField {
    id: control

    readonly property bool hot: hovered || activeFocus

    function shade(alpha) {
        return Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, alpha)
    }

    hoverEnabled: true
    leftPadding: 11
    rightPadding: 11
    topPadding: 8
    bottomPadding: 8
    color: theme.foreground
    selectionColor: shade(theme.selectionFill)
    selectedTextColor: theme.foreground
    placeholderTextColor: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.45)

    background: Rectangle {
        implicitHeight: 30
        radius: theme.radius
        color: control.shade(control.hot ? theme.hoverFill : theme.normalFill)
        border.width: 1
        border.color: control.shade(control.hot ? theme.hoverBorder : theme.normalBorder)
    }
}

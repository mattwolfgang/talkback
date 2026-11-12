import QtQuick
import QtQuick.Controls

// The Omarchy shell's switch (Ui/ToggleSwitch.qml, MIT): a track with a
// sliding knob, a pill when windows are rounded and square when they aren't.
// The caller owns the state: bind `checked` and change it in `switched`, so a
// refused change never shows as done.
AbstractButton {
    id: control

    signal switched(bool on)

    readonly property int trackHeight: 22
    readonly property int trackWidth: 42
    readonly property int knobSize: 16
    readonly property int pad: 6
    readonly property bool hot: hovered || visualFocus
    readonly property bool rounded: theme.radius > 0

    function shade(alpha) {
        return Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, alpha)
    }

    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    implicitWidth: trackWidth + pad * 2
    implicitHeight: trackHeight + pad * 2
    opacity: enabled ? 1 : 0.4
    onClicked: switched(!checked)

    // The hover ring sits outside the track, so a bordered track never goes
    // fainter under the pointer.
    background: Rectangle {
        visible: control.hot
        color: "transparent"
        radius: theme.radius
        border.width: 1
        border.color: control.shade(theme.hoverBorder)
    }

    contentItem: Item {
        Rectangle {
            id: track
            width: control.trackWidth
            height: control.trackHeight
            anchors.centerIn: parent
            radius: control.rounded ? height / 2 : 0
            color: control.shade(control.checked ? theme.selectedFill : theme.normalFill)
            border.width: control.checked ? 0 : 1
            border.color: control.shade(theme.normalBorder)
            Behavior on color { ColorAnimation { duration: 120 } }

            Rectangle {
                readonly property int inset: (track.height - height) / 2
                width: control.knobSize
                height: control.knobSize
                radius: control.rounded ? height / 2 : 0
                anchors.verticalCenter: parent.verticalCenter
                // Placed by x, so a right-to-left layout throws it the other way.
                x: control.checked !== control.mirrored ? track.width - width - inset : inset
                color: control.checked ? theme.foreground : Qt.darker(theme.foreground, 1.25)
                Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            }
        }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}

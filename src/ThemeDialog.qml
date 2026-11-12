import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A card like the shell's popups: the theme background inside the same
// border as the active window, over a dimmed window. Its buttons go in
// `actions`, at the bottom right.
Dialog {
    id: dialog

    default property alias content: body.data
    property alias actions: actionRow.data

    modal: true
    padding: 20
    topPadding: 4

    Overlay.modal: Rectangle {
        color: Qt.rgba(theme.background.r, theme.background.g, theme.background.b, 0.6)
    }

    background: Rectangle {
        color: theme.background
        radius: theme.radius
        border.width: 2
        border.color: theme.popupBorder
    }

    header: Label {
        text: dialog.title
        visible: text.length > 0
        font.bold: true
        font.pixelSize: Math.round(theme.fontSize * 1.333)
        padding: 20
        bottomPadding: 8
        elide: Text.ElideRight
    }

    contentItem: ColumnLayout { id: body; spacing: 10 }

    footer: Item {
        implicitHeight: actionRow.implicitHeight + 32
        RowLayout {
            id: actionRow
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -4
            spacing: 8
        }
    }
}

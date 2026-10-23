import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 860
    height: 800
    minimumWidth: 640
    minimumHeight: 580
    visible: true
    title: "Talkback"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property color panelColor: "#17171a"
    readonly property color borderColor: "#27272c"
    readonly property color mutedColor: "#9b9ba3"
    readonly property color warningColor: "#ff8a80"

    property bool helpVisible: false
    // Single-letter shortcuts stand aside while a text field has the keys.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition")
    readonly property var selected: list.currentIndex >= 0 && list.currentIndex < voices.voices.length
                                    ? voices.voices[list.currentIndex] : null
    readonly property string sample: sampleField.text.length > 0 ? sampleField.text : sampleField.placeholderText
    readonly property var current: { voices.voices; return voices.voice(settings.voice) }

    function describe(voice) {
        if (!voice || !voice.key)
            return settings.voice
        const place = voice.country ? qsTr("%1 (%2)").arg(voice.language).arg(voice.country) : voice.language
        return qsTr("%1 · %2").arg(place).arg(voice.name)
    }

    function preview(key) {
        if (voices.isInstalled(key))
            speaker.say(win.sample, key, settings.speed)
    }

    function previewSelected() {
        preview(win.selected && win.selected.installed ? win.selected.key : settings.voice)
    }

    function activate(voice) {
        if (!voice)
            return
        if (voice.installed)
            settings.voice = voice.key
        else
            voices.download(voice.key)
    }

    function askRemove(voice) {
        if (voice && voice.installed && voice.key !== settings.voice) {
            removeDialog.key = voice.key
            removeDialog.open()
        }
    }

    function nudgeSpeed(step) {
        settings.speed = settings.speed + step
    }

    Connections {
        target: voices
        // Hearing a voice is the reason to download it.
        function onDownloaded(key) { win.preview(key) }
    }

    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.previewSelected() }
    Shortcut { sequences: ["Return", "Enter"]; enabled: !win.typing || searchField.activeFocus; context: Qt.ApplicationShortcut; onActivated: win.activate(win.selected) }
    Shortcut { sequence: "D"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.selected && !win.selected.installed) voices.download(win.selected.key) }
    Shortcut { sequence: "Delete"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.askRemove(win.selected) }
    Shortcut { sequence: "S"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: settings.enabled = !settings.enabled }
    Shortcut { sequence: "V"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: dictation.setActive(!dictation.active) }
    Shortcut { sequence: "I"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: voices.installedOnly = !voices.installedOnly }
    Shortcut { sequences: ["+", "="]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.nudgeSpeed(0.05) }
    Shortcut { sequence: "-"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.nudgeSpeed(-0.05) }
    Shortcut { sequence: "/"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: searchField.forceActiveFocus() }
    Shortcut { sequences: [StandardKey.Find]; context: Qt.ApplicationShortcut; onActivated: searchField.forceActiveFocus() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (win.typing)
                list.forceActiveFocus()
            else
                speaker.stop()
        }
    }

    // Material buttons are pills by default; these follow the window corners.
    component AppButton: Button {
        Material.roundedScale: theme.radius
    }

    component Panel: Rectangle {
        color: win.panelColor
        border.color: win.borderColor
        radius: theme.radius
    }

    component Hint: Label {
        color: win.mutedColor
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    component ToggleCard: Panel {
        id: card
        property string heading
        property string detail
        property string keys
        property bool on
        property bool canToggle: true
        property string note
        signal toggled(bool on)
        default property alias extra: extraColumn.data

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
        implicitHeight: cardLayout.implicitHeight + 32

        ColumnLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: card.heading; font.pixelSize: 16; font.bold: true }
                    Hint { text: card.detail }
                }
                Switch {
                    id: toggle
                    checked: card.on
                    enabled: card.canToggle
                    onToggled: {
                        card.toggled(checked)
                        // The real state decides, so a refused change flips back.
                        checked = Qt.binding(() => card.on)
                    }
                }
            }
            Hint { text: card.keys; color: Qt.lighter(win.mutedColor, 1.15) }
            Hint { visible: text.length > 0; text: card.note; color: win.warningColor }
            ColumnLayout { id: extraColumn; Layout.fillWidth: true }
            Item { Layout.fillHeight: true }
        }
    }

    Dialog {
        id: removeDialog
        property string key
        anchors.centerIn: parent
        modal: true
        title: qsTr("Remove %1?").arg(key)
        standardButtons: Dialog.Ok | Dialog.Cancel
        Material.roundedScale: theme.radius
        Label { text: qsTr("You can download it again any time.") }
        onAccepted: voices.remove(key)
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Talkback"; font.pixelSize: 24; font.bold: true }
                Item { Layout.fillWidth: true }
                Hint { Layout.fillWidth: false; text: qsTr("Press ? for shortcuts") }
            }

            GridLayout {
                Layout.fillWidth: true
                // The cards share a row height, but the voice list gets the rest.
                Layout.fillHeight: false
                columns: win.width >= 760 ? 2 : 1
                columnSpacing: 14
                rowSpacing: 14

                ToggleCard {
                    heading: qsTr("Spoken replies")
                    detail: qsTr("Read Claude's replies aloud when they finish.")
                    keys: qsTr("S here · Super + Alt + X anywhere")
                    on: settings.enabled
                    note: speaker.error
                    onToggled: on => settings.enabled = on

                    RowLayout {
                        visible: !speaker.hookInstalled
                        Layout.fillWidth: true
                        Hint { text: qsTr("Claude Code isn't set up to use Talkback yet."); color: win.warningColor }
                        AppButton {
                            text: qsTr("Set up")
                            Material.background: Material.accentColor
                            Material.foreground: theme.accentForeground
                            Material.roundedScale: theme.radius
                            onClicked: speaker.installHook()
                        }
                    }
                }

                ToggleCard {
                    heading: qsTr("Dictation")
                    detail: qsTr("Let Voxtype listen for the dictation keys (F9, Right Alt, Super + Ctrl + X).")
                    keys: qsTr("V here · Super + Ctrl + Alt + X anywhere")
                    on: dictation.active
                    canToggle: dictation.available && !dictation.busy
                    note: !dictation.available ? qsTr("Voxtype isn't set up. Pick it under Setup > Defaults > Dictation.")
                                               : dictation.error
                    onToggled: on => dictation.setActive(on)
                }
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: voiceLayout.implicitHeight + 32

                ColumnLayout {
                    id: voiceLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Voice"); font.pixelSize: 16; font.bold: true }
                        Label {
                            Layout.fillWidth: true
                            text: win.describe(win.current)
                            elide: Text.ElideRight
                            color: Material.accentColor
                        }
                    }
                    Hint {
                        visible: voices.installedCount >= 0 && !voices.isInstalled(settings.voice) && !voices.loading
                        text: qsTr("%1 isn't downloaded, so replies stay silent. Pick a downloaded voice below.").arg(settings.voice)
                        color: win.warningColor
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: qsTr("Speed") }
                        Slider {
                            id: speedSlider
                            Layout.fillWidth: true
                            from: 0.5
                            to: 2.0
                            stepSize: 0.05
                            value: settings.speed
                            onMoved: settings.speed = value
                        }
                        Label {
                            Layout.preferredWidth: 52
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1×").arg(settings.speed.toLocaleString(Qt.locale(), 'f', 2))
                            font.features: { "tnum": 1 }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        TextField {
                            id: sampleField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Hello! This is how Claude's replies will sound.")
                            onAccepted: win.preview(settings.voice)
                        }
                        AppButton {
                            text: qsTr("Play")
                            highlighted: true
                            Material.background: Material.accentColor
                            Material.foreground: theme.accentForeground
                            Material.roundedScale: theme.radius
                            enabled: voices.installedCount >= 0 && voices.isInstalled(settings.voice)
                            onClicked: win.preview(settings.voice)
                        }
                        AppButton { text: qsTr("Stop"); flat: true; onClicked: speaker.stop() }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search voices by language, name or quality")
                    onTextChanged: voices.filter = text
                    Keys.onUpPressed: list.decrementCurrentIndex()
                    Keys.onDownPressed: list.incrementCurrentIndex()
                }
                CheckBox {
                    text: qsTr("Downloaded only")
                    checked: voices.installedOnly
                    onToggled: voices.installedOnly = checked
                }
            }

            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: list
                    anchors.fill: parent
                    anchors.margins: 1
                    focus: true
                    clip: true
                    model: voices.voices
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool inUse: modelData.key === settings.voice
                        readonly property bool downloadingThis: voices.downloading === modelData.key
                        width: ListView.view.width
                        height: 58

                        Rectangle {
                            anchors.fill: parent
                            color: row.ListView.isCurrentItem ? "#222228" : "transparent"
                        }
                        Rectangle {
                            visible: row.inUse
                            width: 3
                            height: parent.height
                            color: Material.accentColor
                        }
                        TapHandler {
                            onTapped: { list.currentIndex = row.index; list.forceActiveFocus() }
                            onDoubleTapped: win.activate(row.modelData)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 12
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: row.modelData.country
                                          ? qsTr("%1 (%2)").arg(row.modelData.language).arg(row.modelData.country)
                                          : row.modelData.language
                                }
                                Hint {
                                    wrapMode: Text.NoWrap
                                    elide: Text.ElideRight
                                    text: {
                                        let parts = [row.modelData.name]
                                        if (row.modelData.quality)
                                            parts.push(row.modelData.quality.replace("_", " "))
                                        if (row.modelData.speakers > 1)
                                            parts.push(qsTr("%n speakers (plays the first)", "", row.modelData.speakers))
                                        if (!row.modelData.installed && row.modelData.megabytes > 0)
                                            parts.push(qsTr("%1 MB").arg(row.modelData.megabytes.toLocaleString(Qt.locale(), 'f', 0)))
                                        return parts.join(" · ")
                                    }
                                }
                            }

                            AppButton {
                                visible: row.modelData.installed
                                text: qsTr("Play")
                                flat: true
                                onClicked: win.preview(row.modelData.key)
                            }
                            Label {
                                visible: row.inUse
                                text: qsTr("In use")
                                color: Material.accentColor
                                Layout.preferredWidth: useButton.implicitWidth
                                horizontalAlignment: Text.AlignHCenter
                            }
                            AppButton {
                                id: useButton
                                visible: row.modelData.installed && !row.inUse
                                text: qsTr("Use")
                                onClicked: settings.voice = row.modelData.key
                            }
                            AppButton {
                                visible: row.modelData.installed && !row.inUse
                                text: qsTr("Remove")
                                flat: true
                                onClicked: win.askRemove(row.modelData)
                            }
                            BusyIndicator {
                                visible: row.downloadingThis
                                running: visible
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                            }
                            AppButton {
                                visible: !row.modelData.installed
                                text: row.downloadingThis ? qsTr("Downloading…") : qsTr("Download")
                                enabled: voices.downloading === ""
                                onClicked: voices.download(row.modelData.key)
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: win.borderColor
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        color: win.mutedColor
                        text: voices.loading ? qsTr("Loading Piper's voice list…")
                            : searchField.text.length > 0 ? qsTr("No voices match “%1”.").arg(searchField.text)
                            : voices.installedOnly ? qsTr("No voices downloaded yet.")
                            : qsTr("No voice list yet.")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Hint {
                    text: voices.piperMissing ? qsTr("piper-tts isn't installed, so nothing can be spoken. Install the piper-tts package.")
                        : voices.error.length > 0 ? voices.error
                        : qsTr("%n voices", "", list.count) + " · " + qsTr("%n downloaded", "", voices.installedCount)
                    color: voices.piperMissing || voices.error.length > 0 ? win.warningColor : win.mutedColor
                }
                BusyIndicator { visible: voices.loading; running: visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#e60e0e10"

            TapHandler { onTapped: win.helpVisible = false }

            Panel {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 520)
                height: helpColumn.implicitHeight + 40

                ColumnLayout {
                    id: helpColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 8

                    Label { text: qsTr("Shortcuts"); font.pixelSize: 18; font.bold: true }

                    GridLayout {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 6
                        Repeater {
                            model: [
                                ["Space", qsTr("Play the selected voice")],
                                ["Enter", qsTr("Use the selected voice, or download it")],
                                ["D", qsTr("Download the selected voice")],
                                ["Delete", qsTr("Remove the selected voice")],
                                ["↑ ↓", qsTr("Move through the voices")],
                                ["/", qsTr("Search voices")],
                                ["I", qsTr("Show downloaded voices only")],
                                ["+ −", qsTr("Speak faster or slower")],
                                ["S", qsTr("Turn spoken replies on or off")],
                                ["V", qsTr("Turn dictation on or off")],
                                ["Esc", qsTr("Stop speaking")],
                                ["?", qsTr("Show or hide this list")],
                                ["Q", qsTr("Quit")],
                                ["Super + Alt + X", qsTr("Spoken replies, from anywhere")],
                                ["Super + Ctrl + Alt + X", qsTr("Dictation, from anywhere")]
                            ]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.columnSpan: 2
                                Label {
                                    text: parent.modelData[0]
                                    Layout.preferredWidth: 190
                                    color: Material.accentColor
                                    font.family: "monospace"
                                }
                                Label { text: parent.modelData[1]; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                }
            }
        }
    }
}

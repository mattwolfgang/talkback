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
    readonly property bool kokoroMode: settings.engine === "kokoro"
    readonly property string currentVoiceKey: kokoroMode ? settings.kokoroVoice : settings.voice
    readonly property var listModel: kokoroMode ? kokoro.voices : voices.voices
    readonly property var selected: list.currentIndex >= 0 && list.currentIndex < listModel.length
                                    ? listModel[list.currentIndex] : null
    readonly property string sample: sampleField.text.length > 0 ? sampleField.text : sampleField.placeholderText
    readonly property var current: {
        voices.voices
        kokoro.voices
        return kokoroMode ? kokoro.voice(settings.kokoroVoice) : voices.voice(settings.voice)
    }

    function describe(voice) {
        if (!voice || !voice.key)
            return win.currentVoiceKey
        const place = voice.country ? qsTr("%1 (%2)").arg(voice.language).arg(voice.country) : voice.language
        return qsTr("%1 · %2").arg(place).arg(voice.name)
    }

    function preview(key) {
        if (win.kokoroMode) {
            if (kokoro.installed)
                speaker.say(win.sample, "kokoro", key, settings)
        } else if (voices.isInstalled(key)) {
            speaker.say(win.sample, "piper", key, settings)
        }
    }

    function previewSelected() {
        preview(win.selected && win.selected.installed ? win.selected.key : win.currentVoiceKey)
    }

    function switchEngine() {
        if (win.kokoroMode)
            settings.engine = "piper"
        else
            settings.engine = "kokoro"
        list.currentIndex = 0
    }

    function activate(voice) {
        if (!voice)
            return
        if (voice.engine === "kokoro")
            settings.kokoroVoice = voice.key
        else if (voice.installed)
            settings.voice = voice.key
        else
            voices.download(voice.key)
    }

    function askRemove(voice) {
        if (voice && voice.installed && voice.engine !== "kokoro" && voice.key !== settings.voice) {
            removeDialog.key = voice.key
            removeDialog.open()
        }
    }

    readonly property int speakerCount: win.current && win.current.speakers ? win.current.speakers : 1

    // Steps through the current voice's speakers, wrapping at the ends, and
    // plays each one so they can be compared by ear.
    function stepSpeaker(step) {
        if (win.speakerCount < 2)
            return
        const next = (settings.speaker + step + win.speakerCount) % win.speakerCount
        settings.setSpeaker(settings.voice, next)
        win.preview(settings.voice)
    }

    function randomSpeaker() {
        if (win.speakerCount < 2)
            return
        settings.setSpeaker(settings.voice, Math.floor(Math.random() * win.speakerCount))
        win.preview(settings.voice)
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
    Shortcut { sequence: "E"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.switchEngine() }
    Shortcut { sequence: "I"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: voices.installedOnly = !voices.installedOnly }
    Shortcut { sequences: ["+", "="]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.nudgeSpeed(0.05) }
    Shortcut { sequence: "-"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.nudgeSpeed(-0.05) }
    Shortcut { sequence: "["; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.stepSpeaker(-1) }
    Shortcut { sequence: "]"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.stepSpeaker(1) }
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

    // One row of the voice panel: a name, a slider, and its value.
    component SettingSlider: RowLayout {
        id: setting
        property string label
        property string tip
        property real value
        property real from
        property real to
        property real step: 0.05
        property string valueText
        signal moved(real value)

        Layout.fillWidth: true
        spacing: 10
        Label {
            text: setting.label
            Layout.preferredWidth: 130
            ToolTip.visible: tipArea.hovered && setting.tip.length > 0
            ToolTip.text: setting.tip
            ToolTip.delay: 400
            HoverHandler { id: tipArea }
        }
        Slider {
            Layout.fillWidth: true
            from: setting.from
            to: setting.to
            stepSize: setting.step
            value: setting.value
            onMoved: setting.moved(value)
        }
        Label {
            Layout.preferredWidth: 60
            horizontalAlignment: Text.AlignRight
            text: setting.valueText
            font.features: { "tnum": 1 }
        }
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
                    keys: qsTr("V here")
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
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: qsTr("Engine"); Layout.preferredWidth: 130 }
                        TabBar {
                            id: engineBar
                            currentIndex: win.kokoroMode ? 1 : 0
                            Material.roundedScale: theme.radius
                            TabButton { text: "Piper"; width: implicitWidth + 24; onClicked: if (win.kokoroMode) win.switchEngine() }
                            TabButton { text: "Kokoro"; width: implicitWidth + 24; onClicked: if (!win.kokoroMode) win.switchEngine() }
                        }
                        Hint {
                            Layout.fillWidth: true
                            text: win.kokoroMode ? qsTr("More natural; about a second to start. E switches.")
                                                 : qsTr("Fast and light, with hundreds of voices. E switches.")
                        }
                    }
                    Hint {
                        visible: !win.kokoroMode && voices.installedCount >= 0 && !voices.isInstalled(settings.voice) && !voices.loading
                        text: qsTr("%1 isn't downloaded, so replies stay silent. Pick a downloaded voice below.").arg(settings.voice)
                        color: win.warningColor
                    }

                    RowLayout {
                        visible: win.speakerCount > 1
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: qsTr("Speaker"); Layout.preferredWidth: 130 }
                        SpinBox {
                            id: speakerBox
                            from: 0
                            to: Math.max(0, win.speakerCount - 1)
                            value: settings.speaker
                            editable: true
                            Material.roundedScale: theme.radius
                            onValueModified: {
                                settings.setSpeaker(settings.voice, value)
                                value = Qt.binding(() => settings.speaker)
                                win.preview(settings.voice)
                            }
                        }
                        Hint {
                            Layout.fillWidth: true
                            text: qsTr("0 to %1 · [ and ] step and play").arg(win.speakerCount - 1)
                        }
                        AppButton { text: qsTr("Random"); flat: true; onClicked: win.randomSpeaker() }
                    }
                    SettingSlider {
                        label: qsTr("Speed")
                        from: 0.5
                        to: 2.0
                        value: settings.speed
                        valueText: qsTr("%1×").arg(settings.speed.toLocaleString(Qt.locale(), 'f', 2))
                        onMoved: value => settings.speed = value
                    }
                    SettingSlider {
                        visible: !win.kokoroMode
                        label: qsTr("Expression")
                        tip: qsTr("How much the pitch and tone vary. Higher sounds livelier; too high gets wobbly.")
                        from: 0
                        to: 1.5
                        value: settings.expression
                        valueText: settings.expression.toLocaleString(Qt.locale(), 'f', 2)
                        onMoved: value => settings.expression = value
                    }
                    SettingSlider {
                        visible: !win.kokoroMode
                        label: qsTr("Rhythm")
                        tip: qsTr("How much the length of each sound varies. Higher sounds less mechanical; too high stumbles.")
                        from: 0
                        to: 1.5
                        value: settings.rhythm
                        valueText: settings.rhythm.toLocaleString(Qt.locale(), 'f', 2)
                        onMoved: value => settings.rhythm = value
                    }
                    SettingSlider {
                        label: qsTr("Sentence pause")
                        tip: qsTr("Silence after each sentence.")
                        from: 0
                        to: 1.5
                        value: settings.pause
                        valueText: qsTr("%1 s").arg(settings.pause.toLocaleString(Qt.locale(), 'f', 2))
                        onMoved: value => settings.pause = value
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        TextField {
                            id: sampleField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Hello! This is how replies will sound. Here is a second sentence.")
                            onAccepted: win.preview(settings.voice)
                        }
                        AppButton {
                            text: qsTr("Play")
                            highlighted: true
                            Material.background: Material.accentColor
                            Material.foreground: theme.accentForeground
                            Material.roundedScale: theme.radius
                            enabled: win.kokoroMode ? kokoro.installed
                                                    : voices.installedCount >= 0 && voices.isInstalled(settings.voice)
                            onClicked: win.preview(settings.voice)
                        }
                        AppButton { text: qsTr("Stop"); flat: true; onClicked: speaker.stop() }
                        AppButton {
                            visible: !win.kokoroMode
                            text: qsTr("Reset")
                            flat: true
                            onClicked: settings.resetNaturalness()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Put expression, rhythm and pause back to their defaults")
                            ToolTip.delay: 400
                        }
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
                    onTextChanged: {
                        voices.filter = text
                        kokoro.filter = text
                    }
                    Keys.onUpPressed: list.decrementCurrentIndex()
                    Keys.onDownPressed: list.incrementCurrentIndex()
                }
                CheckBox {
                    visible: !win.kokoroMode
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
                    model: win.listModel
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool inUse: modelData.key === win.currentVoiceKey
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
                                        if (row.modelData.speakers > 1) {
                                            parts.push(qsTr("%n speakers", "", row.modelData.speakers))
                                            if (row.modelData.installed)
                                                parts.push(qsTr("speaker %1").arg(settings.speakers[row.modelData.key] || 0))
                                        }
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
                                onClicked: win.activate(row.modelData)
                            }
                            AppButton {
                                visible: row.modelData.installed && !row.inUse && row.modelData.engine !== "kokoro"
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

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 40, 460)
                        visible: win.kokoroMode && !kokoro.installed
                        spacing: 12
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Kokoro isn't installed yet. It needs about 350 MB: the kokoro-onnx Python package and its model and voices.")
                        }
                        AppButton {
                            Layout.alignment: Qt.AlignHCenter
                            visible: !kokoro.installing
                            text: qsTr("Install Kokoro")
                            Material.background: Material.accentColor
                            Material.foreground: theme.accentForeground
                            onClicked: kokoro.install()
                        }
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            visible: kokoro.installing
                            BusyIndicator { running: parent.visible; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                            Hint { Layout.fillWidth: false; text: kokoro.progress }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0 && !(win.kokoroMode && !kokoro.installed)
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
                    readonly property string problem: win.kokoroMode ? kokoro.error
                        : voices.piperMissing ? qsTr("piper-tts isn't installed, so nothing can be spoken. Install the piper-tts package.")
                        : voices.error
                    text: problem.length > 0 ? problem
                        : win.kokoroMode ? (kokoro.installed ? qsTr("%n Kokoro voices", "", list.count) : "")
                        : qsTr("%n voices", "", list.count) + " · " + qsTr("%n downloaded", "", voices.installedCount)
                    color: problem.length > 0 ? win.warningColor : win.mutedColor
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
                                ["E", qsTr("Switch between Piper and Kokoro")],
                                ["I", qsTr("Show downloaded voices only")],
                                ["+ −", qsTr("Speak faster or slower")],
                                ["[ ]", qsTr("Previous or next speaker, and play it")],
                                ["S", qsTr("Turn spoken replies on or off")],
                                ["V", qsTr("Turn dictation on or off")],
                                ["Esc", qsTr("Stop speaking")],
                                ["?", qsTr("Show or hide this list")],
                                ["Q", qsTr("Quit")],
                                ["Super + Alt + X", qsTr("Spoken replies, from anywhere")]
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

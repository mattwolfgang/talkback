import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 860
    height: 800
    minimumWidth: 640
    minimumHeight: 580
    visible: true
    title: "Talkback"
    color: theme.background

    // The shell draws everything in the fontconfig "monospace" family, which
    // omarchy-font-set points at the chosen font, on a 12 px base.
    font.family: "monospace"
    font.pixelSize: theme.fontSize

    // Colors for the stock controls that are left (labels, tooltips, scroll
    // bars, busy indicators), so they follow the theme too.
    palette.window: theme.background
    palette.windowText: theme.foreground
    palette.base: theme.background
    palette.text: theme.foreground
    palette.button: theme.background
    palette.buttonText: theme.foreground
    palette.highlight: theme.accent
    palette.highlightedText: theme.accentForeground
    palette.toolTipBase: theme.background
    palette.toolTipText: theme.foreground
    palette.mid: shade(0.3)
    palette.dark: theme.foreground

    // The shell's type scale, rooted at the theme's base size.
    readonly property int bodySmall: Math.round(theme.fontSize * 0.917)
    readonly property int heading: Math.round(theme.fontSize * 1.333)
    readonly property int display: Math.round(theme.fontSize * 2)

    // Panels are a faint wash of the foreground with a separator-strength
    // edge, as in the shell; hints sit between the text and the background.
    readonly property color panelColor: shade(theme.normalFill)
    readonly property color borderColor: shade(0.12)
    readonly property color mutedColor: Qt.tint(theme.foreground, Qt.rgba(theme.background.r, theme.background.g, theme.background.b, 0.4))
    readonly property color warningColor: theme.urgent

    function shade(alpha) {
        return Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, alpha)
    }

    property bool helpVisible: false
    // Recording a new key for the keys dialog; every shortcut stands aside.
    readonly property bool capturing: keysDialog.capturing !== ""
    // Single-letter shortcuts stand aside while a text field has the keys.
    readonly property bool typing: capturing || (activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition"))
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
    Shortcut { sequence: "E"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.switchEngine() }
    Shortcut { sequence: "I"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: voices.installedOnly = !voices.installedOnly }
    Shortcut { sequences: ["+", "="]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.nudgeSpeed(0.05) }
    Shortcut { sequence: "-"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.nudgeSpeed(-0.05) }
    Shortcut { sequence: "["; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.stepSpeaker(-1) }
    Shortcut { sequence: "]"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.stepSpeaker(1) }
    Shortcut { sequence: "/"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: searchField.forceActiveFocus() }
    Shortcut { sequences: [StandardKey.Find]; enabled: !win.capturing; context: Qt.ApplicationShortcut; onActivated: searchField.forceActiveFocus() }
    Shortcut {
        sequence: "Escape"
        enabled: !win.capturing
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

    component Panel: Rectangle {
        color: win.panelColor
        border.color: win.borderColor
        radius: theme.radius
    }

    component Hint: Label {
        color: win.mutedColor
        font.pixelSize: win.bodySmall
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
        ThemeSlider {
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

    // A switch on one line: the switch, its name, and the keys for it. What
    // it does shows when the name is hovered, so many switches fit in little room.
    component SwitchRow: RowLayout {
        id: row
        property string label
        property string tip
        property string keys
        property bool on
        property bool canToggle: true
        signal toggled(bool on)

        Layout.fillWidth: true
        spacing: 6
        ThemeSwitch {
            checked: row.on
            enabled: row.canToggle
            onSwitched: on => row.toggled(on)
        }
        Label {
            text: row.label
            font.bold: true
            ToolTip.visible: nameHover.hovered && row.tip.length > 0
            ToolTip.text: row.tip
            ToolTip.delay: 400
            HoverHandler { id: nameHover }
            // The name toggles too, as a label beside a switch does.
            TapHandler { enabled: row.canToggle; onTapped: row.toggled(!row.on) }
        }
        Hint {
            text: row.keys
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
        }
    }

    ThemeDialog {
        id: removeDialog
        property string key
        anchors.centerIn: parent
        width: Math.min(win.width - 40, 420)
        title: qsTr("Remove %1?").arg(key)
        Label { text: qsTr("You can download it again any time.") }
        actions: [
            ThemeButton { text: qsTr("Cancel"); onClicked: removeDialog.reject() },
            ThemeButton { text: qsTr("Remove"); primary: true; onClicked: removeDialog.accept() }
        ]
        onAccepted: voices.remove(key)
    }

    ThemeDialog {
        id: keysDialog
        // "toggle" or "stop" while recording that action's new key.
        property string capturing: ""
        anchors.centerIn: parent
        width: Math.min(win.width - 40, 520)
        title: qsTr("Keys that work from anywhere")
        actions: ThemeButton { text: qsTr("Close"); enabled: !keysDialog.capturing; onClicked: keysDialog.close() }
        // Esc can be part of a key, so it only closes the dialog between recordings.
        closePolicy: capturing ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: capturing = ""

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Repeater {
                model: [
                    { action: "toggle", name: qsTr("Turn spoken replies on or off") },
                    { action: "stop", name: qsTr("Stop speaking") }
                ]
                delegate: RowLayout {
                    required property var modelData
                    readonly property string key: modelData.action === "toggle" ? keybindings.toggleKey : keybindings.stopKey
                    readonly property bool recording: keysDialog.capturing === modelData.action
                    Layout.fillWidth: true
                    spacing: 12
                    Label { text: parent.modelData.name; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Label {
                        text: parent.recording ? qsTr("Press keys…") : parent.key ? keybindings.label(parent.key) : qsTr("Not bound")
                        color: parent.key || parent.recording ? theme.accent : win.warningColor
                    }
                    ThemeButton {
                        bordered: true
                        text: parent.recording ? qsTr("Cancel") : qsTr("Change")
                        onClicked: {
                            keysDialog.capturing = parent.recording ? "" : parent.modelData.action
                            catcher.forceActiveFocus()
                        }
                    }
                }
            }

            Hint {
                text: keysDialog.capturing
                    ? qsTr("Hold Super, Alt or Ctrl and press a key. Keys Hyprland already uses don't reach this window. Esc on its own cancels.")
                    : qsTr("Saved in your Hyprland bindings, which reload right away.")
            }
            Hint { visible: text.length > 0; text: keybindings.error; color: win.warningColor }

            // Takes the keys while recording, so they reach no other control.
            Item {
                id: catcher
                Keys.onPressed: event => {
                    if (!keysDialog.capturing)
                        return
                    event.accepted = true
                    const parts = []
                    if (event.modifiers & Qt.MetaModifier) parts.push("SUPER")
                    if (event.modifiers & Qt.ControlModifier) parts.push("CTRL")
                    if (event.modifiers & Qt.AltModifier) parts.push("ALT")
                    if (event.modifiers & Qt.ShiftModifier) parts.push("SHIFT")
                    if (event.key === Qt.Key_Escape && parts.length === 0) {
                        keysDialog.capturing = ""
                        return
                    }
                    // Empty for a modifier on its own: wait for the key.
                    const key = keybindings.keyName(event.key)
                    if (!key)
                        return
                    parts.push(key)
                    if (keybindings.setKey(keysDialog.capturing, parts.join(" + ")))
                        keysDialog.capturing = ""
                }
            }
        }
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
                Label { text: "Talkback"; font.pixelSize: win.display; font.bold: true }
                Item { Layout.fillWidth: true }
                Hint { Layout.fillWidth: false; text: qsTr("Press ? for shortcuts") }
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: switchLayout.implicitHeight + 20

                ColumnLayout {
                    id: switchLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        SwitchRow {
                            label: qsTr("Spoken replies")
                            tip: [qsTr("Read your coding agents' replies aloud when they finish."),
                                  keybindings.stopKey ? qsTr("%1 stops a reply").arg(keybindings.label(keybindings.stopKey)) : ""]
                                 .filter(part => part.length > 0).join("\n")
                            keys: [qsTr("S here"),
                                   keybindings.toggleKey ? qsTr("%1 anywhere").arg(keybindings.label(keybindings.toggleKey)) : ""]
                                  .filter(part => part.length > 0).join(" · ")
                            on: settings.enabled
                            onToggled: on => settings.enabled = on
                        }
                        ThemeButton {
                            visible: keybindings.available
                            text: qsTr("Change keys")
                            onClicked: keysDialog.open()
                        }
                    }

                    // Each agent's own switch, as a chip that's filled while
                    // its replies are read. They only matter while replies are on.
                    RowLayout {
                        Layout.fillWidth: true
                        // Lines the chips up with the switch names above.
                        Layout.leftMargin: 6
                        spacing: 8
                        visible: speaker.hookedAgents.length > 0
                        Hint {
                            Layout.fillWidth: false
                            text: qsTr("Agents")
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 4
                            opacity: settings.enabled ? 1 : 0.45
                            Repeater {
                                model: speaker.hookedAgents
                                delegate: ThemeButton {
                                    required property var modelData
                                    readonly property bool on: settings.mutedAgents.indexOf(modelData.id) < 0
                                    text: modelData.name
                                    selected: on
                                    bordered: !on
                                    opacity: on ? 1 : 0.5
                                    topPadding: 4
                                    bottomPadding: 4
                                    onClicked: settings.setAgentEnabled(modelData.id, !on)
                                }
                            }
                        }
                    }

                    // Only what needs fixing gets a line of its own.
                    Hint { visible: text.length > 0; text: speaker.error; color: win.warningColor }
                    RowLayout {
                        visible: !speaker.hookInstalled
                        Layout.fillWidth: true
                        Hint { text: qsTr("No coding agent is set up to use Talkback yet."); color: win.warningColor }
                        ThemeButton {
                            text: qsTr("Set up")
                            primary: true
                            onClicked: {
                                speaker.installHook()
                                if (keybindings.available)
                                    keybindings.install()
                                if (barPlugin.available && !barPlugin.installed)
                                    barPlugin.install()
                            }
                        }
                    }
                    RowLayout {
                        visible: !keybindings.toggleKey || !keybindings.stopKey
                        Layout.fillWidth: true
                        Hint {
                            text: !keybindings.available ? qsTr("No keys work from anywhere yet. Add them to your Hyprland config; the README has the lines.")
                                : !keybindings.toggleKey && !keybindings.stopKey ? qsTr("The keys that work from anywhere aren't bound yet.")
                                : !keybindings.stopKey ? qsTr("No key stops a reply from anywhere yet.")
                                : qsTr("No key turns spoken replies on and off from anywhere yet.")
                            color: win.warningColor
                        }
                        ThemeButton {
                            visible: keybindings.available
                            text: qsTr("Bind keys")
                            primary: true
                            // A default key that's taken needs picking by hand.
                            onClicked: if (!keybindings.install()) keysDialog.open()
                        }
                    }
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
                        Label { text: qsTr("Voice"); font.pixelSize: win.heading; font.bold: true }
                        Label {
                            Layout.fillWidth: true
                            text: win.describe(win.current)
                            elide: Text.ElideRight
                            color: theme.accent
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: qsTr("Engine"); Layout.preferredWidth: 130 }
                        // A button group, like the shell's: the chosen one is filled.
                        Row {
                            spacing: 4
                            ThemeButton { text: "Piper"; selected: !win.kokoroMode; onClicked: if (win.kokoroMode) win.switchEngine() }
                            ThemeButton { text: "Kokoro"; selected: win.kokoroMode; onClicked: if (!win.kokoroMode) win.switchEngine() }
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
                        ThemeButton { text: "−"; onClicked: win.stepSpeaker(-1) }
                        ThemeField {
                            id: speakerField
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignHCenter
                            text: settings.speaker
                            validator: IntValidator { bottom: 0; top: Math.max(0, win.speakerCount - 1) }
                            onEditingFinished: {
                                if (acceptableInput && Number(text) !== settings.speaker) {
                                    settings.setSpeaker(settings.voice, Number(text))
                                    win.preview(settings.voice)
                                }
                                text = Qt.binding(() => settings.speaker)
                            }
                        }
                        ThemeButton { text: "+"; onClicked: win.stepSpeaker(1) }
                        Hint {
                            Layout.fillWidth: true
                            text: qsTr("0 to %1 · [ and ] step and play").arg(win.speakerCount - 1)
                        }
                        ThemeButton { text: qsTr("Random"); onClicked: win.randomSpeaker() }
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
                        ThemeField {
                            id: sampleField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Hello! This is how replies will sound. Here is a second sentence.")
                            onAccepted: win.preview(settings.voice)
                        }
                        ThemeButton {
                            text: qsTr("Play")
                            primary: true
                            enabled: win.kokoroMode ? kokoro.installed
                                                    : voices.installedCount >= 0 && voices.isInstalled(settings.voice)
                            onClicked: win.preview(settings.voice)
                        }
                        ThemeButton { text: qsTr("Stop"); onClicked: speaker.stop() }
                        ThemeButton {
                            visible: !win.kokoroMode
                            text: qsTr("Reset")
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
                ThemeField {
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
                RowLayout {
                    visible: !win.kokoroMode
                    spacing: 2
                    Label {
                        text: qsTr("Downloaded only")
                        TapHandler { onTapped: voices.installedOnly = !voices.installedOnly }
                    }
                    ThemeSwitch {
                        checked: voices.installedOnly
                        onSwitched: on => voices.installedOnly = on
                    }
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

                        // The shell's menus mark the row under the cursor with a
                        // faint fill and its text in the accent.
                        Rectangle {
                            anchors.fill: parent
                            color: row.ListView.isCurrentItem ? win.shade(theme.rowSelectedFill) : "transparent"
                        }
                        Rectangle {
                            visible: row.inUse
                            width: 3
                            height: parent.height
                            color: theme.accent
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
                                    color: row.ListView.isCurrentItem ? theme.rowSelectedText : theme.foreground
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

                            ThemeButton {
                                visible: row.modelData.installed
                                text: qsTr("Play")
                                onClicked: win.preview(row.modelData.key)
                            }
                            Label {
                                visible: row.inUse
                                text: qsTr("In use")
                                color: theme.accent
                                Layout.preferredWidth: Math.max(implicitWidth, useButton.implicitWidth)
                                horizontalAlignment: Text.AlignHCenter
                            }
                            ThemeButton {
                                id: useButton
                                visible: row.modelData.installed && !row.inUse
                                bordered: true
                                text: qsTr("Use")
                                onClicked: win.activate(row.modelData)
                            }
                            ThemeButton {
                                visible: row.modelData.installed && !row.inUse && row.modelData.engine !== "kokoro"
                                text: qsTr("Remove")
                                onClicked: win.askRemove(row.modelData)
                            }
                            BusyIndicator {
                                visible: row.downloadingThis
                                running: visible
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                            }
                            ThemeButton {
                                visible: !row.modelData.installed
                                bordered: true
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
                        ThemeButton {
                            Layout.alignment: Qt.AlignHCenter
                            visible: !kokoro.installing
                            text: qsTr("Install Kokoro")
                            primary: true
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
            color: Qt.rgba(theme.background.r, theme.background.g, theme.background.b, 0.9)

            TapHandler { onTapped: win.helpVisible = false }

            // A card like the shell's popups.
            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 520)
                height: helpColumn.implicitHeight + 40
                color: theme.background
                radius: theme.radius
                border.width: 2
                border.color: theme.popupBorder

                ColumnLayout {
                    id: helpColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 8

                    Label { text: qsTr("Shortcuts"); font.pixelSize: win.heading; font.bold: true }

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
                                ["Esc", qsTr("Stop speaking")],
                                ["?", qsTr("Show or hide this list")],
                                ["Q", qsTr("Quit")]
                            ].concat(keybindings.toggleKey ? [[keybindings.label(keybindings.toggleKey), qsTr("Spoken replies, from anywhere")]] : [])
                             .concat(keybindings.stopKey ? [[keybindings.label(keybindings.stopKey), qsTr("Stop speaking, from anywhere")]] : [])
                            delegate: RowLayout {
                                required property var modelData
                                Layout.columnSpan: 2
                                Label {
                                    text: parent.modelData[0]
                                    Layout.preferredWidth: 190
                                    color: theme.accent
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

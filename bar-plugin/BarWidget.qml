import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// Talkback's icon in the Omarchy bar, always shown: a speaker while coding
// agents' replies are read aloud and a muted one while they aren't. Click
// switches it, middle-click stops speaking, and right-click opens a popup to
// silence agents one by one. Talkback installs this file (`talkback bar
// install`); edits here are overwritten.
BarWidget {
  id: root
  moduleName: "io.github.mattwolfgang.talkback"

  readonly property string configHome: Quickshell.env("XDG_CONFIG_HOME") || Quickshell.env("HOME") + "/.config"
  // Talkback treats a missing file or setting as on.
  property bool speaking: true
  property var mutedAgents: []
  // From `talkback agent --json`: every agent Talkback has a hook for.
  property var agents: []
  property bool agentsLoaded: false
  property bool popupOpen: false

  // In the language picked under Setup > Language, from i18n/<code>.json.
  Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  // The bar's popup coordinator closes others through this.
  function close() { popupOpen = false }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  function read(text) {
    var on = true
    var muted = []
    var lines = String(text || "").split("\n")
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i].trim()
      var match = line.match(/^enabled=(.*)$/)
      if (match) on = match[1].trim() !== "false"
      match = line.match(/^agent\.([a-z0-9-]+)=false$/)
      if (match) muted.push(match[1])
    }
    root.speaking = on
    root.mutedAgents = muted
  }

  function agentOn(id) {
    return root.mutedAgents.indexOf(id) < 0
  }

  // Flipped here first so the click is felt; the file confirms it a moment later.
  function setSpeaking(on) {
    root.speaking = on
    Quickshell.execDetached(["talkback", "speech", on ? "on" : "off"])
  }

  function setAgent(id, on) {
    var muted = root.mutedAgents.filter(function(agent) { return agent !== id })
    if (!on) muted.push(id)
    root.mutedAgents = muted
    Quickshell.execDetached(["talkback", "agent", id, on ? "on" : "off"])
  }

  FileView {
    id: settingsFile
    path: root.configHome + "/talkback/settings.conf"
    watchChanges: true
    printErrors: false
    onLoaded: root.read(text())
    onLoadFailed: root.read("")
    // text() is stale inside the change signal, so read through reload.
    onFileChanged: reload()
  }

  // Talkback saves by replacing the file, which a watch can lose track of, so
  // a slow re-read keeps the icon right whatever switched it.
  Timer {
    interval: 2000
    running: true
    repeat: true
    onTriggered: settingsFile.reload()
  }

  // Which agents have a hook can change whenever one is installed, so it's
  // asked again each time the popup opens.
  Process {
    id: agentList
    command: ["talkback", "agent", "--json"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        try {
          root.agents = JSON.parse(text).filter(function(agent) { return agent.hooked })
        } catch (e) {
          root.agents = []
        }
        root.agentsLoaded = true
      }
    }
  }

  onPopupOpenChanged: if (popupOpen && !agentList.running) agentList.running = true

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.speaking ? "󰕾" : "󰖁"
    dimmed: !root.speaking
    tooltipText: root.popupOpen ? ""
      : root.speaking
        ? strings.t("tooltip.on")
        : strings.t("tooltip.off")

    onPressed: function(b) {
      if (b === Qt.MiddleButton) Quickshell.execDetached(["talkback-speak", "--stop"])
      else if (b === Qt.RightButton) root.popupOpen = !root.popupOpen
      else root.setSpeaking(!root.speaking)
    }
  }

  PopupCard {
    id: popup
    anchorItem: root
    bar: root.bar
    owner: root
    open: root.popupOpen
    contentWidth: popup.fittedContentWidth(Style.space(280))
    contentHeight: popup.fittedContentHeight(column.implicitHeight)

    Column {
      id: column
      anchors.fill: parent
      spacing: Style.space(8)

      Toggle {
        width: parent.width
        label: strings.t("speech.label")
        description: strings.t("speech.description")
        checked: root.speaking
        onClicked: root.setSpeaking(!root.speaking)
      }

      PanelSeparator { width: parent.width }

      PanelSectionHeader {
        width: parent.width
        text: strings.t("agents.heading")
      }

      Repeater {
        model: root.agents
        delegate: Toggle {
          required property var modelData
          width: column.width
          label: modelData.name
          checked: root.agentOn(modelData.id)
          // Each agent's switch only matters while replies are on.
          opacity: root.speaking ? 1 : 0.45
          onClicked: root.setAgent(modelData.id, !root.agentOn(modelData.id))
        }
      }

      Text {
        visible: root.agentsLoaded && root.agents.length === 0
        width: parent.width
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: strings.t("agents.none").arg("talkback hook install")
        color: Qt.darker(root.bar ? root.bar.foreground : "white", 1.5)
        font.family: Style.font.family
        font.pixelSize: Style.font.bodySmall
      }

      PanelSeparator { width: parent.width }

      Button {
        width: parent.width
        leftAlign: true
        text: strings.t("action.open")
        onClicked: {
          root.popupOpen = false
          Quickshell.execDetached(["omarchy-launch-or-focus", "talkback"])
        }
      }
    }
  }
}

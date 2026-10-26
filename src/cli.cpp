#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

#include "dictation.h"
#include "hooks.h"
#include "keybindings.h"
#include "kokoro.h"
#include "settings.h"
#include "speaker.h"
#include "voices.h"

namespace {
QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

const char *usage =
    "Usage: talkback [command]\n"
    "\n"
    "  open                                  open the window (the default)\n"
    "  speech [on|off|toggle|status]         read the coding agents' replies aloud\n"
    "  dictation [on|off|toggle|status]      let Voxtype listen for the dictation keys\n"
    "  engine [piper|kokoro]                 show or set the speech engine\n"
    "  voice [NAME]                          show or set the engine's voice\n"
    "  kokoro install                        install Kokoro (about 350 MB)\n"
    "  speaker [N]                           show or set the speaker of a multi-speaker voice\n"
    "  say TEXT                              speak TEXT with the current voice\n"
    "  stop                                  stop speaking\n"
    "  hook install [AGENT|all]              set up the hook that reads an agent's replies: claude,\n"
    "                                        codex, grok, cursor, antigravity, omp, or all found (default);\n"
    "                                        also binds the keys below unless given --no-keys\n"
    "  keys [install|toggle KEY|stop KEY]    show, bind, or change the Hyprland keys that work from\n"
    "                                        anywhere, like: talkback keys stop \"SUPER + ALT + ESCAPE\"\n"
    "  status [--json]                       show everything above\n"
    "\n"
    "Add --notify to speech or dictation to show the result as a desktop notification.\n";

void notify(const QString &summary, const QString &icon) {
    QProcess::startDetached("notify-send", {"--app-name=Talkback", "--icon=" + icon, "--expire-time=2500", summary});
}

std::pair<bool, bool> dictationState() {
    QProcess query;
    query.start("systemctl", {"--user", "show", "voxtype.service", "-p", "LoadState,ActiveState", "--value"});
    query.waitForFinished(5000);
    return Dictation::parseState(query.readAllStandardOutput());
}

void printKeys(const Keybindings &keys) {
    const auto describe = [&](const QString &key) { return key.isEmpty() ? QString("not bound") : keys.label(key); };
    out() << "Toggle spoken replies: " << describe(keys.toggleKey()) << "\n"
          << "Stop speaking:         " << describe(keys.stopKey()) << "\n";
}

// on, off, toggle, or status (empty); -1 for anything else.
int parseSwitch(const QString &word, bool current) {
    if (word.isEmpty() || word == "status")
        return current;
    if (word == "on")
        return 1;
    if (word == "off")
        return 0;
    if (word == "toggle")
        return !current;
    return -1;
}
}

int runCli(const QStringList &all) {
    QStringList arguments = all.mid(1);
    const bool json = arguments.removeAll("--json") > 0;
    const bool shouldNotify = arguments.removeAll("--notify") > 0;
    const bool noKeys = arguments.removeAll("--no-keys") > 0;
    const QString command = arguments.value(0);
    const QString argument = arguments.value(1);

    if (command.isEmpty() || command == "open")
        return -1;

    if (command == "help" || command == "--help" || command == "-h") {
        out() << usage;
        return 0;
    }

    if (command == "speech") {
        Settings settings;
        const int wanted = parseSwitch(argument, settings.enabled());
        if (wanted < 0) {
            err() << usage;
            return 1;
        }
        if (bool(wanted) != settings.enabled()) {
            settings.setEnabled(wanted);
            if (!settings.error().isEmpty()) {
                err() << "Couldn't save settings: " << settings.error() << "\n";
                return 1;
            }
            if (!wanted)
                Speaker().stop();
            if (shouldNotify)
                notify(wanted ? QCoreApplication::translate("Cli", "Spoken replies on")
                              : QCoreApplication::translate("Cli", "Spoken replies off"),
                       wanted ? "audio-volume-high" : "audio-volume-muted");
        }
        out() << (settings.enabled() ? "on" : "off") << "\n";
        return 0;
    }

    if (command == "dictation") {
        const auto [available, active] = dictationState();
        if (!available) {
            err() << "Voxtype isn't installed as a user service (voxtype.service).\n";
            return 1;
        }
        const int wanted = parseSwitch(argument, active);
        if (wanted < 0) {
            err() << usage;
            return 1;
        }
        if (bool(wanted) != active) {
            QProcess change;
            change.start("systemctl", {"--user", wanted ? "start" : "stop", "voxtype.service"});
            change.waitForFinished(15000);
            if (change.exitCode() != 0) {
                err() << change.readAllStandardError();
                return 1;
            }
            if (shouldNotify)
                notify(wanted ? QCoreApplication::translate("Cli", "Dictation on")
                              : QCoreApplication::translate("Cli", "Dictation off"),
                       wanted ? "audio-input-microphone" : "microphone-sensitivity-muted");
        }
        out() << (wanted ? "on" : "off") << "\n";
        return 0;
    }

    if (command == "engine") {
        Settings settings;
        if (!argument.isEmpty()) {
            if (argument != "piper" && argument != "kokoro") {
                err() << usage;
                return 1;
            }
            if (argument == "kokoro" && QProcess::execute(Speaker::helper("talkback-kokoro-setup"), {"--check"}) != 0) {
                err() << "Kokoro isn't installed. Run: talkback kokoro install\n";
                return 1;
            }
            settings.setEngine(argument);
        }
        out() << settings.engine() << "\n";
        return settings.error().isEmpty() ? 0 : 1;
    }

    if (command == "kokoro" && argument == "install") {
        return QProcess::execute(Speaker::helper("talkback-kokoro-setup"), {}) == 0 ? 0 : 1;
    }

    if (command == "voice") {
        Settings settings;
        if (argument.isEmpty()) {
            out() << settings.currentVoice() << "\n";
            return 0;
        }
        if (settings.engine() == "kokoro") {
            QProcess list;
            list.start(Kokoro::python(), {"-I", Speaker::helper("talkback-kokoro"), "--list"});
            list.waitForFinished(30000);
            if (!QString::fromUtf8(list.readAllStandardOutput()).split('\n').contains(argument)) {
                err() << "Not a Kokoro voice: " << argument << ". See the list in the Talkback window.\n";
                return 1;
            }
            settings.setKokoroVoice(argument);
            out() << settings.kokoroVoice() << "\n";
            return settings.error().isEmpty() ? 0 : 1;
        }
        if (!Voices::scanInstalled(Voices::defaultVoiceDirectory()).contains(argument)) {
            err() << "Voice not downloaded: " << argument << ". Download it in the Talkback window.\n";
            return 1;
        }
        settings.setVoice(argument);
        out() << settings.voice() << "\n";
        return settings.error().isEmpty() ? 0 : 1;
    }

    if (command == "speaker") {
        Settings settings;
        if (argument.isEmpty()) {
            out() << settings.speaker() << "\n";
            return 0;
        }
        bool ok = false;
        const int speaker = argument.toInt(&ok);
        const int count = Voices::speakerCount(Voices::defaultVoiceDirectory(), settings.voice());
        if (!ok || speaker < 0 || speaker >= count) {
            err() << settings.voice() << " has " << count << (count == 1 ? " speaker" : " speakers")
                  << "; pick 0 to " << count - 1 << ".\n";
            return 1;
        }
        settings.setSpeaker(settings.voice(), speaker);
        out() << settings.speaker() << "\n";
        return settings.error().isEmpty() ? 0 : 1;
    }

    if (command == "say") {
        const QString text = arguments.mid(1).join(' ');
        if (text.isEmpty()) {
            err() << usage;
            return 1;
        }
        Settings settings;
        return QProcess::execute(Speaker::speakCommand(),
                                 Speaker::sayArguments(text, settings.engine(), settings.currentVoice(), settings)) == 0
                   ? 0 : 1;
    }

    if (command == "stop") {
        return QProcess::execute(Speaker::speakCommand(), {"--stop"}) == 0 ? 0 : 1;
    }

    if (command == "hook" && argument == "install") {
        const QString home = QDir::homePath();
        const QString wanted = arguments.value(2, "all");
        QStringList agents;
        if (wanted == "all") {
            for (const QString &agent : Hooks::agents())
                if (Hooks::detected(agent, home))
                    agents.append(agent);
            if (agents.isEmpty()) {
                err() << "Found no coding agents. Name one, like: talkback hook install claude\n";
                return 1;
            }
        } else if (Hooks::agents().contains(wanted)) {
            agents.append(wanted);
        } else {
            err() << "Not an agent Talkback knows: " << wanted << ". Pick one of "
                  << Hooks::agents().join(", ") << ", or all.\n";
            return 1;
        }
        bool ok = true;
        for (const QString &agent : agents) {
            QString error;
            if (!Hooks::install(agent, home, &error)) {
                err() << Hooks::displayName(agent) << ": " << error << "\n";
                ok = false;
                continue;
            }
            out() << "Installed the " << Hooks::displayName(agent) << " hook in " << Hooks::path(agent, home) << "\n";
            if (agent == "codex")
                out() << "  Codex asks you to review new hooks before they run: approve it in /hooks.\n";
        }
        if (!noKeys) {
            // The hooks are what matters; a key that can't be bound is only a warning.
            Keybindings keys;
            if (!keys.install())
                err() << keys.error() << "\n";
            printKeys(keys);
        }
        return ok ? 0 : 1;
    }

    if (command == "keys") {
        Keybindings keys;
        bool ok = true;
        if (argument == "install")
            ok = keys.install();
        else if (argument == "toggle" || argument == "stop")
            ok = keys.setKey(argument, arguments.mid(2).join(' '));
        else if (!argument.isEmpty()) {
            err() << usage;
            return 1;
        }
        if (!ok)
            err() << keys.error() << "\n";
        printKeys(keys);
        return ok ? 0 : 1;
    }

    if (command == "status") {
        Settings settings;
        const auto [available, active] = dictationState();
        QJsonObject hooks;
        QStringList hooked;
        for (const QString &agent : Hooks::agents()) {
            hooks[agent] = Hooks::installed(agent, QDir::homePath());
            if (hooks[agent].toBool())
                hooked.append(agent);
        }
        Keybindings keys;
        const QJsonObject status{
            {"speech", settings.enabled()},
            {"engine", settings.engine()},
            {"voice", settings.currentVoice()},
            {"speed", settings.speed()},
            {"expression", settings.expression()},
            {"rhythm", settings.rhythm()},
            {"pause", settings.pause()},
            {"speaker", settings.speaker()},
            {"voiceInstalled", settings.engine() == "kokoro"
                                   ? QProcess::execute(Speaker::helper("talkback-kokoro-setup"), {"--check"}) == 0
                                   : Voices::scanInstalled(Voices::defaultVoiceDirectory()).contains(settings.voice())},
            {"hookInstalled", !hooked.isEmpty()},
            {"hooks", hooks},
            {"toggleKey", keys.toggleKey()},
            {"stopKey", keys.stopKey()},
            {"dictationAvailable", available},
            {"dictation", active},
        };
        if (json) {
            out() << QJsonDocument(status).toJson(QJsonDocument::Compact) << "\n";
        } else {
            out() << "Spoken replies: " << (settings.enabled() ? "on" : "off") << "\n"
                  << "Engine:         " << settings.engine() << "\n"
                  << "Voice:          " << settings.currentVoice()
                  << (status.value("voiceInstalled").toBool() ? "" : " (not downloaded)") << "\n"
                  << "Speed:          " << QString::number(settings.speed(), 'f', 2) << "x\n"
                  << "Expression:     " << QString::number(settings.expression(), 'f', 2) << "\n"
                  << "Rhythm:         " << QString::number(settings.rhythm(), 'f', 2) << "\n"
                  << "Pause:          " << QString::number(settings.pause(), 'f', 2) << " s\n"
                  << "Speaker:        " << settings.speaker() << "\n"
                  << "Hooks:          " << (hooked.isEmpty() ? "none" : hooked.join(", ")) << "\n"
                  << "Toggle key:     " << (keys.toggleKey().isEmpty() ? "not bound" : keys.label(keys.toggleKey())) << "\n"
                  << "Stop key:       " << (keys.stopKey().isEmpty() ? "not bound" : keys.label(keys.stopKey())) << "\n"
                  << "Dictation:      " << (!available ? "Voxtype not installed" : active ? "on" : "off") << "\n";
        }
        return 0;
    }

    err() << usage;
    return 1;
}

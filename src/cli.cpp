#include "cli.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

#include "dictation.h"
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
    "  speech [on|off|toggle|status]         read Claude's replies aloud\n"
    "  dictation [on|off|toggle|status]      let Voxtype listen for the dictation keys\n"
    "  engine [piper|kokoro]                 show or set the speech engine\n"
    "  voice [NAME]                          show or set the engine's voice\n"
    "  kokoro install                        install Kokoro (about 350 MB)\n"
    "  speaker [N]                           show or set the speaker of a multi-speaker voice\n"
    "  say TEXT                              speak TEXT with the current voice\n"
    "  stop                                  stop speaking\n"
    "  hook install                          set up the Claude Code Stop hook\n"
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
        Speaker speaker;
        if (!speaker.installHook()) {
            err() << speaker.error() << "\n";
            return 1;
        }
        out() << "Installed the Stop hook in " << Speaker::defaultClaudeSettings() << "\n";
        return 0;
    }

    if (command == "status") {
        Settings settings;
        const auto [available, active] = dictationState();
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
            {"hookInstalled", Speaker().hookInstalled()},
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
                  << "Claude hook:    " << (status.value("hookInstalled").toBool() ? "installed" : "missing") << "\n"
                  << "Dictation:      " << (!available ? "Voxtype not installed" : active ? "on" : "off") << "\n";
        }
        return 0;
    }

    err() << usage;
    return 1;
}

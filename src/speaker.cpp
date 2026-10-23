#include "speaker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include "settings.h"

namespace {
bool isOurs(const QJsonValue &hook) {
    const QString command = hook.toObject().value("command").toString();
    return command.contains("talkback-speak") || command.contains("speak-reply.sh");
}
}

Speaker::Speaker(const QString &claudeSettings, QObject *parent)
    : QObject(parent), m_claudeSettings(claudeSettings.isEmpty() ? defaultClaudeSettings() : claudeSettings) {}

QString Speaker::defaultClaudeSettings() {
    return QDir::homePath() + "/.claude/settings.json";
}

// Next to the binary first, so a development build uses bin/talkback-speak
// from its own checkout; the installed package puts both in /usr/bin.
QString Speaker::speakCommand() {
    const QString beside = QCoreApplication::applicationDirPath() + "/../bin/talkback-speak";
    if (QFileInfo(beside).isExecutable())
        return QFileInfo(beside).canonicalFilePath();
    return "talkback-speak";
}

QStringList Speaker::sayArguments(const QString &text, const QString &voice, const Settings &settings) {
    const auto number = [](double value) { return QString::number(value, 'f', 2); };
    return {"--say", text, "--voice", voice, "--speed", number(settings.speed()),
            "--expression", number(settings.expression()), "--rhythm", number(settings.rhythm()),
            "--pause", number(settings.pause()), "--speaker", QString::number(settings.speakerFor(voice))};
}

void Speaker::say(const QString &text, const QString &voice, QObject *object) {
    const auto *settings = qobject_cast<Settings *>(object);
    if (text.trimmed().isEmpty() || !settings)
        return;
    const bool started = QProcess::startDetached(speakCommand(), sayArguments(text, voice, *settings));
    setError(started ? QString() : tr("Couldn't run talkback-speak."));
}

void Speaker::stop() {
    QProcess::startDetached(speakCommand(), {"--stop"});
}

bool Speaker::hookInstalled() const {
    QFile file(m_claudeSettings);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return hasHook(QJsonDocument::fromJson(file.readAll()).object());
}

bool Speaker::installHook() {
    QJsonObject settings;
    QFile current(m_claudeSettings);
    if (current.exists()) {
        if (!current.open(QIODevice::ReadOnly)) {
            setError(tr("Couldn't read %1.").arg(m_claudeSettings));
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(current.readAll(), &parseError);
        // Never overwrite a settings file we can't read back faithfully.
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            setError(tr("%1 isn't valid JSON, so it was left alone.").arg(m_claudeSettings));
            return false;
        }
        settings = document.object();
    }

    QDir().mkpath(QFileInfo(m_claudeSettings).absolutePath());
    QSaveFile file(m_claudeSettings);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(file.errorString());
        return false;
    }
    file.write(QJsonDocument(withHook(settings)).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(file.errorString());
        return false;
    }
    setError({});
    emit hookChanged();
    return true;
}

bool Speaker::hasHook(const QJsonObject &settings) {
    for (const QJsonValue &group : settings.value("hooks").toObject().value("Stop").toArray())
        for (const QJsonValue &hook : group.toObject().value("hooks").toArray())
            if (hook.toObject().value("command").toString().contains("talkback-speak"))
                return true;
    return false;
}

QJsonObject Speaker::withHook(QJsonObject settings) {
    QJsonObject hooks = settings.value("hooks").toObject();
    QJsonArray stop;
    for (const QJsonValue &group : hooks.value("Stop").toArray()) {
        QJsonObject object = group.toObject();
        QJsonArray kept;
        for (const QJsonValue &hook : object.value("hooks").toArray())
            if (!isOurs(hook))
                kept.append(hook);
        if (kept.isEmpty())
            continue;
        object["hooks"] = kept;
        stop.append(object);
    }
    stop.append(QJsonObject{
        {"hooks", QJsonArray{QJsonObject{{"type", "command"}, {"command", "talkback-speak"}, {"timeout", 10}}}}});
    hooks["Stop"] = stop;
    settings["hooks"] = hooks;
    return settings;
}

void Speaker::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

#include "speaker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include "hooks.h"
#include "settings.h"

Speaker::Speaker(const QString &home, QObject *parent)
    : QObject(parent), m_home(home.isEmpty() ? QDir::homePath() : home) {}

// Next to the binary first, so a development build uses bin/talkback-speak
// from its own checkout; the installed package puts both in /usr/bin.
QString Speaker::speakCommand() {
    return helper("talkback-speak");
}

QString Speaker::helper(const QString &name) {
    const QString beside = QCoreApplication::applicationDirPath() + "/../bin/" + name;
    if (QFileInfo(beside).isExecutable())
        return QFileInfo(beside).canonicalFilePath();
    const QString installed = QStandardPaths::findExecutable(name);
    return installed.isEmpty() ? name : installed;
}

QStringList Speaker::sayArguments(const QString &text, const QString &engine, const QString &voice,
                                  const Settings &settings) {
    const auto number = [](double value) { return QString::number(value, 'f', 2); };
    return {"--say", text, "--engine", engine, "--voice", voice, "--speed", number(settings.speed()),
            "--expression", number(settings.expression()), "--rhythm", number(settings.rhythm()),
            "--pause", number(settings.pause()), "--speaker", QString::number(settings.speakerFor(voice))};
}

void Speaker::say(const QString &text, const QString &engine, const QString &voice, QObject *object) {
    const auto *settings = qobject_cast<Settings *>(object);
    if (text.trimmed().isEmpty() || !settings)
        return;
    const bool started = QProcess::startDetached(speakCommand(), sayArguments(text, engine, voice, *settings));
    setError(started ? QString() : tr("Couldn't run talkback-speak."));
}

void Speaker::stop() {
    QProcess::startDetached(speakCommand(), {"--stop"});
}

bool Speaker::hookInstalled() const {
    for (const QString &agent : Hooks::agents())
        if (Hooks::installed(agent, m_home))
            return true;
    return false;
}

bool Speaker::installHook() {
    QStringList agents;
    for (const QString &agent : Hooks::agents())
        if (Hooks::detected(agent, m_home))
            agents.append(agent);
    if (agents.isEmpty())
        agents.append("claude");
    for (const QString &agent : agents) {
        QString error;
        if (!Hooks::install(agent, m_home, &error)) {
            setError(error);
            emit hookChanged();
            return false;
        }
    }
    setError({});
    emit hookChanged();
    return true;
}

void Speaker::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

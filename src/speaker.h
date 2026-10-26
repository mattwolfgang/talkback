#pragma once

#include <QObject>

class Settings;

// Plays previews through talkback-speak, the same script the coding agents
// run, so a preview sounds exactly like a spoken reply. Also checks and
// installs the agents' hooks that run it (see Hooks).
class Speaker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hookInstalled READ hookInstalled NOTIFY hookChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // `home` is where the agents keep their settings; the user's home by default.
    explicit Speaker(const QString &home = {}, QObject *parent = nullptr);

    static QString speakCommand();
    // One of Talkback's helper scripts, from this checkout when developing.
    static QString helper(const QString &name);

    // Whether any agent runs talkback-speak.
    bool hookInstalled() const;
    QString error() const { return m_error; }

    // Speaks with `settings` but the given engine and voice, so any voice can
    // be previewed. QML only passes types it knows, so the settings arrive as
    // a QObject.
    Q_INVOKABLE void say(const QString &text, const QString &engine, const QString &voice, QObject *settings);
    static QStringList sayArguments(const QString &text, const QString &engine, const QString &voice,
                                    const Settings &settings);
    Q_INVOKABLE void stop();
    // Hooks every agent found under home, or Claude Code when none is.
    Q_INVOKABLE bool installHook();

signals:
    void hookChanged();
    void errorChanged();

private:
    void setError(const QString &error);

    QString m_home;
    QString m_error;
};

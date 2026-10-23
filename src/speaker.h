#pragma once

#include <QJsonObject>
#include <QObject>

// Plays previews through talkback-speak, the same script Claude Code runs,
// so a preview sounds exactly like a spoken reply. Also checks and installs
// the Claude Code Stop hook that runs it.
class Speaker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hookInstalled READ hookInstalled NOTIFY hookChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Speaker(const QString &claudeSettings = {}, QObject *parent = nullptr);

    static QString speakCommand();
    static QString defaultClaudeSettings();

    bool hookInstalled() const;
    QString error() const { return m_error; }

    Q_INVOKABLE void say(const QString &text, const QString &voice, double speed);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool installHook();

    static bool hasHook(const QJsonObject &settings);
    // Settings with one Stop hook running talkback-speak, replacing any
    // earlier one (including the speak-reply.sh this app grew out of) and
    // leaving every other hook alone.
    static QJsonObject withHook(QJsonObject settings);

signals:
    void hookChanged();
    void errorChanged();

private:
    void setError(const QString &error);

    QString m_claudeSettings;
    QString m_error;
};

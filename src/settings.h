#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

// The settings talkback-speak reads: whether replies are spoken, which Piper
// voice, and how fast. A plain key=value file, so the shell script and the
// keybindings can read and change it without the app running.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString voice READ voice WRITE setVoice NOTIFY changed)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY changed)

public:
    explicit Settings(const QString &path = {}, QObject *parent = nullptr);

    static QString defaultPath();
    static constexpr const char *defaultVoice = "en_US-lessac-medium";

    bool enabled() const { return m_enabled; }
    QString voice() const { return m_voice; }
    double speed() const { return m_speed; }

    void setEnabled(bool enabled);
    void setVoice(const QString &voice);
    void setSpeed(double speed);

    QString error() const { return m_error; }

signals:
    void changed();

private:
    void reload();
    bool save();
    void watch();

    QString m_path;
    bool m_enabled = true;
    QString m_voice = defaultVoice;
    double m_speed = 1.0;
    QString m_error;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

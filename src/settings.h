#pragma once

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

// The settings talkback-speak reads: whether replies are spoken, which Piper
// voice, how fast, and how lively. A plain key=value file, so the shell script and the
// keybindings can read and change it without the app running.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString voice READ voice WRITE setVoice NOTIFY changed)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY changed)
    Q_PROPERTY(double expression READ expression WRITE setExpression NOTIFY changed)
    Q_PROPERTY(double rhythm READ rhythm WRITE setRhythm NOTIFY changed)
    Q_PROPERTY(double pause READ pause WRITE setPause NOTIFY changed)
    // The speaker of the current voice, for voices trained on many people.
    Q_PROPERTY(int speaker READ speaker NOTIFY changed)
    // Every saved choice, voice to speaker, so QML bindings update with them.
    Q_PROPERTY(QVariantMap speakers READ speakers NOTIFY changed)

public:
    explicit Settings(const QString &path = {}, QObject *parent = nullptr);

    static QString defaultPath();
    static constexpr const char *defaultVoice = "en_US-lessac-medium";
    // Piper's own defaults for its noise settings, and a pause long enough to
    // hear sentences end (Piper's default is none).
    static constexpr double defaultExpression = 0.667;
    static constexpr double defaultRhythm = 0.8;
    static constexpr double defaultPause = 0.3;

    bool enabled() const { return m_enabled; }
    QString voice() const { return m_voice; }
    double speed() const { return m_speed; }
    double expression() const { return m_expression; }
    double rhythm() const { return m_rhythm; }
    double pause() const { return m_pause; }
    int speaker() const { return speakerFor(m_voice); }
    QVariantMap speakers() const;
    Q_INVOKABLE int speakerFor(const QString &voice) const { return m_speakers.value(voice, 0); }

    void setEnabled(bool enabled);
    void setVoice(const QString &voice);
    void setSpeed(double speed);
    void setExpression(double expression);
    void setRhythm(double rhythm);
    void setPause(double pause);
    Q_INVOKABLE void resetNaturalness();
    Q_INVOKABLE void setSpeaker(const QString &voice, int speaker);

    QString error() const { return m_error; }

signals:
    void changed();

private:
    void reload();
    bool save();
    void watch();
    void setNumber(double &field, double value, double low, double high);

    QString m_path;
    bool m_enabled = true;
    QString m_voice = defaultVoice;
    double m_speed = 1.0;
    double m_expression = defaultExpression;
    double m_rhythm = defaultRhythm;
    double m_pause = defaultPause;
    QMap<QString, int> m_speakers;
    QString m_error;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

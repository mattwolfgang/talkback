#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

// Whether Voxtype, Omarchy's dictation backend, is listening for the
// dictation keys. Off stops its user service, so nothing can record until it
// is turned back on (or the next login starts it again).
class Dictation : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    explicit Dictation(const QString &systemctl = "systemctl", const QString &unit = "voxtype.service",
                       QObject *parent = nullptr);
    ~Dictation() override;

    bool available() const { return m_available; }
    bool active() const { return m_active; }
    bool busy() const { return m_change.state() != QProcess::NotRunning; }
    QString error() const { return m_error; }

    Q_INVOKABLE void setActive(bool active);
    Q_INVOKABLE void refresh();

    // `systemctl show -p LoadState,ActiveState --value` output to
    // {available, active}.
    static std::pair<bool, bool> parseState(const QByteArray &output);

signals:
    void changed();

private:
    QString m_systemctl, m_unit;
    bool m_available = false, m_active = false;
    QString m_error;
    QProcess m_query, m_change;
    QTimer m_poll;
};

#include "dictation.h"

Dictation::Dictation(const QString &systemctl, const QString &unit, QObject *parent)
    : QObject(parent), m_systemctl(systemctl), m_unit(unit) {
    connect(&m_query, &QProcess::finished, this, [this] {
        const auto [available, active] = parseState(m_query.readAllStandardOutput());
        if (available != m_available || active != m_active) {
            m_available = available;
            m_active = active;
            emit changed();
        }
    });
    connect(&m_change, &QProcess::finished, this, [this](int code) {
        const QString error = code == 0 ? QString() : QString::fromUtf8(m_change.readAllStandardError()).trimmed();
        m_error = error.isEmpty() && code != 0 ? tr("Couldn't change the Voxtype service.") : error;
        emit changed();
        refresh();
    });

    // The keybindings and Omarchy's own menus change the service too, so
    // check every couple of seconds rather than trusting the last click.
    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, &Dictation::refresh);
    m_poll.start();
    refresh();
}

Dictation::~Dictation() {
    for (QProcess *process : {&m_query, &m_change})
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
}

void Dictation::refresh() {
    if (m_query.state() != QProcess::NotRunning)
        return;
    m_query.start(m_systemctl, {"--user", "show", m_unit, "-p", "LoadState,ActiveState", "--value"});
}

void Dictation::setActive(bool active) {
    if (busy() || !m_available)
        return;
    m_change.start(m_systemctl, {"--user", active ? "start" : "stop", m_unit});
    emit changed();
}

std::pair<bool, bool> Dictation::parseState(const QByteArray &output) {
    const QList<QByteArray> lines = output.trimmed().split('\n');
    const bool available = !lines.isEmpty() && lines.first().trimmed() == "loaded";
    const bool active = available && lines.size() > 1 &&
                        (lines.at(1).trimmed() == "active" || lines.at(1).trimmed() == "activating");
    return {available, active};
}

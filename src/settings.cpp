#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>

Settings::Settings(const QString &path, QObject *parent)
    : QObject(parent), m_path(path.isEmpty() ? defaultPath() : path) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Settings::reload);
    reload();
}

QString Settings::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/talkback/settings.conf";
}

void Settings::setEnabled(bool enabled) {
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    save();
    emit changed();
}

void Settings::setVoice(const QString &voice) {
    if (voice == m_voice || voice.isEmpty())
        return;
    m_voice = voice;
    save();
    emit changed();
}

void Settings::setSpeed(double speed) { setNumber(m_speed, speed, 0.5, 2.0); }
void Settings::setExpression(double expression) { setNumber(m_expression, expression, 0.0, 1.5); }
void Settings::setRhythm(double rhythm) { setNumber(m_rhythm, rhythm, 0.0, 1.5); }
void Settings::setPause(double pause) { setNumber(m_pause, pause, 0.0, 1.5); }

void Settings::resetNaturalness() {
    m_expression = defaultExpression;
    m_rhythm = defaultRhythm;
    m_pause = defaultPause;
    save();
    emit changed();
}

void Settings::setNumber(double &field, double value, double low, double high) {
    // Two decimals is all the sliders show, and it keeps the file readable.
    value = std::round(qBound(low, value, high) * 100) / 100;
    if (qFuzzyCompare(value + 1, field + 1))
        return;
    field = value;
    save();
    emit changed();
}

void Settings::reload() {
    bool enabled = true;
    QString voice = defaultVoice;
    double speed = 1.0, expression = defaultExpression, rhythm = defaultRhythm, pause = defaultPause;
    auto number = [](const QString &value, double &field, double low, double high) {
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (ok)
            field = qBound(low, parsed, high);
    };

    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) {
        for (const QByteArray &raw : file.readAll().split('\n')) {
            const QString line = QString::fromUtf8(raw).trimmed();
            const QString key = line.section('=', 0, 0).trimmed();
            const QString value = line.section('=', 1).trimmed();
            if (key == "enabled")
                enabled = value != "false";
            else if (key == "voice" && !value.isEmpty())
                voice = value;
            else if (key == "speed")
                number(value, speed, 0.5, 2.0);
            else if (key == "expression")
                number(value, expression, 0.0, 1.5);
            else if (key == "rhythm")
                number(value, rhythm, 0.0, 1.5);
            else if (key == "pause")
                number(value, pause, 0.0, 1.5);
        }
    }

    watch();
    const auto differs = [](double a, double b) { return !qFuzzyCompare(a + 1, b + 1); };
    if (enabled != m_enabled || voice != m_voice || differs(speed, m_speed) || differs(expression, m_expression) ||
        differs(rhythm, m_rhythm) || differs(pause, m_pause)) {
        m_enabled = enabled;
        m_voice = voice;
        m_speed = speed;
        m_expression = expression;
        m_rhythm = rhythm;
        m_pause = pause;
        emit changed();
    }
}

// Saving replaces the file, which drops it from the watch, and the keybindings
// change it from outside; watch the directory too and re-arm every time.
void Settings::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    const QString directory = QFileInfo(m_path).absolutePath();
    if (QFileInfo::exists(directory))
        m_watcher.addPath(directory);
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
}

bool Settings::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        m_error = file.errorString();
        return false;
    }
    const auto number = [](double value) { return QString::number(value, 'f', 2); };
    file.write(QStringLiteral("enabled=%1\nvoice=%2\nspeed=%3\nexpression=%4\nrhythm=%5\npause=%6\n")
                   .arg(m_enabled ? "true" : "false", m_voice, number(m_speed), number(m_expression),
                        number(m_rhythm), number(m_pause))
                   .toUtf8());
    if (!file.commit()) {
        m_error = file.errorString();
        return false;
    }
    m_error.clear();
    watch();
    return true;
}

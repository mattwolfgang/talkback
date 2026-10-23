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

void Settings::setSpeed(double speed) {
    // Two decimals is all the slider shows, and it keeps the file readable.
    speed = std::round(qBound(0.5, speed, 2.0) * 100) / 100;
    if (qFuzzyCompare(speed, m_speed))
        return;
    m_speed = speed;
    save();
    emit changed();
}

void Settings::reload() {
    bool enabled = true;
    QString voice = defaultVoice;
    double speed = 1.0;

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
            else if (key == "speed") {
                bool ok = false;
                const double parsed = value.toDouble(&ok);
                if (ok && parsed > 0)
                    speed = qBound(0.5, parsed, 2.0);
            }
        }
    }

    watch();
    if (enabled != m_enabled || voice != m_voice || !qFuzzyCompare(speed, m_speed)) {
        m_enabled = enabled;
        m_voice = voice;
        m_speed = speed;
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
    file.write(QStringLiteral("enabled=%1\nvoice=%2\nspeed=%3\n")
                   .arg(m_enabled ? "true" : "false", m_voice, QString::number(m_speed, 'f', 2))
                   .toUtf8());
    if (!file.commit()) {
        m_error = file.errorString();
        return false;
    }
    m_error.clear();
    watch();
    return true;
}

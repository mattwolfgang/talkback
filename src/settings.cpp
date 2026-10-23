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

void Settings::setEngine(const QString &engine) {
    if (engine == m_engine || (engine != "piper" && engine != "kokoro"))
        return;
    m_engine = engine;
    save();
    emit changed();
}

void Settings::setKokoroVoice(const QString &voice) {
    if (voice == m_kokoroVoice || voice.isEmpty())
        return;
    m_kokoroVoice = voice;
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

QVariantMap Settings::speakers() const {
    QVariantMap speakers;
    for (auto it = m_speakers.cbegin(); it != m_speakers.cend(); ++it)
        speakers.insert(it.key(), it.value());
    return speakers;
}

void Settings::setSpeaker(const QString &voice, int speaker) {
    speaker = qMax(0, speaker);
    if (voice.isEmpty() || speakerFor(voice) == speaker)
        return;
    // Speaker 0 is Piper's default, so it needs no line in the file.
    if (speaker == 0)
        m_speakers.remove(voice);
    else
        m_speakers.insert(voice, speaker);
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
    QString engine = "piper", voice = defaultVoice, kokoroVoice = defaultKokoroVoice;
    double speed = 1.0, expression = defaultExpression, rhythm = defaultRhythm, pause = defaultPause;
    QMap<QString, int> speakers;
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
            else if (key == "engine" && (value == "piper" || value == "kokoro"))
                engine = value;
            else if (key == "voice" && !value.isEmpty())
                voice = value;
            else if (key == "kokoroVoice" && !value.isEmpty())
                kokoroVoice = value;
            else if (key == "speed")
                number(value, speed, 0.5, 2.0);
            else if (key == "expression")
                number(value, expression, 0.0, 1.5);
            else if (key == "rhythm")
                number(value, rhythm, 0.0, 1.5);
            else if (key == "pause")
                number(value, pause, 0.0, 1.5);
            else if (key.startsWith("speaker.") && key.size() > 8) {
                bool ok = false;
                const int speaker = value.toInt(&ok);
                if (ok && speaker > 0)
                    speakers.insert(key.mid(8), speaker);
            }
        }
    }

    watch();
    const auto differs = [](double a, double b) { return !qFuzzyCompare(a + 1, b + 1); };
    if (enabled != m_enabled || engine != m_engine || voice != m_voice || kokoroVoice != m_kokoroVoice || differs(speed, m_speed) || differs(expression, m_expression) ||
        differs(rhythm, m_rhythm) || differs(pause, m_pause) || speakers != m_speakers) {
        m_enabled = enabled;
        m_engine = engine;
        m_voice = voice;
        m_kokoroVoice = kokoroVoice;
        m_speed = speed;
        m_expression = expression;
        m_rhythm = rhythm;
        m_pause = pause;
        m_speakers = speakers;
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
    QString contents = QStringLiteral("enabled=%1\nengine=%2\nvoice=%3\nkokoroVoice=%4\n")
                           .arg(m_enabled ? "true" : "false", m_engine, m_voice, m_kokoroVoice) +
                       QStringLiteral("speed=%1\nexpression=%2\nrhythm=%3\npause=%4\n")
                           .arg(number(m_speed), number(m_expression), number(m_rhythm), number(m_pause));
    for (auto it = m_speakers.cbegin(); it != m_speakers.cend(); ++it)
        contents += QStringLiteral("speaker.%1=%2\n").arg(it.key()).arg(it.value());
    file.write(contents.toUtf8());
    if (!file.commit()) {
        m_error = file.errorString();
        return false;
    }
    m_error.clear();
    watch();
    return true;
}

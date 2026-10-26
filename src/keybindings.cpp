#include "keybindings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
struct Action {
    const char *description;
    const char *command;
    // What finds the line, so a hand-edited command still counts.
    const char *match;
};

const QMap<QString, Action> &actions() {
    static const QMap<QString, Action> map{
        {"toggle", {"Toggle spoken replies", "talkback speech toggle --notify", "talkback speech toggle"}},
        {"stop", {"Stop speaking", "talkback-speak --stop", "talkback-speak --stop"}},
    };
    return map;
}

const QMap<QString, int> &modifiers() {
    static const QMap<QString, int> map{
        {"SHIFT", 1}, {"CAPS", 2}, {"CTRL", 4}, {"CONTROL", 4}, {"ALT", 8}, {"MOD2", 16},
        {"MOD3", 32}, {"SUPER", 64}, {"WIN", 64}, {"LOGO", 64}, {"MOD4", 64}, {"MOD5", 128}};
    return map;
}

const QRegularExpression &bindLine() {
    static const QRegularExpression pattern(R"(^(\s*o\.(?:re)?bind\(\s*")([^"]*)("))");
    return pattern;
}

int lineOf(const QStringList &lines, const QString &action) {
    const QString match = actions().value(action).match;
    for (int i = 0; i < lines.size(); ++i)
        if (!lines[i].trimmed().startsWith("--") && bindLine().match(lines[i]).hasMatch() && lines[i].contains(match))
            return i;
    return -1;
}

QString readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
}

Keybindings::Keybindings(const QString &bindingsFile, const QString &hyprctl, QObject *parent)
    : QObject(parent), m_path(bindingsFile.isEmpty() ? defaultBindingsFile() : bindingsFile), m_hyprctl(hyprctl) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Keybindings::reload);
    reload();
}

QString Keybindings::defaultBindingsFile() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/hypr/bindings.lua";
}

// Plain Hyprland and other Lua setups write their keys differently.
bool Keybindings::available() const {
    const QString lua = readFile(m_path);
    return lua.contains("o.bind") || lua.contains("o.rebind") || lua.contains("omarchy");
}

bool Keybindings::install() {
    if (!available()) {
        setError(tr("Talkback can only bind keys in Omarchy's %1.").arg(m_path));
        return false;
    }
    bool running = false;
    const QByteArray binds = hyprctl({"binds", "-j"}, &running);
    if (!running) {
        setError(tr("Hyprland isn't running, so the keys weren't bound."));
        return false;
    }
    const QString before = readFile(m_path);
    QString lua = before;
    QStringList taken;
    for (const auto &[action, combo] : {std::pair{"toggle", defaultToggle}, std::pair{"stop", defaultStop}}) {
        if (!keyIn(lua, action).isEmpty())
            continue;
        const QString other = holder(binds, combo, action);
        if (other.isEmpty())
            lua = withKey(lua, action, combo);
        else
            taken.append(tr("%1 is already “%2”").arg(label(combo), other));
    }
    if (lua != before && !write(lua))
        return false;
    if (!taken.isEmpty()) {
        setError(tr("Left alone: %1. Pick other keys.").arg(taken.join("; ")));
        return false;
    }
    setError({});
    return true;
}

bool Keybindings::setKey(const QString &action, const QString &combo) {
    const QString key = normalize(combo);
    if (!actions().contains(action) || key.isEmpty()) {
        setError(tr("That isn't a key Hyprland can bind."));
        return false;
    }
    // A bare key, or one with only Shift, would stop typing in every other app.
    if ((modmask(key) & (64 | 8 | 4)) == 0) {
        setError(tr("Add Super, Alt or Ctrl, so the key still types in other apps."));
        return false;
    }
    if (!available()) {
        setError(tr("Talkback can only bind keys in Omarchy's %1.").arg(m_path));
        return false;
    }
    bool running = false;
    const QByteArray binds = hyprctl({"binds", "-j"}, &running);
    if (!running) {
        setError(tr("Hyprland isn't running, so the key wasn't changed."));
        return false;
    }
    const QString other = holder(binds, key, action);
    if (!other.isEmpty()) {
        setError(tr("%1 is already “%2”.").arg(label(key), other));
        return false;
    }
    if (!write(withKey(readFile(m_path), action, key)))
        return false;
    setError({});
    return true;
}

QString Keybindings::keyName(int qtKey) const {
    if ((qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) || (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9))
        return QString(QChar(qtKey));
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return "F" + QString::number(qtKey - Qt::Key_F1 + 1);
    static const QMap<int, QString> names{
        {Qt::Key_Escape, "ESCAPE"}, {Qt::Key_Return, "RETURN"}, {Qt::Key_Enter, "RETURN"},
        {Qt::Key_Space, "SPACE"}, {Qt::Key_Tab, "TAB"}, {Qt::Key_Backspace, "BACKSPACE"},
        {Qt::Key_Delete, "DELETE"}, {Qt::Key_Insert, "INSERT"}, {Qt::Key_Home, "HOME"}, {Qt::Key_End, "END"},
        {Qt::Key_PageUp, "PRIOR"}, {Qt::Key_PageDown, "NEXT"}, {Qt::Key_Left, "LEFT"}, {Qt::Key_Right, "RIGHT"},
        {Qt::Key_Up, "UP"}, {Qt::Key_Down, "DOWN"}, {Qt::Key_Print, "PRINT"}, {Qt::Key_Pause, "PAUSE"},
        {Qt::Key_Comma, "COMMA"}, {Qt::Key_Period, "PERIOD"}, {Qt::Key_Slash, "SLASH"},
        {Qt::Key_Backslash, "BACKSLASH"}, {Qt::Key_Semicolon, "SEMICOLON"}, {Qt::Key_Apostrophe, "APOSTROPHE"},
        {Qt::Key_BracketLeft, "BRACKETLEFT"}, {Qt::Key_BracketRight, "BRACKETRIGHT"},
        {Qt::Key_Minus, "MINUS"}, {Qt::Key_Equal, "EQUAL"}, {Qt::Key_QuoteLeft, "GRAVE"}};
    return names.value(qtKey);
}

QString Keybindings::label(const QString &combo) const {
    static const QMap<QString, QString> words{
        {"SUPER", "Super"}, {"WIN", "Super"}, {"LOGO", "Super"}, {"MOD4", "Super"}, {"ALT", "Alt"},
        {"CTRL", "Ctrl"}, {"CONTROL", "Ctrl"}, {"SHIFT", "Shift"}, {"ESCAPE", "Esc"}, {"RETURN", "Enter"},
        {"PRIOR", "Page Up"}, {"NEXT", "Page Down"}, {"COMMA", ","}, {"PERIOD", "."}, {"SLASH", "/"},
        {"BACKSLASH", "\\"}, {"SEMICOLON", ";"}, {"APOSTROPHE", "'"}, {"BRACKETLEFT", "["},
        {"BRACKETRIGHT", "]"}, {"MINUS", "-"}, {"EQUAL", "="}, {"GRAVE", "`"}};
    QStringList parts;
    for (const QString &part : normalize(combo).split(" + ", Qt::SkipEmptyParts)) {
        if (words.contains(part))
            parts.append(words.value(part));
        else if (part.size() <= 3)
            parts.append(part);
        else
            parts.append(part.left(1) + part.mid(1).toLower());
    }
    return parts.join(" + ");
}

QString Keybindings::keyIn(const QString &lua, const QString &action) {
    const QStringList lines = lua.split('\n');
    const int line = lineOf(lines, action);
    return line < 0 ? QString() : normalize(bindLine().match(lines[line]).captured(2));
}

QString Keybindings::withKey(const QString &lua, const QString &action, const QString &combo) {
    QStringList lines = lua.split('\n');
    const int line = lineOf(lines, action);
    if (line >= 0) {
        const QRegularExpressionMatch match = bindLine().match(lines[line]);
        lines[line].replace(match.capturedStart(2), match.capturedLength(2), combo);
        return lines.join('\n');
    }
    const Action &spec = actions().value(action);
    const QString bind = QString("o.bind(\"%1\", \"%2\", \"%3\")").arg(combo, spec.description, spec.command);
    // Next to the other one, when it's already there.
    for (const QString &other : actions().keys()) {
        const int at = lineOf(lines, other);
        if (at >= 0) {
            lines.insert(at + 1, bind);
            return lines.join('\n');
        }
    }
    QString result = lua;
    if (!result.isEmpty() && !result.endsWith('\n'))
        result += '\n';
    return result + "\n-- Talkback\n" + bind + "\n";
}

QString Keybindings::normalize(const QString &combo) {
    QStringList parts;
    for (const QString &part : combo.split('+'))
        if (!part.trimmed().isEmpty())
            parts.append(part.trimmed().toUpper());
    if (parts.isEmpty())
        return {};
    static const QRegularExpression keyPattern("^[A-Z0-9_]+$");
    const QString key = parts.last();
    if (modifiers().contains(key) || !keyPattern.match(key).hasMatch())
        return {};
    for (int i = 0; i < parts.size() - 1; ++i)
        if (!modifiers().contains(parts[i]))
            return {};
    return parts.join(" + ");
}

int Keybindings::modmask(const QString &combo) {
    const QStringList parts = normalize(combo).split(" + ", Qt::SkipEmptyParts);
    int mask = 0;
    for (int i = 0; i < parts.size() - 1; ++i)
        mask |= modifiers().value(parts[i]);
    return mask;
}

QString Keybindings::holder(const QByteArray &binds, const QString &combo, const QString &action) {
    const QString normalized = normalize(combo);
    const QString key = normalized.section(" + ", -1);
    const int mask = modmask(normalized);
    const QString own = actions().value(action).description;
    for (const QJsonValue &value : QJsonDocument::fromJson(binds).array()) {
        const QJsonObject bind = value.toObject();
        if (!bind.value("submap").toString().isEmpty() || bind.value("modmask").toInt() != mask ||
            bind.value("key").toString().toUpper() != key)
            continue;
        const QString description = bind.value("description").toString();
        if (description == own)
            continue;
        return description.isEmpty()
                   ? (bind.value("dispatcher").toString() + " " + bind.value("arg").toString()).trimmed()
                   : description;
    }
    return {};
}

void Keybindings::reload() {
    const QString lua = readFile(m_path);
    const QString toggle = keyIn(lua, "toggle");
    const QString stop = keyIn(lua, "stop");
    watch();
    if (toggle == m_toggleKey && stop == m_stopKey)
        return;
    m_toggleKey = toggle;
    m_stopKey = stop;
    emit changed();
}

// Saving replaces the file, which drops it from the watch; watch the
// directory too and re-arm every time.
void Keybindings::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    const QString directory = QFileInfo(m_path).absolutePath();
    if (QFileInfo::exists(directory))
        m_watcher.addPath(directory);
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
}

// Hyprland reloads the file when it changes; a change it reports an error
// for is put back, so a bad key never breaks the rest of the config.
bool Keybindings::write(const QString &lua) {
    const QString before = readFile(m_path);
    const QByteArray errorsBefore = hyprctl({"configerrors"}).trimmed();
    const auto save = [this](const QString &contents) {
        QSaveFile file(m_path);
        if (!file.open(QIODevice::WriteOnly)) {
            setError(file.errorString());
            return false;
        }
        file.write(contents.toUtf8());
        if (!file.commit()) {
            setError(file.errorString());
            return false;
        }
        return true;
    };
    if (!save(lua))
        return false;
    hyprctl({"reload"});
    const QByteArray errorsAfter = hyprctl({"configerrors"}).trimmed();
    if (!errorsAfter.isEmpty() && errorsAfter != errorsBefore) {
        save(before);
        hyprctl({"reload"});
        reload();
        setError(tr("Hyprland rejected the change, so it was undone: %1").arg(QString::fromUtf8(errorsAfter)));
        return false;
    }
    reload();
    return true;
}

QByteArray Keybindings::hyprctl(const QStringList &arguments, bool *ok) const {
    QProcess process;
    process.start(m_hyprctl, arguments);
    const bool finished = process.waitForFinished(5000);
    if (ok)
        *ok = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return process.readAllStandardOutput();
}

void Keybindings::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

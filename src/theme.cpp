// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QHash<QString, QString> Theme::readToml(const QString &path) {
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return values;
    // Omarchy's theme files only use [sections] and key = "string" or number
    // lines, so this reads those and skips anything else.
    static const QRegularExpression section(R"(^\s*\[([A-Za-z0-9_.-]+)\]\s*(#.*)?$)");
    static const QRegularExpression entry(R"re(^\s*([A-Za-z0-9_-]+)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^#\s]+)))re");
    QString prefix;
    const auto lines = QString::fromUtf8(file.readAll()).split('\n');
    for (const auto &line : lines) {
        if (const auto match = section.match(line); match.hasMatch()) {
            prefix = match.captured(1) + '.';
            continue;
        }
        if (const auto match = entry.match(line); match.hasMatch()) {
            QString value = match.captured(2);
            if (value.isEmpty())
                value = match.captured(3);
            if (value.isEmpty())
                value = match.captured(4);
            values.insert(prefix + match.captured(1), value.trimmed());
        }
    }
    return values;
}

QColor Theme::color(const QString &key, const QColor &fallback) const {
    const QColor value(m_colors.value(key));
    return value.isValid() ? value : fallback;
}

// Red is the theme's warning color; older themes only number their colors.
QColor Theme::urgent() const {
    return color("red", color("color1", QColor("#ff8a80")));
}

QColor Theme::shellColor(const QString &key, const QColor &fallback) const {
    return parseColor(m_shell.value(key), fallback);
}

// shell.toml values are a hex color, a palette role ("accent"), another key
// ("hyprland.active-border"), or a Hyprland gradient, whose first stop stands
// in for it here.
QColor Theme::parseColor(QString value, const QColor &fallback, int depth) const {
    value = value.trimmed();
    if (value.isEmpty() || depth > 4)
        return fallback;
    const auto role = value.toLower();
    if (role == "foreground" || role == "text")
        return foreground();
    if (role == "accent")
        return accent();
    if (role == "urgent")
        return urgent();
    if (role == "background")
        return background();
    if (role == "muted")
        return color("muted", fallback);
    if (m_shell.contains(role))
        return parseColor(m_shell.value(role), fallback, depth + 1);
    const auto first = value.section(' ', 0, 0, QString::SectionSkipEmpty);
    static const QRegularExpression hyprland(R"(^rgba?\(([0-9a-fA-F]{6})([0-9a-fA-F]{2})?\)$)");
    if (const auto match = hyprland.match(first); match.hasMatch())
        return QColor('#' + match.captured(2) + match.captured(1));
    const QColor parsed(first);
    return parsed.isValid() ? parsed : fallback;
}

double Theme::alpha(const QString &key, double fallback) const {
    bool ok = false;
    const double value = m_shell.value(key).toDouble(&ok);
    return ok ? std::clamp(value, 0.0, 1.0) : fallback;
}

// The shell's type scale is rooted at [font] base-size, 12 px by default.
int Theme::fontSize() const {
    bool ok = false;
    const int size = qRound(m_shell.value("font.base-size").toDouble(&ok));
    return ok && size > 0 ? size : 12;
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QColor Theme::accentForeground() const {
    const QColor c = accent();
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? Qt::black : Qt::white;
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml",
                           m_directory + "/theme/shell.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    auto colors = readToml(m_directory + "/theme/colors.toml");
    auto shell = readToml(m_directory + "/theme/shell.toml");
    if (colors != m_colors || shell != m_shell) {
        m_colors = std::move(colors);
        m_shell = std::move(shell);
        emit changed();
    }
}

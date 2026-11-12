#include "barplugin.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>

namespace {
// Compiled in from bar-plugin/, so the binary always installs its own version.
const QString bundled = ":/bar-plugin";
// Left beside them once the icon has been put in the bar, so installing again
// never puts back an icon someone took out.
const QString enabledMarker = ".talkback-enabled";
}

BarPlugin::BarPlugin(const QString &pluginsDirectory, const QString &omarchy, const QString &omarchyShell,
                     QObject *parent)
    : QObject(parent), m_omarchy(omarchy), m_omarchyShell(omarchyShell) {
    const QString plugins = !pluginsDirectory.isEmpty() ? pluginsDirectory
        : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/omarchy/plugins";
    m_directory = plugins + '/' + id;
}

QString BarPlugin::directory() const {
    return m_directory;
}

bool BarPlugin::available() const {
    const QString omarchyConfig = QFileInfo(QFileInfo(m_directory).absolutePath()).absolutePath();
    return QFileInfo(omarchyConfig).isDir() && !QStandardPaths::findExecutable(m_omarchyShell).isEmpty();
}

bool BarPlugin::installed() const {
    return QFileInfo::exists(m_directory + "/manifest.json");
}

bool BarPlugin::install() {
    if (!available())
        return fail(tr("The Omarchy shell isn't here, so there's no bar to add the icon to."));
    const bool firstTime = !QFileInfo::exists(m_directory + '/' + enabledMarker);
    if (!QDir().mkpath(m_directory))
        return fail(tr("Couldn't create %1.").arg(m_directory));
    if (!syncFiles())
        return false;
    emit changed();

    // The shell finds new plugins on a rescan; a copy it already knows reloads
    // by itself when its files change.
    if (firstTime) {
        if (!run(m_omarchyShell, {"shell", "rescanPlugins"}))
            return fail(tr("The bar plugin is in %1, but the Omarchy shell didn't pick it up. Run: %2")
                            .arg(m_directory, QString("omarchy-shell shell rescanPlugins && omarchy plugin enable ") + id));
        // The rescan finishes in the background, so the shell may not know
        // the plugin for a moment.
        bool enabled = false;
        for (int attempt = 0; attempt < 20 && !enabled; ++attempt) {
            if (attempt > 0)
                QThread::msleep(250);
            enabled = run(m_omarchy, {"plugin", "enable", id});
        }
        if (!enabled)
            return fail(tr("The bar plugin is installed but couldn't be turned on. Run: omarchy plugin enable %1").arg(id));
        QFile marker(m_directory + '/' + enabledMarker);
        if (!marker.open(QIODevice::WriteOnly))
            return fail(tr("Couldn't write %1: %2").arg(marker.fileName(), marker.errorString()));
    }
    m_error.clear();
    emit changed();
    return true;
}

bool BarPlugin::refresh(bool *updated) {
    if (updated)
        *updated = false;
    // Only a copy Talkback put there, so a removed icon stays removed.
    if (!installed())
        return true;
    int changes = 0;
    if (!syncFiles(&changes))
        return false;
    if (updated)
        *updated = changes > 0;
    if (changes > 0) {
        m_error.clear();
        emit changed();
    }
    return true;
}

// Makes the plugin directory match the copy in the binary: files that differ
// are rewritten and files it no longer has are deleted. Untouched files keep
// their timestamps, so the shell only reloads the plugin when something changed.
bool BarPlugin::syncFiles(int *changes) {
    if (!QFileInfo::exists(bundled + "/manifest.json"))
        return fail(tr("This build of Talkback is missing its bar plugin."));
    int count = 0;
    QSet<QString> wanted{enabledMarker};
    QDirIterator it(bundled, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString name = path.mid(bundled.size() + 1);
        wanted.insert(name);
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly))
            return fail(tr("This build of Talkback is missing its bar plugin."));
        const QByteArray contents = source.readAll();
        const QString targetPath = m_directory + '/' + name;
        QFile current(targetPath);
        if (current.open(QIODevice::ReadOnly) && current.readAll() == contents)
            continue;
        current.close();
        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath()))
            return fail(tr("Couldn't write %1.").arg(targetPath));
        // Written beside the target and renamed, so the shell never loads half a file.
        QSaveFile target(targetPath);
        if (!target.open(QIODevice::WriteOnly) || target.write(contents) < 0 || !target.commit())
            return fail(tr("Couldn't write %1: %2").arg(target.fileName(), target.errorString()));
        ++count;
    }
    // A file an older version shipped would otherwise linger, and a stray
    // QML file could even load.
    QDirIterator existing(m_directory, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (existing.hasNext()) {
        const QString path = existing.next();
        if (!wanted.contains(path.mid(m_directory.size() + 1)) && QFile::remove(path))
            ++count;
    }
    if (changes)
        *changes = count;
    return true;
}

bool BarPlugin::remove() {
    if (!installed()) {
        m_error.clear();
        return true;
    }
    // Disabling only fails when the shell doesn't know the plugin, which is
    // fine when the files are going anyway.
    run(m_omarchy, {"plugin", "disable", id});
    if (!QDir(m_directory).removeRecursively())
        return fail(tr("Couldn't delete %1.").arg(m_directory));
    run(m_omarchyShell, {"shell", "rescanPlugins"});
    m_error.clear();
    emit changed();
    return true;
}

bool BarPlugin::run(const QString &program, const QStringList &arguments) {
    QProcess process;
    // Their output would only repeat what Talkback says, and a retry would
    // print every miss.
    process.start(program, arguments);
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool BarPlugin::fail(const QString &error) {
    m_error = error;
    emit changed();
    return false;
}

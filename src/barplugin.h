#pragma once

#include <QObject>
#include <QStringList>

// Talkback's icon in the Omarchy bar: a small shell plugin, bundled in the
// binary, that shows whether spoken replies are on and switches them with a
// click. Installing it copies it into ~/.config/omarchy/plugins and enables it.
class BarPlugin : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    static constexpr const char *id = "io.github.mattwolfgang.talkback";

    // `pluginsDirectory` defaults to ~/.config/omarchy/plugins; the two
    // programs are Omarchy's, and tests pass stand-ins.
    explicit BarPlugin(const QString &pluginsDirectory = {}, const QString &omarchy = "omarchy",
                       const QString &omarchyShell = "omarchy-shell", QObject *parent = nullptr);

    // Whether this is Omarchy with its shell, the only bar Talkback joins.
    bool available() const;
    bool installed() const;
    QString directory() const;
    QString error() const { return m_error; }

    // Writes the plugin's files, refreshing an older copy, and enables it the
    // first time so it takes its place in the bar. A copy already there keeps
    // whatever place it has, or stays off if it was turned off.
    Q_INVOKABLE bool install();
    // Brings an installed copy up to the version in this binary, after a
    // package update, without touching its place in the bar. Does nothing
    // when the plugin isn't installed. `updated` says whether files changed.
    bool refresh(bool *updated = nullptr);
    // Takes the icon out of the bar and deletes the plugin's files.
    Q_INVOKABLE bool remove();

signals:
    void changed();

private:
    bool syncFiles(int *changes = nullptr);
    bool run(const QString &program, const QStringList &arguments);
    bool fail(const QString &error);

    QString m_directory;
    QString m_omarchy, m_omarchyShell;
    QString m_error;
};

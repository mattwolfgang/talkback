#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

// The two Hyprland keys that work from anywhere: one turns spoken replies on
// and off, the other stops a reply. On Omarchy they're o.bind lines in
// ~/.config/hypr/bindings.lua. Talkback finds them by their commands, so
// they can also be moved or changed by hand.
class Keybindings : public QObject {
    Q_OBJECT
    // Whether this is Omarchy's Lua config, the only one Talkback edits.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // Hyprland combos like "SUPER + ALT + X"; empty when not bound.
    Q_PROPERTY(QString toggleKey READ toggleKey NOTIFY changed)
    Q_PROPERTY(QString stopKey READ stopKey NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    static constexpr const char *defaultToggle = "SUPER + ALT + X";
    static constexpr const char *defaultStop = "SUPER + ALT + ESCAPE";

    explicit Keybindings(const QString &bindingsFile = {}, const QString &hyprctl = "hyprctl",
                         QObject *parent = nullptr);

    static QString defaultBindingsFile();

    bool available() const;
    QString toggleKey() const { return m_toggleKey; }
    QString stopKey() const { return m_stopKey; }
    QString error() const { return m_error; }

    // Binds each missing key to its default, unless something else already
    // has that key. False with `error` saying which keys were left alone.
    Q_INVOKABLE bool install();
    // `action` is "toggle" or "stop".
    Q_INVOKABLE bool setKey(const QString &action, const QString &combo);
    // Hyprland's name for a Qt key, or empty when it can't be bound.
    Q_INVOKABLE QString keyName(int qtKey) const;
    // "SUPER + ALT + ESCAPE" as people write it: "Super + Alt + Esc".
    Q_INVOKABLE QString label(const QString &combo) const;

    // The combo of the o.bind line running `command`, or empty.
    static QString keyIn(const QString &lua, const QString &action);
    // The config with `action` bound to `combo`: its line changed in place,
    // or added under a "-- Talkback" heading at the end.
    static QString withKey(const QString &lua, const QString &action, const QString &combo);
    // Upper case with single spaces around each "+"; empty when it isn't a
    // combo Hyprland can bind (an unknown modifier, or no key).
    static QString normalize(const QString &combo);
    static int modmask(const QString &combo);
    // What `hyprctl binds -j` says already uses `combo`, besides Talkback's own
    // binding for `action`; empty when it's free.
    static QString holder(const QByteArray &binds, const QString &combo, const QString &action);

signals:
    void changed();
    void errorChanged();

private:
    void reload();
    void watch();
    bool write(const QString &lua);
    QByteArray hyprctl(const QStringList &arguments, bool *ok = nullptr) const;
    void setError(const QString &error);

    QString m_path;
    QString m_hyprctl;
    QString m_toggleKey;
    QString m_stopKey;
    QString m_error;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

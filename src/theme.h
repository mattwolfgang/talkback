#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The Omarchy look, read from the current theme: the palette in colors.toml
// and the shell's control and type tokens in shell.toml, so the window draws
// its controls the way the bar, menus and launcher do.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor urgent READ urgent NOTIFY changed)
    Q_PROPERTY(QColor popupBorder READ popupBorder NOTIFY changed)
    Q_PROPERTY(QColor rowSelectedText READ rowSelectedText NOTIFY changed)
    Q_PROPERTY(bool light READ light NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    Q_PROPERTY(int fontSize READ fontSize NOTIFY changed)
    Q_PROPERTY(double normalFill READ normalFill NOTIFY changed)
    Q_PROPERTY(double hoverFill READ hoverFill NOTIFY changed)
    Q_PROPERTY(double selectedFill READ selectedFill NOTIFY changed)
    Q_PROPERTY(double pressedFill READ pressedFill NOTIFY changed)
    Q_PROPERTY(double selectionFill READ selectionFill NOTIFY changed)
    Q_PROPERTY(double normalBorder READ normalBorder NOTIFY changed)
    Q_PROPERTY(double hoverBorder READ hoverBorder NOTIFY changed)
    Q_PROPERTY(double rowSelectedFill READ rowSelectedFill NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QColor accent() const { return color("accent", QColor("#FFD60A")); }
    QColor accentForeground() const;
    QColor foreground() const { return color("foreground", QColor("#e4e4e8")); }
    QColor background() const { return color("background", QColor("#0e0e10")); }
    QColor urgent() const;
    QColor popupBorder() const { return shellColor("popups.border", accent()); }
    QColor rowSelectedText() const { return shellColor("menu.selected-text", accent()); }
    bool light() const { return m_colors.value("mode") == "light"; }
    int radius() const { return m_radius; }
    int fontSize() const;
    double normalFill() const { return alpha("controls.normal-fill-alpha", 0.04); }
    double hoverFill() const { return alpha("controls.hover-cursor-fill-alpha", 0.08); }
    double selectedFill() const { return alpha("controls.selected-fill-alpha", 0.18); }
    double pressedFill() const { return alpha("controls.pressed-fill-alpha", 0.22); }
    double selectionFill() const { return alpha("controls.selection-fill-alpha", 0.35); }
    double normalBorder() const { return alpha("controls.normal-border-alpha", 0.4); }
    double hoverBorder() const { return alpha("controls.hover-cursor-border-alpha", 0.25); }
    double rowSelectedFill() const { return alpha("menu.selected-background-alpha", 0.08); }

    // "section.key" to the raw value, with quotes and comments stripped.
    static QHash<QString, QString> readToml(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    QColor color(const QString &key, const QColor &fallback) const;
    QColor shellColor(const QString &key, const QColor &fallback) const;
    QColor parseColor(QString value, const QColor &fallback, int depth = 0) const;
    double alpha(const QString &key, double fallback) const;

    QString m_directory;
    QHash<QString, QString> m_colors, m_shell;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};

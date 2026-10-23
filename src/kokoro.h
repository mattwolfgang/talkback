#pragma once

#include <QObject>
#include <QProcess>
#include <QVariantList>

// Kokoro, the second speech engine: whether it's installed, installing it
// (talkback-kokoro-setup), and its voices (talkback-kokoro --list).
class Kokoro : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool installing READ installing NOTIFY changed)
    Q_PROPERTY(QString progress READ progress NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList voices READ voices NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)

public:
    explicit Kokoro(QObject *parent = nullptr);
    ~Kokoro() override;

    static QString python();

    bool installed() const { return m_installed; }
    bool installing() const { return m_setup.state() != QProcess::NotRunning; }
    QString progress() const { return m_progress; }
    QString error() const { return m_error; }
    QVariantList voices() const;
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    Q_INVOKABLE void install();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap voice(const QString &key) const;

    // A Kokoro voice name (af_heart) to {key, name, language, quality}: the
    // first letter is the language, the second f or m.
    static QVariantMap describe(const QString &key);
    static QVariantList parseList(const QByteArray &output);

signals:
    void changed();

private:
    bool m_installed = false;
    QString m_progress;
    QString m_error;
    QVariantList m_voices;
    QString m_filter;
    QProcess m_setup, m_list;
};

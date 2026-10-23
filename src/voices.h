#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QVariantList>

// Piper's voice catalog (voices.json from rhasspy/piper-voices), which voices
// are downloaded, and downloading or removing them. Downloads go through
// Piper's own downloader, so files land exactly where piper-tts expects them.
class Voices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList voices READ voices NOTIFY voicesChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY voicesChanged)
    Q_PROPERTY(bool installedOnly READ installedOnly WRITE setInstalledOnly NOTIFY voicesChanged)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY voicesChanged)
    Q_PROPERTY(QString downloading READ downloading NOTIFY busyChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool piperMissing READ piperMissing CONSTANT)

public:
    explicit Voices(const QString &voiceDirectory = {}, const QString &cachePath = {}, QObject *parent = nullptr);
    ~Voices() override;

    static QString defaultVoiceDirectory();
    static const char *catalogUrl;

    QVariantList voices() const { return m_filtered; }
    QString filter() const { return m_filter; }
    bool installedOnly() const { return m_installedOnly; }
    int installedCount() const { return m_installed.size(); }
    QString downloading() const { return m_downloading; }
    bool loading() const { return m_reply != nullptr; }
    QString error() const { return m_error; }
    bool piperMissing() const { return m_piperMissing; }

    void setFilter(const QString &filter);
    void setInstalledOnly(bool installedOnly);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void download(const QString &key);
    Q_INVOKABLE bool remove(const QString &key);
    Q_INVOKABLE bool isInstalled(const QString &key) const { return m_installed.contains(key); }
    Q_INVOKABLE QVariantMap voice(const QString &key) const;

    // Catalog JSON to one map per voice, sorted with `firstFamily` (the
    // desktop's language) first. Voices downloaded from elsewhere that the
    // catalog doesn't list still show, so they can be used and removed.
    static QVariantList parseCatalog(const QByteArray &json, const QSet<QString> &installed,
                                     const QString &firstFamily = {});
    static QVariantList filtered(const QVariantList &all, const QString &filter, bool installedOnly);
    static QSet<QString> scanInstalled(const QString &directory);

signals:
    void voicesChanged();
    void busyChanged();
    void errorChanged();
    void downloaded(const QString &key);

private:
    void rebuild();
    void setError(const QString &error);

    QString m_directory;
    QString m_cachePath;
    QByteArray m_catalog;
    QSet<QString> m_installed;
    QVariantList m_all, m_filtered;
    QString m_filter;
    bool m_installedOnly = false;
    QString m_downloading;
    QString m_error;
    bool m_piperMissing = false;
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;
    QProcess m_download;
};

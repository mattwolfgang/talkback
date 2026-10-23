#include "voices.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

const char *Voices::catalogUrl = "https://huggingface.co/rhasspy/piper-voices/resolve/main/voices.json";

Voices::Voices(const QString &voiceDirectory, const QString &cachePath, QObject *parent)
    : QObject(parent),
      m_directory(voiceDirectory.isEmpty() ? defaultVoiceDirectory() : voiceDirectory),
      m_cachePath(cachePath.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/talkback/voices.json"
                      : cachePath) {
    m_piperMissing = QStandardPaths::findExecutable("piper-tts").isEmpty();

    connect(&m_download, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString key = m_downloading;
        m_downloading.clear();
        m_installed = scanInstalled(m_directory);
        if (code != 0 || status != QProcess::NormalExit || !m_installed.contains(key)) {
            const QString output = QString::fromUtf8(m_download.readAllStandardError()).trimmed();
            setError(tr("Couldn't download %1. %2").arg(key, output.section('\n', -1)));
        } else {
            setError({});
            emit downloaded(key);
        }
        rebuild();
        emit busyChanged();
    });
    connect(&m_download, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_downloading.clear();
        setError(tr("Couldn't start Python to download the voice."));
        emit busyChanged();
    });

    m_installed = scanInstalled(m_directory);
    QFile cache(m_cachePath);
    if (cache.open(QIODevice::ReadOnly))
        m_catalog = cache.readAll();
    rebuild();
}

Voices::~Voices() {
    if (m_download.state() != QProcess::NotRunning) {
        m_download.kill();
        m_download.waitForFinished(1000);
    }
}

QString Voices::defaultVoiceDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/piper-voices";
}

void Voices::setFilter(const QString &filter) {
    if (filter == m_filter)
        return;
    m_filter = filter;
    m_filtered = filtered(m_all, m_filter, m_installedOnly);
    emit voicesChanged();
}

void Voices::setInstalledOnly(bool installedOnly) {
    if (installedOnly == m_installedOnly)
        return;
    m_installedOnly = installedOnly;
    m_filtered = filtered(m_all, m_filter, m_installedOnly);
    emit voicesChanged();
}

void Voices::refresh() {
    if (m_reply)
        return;
    m_installed = scanInstalled(m_directory);
    rebuild();

    QNetworkRequest request{QUrl(catalogUrl)};
    request.setTransferTimeout(20000);
    m_reply = m_network.get(request);
    emit busyChanged();
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || !QJsonDocument::fromJson(data).isObject()) {
            // An old copy is still good enough to pick from, so only say so
            // when there's nothing to show.
            if (m_catalog.isEmpty())
                setError(tr("Couldn't load Piper's voice list: %1").arg(reply->errorString()));
        } else {
            m_catalog = data;
            QDir().mkpath(QFileInfo(m_cachePath).absolutePath());
            QSaveFile cache(m_cachePath);
            if (cache.open(QIODevice::WriteOnly)) {
                cache.write(data);
                cache.commit();
            }
            if (m_downloading.isEmpty())
                setError({});
            rebuild();
        }
        emit busyChanged();
    });
}

void Voices::download(const QString &key) {
    if (!m_downloading.isEmpty() || key.isEmpty() || m_installed.contains(key))
        return;
    QDir().mkpath(m_directory);
    m_downloading = key;
    setError({});
    emit busyChanged();
    m_download.start("python", {"-m", "piper.download_voices", "--download-dir", m_directory, key});
}

bool Voices::remove(const QString &key) {
    if (!m_installed.contains(key))
        return false;
    QDir directory(m_directory);
    const bool removed = directory.remove(key + ".onnx");
    directory.remove(key + ".onnx.json");
    m_installed = scanInstalled(m_directory);
    if (!removed)
        setError(tr("Couldn't remove %1.").arg(key));
    rebuild();
    return removed;
}

QVariantMap Voices::voice(const QString &key) const {
    for (const QVariant &entry : m_all)
        if (entry.toMap().value("key").toString() == key)
            return entry.toMap();
    return {};
}

void Voices::rebuild() {
    m_all = parseCatalog(m_catalog, m_installed, QLocale().name().section('_', 0, 0));
    m_filtered = filtered(m_all, m_filter, m_installedOnly);
    emit voicesChanged();
}

void Voices::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

QSet<QString> Voices::scanInstalled(const QString &directory) {
    QSet<QString> installed;
    const QStringList models = QDir(directory).entryList({"*.onnx"}, QDir::Files);
    for (const QString &model : models)
        if (QFileInfo::exists(directory + "/" + model + ".json"))
            installed.insert(model.chopped(5));
    return installed;
}

int Voices::speakerCount(const QString &directory, const QString &key) {
    QFile config(directory + "/" + key + ".onnx.json");
    if (!config.open(QIODevice::ReadOnly))
        return 1;
    return qMax(1, QJsonDocument::fromJson(config.readAll()).object().value("num_speakers").toInt(1));
}

QVariantList Voices::parseCatalog(const QByteArray &json, const QSet<QString> &installed, const QString &firstFamily) {
    static const QHash<QString, int> qualityOrder{{"x_low", 0}, {"low", 1}, {"medium", 2}, {"high", 3}};
    const QJsonObject catalog = QJsonDocument::fromJson(json).object();

    QVariantList voices;
    QSet<QString> listed;
    for (auto it = catalog.begin(); it != catalog.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const QJsonObject language = entry.value("language").toObject();
        qint64 bytes = 0;
        const QJsonObject files = entry.value("files").toObject();
        for (auto file = files.begin(); file != files.end(); ++file)
            bytes += file.value().toObject().value("size_bytes").toInteger();

        voices.append(QVariantMap{
            {"key", it.key()},
            {"name", entry.value("name").toString()},
            {"family", language.value("family").toString()},
            {"language", language.value("name_english").toString()},
            {"country", language.value("country_english").toString()},
            {"quality", entry.value("quality").toString()},
            {"qualityRank", qualityOrder.value(entry.value("quality").toString(), 2)},
            {"speakers", entry.value("num_speakers").toInt(1)},
            {"megabytes", qRound(bytes / 1e6)},
            {"installed", installed.contains(it.key())},
        });
        listed.insert(it.key());
    }

    for (const QString &key : installed) {
        if (listed.contains(key))
            continue;
        voices.append(QVariantMap{
            {"key", key}, {"name", key}, {"family", key.section('_', 0, 0)}, {"language", key.section('-', 0, 0)},
            {"country", QString()}, {"quality", QString()}, {"qualityRank", 2}, {"speakers", 1},
            {"megabytes", 0}, {"installed", true},
        });
    }

    std::sort(voices.begin(), voices.end(), [&](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap(), y = b.toMap();
        const bool xFirst = x.value("family") == firstFamily, yFirst = y.value("family") == firstFamily;
        if (xFirst != yFirst)
            return xFirst;
        const auto tuple = [](const QVariantMap &v) {
            return std::make_tuple(v.value("language").toString(), v.value("country").toString(),
                                   v.value("name").toString(), v.value("qualityRank").toInt());
        };
        return tuple(x) < tuple(y);
    });
    return voices;
}

QVariantList Voices::filtered(const QVariantList &all, const QString &filter, bool installedOnly) {
    const QStringList words = filter.split(' ', Qt::SkipEmptyParts);
    QVariantList result;
    for (const QVariant &entry : all) {
        const QVariantMap voice = entry.toMap();
        if (installedOnly && !voice.value("installed").toBool())
            continue;
        const QString haystack = QStringList{voice.value("key").toString(), voice.value("name").toString(),
                                             voice.value("language").toString(), voice.value("country").toString(),
                                             voice.value("quality").toString()}.join(' ');
        const bool matches = std::all_of(words.begin(), words.end(), [&](const QString &word) {
            return haystack.contains(word, Qt::CaseInsensitive);
        });
        if (matches)
            result.append(voice);
    }
    return result;
}

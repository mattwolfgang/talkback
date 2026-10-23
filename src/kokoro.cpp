#include "kokoro.h"

#include <QFileInfo>
#include <QStandardPaths>

#include "speaker.h"
#include "voices.h"

Kokoro::Kokoro(QObject *parent) : QObject(parent) {
    m_setup.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_setup, &QProcess::readyRead, this, [this] {
        const QStringList lines = QString::fromUtf8(m_setup.readAll()).split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            m_progress = lines.last().trimmed();
            emit changed();
        }
    });
    connect(&m_setup, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (code != 0 || status != QProcess::NormalExit)
            m_error = tr("Couldn't install Kokoro. %1").arg(m_progress);
        m_progress.clear();
        emit changed();
        refresh();
    });
    connect(&m_setup, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_error = tr("Couldn't run talkback-kokoro-setup.");
            emit changed();
        }
    });
    connect(&m_list, &QProcess::finished, this, [this](int code) {
        m_voices = code == 0 ? parseList(m_list.readAllStandardOutput()) : QVariantList();
        m_installed = !m_voices.isEmpty();
        if (code != 0 && m_error.isEmpty())
            m_error = tr("Kokoro is installed but didn't start: %1")
                          .arg(QString::fromUtf8(m_list.readAllStandardError()).trimmed().section('\n', -1));
        emit changed();
    });
    refresh();
}

Kokoro::~Kokoro() {
    for (QProcess *process : {&m_setup, &m_list})
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
}

QString Kokoro::python() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/talkback/kokoro/venv/bin/python";
}

void Kokoro::refresh() {
    if (m_list.state() != QProcess::NotRunning)
        return;
    if (!QFileInfo(python()).isExecutable()) {
        if (m_installed) {
            m_installed = false;
            m_voices.clear();
            emit changed();
        }
        return;
    }
    m_list.start(python(), {"-I", Speaker::helper("talkback-kokoro"), "--list"});
}

void Kokoro::install() {
    if (installing())
        return;
    m_error.clear();
    m_progress = tr("Starting…");
    m_setup.start(Speaker::helper("talkback-kokoro-setup"), {});
    emit changed();
}

void Kokoro::setFilter(const QString &filter) {
    if (filter == m_filter)
        return;
    m_filter = filter;
    emit changed();
}

QVariantList Kokoro::voices() const {
    return Voices::filtered(m_voices, m_filter, false);
}

QVariantMap Kokoro::voice(const QString &key) const {
    for (const QVariant &entry : m_voices)
        if (entry.toMap().value("key").toString() == key)
            return entry.toMap();
    return describe(key);
}

QVariantMap Kokoro::describe(const QString &key) {
    static const QHash<QChar, std::pair<const char *, const char *>> languages{
        {'a', {QT_TR_NOOP("English"), QT_TR_NOOP("United States")}},
        {'b', {QT_TR_NOOP("English"), QT_TR_NOOP("Great Britain")}},
        {'e', {QT_TR_NOOP("Spanish"), ""}},
        {'f', {QT_TR_NOOP("French"), ""}},
        {'h', {QT_TR_NOOP("Hindi"), ""}},
        {'i', {QT_TR_NOOP("Italian"), ""}},
        {'j', {QT_TR_NOOP("Japanese"), ""}},
        {'p', {QT_TR_NOOP("Portuguese"), QT_TR_NOOP("Brazil")}},
        {'z', {QT_TR_NOOP("Chinese"), ""}},
    };
    const auto at = [&key](int i) { return i < key.size() ? key.at(i) : QChar(); };
    const auto language = languages.value(at(0), {"", ""});
    QString name = key.section('_', 1);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    const QChar gender = at(1);
    return QVariantMap{
        {"key", key},
        {"name", name.isEmpty() ? key : name},
        {"family", QString(at(0))},
        {"language", tr(language.first)},
        {"country", *language.second ? tr(language.second) : QString()},
        {"quality", gender == u'f' ? tr("female") : gender == u'm' ? tr("male") : QString()},
        {"qualityRank", 0},
        {"speakers", 1},
        {"megabytes", 0},
        {"installed", true},
        {"engine", QStringLiteral("kokoro")},
    };
}

QVariantList Kokoro::parseList(const QByteArray &output) {
    QVariantList voices;
    for (const QByteArray &line : output.split('\n')) {
        const QString key = QString::fromUtf8(line).trimmed();
        if (!key.isEmpty())
            voices.append(describe(key));
    }
    // American English first, then British, then the rest by language.
    std::stable_sort(voices.begin(), voices.end(), [](const QVariant &a, const QVariant &b) {
        static const QString order = "abefhijpz";
        return order.indexOf(a.toMap().value("family").toString()) < order.indexOf(b.toMap().value("family").toString());
    });
    return voices;
}

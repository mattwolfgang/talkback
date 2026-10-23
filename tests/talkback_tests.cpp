#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "dictation.h"
#include "settings.h"
#include "speaker.h"
#include "theme.h"
#include "translator.h"
#include "voices.h"

class TalkbackTests : public QObject {
    Q_OBJECT

private slots:
    void settingsDefaultWithoutAFile()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("talkback/settings.conf"));
        QVERIFY(settings.enabled());
        QCOMPARE(settings.voice(), QString(Settings::defaultVoice));
        QCOMPARE(settings.speed(), 1.0);
    }

    void settingsSaveWhatTheScriptReads()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("talkback/settings.conf");
        Settings settings(path);
        settings.setEnabled(false);
        settings.setVoice("en_GB-alba-medium");
        settings.setSpeed(1.234);
        QCOMPARE(read(path), QByteArray("enabled=false\nvoice=en_GB-alba-medium\nspeed=1.23\n"
                                        "expression=0.67\nrhythm=0.80\npause=0.30\n"));

        Settings again(path);
        QVERIFY(!again.enabled());
        QCOMPARE(again.voice(), QString("en_GB-alba-medium"));
        QCOMPARE(again.speed(), 1.23);
    }

    void settingsKeepNaturalness()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.conf");
        write(path, "expression=0.9\nrhythm=1.1\npause=7\n");
        Settings settings(path);
        QCOMPARE(settings.expression(), 0.9);
        QCOMPARE(settings.rhythm(), 1.1);
        QCOMPARE(settings.pause(), 1.5);
        settings.resetNaturalness();
        QCOMPARE(settings.expression(), Settings::defaultExpression);
        QCOMPARE(settings.pause(), Settings::defaultPause);
    }

    void settingsClampSpeed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("settings.conf"));
        settings.setSpeed(9);
        QCOMPARE(settings.speed(), 2.0);
        settings.setSpeed(0.1);
        QCOMPARE(settings.speed(), 0.5);
    }

    void settingsFollowOutsideChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.conf");
        write(path, "enabled=true\n");
        Settings settings(path);
        QSignalSpy spy(&settings, &Settings::changed);
        write(path, "enabled=false\nvoice=x\n");
        QTRY_VERIFY(!settings.enabled());
        QCOMPARE(settings.voice(), QString("x"));
        QVERIFY(spy.count() >= 1);
    }

    void catalogSortsTheDesktopLanguageFirst()
    {
        const QByteArray json = R"({
            "de_DE-thorsten-high": {"name": "thorsten", "quality": "high", "num_speakers": 1,
                "language": {"family": "de", "name_english": "German", "country_english": "Germany"},
                "files": {"a.onnx": {"size_bytes": 2000000}, "a.onnx.json": {"size_bytes": 1000}}},
            "en_US-lessac-medium": {"name": "lessac", "quality": "medium", "num_speakers": 1,
                "language": {"family": "en", "name_english": "English", "country_english": "United States"},
                "files": {}}
        })";
        const QVariantList voices = Voices::parseCatalog(json, {"en_US-lessac-medium", "my-own-voice"}, "en");
        QCOMPARE(voices.size(), 3);
        QCOMPARE(voices.at(0).toMap().value("key").toString(), QString("en_US-lessac-medium"));
        QVERIFY(voices.at(0).toMap().value("installed").toBool());
        const QVariantMap german = voices.at(1).toMap().value("family") == "de" ? voices.at(1).toMap()
                                                                                 : voices.at(2).toMap();
        QCOMPARE(german.value("megabytes").toInt(), 2);
        QVERIFY(!german.value("installed").toBool());
    }

    void filterMatchesEveryWord()
    {
        const QVariantList all{
            QVariantMap{{"key", "en_GB-alba-medium"}, {"name", "alba"}, {"language", "English"},
                        {"country", "Great Britain"}, {"quality", "medium"}, {"installed", false}},
            QVariantMap{{"key", "en_US-amy-low"}, {"name", "amy"}, {"language", "English"},
                        {"country", "United States"}, {"quality", "low"}, {"installed", true}},
        };
        QCOMPARE(Voices::filtered(all, "english britain", false).size(), 1);
        QCOMPARE(Voices::filtered(all, "ENGLISH", false).size(), 2);
        QCOMPARE(Voices::filtered(all, "", true).size(), 1);
    }

    void scanNeedsTheModelAndItsConfig()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.onnx"), "x");
        write(dir.filePath("a.onnx.json"), "{}");
        write(dir.filePath("b.onnx"), "x");
        QCOMPARE(Voices::scanInstalled(dir.path()), QSet<QString>{"a"});
    }

    void dictationReadsSystemctl()
    {
        QCOMPARE(Dictation::parseState("loaded\nactive\n"), std::make_pair(true, true));
        QCOMPARE(Dictation::parseState("loaded\ninactive\n"), std::make_pair(true, false));
        QCOMPARE(Dictation::parseState("not-found\ninactive\n"), std::make_pair(false, false));
    }

    void hookReplacesTheOldOneAndKeepsOthers()
    {
        const QJsonObject before = QJsonDocument::fromJson(R"({
            "theme": "dark",
            "hooks": {
                "SessionStart": [{"hooks": [{"type": "command", "command": "other.sh"}]}],
                "Stop": [{"hooks": [{"type": "command", "command": "bash /x/speak-reply.sh"}]},
                         {"hooks": [{"type": "command", "command": "keep-me.sh"}]}]
            }
        })").object();
        QVERIFY(!Speaker::hasHook(before));

        const QJsonObject after = Speaker::withHook(before);
        QVERIFY(Speaker::hasHook(after));
        QCOMPARE(after.value("theme").toString(), QString("dark"));
        QCOMPARE(after.value("hooks").toObject().value("SessionStart"), before.value("hooks").toObject().value("SessionStart"));
        const QJsonArray stop = after.value("hooks").toObject().value("Stop").toArray();
        QCOMPARE(stop.size(), 2);
        QVERIFY(!QJsonDocument(stop).toJson().contains("speak-reply.sh"));
        QVERIFY(QJsonDocument(stop).toJson().contains("keep-me.sh"));

        // Installing twice leaves one hook.
        QCOMPARE(Speaker::withHook(after).value("hooks").toObject().value("Stop").toArray().size(), 2);
    }

    void hookInstallRefusesBrokenJson()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.json");
        write(path, "{ not json");
        Speaker speaker(path);
        QVERIFY(!speaker.installHook());
        QCOMPARE(read(path), QByteArray("{ not json"));
    }

    // QML can't call a method whose parameter types it doesn't know; it fails
    // at the call ("Unknown method parameter type"), not at load.
    void invokablesTakeTypesQmlKnows()
    {
        Settings settings(QDir::tempPath() + "/talkback-unused.conf");
        Voices voices(QDir::tempPath() + "/talkback-no-voices", QDir::tempPath() + "/talkback-no-cache.json");
        Dictation dictation("true");
        Speaker speaker(QDir::tempPath() + "/talkback-no-claude.json");
        for (const QObject *object : std::initializer_list<const QObject *>{&settings, &voices, &dictation, &speaker}) {
            const QMetaObject *meta = object->metaObject();
            for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
                const QMetaMethod method = meta->method(i);
                if (method.methodType() != QMetaMethod::Method)
                    continue;
                for (int p = 0; p < method.parameterCount(); ++p)
                    QVERIFY2(method.parameterMetaType(p).isValid(),
                             qPrintable(QString("%1::%2").arg(meta->className(), method.methodSignature())));
            }
        }
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
};

QTEST_MAIN(TalkbackTests)
#include "talkback_tests.moc"

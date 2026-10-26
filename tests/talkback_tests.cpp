#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "dictation.h"
#include "hooks.h"
#include "keybindings.h"
#include "kokoro.h"
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
        QCOMPARE(read(path), QByteArray("enabled=false\nengine=piper\nvoice=en_GB-alba-medium\nkokoroVoice=af_heart\n"
                                        "speed=1.23\nexpression=0.67\nrhythm=0.80\npause=0.30\n"));

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

    void settingsRememberASpeakerPerVoice()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.conf");
        {
            Settings settings(path);
            settings.setVoice("en_US-libritts_r-medium");
            settings.setSpeaker("en_US-libritts_r-medium", 412);
            settings.setSpeaker("en_GB-vctk-medium", 7);
            QCOMPARE(settings.speaker(), 412);
            QVERIFY(read(path).contains("speaker.en_US-libritts_r-medium=412\n"));
            settings.setSpeaker("en_GB-vctk-medium", 0);
            QVERIFY(!read(path).contains("en_GB-vctk-medium"));
        }
        Settings again(path);
        QCOMPARE(again.speaker(), 412);
        QCOMPARE(again.speakerFor("en_GB-vctk-medium"), 0);
        QCOMPARE(again.speakers().value("en_US-libritts_r-medium").toInt(), 412);
    }

    void speakerCountComesFromTheVoiceConfig()
    {
        QTemporaryDir dir;
        write(dir.filePath("many.onnx.json"), R"({"num_speakers": 904})");
        QCOMPARE(Voices::speakerCount(dir.path(), "many"), 904);
        QCOMPARE(Voices::speakerCount(dir.path(), "missing"), 1);
    }

    void settingsKeepAVoicePerEngine()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.conf");
        {
            Settings settings(path);
            settings.setVoice("en_GB-alba-medium");
            settings.setEngine("kokoro");
            settings.setKokoroVoice("bm_george");
            settings.setEngine("espeak");
            QCOMPARE(settings.engine(), QString("kokoro"));
            QCOMPARE(settings.currentVoice(), QString("bm_george"));
        }
        Settings again(path);
        QCOMPARE(again.engine(), QString("kokoro"));
        QCOMPARE(again.kokoroVoice(), QString("bm_george"));
        again.setEngine("piper");
        QCOMPARE(again.currentVoice(), QString("en_GB-alba-medium"));
    }

    void kokoroVoiceNamesDescribeThemselves()
    {
        const QVariantMap heart = Kokoro::describe("af_heart");
        QCOMPARE(heart.value("name").toString(), QString("Heart"));
        QCOMPARE(heart.value("language").toString(), QString("English"));
        QCOMPARE(heart.value("country").toString(), QString("United States"));
        QCOMPARE(heart.value("quality").toString(), QString("female"));
        QCOMPARE(Kokoro::describe("bm_george").value("country").toString(), QString("Great Britain"));
        QCOMPARE(Kokoro::describe("zm_yunxi").value("quality").toString(), QString("male"));

        const QVariantList list = Kokoro::parseList("zf_xiaobei\nbf_emma\naf_heart\n\n");
        QCOMPARE(list.size(), 3);
        QCOMPARE(list.at(0).toMap().value("key").toString(), QString("af_heart"));
        QCOMPARE(list.at(1).toMap().value("key").toString(), QString("bf_emma"));
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
        for (const QString agent : {"claude", "codex", "grok"}) {
            QVERIFY(!Hooks::hasHook(agent, before));

            const QJsonObject after = Hooks::withHook(agent, before);
            QVERIFY(Hooks::hasHook(agent, after));
            QCOMPARE(after.value("theme").toString(), QString("dark"));
            QCOMPARE(after.value("hooks").toObject().value("SessionStart"), before.value("hooks").toObject().value("SessionStart"));
            const QJsonArray stop = after.value("hooks").toObject().value("Stop").toArray();
            QCOMPARE(stop.size(), 2);
            QVERIFY(!QJsonDocument(stop).toJson().contains("speak-reply.sh"));
            QVERIFY(QJsonDocument(stop).toJson().contains("keep-me.sh"));

            // Installing twice leaves one hook.
            QCOMPARE(Hooks::withHook(agent, after).value("hooks").toObject().value("Stop").toArray().size(), 2);
        }
        // Claude's command stays what earlier versions installed; the others name their agent.
        const auto command = [&](const QString &agent) {
            return Hooks::withHook(agent, {}).value("hooks").toObject().value("Stop").toArray()
                .at(0).toObject().value("hooks").toArray().at(0).toObject().value("command").toString();
        };
        QCOMPARE(command("claude"), QString("talkback-speak"));
        QCOMPARE(command("codex"), QString("talkback-speak --agent codex"));
    }

    void cursorHookUsesTheResponseEvent()
    {
        const QJsonObject before = QJsonDocument::fromJson(R"({
            "version": 1,
            "hooks": {"afterAgentResponse": [{"command": "keep-me.sh"}], "stop": [{"command": "other.sh"}]}
        })").object();
        QVERIFY(!Hooks::hasHook("cursor", before));
        const QJsonObject after = Hooks::withHook("cursor", Hooks::withHook("cursor", before));
        QVERIFY(Hooks::hasHook("cursor", after));
        const QJsonObject hooks = after.value("hooks").toObject();
        QCOMPARE(hooks.value("stop"), before.value("hooks").toObject().value("stop"));
        const QJsonArray responses = hooks.value("afterAgentResponse").toArray();
        QCOMPARE(responses.size(), 2);
        QCOMPARE(responses.at(1).toObject().value("command").toString(), QString("talkback-speak --agent cursor"));
        QCOMPARE(Hooks::withHook("cursor", {}).value("version").toInt(), 1);
    }

    void antigravityHookIsItsOwnNamedHook()
    {
        const QJsonObject before = QJsonDocument::fromJson(R"({
            "lint": {"PostToolUse": [{"matcher": "*", "hooks": [{"command": "lint.sh"}]}]}
        })").object();
        QVERIFY(!Hooks::hasHook("antigravity", before));
        const QJsonObject after = Hooks::withHook("antigravity", before);
        QVERIFY(Hooks::hasHook("antigravity", after));
        QCOMPARE(after.value("lint"), before.value("lint"));
        QCOMPARE(after.value("talkback").toObject().value("Stop").toArray().size(), 1);
    }

    void hookInstallWritesEachAgentsFile()
    {
        QTemporaryDir home;
        for (const QString &agent : Hooks::agents()) {
            QVERIFY(!Hooks::installed(agent, home.path()));
            QString error;
            QVERIFY2(Hooks::install(agent, home.path(), &error), qPrintable(error));
            QVERIFY2(Hooks::installed(agent, home.path()), qPrintable(agent));
        }
        QVERIFY(read(home.filePath(".omp/agent/extensions/talkback.ts")).contains("agent_end"));
        QVERIFY(QFile::exists(home.filePath(".grok/hooks/talkback.json")));
    }

    void installHookPicksTheAgentsFound()
    {
        QTemporaryDir home;
        QDir(home.path()).mkpath(".codex");
        QDir(home.path()).mkpath(".gemini");
        Speaker speaker(home.path());
        QVERIFY(!speaker.hookInstalled());
        QVERIFY(speaker.installHook());
        QVERIFY(speaker.hookInstalled());
        QVERIFY(Hooks::installed("codex", home.path()));
        // ~/.gemini alone is the Gemini CLI, not Antigravity.
        QVERIFY(!Hooks::installed("antigravity", home.path()));
        QVERIFY(!Hooks::installed("claude", home.path()));
    }

    void hookInstallRefusesBrokenJson()
    {
        QTemporaryDir home;
        QDir(home.path()).mkpath(".claude");
        const QString path = home.filePath(".claude/settings.json");
        write(path, "{ not json");
        Speaker speaker(home.path());
        QVERIFY(!speaker.installHook());
        QCOMPARE(read(path), QByteArray("{ not json"));
    }

    void keysAreFoundByTheirCommands()
    {
        const QString lua = "-- o.bind(\"SUPER + Q\", \"Old\", \"talkback-speak --stop\")\n"
                            "o.bind(\"super + alt + x\", \"Mine\", \"talkback speech toggle --notify\")\n";
        QCOMPARE(Keybindings::keyIn(lua, "toggle"), QString("SUPER + ALT + X"));
        // Commented-out lines don't count.
        QCOMPARE(Keybindings::keyIn(lua, "stop"), QString());

        const QString changed = Keybindings::withKey(lua, "toggle", "SUPER + CTRL + T");
        QVERIFY(changed.contains("o.bind(\"SUPER + CTRL + T\", \"Mine\", \"talkback speech toggle --notify\")"));
        // A missing key goes right after the other one.
        const QString added = Keybindings::withKey(changed, "stop", "SUPER + ALT + ESCAPE");
        QCOMPARE(added.split('\n').at(2), QString("o.bind(\"SUPER + ALT + ESCAPE\", \"Stop speaking\", \"talkback-speak --stop\")"));
        // With neither, both go under a heading at the end.
        const QString fresh = Keybindings::withKey("-- mine", "stop", "SUPER + ALT + ESCAPE");
        QCOMPARE(fresh, QString("-- mine\n\n-- Talkback\no.bind(\"SUPER + ALT + ESCAPE\", \"Stop speaking\", \"talkback-speak --stop\")\n"));
    }

    void keyCombosNormalizeAndLabel()
    {
        QCOMPARE(Keybindings::normalize(" super+alt + escape "), QString("SUPER + ALT + ESCAPE"));
        QCOMPARE(Keybindings::normalize("SUPER + HYPER + X"), QString());
        QCOMPARE(Keybindings::normalize("SUPER + ALT"), QString());
        QCOMPARE(Keybindings::modmask("SUPER + ALT + X"), 72);
        Keybindings keys(QDir::tempPath() + "/talkback-no-bindings.lua", "false");
        QCOMPARE(keys.label("SUPER + ALT + ESCAPE"), QString("Super + Alt + Esc"));
        QCOMPARE(keys.label("CTRL + SPACE"), QString("Ctrl + Space"));
        QCOMPARE(keys.keyName(Qt::Key_X), QString("X"));
        QCOMPARE(keys.keyName(Qt::Key_Escape), QString("ESCAPE"));
        QCOMPARE(keys.keyName(Qt::Key_Alt), QString());
    }

    void keyHolderSkipsTalkbacksOwn()
    {
        const QByteArray binds = R"([
            {"modmask": 72, "key": "ESCAPE", "submap": "", "description": "Stop speaking"},
            {"modmask": 72, "key": "K", "submap": "", "description": "Tmux keybindings"},
            {"modmask": 72, "key": "J", "submap": "resize", "description": "Only in a submap"}
        ])";
        QCOMPARE(Keybindings::holder(binds, "SUPER + ALT + ESCAPE", "stop"), QString());
        QCOMPARE(Keybindings::holder(binds, "SUPER + ALT + ESCAPE", "toggle"), QString("Stop speaking"));
        QCOMPARE(Keybindings::holder(binds, "super + alt + k", "stop"), QString("Tmux keybindings"));
        QCOMPARE(Keybindings::holder(binds, "SUPER + ALT + J", "stop"), QString());
    }

    void keysInstallSkipsTakenKeys()
    {
        QTemporaryDir dir;
        const QString lua = dir.filePath("bindings.lua");
        write(lua, "-- Keep only your personal keybinding overrides here.\no.bind(\"SUPER + B\", \"Browser\", \"x\")\n");
        write(dir.filePath("binds.json"), R"([{"modmask": 72, "key": "X", "submap": "", "description": "Something else"}])");
        const QString hyprctl = fakeHyprctl(dir);

        Keybindings keys(lua, hyprctl);
        QVERIFY(keys.available());
        QVERIFY(!keys.install());
        QVERIFY(keys.error().contains("Something else"));
        QCOMPARE(keys.toggleKey(), QString());
        QCOMPARE(keys.stopKey(), QString("SUPER + ALT + ESCAPE"));
        QVERIFY(read(lua).startsWith("-- Keep only your personal keybinding overrides here.\no.bind(\"SUPER + B\""));

        QVERIFY(!keys.setKey("toggle", "X"));
        QVERIFY(!keys.setKey("toggle", "SUPER + ALT + X"));
        QVERIFY2(keys.setKey("toggle", "SUPER + CTRL + T"), qPrintable(keys.error()));
        QCOMPARE(keys.toggleKey(), QString("SUPER + CTRL + T"));
        QCOMPARE(Keybindings::keyIn(read(lua), "toggle"), QString("SUPER + CTRL + T"));
    }

    void keysChangeHyprlandRejectsIsUndone()
    {
        QTemporaryDir dir;
        const QString lua = dir.filePath("bindings.lua");
        const QByteArray before = "o.bind(\"SUPER + ALT + ESCAPE\", \"Stop speaking\", \"talkback-speak --stop\")\n";
        write(lua, before);
        write(dir.filePath("binds.json"), "[]");
        // This Hyprland reports an error after every reload.
        write(dir.filePath("errors-after-reload"), "bindings.lua:1: bad key");
        Keybindings keys(lua, fakeHyprctl(dir));
        QVERIFY(!keys.setKey("stop", "SUPER + ALT + Q"));
        QVERIFY(keys.error().contains("bad key"));
        QCOMPARE(read(lua), before);
        QCOMPARE(keys.stopKey(), QString("SUPER + ALT + ESCAPE"));
    }

    void keysNeedOmarchysConfig()
    {
        QTemporaryDir dir;
        write(dir.filePath("bindings.lua"), "bind = SUPER, X, exec, foo\n");
        Keybindings keys(dir.filePath("bindings.lua"), fakeHyprctl(dir));
        QVERIFY(!keys.available());
        QVERIFY(!keys.install());
    }

    // QML can't call a method whose parameter types it doesn't know; it fails
    // at the call ("Unknown method parameter type"), not at load.
    void invokablesTakeTypesQmlKnows()
    {
        Settings settings(QDir::tempPath() + "/talkback-unused.conf");
        Voices voices(QDir::tempPath() + "/talkback-no-voices", QDir::tempPath() + "/talkback-no-cache.json");
        Dictation dictation("true");
        Speaker speaker(QDir::tempPath() + "/talkback-no-home");
        Kokoro kokoro;
        Keybindings keybindings(QDir::tempPath() + "/talkback-no-bindings.lua", "false");
        for (const QObject *object : std::initializer_list<const QObject *>{&settings, &voices, &dictation, &speaker, &kokoro, &keybindings}) {
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

    // Stands in for hyprctl: binds come from binds.json, and after a reload
    // configerrors prints errors-after-reload when there is one.
    static QString fakeHyprctl(const QTemporaryDir &dir)
    {
        const QString path = dir.filePath("hyprctl");
        write(path, QString(R"(#!/bin/sh
cd '%1'
case $1 in
  binds) cat binds.json 2>/dev/null || echo '[]' ;;
  reload) touch reloaded ;;
  configerrors) [ -f reloaded ] && cat errors-after-reload 2>/dev/null; echo ;;
esac
)").arg(dir.path()).toUtf8());
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner);
        return path;
    }

    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
};

QTEST_MAIN(TalkbackTests)
#include "talkback_tests.moc"

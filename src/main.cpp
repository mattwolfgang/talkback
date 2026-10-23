// talkback — pick the Piper voice that reads Claude's replies aloud, and turn
// spoken replies and Voxtype dictation on and off.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "cli.h"
#include "dictation.h"
#include "settings.h"
#include "speaker.h"
#include "theme.h"
#include "translator.h"
#include "voices.h"

int main(int argc, char *argv[]) {
    // Commands from the keybindings must not need a display.
    const bool windowed = argc < 2 || QByteArray(argv[1]) == "open";
    if (!windowed)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("talkback");
    app.setApplicationVersion("0.1.0");

    // Matches the window to talkback.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("talkback");
    app.setWindowIcon(QIcon::fromTheme("talkback"));

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    const int code = runCli(app.arguments());
    if (code >= 0)
        return code;

    QQuickStyle::setStyle("Material");

    Theme theme;
    Settings settings;
    Voices voices;
    Dictation dictation;
    Speaker speaker;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("voices", &voices);
    engine.rootContext()->setContextProperty("dictation", &dictation);
    engine.rootContext()->setContextProperty("speaker", &speaker);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    voices.refresh();
    return app.exec();
}

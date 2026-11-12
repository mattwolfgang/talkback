// talkback — pick the Piper or Kokoro voice that reads coding agents' replies
// aloud, and turn spoken replies on and off for each agent.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "cli.h"
#include "keybindings.h"
#include "kokoro.h"
#include "settings.h"
#include "speaker.h"
#include "barplugin.h"
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
    app.setApplicationVersion("0.2.0");

    // Matches the window to talkback.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("talkback");
    app.setWindowIcon(QIcon::fromTheme("talkback"));

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    // A package update brings a new bar icon inside the binary. Whatever runs
    // Talkback first (the window, a key, or the icon itself) brings the copy
    // the Omarchy shell loads up to date; a removed icon stays removed.
    BarPlugin().refresh();

    const int code = runCli(app.arguments());
    if (code >= 0)
        return code;

    // The plain style, so every control can be drawn the way the Omarchy shell
    // draws its own instead of looking like a stock Qt app.
    QQuickStyle::setStyle("Basic");

    Theme theme;
    Settings settings;
    Voices voices;
    Speaker speaker;
    Kokoro kokoro;
    Keybindings keybindings;
    BarPlugin barPlugin;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("voices", &voices);
    engine.rootContext()->setContextProperty("speaker", &speaker);
    engine.rootContext()->setContextProperty("kokoro", &kokoro);
    engine.rootContext()->setContextProperty("keybindings", &keybindings);
    engine.rootContext()->setContextProperty("barPlugin", &barPlugin);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    voices.refresh();
    return app.exec();
}

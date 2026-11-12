QT += core gui qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = talkback
TEMPLATE = app

HEADERS += \
    src/barplugin.h \
    src/cli.h \
    src/hooks.h \
    src/keybindings.h \
    src/kokoro.h \
    src/settings.h \
    src/speaker.h \
    src/theme.h \
    src/translator.h \
    src/voices.h

SOURCES += \
    src/main.cpp \
    src/barplugin.cpp \
    src/cli.cpp \
    src/hooks.cpp \
    src/keybindings.cpp \
    src/kokoro.cpp \
    src/settings.cpp \
    src/speaker.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/voices.cpp

RESOURCES += src/resources.qrc

QT += core gui qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = talkback
TEMPLATE = app

HEADERS += \
    src/cli.h \
    src/dictation.h \
    src/settings.h \
    src/speaker.h \
    src/theme.h \
    src/translator.h \
    src/voices.h

SOURCES += \
    src/main.cpp \
    src/cli.cpp \
    src/dictation.cpp \
    src/settings.cpp \
    src/speaker.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/voices.cpp

RESOURCES += src/resources.qrc

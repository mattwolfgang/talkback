QT += core gui quick quickcontrols2 network testlib
CONFIG += c++17 testcase
TARGET = talkback_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/dictation.h ../src/hooks.h ../src/keybindings.h ../src/kokoro.h ../src/settings.h ../src/speaker.h ../src/theme.h ../src/translator.h ../src/voices.h
SOURCES += talkback_tests.cpp ../src/dictation.cpp ../src/hooks.cpp ../src/keybindings.cpp ../src/kokoro.cpp ../src/settings.cpp ../src/speaker.cpp ../src/theme.cpp \
    ../src/translator.cpp ../src/voices.cpp

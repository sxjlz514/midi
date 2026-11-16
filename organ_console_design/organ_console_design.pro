QT += quick serialport

CONFIG += c++17

TARGET = organ_console_design
TEMPLATE = app

SOURCES += \
    main.cpp \
    backend/consolebridge.cpp \
    backend/serialclient.cpp \
    backend/keymapping.cpp \
    backend/keystateaggregator.cpp \
    backend/midiparser.cpp \
    backend/miditoocf.cpp \
    backend/ocffile.cpp \
    backend/ocfwriter.cpp \
    backend/ocftomidi.cpp \
    backend/midiplayer.cpp \
    backend/midiinput.cpp \
    backend/midirecorder.cpp

HEADERS += \
    backend/consolebridge.h \
    backend/serialclient.h \
    backend/keymapping.h \
    backend/keystateaggregator.h \
    backend/midiparser.h \
    backend/miditoocf.h \
    backend/ocffile.h \
    backend/ocfwriter.h \
    backend/ocftomidi.h \
    backend/midiplayer.h \
    backend/midiinput.h \
    backend/midirecorder.h

RESOURCES += qml.qrc

# Windows 多媒体 MIDI 输入（winmm）
win32: LIBS += -lwinmm

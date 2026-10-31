# organ_console.pro
# LubanCat Linux 版本：Qt 5.15，ARM64。
QT       += core gui serialport widgets quick qml quickcontrols2
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TEMPLATE = app
TARGET   = organ_console_linux

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    serialclient.cpp \
    keymapping.cpp \
    keystateaggregator.cpp \
    midiparser.cpp \
    ocffile.cpp \
    midiplayer.cpp \
    midiinput.cpp

HEADERS += \
    mainwindow.h \
    serialclient.h \
    keymapping.h \
    keystateaggregator.h \
    midiparser.h \
    ocffile.h \
    midiplayer.h \
    midiinput.h

RESOURCES += \
    tools.qrc \
    qml.qrc

# Windows 多媒体 MIDI 输入（winmm）
win32: LIBS += -lwinmm

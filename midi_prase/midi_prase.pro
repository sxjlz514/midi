# midi_prase.pro — MIDI 解析 + 串口自动演奏（16 路）
QT       += core gui serialport widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += c++17
TEMPLATE = app
TARGET   = midi_prase

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    keymapping.cpp \
    midiparser.cpp \
    midiplayer.cpp

HEADERS += \
    mainwindow.h \
    keymapping.h \
    midiparser.h \
    midiplayer.h

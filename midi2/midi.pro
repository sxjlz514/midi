QT += widgets serialport

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    devicebar.cpp \
    midifiledecoder.cpp \
    midifileplayer.cpp \
    midikeyrouter.cpp \
    midimessageformatter.cpp \
    midiinputpanel.cpp \
    midirecorder.cpp \
    midiinput.cpp \
    serialpanel.cpp

HEADERS += \
    mainwindow.h \
    devicebar.h \
    midifiledecoder.h \
    midifileplayer.h \
    midikeyrouter.h \
    midimessageformatter.h \
    midiinputpanel.h \
    midirecorder.h \
    midiinput.h \
    serialcommand.h \
    serialpanel.h

win32:LIBS += -lwinmm

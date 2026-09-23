QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    midifiledecoder.cpp \
    midimessageformatter.cpp \
    midiinputpanel.cpp \
    midirecorder.cpp \
    midiinput.cpp

HEADERS += \
    mainwindow.h \
    midifiledecoder.h \
    midimessageformatter.h \
    midiinputpanel.h \
    midirecorder.h \
    midiinput.h

win32:LIBS += -lwinmm

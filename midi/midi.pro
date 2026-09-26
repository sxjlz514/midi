QT += widgets serialport

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    midiinput.cpp \
    appcontroller.cpp \
    notemapper.cpp \
    organstate.cpp \
    frame.cpp \
    seriallink.cpp \
    valvepanel.cpp

HEADERS += \
    mainwindow.h \
    midiinput.h \
    midievent.h \
    appcontroller.h \
    notemapper.h \
    organstate.h \
    command.h \
    frame.h \
    seriallink.h \
    valvepanel.h

win32:LIBS += -lwinmm

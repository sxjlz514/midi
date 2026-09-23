QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    midiinput.cpp

HEADERS += \
    mainwindow.h \
    midiinput.h

win32:LIBS += -lwinmm

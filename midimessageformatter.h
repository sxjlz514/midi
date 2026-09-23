#pragma once

#include <QString>
#include <QtGlobal>

class MidiMessageFormatter
{
public:
    static QString noteName(int midiNote);

    static QString describe(
        quint8 status,
        quint8 data1,
        quint8 data2
        );

    static QString formatLine(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );
};

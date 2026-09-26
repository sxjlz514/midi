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

    /* 简洁音符行：只保留 时间 + 音符 + ON/OFF，
     * 用于解析 .mid 文件后的显示 */
    static QString formatNoteLine(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );
};

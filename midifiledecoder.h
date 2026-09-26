#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct MidiFileEvent
{
    quint32 tick = 0;
    quint32 timestampMs = 0;
    quint8 status = 0;
    quint8 data1 = 0;
    quint8 data2 = 0;
};

struct MidiFileParseResult
{
    bool ok = false;
    QString error;

    quint16 format = 0;
    quint16 trackCount = 0;
    quint16 division = 0;

    QVector<MidiFileEvent> events;
};

class MidiFileDecoder
{
public:
    static MidiFileParseResult parse(
        const QString &filePath
        );

    static QStringList decodeToLines(
        const QString &filePath,
        QString *errorMessage = nullptr
        );
};

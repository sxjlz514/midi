/**
 * @file ocffile.h
 * @brief OCF v1 binary 32-bit bitmap stream reader and human-readable dump.
 */

#ifndef OCFFILE_H
#define OCFFILE_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct OcfFrame {
    quint64 timeMs = 0;
    quint32 bitmap = 0;
};

struct OcfData {
    quint8 version = 1;
    quint8 bitmapBits = 32;
    quint8 mappingMode = 0; // 0=range filter, 1=wrap
    quint8 rangeStart = 60;
    quint8 rangeEnd = 91;
    quint16 aggregationWindowMs = 30;
    quint64 durationMs = 0;
    QVector<OcfFrame> frames;
};

class OcfFile
{
public:
    static bool readFile(const QString &path, OcfData &out, QString &error);
    static bool parse(const QByteArray &bytes, OcfData &out, QString &error);
    static QString toText(const OcfData &data, const QString &sourceName,
                          bool crcValid = true);
};

#endif // OCFFILE_H

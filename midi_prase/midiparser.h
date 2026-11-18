/**
 * @file midiparser.h
 * @brief 标准 MIDI 文件 (SMF Format 0/1) 解析 → 绝对时间 Note 事件列表
 */

#ifndef MIDIPARSER_H
#define MIDIPARSER_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>

enum class MidiEventType {
    NoteOn,
    NoteOff
};

struct MidiNoteEvent {
    qint64 timeMs = 0;          /**< 相对曲目起点的毫秒 */
    MidiEventType type = MidiEventType::NoteOff;
    int midiNote = 0;           /**< 0..127 */
    int velocity = 0;
    int midiChannel = 0;        /**< MIDI 通道 0..15（文件内），非风琴通道 */
};

struct MidiParseResult {
    bool ok = false;
    QString error;
    int format = 0;
    int trackCount = 0;
    int ticksPerQuarter = 480;
    qint64 durationMs = 0;
    QVector<MidiNoteEvent> events; /**< 已按 timeMs 排序 */
};

class MidiParser
{
public:
    /** 从文件路径解析 */
    static MidiParseResult parseFile(const QString &path);

    /** 从内存数据解析 */
    static MidiParseResult parseData(const QByteArray &data);

private:
    struct TempoPoint {
        qint64 absTick = 0;
        int usPerQuarter = 500000; /**< 默认 120 BPM */
    };

    struct RawEvent {
        qint64 absTick = 0;
        MidiEventType type = MidiEventType::NoteOff;
        int midiNote = 0;
        int velocity = 0;
        int midiChannel = 0;
    };

    static bool readHeader(const QByteArray &data, int &pos,
                           int &format, int &ntrks, int &division,
                           QString &err);
    static bool readTrack(const QByteArray &data, int &pos,
                          QVector<RawEvent> &outEvents,
                          QVector<TempoPoint> &tempos,
                          QString &err);
    static quint32 readVarLen(const QByteArray &data, int &pos, QString &err);
    static qint64 ticksToMs(qint64 absTick,
                            int ticksPerQuarter,
                            const QVector<TempoPoint> &tempos);
};

#endif // MIDIPARSER_H

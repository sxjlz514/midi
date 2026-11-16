/**
 * @file ocftomidi.h
 * @brief 位图帧 → 标准 MIDI（力度统一填固定值）。
 *
 * 音高由映射表反查：通道 n → mapping.midiNoteForChannel(n)。
 * 时间戳直接取帧时间（位图变化点）。
 */

#ifndef OCFTOMIDI_H
#define OCFTOMIDI_H

#include "keymapping.h"
#include "ocffile.h"

#include <QString>
#include <QVector>

class OcfToMidi
{
public:
    /**
     * @param frames      已按时间升序的位图帧
     * @param durationMs  曲目时长
     * @param mapping     当前映射（用于反查音高）
     * @param velocity    NoteOn 力度（位图无力度信息）
     */
    static bool exportMidi(const QString &path, const QVector<OcfFrame> &frames,
                           qint64 durationMs, const KeyMapping &mapping,
                           int velocity, QString &error);

    /** 只组包，便于测试。 */
    static QByteArray encode(const QVector<OcfFrame> &frames, qint64 durationMs,
                             const KeyMapping &mapping, int velocity);
};

#endif // OCFTOMIDI_H

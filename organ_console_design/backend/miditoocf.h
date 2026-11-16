/**
 * @file miditoocf.h
 * @brief MIDI → OCF 转换（逐条复刻后端 tools/midi_to_ocf.py 的算法）。
 *
 * 算法要点：
 *  - slot = note - rangeStart（range 模式），范围外忽略
 *  - slot = (note - rangeStart) % span（wrap 模式）
 *  - 用 (midiChannel, note) 计数处理重叠/同音
 *  - 位图变化后等待 windowMs 静默才落一帧（帧时间为事件时间 + 窗口）
 *  - 结束仍有按住的音时补清零帧，并保证最后一帧位图为 0
 */

#ifndef MIDITOOCF_H
#define MIDITOOCF_H

#include "midiparser.h"
#include "ocffile.h"

#include <QString>
#include <QVector>

class MidiToOcf
{
public:
    struct Options {
        bool wrap = false;   /**< false=range（范围外忽略），true=wrap（按跨度循环） */
        int rangeStart = 60;
        int rangeEnd = 91;
        int windowMs = 30;
    };

    /**
     * @brief 把已解析的 MIDI 事件折成 OCF 位图流。
     * @param events  MidiParser 输出（按时间排序、同刻 OFF 在 ON 之前）
     * @param durationMs 源文件时长（最后事件时间）
     */
    static bool convert(const QVector<MidiNoteEvent> &events, qint64 durationMs,
                        const Options &options, OcfData &out, QString &error);
};

#endif // MIDITOOCF_H

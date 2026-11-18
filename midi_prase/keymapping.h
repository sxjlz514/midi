/**
 * @file keymapping.h
 * @brief 音高(MIDI Note) ↔ 通道 1..16 映射表
 *
 * 默认：连续半音，通道1 = C4(60) … 通道16 = D#5(75)
 * 与下位机键号一致：1–8=N1，9–16=N2
 */

#ifndef KEYMAPPING_H
#define KEYMAPPING_H

#include <QString>
#include <QVector>
#include <optional>

struct PitchMapEntry {
    int channel = 0;      /**< 1..16，对应串口 ON/OFF n */
    int midiNote = 0;     /**< 0..127 */
    QString noteName;     /**< 如 C4、C#4 */
};

class KeyMapping
{
public:
    static constexpr int kChannelCount = 16;
    /** 默认起点：C4 = MIDI 60 */
    static constexpr int kDefaultBaseNote = 60;

    KeyMapping();

    /** 恢复默认：通道 i → midiNote = baseNote + (i-1) */
    void resetToDefault(int baseNote = kDefaultBaseNote);

    /** 通道 1..16 → MIDI 音高；非法通道返回 -1 */
    int midiNoteForChannel(int channel) const;

    /** MIDI 音高 → 通道 1..16；未映射返回 nullopt */
    std::optional<int> channelForMidiNote(int midiNote) const;

    /** 设置某通道的 MIDI 音高（0..127），并刷新 noteName */
    bool setChannelMidiNote(int channel, int midiNote);

    const QVector<PitchMapEntry> &entries() const { return m_entries; }

    static QString midiNoteToName(int midiNote);

private:
    QVector<PitchMapEntry> m_entries; /**< index 0 → 通道 1 */
};

#endif // KEYMAPPING_H

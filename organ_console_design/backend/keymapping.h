/**
 * @file keymapping.h
 * @brief 音高(MIDI Note 0..127) ↔ 风琴通道 1..n 映射表，n 为 1..32。
 *
 * 默认 16 路连续半音，可在界面改为不超过 32 的任意键数。
 * 从板划分：1–8=N1，9–16=N2，17–24=N3，25–32=N4。
 * 实时 MIDI 与自动演奏共用同一张表。
 */

#ifndef KEYMAPPING_H
#define KEYMAPPING_H

#include <QString>
#include <QVector>
#include <optional>

/**
 * 一行映射：一个风琴通道对应一个 MIDI 音高。
 * @var channel  通道号 1..n，对应位图 bit(n-1)
 * @var midiNote MIDI 音符号 0..127
 * @var noteName 显示用音名，如 "C4"
 */
struct PitchMapEntry {
    int channel = 0;
    int midiNote = 0;
    QString noteName;
};

class KeyMapping
{
public:
    /** 位图与 CAN 四字节能容纳的最大通道数。 */
    static constexpr int kMaxChannelCount = 32;
    /** 默认映射起点：中央 C 上一组 C4 = MIDI 48。 */
    static constexpr int kDefaultBaseNote = 48;

    KeyMapping();

    int channelCount() const { return m_channelCount; }

    /**
     * 调整键位数。已有通道的音高保留；新增通道尽量沿用默认半音阶。
     * @return 越界或没有空闲音高时 false
     */
    bool setChannelCount(int count);

    /** 通道所在从板名；超出 32 返回空。 */
    static QString boardNameForChannel(int channel);

    /**
     * 恢复默认：通道 i 对应 midiNote = baseNote + (i-1)。
     * @param baseNote 通道1 的 MIDI 音高
     */
    void resetToDefault(int baseNote = kDefaultBaseNote);

    /** 通道 → MIDI 音高；非法通道返回 -1。 */
    int midiNoteForChannel(int channel) const;

    /** MIDI 音高 → 通道；未映射返回 nullopt。 */
    std::optional<int> channelForMidiNote(int midiNote) const;

    /**
     * 设置某通道的 MIDI 音高并更新音名。
     * @return 通道或音高越界时 false
     */
    bool setChannelMidiNote(int channel, int midiNote);

    /** 只读访问整表（表格刷新用）。 */
    const QVector<PitchMapEntry> &entries() const { return m_entries; }

    /** MIDI 音高转音名（C-1..G9 惯例，60=C4）。 */
    static QString midiNoteToName(int midiNote);

private:
    QVector<PitchMapEntry> m_entries; /**< index 0 → 通道 1 */
    int m_channelCount = 16;
};

#endif // KEYMAPPING_H

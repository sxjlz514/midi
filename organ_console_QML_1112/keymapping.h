/**
 * @file keymapping.h
 * @brief 音高(MIDI Note 0..127) ↔ 风琴通道 1..N 映射表，N 可在 1..32 之间配置。
 *
 * 默认连续半音：通道1 = C4(60) … 通道N = 59+N。
 * 与下位机键号一致：每 8 键一块从板，1–8 = N1，9–16 = N2，17–24 = N3，25–32 = N4。
 * 实时 MIDI、自动演奏与录制共用同一张表。
 */

#ifndef KEYMAPPING_H
#define KEYMAPPING_H

#include <QString>
#include <QVector>
#include <optional>

/**
 * 一行映射：一个风琴通道对应一个 MIDI 音高。
 * @var channel  通道号 1..N，对应串口位图的 bit(channel-1)
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
    /** 协议位图宽度，也是最大通道数。 */
    static constexpr int kMaxChannels = 32;
    /** 默认映射起点：中央 C（C4）= MIDI 60。 */
    static constexpr int kDefaultBaseNote = 60;
    /** 默认键位数（保持与旧硬件两从板 × 8 键兼容）。 */
    static constexpr int kDefaultChannelCount = 16;

    explicit KeyMapping(int channelCount = kDefaultChannelCount);

    /** 当前配置的键位数 1..32。 */
    int channelCount() const { return static_cast<int>(m_entries.size()); }

    /**
     * 恢复默认：通道 i 对应 midiNote = baseNote + (i-1)。
     * @param baseNote 通道1 的 MIDI 音高
     * @param count    键位数；<0 表示沿用当前数量
     */
    void resetToDefault(int baseNote = kDefaultBaseNote, int count = -1);

    /**
     * @brief 修改键位数，保留已有通道映射。
     *
     * 新增通道会在 0..127 内寻找一段连续空闲半音一次性补齐；
     * 找不到足够的连续空闲音高时返回 false，原表保持不变。
     * 缩小时直接截断高位通道。
     */
    bool setChannelCount(int count);

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
};

#endif // KEYMAPPING_H

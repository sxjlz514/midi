#pragma once

// =============================================================================
//  NoteMapper.h —— 音符 -> 电磁阀 的映射
// -----------------------------------------------------------------------------
//  规则：一一对应（一个音符对应一个阀，不叠加）。
//
//      阀号 valveIndex = 音符 note - baseNote
//      boardId = valveIndex / 16        （第几个从控板，0~2）
//      channel = valveIndex % 16        （板内第几路，0~15）
//
//  baseNote 是“起始音符”，默认 36（MIDI C2）：音符 36 -> 阀 0，音符 83 -> 阀 47。
//  超出范围（0~47 之外）的音符忽略。
// =============================================================================

class NoteMapper
{
public:
    explicit NoteMapper(int baseNote = 36);

    // 返回音符对应的阀号（0~47）；不在范围内返回 -1。
    int noteToValve(int note) const;

    // 由阀号反推从控板编号（0~2）与板内通道（0~15）。
    static int valveToBoard(int valveIndex);
    static int valveToChannel(int valveIndex);

    int baseNote() const { return m_baseNote; }
    void setBaseNote(int baseNote) { m_baseNote = baseNote; }

private:
    int m_baseNote; // 起始音符
};

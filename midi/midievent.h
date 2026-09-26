#pragma once

#include <QMetaType>
#include <QtGlobal>

// =============================================================================
//  MidiEvent.h —— MIDI 事件的数据结构
// -----------------------------------------------------------------------------
//  MIDI 键盘传来的不是“声音”，而是“动作”：哪个键按下/松开、力度多大。
//  MidiInput 收到原始字节后，会翻译成本结构体再通过信号发出去，
//  上层（AppController）只需要关心“音符”“力度”，不用再解析字节。
// =============================================================================

enum class MidiEventType
{
    NoteOn,          // 按下琴键
    NoteOff,         // 松开琴键
    ControlChange,   // 控制变化（踏板、音栓开关等）
    Other,           // 其他暂不处理的消息
};

struct MidiEvent
{
    MidiEventType type = MidiEventType::Other;
    quint8 status = 0;     // 原始状态字节，如 0x90
    int channel = 1;       // MIDI 通道，1~16
    int note = 0;          // 音符号，0~127（60 = 中央 C）
    int velocity = 0;      // 力度，0~127
    int controller = 0;    // 仅 ControlChange 有效：控制器编号
    int value = 0;         // 仅 ControlChange 有效：控制器值
    quint32 timestampMs = 0; // 时间戳（毫秒）
};

// 注册到 Qt 元对象系统，便于跨线程信号传递。
Q_DECLARE_METATYPE(MidiEvent)

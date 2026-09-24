#pragma once

#include <QObject>
#include <QStringList>
#include <QtGlobal>

#include "midievent.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmsystem.h>

// =============================================================================
//  MidiInput.h —— MIDI 键盘输入（Windows WinMM 实现）
// -----------------------------------------------------------------------------
//  职责：
//    1. 枚举系统上的 MIDI 输入设备（USB MIDI 键盘/虚拟端口都会出现）
//    2. 打开 / 关闭指定设备
//    3. 在后台接收 MIDI 消息，翻译成 MidiEvent 后通过信号发出
//
//  线程说明：
//    WinMM 回调不在 Qt GUI 线程。回调里只做一件事——用
//    QMetaObject::invokeMethod(..., Qt::QueuedConnection) 把数据投递回 Qt 线程，
//    然后在 Qt 线程里 emit 信号。绝不能直接在回调里做耗时操作。
// =============================================================================

class MidiInput : public QObject
{
    Q_OBJECT

public:
    explicit MidiInput(QObject *parent = nullptr);
    ~MidiInput() override;

    // 返回可用 MIDI 输入设备名列表。
    QStringList devices() const;

    // 打开第 deviceIndex 个 MIDI 输入设备。失败时通过 errorMessage 返回原因。
    bool open(unsigned int deviceIndex, QString *errorMessage = nullptr);

    void close();
    bool isOpen() const;

signals:
    // 翻译后的结构化事件（上层主要用它）。
    void midiEvent(const MidiEvent &event);
    // 原始字节（调试/日志用）。
    void rawMessageReceived(quint8 status, quint8 data1, quint8 data2, quint32 timestampMs);
    // 供界面显示的日志。
    void logMessage(const QString &text);

private:
    static void CALLBACK midiCallback(
        HMIDIIN midiHandle,
        UINT message,
        DWORD_PTR instance,
        DWORD_PTR parameter1,
        DWORD_PTR parameter2);

    static QString midiErrorText(MMRESULT result);

    // 把 WinMM 的原始字节翻译成 MidiEvent。
    static MidiEvent parseMessage(quint8 status, quint8 data1, quint8 data2, quint32 timestampMs);

private:
    HMIDIIN m_handle = nullptr;
};

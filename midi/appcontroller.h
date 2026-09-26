#pragma once

#include <QByteArray>
#include <QObject>

#include "command.h"
#include "midievent.h"
#include "notemapper.h"
#include "organstate.h"

class MidiInput;
class SerialLink;

// =============================================================================
//  AppController.h —— 应用逻辑的总调度
// -----------------------------------------------------------------------------
//  把各模块“粘”成完整数据流：
//
//    MIDI 键盘 --(midiEvent)--> AppController --更新--> OrganState
//                                    |                     |
//                                    |                     +--(snapshot)--> 6 字节位图
//                                    v
//                            SerialLink --(SET_STATE 帧)--> STM32 主控
//
//  界面（MainWindow）只跟 AppController 打交道：
//    调用它的方法（连接串口、测试某路阀……），监听它的信号（状态、日志）刷新界面。
// =============================================================================

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);

    SerialLink *serial() const { return m_serial; }
    MidiInput *midi() const { return m_midi; }

    // --- 界面调用的操作 ---
    void connectSerial(const QString &portName);
    void disconnectSerial();
    void setSimulationMode(bool on);
    bool isSimulationMode() const;

    void testValve(int valveIndex, bool on); // 手动开关某一路（调试）
    void allOff();                           // 全部关闭

    int baseNote() const { return m_mapper.baseNote(); }
    void setBaseNote(int baseNote);

signals:
    void valveStateChanged(const QByteArray &bitmap); // 状态位图变化
    void logMessage(const QString &text);             // 日志

private slots:
    void handleMidiEvent(const MidiEvent &event);
    void handleFrameReceived(Command cmd, const QByteArray &payload);

private:
    void pushState(); // 同步状态：刷新界面 + 发送给主控

    OrganState m_state;   // 48 路状态
    NoteMapper m_mapper;  // 音符 -> 阀

    SerialLink *m_serial = nullptr; // 子对象，随本类销毁
    MidiInput *m_midi = nullptr;    // 子对象，随本类销毁
};

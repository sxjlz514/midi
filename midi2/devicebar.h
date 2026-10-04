#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

class MidiInput;
class SerialCommand;

/*
 * 窗口顶部的公共设备栏：MIDI 设备选择 + 串口选择。
 *
 * 只持有 MidiInput / SerialCommand 的指针，对象由 MainWindow 拥有。
 * 两个页签下的所有消息都来自这里的同一个 MIDI 输入。
 */
class DeviceBar : public QWidget
{
    Q_OBJECT

public:
    DeviceBar(
        MidiInput *midi,
        SerialCommand *serial,
        QWidget *parent = nullptr
        );

    void refreshMidiDevices();

    void refreshSerialPorts();

signals:
    void allOffRequested();

private slots:
    void toggleMidiDevice();

    void toggleSerialPort();

    void queryStatus();

    void onSerialConnectedChanged(bool connected);

    void onSerialError(const QString &message);

private:
    QWidget *createMidiRow();

    QWidget *createSerialRow();

    MidiInput *m_midi = nullptr;
    SerialCommand *m_serial = nullptr;

    QComboBox *m_midiCombo = nullptr;
    QPushButton *m_midiRefreshButton = nullptr;
    QPushButton *m_midiOpenButton = nullptr;
    QLabel *m_midiStatusLabel = nullptr;

    QComboBox *m_serialCombo = nullptr;
    QPushButton *m_serialRefreshButton = nullptr;
    QPushButton *m_serialOpenButton = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_queryButton = nullptr;
    QPushButton *m_allOffButton = nullptr;
    QLabel *m_serialStatusLabel = nullptr;
};

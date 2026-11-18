/**
 * @file mainwindow.h
 * @brief MIDI 解析 + 映射表 + 串口自动演奏
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "keymapping.h"
#include "midiplayer.h"

#include <QMainWindow>
#include <QSerialPort>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QTableWidget;
class QLineEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshPortList();
    void toggleSerialPort();
    void onSerialReadyRead();
    void onSerialErrorOccurred(QSerialPort::SerialPortError error);

    void openMidiFile();
    void onPlay();
    void onPause();
    void onStop();
    void onAllOff();
    void onResetMapping();
    void onMappingCellChanged(int row, int column);
    void onTempoChanged(int value);

    void onChannelCommand(int channel, bool on);
    void onUnmappedNote(int midiNote, bool on);
    void onPlayerFinished();
    void onPositionChanged(qint64 ms);

private:
    void setupUi();
    void rebuildMappingTable();
    void updatePlayUi();
    bool sendCommandLine(const QString &line);
    void sendChannelState(int channel, bool on);
    void appendLog(const QString &text);
    void updateUiConnectedState(bool connected);
    static QString formatMs(qint64 ms);

    QSerialPort *m_serial = nullptr;
    KeyMapping m_mapping;
    MidiPlayer *m_player = nullptr;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    QLineEdit *m_fileEdit = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_pauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_allOffBtn = nullptr;
    QLabel *m_timeLabel = nullptr;
    QSlider *m_tempoSlider = nullptr;
    QLabel *m_tempoLabel = nullptr;

    QTableWidget *m_mapTable = nullptr;
    QPushButton *m_resetMapBtn = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    bool m_blockMapEdit = false;
    qint64 m_durationMs = 0;
};

#endif // MAINWINDOW_H

#pragma once

#include <QMainWindow>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class MidiInput;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshDevices();
    void toggleMidiDevice();

    void onMidiMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private:
    static QString noteName(int midiNote);

private:
    MidiInput *m_midiInput = nullptr;

    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    quint64 m_messageCount = 0;
};

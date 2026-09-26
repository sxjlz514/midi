#pragma once

#include <QElapsedTimer>
#include <QMainWindow>

class QPushButton;
class QTabWidget;

class MidiInputPanel;
class MidiRecorder;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void toggleRecording();
    void openFile();

    void onRecordMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private:
    QTabWidget *m_tabs = nullptr;

    MidiInputPanel *m_inputPanel = nullptr;
    MidiInputPanel *m_recordPanel = nullptr;

    QPushButton *m_recordButton = nullptr;
    QPushButton *m_openFileButton = nullptr;

    MidiRecorder *m_recorder = nullptr;
    QElapsedTimer m_recordTimer;
};

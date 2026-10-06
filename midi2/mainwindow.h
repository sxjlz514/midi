#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QtGlobal>

class QCheckBox;
class QCloseEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QTabWidget;

class DeviceBar;
class MidiFilePlayer;
class MidiInput;
class MidiInputPanel;
class MidiKeyRouter;
class MidiRecorder;
class SerialCommand;
class SerialPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleRecording();
    void openFile();

    void onLiveMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

    void onFileEvent(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

    void onPlayRequested();
    void onPauseRequested();
    void onStopRequested();
    void onPlaybackFinished();
    void onPlaybackProgress(
        quint32 currentMs,
        quint32 totalMs
        );
    void onPlaybackStateChanged();

    void onRouterCommandSent(const QString &line);
    void onRouterMessageIgnored(const QString &reason);
    void onBoardStatusReceived(quint32 bits);

    void updateForwardLabels();
    void updatePlaybackWidgets();
    void updateBoardStatusLabel();

private:
    QWidget *createLivePage();
    QWidget *createRecordPage();

    /* 当前正在转发的是哪一路（手动面板操作为 SourceNone） */
    enum ForwardSource {
        SourceNone = 0,
        SourceLive = 1,
        SourceFile = 2
    };

    MidiInput *m_midi = nullptr;
    SerialCommand *m_serial = nullptr;
    MidiKeyRouter *m_router = nullptr;
    MidiFilePlayer *m_player = nullptr;

    QTabWidget *m_tabs = nullptr;
    DeviceBar *m_deviceBar = nullptr;
    MidiInputPanel *m_livePanel = nullptr;
    MidiInputPanel *m_recordPanel = nullptr;
    SerialPanel *m_serialPanel = nullptr;

    QCheckBox *m_forwardLiveCheck = nullptr;
    QLabel *m_forwardLiveLabel = nullptr;
    QCheckBox *m_forwardFileCheck = nullptr;
    QLabel *m_forwardFileLabel = nullptr;

    QPushButton *m_recordButton = nullptr;
    QPushButton *m_openFileButton = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QLabel *m_playFileLabel = nullptr;
    QProgressBar *m_playProgress = nullptr;

    QLabel *m_boardStatusLabel = nullptr;

    MidiRecorder *m_recorder = nullptr;
    QElapsedTimer m_recordTimer;

    int m_liveSentCount = 0;
    int m_liveIgnoredCount = 0;
    int m_fileSentCount = 0;
    int m_fileIgnoredCount = 0;

    ForwardSource m_forwardSource = SourceNone;
};

/**
 * @file mainwindow.h
 * @brief 主窗口：三模式切换 + 共享串口/映射表 + 调试回报开关。
 *
 * 模式：
 *   0 实时控制 — 外接 MIDI 设备 NoteOn/Off → 映射 → ON/OFF
 *   1 自动演奏 — 导入 SMF → MidiPlayer → ON/OFF
 *   2 直接控制 — 鼠标/电脑键盘 → ON/OFF（原 serial_test）
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "keymapping.h"
#include "keystateaggregator.h"
#include "midiinput.h"
#include "midiparser.h"
#include "midirecorder.h"
#include "midiplayer.h"
#include "ocffile.h"
#include "serialclient.h"

#include <QMainWindow>
#include <QTimer>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QProcess;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QKeyEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_PROPERTY(bool serialConnected READ serialConnected NOTIFY serialStateChanged)
    Q_PROPERTY(QString serialStatus READ serialStatus NOTIFY serialStateChanged)
    Q_PROPERTY(QString linkedPort READ linkedPort NOTIFY serialStateChanged)
    Q_PROPERTY(int baudRate READ baudRate NOTIFY serialStateChanged)
    Q_PROPERTY(QStringList serialPortNames READ serialPortNames NOTIFY serialPortsChanged)
    Q_PROPERTY(QStringList midiInputDevices READ midiInputDevices NOTIFY midiInputStateChanged)
    Q_PROPERTY(bool midiInputOpen READ midiInputOpen NOTIFY midiInputStateChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY fileStateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY playbackPositionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY fileStateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackStateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY playbackStateChanged)
    Q_PROPERTY(int tempoPercent READ tempoPercent NOTIFY playbackStateChanged)
    Q_PROPERTY(QStringList logLines READ logLines NOTIFY logLinesChanged)
    Q_PROPERTY(QVariantList mappingRows READ mappingRows NOTIFY mappingRowsChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY channelCountChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int recordWindowMs READ recordWindowMs NOTIFY recordingChanged)
    Q_PROPERTY(int recordDebounceMs READ recordDebounceMs NOTIFY recordingChanged)
    Q_PROPERTY(bool converting READ converting NOTIFY conversionStateChanged)
    Q_PROPERTY(QVariantList waterfallNotes READ waterfallNotes NOTIFY waterfallNotesChanged)
    Q_PROPERTY(bool virtualPlayback READ virtualPlayback WRITE setVirtualPlayback NOTIFY virtualPlaybackChanged)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool serialConnected() const;
    QString serialStatus() const;
    QString linkedPort() const { return m_linkPort; }
    int baudRate() const { return m_baudRate; }
    QStringList serialPortNames() const;
    QStringList midiInputDevices() const;
    bool midiInputOpen() const;
    QString currentFile() const;
    qint64 positionMs() const { return m_positionMs; }
    qint64 durationMs() const { return m_durationMs; }
    bool playing() const;
    bool paused() const;
    int tempoPercent() const { return m_tempoPercent; }
    QStringList logLines() const { return m_logs; }
    QVariantList mappingRows() const;
    int channelCount() const { return m_mapping.channelCount(); }
    bool recording() const { return m_recorder.isRecording(); }
    int recordWindowMs() const { return m_recorder.windowMs(); }
    int recordDebounceMs() const { return m_recorder.debounceMs(); }
    bool converting() const { return m_converterProcess != nullptr; }
    QVariantList waterfallNotes() const { return m_waterfallNotes; }
    bool virtualPlayback() const { return m_virtualPlayback; }

    /** 瀑布流用：播放中返回播放器实时位置，避免仅依赖节拍通知。 */
    Q_INVOKABLE qint64 livePositionMs() const;
    Q_INVOKABLE void setVirtualPlayback(bool enabled);

    Q_INVOKABLE void refreshSerialPorts();
    Q_INVOKABLE void toggleSerial(const QString &portName, int baudRate);
    Q_INVOKABLE void setRemoteDebug(bool enabled);
    Q_INVOKABLE void setBaudRate(int baudRate);
    Q_INVOKABLE void refreshMidiInputs();
    Q_INVOKABLE void toggleMidiInput(int deviceIndex);
    Q_INVOKABLE void chooseControlFile();
    Q_INVOKABLE void convertMidiFile(const QString &mappingMode, int rangeStart,
                                     int rangeEnd, int windowMs);
    Q_INVOKABLE QString readOcfText(const QString &path);
    Q_INVOKABLE void chooseOcfToInspect();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setTempo(int percent);
    Q_INVOKABLE void pressChannel(int channel);
    Q_INVOKABLE void releaseChannel(int channel);
    Q_INVOKABLE void allOff();
    Q_INVOKABLE QString setMappingNote(int channel, int midiNote);
    Q_INVOKABLE void resetMapping();
    Q_INVOKABLE void activateMode(int modeIndex);
    /** 将键位数改为 1..32。失败时返回原因，成功返回空字符串。 */
    Q_INVOKABLE QString setChannelCount(int count);
    Q_INVOKABLE void setRecordWindowMs(int ms);
    Q_INVOKABLE void setRecordDebounceMs(int ms);
    /**
     * 开始录制，或停止并把结果存成 MIDI。
     * 返回给界面提示的一句话。
     */
    Q_INVOKABLE QString toggleRecording();

signals:
    void serialStateChanged();
    void serialPortsChanged();
    void midiInputStateChanged();
    void fileStateChanged();
    void playbackStateChanged();
    void playbackPositionChanged();
    void logLinesChanged();
    void mappingRowsChanged();
    void channelCountChanged();
    void recordingChanged();
    void conversionStateChanged();
    void conversionFinished(bool success, const QString &path, const QString &error);
    void waterfallNotesChanged();
    void virtualPlaybackChanged();

protected:
    /** 仅「直接控制」模式：键盘按下 → ON（忽略 auto-repeat）。 */
    void keyPressEvent(QKeyEvent *event) override;
    /** 仅「直接控制」模式：键盘松开 → OFF。 */
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void refreshPortList();
    void scanForBoard();
    void onProbeFinished(const QString &portName, bool matched);
    void toggleSerialPort();
    void onSerialBytes(const QString &text);
    void onSerialError(const QString &text);
    void onSerialConnectionChanged(bool connected);
    void onDebugToggled(bool enabled);
    void onModeTabChanged(int index);

    void refreshMidiDevices();
    void toggleMidiDevice();
    void onLiveNoteOn(int midiNote, int velocity);
    void onLiveNoteOff(int midiNote, int velocity);

    void openMidiFile();
    void convertMidiToOcf();
    void inspectOcf();
    void onPlay();
    void onPause();
    void onStop();
    void onTempoChanged(int sliderValue);
    void onChannelCommand(int channel, bool on);
    void onUnmappedNote(int midiNote, bool on);
    void onPlayerFinished();
    void onPositionChanged(qint64 ms);
    void onBitmapCommand(quint32 bitmap);

    void onDirectPressed(int channel);
    void onDirectReleased(int channel);
    void onAllOffClicked();
    void onResetMapping();
    void onMappingCellChanged(int row, int column);
    /** 聚合器发出 ST 快照时写日志（仅调试开）。 */
    void onSnapshotSent(quint32 bitmap, quint16 seq);

private:
    void setupUi();
    QWidget *buildRealtimePage();
    QWidget *buildAutoPlayPage();
    QWidget *buildDirectPage();
    void rebuildMappingTable();
    void rebuildWaterfallNotes();
    bool loadControlFile(const QString &path);
    /** 设置文件路径：优先程序目录下 setting.txt，写不了则用用户配置目录。 */
    QString settingsFilePath(bool forWrite) const;
    void loadSettings();
    void saveSettings() const;
    void startOcfConversion(const QString &inputPath, const QString &mappingMode,
                            int rangeStart, int rangeEnd, int windowMs);
    void updatePlayUi();
    void updateLiveUi();
    void highlightChannel(int channel, bool on);
    bool sendChannel(int channel, bool on);
    void appendLog(const QString &text);
    int keyToChannel(int key) const;
    static QString formatMs(qint64 ms);

    SerialClient *m_serial = nullptr;           /**< 主板 COM */
    KeyStateAggregator *m_aggregator = nullptr; /**< 短窗位图聚合 */
    MidiInputDevice *m_midiIn = nullptr;        /**< 外接 MIDI */
    MidiPlayer *m_player = nullptr;             /**< 文件回放 */
    KeyMapping m_mapping;                       /**< 音高↔通道 */
    PerformanceRecorder m_recorder;             /**< 演奏录制 */

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_refreshPortBtn = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QLabel *m_serialStatus = nullptr;
    QCheckBox *m_debugCheck = nullptr;  /**< 同步下位机 DEBUG ON/OFF */
    QTabWidget *m_modeTabs = nullptr;

    QComboBox *m_midiDevCombo = nullptr;
    QPushButton *m_refreshMidiBtn = nullptr;
    QPushButton *m_openMidiBtn = nullptr;
    QLabel *m_liveHint = nullptr;

    QLineEdit *m_fileEdit = nullptr;
    QPushButton *m_openFileBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_pauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_convertOcfBtn = nullptr;
    QPushButton *m_inspectOcfBtn = nullptr;
    QLabel *m_timeLabel = nullptr;
    QSlider *m_tempoSlider = nullptr;
    QLabel *m_tempoLabel = nullptr;
    QComboBox *m_ocfMappingCombo = nullptr;
    QSpinBox *m_ocfRangeStart = nullptr;
    QSpinBox *m_ocfRangeEnd = nullptr;
    QSpinBox *m_ocfWindowMs = nullptr;

    QVector<QPushButton *> m_directButtons; /**< 直接控制 16 键 */
    QVector<bool> m_keyHeld;                /**< 键盘按住防连发 */

    QTableWidget *m_mapTable = nullptr;
    QPushButton *m_resetMapBtn = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QPushButton *m_allOffBtn = nullptr;

    bool m_blockMapEdit = false; /**< 刷新表格时忽略 cellChanged */
    bool m_loadingSettings = false; /**< 加载 setting.txt 时禁止回写 */
    bool m_debugEnabled = false; /**< 与复选框一致：是否显示 RX/TX 热路径日志 */
    qint64 m_durationMs = 0;
    qint64 m_positionMs = 0;
    int m_tempoPercent = 100;
    int m_activeMode = -1;
    QStringList m_logs;
    QProcess *m_converterProcess = nullptr;
    bool m_logNotifyPending = false;

    QVector<MidiNoteEvent> m_controlEvents;
    QVector<OcfFrame> m_controlFrames;
    bool m_controlIsOcf = false;
    QVariantList m_waterfallNotes;
    bool m_virtualPlayback = false; /**< 虚拟演奏：不连串口也可播瀑布流 */

    QTimer m_scanTimer;
    QStringList m_probeQueue;
    QString m_linkPort;
    int m_baudRate = 115200;
    bool m_probeBusy = false;
};

#endif // MAINWINDOW_H

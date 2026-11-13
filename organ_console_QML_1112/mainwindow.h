/**
 * @file mainwindow.h
 * @brief QML 后端：串口、MIDI、回放、录制、映射与调试开关。
 *
 * 模式：
 *   0 实时控制 — 外接 MIDI 设备 NoteOn/Off → 映射 → 位图
 *   1 自动演奏 — 导入 SMF/OCF → MidiPlayer → 位图
 *   2 直接控制 — 鼠标/电脑键盘 → 位图
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "keymapping.h"
#include "keystateaggregator.h"
#include "midiinput.h"
#include "midiplayer.h"
#include "serialclient.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QProcess;
class QEvent;
class MidiRecorder;

class MainWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serialConnected READ serialConnected NOTIFY serialStateChanged)
    Q_PROPERTY(QString serialStatus READ serialStatus NOTIFY serialStateChanged)
    Q_PROPERTY(QStringList serialPortNames READ serialPortNames NOTIFY serialPortsChanged)
    Q_PROPERTY(bool remoteDebug READ remoteDebug NOTIFY remoteDebugChanged)
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
    Q_PROPERTY(bool converting READ converting NOTIFY conversionStateChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY channelCountChanged)
    Q_PROPERTY(quint32 activeBitmap READ activeBitmap NOTIFY activeBitmapChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingStateChanged)
    Q_PROPERTY(qint64 recordingElapsedMs READ recordingElapsedMs NOTIFY recordingElapsedChanged)
    Q_PROPERTY(int recordedEventCount READ recordedEventCount NOTIFY recordingStateChanged)
    Q_PROPERTY(bool hasUnsavedRecording READ hasUnsavedRecording NOTIFY recordingStateChanged)

public:
    explicit MainWindow(QObject *parent = nullptr);
    ~MainWindow() override;

    bool serialConnected() const;
    QString serialStatus() const;
    QStringList serialPortNames() const;
    bool remoteDebug() const { return m_debugEnabled; }
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
    bool converting() const { return m_converterProcess != nullptr; }
    int channelCount() const { return m_channelCount; }
    quint32 activeBitmap() const { return m_activeBitmap; }
    bool recording() const;
    qint64 recordingElapsedMs() const;
    int recordedEventCount() const;
    bool hasUnsavedRecording() const;

    Q_INVOKABLE void refreshSerialPorts();
    Q_INVOKABLE void toggleSerial(const QString &portName, int baudRate);
    Q_INVOKABLE void setRemoteDebug(bool enabled);
    Q_INVOKABLE void refreshMidiInputs();
    Q_INVOKABLE void toggleMidiInput(int deviceIndex);
    Q_INVOKABLE void chooseControlFile();
    Q_INVOKABLE void convertMidiFile(const QString &mappingMode, int rangeStart,
                                     int rangeEnd, int windowMs);
    Q_INVOKABLE QString readOcfText(const QString &path);
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
    Q_INVOKABLE QString setChannelCount(int count);
    Q_INVOKABLE QString startRecording();
    Q_INVOKABLE QString stopRecording();
    Q_INVOKABLE QString saveRecording();
    Q_INVOKABLE void discardRecording();

signals:
    void serialStateChanged();
    void serialPortsChanged();
    void remoteDebugChanged();
    void midiInputStateChanged();
    void fileStateChanged();
    void playbackStateChanged();
    void playbackPositionChanged();
    void logLinesChanged();
    void mappingRowsChanged();
    void conversionStateChanged();
    void conversionFinished(bool success, const QString &path, const QString &error);
    void channelCountChanged();
    void activeBitmapChanged();
    void recordingStateChanged();
    void recordingElapsedChanged();
    void recordingSaved(const QString &path);
    void recordingError(const QString &message);

protected:
    /** 安装到 QML 窗口上，绕开 QML 焦点问题处理 32 键直接控制。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void refreshPortList();
    void onSerialBytes(const QString &text);
    void onSerialError(const QString &text);
    void onSerialConnectionChanged(bool connected);
    void onModeTabChanged(int index);

    void refreshMidiDevices();
    void onLiveNoteOn(int midiNote, int velocity);
    void onLiveNoteOff(int midiNote, int velocity);

    void openMidiFile();
    void onPlay();
    void onPause();
    void onStop();
    void onChannelCommand(int channel, bool on);
    void onUnmappedNote(int midiNote, bool on);
    void onPlayerFinished();
    void onPositionChanged(qint64 ms);
    void onBitmapCommand(quint32 bitmap);

    void onDirectPressed(int channel);
    void onDirectReleased(int channel);
    void onAllOffClicked();
    void onResetMapping();
    /** 聚合器发出 ST 快照时写日志（仅调试开）。 */
    void onSnapshotSent(quint32 bitmap, quint16 seq);

private:
    bool loadControlFile(const QString &path);
    void startOcfConversion(const QString &inputPath, const QString &mappingMode,
                            int rangeStart, int rangeEnd, int windowMs);
    void updatePlayUi();
    void updateLiveUi();
    void highlightChannel(int channel, bool on);
    bool sendChannel(int channel, bool on);
    void appendLog(const QString &text);
    int keyToChannel(int key) const;
    void releaseDirectKeys();
    static QString formatMs(qint64 ms);
    static QString boardName(int channel);
    void loadSettings();
    void saveSettings();
    QString defaultRecordingPath() const;
    quint32 channelMask() const;
    void setActiveBitmap(quint32 bitmap);

    SerialClient *m_serial = nullptr;           /**< 主板 COM */
    KeyStateAggregator *m_aggregator = nullptr; /**< 短窗位图聚合 */
    MidiInputDevice *m_midiIn = nullptr;        /**< 外接 MIDI */
    MidiPlayer *m_player = nullptr;             /**< 文件回放 */
    MidiRecorder *m_recorder = nullptr;         /**< MIDI 录制 */
    KeyMapping m_mapping;                       /**< 音高↔通道 */

    QVector<bool> m_keyHeld; /**< 直接控制键盘按住状态 */

    bool m_debugEnabled = false; /**< 是否显示 RX/TX 热路径日志并同步下位机 DEBUG */
    qint64 m_durationMs = 0;
    qint64 m_positionMs = 0;
    int m_tempoPercent = 100;
    int m_activeMode = -1;
    int m_channelCount = KeyMapping::kDefaultChannelCount;
    quint32 m_activeBitmap = 0;
    QString m_currentFile;
    QStringList m_serialPorts;
    QStringList m_midiDevices;
    QStringList m_logs;
    QProcess *m_converterProcess = nullptr;
};

#endif // MAINWINDOW_H

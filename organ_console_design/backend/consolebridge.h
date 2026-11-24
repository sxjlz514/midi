/**
 * @file consolebridge.h
 * @brief 前端与后端模块之间的桥接层（QObject，供 QML 以 "console" 使用）。
 *
 * 职责边界：
 *  - 只做状态聚合与协议衔接，不复制任何 Widgets 代码；
 *  - 内部统一使用 OCF 位图帧作为播放数据（录制 / 导入均折成帧）；
 *  - 播放与实时弹奏互斥：播放期间拒绝键盘与 MIDI 输入。
 */

#ifndef CONSOLEBRIDGE_H
#define CONSOLEBRIDGE_H

#include "keymapping.h"
#include "keystateaggregator.h"
#include "midiinput.h"
#include "midiplayer.h"
#include "ocffile.h"
#include "serialclient.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class ConsoleBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool serialConnected READ serialConnected NOTIFY serialStateChanged)
    Q_PROPERTY(QString serialStatus READ serialStatus NOTIFY serialStateChanged)
    Q_PROPERTY(QString linkedPort READ linkedPort NOTIFY serialStateChanged)
    Q_PROPERTY(int baudRate READ baudRate NOTIFY serialStateChanged)
    Q_PROPERTY(QStringList serialPortNames READ serialPortNames NOTIFY serialPortsChanged)
    Q_PROPERTY(QStringList midiInputDevices READ midiInputDevices NOTIFY midiInputStateChanged)
    Q_PROPERTY(bool midiInputOpen READ midiInputOpen NOTIFY midiInputStateChanged)
    Q_PROPERTY(bool boardOnline READ boardOnline NOTIFY boardStateChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY fileStateChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY fileStateChanged)
    Q_PROPERTY(bool fileLoaded READ fileLoaded NOTIFY fileStateChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY fileStateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY playbackPositionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackStateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY playbackStateChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(qint64 recordElapsedMs READ recordElapsedMs NOTIFY recordingChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY fileStateChanged)
    Q_PROPERTY(bool unsavedRecording READ unsavedRecording NOTIFY fileStateChanged)
    Q_PROPERTY(QStringList logLines READ logLines NOTIFY logLinesChanged)
    Q_PROPERTY(QVariantList mappingRows READ mappingRows NOTIFY mappingRowsChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY mappingRowsChanged)
    Q_PROPERTY(int displayChannelCount READ displayChannelCount NOTIFY displayChanged)
    Q_PROPERTY(QVariantList displayMapping READ displayMapping NOTIFY displayChanged)
    Q_PROPERTY(QVariantList activeChannels READ activeChannels NOTIFY activeChannelsChanged)
    Q_PROPERTY(QVariantList waterfallNotes READ waterfallNotes NOTIFY waterfallNotesChanged)
    Q_PROPERTY(bool virtualPlayback READ virtualPlayback WRITE setVirtualPlayback NOTIFY virtualPlaybackChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ConsoleBridge(QObject *parent = nullptr);
    ~ConsoleBridge() override;

    bool serialConnected() const { return m_serial->isOpen(); }
    QString serialStatus() const;
    QString linkedPort() const { return m_linkedPort; }
    int baudRate() const { return m_baudRate; }
    QStringList serialPortNames() const { return m_portNames; }
    QStringList midiInputDevices() const { return m_midi->listDevices(); }
    bool midiInputOpen() const { return m_midi->isOpen(); }
    bool boardOnline() const { return m_boardOnline; }
    QString currentFile() const { return m_currentFile; }
    QStringList recentFiles() const { return m_recentFiles; }
    bool fileLoaded() const { return m_hasCurrent; }
    qint64 durationMs() const { return m_durationMs; }
    qint64 positionMs() const { return m_positionMs; }
    bool playing() const { return m_player->isPlaying() && !m_player->isPaused(); }
    bool paused() const { return m_player->isPaused(); }
    bool recording() const { return m_recording; }
    qint64 recordElapsedMs() const { return m_recordElapsedMs; }
    bool canSave() const { return m_hasCurrent; }
    bool unsavedRecording() const { return m_unsaved; }
    QStringList logLines() const { return m_logs; }
    QVariantList mappingRows() const;
    int channelCount() const { return m_mapping.channelCount(); }
    int displayChannelCount() const { return m_displayCount; }
    QVariantList displayMapping() const { return m_displayMapping; }
    QVariantList activeChannels() const;
    QVariantList waterfallNotes() const { return m_waterfallNotes; }
    bool virtualPlayback() const { return m_virtualPlayback; }
    QString lastError() const { return m_lastError; }

    /** 启动后调用：读设置、枚举设备、尝试自动连接上次串口。 */
    Q_INVOKABLE void initialize();

    /** 播放中的实时位置（毫秒），供界面定时器拉取。 */
    Q_INVOKABLE qint64 livePositionMs() const;

    Q_INVOKABLE void refreshSerialPorts();
    Q_INVOKABLE bool connectSerial(const QString &portName, int baud);
    Q_INVOKABLE void disconnectSerial();
    Q_INVOKABLE void setBaudRate(int baudRate);

    Q_INVOKABLE void refreshMidiDevices();
    Q_INVOKABLE bool connectMidi(int deviceIndex);
    Q_INVOKABLE void disconnectMidi();

    /** 解析 MIDI 并按参数折成 OCF，只在内存中播放。返回空串表示成功。 */
    Q_INVOKABLE QString loadMidiAsOcf(const QString &path, int rangeStart, int rangeEnd,
                                      bool wrap, int windowMs);
    /** 直接读取 OCF 文件。返回空串表示成功。 */
    Q_INVOKABLE QString loadOcfFile(const QString &path);

    /** 按后缀自动打开（.ocf 直读；.mid 用当前映射推导的默认参数转换）。 */
    Q_INVOKABLE QString openFileAuto(const QString &path);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

    /** 关闭当前文件：停止播放、清空内容，键盘恢复为配置通道数。 */
    Q_INVOKABLE void closeFile();

    Q_INVOKABLE QString startRecording();
    Q_INVOKABLE QString stopRecording();

    /** 保存当前内存中的曲目；按后缀写 .ocf 或 .mid。返回空串表示成功。 */
    Q_INVOKABLE QString saveCurrent(const QString &path);

    /** 鼠标点按琴键：按通道下发（仅在非播放态有效）。 */
    Q_INVOKABLE void pressChannel(int channel);
    Q_INVOKABLE void releaseChannel(int channel);

    /**
     * 修改映射音高。
     * @param cascade true=跟随模式（后续通道依次顺移）；false=只改这一个
     * @return 空串成功，否则为拒绝原因
     */
    Q_INVOKABLE QString setMappingNote(int channel, int midiNote, bool cascade);

    Q_INVOKABLE void setVirtualPlayback(bool enabled);
    Q_INVOKABLE void clearLog();

signals:
    /** 锁定态（文件已加载）下收到输入：true 按下、false 松开。 */
    void keyboardLocked(bool pressed);

    void serialStateChanged();
    void serialPortsChanged();
    void midiInputStateChanged();
    void boardStateChanged();
    void fileStateChanged();
    void playbackStateChanged();
    void playbackPositionChanged();
    void recordingChanged();
    void logLinesChanged();
    void mappingRowsChanged();
    void displayChanged();
    void activeChannelsChanged();
    void waterfallNotesChanged();
    void virtualPlaybackChanged();
    void lastErrorChanged();

    /** 实时弹奏的通道边沿，供界面绘制向上瀑布流。 */
    void liveNote(int channel, bool on);
    /** 面向用户的提示（toast）。 */
    void toast(const QString &message);

private slots:
    void onSerialBytes(const QString &text);
    void onSerialError(const QString &text);
    void onSerialConnectionChanged(bool connected);
    void onLiveNoteOn(int midiNote, int velocity);
    void onLiveNoteOff(int midiNote, int velocity);
    void onBitmapCommand(quint32 bitmap);
    void onPlayerFinished();
    void onPositionChanged(qint64 ms);
    void onRecordFlushTimeout();
    void onRecordTick();
    void onBoardProbeTimeout();
    void onPortWatch();

private:
    void loadSettings();
    void saveSettings() const;
    QString settingsFilePath(bool forWrite) const;

    void appendLog(const QString &text);
    void setLastError(const QString &text);
    void addRecent(const QString &path);

    bool channelGuard() const;              /**< 播放/暂停中禁止键盘与 MIDI 输入 */
    void applyChannelState(int channel, bool on);
    void setActiveChannel(int channel, bool on);
    void markRecordBitmap(int channel, bool on);

    void setOcfDisplay(bool ocf);
    void rebuildDisplayMapping();
    void rebuildWaterfallNotes();

    void loadFrameData(const OcfData &data, const QString &path, bool unsaved);
    OcfData buildSaveData() const;

    int displayNoteForChannel(int channel) const;

    SerialClient *m_serial = nullptr;
    KeyStateAggregator *m_aggregator = nullptr;
    MidiInputDevice *m_midi = nullptr;
    MidiPlayer *m_player = nullptr;
    KeyMapping m_mapping;

    QStringList m_logs;
    QString m_lastError;
    bool m_logNotifyPending = false;

    QStringList m_portNames;
    QString m_linkedPort;
    int m_baudRate = 115200;
    bool m_boardOnline = false;

    QStringList m_recentFiles;
    QString m_currentFile;
    bool m_hasCurrent = false;
    bool m_unsaved = false;
    bool m_ocfDisplay = false;
    OcfData m_current;
    qint64 m_durationMs = 0;
    qint64 m_positionMs = 0;

    int m_displayCount = 16;
    QVariantList m_displayMapping;
    QVariantList m_waterfallNotes;
    QSet<int> m_activeChannels;

    bool m_mappingEdited = false;
    int m_baseNote = 48;

    bool m_recording = false;
    QVector<OcfFrame> m_recordFrames;
    quint32 m_recordBitmap = 0;
    quint32 m_recordLastEmitted = 0;
    qint64 m_recordElapsedMs = 0;
    QElapsedTimer m_recordClock;
    QTimer m_recordFlushTimer;
    QTimer m_recordTick;
    int m_recordWindowMs = 4;

    bool m_virtualPlayback = false;

    QTimer m_portWatch;
    QTimer m_boardProbe;
};

#endif // CONSOLEBRIDGE_H

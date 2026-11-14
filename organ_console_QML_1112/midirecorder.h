/**
 * @file midirecorder.h
 * @brief 把实时 MIDI 与直接控制音符录制成标准 MIDI 文件（SMF Format 0）。
 *
 * 时间基准：division=480，tempo=480000 µs/四分音符，即 1 tick = 1 ms。
 */

#ifndef MIDIRECORDER_H
#define MIDIRECORDER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class MidiRecorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY elapsedChanged)
    Q_PROPERTY(int eventCount READ eventCount NOTIFY eventCountChanged)

public:
    explicit MidiRecorder(QObject *parent = nullptr);

    bool isRecording() const { return m_recording; }
    /** 录音已停止但尚未写盘。 */
    bool hasUnsaved() const { return !m_recording && !m_events.isEmpty(); }
    qint64 elapsedMs() const;
    int eventCount() const { return static_cast<int>(m_events.size()); }

    /** 开始新录制；会清空上一次的内存数据。 */
    void start(int channelCount, const QVector<int> &mappingNotes);
    /** 停止捕获并补齐所有仍按住音符的 Note Off，数据保留在内存。 */
    void stop();
    /** 停止（如仍在录制）并写入 path。成功后清空内存数据。 */
    bool stopAndSave(const QString &path, QString &error);
    /** 把当前内存中的录制写入 path。 */
    bool writeFile(const QString &path, QString &error);
    /** 丢弃内存录制。 */
    void cancel();

    void noteOn(int note, int velocity);
    void noteOff(int note);

signals:
    void recordingChanged();
    void eventCountChanged();
    void elapsedChanged(qint64 ms);

private slots:
    void tick();

private:
    struct Event {
        qint64 ms = 0;
        quint8 status = 0;
        quint8 note = 0;
        quint8 velocity = 0;
    };

    void appendEventAt(int note, int velocity, bool on, qint64 ms);
    qint64 nowMs() const;
    QByteArray buildSmf() const;

    static void appendVarLen(QByteArray &out, quint32 value);
    static void appendBe16(QByteArray &out, quint16 value);
    static void appendBe32(QByteArray &out, quint32 value);

    QVector<Event> m_events;
    QHash<int, int> m_activeNotes; /**< note -> 未释放次数 */
    QVector<int> m_mappingNotes;   /**< 录制开始时的音高表快照 */
    int m_channelCount = 16;
    bool m_recording = false;
    qint64 m_lastMs = 0;
    QElapsedTimer m_clock;
    QTimer m_uiTimer;
};

#endif // MIDIRECORDER_H

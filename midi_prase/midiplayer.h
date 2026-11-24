/**
 * @file midiplayer.h
 * @brief 按时间轴回放 Note 事件，经映射表转为通道 ON/OFF
 */

#ifndef MIDIPLAYER_H
#define MIDIPLAYER_H

#include "keymapping.h"
#include "midiparser.h"

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <bitset>

class MidiPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MidiPlayer(QObject *parent = nullptr);

    void setMapping(const KeyMapping *mapping);
    void setEvents(const QVector<MidiNoteEvent> &events);

    bool isPlaying() const { return m_playing; }
    bool isPaused() const { return m_paused; }
    qint64 positionMs() const;
    qint64 durationMs() const { return m_durationMs; }

public slots:
    void play();
    void pause();
    void stop();
    void setTempoScale(double scale); /**< 1.0=原速 */

signals:
    /** 通道 1..16 开关（已映射） */
    void channelCommand(int channel, bool on);

    /** 未映射的音高（可记日志） */
    void unmappedNote(int midiNote, bool on);

    void positionChanged(qint64 ms);
    void finished();
    void stopped();

private slots:
    void onTick();

private:
    void releaseAllChannels();
    void applyEvent(const MidiNoteEvent &ev);
    void scheduleNext();

    const KeyMapping *m_mapping = nullptr;
    QVector<MidiNoteEvent> m_events;
    int m_index = 0;
    qint64 m_durationMs = 0;
    double m_tempoScale = 1.0;

    bool m_playing = false;
    bool m_paused = false;
    qint64 m_pauseOffsetMs = 0;

    QElapsedTimer m_clock;
    QTimer m_timer;
    std::bitset<17> m_channelOn; /**< index 1..16 */
};

#endif // MIDIPLAYER_H

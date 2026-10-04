#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "midifiledecoder.h"

class QTimer;

/*
 * .mid 文件回放：按文件里的时间戳逐个抛出音符事件。
 *
 * 事件的消费方式与 MidiInput::messageReceived 一致，
 * 因此可以直接喂给 MidiKeyRouter。
 */
class MidiFilePlayer : public QObject
{
    Q_OBJECT

public:
    explicit MidiFilePlayer(QObject *parent = nullptr);

    bool load(
        const QString &filePath,
        QString *errorMessage = nullptr
        );

    QString filePath() const;

    int eventCount() const;

    quint32 totalMs() const;

    bool isPlaying() const;

    bool isPaused() const;

public slots:
    void play();

    void pause();

    void resume();

    void stop();

signals:
    void eventPlayed(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

    void progressChanged(
        quint32 currentMs,
        quint32 totalMs
        );

    void stateChanged();

    void finished();

private slots:
    void playNext();

private:
    /* 排下一个事件的等待时间；同一时刻的事件直接连发 */
    void scheduleNext();

    /* 发出第 index 个事件，并把播放头推进到它的时间戳 */
    void emitEvent(int index);

    void finishPlayback();

    QVector<MidiFileEvent> m_events;

    QTimer *m_timer = nullptr;

    QString m_filePath;

    int m_index = 0;

    quint32 m_playheadMs = 0;

    /* 暂停时下一个事件还剩多少毫秒 */
    int m_pendingMs = 0;

    bool m_playing = false;

    bool m_paused = false;
};

#include "midifileplayer.h"

#include <QTimer>

#include <algorithm>

MidiFilePlayer::MidiFilePlayer(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);

    connect(
        m_timer,
        &QTimer::timeout,
        this,
        &MidiFilePlayer::playNext
        );
}

bool MidiFilePlayer::load(
    const QString &filePath,
    QString *errorMessage)
{
    stop();

    const MidiFileParseResult parsed =
        MidiFileDecoder::parse(filePath);

    if (!parsed.ok) {
        if (errorMessage) {
            *errorMessage = parsed.error;
        }

        return false;
    }

    QVector<MidiFileEvent> events;
    events.reserve(parsed.events.size());

    for (const MidiFileEvent &event : parsed.events) {
        const quint8 command = event.status & 0xF0;

        /* 只保留音符开关事件，与 MidiFileDecoder::decodeToLines 约定一致 */
        if (command != 0x80 &&
            command != 0x90) {

            continue;
        }

        events.append(event);
    }

    std::stable_sort(
        events.begin(),
        events.end(),
        [](const MidiFileEvent &left,
           const MidiFileEvent &right) {
            return left.timestampMs < right.timestampMs;
        }
        );

    m_events = events;
    m_filePath = filePath;
    m_index = 0;
    m_playheadMs = 0;
    m_pendingMs = 0;

    if (errorMessage) {
        errorMessage->clear();
    }

    emit progressChanged(0, totalMs());
    emit stateChanged();

    return true;
}

QString MidiFilePlayer::filePath() const
{
    return m_filePath;
}

int MidiFilePlayer::eventCount() const
{
    return m_events.size();
}

quint32 MidiFilePlayer::totalMs() const
{
    if (m_events.isEmpty()) {
        return 0;
    }

    return m_events.last().timestampMs;
}

bool MidiFilePlayer::isPlaying() const
{
    return m_playing;
}

bool MidiFilePlayer::isPaused() const
{
    return m_paused;
}

void MidiFilePlayer::play()
{
    if (m_events.isEmpty()) {
        return;
    }

    if (m_paused) {
        resume();
        return;
    }

    if (m_playing) {
        return;
    }

    m_playing = true;
    m_paused = false;
    m_index = 0;
    m_playheadMs = 0;
    m_pendingMs = 0;

    emit stateChanged();

    scheduleNext();
}

void MidiFilePlayer::resume()
{
    if (!m_paused) {
        return;
    }

    m_paused = false;
    m_playing = true;

    emit stateChanged();

    if (m_pendingMs > 0) {
        m_timer->start(m_pendingMs);
        return;
    }

    playNext();
}

void MidiFilePlayer::pause()
{
    if (!m_playing || m_paused) {
        return;
    }

    m_pendingMs =
        m_timer->isActive()
            ? qMax(0, m_timer->remainingTime())
            : 0;

    m_timer->stop();

    m_playing = false;
    m_paused = true;

    emit stateChanged();
}

void MidiFilePlayer::stop()
{
    m_timer->stop();

    m_playing = false;
    m_paused = false;
    m_index = 0;
    m_playheadMs = 0;
    m_pendingMs = 0;

    emit progressChanged(0, totalMs());
    emit stateChanged();
}

void MidiFilePlayer::playNext()
{
    if (!m_playing) {
        return;
    }

    if (m_index >= m_events.size()) {
        finishPlayback();
        return;
    }

    /* 定时器到点：当前这个事件已经到期，直接发，不再重算延迟 */
    emitEvent(m_index);

    scheduleNext();
}

void MidiFilePlayer::emitEvent(int index)
{
    const MidiFileEvent event = m_events.at(index);

    emit eventPlayed(
        event.status,
        event.data1,
        event.data2,
        event.timestampMs
        );

    m_playheadMs = event.timestampMs;
    m_index = index + 1;

    emit progressChanged(m_playheadMs, totalMs());
}

void MidiFilePlayer::scheduleNext()
{
    /*
     * 用循环而不是递归推进：同一时刻的事件（和弦）会一次发完，
     * 同时避免长文件在同一时刻堆积时递归过深。
     */
    while (m_playing &&
           m_index < m_events.size()) {

        const qint32 delay =
            qint32(m_events.at(m_index).timestampMs) -
            qint32(m_playheadMs);

        if (delay > 0) {
            m_pendingMs = delay;
            m_timer->start(delay);

            return;
        }

        emitEvent(m_index);
    }

    if (m_playing) {
        finishPlayback();
    }
}

void MidiFilePlayer::finishPlayback()
{
    m_playing = false;
    m_paused = false;
    m_playheadMs = totalMs();

    emit progressChanged(m_playheadMs, totalMs());
    emit stateChanged();
    emit finished();
}

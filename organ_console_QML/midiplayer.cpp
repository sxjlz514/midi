/**
 * @file midiplayer.cpp
 * @brief MidiPlayer：QElapsedTimer + PreciseTimer 按毫秒发放映射后的通道命令。
 */

#include "midiplayer.h"

MidiPlayer::MidiPlayer(QObject *parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MidiPlayer::onTick);
}

void MidiPlayer::setMapping(const KeyMapping *mapping)
{
    m_mapping = mapping;
}

void MidiPlayer::setEvents(const QVector<MidiNoteEvent> &events)
{
    stop();
    m_framePlayback = false;
    m_frames.clear();
    m_events = events;
    m_durationMs = events.isEmpty() ? 0 : events.last().timeMs;
}

void MidiPlayer::setFrames(const QVector<OcfFrame> &frames, qint64 durationMs)
{
    stop();
    m_framePlayback = true;
    m_events.clear();
    m_frames = frames;
    m_durationMs = durationMs;
}

qint64 MidiPlayer::positionMs() const
{
    if (!m_playing || m_paused) {
        return m_pauseOffsetMs;
    }
    return m_pauseOffsetMs
           + static_cast<qint64>(m_clock.elapsed() * m_tempoScale + 0.5);
}

void MidiPlayer::setTempoScale(double scale)
{
    if (scale < 0.1) {
        scale = 0.1;
    }
    if (scale > 4.0) {
        scale = 4.0;
    }
    if (m_playing && !m_paused) {
        m_pauseOffsetMs = positionMs();
        m_clock.restart();
    }
    m_tempoScale = scale;
}

void MidiPlayer::play()
{
    if (m_framePlayback ? m_frames.isEmpty() : m_events.isEmpty()) {
        return;
    }
    if (m_paused) {
        m_paused = false;
        m_playing = true;
        m_clock.restart();
        scheduleNext();
        return;
    }
    releaseAllChannels();
    m_index = 0;
    m_pauseOffsetMs = 0;
    m_playing = true;
    m_paused = false;
    m_clock.restart();
    scheduleNext();
}

void MidiPlayer::pause()
{
    if (!m_playing || m_paused) {
        return;
    }
    m_pauseOffsetMs = positionMs();
    m_paused = true;
    m_timer.stop();
}

void MidiPlayer::stop()
{
    m_timer.stop();
    const bool was = m_playing || m_paused;
    if (was && m_framePlayback) {
        emit bitmapCommand(0);
    }
    m_playing = false;
    m_paused = false;
    m_index = 0;
    m_pauseOffsetMs = 0;
    releaseAllChannels();
    if (was) {
        emit stopped();
    }
    emit positionChanged(0);
}

void MidiPlayer::releaseAllChannels()
{
    for (int ch = 1; ch <= KeyMapping::kChannelCount; ++ch) {
        if (m_channelOn.test(static_cast<size_t>(ch))) {
            m_channelOn.reset(static_cast<size_t>(ch));
            emit channelCommand(ch, false);
        }
    }
}

void MidiPlayer::applyEvent(const MidiNoteEvent &ev)
{
    if (!m_mapping) {
        return;
    }
    const auto chOpt = m_mapping->channelForMidiNote(ev.midiNote);
    if (!chOpt.has_value()) {
        emit unmappedNote(ev.midiNote, ev.type == MidiEventType::NoteOn);
        return;
    }
    const int ch = *chOpt;
    const bool on = (ev.type == MidiEventType::NoteOn);
    if (on) {
        m_channelOn.set(static_cast<size_t>(ch));
    } else {
        m_channelOn.reset(static_cast<size_t>(ch));
    }
    emit channelCommand(ch, on);
}

void MidiPlayer::scheduleNext()
{
    if (!m_playing || m_paused) {
        return;
    }
    const int itemCount = m_framePlayback ? m_frames.size() : m_events.size();
    if (m_index >= itemCount) {
        releaseAllChannels();
        m_playing = false;
        m_timer.stop();
        emit positionChanged(m_durationMs);
        emit finished();
        return;
    }

    const qint64 now = positionMs();
    const qint64 target = m_framePlayback
        ? static_cast<qint64>(m_frames[m_index].timeMs)
        : m_events[m_index].timeMs;
    qint64 delay = target - now;
    if (delay < 0) {
        delay = 0;
    }
    const qint64 wallDelay = static_cast<qint64>(delay / m_tempoScale);
    m_timer.start(static_cast<int>(qMin(wallDelay, qint64(50))));
}

void MidiPlayer::onTick()
{
    if (!m_playing || m_paused) {
        return;
    }

    const qint64 now = positionMs();
    emit positionChanged(now);

    const int itemCount = m_framePlayback ? m_frames.size() : m_events.size();
    while (m_index < itemCount) {
        const qint64 eventTime = m_framePlayback
            ? static_cast<qint64>(m_frames[m_index].timeMs)
            : m_events[m_index].timeMs;
        if (eventTime > now) {
            break;
        }
        if (m_framePlayback) {
            emit bitmapCommand(m_frames[m_index].bitmap);
        } else {
            applyEvent(m_events[m_index]);
        }
        ++m_index;
    }

    if (m_index >= itemCount) {
        releaseAllChannels();
        m_playing = false;
        m_timer.stop();
        emit positionChanged(m_durationMs);
        emit finished();
        return;
    }

    scheduleNext();
}

/**
 * @file midirecorder.cpp
 * @brief 消抖后把时间落到聚合窗网格上，再写成 1 tick = 1 ms 的 MIDI 文件。
 */

#include "midirecorder.h"

#include <QElapsedTimer>
#include <QFile>
#include <QtEndian>
#include <algorithm>

namespace {

qint64 clockNs()
{
    static QElapsedTimer clock;
    static const bool started = [] {
        clock.start();
        return true;
    }();
    Q_UNUSED(started);
    return clock.nsecsElapsed();
}

void writeVarLen(QByteArray &out, quint32 value)
{
    quint32 buffer = value & 0x7Fu;
    value >>= 7;
    while (value) {
        buffer <<= 8;
        buffer |= (value & 0x7Fu) | 0x80u;
        value >>= 7;
    }
    while (true) {
        out.append(static_cast<char>(buffer & 0xFFu));
        if (buffer & 0x80u) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

void appendBe32(QByteArray &out, quint32 value)
{
    const quint32 be = qToBigEndian(value);
    out.append(reinterpret_cast<const char *>(&be), 4);
}

} // namespace

void PerformanceRecorder::setWindowMs(int ms)
{
    m_windowMs = qBound(1, ms, 100);
}

void PerformanceRecorder::setDebounceMs(int ms)
{
    m_debounceMs = qBound(0, ms, 50);
}

void PerformanceRecorder::start()
{
    clear();
    m_recording = true;
    m_originNs = clockNs();
}

void PerformanceRecorder::clear()
{
    m_recording = false;
    m_order = 0;
    m_notes.clear();
    m_events.clear();
}

qint64 PerformanceRecorder::quantizeUs(qint64 timeUs) const
{
    const qint64 windowUs = static_cast<qint64>(m_windowMs) * 1000;
    if (timeUs < 0) {
        timeUs = 0;
    }
    return (timeUs / windowUs) * windowUs;
}

void PerformanceRecorder::commit(int midiNote, bool on, int velocity, qint64 timeUs)
{
    NoteState &state = m_notes[midiNote];
    state.on = on;
    state.hasPending = false;
    Event event;
    event.timeMs = quantizeUs(timeUs) / 1000;
    event.note = midiNote;
    event.on = on;
    event.velocity = on ? qBound(1, velocity, 127) : 0;
    event.order = m_order++;
    m_events.push_back(event);
}

void PerformanceRecorder::flushPending(qint64 nowUs)
{
    const qint64 debounceUs = static_cast<qint64>(m_debounceMs) * 1000;
    for (auto it = m_notes.begin(); it != m_notes.end(); ++it) {
        NoteState &state = it.value();
        if (!state.hasPending) {
            continue;
        }
        if (nowUs - state.pendingUs < debounceUs) {
            continue;
        }
        const int note = it.key();
        const bool on = state.pendingOn;
        const int velocity = state.pendingVelocity;
        const qint64 at = state.pendingUs;
        state.hasPending = false;
        if (state.on == on) {
            continue;
        }
        commit(note, on, velocity, at);
    }
}

void PerformanceRecorder::note(int midiNote, bool on, int velocity)
{
    if (!m_recording || midiNote < 0 || midiNote > 127) {
        return;
    }
    const qint64 nowUs = (clockNs() - m_originNs) / 1000;
    flushPending(nowUs);

    NoteState &state = m_notes[midiNote];
    if (m_debounceMs <= 0) {
        if (state.on == on) {
            return;
        }
        commit(midiNote, on, velocity, nowUs);
        return;
    }

    if (state.hasPending) {
        const bool reversal = state.pendingOn != on;
        const bool inside = nowUs - state.pendingUs < static_cast<qint64>(m_debounceMs) * 1000;
        if (reversal && inside) {
            state.hasPending = false;
            return;
        }
        if (state.on != state.pendingOn) {
            commit(midiNote, state.pendingOn, state.pendingVelocity, state.pendingUs);
        } else {
            state.hasPending = false;
        }
    }

    if (state.on == on) {
        return;
    }
    state.hasPending = true;
    state.pendingOn = on;
    state.pendingVelocity = velocity;
    state.pendingUs = nowUs;
}

void PerformanceRecorder::releaseOpenNotes()
{
    if (!m_recording) {
        return;
    }
    const qint64 nowUs = (clockNs() - m_originNs) / 1000;
    flushPending(nowUs + static_cast<qint64>(m_debounceMs) * 1000);
    for (auto it = m_notes.begin(); it != m_notes.end(); ++it) {
        if (it.value().on) {
            commit(it.key(), false, 0, nowUs);
        }
    }
}

bool PerformanceRecorder::saveSmf(const QString &path, QString &error)
{
    if (m_recording) {
        releaseOpenNotes();
    }
    QVector<Event> events = m_events;
    std::sort(events.begin(), events.end(), [](const Event &a, const Event &b) {
        if (a.timeMs != b.timeMs) {
            return a.timeMs < b.timeMs;
        }
        if (a.on != b.on) {
            return !a.on && b.on;
        }
        return a.order < b.order;
    });

    QHash<int, qint64> openAt;
    for (Event &event : events) {
        if (event.on) {
            openAt.insert(event.note, event.timeMs);
            continue;
        }
        const qint64 onMs = openAt.value(event.note, -1);
        if (onMs >= 0 && event.timeMs <= onMs) {
            event.timeMs = onMs + m_windowMs;
        }
        openAt.remove(event.note);
    }
    std::sort(events.begin(), events.end(), [](const Event &a, const Event &b) {
        if (a.timeMs != b.timeMs) {
            return a.timeMs < b.timeMs;
        }
        if (a.on != b.on) {
            return !a.on && b.on;
        }
        return a.order < b.order;
    });

    QByteArray track;
    writeVarLen(track, 0);
    track.append('\xFF');
    track.append('\x51');
    track.append('\x03');
    track.append('\x0F');
    track.append('\x42');
    track.append('\x40');

    qint64 lastTick = 0;
    for (const Event &event : events) {
        const qint64 tick = qMax<qint64>(0, event.timeMs);
        writeVarLen(track, static_cast<quint32>(tick - lastTick));
        lastTick = tick;
        track.append(event.on ? '\x90' : '\x80');
        track.append(static_cast<char>(event.note));
        track.append(static_cast<char>(event.on ? event.velocity : 0));
    }
    writeVarLen(track, 0);
    track.append('\xFF');
    track.append('\x2F');
    track.append('\x00');

    QByteArray file;
    file.append("MThd");
    appendBe32(file, 6);
    file.append('\x00');
    file.append('\x00');
    file.append('\x00');
    file.append('\x01');
    file.append('\x03');
    file.append('\xE8');
    file.append("MTrk");
    appendBe32(file, static_cast<quint32>(track.size()));
    file.append(track);

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("无法写入 MIDI 文件：%1").arg(out.errorString());
        return false;
    }
    if (out.write(file) != file.size()) {
        error = QStringLiteral("写入 MIDI 文件不完整。");
        return false;
    }
    error.clear();
    return true;
}

/**
 * @file midirecorder.cpp
 * @brief MidiRecorder 实现：捕获音符、补齐 Note Off、写入 SMF Format 0。
 */

#include "midirecorder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtGlobal>

MidiRecorder::MidiRecorder(QObject *parent)
    : QObject(parent)
{
    m_uiTimer.setInterval(100);
    connect(&m_uiTimer, &QTimer::timeout, this, &MidiRecorder::tick);
}

qint64 MidiRecorder::elapsedMs() const
{
    return m_recording ? m_clock.elapsed() : m_lastMs;
}

void MidiRecorder::start(int channelCount, const QVector<int> &mappingNotes)
{
    m_uiTimer.stop();
    m_events.clear();
    m_activeNotes.clear();
    m_mappingNotes = mappingNotes;
    m_channelCount = qBound(1, channelCount, 32);
    m_recording = true;
    m_lastMs = 0;
    m_clock.start();
    m_uiTimer.start();

    emit recordingChanged();
    emit eventCountChanged();
    emit elapsedChanged(0);
}

void MidiRecorder::stop()
{
    if (!m_recording) {
        return;
    }
    const qint64 end = qMax(m_clock.elapsed(), m_lastMs);
    for (auto it = m_activeNotes.constBegin(); it != m_activeNotes.constEnd(); ++it) {
        for (int i = 0; i < it.value(); ++i) {
            appendEventAt(it.key(), 0, false, end);
        }
    }
    m_activeNotes.clear();
    m_lastMs = qMax(m_lastMs, end);
    m_recording = false;
    m_uiTimer.stop();
    emit recordingChanged();
    emit elapsedChanged(m_lastMs);
}

void MidiRecorder::cancel()
{
    const bool wasRecording = m_recording;
    m_uiTimer.stop();
    m_recording = false;
    m_events.clear();
    m_activeNotes.clear();
    m_mappingNotes.clear();
    m_lastMs = 0;
    emit eventCountChanged();
    emit elapsedChanged(0);
    if (wasRecording) {
        emit recordingChanged();
    }
}

bool MidiRecorder::stopAndSave(const QString &path, QString &error)
{
    if (m_recording) {
        stop();
    }
    return writeFile(path, error);
}

bool MidiRecorder::writeFile(const QString &path, QString &error)
{
    if (m_recording) {
        error = QStringLiteral("请先停止录制。");
        return false;
    }
    if (path.isEmpty()) {
        error = QStringLiteral("保存路径为空。");
        return false;
    }

    const QByteArray smf = buildSmf();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法写入文件：%1").arg(file.errorString());
        return false;
    }
    if (file.write(smf) != smf.size()) {
        error = QStringLiteral("写入文件失败：%1").arg(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = QStringLiteral("提交文件失败：%1").arg(file.errorString());
        return false;
    }

    m_events.clear();
    m_activeNotes.clear();
    m_mappingNotes.clear();
    m_lastMs = 0;
    emit eventCountChanged();
    emit elapsedChanged(0);
    return true;
}

void MidiRecorder::noteOn(int note, int velocity)
{
    if (!m_recording || note < 0 || note > 127) {
        return;
    }
    if (velocity <= 0) {
        noteOff(note);
        return;
    }
    velocity = qBound(1, velocity, 127);
    ++m_activeNotes[note];
    appendEventAt(note, velocity, true, nowMs());
}

void MidiRecorder::noteOff(int note)
{
    if (!m_recording || note < 0 || note > 127) {
        return;
    }
    auto it = m_activeNotes.find(note);
    if (it == m_activeNotes.end() || it.value() <= 0) {
        return;
    }
    if (--it.value() <= 0) {
        m_activeNotes.erase(it);
    }
    appendEventAt(note, 0, false, nowMs());
}

void MidiRecorder::tick()
{
    if (m_recording) {
        emit elapsedChanged(m_clock.elapsed());
    }
}

qint64 MidiRecorder::nowMs() const
{
    return qMax(m_clock.elapsed(), m_lastMs);
}

void MidiRecorder::appendEventAt(int note, int velocity, bool on, qint64 ms)
{
    Event event;
    event.ms = qMax(ms, m_lastMs);
    event.status = on ? 0x90 : 0x80;
    event.note = static_cast<quint8>(qBound(0, note, 127));
    event.velocity = static_cast<quint8>(qBound(0, velocity, 127));
    m_lastMs = event.ms;
    m_events.push_back(event);
    emit eventCountChanged();
}

QByteArray MidiRecorder::buildSmf() const
{
    QByteArray track;

    auto appendMeta = [&track](quint8 type, const QByteArray &payload) {
        appendVarLen(track, 0);
        track.append(static_cast<char>(0xFF));
        track.append(static_cast<char>(type));
        appendVarLen(track, static_cast<quint32>(payload.size()));
        track.append(payload);
    };

    appendMeta(0x03, QByteArrayLiteral("Organ Console Recording"));
    /* 480000 µs / 四分音符，division 480 → 1 tick = 1 ms */
    appendMeta(0x51, QByteArray::fromHex("075300"));
    appendMeta(0x58, QByteArray::fromHex("04021808"));

    QJsonObject meta;
    meta.insert(QStringLiteral("app"), QStringLiteral("organ_console"));
    meta.insert(QStringLiteral("channelCount"), m_channelCount);
    QJsonArray mapping;
    for (int note : m_mappingNotes) {
        mapping.append(note);
    }
    meta.insert(QStringLiteral("mapping"), mapping);
    meta.insert(QStringLiteral("baseNote"),
                m_mappingNotes.isEmpty() ? 60 : m_mappingNotes.first());
    meta.insert(QStringLiteral("recordedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    appendMeta(0x7F, QJsonDocument(meta).toJson(QJsonDocument::Compact));

    qint64 previous = 0;
    for (const Event &event : m_events) {
        qint64 delta = event.ms - previous;
        if (delta < 0) {
            delta = 0;
        }
        constexpr qint64 kMaxDelta = 0x0FFFFFFF;
        appendVarLen(track, static_cast<quint32>(qMin(delta, kMaxDelta)));
        track.append(static_cast<char>(event.status));
        track.append(static_cast<char>(event.note));
        track.append(static_cast<char>(event.velocity));
        previous = event.ms;
    }

    /* End of Track */
    appendVarLen(track, 0);
    track.append(static_cast<char>(0xFF));
    track.append(static_cast<char>(0x2F));
    track.append(static_cast<char>(0x00));

    QByteArray out;
    out.append(QByteArrayLiteral("MThd"));
    appendBe32(out, 6);
    appendBe16(out, 0);       // Format 0
    appendBe16(out, 1);       // one track
    appendBe16(out, 480);     // ticks per quarter note
    out.append(QByteArrayLiteral("MTrk"));
    appendBe32(out, static_cast<quint32>(track.size()));
    out.append(track);
    return out;
}

void MidiRecorder::appendVarLen(QByteArray &out, quint32 value)
{
    value &= 0x0FFFFFFFu;
    QByteArray tmp;
    tmp.append(static_cast<char>(value & 0x7F));
    while ((value >>= 7) != 0) {
        tmp.append(static_cast<char>((value & 0x7F) | 0x80));
    }
    for (int i = static_cast<int>(tmp.size()) - 1; i >= 0; --i) {
        out.append(tmp.at(i));
    }
}

void MidiRecorder::appendBe16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

void MidiRecorder::appendBe32(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>((value >> 24) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

#include "midirecorder.h"

#include <QByteArray>
#include <QFile>

#include <algorithm>

namespace {
constexpr quint16 kDivision = 480;
constexpr quint32 kTempoUsPerQuarter = 500000;
}

MidiRecorder::MidiRecorder(QObject *parent)
    : QObject(parent)
{
}

MidiRecorder::~MidiRecorder()
{
}

void MidiRecorder::start()
{
    m_events.clear();
    m_recording = true;
}

void MidiRecorder::stop()
{
    m_recording = false;
}

bool MidiRecorder::isRecording() const
{
    return m_recording;
}

int MidiRecorder::eventCount() const
{
    return m_events.size();
}

void MidiRecorder::clear()
{
    m_events.clear();
}

void MidiRecorder::appendMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    if (!m_recording) {
        return;
    }

    if (!isNoteEvent(status, data1, data2)) {
        return;
    }

    MidiRecordEvent event;
    event.timestampMs = timestampMs;
    event.status = status;
    event.data1 = data1;
    event.data2 = data2;

    m_events.append(event);
}

bool MidiRecorder::isNoteEvent(
    quint8 status,
    quint8,
    quint8)
{
    const quint8 command =
        status & 0xF0;

    return command == 0x80 ||
           command == 0x90;
}

bool MidiRecorder::save(
    const QString &filePath,
    QString *errorMessage)
{
    const auto fail =
        [errorMessage](const QString &text) {
            if (errorMessage) {
                *errorMessage = text;
            }

            return false;
        };

    /*
     * WinMM时间戳可能不严格递增，
     * 按时间排序后再写入轨道。
     */
    QVector<MidiRecordEvent> events = m_events;

    std::sort(
        events.begin(),
        events.end(),
        [](const MidiRecordEvent &left,
           const MidiRecordEvent &right) {
            return left.timestampMs <
                   right.timestampMs;
        }
        );

    QByteArray trackData;

    writeVarLen(trackData, 0);
    trackData.append(char(0xFF));
    trackData.append(char(0x51));
    trackData.append(char(0x03));

    trackData.append(
        char((kTempoUsPerQuarter >> 16) & 0xFF)
        );
    trackData.append(
        char((kTempoUsPerQuarter >> 8) & 0xFF)
        );
    trackData.append(
        char(kTempoUsPerQuarter & 0xFF)
        );

    quint32 previousTick = 0;

    for (const MidiRecordEvent &event : events) {
        const quint32 tick =
            msToTicks(event.timestampMs);

        quint32 delta = 0;

        if (tick > previousTick) {
            delta = tick - previousTick;
        }

        previousTick = tick > previousTick
                           ? tick
                           : previousTick;

        writeVarLen(trackData, delta);

        trackData.append(char(event.status));
        trackData.append(char(event.data1));
        trackData.append(char(event.data2));
    }

    writeVarLen(trackData, 0);
    trackData.append(char(0xFF));
    trackData.append(char(0x2F));
    trackData.append(char(0x00));

    QByteArray header;
    header.append("MThd", 4);
    writeUInt32BE(header, 6);
    header.append(char(0x00));
    header.append(char(0x00));
    header.append(char(0x00));
    header.append(char(0x01));
    header.append(
        char((kDivision >> 8) & 0xFF)
        );
    header.append(char(kDivision & 0xFF));

    QByteArray trackChunk;
    trackChunk.append("MTrk", 4);
    writeUInt32BE(
        trackChunk,
        quint32(trackData.size())
        );
    trackChunk.append(trackData);

    QByteArray fileData = header + trackChunk;

    QFile file(filePath);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Truncate)) {

        return fail(
            QStringLiteral(
                "无法写入文件：%1"
                ).arg(file.errorString())
            );
    }

    const qint64 written =
        file.write(fileData);

    file.close();

    if (written != fileData.size()) {
        return fail(
            QStringLiteral(
                "文件写入不完整：%1"
                ).arg(file.errorString())
            );
    }

    if (errorMessage) {
        errorMessage->clear();
    }

    return true;
}

void MidiRecorder::writeUInt32BE(
    QByteArray &out,
    quint32 value)
{
    out.append(char((value >> 24) & 0xFF));
    out.append(char((value >> 16) & 0xFF));
    out.append(char((value >> 8) & 0xFF));
    out.append(char(value & 0xFF));
}

void MidiRecorder::writeVarLen(
    QByteArray &out,
    quint32 value)
{
    QByteArray buffer;
    buffer.append(char(value & 0x7F));

    while ((value >>= 7) > 0) {
        buffer.prepend(
            char((value & 0x7F) | 0x80)
            );
    }

    out.append(buffer);
}

quint32 MidiRecorder::msToTicks(quint32 timestampMs)
{
    const qint64 ticks =
        (qint64(timestampMs) * kDivision * 120) /
        60000;

    return quint32(ticks);
}

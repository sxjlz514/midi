#include "midifiledecoder.h"
#include "midimessageformatter.h"

#include <QFile>

#include <algorithm>

namespace {

struct TempoChange
{
    quint32 tick = 0;
    quint32 tempoUs = 500000;
};

class ByteReader
{
public:
    ByteReader(const QByteArray &data,
               int start,
               int end)
        : m_data(data)
        , m_pos(start)
        , m_end(end)
    {
    }

    int position() const
    {
        return m_pos;
    }

    bool atEnd() const
    {
        return m_pos >= m_end;
    }

    bool readByte(quint8 &value)
    {
        if (m_pos >= m_end) {
            return false;
        }

        value = quint8(m_data.at(m_pos));
        ++m_pos;
        return true;
    }

    bool readVarLen(quint32 &value)
    {
        value = 0;

        for (int index = 0; index < 4; ++index) {
            quint8 byte = 0;

            if (!readByte(byte)) {
                return false;
            }

            value = (value << 7) |
                    (byte & 0x7F);

            if ((byte & 0x80) == 0) {
                return true;
            }
        }

        return false;
    }

    bool skip(int count)
    {
        if (count < 0 ||
            m_pos + count > m_end) {
            return false;
        }

        m_pos += count;
        return true;
    }

private:
    const QByteArray &m_data;
    int m_pos;
    int m_end;
};

quint16 readUInt16BE(const char *bytes)
{
    return quint16(
        (quint8(bytes[0]) << 8) |
        quint8(bytes[1])
        );
}

quint32 readUInt32BE(const char *bytes)
{
    return (quint32(quint8(bytes[0])) << 24) |
           (quint32(quint8(bytes[1])) << 16) |
           (quint32(quint8(bytes[2])) << 8) |
           quint32(quint8(bytes[3]));
}

int dataByteCount(quint8 status)
{
    const quint8 command =
        status & 0xF0;

    if (command == 0xC0 ||
        command == 0xD0) {
        return 1;
    }

    return 2;
}

quint32 ticksToMs(
    quint32 tick,
    quint16 division,
    const QVector<TempoChange> &tempos)
{
    if (division == 0) {
        return 0;
    }

    qint64 currentTick = 0;
    qint64 currentMs = 0;
    qint64 tempo = 500000;

    for (const TempoChange &change : tempos) {
        if (change.tick >= tick) {
            break;
        }

        const qint64 deltaTicks =
            qint64(change.tick) - currentTick;

        currentMs +=
            (deltaTicks * tempo) /
            (qint64(division) * 1000);

        currentTick = change.tick;
        tempo = change.tempoUs;
    }

    const qint64 deltaTicks =
        qint64(tick) - currentTick;

    currentMs +=
        (deltaTicks * tempo) /
        (qint64(division) * 1000);

    if (currentMs < 0) {
        currentMs = 0;
    }

    return quint32(currentMs);
}

bool parseTrack(
    const QByteArray &data,
    int trackStart,
    int trackEnd,
    quint16 division,
    QVector<MidiFileEvent> &events,
    QVector<TempoChange> &tempos,
    QString &error)
{
    ByteReader reader(data, trackStart, trackEnd);

    quint32 absoluteTick = 0;
    quint8 runningStatus = 0;

    while (!reader.atEnd()) {
        quint32 delta = 0;

        if (!reader.readVarLen(delta)) {
            error = QStringLiteral(
                "变长数量(delta)解析失败"
                );
            return false;
        }

        absoluteTick += delta;

        quint8 firstByte = 0;

        if (!reader.readByte(firstByte)) {
            error = QStringLiteral(
                "轨道数据在事件处被截断"
                );
            return false;
        }

        if (firstByte == 0xFF) {
            quint8 metaType = 0;

            if (!reader.readByte(metaType)) {
                error = QStringLiteral(
                    "元事件类型被截断"
                    );
                return false;
            }

            quint32 metaLength = 0;

            if (!reader.readVarLen(metaLength)) {
                error = QStringLiteral(
                    "元事件长度解析失败"
                    );
                return false;
            }

            if (metaType == 0x2F) {
                return true;
            }

            if (metaType == 0x51 &&
                metaLength == 3) {

                quint8 b0 = 0;
                quint8 b1 = 0;
                quint8 b2 = 0;

                if (reader.readByte(b0) &&
                    reader.readByte(b1) &&
                    reader.readByte(b2)) {

                    TempoChange change;
                    change.tick = absoluteTick;
                    change.tempoUs =
                        (quint32(b0) << 16) |
                        (quint32(b1) << 8) |
                        quint32(b2);

                    tempos.append(change);

                    continue;
                }
            }

            if (!reader.skip(int(metaLength))) {
                error = QStringLiteral(
                    "元事件数据被截断"
                    );
                return false;
            }

            continue;
        }

        if (firstByte == 0xF0 ||
            firstByte == 0xF7) {

            quint32 sysexLength = 0;

            if (!reader.readVarLen(sysexLength)) {
                error = QStringLiteral(
                    "SysEx长度解析失败"
                    );
                return false;
            }

            if (!reader.skip(int(sysexLength))) {
                error = QStringLiteral(
                    "SysEx数据被截断"
                    );
                return false;
            }

            continue;
        }

        quint8 status = 0;
        int alreadyReadCount = 0;
        quint8 firstData = 0;

        if (firstByte & 0x80) {
            status = firstByte;

            if (status < 0xF0) {
                runningStatus = status;
            }
        } else {
            if (runningStatus == 0) {
                error = QStringLiteral(
                    "缺少running status"
                    );
                return false;
            }

            status = runningStatus;
            firstData = firstByte;
            alreadyReadCount = 1;
        }

        const int dataCount =
            dataByteCount(status);

        quint8 dataBytes[2] = {0, 0};

        for (int index = alreadyReadCount;
             index < dataCount;
             ++index) {

            quint8 byte = 0;

            if (!reader.readByte(byte) ||
                (byte & 0x80)) {

                error = QStringLiteral(
                    "事件数据字节无效"
                    );
                return false;
            }

            dataBytes[index] = byte;
        }

        if (alreadyReadCount == 1) {
            dataBytes[0] = firstData;
        }

        if (status >= 0xF0) {
            continue;
        }

        MidiFileEvent event;
        event.tick = absoluteTick;
        event.status = status;
        event.data1 = dataBytes[0];
        event.data2 = dataBytes[1];

        events.append(event);
    }

    Q_UNUSED(division)

    return true;
}

} // namespace

MidiFileParseResult MidiFileDecoder::parse(
    const QString &filePath)
{
    MidiFileParseResult result;

    const auto fail =
        [&result](const QString &text) {
            result.ok = false;
            result.error = text;
            return result;
        };

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            QStringLiteral(
                "无法打开文件：%1"
                ).arg(file.errorString())
            );
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.size() < 14) {
        return fail(
            QStringLiteral("文件太小，不是有效的MIDI文件")
            );
    }

    if (!data.startsWith("MThd")) {
        return fail(
            QStringLiteral("缺少MThd头块")
            );
    }

    const quint32 headerLength =
        readUInt32BE(data.constData() + 4);

    if (headerLength < 6) {
        return fail(
            QStringLiteral("MThd头块长度无效")
            );
    }

    if (data.size() < qint64(8 + headerLength)) {
        return fail(
            QStringLiteral("MThd头块被截断")
            );
    }

    result.format =
        readUInt16BE(data.constData() + 8);

    result.trackCount =
        readUInt16BE(data.constData() + 10);

    result.division =
        readUInt16BE(data.constData() + 12);

    if (result.division & 0x8000) {
        return fail(
            QStringLiteral(
                "暂不支持SMPTE时间division"
                )
            );
    }

    QVector<TempoChange> tempos;

    int offset = 8 + int(headerLength);
    int tracksSeen = 0;

    while (offset + 8 <= data.size() &&
           tracksSeen < result.trackCount) {

        if (data.mid(offset, 4) != "MTrk") {
            const quint32 chunkLength =
                readUInt32BE(
                    data.constData() + offset + 4
                    );

            offset += 8 + int(chunkLength);
            continue;
        }

        const quint32 trackLength =
            readUInt32BE(data.constData() + offset + 4);

        const qint64 trackStart =
            qint64(offset) + 8;

        const qint64 trackEnd =
            trackStart + qint64(trackLength);

        if (trackEnd > data.size()) {
            return fail(
                QStringLiteral("MTrk轨道数据被截断")
                );
        }

        QString trackError;

        if (!parseTrack(
                data,
                int(trackStart),
                int(trackEnd),
                result.division,
                result.events,
                tempos,
                trackError)) {

            return fail(trackError);
        }

        offset = int(trackEnd);
        ++tracksSeen;
    }

    if (tracksSeen == 0) {
        return fail(
            QStringLiteral("没有找到MTrk轨道")
            );
    }

    std::stable_sort(
        tempos.begin(),
        tempos.end(),
        [](const TempoChange &left,
           const TempoChange &right) {
            return left.tick < right.tick;
        }
        );

    for (MidiFileEvent &event : result.events) {
        event.timestampMs = ticksToMs(
            event.tick,
            result.division,
            tempos
            );
    }

    result.ok = true;
    result.error.clear();

    return result;
}

QStringList MidiFileDecoder::decodeToLines(
    const QString &filePath,
    QString *errorMessage)
{
    const MidiFileParseResult parsed =
        parse(filePath);

    if (!parsed.ok) {
        if (errorMessage) {
            *errorMessage = parsed.error;
        }

        return {};
    }

    if (errorMessage) {
        errorMessage->clear();
    }

    QStringList lines;
    lines.reserve(parsed.events.size());

    for (const MidiFileEvent &event : parsed.events) {
        const quint8 command =
            event.status & 0xF0;

        /* 只保留音符开关事件，与 MidiRecorder::isNoteEvent 约定一致 */
        if (command != 0x80 &&
            command != 0x90) {
            continue;
        }

        lines.append(
            MidiMessageFormatter::formatNoteLine(
                event.status,
                event.data1,
                event.data2,
                event.timestampMs
                )
            );
    }

    return lines;
}

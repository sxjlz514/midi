/**
 * @file ocftomidi.cpp
 * @brief 帧差分生成 NoteOn/NoteOff，写成 1 tick = 1 ms 的 SMF。
 */

#include "ocftomidi.h"

#include <QFile>
#include <QtEndian>
#include <algorithm>

namespace {

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

struct NoteEdge {
    qint64 timeMs = 0;
    int midiNote = 0;
    bool on = false;
    int order = 0;
};

} // namespace

QByteArray OcfToMidi::encode(const QVector<OcfFrame> &frames, qint64 durationMs,
                             const KeyMapping &mapping, int velocity)
{
    QVector<NoteEdge> edges;
    const int channelCount = mapping.channelCount();
    quint32 previous = 0;
    int order = 0;

    for (const OcfFrame &frame : frames) {
        const qint64 t = static_cast<qint64>(frame.timeMs);
        const quint32 current = frame.bitmap;
        for (int bit = 0; bit < 32; ++bit) {
            const quint32 mask = 1u << bit;
            const bool was = (previous & mask) != 0;
            const bool now = (current & mask) != 0;
            if (was == now) {
                continue;
            }
            const int channel = bit + 1;
            if (channel > channelCount) {
                continue;
            }
            const int note = mapping.midiNoteForChannel(channel);
            if (note < 0) {
                continue;
            }
            NoteEdge edge;
            edge.timeMs = t;
            edge.midiNote = note;
            edge.on = now;
            edge.order = order++;
            edges.push_back(edge);
        }
        previous = current;
    }

    // 最后仍有按住的音：在曲目结束时补 OFF。
    if (previous != 0) {
        const qint64 t = qMax<qint64>(durationMs, frames.isEmpty()
                                                              ? 0
                                                              : static_cast<qint64>(frames.last().timeMs));
        for (int bit = 0; bit < 32; ++bit) {
            if ((previous & (1u << bit)) == 0) {
                continue;
            }
            const int channel = bit + 1;
            if (channel > channelCount) {
                continue;
            }
            const int note = mapping.midiNoteForChannel(channel);
            if (note < 0) {
                continue;
            }
            NoteEdge edge;
            edge.timeMs = t;
            edge.midiNote = note;
            edge.on = false;
            edge.order = order++;
            edges.push_back(edge);
        }
    }

    std::sort(edges.begin(), edges.end(), [](const NoteEdge &a, const NoteEdge &b) {
        if (a.timeMs != b.timeMs) {
            return a.timeMs < b.timeMs;
        }
        if (a.on != b.on) {
            return !a.on && b.on; // 同刻先 OFF
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
    for (const NoteEdge &edge : edges) {
        const qint64 tick = qMax<qint64>(0, edge.timeMs);
        writeVarLen(track, static_cast<quint32>(qMax<qint64>(0, tick - lastTick)));
        lastTick = tick;
        track.append(edge.on ? '\x90' : '\x80');
        track.append(static_cast<char>(edge.midiNote));
        track.append(static_cast<char>(edge.on ? qBound(1, velocity, 127) : 0));
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
    return file;
}

bool OcfToMidi::exportMidi(const QString &path, const QVector<OcfFrame> &frames,
                           qint64 durationMs, const KeyMapping &mapping,
                           int velocity, QString &error)
{
    error.clear();
    const QByteArray bytes = encode(frames, durationMs, mapping, velocity);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("无法写入 MIDI：%1").arg(file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        error = QStringLiteral("写入 MIDI 不完整。");
        return false;
    }
    return true;
}

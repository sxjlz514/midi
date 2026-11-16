/**
 * @file miditoocf.cpp
 * @brief 与 midi_to_ocf.py 的 encode_ocf 保持逐字节一致。
 */

#include "miditoocf.h"

#include <QHash>
#include <QPair>

bool MidiToOcf::convert(const QVector<MidiNoteEvent> &events, qint64 durationMs,
                        const Options &options, OcfData &out, QString &error)
{
    error.clear();

    const int span = options.rangeEnd - options.rangeStart + 1;
    if (options.rangeStart < 0 || options.rangeEnd > 127
        || options.rangeStart > options.rangeEnd) {
        error = QStringLiteral("音高范围必须满足 0 ≤ 起始 ≤ 结束 ≤ 127。");
        return false;
    }
    if (span > 32) {
        error = QStringLiteral("32 位位图最多映射 32 个音高。");
        return false;
    }
    if (options.windowMs < 1 || options.windowMs > 65535) {
        error = QStringLiteral("聚合窗必须在 1..65535 ms。");
        return false;
    }

    const int low = options.rangeStart;
    const int high = options.rangeEnd;
    const int windowMs = options.windowMs;

    auto slotFor = [&](int note) -> int {
        if (!options.wrap) {
            return (note >= low && note <= high) ? note - low : -1;
        }
        int rel = note - low;
        rel %= span;
        if (rel < 0) {
            rel += span;
        }
        return rel;
    };

    QHash<QPair<int, int>, int> refBySource; /**< (midiChannel,note) → 按住计数 */
    int refsBySlot[32] = {0};
    quint32 bitmap = 0;
    quint32 lastEmitted = 0;
    bool pending = false;
    qint64 deadline = 0;
    QVector<OcfFrame> frames;

    auto flush = [&](qint64 atMs) {
        if (!pending) {
            return;
        }
        if (bitmap != lastEmitted) {
            OcfFrame frame;
            frame.timeMs = atMs < 0 ? 0 : static_cast<quint64>(atMs);
            frame.bitmap = bitmap;
            frames.push_back(frame);
            lastEmitted = bitmap;
        }
        pending = false;
    };

    qint64 lastEventMs = 0;
    int i = 0;
    while (i < events.size()) {
        const qint64 eventMs = events.at(i).timeMs;

        // 同一个时间戳的一组事件：先看窗口是否已到期。
        if (pending && eventMs > deadline) {
            flush(deadline);
        }

        bool didMappedEvent = false;
        while (i < events.size() && events.at(i).timeMs == eventMs) {
            const MidiNoteEvent &ev = events.at(i);
            const int slot = slotFor(ev.midiNote);
            ++i;
            if (slot < 0) {
                continue;
            }
            didMappedEvent = true;
            const QPair<int, int> source(ev.midiChannel, ev.midiNote);
            const quint32 bit = 1u << slot;
            if (ev.type == MidiEventType::NoteOn) {
                refBySource[source] += 1;
                refsBySlot[slot] += 1;
                bitmap |= bit;
            } else {
                const int ref = refBySource.value(source, 0);
                if (ref > 0) {
                    refBySource[source] = ref - 1;
                    refsBySlot[slot] -= 1;
                    if (refsBySlot[slot] == 0) {
                        bitmap &= ~bit;
                    }
                }
            }
        }

        if (didMappedEvent) {
            pending = true;
            deadline = eventMs + windowMs;
        }
        lastEventMs = qMax(lastEventMs, eventMs);
    }

    // 文件结束时仍按住的音：与脚本一致，先 flush 一次，再整体清零补帧。
    bool anyRef = false;
    for (auto it = refBySource.constBegin(); it != refBySource.constEnd(); ++it) {
        if (it.value() > 0) {
            anyRef = true;
            break;
        }
    }
    if (anyRef) {
        if (pending && durationMs > deadline) {
            flush(deadline);
        }
        for (int slot = 0; slot < 32; ++slot) {
            if (refsBySlot[slot] != 0) {
                refsBySlot[slot] = 0;
                bitmap &= ~(1u << slot);
            }
        }
        pending = true;
        deadline = qMax(durationMs, lastEventMs) + windowMs;
    }

    if (pending) {
        flush(deadline);
    }
    if (frames.isEmpty() || frames.last().bitmap != 0) {
        OcfFrame tail;
        const qint64 base = frames.isEmpty() ? 0 : static_cast<qint64>(frames.last().timeMs);
        tail.timeMs = static_cast<quint64>(qMax(durationMs, base));
        tail.bitmap = 0;
        frames.push_back(tail);
    }

    OcfData data;
    data.version = 1;
    data.bitmapBits = 32;
    data.mappingMode = options.wrap ? 1 : 0;
    data.rangeStart = static_cast<quint8>(low);
    data.rangeEnd = static_cast<quint8>(high);
    data.aggregationWindowMs = static_cast<quint16>(windowMs);
    const qint64 fileDuration = qMax(durationMs,
                                     frames.isEmpty() ? 0 : static_cast<qint64>(frames.last().timeMs));
    data.durationMs = static_cast<quint64>(qMax<qint64>(0, fileDuration));
    data.frames = frames;

    out = data;
    return true;
}

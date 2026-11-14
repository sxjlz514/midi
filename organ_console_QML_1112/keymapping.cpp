/**
 * @file keymapping.cpp
 * @brief KeyMapping 实现：默认半音阶表、运行时通道数与音名转换。
 */

#include "keymapping.h"

#include <QSet>
#include <QtGlobal>

KeyMapping::KeyMapping(int channelCount)
{
    resetToDefault(kDefaultBaseNote, channelCount);
}

void KeyMapping::resetToDefault(int baseNote, int count)
{
    if (count < 0) {
        count = m_entries.isEmpty() ? kDefaultChannelCount : channelCount();
    }
    count = qBound(1, count, kMaxChannels);
    baseNote = qBound(0, baseNote, 127);
    if (baseNote + count - 1 > 127) {
        baseNote = 128 - count;
    }

    m_entries.clear();
    m_entries.reserve(count);
    for (int ch = 1; ch <= count; ++ch) {
        PitchMapEntry e;
        e.channel = ch;
        e.midiNote = baseNote + (ch - 1);
        e.noteName = midiNoteToName(e.midiNote);
        m_entries.push_back(e);
    }
}

bool KeyMapping::setChannelCount(int count)
{
    if (count < 1 || count > kMaxChannels) {
        return false;
    }

    const int oldCount = channelCount();
    if (count == oldCount) {
        return true;
    }
    if (count < oldCount) {
        m_entries.resize(count);
        return true;
    }

    QSet<int> used;
    for (const PitchMapEntry &e : m_entries) {
        used.insert(e.midiNote);
    }

    const int addCount = count - oldCount;
    int preferred = m_entries.isEmpty() ? kDefaultBaseNote
                                        : m_entries.first().midiNote + oldCount;
    preferred = qBound(0, preferred, 127);

    auto runIsFree = [&](int start) {
        if (start < 0 || start + addCount - 1 > 127) {
            return false;
        }
        for (int i = 0; i < addCount; ++i) {
            if (used.contains(start + i)) {
                return false;
            }
        }
        return true;
    };

    int runStart = -1;
    for (int start = preferred; start + addCount - 1 <= 127; ++start) {
        if (runIsFree(start)) {
            runStart = start;
            break;
        }
    }
    if (runStart < 0) {
        for (int start = 0; start + addCount - 1 <= 127; ++start) {
            if (runIsFree(start)) {
                runStart = start;
                break;
            }
        }
    }
    if (runStart < 0) {
        return false;
    }

    m_entries.reserve(count);
    for (int ch = oldCount + 1; ch <= count; ++ch) {
        PitchMapEntry e;
        e.channel = ch;
        e.midiNote = runStart + (ch - 1 - oldCount);
        e.noteName = midiNoteToName(e.midiNote);
        m_entries.push_back(e);
    }
    return true;
}

int KeyMapping::midiNoteForChannel(int channel) const
{
    if (channel < 1 || channel > channelCount()) {
        return -1;
    }
    return m_entries[channel - 1].midiNote;
}

std::optional<int> KeyMapping::channelForMidiNote(int midiNote) const
{
    if (midiNote < 0 || midiNote > 127) {
        return std::nullopt;
    }
    for (const PitchMapEntry &e : m_entries) {
        if (e.midiNote == midiNote) {
            return e.channel;
        }
    }
    return std::nullopt;
}

bool KeyMapping::setChannelMidiNote(int channel, int midiNote)
{
    if (channel < 1 || channel > channelCount()) {
        return false;
    }
    if (midiNote < 0 || midiNote > 127) {
        return false;
    }
    PitchMapEntry &e = m_entries[channel - 1];
    e.midiNote = midiNote;
    e.noteName = midiNoteToName(midiNote);
    return true;
}

QString KeyMapping::midiNoteToName(int midiNote)
{
    static const char *kNames[12] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };
    if (midiNote < 0 || midiNote > 127) {
        return QStringLiteral("?");
    }
    const int octave = (midiNote / 12) - 1;
    return QStringLiteral("%1%2").arg(QLatin1String(kNames[midiNote % 12])).arg(octave);
}

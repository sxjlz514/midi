/**
 * @file keymapping.cpp
 * @brief KeyMapping 实现：默认半音阶表与音名转换。
 */

#include "keymapping.h"

#include <QSet>

KeyMapping::KeyMapping()
{
    resetToDefault(kDefaultBaseNote);
}

QString KeyMapping::boardNameForChannel(int channel)
{
    if (channel >= 1 && channel <= 8) {
        return QStringLiteral("N1");
    }
    if (channel <= 16) {
        return QStringLiteral("N2");
    }
    if (channel <= 24) {
        return QStringLiteral("N3");
    }
    if (channel <= kMaxChannelCount) {
        return QStringLiteral("N4");
    }
    return QString();
}

bool KeyMapping::setChannelCount(int count)
{
    if (count < 1 || count > kMaxChannelCount) {
        return false;
    }
    if (count == m_channelCount) {
        return true;
    }
    if (count < m_channelCount) {
        m_entries.resize(count);
        m_channelCount = count;
        return true;
    }

    QSet<int> used;
    for (const PitchMapEntry &entry : m_entries) {
        used.insert(entry.midiNote);
    }
    for (int ch = m_channelCount + 1; ch <= count; ++ch) {
        int note = kDefaultBaseNote + (ch - 1);
        if (note > 127 || used.contains(note)) {
            note = -1;
            for (int candidate = 0; candidate <= 127; ++candidate) {
                if (!used.contains(candidate)) {
                    note = candidate;
                    break;
                }
            }
        }
        if (note < 0) {
            return false;
        }
        used.insert(note);
        PitchMapEntry entry;
        entry.channel = ch;
        entry.midiNote = note;
        entry.noteName = midiNoteToName(note);
        m_entries.push_back(entry);
    }
    m_channelCount = count;
    return true;
}

void KeyMapping::resetToDefault(int baseNote)
{
    const int count = m_channelCount;
    m_entries.clear();
    m_entries.reserve(count);
    QSet<int> used;
    for (int ch = 1; ch <= count; ++ch) {
        int note = baseNote + (ch - 1);
        if (note < 0 || note > 127 || used.contains(note)) {
            note = -1;
            for (int candidate = 0; candidate <= 127; ++candidate) {
                if (!used.contains(candidate)) {
                    note = candidate;
                    break;
                }
            }
        }
        used.insert(note);
        PitchMapEntry e;
        e.channel = ch;
        e.midiNote = note;
        e.noteName = midiNoteToName(e.midiNote);
        m_entries.push_back(e);
    }
}

int KeyMapping::midiNoteForChannel(int channel) const
{
    if (channel < 1 || channel > m_channelCount) {
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
    if (channel < 1 || channel > m_channelCount) {
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

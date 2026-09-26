#include "notemapper.h"

#include "organstate.h"

NoteMapper::NoteMapper(int baseNote)
    : m_baseNote(baseNote)
{
}

int NoteMapper::noteToValve(int note) const
{
    const int valveIndex = note - m_baseNote;

    if (valveIndex < 0 || valveIndex >= OrganState::ValveCount) {
        return -1; // 没有对应阀
    }
    return valveIndex;
}

int NoteMapper::valveToBoard(int valveIndex)
{
    return valveIndex / OrganState::ChannelsPerBoard;
}

int NoteMapper::valveToChannel(int valveIndex)
{
    return valveIndex % OrganState::ChannelsPerBoard;
}

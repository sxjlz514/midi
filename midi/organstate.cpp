#include "organstate.h"

OrganState::OrganState()
{
    m_valves.reset(); // 初始全部关闭
}

void OrganState::setValve(int index, bool on)
{
    if (index < 0 || index >= ValveCount) {
        return;
    }
    m_valves.set(static_cast<std::size_t>(index), on);
}

bool OrganState::valve(int index) const
{
    if (index < 0 || index >= ValveCount) {
        return false;
    }
    return m_valves.test(static_cast<std::size_t>(index));
}

void OrganState::allOff()
{
    m_valves.reset();
}

QByteArray OrganState::snapshot() const
{
    QByteArray bitmap(BitmapBytes, '\0');

    for (int index = 0; index < ValveCount; ++index) {
        if (m_valves.test(static_cast<std::size_t>(index))) {
            const int byteIndex = index / 8; // 落在第几个字节
            const int bitIndex = index % 8;  // 字节内第几位
            bitmap[byteIndex] = static_cast<char>(
                static_cast<unsigned char>(bitmap[byteIndex]) | (1u << bitIndex));
        }
    }
    return bitmap;
}

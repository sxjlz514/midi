#include "frame.h"

quint8 Frame::crc8(const QByteArray &data)
{
    // CRC8 计算：初值 0，逐字节异或后逐位处理；
    // 最高位为 1 则左移一位并异或多项式 0x07，否则只左移。
    quint8 crc = 0x00;
    for (char ch : data) {
        crc ^= static_cast<quint8>(static_cast<unsigned char>(ch));
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80) {
                crc = static_cast<quint8>((crc << 1) ^ 0x07);
            } else {
                crc = static_cast<quint8>(crc << 1);
            }
        }
    }
    return crc;
}

QByteArray Frame::encode(Command cmd, const QByteArray &payload)
{
    QByteArray frame;
    frame.reserve(payload.size() + 5);

    frame.append(static_cast<char>(Sof));            // 帧头
    frame.append(static_cast<char>(payload.size())); // 长度
    frame.append(static_cast<char>(toByte(cmd)));    // 命令
    frame.append(payload);                           // 数据

    // 校验：覆盖“长度 + 命令 + 数据”
    const QByteArray checked = frame.mid(1);
    frame.append(static_cast<char>(crc8(checked)));

    frame.append(static_cast<char>(Eof));            // 帧尾
    return frame;
}

bool Frame::decode(const QByteArray &buffer, Command &cmd, QByteArray &payload)
{
    if (buffer.size() < 5) { // 最小帧长
        return false;
    }
    if (static_cast<quint8>(buffer.at(0)) != Sof) {
        return false;
    }

    const int len = static_cast<quint8>(buffer.at(1));
    const int expectedSize = len + 5;
    if (buffer.size() < expectedSize) {
        return false;
    }
    if (static_cast<quint8>(buffer.at(expectedSize - 1)) != Eof) {
        return false;
    }

    const quint8 cmdByte = static_cast<quint8>(buffer.at(2));
    const QByteArray data = buffer.mid(3, len);
    const quint8 crcReceived = static_cast<quint8>(buffer.at(3 + len));

    const QByteArray checked = buffer.mid(1, 2 + len);
    if (crc8(checked) != crcReceived) {
        return false;
    }

    cmd = static_cast<Command>(cmdByte);
    payload = data;
    return true;
}

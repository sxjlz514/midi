#pragma once

#include <QByteArray>
#include <QtGlobal>

#include "command.h"

// =============================================================================
//  Frame.h —— 协议帧的打包与解析
// -----------------------------------------------------------------------------
//  帧格式：
//      +------+------+------+-------------+-------+------+
//      | SOF  | LEN  | CMD  |  PAYLOAD    | CRC8  | EOF  |
//      | 0xAA | 长度 | 命令 |   数据...   | 校验  | 0x55 |
//      +------+------+------+-------------+-------+------+
//
//  所有函数都是 static（纯工具类），直接 Frame::encode(...) 调用。
// =============================================================================

class Frame
{
public:
    static constexpr quint8 Sof = 0xAA;  // 帧头
    static constexpr quint8 Eof = 0x55;  // 帧尾

    // 计算 CRC8 校验值（多项式 0x07，初值 0x00）。
    static quint8 crc8(const QByteArray &data);

    // 把一条命令打包成完整帧。
    static QByteArray encode(Command cmd, const QByteArray &payload = QByteArray());

    // 尝试从缓冲区开头解析出一帧（最小实现，用于测试与简单场景）。
    static bool decode(const QByteArray &buffer, Command &cmd, QByteArray &payload);

private:
    Frame() = delete; // 纯工具类，禁止创建对象
};

#pragma once

#include <QtGlobal>

// =============================================================================
//  Command.h —— 上位机与主控之间的通信命令字
// -----------------------------------------------------------------------------
//  协议帧格式： [SOF 0xAA][LEN][CMD][PAYLOAD...][CRC8][EOF 0x55]
//  这里的 CMD 就是一个字节的命令字。
//
//  使用 enum class（强类型枚举），需要整数时用 static_cast<quint8>(...)。
// =============================================================================

enum class Command : quint8
{
    // --- 上位机 -> 主控 ---
    SetState = 0x01,   // 全量状态同步：payload 为 6 字节位图（48 路开关）
    SetOne   = 0x02,   // 单路开关（调试用）：payload = boardId, channel, state
    AllOff   = 0x03,   // 全部关闭（安全保护）

    // --- 心跳 ---
    Ping     = 0x10,   // 上位机 -> 主控
    Pong     = 0x11,   // 主控 -> 上位机

    // --- 状态 ---
    GetStatus = 0x12,  // 上位机 -> 主控
    Status    = 0x13,  // 主控 -> 上位机：payload = boardId, errCode
};

inline quint8 toByte(Command cmd)
{
    return static_cast<quint8>(cmd);
}

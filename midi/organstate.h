#pragma once

#include <QByteArray>
#include <bitset>

// =============================================================================
//  OrganState.h —— 48 路电磁阀的开关状态
// -----------------------------------------------------------------------------
//  这是上位机的“当前状态表”。无论键盘发生什么，最终都落到这张表上：
//    按下某音符 -> 把对应阀置为“开”
//    松开音符   -> 把对应阀置为“关”
//  再由 Frame 把整张表（48 位 = 6 字节）打包成协议帧发给主控。
//
//  用“全量状态”而非“只发变化”：幂等、抗丢帧、无状态漂移。
// =============================================================================

class OrganState
{
public:
    static constexpr int ValveCount = 48;         // 3 板 × 16 路
    static constexpr int ChannelsPerBoard = 16;   // 每板通道数
    static constexpr int BitmapBytes = ValveCount / 8; // 6 字节

    OrganState();

    void setValve(int index, bool on); // 设置某路开关（越界忽略）
    bool valve(int index) const;       // 读取某路状态（越界返回 false）
    void allOff();                     // 全部关闭

    // 导出为 6 字节位图：字节 k 的 bit b 对应阀号 (k*8 + b)，LSB 对齐。
    QByteArray snapshot() const;

private:
    std::bitset<ValveCount> m_valves;
};

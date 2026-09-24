#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include "command.h"

class QSerialPort;

// =============================================================================
//  SerialLink.h —— 串口通信封装（上位机 <-> STM32 主控）
// -----------------------------------------------------------------------------
//  职责：
//    1. 枚举可用串口（STM32 通过 USB CDC 会显示为一个 COM 口）
//    2. 打开 / 关闭串口
//    3. 把命令打包成帧后发送
//    4. 接收字节流，切分出一帧帧，解析后通过 frameReceived 信号发出
//
//  内置“模拟模式”（无硬件调试用）：
//    打开模拟模式后，不占用真实串口，而是在本进程内扮演主控——
//    收到 PING 自动回 PONG，收到 SET_STATE 把位图打印到日志。
//    这样没有 STM32 也能验证“连接 -> 发帧 -> 收帧 -> 解析”整条链路。
// =============================================================================

class SerialLink : public QObject
{
    Q_OBJECT

public:
    explicit SerialLink(QObject *parent = nullptr);
    ~SerialLink() override;

    // 列出当前系统可用的串口名（如 "COM5"）。
    static QStringList availablePorts();

    // 是否启用模拟模式（不打开真实串口，由本进程模拟主控应答）。
    void setSimulationMode(bool on);
    bool isSimulationMode() const { return m_simulated; }

    // 打开串口。模拟模式下 portName 可忽略。baudRate 对 USB CDC 是名义值。
    bool open(const QString &portName, int baudRate = 115200);
    void close();
    bool isOpen() const;

    // 发送一条命令（内部自动打包成帧）。
    void send(Command cmd, const QByteArray &payload = QByteArray());

signals:
    void frameReceived(Command cmd, const QByteArray &payload); // 解析出一帧
    void connectionChanged(bool connected);                     // 连接状态变化
    void logMessage(const QString &text);                       // 供界面显示

private slots:
    void handleReadyRead(); // 串口有数据可读

private:
    // 模拟模式下对收到的命令做出“主控式”应答。
    void handleSimulatedFrame(Command cmd, const QByteArray &payload);
    // 把位图转成 "00 01 00 ..." 形式的十六进制字符串，便于日志查看。
    static QString bitmapToHex(const QByteArray &bitmap);

    QSerialPort *m_port = nullptr; // 真实串口对象（父对象为 this）
    QByteArray m_rxBuffer;         // 接收缓存（数据可能分批到达）
    bool m_simulated = false;      // 是否处于模拟模式
    bool m_simulatedOpen = false;  // 模拟模式下的“已打开”状态
};

/**
 * @file serialclient.h
 * @brief 主板 UART 客户端：打开 COM、发送文本帧、同步 DEBUG。
 *
 * 热路径推荐：
 *   @ST,<seq>,<t_ms>,<bitmap8hex>,<crc16>\n
 * 兼容：
 *   ALL_OFF / DEBUG ON|OFF / ON n / OFF n（调试用）
 */

#ifndef SERIALCLIENT_H
#define SERIALCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QSerialPort>
#include <QString>

class SerialClient : public QObject
{
    Q_OBJECT

public:
    /** 一帧 32 位位图能表示的最大通道数。实际键数由映射表决定。 */
    static constexpr int kMaxChannelCount = 32;

    explicit SerialClient(QObject *parent = nullptr);
    ~SerialClient() override;

    virtual bool isOpen() const;
    bool openPort(const QString &portName, int baud);
    void closePort();

    /**
     * @brief 发送一行 ASCII 并追加 '\\n'。
     * @param line 不含换行
     */
    virtual bool sendLine(const QString &line);

    /**
     * @brief 解析单条 @OK/@ERR 回报行（供接收循环与测试复用）。
     * @return 是否是可识别的确认行
     */
    static bool parseAckLine(const QByteArray &line, quint16 &seq, bool &ok, int &code);

    /**
     * @brief 兼容单键命令（优先走聚合器，一般不直接调用）。
     */
    bool sendChannelState(int channel, bool on);

    bool sendAllOff();
    bool sendRemoteDebug(bool enabled);

signals:
    void bytesReceived(const QString &text);
    void errorOccurred(const QString &text);
    void connectionChanged(bool connected);
    /**
     * 主板对 @ST 帧的确认。
     * @param seq  与 @ST 帧相同的 4 位十六进制序号
     * @param ok   true=@OK，false=@ERR
     * @param code @ERR 的错误码；@OK 时为 0
     */
    void ackReceived(quint16 seq, bool ok, int code);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    /** 从接收缓存中切出完整行并解析 @OK/@ERR。 */
    void processAckLines();

    QSerialPort *m_port = nullptr;
    QByteArray m_rxBuffer; /**< 串口数据可能分片到达，按行缓存 */
};

#endif // SERIALCLIENT_H

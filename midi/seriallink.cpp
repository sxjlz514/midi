#include "seriallink.h"

#include <QSerialPort>
#include <QSerialPortInfo>

#include "frame.h"

SerialLink::SerialLink(QObject *parent)
    : QObject(parent)
    , m_port(new QSerialPort(this)) // 以 this 为父对象，随本类销毁
{
    // 串口有数据可读时，Qt 会发出 readyRead，连接到自己的槽。
    connect(m_port, &QSerialPort::readyRead, this, &SerialLink::handleReadyRead);
}

SerialLink::~SerialLink() = default;

QStringList SerialLink::availablePorts()
{
    QStringList names;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        names << info.portName(); // 例如 "COM5"
    }
    return names;
}

void SerialLink::setSimulationMode(bool on)
{
    if (m_simulated == on) {
        return;
    }

    // 切换模式前先关闭当前连接，避免状态混乱。
    close();
    m_simulated = on;

    emit logMessage(on ? tr("已开启串口模拟模式（无需 STM32 也能调试）")
                       : tr("已关闭串口模拟模式"));
}

bool SerialLink::open(const QString &portName, int baudRate)
{
    if (m_simulated) {
        // 模拟模式：不打开真实端口，直接视为已连接。
        m_simulatedOpen = true;
        m_rxBuffer.clear();
        emit connectionChanged(true);
        emit logMessage(tr("已进入模拟模式（本机模拟主控应答）"));
        return true;
    }

    if (m_port->isOpen()) {
        m_port->close();
    }

    m_port->setPortName(portName);
    m_port->setBaudRate(baudRate);
    // 8N1：8 数据位、无校验、1 停止位（与主控固件保持一致）。
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit logMessage(tr("打开串口 %1 失败：%2").arg(portName, m_port->errorString()));
        return false;
    }

    m_rxBuffer.clear();
    emit connectionChanged(true);
    emit logMessage(tr("已打开串口 %1，波特率 %2").arg(portName).arg(baudRate));
    return true;
}

void SerialLink::close()
{
    if (m_simulated) {
        if (m_simulatedOpen) {
            m_simulatedOpen = false;
            emit connectionChanged(false);
            emit logMessage(tr("模拟连接已关闭"));
        }
        return;
    }

    if (m_port->isOpen()) {
        m_port->close();
        emit connectionChanged(false);
        emit logMessage(tr("串口已关闭"));
    }
}

bool SerialLink::isOpen() const
{
    if (m_simulated) {
        return m_simulatedOpen;
    }
    return m_port->isOpen();
}

void SerialLink::send(Command cmd, const QByteArray &payload)
{
    if (!isOpen()) {
        emit logMessage(tr("串口未打开，发送被忽略"));
        return;
    }

    if (m_simulated) {
        handleSimulatedFrame(cmd, payload);
        return;
    }

    // 真实模式：打包成帧写入串口（异步，不阻塞界面）。
    const QByteArray frame = Frame::encode(cmd, payload);
    m_port->write(frame);
}

void SerialLink::handleSimulatedFrame(Command cmd, const QByteArray &payload)
{
    switch (cmd) {
    case Command::Ping:
        // 模拟主控立即回应 PONG。
        emit frameReceived(Command::Pong, QByteArray());
        break;

    case Command::SetState:
        // 模拟主控“执行”全量状态：把 48 路位图打印出来。
        emit logMessage(tr("[模拟主控] 收到 SET_STATE：%1").arg(bitmapToHex(payload)));
        break;

    case Command::SetOne:
        emit logMessage(tr("[模拟主控] 收到 SET_ONE：%1").arg(bitmapToHex(payload)));
        break;

    case Command::AllOff:
        emit logMessage(tr("[模拟主控] 收到 ALL_OFF，全部关闭"));
        break;

    case Command::GetStatus:
        // 回一个示例状态：板 0，无错误。
        emit frameReceived(Command::Status, QByteArray{'\x00', '\x00'});
        break;

    default:
        emit logMessage(tr("[模拟主控] 收到命令 0x%1")
                            .arg(toByte(cmd), 2, 16, QLatin1Char('0')));
        break;
    }
}

QString SerialLink::bitmapToHex(const QByteArray &bitmap)
{
    // toHex(' ') 会把每个字节转成两位十六进制并用空格分隔。
    return QString::fromLatin1(bitmap.toHex(' ')).toUpper();
}

void SerialLink::handleReadyRead()
{
    // 把新到的数据追加到缓存（一帧可能分多次到达）。
    m_rxBuffer.append(m_port->readAll());

    for (;;) {
        // 开头不是帧头 SOF 就丢弃一个字节，跳过脏数据。
        while (!m_rxBuffer.isEmpty()
               && static_cast<quint8>(m_rxBuffer.at(0)) != Frame::Sof) {
            m_rxBuffer.remove(0, 1);
        }

        if (m_rxBuffer.size() < 2) {
            return; // 长度字段还没到
        }

        const int payloadLen = static_cast<quint8>(m_rxBuffer.at(1));
        const int frameLen = payloadLen + 5;
        if (m_rxBuffer.size() < frameLen) {
            return; // 帧还没收全
        }

        const QByteArray oneFrame = m_rxBuffer.left(frameLen);
        Command cmd{};
        QByteArray payload;
        if (Frame::decode(oneFrame, cmd, payload)) {
            emit frameReceived(cmd, payload);
        }
        m_rxBuffer.remove(0, frameLen);
    }
}

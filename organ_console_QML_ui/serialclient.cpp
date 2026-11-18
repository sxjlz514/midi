/**
 * @file serialclient.cpp
 * @brief SerialClient 实现：8N1 收发与通道命令封装。
 */

#include "serialclient.h"

#include <QList>

SerialClient::SerialClient(QObject *parent)
    : QObject(parent)
    , m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead, this, &SerialClient::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialClient::onSerialError);
}

SerialClient::~SerialClient()
{
    closePort();
}

bool SerialClient::isOpen() const
{
    return m_port->isOpen();
}

bool SerialClient::openPort(const QString &portName, int baud)
{
    if (m_port->isOpen()) {
        closePort();
    }

    m_rxBuffer.clear();
    m_port->setPortName(portName);
    m_port->setBaudRate(baud);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_port->errorString());
        emit connectionChanged(false);
        return false;
    }

    emit connectionChanged(true);
    return true;
}

void SerialClient::closePort()
{
    if (m_port->isOpen()) {
        m_port->close();
        m_rxBuffer.clear();
        emit connectionChanged(false);
    }
}

bool SerialClient::sendLine(const QString &line)
{
    if (!m_port->isOpen()) {
        return false;
    }
    const QByteArray payload = (line.trimmed() + QLatin1Char('\n')).toUtf8();
    const qint64 n = m_port->write(payload);
    m_port->flush();
    return n == payload.size();
}

bool SerialClient::sendChannelState(int channel, bool on)
{
    if (channel < 1 || channel > kMaxChannelCount) {
        return false;
    }
    const QString cmd = QStringLiteral("%1 %2")
                            .arg(on ? QStringLiteral("ON") : QStringLiteral("OFF"))
                            .arg(channel);
    return sendLine(cmd);
}

bool SerialClient::sendAllOff()
{
    return sendLine(QStringLiteral("ALL_OFF"));
}

bool SerialClient::sendRemoteDebug(bool enabled)
{
    return sendLine(enabled ? QStringLiteral("DEBUG ON")
                            : QStringLiteral("DEBUG OFF"));
}

void SerialClient::onReadyRead()
{
    const QByteArray data = m_port->readAll();
    if (data.isEmpty()) {
        return;
    }
    m_rxBuffer.append(data);
    emit bytesReceived(QString::fromLocal8Bit(data));
    processAckLines();
}

void SerialClient::processAckLines()
{
    while (true) {
        int end = m_rxBuffer.indexOf('\n');
        const int cr = m_rxBuffer.indexOf('\r');
        if (end < 0 || (cr >= 0 && cr < end)) {
            end = cr;
        }
        if (end < 0) {
            break;
        }
        const QByteArray line = m_rxBuffer.left(end);
        m_rxBuffer.remove(0, end + 1);
        if (line.trimmed().isEmpty()) {
            continue;
        }

        quint16 seq = 0;
        bool ok = false;
        int code = 0;
        if (parseAckLine(line, seq, ok, code)) {
            emit ackReceived(seq, ok, code);
        }
    }

    // 非协议数据（调试文本）不应无限增长；保留最后一段不完整行。
    if (m_rxBuffer.size() > 8192) {
        m_rxBuffer = m_rxBuffer.right(512);
    }
}

bool SerialClient::parseAckLine(const QByteArray &raw, quint16 &seq, bool &ok, int &code)
{
    const QByteArray line = raw.trimmed();
    if (line.isEmpty()) {
        return false;
    }

    // 帧序号与 @ST 一致，使用 4 位十六进制；错误码为十进制。
    const QByteArray tag4 = line.left(4).toUpper();
    const QByteArray tag5 = line.left(5).toUpper();
    if (tag4 != QByteArrayLiteral("@OK,") && tag5 != QByteArrayLiteral("@ERR,")) {
        return false;
    }

    const QList<QByteArray> parts = line.split(',');
    if (parts.size() < 2) {
        return false;
    }
    bool seqOk = false;
    const quint16 parsedSeq = parts.at(1).trimmed().toUShort(&seqOk, 16);
    if (!seqOk) {
        return false;
    }

    if (tag4 == QByteArrayLiteral("@OK,")) {
        seq = parsedSeq;
        ok = true;
        code = 0;
        return true;
    }

    bool codeOk = false;
    const int parsedCode = parts.size() >= 3
        ? parts.at(2).trimmed().toInt(&codeOk)
        : 0;
    seq = parsedSeq;
    ok = false;
    code = codeOk ? parsedCode : 0;
    return true;
}

void SerialClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) {
        return;
    }
    emit errorOccurred(m_port->errorString());
    if (m_port->isOpen()) {
        m_port->close();
        emit connectionChanged(false);
    }
}

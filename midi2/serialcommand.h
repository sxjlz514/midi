#pragma once

/*
 * SerialCommand —— 串口按键控制指令封装（单文件类，header-only）
 *
 * 本类把下位机（按键 / 继电器控制板）的文本指令通过串口发出去：
 *
 *     命令            作用                                示例
 *     ON <n>...       吸合指定键（可多个），n = 1..32       ON 1 17 32
 *     OFF <n>...      释放指定键                           OFF 1 17
 *     SET <n>...      仅这些键吸合，其余全关               SET 1 2 18
 *     ALL_OFF         急停，32 路全关（CAN ID 0x000）       ALL_OFF
 *     DISCOVER        广播发现，收 N1 / N2 上报            DISCOVER
 *     SYNC            广播时间基准                         SYNC
 *     START           系统开始                             START
 *     STOP            系统停止                             STOP
 *     PAUSE           系统暂停                             PAUSE
 *     STATUS          打印当前 32 位状态                   STATUS
 *     HELP 或 ?       帮助                                 HELP
 *
 * 发送格式：ASCII 文本 + 行尾，行尾默认 "\r\n"（可用 setLineEnding 改成 "\n" 等）。
 * 收到的数据按 "\r\n" / "\n" / "\r" 拆行，通过 lineReceived 信号抛出；
 * 形如 "N1 ..." / "N2 ..." 的上报行另外通过 nodeReported 信号抛出。
 *
 * 线程约定：QSerialPort 只能在它所属线程（一般是 GUI 线程）里使用。
 * 如果要在别的线程发指令，请用
 *     QMetaObject::invokeMethod(controller, [=]{ controller->allOff(); },
 *                               Qt::QueuedConnection);
 * 切回本类所属线程再调用（与 midiinput.cpp 里 WinMM 回调的处理方式一致）。
 *
 * 典型用法（Java 类比：new 一个对象 → 调方法）：
 *
 *     SerialCommand *board = new SerialCommand(this);       // this 是父对象，负责回收内存
 *     connect(board, &SerialCommand::lineReceived, this,
 *             [](const QString &line) { qDebug() << line; });
 *     connect(board, &SerialCommand::errorOccurred, this,
 *             [](const QString &text) { qDebug() << text; });
 *
 *     if (board->open(QStringLiteral("COM3"))) {            // 默认 115200 8N1
 *         board->on(QList<int>{1, 17, 32});                 // ON 1 17 32
 *         board->setOnly(QList<int>{1, 2, 18});             // SET 1 2 18
 *         board->off(17);                                   // OFF 17
 *         board->allOff();                                  // ALL_OFF
 *         board->status();                                  // STATUS，结果走 lineReceived
 *     }
 */

#include <QByteArray>
#include <QIODevice>
#include <QList>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class SerialCommand : public QObject
{
    Q_OBJECT

public:
    /* 指令种类。Java 类比：enum Command { ON, OFF, SET, ... } */
    enum class Command {
        On,             // ON  <n>...
        Off,            // OFF <n>...
        SetExclusive,   // SET <n>...
        AllOff,         // ALL_OFF
        Discover,       // DISCOVER
        Sync,           // SYNC
        Start,          // START
        Stop,           // STOP
        Pause,          // PAUSE
        Status,         // STATUS
        Help            // HELP
    };

    /* 键号范围：n = 1..32 */
    static constexpr int KeyMin = 1;
    static constexpr int KeyMax = 32;

    /* 串口默认波特率 */
    static constexpr qint32 DefaultBaudRate = 115200;

    explicit SerialCommand(QObject *parent = nullptr)
        : QObject(parent)
    {
        /* 指定 parent 后由 Qt 对象树负责 delete，不用手写析构释放 */
        m_serial = new QSerialPort(this);

        connect(
            m_serial,
            &QSerialPort::readyRead,
            this,
            &SerialCommand::onReadyRead
            );

        connect(
            m_serial,
            &QSerialPort::errorOccurred,
            this,
            &SerialCommand::onSerialError
            );
    }

    ~SerialCommand() override
    {
        close();
    }

    /* ------------------------------------------------------------------
     * 连接管理
     * ------------------------------------------------------------------ */

    /* 打开串口。默认 115200 8N1 无流控；失败返回 false 并写出原因 */
    bool open(
        const QString &portName,
        qint32 baudRate = DefaultBaudRate,
        QString *errorMessage = nullptr
        )
    {
        close();

        m_serial->setPortName(portName);
        m_serial->setBaudRate(baudRate);
        m_serial->setDataBits(QSerialPort::Data8);
        m_serial->setParity(QSerialPort::NoParity);
        m_serial->setStopBits(QSerialPort::OneStop);
        m_serial->setFlowControl(QSerialPort::NoFlowControl);

        if (!m_serial->open(QIODevice::ReadWrite)) {
            const QString text = QStringLiteral("打开串口 %1 失败：%2")
                                     .arg(portName, m_serial->errorString());
            if (errorMessage)
                *errorMessage = text;
            emit errorOccurred(text);
            return false;
        }

        m_serial->clear(QSerialPort::AllDirections);
        m_rxBuffer.clear();
        emit connectedChanged(true);
        return true;
    }

    void close()
    {
        if (!m_serial->isOpen())
            return;

        m_serial->close();
        m_rxBuffer.clear();
        emit connectedChanged(false);
    }

    bool isOpen() const
    {
        return m_serial->isOpen();
    }

    QString portName() const
    {
        return m_serial->portName();
    }

    /* 当前配置的波特率（串口未打开时返回上次设置的值） */
    qint32 baudRate() const
    {
        return m_serial->baudRate();
    }

    /* 本机可用串口名列表，例如 {"COM3", "COM5"} */
    static QStringList availablePorts()
    {
        QStringList names;
        const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo &info : infos)
            names.append(info.portName());
        return names;
    }

    /* 行尾，默认 "\r\n" */
    void setLineEnding(const QByteArray &ending)
    {
        if (!ending.isEmpty())
            m_lineEnding = ending;
    }

    QByteArray lineEnding() const
    {
        return m_lineEnding;
    }

    /* ------------------------------------------------------------------
     * 具体指令（每个开一个方法，返回是否成功发出）
     * ------------------------------------------------------------------ */

    bool on(int key)
    {
        return on(QList<int>{key});
    }

    /* ON 1 17 32 */
    bool on(const QList<int> &keys)
    {
        return sendCommand(Command::On, keys);
    }

    bool off(int key)
    {
        return off(QList<int>{key});
    }

    /* OFF 1 17 */
    bool off(const QList<int> &keys)
    {
        return sendCommand(Command::Off, keys);
    }

    /* SET 1 2 18：只有列出的键吸合，其余全关 */
    bool setOnly(const QList<int> &keys)
    {
        return sendCommand(Command::SetExclusive, keys);
    }

    /* ALL_OFF：急停，32 路全关 */
    bool allOff()
    {
        return sendCommand(Command::AllOff);
    }

    /* DISCOVER：广播发现，回复通过 lineReceived / nodeReported 抛出 */
    bool discover()
    {
        return sendCommand(Command::Discover);
    }

    /* SYNC：广播时间基准 */
    bool sync()
    {
        return sendCommand(Command::Sync);
    }

    /* START：系统开始 */
    bool start()
    {
        return sendCommand(Command::Start);
    }

    /* STOP：系统停止 */
    bool stop()
    {
        return sendCommand(Command::Stop);
    }

    /* PAUSE：系统暂停 */
    bool pause()
    {
        return sendCommand(Command::Pause);
    }

    /* STATUS：打印当前 32 位状态，回复通过 lineReceived 抛出 */
    bool status()
    {
        return sendCommand(Command::Status);
    }

    /* HELP：帮助（下位机里 "?" 是同一功能的别名） */
    bool help()
    {
        return sendCommand(Command::Help);
    }

    /* ------------------------------------------------------------------
     * 底层发送
     * ------------------------------------------------------------------ */

    /* 发送一条指令：键号会先做范围检查与去重，再自动补行尾 */
    bool sendCommand(Command command, const QList<int> &keys = {})
    {
        bool keysOk = true;
        const QList<int> checkedKeys = normalizeKeys(keys, &keysOk);
        if (!keysOk) {
            emit errorOccurred(
                QStringLiteral("%1 指令包含非法键号，只允许 %2..%3")
                    .arg(commandName(command))
                    .arg(KeyMin)
                    .arg(KeyMax)
                );
            return false;
        }

        const QByteArray payload = buildCommand(command, checkedKeys);
        if (payload.isEmpty()) {
            emit errorOccurred(
                QStringLiteral("%1 指令至少需要一个键号").arg(commandName(command))
                );
            return false;
        }

        return sendRaw(payload + m_lineEnding);
    }

    /* 发送一行自定义文本，自动补行尾 */
    bool sendLine(const QString &line)
    {
        const QString text = line.trimmed();
        if (text.isEmpty())
            return false;

        return sendRaw(text.toLatin1() + m_lineEnding);
    }

    /* 原样发送字节，不补行尾 */
    bool sendRaw(const QByteArray &data)
    {
        if (data.isEmpty())
            return false;

        if (!m_serial->isOpen()) {
            emit errorOccurred(QStringLiteral("串口未打开，指令未发送"));
            return false;
        }

        const qint64 written = m_serial->write(data);
        if (written != data.size()) {
            emit errorOccurred(
                QStringLiteral("串口写入失败：%1").arg(m_serial->errorString())
                );
            return false;
        }

        m_serial->flush();
        return true;
    }

    /* ------------------------------------------------------------------
     * 纯函数：只组包不发送（static，方便单独测试；不包含行尾）
     * ------------------------------------------------------------------ */

    static QByteArray buildCommand(Command command, const QList<int> &keys = {})
    {
        QString text = commandName(command);

        if (command == Command::On
            || command == Command::Off
            || command == Command::SetExclusive) {

            if (keys.isEmpty())
                return QByteArray();

            text += QLatin1Char(' ') + keyListText(keys);
        }

        return text.toLatin1();
    }

    static QString commandName(Command command)
    {
        switch (command) {
        case Command::On:
            return QStringLiteral("ON");
        case Command::Off:
            return QStringLiteral("OFF");
        case Command::SetExclusive:
            return QStringLiteral("SET");
        case Command::AllOff:
            return QStringLiteral("ALL_OFF");
        case Command::Discover:
            return QStringLiteral("DISCOVER");
        case Command::Sync:
            return QStringLiteral("SYNC");
        case Command::Start:
            return QStringLiteral("START");
        case Command::Stop:
            return QStringLiteral("STOP");
        case Command::Pause:
            return QStringLiteral("PAUSE");
        case Command::Status:
            return QStringLiteral("STATUS");
        case Command::Help:
            return QStringLiteral("HELP");
        }

        return QString();
    }

    static bool isValidKey(int key)
    {
        return key >= KeyMin && key <= KeyMax;
    }

    /* 去掉非法键号与重复键号，保留首次出现的顺序；ok 返回整体是否合法 */
    static QList<int> normalizeKeys(const QList<int> &keys, bool *ok = nullptr)
    {
        QList<int> result;
        bool valid = true;

        for (int key : keys) {
            if (!isValidKey(key)) {
                valid = false;
                continue;
            }

            if (!result.contains(key))
                result.append(key);
        }

        if (ok)
            *ok = valid;

        return result;
    }

    /* 把 "N1 12 34" 这样的上报行拆成节点名与正文 */
    static bool parseNodeReport(
        const QString &line,
        QString *node,
        QString *text
        )
    {
        const int spaceIndex = line.indexOf(QLatin1Char(' '));
        const QString head = (spaceIndex < 0) ? line : line.left(spaceIndex);

        if (head.size() < 2 || head.at(0) != QLatin1Char('N'))
            return false;

        for (int i = 1; i < head.size(); ++i) {
            if (!head.at(i).isDigit())
                return false;
        }

        if (node)
            *node = head;
        if (text)
            *text = (spaceIndex < 0) ? QString() : line.mid(spaceIndex + 1).trimmed();

        return true;
    }

signals:
    /* 串口打开 / 关闭 */
    void connectedChanged(bool connected);

    /* 收到一行文本（已去掉行尾与首尾空白） */
    void lineReceived(const QString &line);

    /* 收到形如 "N1 ..." / "N2 ..." 的上报，node 为 "N1" / "N2" */
    void nodeReported(const QString &node, const QString &text);

    /* 出错信息，可直接显示到界面 */
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead()
    {
        m_rxBuffer += m_serial->readAll();

        int start = 0;
        for (int i = 0; i < m_rxBuffer.size(); ++i) {
            const char ch = m_rxBuffer.at(i);
            if (ch != '\n' && ch != '\r')
                continue;

            const QString line =
                QString::fromLatin1(m_rxBuffer.mid(start, i - start)).trimmed();
            start = i + 1;

            if (!line.isEmpty())
                dispatchLine(line);
        }

        m_rxBuffer.remove(0, start);

        /* 对端一直不发换行时，防止缓冲区无限增长 */
        if (m_rxBuffer.size() > MaxRxBufferSize)
            m_rxBuffer = m_rxBuffer.right(MaxRxBufferSize);
    }

    void onSerialError(QSerialPort::SerialPortError error)
    {
        if (error == QSerialPort::NoError)
            return;

        emit errorOccurred(
            QStringLiteral("串口错误：%1").arg(m_serial->errorString())
            );

        /* 设备被拔掉 / 找不到：直接关闭并通知界面 */
        if (error == QSerialPort::ResourceError
            || error == QSerialPort::DeviceNotFoundError
            || error == QSerialPort::PermissionError) {

            close();
        }
    }

private:
    static constexpr int MaxRxBufferSize = 4096;

    void dispatchLine(const QString &line)
    {
        emit lineReceived(line);

        QString node;
        QString text;
        if (parseNodeReport(line, &node, &text))
            emit nodeReported(node, text);
    }

    static QString keyListText(const QList<int> &keys)
    {
        QStringList parts;
        parts.reserve(keys.size());

        for (int key : keys)
            parts.append(QString::number(key));

        return parts.join(QLatin1Char(' '));
    }

private:
    QSerialPort *m_serial = nullptr;
    QByteArray m_lineEnding = QByteArrayLiteral("\r\n");
    QByteArray m_rxBuffer;
};

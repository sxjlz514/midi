#include "serialpanel.h"
#include "midimessageformatter.h"
#include "serialcommand.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

namespace {

enum AutoQueryMode {
    AutoQueryOff = 0,
    AutoQueryAfterManual = 1,
    AutoQueryEvery500Ms = 2
};

const int kAutoQueryIntervalMs = 500;

QString lightStyle(bool on)
{
    return on
               ? QStringLiteral(
                     "background-color:#2ecc71;"
                     "color:#ffffff;"
                     "border-radius:3px;"
                     )
               : QStringLiteral(
                     "background-color:#d5d5d5;"
                     "color:#707070;"
                     "border-radius:3px;"
                     );
}

bool isHexDigit(QChar character)
{
    const ushort code = character.unicode();

    if (code >= '0' && code <= '9') {
        return true;
    }

    const ushort lower =
        QChar(character.toLower()).unicode();

    return lower >= 'a' && lower <= 'f';
}

/* "STATUS 0x0000FFFF" 这类回包：取 0x 后面的十六进制数 */
bool hexInLine(const QString &line, quint32 *bits)
{
    const int start =
        line.indexOf(
            QStringLiteral("0x"),
            0,
            Qt::CaseInsensitive
            );

    if (start < 0) {
        return false;
    }

    int end = start + 2;

    while (end < line.size() &&
           isHexDigit(line.at(end))) {

        ++end;
    }

    const QString hex =
        line.mid(start + 2, end - start - 2);

    if (hex.isEmpty() || hex.size() > 8) {
        return false;
    }

    bool ok = false;
    const uint value = hex.toUInt(&ok, 16);

    if (!ok) {
        return false;
    }

    if (bits) {
        *bits = value;
    }

    return true;
}

/* 恰好 32 个 0/1（允许空格、冒号、逗号等分隔） */
bool binaryInLine(const QString &line, quint32 *bits)
{
    QString compact;

    for (const QChar character : line) {
        if (character == QLatin1Char('0') ||
            character == QLatin1Char('1')) {

            compact.append(character);
            continue;
        }

        if (character.isSpace() ||
            character == QLatin1Char(':') ||
            character == QLatin1Char(',') ||
            character == QLatin1Char('_') ||
            character == QLatin1Char('-')) {

            continue;
        }

        return false;
    }

    if (compact.size() != 32) {
        return false;
    }

    quint32 value = 0;

    for (const QChar character : compact) {
        value = (value << 1) |
                (character == QLatin1Char('1') ? 1u : 0u);
    }

    if (bits) {
        *bits = value;
    }

    return true;
}

/* 整行就是一个十进制数 */
bool decimalInLine(const QString &line, quint32 *bits)
{
    bool ok = false;
    const qulonglong value = line.toULongLong(&ok, 10);

    if (!ok || value > 0xFFFFFFFFull) {
        return false;
    }

    if (bits) {
        *bits = quint32(value);
    }

    return true;
}

/*
 * 尽力解析 32 位状态。
 * 只认三种明确格式，识别不了就返回 false，由调用方显示原文。
 */
bool parseStatusBits(const QString &line, quint32 *bits)
{
    const QString text = line.trimmed();

    if (text.isEmpty()) {
        return false;
    }

    return hexInLine(text, bits) ||
           binaryInLine(text, bits) ||
           decimalInLine(text, bits);
}

/* 只对"看起来是状态回包"的行做解析，避免普通日志文本被误判 */
bool looksLikeStatus(const QString &line)
{
    if (line.contains(QStringLiteral("STATUS"), Qt::CaseInsensitive) ||
        line.contains(QStringLiteral("状态"))) {

        return true;
    }

    const QString trimmed = line.trimmed();

    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        return true;
    }

    quint32 ignored = 0;

    return binaryInLine(trimmed, &ignored);
}

}   /* namespace */

SerialPanel::SerialPanel(
    SerialCommand *serial,
    MidiKeyRouter *router,
    QWidget *parent)
    : QWidget(parent)
    , m_serial(serial)
    , m_router(router)
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *topLayout = new QHBoxLayout();
    auto *leftLayout = new QVBoxLayout();
    auto *rightLayout = new QVBoxLayout();

    leftLayout->addWidget(createManualGroup(), 1);

    rightLayout->addWidget(createStatusGroup());
    rightLayout->addWidget(createMappingGroup());
    rightLayout->addStretch();

    topLayout->addLayout(leftLayout, 1);
    topLayout->addLayout(rightLayout, 1);

    mainLayout->addLayout(topLayout, 1);
    mainLayout->addWidget(createLogGroup(), 1);

    m_autoQueryTimer = new QTimer(this);
    m_autoQueryTimer->setInterval(kAutoQueryIntervalMs);

    connect(
        m_autoQueryTimer,
        &QTimer::timeout,
        this,
        [this]() {
            if (m_serial->isOpen()) {
                m_serial->status();
            }
        }
        );

    connect(
        m_serial,
        &SerialCommand::lineReceived,
        this,
        &SerialPanel::onSerialLine
        );

    connect(
        m_serial,
        &SerialCommand::nodeReported,
        this,
        &SerialPanel::onNodeReported
        );

    connect(
        m_serial,
        &SerialCommand::errorOccurred,
        this,
        &SerialPanel::onSerialError
        );

    connect(
        m_serial,
        &SerialCommand::connectedChanged,
        this,
        &SerialPanel::onConnectedChanged
        );

    connect(
        m_router,
        &MidiKeyRouter::commandSent,
        this,
        [this](const QString &line) {
            appendTx(line, true);
        }
        );

    updateMappingHint();
    onConnectedChanged(m_serial->isOpen());
}

QGroupBox *SerialPanel::createManualGroup()
{
    auto *group =
        new QGroupBox(QStringLiteral("手动指令（勾选键号后下发）"));

    auto *layout = new QVBoxLayout(group);

    auto *grid = new QGridLayout();
    grid->setSpacing(3);

    for (int index = 0; index < SerialCommand::KeyMax; ++index) {
        auto *button =
            new QPushButton(QString::number(index + 1));

        button->setCheckable(true);
        button->setFixedSize(42, 26);

        m_keyButtons.append(button);
        grid->addWidget(button, index / 8, index % 8);
    }

    layout->addLayout(grid);

    auto *keyRow = new QHBoxLayout();

    auto *onButton = new QPushButton(QStringLiteral("ON 吸合"));
    auto *offButton = new QPushButton(QStringLiteral("OFF 释放"));
    auto *setButton = new QPushButton(QStringLiteral("SET 仅这些"));
    auto *allOffButton = new QPushButton(QStringLiteral("急停 ALL_OFF"));

    allOffButton->setStyleSheet(
        QStringLiteral(
            "QPushButton{color:#c0392b;font-weight:bold;}"
            )
        );

    keyRow->addWidget(onButton);
    keyRow->addWidget(offButton);
    keyRow->addWidget(setButton);
    keyRow->addWidget(allOffButton);
    keyRow->addStretch();

    layout->addLayout(keyRow);

    auto *systemRow = new QHBoxLayout();

    auto *discoverButton = new QPushButton(QStringLiteral("DISCOVER"));
    auto *syncButton = new QPushButton(QStringLiteral("SYNC"));
    auto *startButton = new QPushButton(QStringLiteral("START"));
    auto *stopButton = new QPushButton(QStringLiteral("STOP"));
    auto *pauseButton = new QPushButton(QStringLiteral("PAUSE"));
    auto *helpButton = new QPushButton(QStringLiteral("HELP"));

    systemRow->addWidget(discoverButton);
    systemRow->addWidget(syncButton);
    systemRow->addWidget(startButton);
    systemRow->addWidget(stopButton);
    systemRow->addWidget(pauseButton);
    systemRow->addWidget(helpButton);
    systemRow->addStretch();

    layout->addLayout(systemRow);

    m_commandButtons
        << onButton
        << offButton
        << setButton
        << allOffButton
        << discoverButton
        << syncButton
        << startButton
        << stopButton
        << pauseButton
        << helpButton;

    connect(onButton, &QPushButton::clicked, this, &SerialPanel::sendOn);
    connect(offButton, &QPushButton::clicked, this, &SerialPanel::sendOff);
    connect(setButton, &QPushButton::clicked, this, &SerialPanel::sendSet);
    connect(allOffButton, &QPushButton::clicked, this, &SerialPanel::sendAllOff);

    connect(discoverButton, &QPushButton::clicked, this, &SerialPanel::sendDiscover);
    connect(syncButton, &QPushButton::clicked, this, &SerialPanel::sendSync);
    connect(startButton, &QPushButton::clicked, this, &SerialPanel::sendStart);
    connect(stopButton, &QPushButton::clicked, this, &SerialPanel::sendStop);
    connect(pauseButton, &QPushButton::clicked, this, &SerialPanel::sendPause);
    connect(helpButton, &QPushButton::clicked, this, &SerialPanel::sendHelp);

    return group;
}

QGroupBox *SerialPanel::createStatusGroup()
{
    auto *group =
        new QGroupBox(QStringLiteral("下位机状态（STATUS）"));

    auto *layout = new QVBoxLayout(group);

    auto *row = new QHBoxLayout();

    auto *queryButton =
        new QPushButton(QStringLiteral("查询状态"));

    m_commandButtons.append(queryButton);

    m_autoQueryCombo = new QComboBox();
    m_autoQueryCombo->addItem(
        QStringLiteral("自动查询：关闭"),
        int(AutoQueryOff)
        );
    m_autoQueryCombo->addItem(
        QStringLiteral("自动查询：手动操作后"),
        int(AutoQueryAfterManual)
        );
    m_autoQueryCombo->addItem(
        QStringLiteral("自动查询：每500ms"),
        int(AutoQueryEvery500Ms)
        );
    m_autoQueryCombo->setCurrentIndex(1);

    m_stateSummaryLabel =
        new QLabel(QStringLiteral("状态：未查询"));

    row->addWidget(queryButton);
    row->addWidget(m_autoQueryCombo);
    row->addWidget(m_stateSummaryLabel, 1);

    layout->addLayout(row);

    auto *grid = new QGridLayout();
    grid->setSpacing(3);

    for (int index = 0; index < SerialCommand::KeyMax; ++index) {
        auto *light = new QLabel(QString::number(index + 1));

        light->setAlignment(Qt::AlignCenter);
        light->setFixedSize(34, 24);
        light->setStyleSheet(lightStyle(false));

        light->setToolTip(
            QStringLiteral("键%1：未知（点「查询状态」）")
                .arg(index + 1)
            );

        m_stateLights.append(light);
        grid->addWidget(light, index / 8, index % 8);
    }

    layout->addLayout(grid);

    auto *hint =
        new QLabel(
            QStringLiteral("灰=释放，绿=吸合；最低位是键1")
            );

    hint->setStyleSheet(QStringLiteral("color:#707070;"));

    layout->addWidget(hint);

    connect(
        queryButton,
        &QPushButton::clicked,
        this,
        &SerialPanel::sendStatus
        );

    connect(
        m_autoQueryCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this](int) {
            if (!m_autoQueryTimer) {
                return;
            }

            if (m_autoQueryCombo->currentData().toInt() ==
                    AutoQueryEvery500Ms &&
                m_serial->isOpen()) {

                m_autoQueryTimer->start();
            } else {
                m_autoQueryTimer->stop();
            }
        }
        );

    return group;
}

QGroupBox *SerialPanel::createMappingGroup()
{
    auto *group =
        new QGroupBox(QStringLiteral("MIDI → 键号映射"));

    auto *layout = new QVBoxLayout(group);

    auto *row = new QHBoxLayout();

    row->addWidget(new QLabel(QStringLiteral("基准音符：")));

    m_baseNoteSpin = new QSpinBox();
    m_baseNoteSpin->setRange(0, 95);
    m_baseNoteSpin->setValue(0);

    row->addWidget(m_baseNoteSpin);

    auto *useNoteButton =
        new QPushButton(QStringLiteral("用刚收到的音作基准"));

    row->addWidget(useNoteButton);
    row->addStretch();

    layout->addLayout(row);

    m_baseNoteHint = new QLabel();
    m_baseNoteHint->setWordWrap(true);

    layout->addWidget(m_baseNoteHint);

    auto *channelRow = new QHBoxLayout();

    channelRow->addWidget(new QLabel(QStringLiteral("接受通道：")));

    m_channelCombo = new QComboBox();
    m_channelCombo->addItem(QStringLiteral("全部通道"), -1);

    for (int channel = 1; channel <= 16; ++channel) {
        m_channelCombo->addItem(
            QStringLiteral("通道%1").arg(channel),
            channel
            );
    }

    channelRow->addWidget(m_channelCombo);

    m_lastNoteLabel =
        new QLabel(QStringLiteral("最近收到：—"));

    channelRow->addWidget(m_lastNoteLabel, 1);

    layout->addLayout(channelRow);

    connect(
        m_baseNoteSpin,
        &QSpinBox::valueChanged,
        this,
        &SerialPanel::onMappingWidgetChanged
        );

    connect(
        m_channelCombo,
        &QComboBox::currentIndexChanged,
        this,
        &SerialPanel::onMappingWidgetChanged
        );

    connect(
        useNoteButton,
        &QPushButton::clicked,
        this,
        &SerialPanel::useLastNoteAsBase
        );

    return group;
}

QGroupBox *SerialPanel::createLogGroup()
{
    auto *group =
        new QGroupBox(QStringLiteral("串口收发日志"));

    auto *layout = new QVBoxLayout(group);

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(5000);

    m_logEdit->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
            )
        );

    layout->addWidget(m_logEdit);

    return group;
}

MidiKeyMapping SerialPanel::mapping() const
{
    MidiKeyMapping result;

    result.baseNote = m_baseNoteSpin->value();
    result.channel = m_channelCombo->currentData().toInt();

    return result;
}

bool SerialPanel::parseStatusLine(
    const QString &line,
    quint32 *bits)
{
    return parseStatusBits(line, bits);
}

void SerialPanel::appendLog(const QString &line)
{
    m_logEdit->appendPlainText(line);
}

void SerialPanel::appendTx(const QString &line, bool ok)
{
    appendLog(
        ok
            ? QStringLiteral("TX: %1").arg(line)
            : QStringLiteral("!! 发送失败：%1").arg(line)
        );
}

QList<int> SerialPanel::checkedKeys() const
{
    QList<int> keys;

    for (int index = 0; index < m_keyButtons.size(); ++index) {
        if (m_keyButtons.at(index)->isChecked()) {
            keys.append(index + 1);
        }
    }

    return keys;
}

void SerialPanel::clearKeySelection()
{
    for (QPushButton *button : m_keyButtons) {
        button->setChecked(false);
    }
}

void SerialPanel::maybeAutoQuery()
{
    if (m_autoQueryCombo->currentData().toInt() ==
        AutoQueryAfterManual) {

        m_serial->status();
    }
}

void SerialPanel::sendOn()
{
    const QList<int> keys = checkedKeys();

    if (keys.isEmpty()) {
        appendLog(QStringLiteral("!! 请先勾选键号"));
        return;
    }

    m_router->pressKeys(keys);
    maybeAutoQuery();
}

void SerialPanel::sendOff()
{
    const QList<int> keys = checkedKeys();

    if (keys.isEmpty()) {
        appendLog(QStringLiteral("!! 请先勾选键号"));
        return;
    }

    m_router->releaseKeys(keys);
    maybeAutoQuery();
}

void SerialPanel::sendSet()
{
    const QList<int> keys = checkedKeys();

    if (keys.isEmpty()) {
        appendLog(QStringLiteral("!! 请先勾选键号"));
        return;
    }

    m_router->setOnlyKeys(keys);
    maybeAutoQuery();
}

void SerialPanel::sendAllOff()
{
    m_router->releaseAll();
    clearKeySelection();
    maybeAutoQuery();
}

void SerialPanel::sendDiscover()
{
    appendTx(
        QStringLiteral("DISCOVER"),
        m_serial->discover()
        );
}

void SerialPanel::sendSync()
{
    appendTx(
        QStringLiteral("SYNC"),
        m_serial->sync()
        );
}

void SerialPanel::sendStart()
{
    appendTx(
        QStringLiteral("START"),
        m_serial->start()
        );
}

void SerialPanel::sendStop()
{
    appendTx(
        QStringLiteral("STOP"),
        m_serial->stop()
        );
}

void SerialPanel::sendPause()
{
    appendTx(
        QStringLiteral("PAUSE"),
        m_serial->pause()
        );
}

void SerialPanel::sendHelp()
{
    appendTx(
        QStringLiteral("HELP"),
        m_serial->help()
        );
}

void SerialPanel::sendStatus()
{
    appendTx(
        QStringLiteral("STATUS"),
        m_serial->status()
        );
}

void SerialPanel::onMappingWidgetChanged()
{
    updateMappingHint();

    emit mappingChanged(mapping());
}

void SerialPanel::updateMappingHint()
{
    const int baseNote = m_baseNoteSpin->value();

    m_baseNoteHint->setText(
        QStringLiteral("%1 = 键1；%2 = 键32；算出的键号不在1..32的音符会被丢弃")
            .arg(MidiMessageFormatter::noteName(baseNote))
            .arg(MidiMessageFormatter::noteName(baseNote + 31))
        );
}

void SerialPanel::useLastNoteAsBase()
{
    if (m_lastNote < 0) {
        appendLog(
            QStringLiteral("!! 还没收到MIDI音符，先在页签1弹一下最低音")
            );

        return;
    }

    m_baseNoteSpin->setValue(m_lastNote);

    appendLog(
        QStringLiteral("已把基准音符设为 %1")
            .arg(MidiMessageFormatter::noteName(m_lastNote))
        );
}

void SerialPanel::observeMidiMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32)
{
    const quint8 command = status & 0xF0;

    if (command != 0x90 || data2 == 0) {
        return;
    }

    m_lastNote = int(data1);

    m_lastNoteLabel->setText(
        QStringLiteral("最近收到：%1")
            .arg(MidiMessageFormatter::noteName(m_lastNote))
        );
}

void SerialPanel::onSerialLine(const QString &line)
{
    appendLog(QStringLiteral("RX: %1").arg(line));

    if (!looksLikeStatus(line)) {
        return;
    }

    quint32 bits = 0;

    if (!parseStatusBits(line, &bits)) {
        m_stateSummaryLabel->setText(
            QStringLiteral("状态：回包格式未识别（原文见日志）")
            );

        return;
    }

    updateStateLights(bits);

    emit statusReceived(bits);
}

void SerialPanel::onNodeReported(
    const QString &node,
    const QString &text)
{
    appendLog(
        QStringLiteral("RX: 节点%1 上报 %2")
            .arg(
                node,
                text.isEmpty()
                    ? QStringLiteral("(无内容)")
                    : text
                )
        );
}

void SerialPanel::onSerialError(const QString &message)
{
    appendLog(QStringLiteral("!! %1").arg(message));
}

void SerialPanel::onConnectedChanged(bool connected)
{
    for (QPushButton *button : m_commandButtons) {
        button->setEnabled(connected);
    }

    if (connected) {
        appendLog(
            QStringLiteral("串口已连接：%1 @%2")
                .arg(m_serial->portName())
                .arg(m_serial->baudRate())
            );

        if (m_autoQueryTimer &&
            m_autoQueryCombo->currentData().toInt() ==
                AutoQueryEvery500Ms) {

            m_autoQueryTimer->start();
        }
    } else {
        appendLog(QStringLiteral("串口已断开"));

        if (m_autoQueryTimer) {
            m_autoQueryTimer->stop();
        }

        m_stateSummaryLabel->setText(
            QStringLiteral("状态：未连接")
            );
    }

    emit connectedChanged(connected);
}

void SerialPanel::updateStateLights(quint32 bits)
{
    QStringList pressedKeys;

    for (int index = 0; index < m_stateLights.size(); ++index) {
        const bool on = ((bits >> index) & 0x1u) != 0;

        m_stateLights.at(index)->setStyleSheet(lightStyle(on));

        m_stateLights.at(index)->setToolTip(
            QStringLiteral("键%1：%2")
                .arg(index + 1)
                .arg(on
                         ? QStringLiteral("吸合")
                         : QStringLiteral("释放"))
            );

        if (on) {
            pressedKeys.append(QString::number(index + 1));
        }
    }

    if (pressedKeys.isEmpty()) {
        m_stateSummaryLabel->setText(
            QStringLiteral("状态：32路全部释放")
            );
    } else {
        m_stateSummaryLabel->setText(
            QStringLiteral("状态：吸合键 %1")
                .arg(pressedKeys.join(QLatin1Char(',')))
            );
    }
}

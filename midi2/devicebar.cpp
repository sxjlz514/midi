#include "devicebar.h"
#include "midiinput.h"
#include "serialcommand.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/* 串口下拉里可选的波特率 */
const int kBaudRates[] = {
    9600,
    19200,
    38400,
    57600,
    115200
};

}   /* namespace */

DeviceBar::DeviceBar(
    MidiInput *midi,
    SerialCommand *serial,
    QWidget *parent)
    : QWidget(parent)
    , m_midi(midi)
    , m_serial(serial)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 4);
    mainLayout->setSpacing(4);

    mainLayout->addWidget(createMidiRow());
    mainLayout->addWidget(createSerialRow());

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    mainLayout->addWidget(separator);

    connect(
        m_midiRefreshButton,
        &QPushButton::clicked,
        this,
        &DeviceBar::refreshMidiDevices
        );

    connect(
        m_midiOpenButton,
        &QPushButton::clicked,
        this,
        &DeviceBar::toggleMidiDevice
        );

    connect(
        m_serialRefreshButton,
        &QPushButton::clicked,
        this,
        &DeviceBar::refreshSerialPorts
        );

    connect(
        m_serialOpenButton,
        &QPushButton::clicked,
        this,
        &DeviceBar::toggleSerialPort
        );

    connect(
        m_queryButton,
        &QPushButton::clicked,
        this,
        &DeviceBar::queryStatus
        );

    connect(
        m_allOffButton,
        &QPushButton::clicked,
        this,
        &DeviceBar::allOffRequested
        );

    connect(
        m_serial,
        &SerialCommand::connectedChanged,
        this,
        &DeviceBar::onSerialConnectedChanged
        );

    connect(
        m_serial,
        &SerialCommand::errorOccurred,
        this,
        &DeviceBar::onSerialError
        );

    refreshMidiDevices();
    refreshSerialPorts();
}

QWidget *DeviceBar::createMidiRow()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    m_midiCombo = new QComboBox;
    m_midiCombo->setMinimumWidth(220);

    m_midiRefreshButton =
        new QPushButton(QStringLiteral("刷新设备"));

    m_midiOpenButton =
        new QPushButton(QStringLiteral("打开设备"));

    m_midiStatusLabel =
        new QLabel(QStringLiteral("状态：未打开"));

    layout->addWidget(new QLabel(QStringLiteral("MIDI输入设备：")));
    layout->addWidget(m_midiCombo, 1);
    layout->addWidget(m_midiRefreshButton);
    layout->addWidget(m_midiOpenButton);
    layout->addWidget(m_midiStatusLabel, 1);

    return row;
}

QWidget *DeviceBar::createSerialRow()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    m_serialCombo = new QComboBox;
    m_serialCombo->setMinimumWidth(160);

    m_serialRefreshButton =
        new QPushButton(QStringLiteral("刷新"));

    m_serialOpenButton =
        new QPushButton(QStringLiteral("打开串口"));

    m_baudCombo = new QComboBox;

    for (int baudRate : kBaudRates) {
        m_baudCombo->addItem(
            QString::number(baudRate),
            baudRate
            );
    }

    const int defaultIndex =
        m_baudCombo->findData(
            int(SerialCommand::DefaultBaudRate)
            );

    if (defaultIndex >= 0) {
        m_baudCombo->setCurrentIndex(defaultIndex);
    }

    m_queryButton =
        new QPushButton(QStringLiteral("查询状态"));

    m_allOffButton =
        new QPushButton(QStringLiteral("急停 ALL_OFF"));

    m_allOffButton->setStyleSheet(
        QStringLiteral(
            "QPushButton{color:#c0392b;font-weight:bold;}"
            )
        );

    m_serialStatusLabel =
        new QLabel(QStringLiteral("状态：未连接"));

    layout->addWidget(new QLabel(QStringLiteral("串口：")));
    layout->addWidget(m_serialCombo, 1);
    layout->addWidget(m_serialRefreshButton);
    layout->addWidget(m_serialOpenButton);
    layout->addWidget(new QLabel(QStringLiteral("波特率：")));
    layout->addWidget(m_baudCombo);
    layout->addWidget(m_queryButton);
    layout->addWidget(m_allOffButton);
    layout->addWidget(m_serialStatusLabel, 1);

    return row;
}

void DeviceBar::refreshMidiDevices()
{
    if (m_midi->isOpen()) {
        m_midi->close();

        m_midiOpenButton->setText(QStringLiteral("打开设备"));
        m_midiStatusLabel->setText(QStringLiteral("状态：已关闭"));
    }

    m_midiCombo->clear();

    const QStringList devices = m_midi->devices();

    int preferredIndex = -1;

    for (int index = 0; index < devices.size(); ++index) {
        const QString deviceName = devices.at(index);

        m_midiCombo->addItem(
            QStringLiteral("%1：%2")
                .arg(index)
                .arg(deviceName),
            index
            );

        /*
         * 优先选择主端口SMK25Mini，
         * 暂时不选择MIDIIN2辅助端口。
         */
        if (preferredIndex < 0 &&
            deviceName.compare(
                QStringLiteral("SMK25Mini"),
                Qt::CaseInsensitive
                ) == 0) {

            preferredIndex = index;
        }
    }

    if (preferredIndex >= 0 &&
        preferredIndex < m_midiCombo->count()) {

        m_midiCombo->setCurrentIndex(preferredIndex);
    }

    if (devices.isEmpty()) {
        m_midiStatusLabel->setText(
            QStringLiteral("状态：没有检测到MIDI输入设备")
            );
    } else {
        m_midiStatusLabel->setText(
            QStringLiteral("状态：检测到%1个MIDI输入端口")
                .arg(devices.size())
            );
    }
}

void DeviceBar::refreshSerialPorts()
{
    /* 串口打开时不刷新，避免下拉内容和实际打开的口不一致 */
    if (m_serial->isOpen()) {
        return;
    }

    const QString currentPort = m_serialCombo->currentText();

    m_serialCombo->clear();

    const QStringList ports =
        SerialCommand::availablePorts();

    m_serialCombo->addItems(ports);

    const int currentIndex =
        m_serialCombo->findText(currentPort);

    if (currentIndex >= 0) {
        m_serialCombo->setCurrentIndex(currentIndex);
    }

    if (ports.isEmpty()) {
        m_serialStatusLabel->setText(
            QStringLiteral("状态：没有检测到串口")
            );
    } else {
        m_serialStatusLabel->setText(
            QStringLiteral("状态：检测到%1个串口")
                .arg(ports.size())
            );
    }
}

void DeviceBar::toggleMidiDevice()
{
    if (m_midi->isOpen()) {
        m_midi->close();

        m_midiOpenButton->setText(QStringLiteral("打开设备"));

        m_midiRefreshButton->setEnabled(true);
        m_midiCombo->setEnabled(true);

        m_midiStatusLabel->setText(QStringLiteral("状态：已关闭"));

        return;
    }

    if (m_midiCombo->currentIndex() < 0) {
        m_midiStatusLabel->setText(
            QStringLiteral("状态：请选择MIDI输入设备")
            );

        return;
    }

    const unsigned int deviceIndex =
        m_midiCombo->currentData().toUInt();

    QString errorMessage;

    if (!m_midi->open(deviceIndex, &errorMessage)) {
        m_midiStatusLabel->setText(
            QStringLiteral("状态：打开失败——%1")
                .arg(errorMessage)
            );

        return;
    }

    m_midiOpenButton->setText(QStringLiteral("关闭设备"));

    m_midiRefreshButton->setEnabled(false);
    m_midiCombo->setEnabled(false);

    m_midiStatusLabel->setText(
        QStringLiteral("状态：已打开 %1")
            .arg(m_midiCombo->currentText())
        );
}

void DeviceBar::toggleSerialPort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        return;
    }

    if (m_serialCombo->currentIndex() < 0) {
        m_serialStatusLabel->setText(
            QStringLiteral("状态：请选择串口")
            );

        return;
    }

    const QString portName = m_serialCombo->currentText();
    const qint32 baudRate = m_baudCombo->currentData().toInt();

    QString errorMessage;

    if (!m_serial->open(portName, baudRate, &errorMessage)) {
        m_serialStatusLabel->setText(
            QStringLiteral("状态：打开失败——%1")
                .arg(errorMessage)
            );

        return;
    }
}

void DeviceBar::queryStatus()
{
    m_serial->status();
}

void DeviceBar::onSerialConnectedChanged(bool connected)
{
    if (connected) {
        m_serialOpenButton->setText(QStringLiteral("关闭串口"));

        m_serialCombo->setEnabled(false);
        m_serialRefreshButton->setEnabled(false);
        m_baudCombo->setEnabled(false);

        m_serialStatusLabel->setStyleSheet(QString());

        m_serialStatusLabel->setText(
            QStringLiteral("状态：已打开 %1 @%2")
                .arg(m_serial->portName())
                .arg(m_serial->baudRate())
            );

        return;
    }

    m_serialOpenButton->setText(QStringLiteral("打开串口"));

    m_serialCombo->setEnabled(true);
    m_serialRefreshButton->setEnabled(true);
    m_baudCombo->setEnabled(true);

    m_serialStatusLabel->setText(QStringLiteral("状态：未连接"));
}

void DeviceBar::onSerialError(const QString &message)
{
    m_serialStatusLabel->setStyleSheet(
        QStringLiteral("color:#c0392b;")
        );

    m_serialStatusLabel->setText(
        QStringLiteral("状态：%1").arg(message)
        );
}

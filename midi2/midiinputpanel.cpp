#include "midiinputpanel.h"
#include "midiinput.h"
#include "midimessageformatter.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

MidiInputPanel::MidiInputPanel(QWidget *parent)
    : QWidget(parent)
    , m_midiInput(new MidiInput(this))
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *deviceLayout = new QHBoxLayout();

    auto *deviceLabel =
        new QLabel(QStringLiteral("MIDI输入设备："));

    m_deviceCombo = new QComboBox;
    m_refreshButton =
        new QPushButton(QStringLiteral("刷新设备"));

    m_openButton =
        new QPushButton(QStringLiteral("打开设备"));

    deviceLayout->addWidget(deviceLabel);
    deviceLayout->addWidget(m_deviceCombo, 1);
    deviceLayout->addWidget(m_refreshButton);
    deviceLayout->addWidget(m_openButton);

    m_statusLabel =
        new QLabel(QStringLiteral("状态：未打开"));

    m_countLabel =
        new QLabel(QStringLiteral("消息数量：0"));

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(5000);

    m_logEdit->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
            )
        );

    mainLayout->addLayout(deviceLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_countLabel);
    mainLayout->addWidget(m_logEdit, 1);

    connect(
        m_refreshButton,
        &QPushButton::clicked,
        this,
        &MidiInputPanel::refreshDevices
        );

    connect(
        m_openButton,
        &QPushButton::clicked,
        this,
        &MidiInputPanel::toggleMidiDevice
        );

    connect(
        m_midiInput,
        &MidiInput::messageReceived,
        this,
        &MidiInputPanel::onMidiMessage
        );

    refreshDevices();
}

MidiInputPanel::~MidiInputPanel()
{
    m_midiInput->close();
}

QPlainTextEdit *MidiInputPanel::logEdit() const
{
    return m_logEdit;
}

void MidiInputPanel::appendLog(const QString &line)
{
    m_logEdit->appendPlainText(line);
}

void MidiInputPanel::clearLog()
{
    m_logEdit->clear();
}

MidiInput *MidiInputPanel::input() const
{
    return m_midiInput;
}

void MidiInputPanel::refreshDevices()
{
    if (m_midiInput->isOpen()) {
        m_midiInput->close();

        m_openButton->setText(
            QStringLiteral("打开设备")
            );

        m_statusLabel->setText(
            QStringLiteral("状态：已关闭")
            );
    }

    m_deviceCombo->clear();

    const QStringList devices =
        m_midiInput->devices();

    int preferredIndex = -1;

    for (int index = 0;
         index < devices.size();
         ++index) {

        const QString deviceName =
            devices.at(index);

        m_deviceCombo->addItem(
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

    if (preferredIndex >= 0) {
        m_deviceCombo->setCurrentIndex(
            preferredIndex
            );
    }

    if (devices.isEmpty()) {
        m_statusLabel->setText(
            QStringLiteral(
                "状态：没有检测到MIDI输入设备"
                )
            );
    } else {
        m_statusLabel->setText(
            QStringLiteral(
                "状态：检测到%1个MIDI输入端口"
                ).arg(devices.size())
            );
    }
}

void MidiInputPanel::toggleMidiDevice()
{
    if (m_midiInput->isOpen()) {
        m_midiInput->close();

        m_openButton->setText(
            QStringLiteral("打开设备")
            );

        m_refreshButton->setEnabled(true);
        m_deviceCombo->setEnabled(true);

        m_statusLabel->setText(
            QStringLiteral("状态：已关闭")
            );

        m_logEdit->appendPlainText(
            QStringLiteral("MIDI设备已关闭")
            );

        return;
    }

    if (m_deviceCombo->currentIndex() < 0) {
        m_statusLabel->setText(
            QStringLiteral(
                "状态：请选择MIDI输入设备"
                )
            );

        return;
    }

    const unsigned int deviceIndex =
        m_deviceCombo
            ->currentData()
            .toUInt();

    QString errorMessage;

    if (!m_midiInput->open(
            deviceIndex,
            &errorMessage)) {

        m_statusLabel->setText(
            QStringLiteral(
                "状态：打开失败——%1"
                ).arg(errorMessage)
            );

        return;
    }

    const QString deviceName =
        m_deviceCombo->currentText();

    m_openButton->setText(
        QStringLiteral("关闭设备")
        );

    m_refreshButton->setEnabled(false);
    m_deviceCombo->setEnabled(false);

    m_statusLabel->setText(
        QStringLiteral(
            "状态：已打开 %1"
            ).arg(deviceName)
        );

    m_logEdit->appendPlainText(
        QStringLiteral(
            "已打开MIDI输入：%1"
            ).arg(deviceName)
        );
}

void MidiInputPanel::onMidiMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    m_messageCount++;

    m_countLabel->setText(
        QStringLiteral("消息数量：%1")
            .arg(m_messageCount)
        );

    m_logEdit->appendPlainText(
        MidiMessageFormatter::formatLine(
            status,
            data1,
            data2,
            timestampMs
            )
        );

    emit received(
        status,
        data1,
        data2,
        timestampMs
        );
}

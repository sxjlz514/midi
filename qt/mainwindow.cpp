#include "mainwindow.h"
#include "midiinput.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_midiInput(new MidiInput(this))
{
    setWindowTitle(
        QStringLiteral("SMK25Mini MIDI输入测试")
        );

    resize(850, 560);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
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

    setCentralWidget(centralWidget);

    connect(
        m_refreshButton,
        &QPushButton::clicked,
        this,
        &MainWindow::refreshDevices
        );

    connect(
        m_openButton,
        &QPushButton::clicked,
        this,
        &MainWindow::toggleMidiDevice
        );

    connect(
        m_midiInput,
        &MidiInput::messageReceived,
        this,
        &MainWindow::onMidiMessage
        );

    refreshDevices();
}

MainWindow::~MainWindow()
{
    m_midiInput->close();
}

void MainWindow::refreshDevices()
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

void MainWindow::toggleMidiDevice()
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

void MainWindow::onMidiMessage(
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

    const quint8 command =
        status & 0xF0;

    const int channel =
        (status & 0x0F) + 1;

    QString description;

    switch (command) {
    case 0x80:
        description = QStringLiteral(
                          "Note Off  %1  Velocity=%2"
                          )
                          .arg(noteName(data1))
                          .arg(data2);
        break;

    case 0x90:
        if (data2 == 0) {
            description = QStringLiteral(
                              "Note Off  %1  Velocity=0"
                              ).arg(noteName(data1));
        } else {
            description = QStringLiteral(
                              "Note On   %1  Velocity=%2"
                              )
                              .arg(noteName(data1))
                              .arg(data2);
        }
        break;

    case 0xA0:
        description = QStringLiteral(
                          "Poly Aftertouch  Note=%1 Value=%2"
                          )
                          .arg(noteName(data1))
                          .arg(data2);
        break;

    case 0xB0:
        description = QStringLiteral(
                          "Control Change  CC=%1 Value=%2"
                          )
                          .arg(data1)
                          .arg(data2);
        break;

    case 0xC0:
        description = QStringLiteral(
                          "Program Change  Program=%1"
                          ).arg(data1);
        break;

    case 0xD0:
        description = QStringLiteral(
                          "Channel Pressure  Value=%1"
                          ).arg(data1);
        break;

    case 0xE0: {
        const int rawValue =
            (static_cast<int>(data2) << 7) |
            data1;

        const int signedValue =
            rawValue - 8192;

        description = QStringLiteral(
                          "Pitch Bend  Value=%1"
                          ).arg(signedValue);
        break;
    }

    default:
        description = QStringLiteral(
            "System/Unknown Message"
            );
        break;
    }

    const QString line = QStringLiteral(
                             "[%1 ms] CH=%2  %3  RAW=%4 %5 %6"
                             )
                             .arg(timestampMs, 8)
                             .arg(channel, 2)
                             .arg(description)
                             .arg(status, 2, 16, QChar('0'))
                             .arg(data1, 2, 16, QChar('0'))
                             .arg(data2, 2, 16, QChar('0'))
                             .toUpper();

    m_logEdit->appendPlainText(line);
}

QString MainWindow::noteName(int midiNote)
{
    static const char *names[] = {
        "C", "C#", "D", "D#",
        "E", "F", "F#", "G",
        "G#", "A", "A#", "B"
    };

    if (midiNote < 0 || midiNote > 127) {
        return QStringLiteral("Invalid");
    }

    const int octave =
        midiNote / 12 - 1;

    const int noteIndex =
        midiNote % 12;

    return QStringLiteral("%1%2 (MIDI %3)")
        .arg(
            QString::fromLatin1(
                names[noteIndex]
                )
            )
        .arg(octave)
        .arg(midiNote);
}

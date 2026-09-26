#include "mainwindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "appcontroller.h"
#include "midiinput.h"
#include "seriallink.h"
#include "valvepanel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new AppController(this))
{
    setWindowTitle(QStringLiteral("管风琴 MIDI 上位机"));
    resize(900, 640);

    buildUi();
    buildMenus();

    // --- 把业务信号接到界面 ---
    connect(m_controller, &AppController::valveStateChanged,
            m_valvePanel, &ValvePanel::setState);
    connect(m_controller, &AppController::logMessage,
            this, &MainWindow::appendLog);

    // MIDI 事件与日志
    connect(m_controller->midi(), &MidiInput::midiEvent,
            this, &MainWindow::onMidiEvent);
    connect(m_controller->midi(), &MidiInput::logMessage,
            this, &MainWindow::appendLog);

    // 串口连接状态
    connect(m_controller->serial(), &SerialLink::connectionChanged,
            this, &MainWindow::updateSerialStatus);

    refreshMidiDevices();
    refreshSerialPorts();

    appendLog(QStringLiteral("程序已启动。可勾选“模拟模式”在没有 STM32 的情况下调试。"));
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // --- MIDI 设备行 ---
    auto *deviceLayout = new QHBoxLayout();
    deviceLayout->addWidget(new QLabel(QStringLiteral("MIDI输入设备：")));
    m_deviceCombo = new QComboBox;
    m_refreshButton = new QPushButton(QStringLiteral("刷新设备"));
    m_openButton = new QPushButton(QStringLiteral("打开设备"));
    deviceLayout->addWidget(m_deviceCombo, 1);
    deviceLayout->addWidget(m_refreshButton);
    deviceLayout->addWidget(m_openButton);

    // --- 串口行 ---
    auto *serialLayout = new QHBoxLayout();
    serialLayout->addWidget(new QLabel(QStringLiteral("串口(主控)：")));
    m_serialCombo = new QComboBox;
    m_serialRefreshButton = new QPushButton(QStringLiteral("刷新串口"));
    m_serialOpenButton = new QPushButton(QStringLiteral("连接"));
    m_simulationCheck = new QCheckBox(QStringLiteral("模拟模式"));
    serialLayout->addWidget(m_serialCombo, 1);
    serialLayout->addWidget(m_serialRefreshButton);
    serialLayout->addWidget(m_serialOpenButton);
    serialLayout->addWidget(m_simulationCheck);

    // --- 状态标签 ---
    m_midiStatusLabel = new QLabel(QStringLiteral("状态：未打开"));
    m_countLabel = new QLabel(QStringLiteral("消息数量：0"));
    m_serialStatusLabel = new QLabel(QStringLiteral("串口：未连接"));

    auto *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_midiStatusLabel);
    statusLayout->addStretch(1);
    statusLayout->addWidget(m_countLabel);
    statusLayout->addSpacing(24);
    statusLayout->addWidget(m_serialStatusLabel);

    // --- 阀状态面板 ---
    m_valvePanel = new ValvePanel;

    // --- 日志 ---
    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(5000);
    m_logEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    mainLayout->addLayout(deviceLayout);
    mainLayout->addLayout(serialLayout);
    mainLayout->addLayout(statusLayout);
    mainLayout->addWidget(m_valvePanel);
    mainLayout->addWidget(new QLabel(QStringLiteral("日志：")));
    mainLayout->addWidget(m_logEdit, 1);

    setCentralWidget(centralWidget);

    // --- 信号连接 ---
    connect(m_refreshButton, &QPushButton::clicked,
            this, &MainWindow::refreshMidiDevices);
    connect(m_openButton, &QPushButton::clicked,
            this, &MainWindow::toggleMidiDevice);
    connect(m_serialRefreshButton, &QPushButton::clicked,
            this, &MainWindow::refreshSerialPorts);
    connect(m_serialOpenButton, &QPushButton::clicked,
            this, &MainWindow::toggleSerial);
    connect(m_simulationCheck, &QCheckBox::toggled,
            this, &MainWindow::onSimulationToggled);
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(QStringLiteral("退出(&Q)"),
                        QKeySequence(QStringLiteral("Ctrl+Q")),
                        this, &QWidget::close);

    QMenu *testMenu = menuBar()->addMenu(QStringLiteral("测试(&T)"));
    testMenu->addAction(QStringLiteral("手动开关某一路..."),
                        this, &MainWindow::onTestValve);
    testMenu->addAction(QStringLiteral("全部关闭"),
                        this, &MainWindow::onAllOff);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于..."), this, &MainWindow::onAbout);
}

void MainWindow::refreshMidiDevices()
{
    MidiInput *midi = m_controller->midi();

    if (midi->isOpen()) {
        midi->close();
        m_openButton->setText(QStringLiteral("打开设备"));
    }

    m_deviceCombo->clear();
    const QStringList devices = midi->devices();

    for (int index = 0; index < devices.size(); ++index) {
        m_deviceCombo->addItem(
            QStringLiteral("%1：%2").arg(index).arg(devices.at(index)), index);
    }

    if (devices.isEmpty()) {
        m_midiStatusLabel->setText(QStringLiteral("状态：没有检测到MIDI输入设备"));
    } else {
        m_midiStatusLabel->setText(
            QStringLiteral("状态：检测到%1个MIDI输入端口").arg(devices.size()));
    }
}

void MainWindow::toggleMidiDevice()
{
    MidiInput *midi = m_controller->midi();

    if (midi->isOpen()) {
        midi->close();
        m_openButton->setText(QStringLiteral("打开设备"));
        m_refreshButton->setEnabled(true);
        m_deviceCombo->setEnabled(true);
        m_midiStatusLabel->setText(QStringLiteral("状态：已关闭"));
        appendLog(QStringLiteral("MIDI设备已关闭"));
        return;
    }

    if (m_deviceCombo->currentIndex() < 0) {
        m_midiStatusLabel->setText(QStringLiteral("状态：请选择MIDI输入设备"));
        return;
    }

    const unsigned int deviceIndex = m_deviceCombo->currentData().toUInt();
    QString errorMessage;

    if (!midi->open(deviceIndex, &errorMessage)) {
        m_midiStatusLabel->setText(
            QStringLiteral("状态：打开失败——%1").arg(errorMessage));
        return;
    }

    const QString deviceName = m_deviceCombo->currentText();
    m_openButton->setText(QStringLiteral("关闭设备"));
    m_refreshButton->setEnabled(false);
    m_deviceCombo->setEnabled(false);
    m_midiStatusLabel->setText(QStringLiteral("状态：已打开 %1").arg(deviceName));
    appendLog(QStringLiteral("已打开MIDI输入：%1").arg(deviceName));
}

void MainWindow::refreshSerialPorts()
{
    m_serialCombo->clear();
    const QStringList ports = SerialLink::availablePorts();
    m_serialCombo->addItems(ports);

    if (ports.isEmpty()) {
        appendLog(QStringLiteral("未发现串口。可勾选“模拟模式”进行无硬件调试。"));
    }
}

void MainWindow::toggleSerial()
{
    SerialLink *serial = m_controller->serial();

    if (serial->isOpen()) {
        m_controller->disconnectSerial();
        m_serialOpenButton->setText(QStringLiteral("连接"));
        m_serialCombo->setEnabled(true);
        m_serialRefreshButton->setEnabled(true);
        return;
    }

    // 模拟模式下不需要真实端口，直接连接。
    if (serial->isSimulationMode()) {
        m_controller->connectSerial(QString());
    } else {
        if (m_serialCombo->currentIndex() < 0) {
            QMessageBox::warning(this, QStringLiteral("连接串口"),
                                 QStringLiteral("没有可用串口，请勾选“模拟模式”。"));
            return;
        }
        m_controller->connectSerial(m_serialCombo->currentText());
    }

    m_serialOpenButton->setText(QStringLiteral("断开"));
    m_serialCombo->setEnabled(false);
    m_serialRefreshButton->setEnabled(false);

    // 连上后立刻发一次 PING，验证链路。
    m_controller->serial()->send(Command::Ping);
}

void MainWindow::onSimulationToggled(bool on)
{
    m_controller->setSimulationMode(on);
    m_serialCombo->setEnabled(!on);
    m_serialRefreshButton->setEnabled(!on);
}

void MainWindow::onMidiEvent(const MidiEvent &event)
{
    m_messageCount++;
    m_countLabel->setText(QStringLiteral("消息数量：%1").arg(m_messageCount));

    QString description;
    switch (event.type) {
    case MidiEventType::NoteOn:
        description = QStringLiteral("Note On   %1  Velocity=%2")
                          .arg(noteName(event.note)).arg(event.velocity);
        break;
    case MidiEventType::NoteOff:
        description = QStringLiteral("Note Off  %1  Velocity=%2")
                          .arg(noteName(event.note)).arg(event.velocity);
        break;
    case MidiEventType::ControlChange:
        description = QStringLiteral("Control Change  CC=%1 Value=%2")
                          .arg(event.controller).arg(event.value);
        break;
    case MidiEventType::Other:
        description = QStringLiteral("System/Unknown Message");
        break;
    }

    const QString line = QStringLiteral("[%1 ms] CH=%2  %3  RAW=%4")
                             .arg(event.timestampMs, 8)
                             .arg(event.channel, 2)
                             .arg(description)
                             .arg(event.status, 2, 16, QChar('0'))
                             .toUpper();
    appendLog(line);
}

void MainWindow::onTestValve()
{
    bool ok = false;
    const int valveIndex = QInputDialog::getInt(
        this, QStringLiteral("测试电磁阀"), QStringLiteral("阀号（0~47）："),
        0, 0, 47, 1, &ok);
    if (!ok) {
        return;
    }

    const QStringList states{QStringLiteral("关闭"), QStringLiteral("打开")};
    const QString state = QInputDialog::getItem(
        this, QStringLiteral("测试电磁阀"), QStringLiteral("目标状态："),
        states, 1, false, &ok);
    if (!ok) {
        return;
    }

    m_controller->testValve(valveIndex, state == states.at(1));
}

void MainWindow::onAllOff()
{
    m_controller->allOff();
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this, QStringLiteral("关于"),
        QStringLiteral("管风琴 MIDI 上位机 v0.1\n\n"
                       "MIDI 键盘 -> 上位机（映射/编码）-> STM32 主控 -> CAN -> 从控 -> PWM -> 电磁阀\n\n"
                       "勾选“模拟模式”可在没有 STM32 时调试。"));
}

void MainWindow::appendLog(const QString &text)
{
    if (m_logEdit) {
        m_logEdit->appendPlainText(text);
    }
}

void MainWindow::updateSerialStatus(bool connected)
{
    m_serialStatusLabel->setText(
        connected ? QStringLiteral("串口：已连接") : QStringLiteral("串口：未连接"));
    m_serialOpenButton->setText(
        connected ? QStringLiteral("断开") : QStringLiteral("连接"));
}

QString MainWindow::noteName(int midiNote)
{
    static const char *names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    if (midiNote < 0 || midiNote > 127) {
        return QStringLiteral("Invalid");
    }

    const int octave = midiNote / 12 - 1;
    const int noteIndex = midiNote % 12;

    return QStringLiteral("%1%2 (MIDI %3)")
        .arg(QString::fromLatin1(names[noteIndex]))
        .arg(octave)
        .arg(midiNote);
}

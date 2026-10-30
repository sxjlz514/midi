/**
 * @file mainwindow.cpp
 * @brief 三模式 UI 与串口/MIDI/回放的接线。
 */

#include "mainwindow.h"

#include "midiparser.h"
#include "ocffile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QSerialPortInfo>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVariantMap>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new SerialClient(this))
    , m_aggregator(new KeyStateAggregator(m_serial, this))
    , m_midiIn(new MidiInputDevice(this))
    , m_player(new MidiPlayer(this))
    , m_keyHeld(SerialClient::kChannelCount, false)
{
    m_player->setMapping(&m_mapping);
    m_aggregator->setWindowMs(KeyStateAggregator::kDefaultWindowMs);

    setupUi();
    rebuildMappingTable();
    refreshPortList();
    refreshMidiDevices();
    updatePlayUi();
    updateLiveUi();

    connect(m_serial, &SerialClient::bytesReceived, this, &MainWindow::onSerialBytes);
    connect(m_serial, &SerialClient::errorOccurred, this, &MainWindow::onSerialError);
    connect(m_serial, &SerialClient::connectionChanged,
            this, &MainWindow::onSerialConnectionChanged);

    connect(m_aggregator, &KeyStateAggregator::snapshotSent,
            this, &MainWindow::onSnapshotSent);

    connect(m_midiIn, &MidiInputDevice::noteOn, this, &MainWindow::onLiveNoteOn);
    connect(m_midiIn, &MidiInputDevice::noteOff, this, &MainWindow::onLiveNoteOff);
    connect(m_midiIn, &MidiInputDevice::errorOccurred, this, &MainWindow::onSerialError);

    connect(m_player, &MidiPlayer::channelCommand, this, &MainWindow::onChannelCommand);
    connect(m_player, &MidiPlayer::bitmapCommand, this, &MainWindow::onBitmapCommand);
    connect(m_player, &MidiPlayer::unmappedNote, this, &MainWindow::onUnmappedNote);
    connect(m_player, &MidiPlayer::finished, this, &MainWindow::onPlayerFinished);
    connect(m_player, &MidiPlayer::stopped, this, &MainWindow::updatePlayUi);
    connect(m_player, &MidiPlayer::positionChanged, this, &MainWindow::onPositionChanged);

    setWindowTitle(QStringLiteral("管风琴控制台 — 短窗聚合 / 单帧位图"));
    resize(960, 720);
}

MainWindow::~MainWindow()
{
    m_player->stop();
    m_midiIn->closeDevice();
    if (m_serial->isOpen()) {
        m_aggregator->allOffFlush();
        m_serial->closePort();
    }
}

bool MainWindow::serialConnected() const
{
    return m_serial->isOpen();
}

QString MainWindow::serialStatus() const
{
    return m_serial->isOpen() ? QStringLiteral("已连接") : QStringLiteral("未连接");
}

QStringList MainWindow::serialPortNames() const
{
    QStringList names;
    for (int i = 0; i < m_portCombo->count(); ++i) {
        names.push_back(m_portCombo->itemData(i).toString());
    }
    return names;
}

QStringList MainWindow::midiInputDevices() const
{
    return m_midiIn->listDevices();
}

bool MainWindow::midiInputOpen() const
{
    return m_midiIn->isOpen();
}

QString MainWindow::currentFile() const
{
    return m_fileEdit->text();
}

bool MainWindow::playing() const
{
    return m_player->isPlaying() && !m_player->isPaused();
}

bool MainWindow::paused() const
{
    return m_player->isPaused();
}

QVariantList MainWindow::mappingRows() const
{
    QVariantList rows;
    for (const PitchMapEntry &entry : m_mapping.entries()) {
        QVariantMap row;
        row.insert(QStringLiteral("channel"), entry.channel);
        row.insert(QStringLiteral("board"), entry.channel <= 8
                       ? QStringLiteral("N1") : QStringLiteral("N2"));
        row.insert(QStringLiteral("midiNote"), entry.midiNote);
        row.insert(QStringLiteral("noteName"), entry.noteName);
        rows.push_back(row);
    }
    return rows;
}

void MainWindow::refreshSerialPorts()
{
    refreshPortList();
}

void MainWindow::toggleSerial(const QString &portName, int baudRate)
{
    if (m_serial->isOpen()) {
        toggleSerialPort();
        return;
    }
    const int portIndex = m_portCombo->findData(portName);
    if (portIndex < 0) {
        appendLog(QStringLiteral("请选择一个可用串口。"));
        return;
    }
    m_portCombo->setCurrentIndex(portIndex);
    m_baudCombo->setCurrentText(QString::number(baudRate));
    toggleSerialPort();
}

void MainWindow::setRemoteDebug(bool enabled)
{
    m_debugCheck->setChecked(enabled);
}

void MainWindow::refreshMidiInputs()
{
    refreshMidiDevices();
}

void MainWindow::toggleMidiInput(int deviceIndex)
{
    if (m_midiIn->isOpen()) {
        toggleMidiDevice();
        return;
    }
    if (deviceIndex >= 0 && deviceIndex < m_midiDevCombo->count()) {
        m_midiDevCombo->setCurrentIndex(deviceIndex);
    }
    toggleMidiDevice();
}

void MainWindow::chooseControlFile()
{
    openMidiFile();
}

void MainWindow::convertMidiFile(const QString &mappingMode, int rangeStart,
                                 int rangeEnd, int windowMs)
{
    const int mappingIndex = m_ocfMappingCombo->findData(mappingMode);
    if (mappingIndex >= 0) {
        m_ocfMappingCombo->setCurrentIndex(mappingIndex);
    }
    m_ocfRangeStart->setValue(rangeStart);
    m_ocfRangeEnd->setValue(rangeEnd);
    m_ocfWindowMs->setValue(windowMs);
    convertMidiToOcf();
}

QString MainWindow::readOcfText(const QString &requestedPath)
{
    QString path = requestedPath;
    if (path.isEmpty()) {
        const QString loadedPath = m_fileEdit->text();
        if (QFileInfo(loadedPath).suffix().compare(QStringLiteral("ocf"), Qt::CaseInsensitive) == 0
            && QFileInfo::exists(loadedPath)) {
            path = loadedPath;
        } else {
            QString initialDir = QFileInfo(loadedPath).exists()
                ? QFileInfo(loadedPath).absolutePath()
                : QCoreApplication::applicationDirPath();
            const QString convertedDir = QDir(initialDir).filePath(QStringLiteral("converted"));
            if (QDir(convertedDir).exists()) {
                initialDir = convertedDir;
            }
            path = QFileDialog::getOpenFileName(
                nullptr, QStringLiteral("选择要查看的 OCF"), initialDir,
                QStringLiteral("OCF 位图流 (*.ocf);;All (*.*)"));
        }
    }
    if (path.isEmpty()) {
        return QString();
    }
    OcfData data;
    QString error;
    if (!OcfFile::readFile(path, data, error)) {
        return QStringLiteral("OCF 解析失败：%1").arg(error);
    }
    return OcfFile::toText(data, path);
}

void MainWindow::chooseOcfToInspect()
{
    inspectOcf();
}

void MainWindow::play()
{
    onPlay();
}

void MainWindow::pause()
{
    onPause();
}

void MainWindow::stop()
{
    onStop();
}

void MainWindow::setTempo(int percent)
{
    m_tempoSlider->setValue(qBound(25, percent, 200));
}

void MainWindow::pressChannel(int channel)
{
    onDirectPressed(channel);
}

void MainWindow::releaseChannel(int channel)
{
    onDirectReleased(channel);
}

void MainWindow::allOff()
{
    onAllOffClicked();
}

QString MainWindow::setMappingNote(int channel, int midiNote)
{
    if (channel < 1 || channel > KeyMapping::kChannelCount || midiNote < 0 || midiNote > 127) {
        return QStringLiteral("通道或 MIDI 音高超出范围。");
    }
    for (int other = 1; other <= KeyMapping::kChannelCount; ++other) {
        if (other != channel && m_mapping.midiNoteForChannel(other) == midiNote) {
            return QStringLiteral("MIDI 音高 %1 已分配给通道 %2。").arg(midiNote).arg(other);
        }
    }
    m_mapping.setChannelMidiNote(channel, midiNote);
    rebuildMappingTable();
    return QString();
}

void MainWindow::resetMapping()
{
    onResetMapping();
}

void MainWindow::activateMode(int modeIndex)
{
    if (modeIndex < -1 || modeIndex > 2 || modeIndex == m_activeMode) {
        return;
    }
    m_activeMode = modeIndex;
    onModeTabChanged(modeIndex);
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);

    auto *serialBox = new QGroupBox(QStringLiteral("串口与调试"), central);
    auto *serialLay = new QHBoxLayout(serialBox);
    m_portCombo = new QComboBox(serialBox);
    m_portCombo->setMinimumWidth(160);
    m_baudCombo = new QComboBox(serialBox);
    m_baudCombo->addItems({QStringLiteral("115200"), QStringLiteral("57600"),
                           QStringLiteral("9600"), QStringLiteral("921600")});
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_refreshPortBtn = new QPushButton(QStringLiteral("刷新"), serialBox);
    m_connectBtn = new QPushButton(QStringLiteral("打开串口"), serialBox);
    m_serialStatus = new QLabel(QStringLiteral("未连接"), serialBox);
    m_debugCheck = new QCheckBox(QStringLiteral("调试回报"), serialBox);
    m_debugCheck->setToolTip(QStringLiteral(
        "开启后主板回显 STATUS/CAN/KEY；关闭后仅下发指令（需烧录带 DEBUG 命令的固件）。"));
    m_debugCheck->setChecked(false);
    m_allOffBtn = new QPushButton(QStringLiteral("ALL_OFF"), serialBox);

    serialLay->addWidget(new QLabel(QStringLiteral("端口:"), serialBox));
    serialLay->addWidget(m_portCombo);
    serialLay->addWidget(new QLabel(QStringLiteral("波特率:"), serialBox));
    serialLay->addWidget(m_baudCombo);
    serialLay->addWidget(m_refreshPortBtn);
    serialLay->addWidget(m_connectBtn);
    serialLay->addWidget(m_debugCheck);
    serialLay->addWidget(m_allOffBtn);
    serialLay->addWidget(m_serialStatus, 1);

    connect(m_refreshPortBtn, &QPushButton::clicked, this, &MainWindow::refreshPortList);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::toggleSerialPort);
    connect(m_debugCheck, &QCheckBox::toggled, this, &MainWindow::onDebugToggled);
    connect(m_allOffBtn, &QPushButton::clicked, this, &MainWindow::onAllOffClicked);
    root->addWidget(serialBox);

    m_modeTabs = new QTabWidget(central);
    m_modeTabs->addTab(buildRealtimePage(), QStringLiteral("1. 实时控制"));
    m_modeTabs->addTab(buildAutoPlayPage(), QStringLiteral("2. 自动演奏"));
    m_modeTabs->addTab(buildDirectPage(), QStringLiteral("3. 直接控制"));
    connect(m_modeTabs, &QTabWidget::currentChanged, this, &MainWindow::onModeTabChanged);
    root->addWidget(m_modeTabs);

    auto *mapBox = new QGroupBox(
        QStringLiteral("键位映射（MIDI 音高 ↔ 通道；实时与自动共用）"), central);
    auto *mapLay = new QVBoxLayout(mapBox);
    m_mapTable = new QTableWidget(KeyMapping::kChannelCount, 4, mapBox);
    m_mapTable->setHorizontalHeaderLabels({
        QStringLiteral("通道"), QStringLiteral("从板"),
        QStringLiteral("MIDI音高"), QStringLiteral("音名")
    });
    m_mapTable->horizontalHeader()->setStretchLastSection(true);
    m_mapTable->verticalHeader()->setVisible(false);
    m_mapTable->setMaximumHeight(260);
    mapLay->addWidget(m_mapTable);
    m_resetMapBtn = new QPushButton(QStringLiteral("恢复默认 C4(60)…D#5(75)"), mapBox);
    mapLay->addWidget(m_resetMapBtn);
    connect(m_mapTable, &QTableWidget::cellChanged, this, &MainWindow::onMappingCellChanged);
    connect(m_resetMapBtn, &QPushButton::clicked, this, &MainWindow::onResetMapping);
    root->addWidget(mapBox);

    m_logEdit = new QPlainTextEdit(central);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(800);
    m_logEdit->setPlaceholderText(QStringLiteral("日志（调试开启时显示热路径收发）"));
    root->addWidget(m_logEdit, 1);

    appendLog(QStringLiteral(
        "短窗聚合 %1 ms → @ST 位图帧；CAN 单帧广播。映射：通道1=C4(60)…16=D#5(75)。")
                  .arg(KeyStateAggregator::kDefaultWindowMs));
}

QWidget *MainWindow::buildRealtimePage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    auto *row = new QHBoxLayout();
    m_midiDevCombo = new QComboBox(page);
    m_refreshMidiBtn = new QPushButton(QStringLiteral("刷新设备"), page);
    m_openMidiBtn = new QPushButton(QStringLiteral("打开 MIDI 输入"), page);
    row->addWidget(new QLabel(QStringLiteral("MIDI 输入:"), page));
    row->addWidget(m_midiDevCombo, 1);
    row->addWidget(m_refreshMidiBtn);
    row->addWidget(m_openMidiBtn);
    lay->addLayout(row);
    m_liveHint = new QLabel(
        QStringLiteral("插入 USB MIDI 键盘后刷新。NoteOn/Off 经映射表转为 ON/OFF。"
                       "Windows 使用 winmm。请先打开主板串口。"),
        page);
    m_liveHint->setWordWrap(true);
    lay->addWidget(m_liveHint);
    lay->addStretch(1);
    connect(m_refreshMidiBtn, &QPushButton::clicked, this, &MainWindow::refreshMidiDevices);
    connect(m_openMidiBtn, &QPushButton::clicked, this, &MainWindow::toggleMidiDevice);
    return page;
}

QWidget *MainWindow::buildAutoPlayPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    auto *fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(page);
    m_fileEdit->setReadOnly(true);
    m_openFileBtn = new QPushButton(QStringLiteral("打开 MIDI / OCF…"), page);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(m_openFileBtn);
    lay->addLayout(fileRow);

    auto *ctrl = new QHBoxLayout();
    m_playBtn = new QPushButton(QStringLiteral("播放"), page);
    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), page);
    m_stopBtn = new QPushButton(QStringLiteral("停止"), page);
    m_timeLabel = new QLabel(QStringLiteral("00:00.000 / 00:00.000"), page);
    ctrl->addWidget(m_playBtn);
    ctrl->addWidget(m_pauseBtn);
    ctrl->addWidget(m_stopBtn);
    ctrl->addWidget(m_timeLabel, 1);
    lay->addLayout(ctrl);

    auto *tempoRow = new QHBoxLayout();
    m_tempoSlider = new QSlider(Qt::Horizontal, page);
    m_tempoSlider->setRange(25, 200);
    m_tempoSlider->setValue(100);
    m_tempoLabel = new QLabel(QStringLiteral("1.00x"), page);
    tempoRow->addWidget(new QLabel(QStringLiteral("速度:"), page));
    tempoRow->addWidget(m_tempoSlider, 1);
    tempoRow->addWidget(m_tempoLabel);
    lay->addLayout(tempoRow);

    auto *ocfBox = new QGroupBox(QStringLiteral("MIDI → OCF 位图流"), page);
    auto *ocfRow = new QHBoxLayout(ocfBox);
    m_ocfMappingCombo = new QComboBox(ocfBox);
    m_ocfMappingCombo->addItem(QStringLiteral("范围外忽略"), QStringLiteral("range"));
    m_ocfMappingCombo->addItem(QStringLiteral("取余循环"), QStringLiteral("wrap"));
    m_ocfRangeStart = new QSpinBox(ocfBox);
    m_ocfRangeStart->setRange(0, 127);
    m_ocfRangeStart->setValue(60);
    m_ocfRangeStart->setPrefix(QStringLiteral("起音 "));
    m_ocfRangeEnd = new QSpinBox(ocfBox);
    m_ocfRangeEnd->setRange(0, 127);
    m_ocfRangeEnd->setValue(91);
    m_ocfRangeEnd->setPrefix(QStringLiteral("止音 "));
    m_ocfWindowMs = new QSpinBox(ocfBox);
    m_ocfWindowMs->setRange(1, 1000);
    m_ocfWindowMs->setValue(30);
    m_ocfWindowMs->setSuffix(QStringLiteral(" ms"));
    m_convertOcfBtn = new QPushButton(QStringLiteral("选择 MIDI 并转换"), ocfBox);
    m_inspectOcfBtn = new QPushButton(QStringLiteral("查看 OCF 文本"), ocfBox);
    ocfRow->addWidget(new QLabel(QStringLiteral("映射:"), ocfBox));
    ocfRow->addWidget(m_ocfMappingCombo);
    ocfRow->addWidget(m_ocfRangeStart);
    ocfRow->addWidget(m_ocfRangeEnd);
    ocfRow->addWidget(new QLabel(QStringLiteral("聚合窗:"), ocfBox));
    ocfRow->addWidget(m_ocfWindowMs);
    ocfRow->addWidget(m_convertOcfBtn);
    ocfRow->addWidget(m_inspectOcfBtn);
    lay->addWidget(ocfBox);
    lay->addStretch(1);

    connect(m_openFileBtn, &QPushButton::clicked, this, &MainWindow::openMidiFile);
    connect(m_convertOcfBtn, &QPushButton::clicked, this, &MainWindow::convertMidiToOcf);
    connect(m_inspectOcfBtn, &QPushButton::clicked, this, &MainWindow::inspectOcf);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_tempoSlider, &QSlider::valueChanged, this, &MainWindow::onTempoChanged);
    return page;
}

QWidget *MainWindow::buildDirectPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    auto *hint = new QLabel(
        QStringLiteral("鼠标按住按钮或键盘：1-9→通道1-9，0→10，QWERTY→11-16。"),
        page);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    auto *grid = new QGridLayout();
    m_directButtons.clear();
    for (int i = 0; i < SerialClient::kChannelCount; ++i) {
        const int ch = i + 1;
        auto *btn = new QPushButton(QString::number(ch), page);
        btn->setMinimumSize(64, 48);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 16px; font-weight: bold; }"
            "QPushButton:pressed { background-color: #4CAF50; color: white; }"));
        connect(btn, &QPushButton::pressed, this, [this, ch]() { onDirectPressed(ch); });
        connect(btn, &QPushButton::released, this, [this, ch]() { onDirectReleased(ch); });
        grid->addWidget(btn, i / 4, i % 4);
        m_directButtons.push_back(btn);
    }
    lay->addLayout(grid);
    lay->addStretch(1);
    return page;
}

void MainWindow::rebuildMappingTable()
{
    m_blockMapEdit = true;
    for (int i = 0; i < KeyMapping::kChannelCount; ++i) {
        const PitchMapEntry &e = m_mapping.entries().at(i);
        const QString board = (e.channel <= 8) ? QStringLiteral("N1")
                                               : QStringLiteral("N2");
        auto setCell = [&](int col, const QString &text, bool editable) {
            QTableWidgetItem *item = m_mapTable->item(i, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_mapTable->setItem(i, col, item);
            }
            item->setText(text);
            Qt::ItemFlags flags = item->flags();
            if (editable) {
                flags |= Qt::ItemIsEditable;
            } else {
                flags &= ~Qt::ItemIsEditable;
            }
            item->setFlags(flags);
        };
        setCell(0, QString::number(e.channel), false);
        setCell(1, board, false);
        setCell(2, QString::number(e.midiNote), true);
        setCell(3, e.noteName, false);
    }
    m_mapTable->resizeColumnsToContents();
    m_blockMapEdit = false;
    emit mappingRowsChanged();
}

void MainWindow::refreshPortList()
{
    const QString cur = m_portCombo->currentData().toString();
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        m_portCombo->addItem(
            QStringLiteral("%1 — %2").arg(info.portName(), info.description()),
            info.portName());
    }
    const int idx = m_portCombo->findData(cur);
    if (idx >= 0) {
        m_portCombo->setCurrentIndex(idx);
    }
    appendLog(QStringLiteral("串口列表：%1 个。").arg(m_portCombo->count()));
    emit serialPortsChanged();
}

void MainWindow::toggleSerialPort()
{
    if (m_serial->isOpen()) {
        m_aggregator->allOffFlush();
        m_serial->closePort();
        m_aggregator->resetLocal();
        appendLog(QStringLiteral("串口已关闭。"));
        return;
    }
    const QString name = m_portCombo->currentData().toString();
    if (name.isEmpty()) {
        appendLog(QStringLiteral("请选择一个可用串口。"));
        return;
    }
    if (!m_serial->openPort(name, m_baudCombo->currentText().toInt())) {
        return;
    }
    m_aggregator->resetLocal();
    m_serial->sendRemoteDebug(m_debugCheck->isChecked());
    appendLog(QStringLiteral("已打开 %1，DEBUG=%2，聚合窗=%3ms")
                  .arg(name, m_debugCheck->isChecked() ? QStringLiteral("ON")
                                                       : QStringLiteral("OFF"))
                  .arg(m_aggregator->windowMs()));
}

void MainWindow::onSerialBytes(const QString &text)
{
    if (!m_debugEnabled) {
        return;
    }
    const QString t = text.trimmed();
    if (!t.isEmpty()) {
        appendLog(QStringLiteral("[RX] %1").arg(t));
    }
}

void MainWindow::onSerialError(const QString &text)
{
    appendLog(QStringLiteral("错误：%1").arg(text));
}

void MainWindow::onSerialConnectionChanged(bool connected)
{
    m_connectBtn->setText(connected ? QStringLiteral("关闭串口")
                                    : QStringLiteral("打开串口"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_refreshPortBtn->setEnabled(!connected);
    m_serialStatus->setText(connected ? QStringLiteral("已连接")
                                      : QStringLiteral("未连接"));
    m_serialStatus->setStyleSheet(
        connected ? QStringLiteral("color: green; font-weight: bold;")
                  : QStringLiteral("color: gray;"));
    emit serialStateChanged();
}

void MainWindow::onDebugToggled(bool enabled)
{
    m_debugEnabled = enabled;
    if (m_serial->isOpen()) {
        m_serial->sendRemoteDebug(enabled);
    }
    appendLog(enabled ? QStringLiteral("调试回报：开（下位机将打印 STATUS/CAN）")
                      : QStringLiteral("调试回报：关（仅指令下发）"));
}

void MainWindow::onModeTabChanged(int index)
{
    Q_UNUSED(index);
    m_player->stop();
    if (m_midiIn->isOpen()) {
        m_midiIn->closeDevice();
        m_openMidiBtn->setText(QStringLiteral("打开 MIDI 输入"));
    }
    for (int i = 0; i < m_keyHeld.size(); ++i) {
        m_keyHeld[i] = false;
    }
    m_aggregator->allOffFlush();
    updatePlayUi();
    updateLiveUi();
    appendLog(QStringLiteral("已切换模式，并刷出全关位图。"));
}

void MainWindow::refreshMidiDevices()
{
    m_midiDevCombo->clear();
    const QStringList names = m_midiIn->listDevices();
    for (const QString &n : names) {
        m_midiDevCombo->addItem(n);
    }
    if (names.isEmpty()) {
        m_midiDevCombo->addItem(QStringLiteral("(未发现 MIDI 输入)"));
    }
    appendLog(QStringLiteral("MIDI 输入设备：%1 个。").arg(names.size()));
    emit midiInputStateChanged();
}

void MainWindow::toggleMidiDevice()
{
    if (m_midiIn->isOpen()) {
        m_midiIn->closeDevice();
        m_serial->sendAllOff();
        updateLiveUi();
        appendLog(QStringLiteral("MIDI 输入已关闭。"));
        return;
    }
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("请先打开主板串口。"));
        return;
    }
    const int idx = m_midiDevCombo->currentIndex();
    if (m_midiIn->listDevices().isEmpty() || idx < 0) {
        appendLog(QStringLiteral("没有可用的 MIDI 输入设备。"));
        return;
    }
    if (m_midiIn->openDevice(idx)) {
        appendLog(QStringLiteral("已打开 MIDI：%1").arg(m_midiDevCombo->currentText()));
    }
    updateLiveUi();
}

void MainWindow::updateLiveUi()
{
    m_openMidiBtn->setText(m_midiIn->isOpen() ? QStringLiteral("关闭 MIDI 输入")
                                              : QStringLiteral("打开 MIDI 输入"));
    m_midiDevCombo->setEnabled(!m_midiIn->isOpen());
    m_refreshMidiBtn->setEnabled(!m_midiIn->isOpen());
    emit midiInputStateChanged();
}

void MainWindow::onLiveNoteOn(int midiNote, int velocity)
{
    Q_UNUSED(velocity);
    const auto ch = m_mapping.channelForMidiNote(midiNote);
    if (!ch.has_value()) {
        onUnmappedNote(midiNote, true);
        return;
    }
    sendChannel(*ch, true);
}

void MainWindow::onLiveNoteOff(int midiNote, int velocity)
{
    Q_UNUSED(velocity);
    const auto ch = m_mapping.channelForMidiNote(midiNote);
    if (!ch.has_value()) {
        return;
    }
    sendChannel(*ch, false);
}

void MainWindow::openMidiFile()
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("打开 MIDI 或 OCF"), QString(),
        QStringLiteral("MIDI / OCF (*.mid *.midi *.ocf);;MIDI (*.mid *.midi);;OCF (*.ocf);;All (*.*)"));
    if (!path.isEmpty()) {
        loadControlFile(path);
    }
}

bool MainWindow::loadControlFile(const QString &path)
{
    if (QFileInfo(path).suffix().compare(QStringLiteral("ocf"), Qt::CaseInsensitive) == 0) {
        OcfData data;
        QString error;
        if (!OcfFile::readFile(path, data, error)) {
            appendLog(QStringLiteral("OCF 读取失败：%1").arg(error));
            return false;
        }
        m_player->stop();
        m_player->setFrames(data.frames, static_cast<qint64>(data.durationMs));
        m_durationMs = static_cast<qint64>(data.durationMs);
        m_positionMs = 0;
        m_fileEdit->setText(path);
        appendLog(QStringLiteral("已加载 OCF v%1：%2 条位图记录，%3 ms，CRC32 通过。")
                      .arg(data.version).arg(data.frames.size()).arg(data.durationMs));
        m_timeLabel->setText(QStringLiteral("%1 / %2").arg(formatMs(0), formatMs(m_durationMs)));
        updatePlayUi();
        emit fileStateChanged();
        emit playbackPositionChanged();
        return true;
    }

    const MidiParseResult r = MidiParser::parseFile(path);
    if (!r.ok) {
        appendLog(QStringLiteral("MIDI 解析失败：%1").arg(r.error));
        return false;
    }
    m_player->stop();
    m_player->setEvents(r.events);
    m_durationMs = r.durationMs;
    m_positionMs = 0;
    m_fileEdit->setText(path);

    int mapped = 0;
    int unmapped = 0;
    QSet<int> seen;
    for (const MidiNoteEvent &ev : r.events) {
        if (ev.type != MidiEventType::NoteOn || seen.contains(ev.midiNote)) {
            continue;
        }
        seen.insert(ev.midiNote);
        if (m_mapping.channelForMidiNote(ev.midiNote).has_value()) {
            ++mapped;
        } else {
            ++unmapped;
        }
    }
    appendLog(QStringLiteral("已加载 Format=%1 Tracks=%2 事件=%3 时长=%4 映射内音高%5/外%6")
                  .arg(r.format).arg(r.trackCount).arg(r.events.size())
                  .arg(formatMs(r.durationMs)).arg(mapped).arg(unmapped));
    m_timeLabel->setText(QStringLiteral("%1 / %2").arg(formatMs(0), formatMs(m_durationMs)));
    updatePlayUi();
    emit fileStateChanged();
    emit playbackPositionChanged();
    return true;
}

void MainWindow::convertMidiToOcf()
{
    if (m_converterProcess) {
        return;
    }
    const QString inputPath = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("选择要转换的 MIDI"), QString(),
        QStringLiteral("MIDI (*.mid *.midi);;All (*.*)"));
    if (inputPath.isEmpty()) {
        return;
    }
    startOcfConversion(inputPath, m_ocfMappingCombo->currentData().toString(),
                       m_ocfRangeStart->value(), m_ocfRangeEnd->value(),
                       m_ocfWindowMs->value());
}

void MainWindow::startOcfConversion(const QString &inputPath, const QString &mappingMode,
                                    int low, int high, int windowMs)
{
    if (high < low || high - low + 1 > 32) {
        const QString error = QStringLiteral("范围首尾均包含，必须从低到高且最多 32 个音高。");
        appendLog(QStringLiteral("映射范围无效：%1").arg(error));
        emit conversionFinished(false, QString(), error);
        return;
    }
    if (windowMs < 1 || windowMs > 65535) {
        const QString error = QStringLiteral("聚合窗必须在 1..65535 ms 之间。");
        appendLog(error);
        emit conversionFinished(false, QString(), error);
        return;
    }

    QString executable = QStandardPaths::findExecutable(QStringLiteral("python"));
    QStringList processArgs;
    if (executable.isEmpty()) {
        executable = QStandardPaths::findExecutable(QStringLiteral("python3"));
    }
    if (executable.isEmpty()) {
        executable = QStandardPaths::findExecutable(QStringLiteral("py"));
        if (!executable.isEmpty()) {
            processArgs.push_back(QStringLiteral("-3"));
        }
    }
    if (executable.isEmpty()) {
        const QString error = QStringLiteral("转换脚本已嵌入软件，但运行时需要 Python 3。"
                                             "请安装 Python 3 并将 python 或 py 加入 PATH。");
        appendLog(error);
        emit conversionFinished(false, QString(), error);
        return;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(appData)) {
        const QString error = QStringLiteral("无法创建应用数据目录：%1").arg(appData);
        appendLog(error);
        emit conversionFinished(false, QString(), error);
        return;
    }
    QFile embedded(QStringLiteral(":/tools/midi_to_ocf.py"));
    if (!embedded.open(QIODevice::ReadOnly)) {
        const QString error = QStringLiteral("无法读取软件内置的 Python 转换脚本。");
        appendLog(error);
        emit conversionFinished(false, QString(), error);
        return;
    }
    const QString scriptPath = QDir(appData).filePath(QStringLiteral("midi_to_ocf.py"));
    QFile script(scriptPath);
    const QByteArray scriptBytes = embedded.readAll();
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || script.write(scriptBytes) != scriptBytes.size()) {
        const QString error = QStringLiteral("无法写入 Python 转换脚本：%1").arg(scriptPath);
        appendLog(error);
        emit conversionFinished(false, QString(), error);
        return;
    }
    script.close();

    const QFileInfo inputInfo(inputPath);
    const QString outputDir = QDir(inputInfo.absolutePath()).filePath(QStringLiteral("converted"));
    if (!QDir().mkpath(outputDir)) {
        appendLog(QStringLiteral("无法创建输出目录：%1").arg(outputDir));
        emit conversionFinished(false, QString(), outputDir);
        return;
    }
    const QString outputPath = QDir(outputDir).filePath(
        inputInfo.completeBaseName() + QStringLiteral(".ocf"));
    processArgs << scriptPath << inputPath
                << QStringLiteral("--mapping") << mappingMode
                << QStringLiteral("--range-start") << QString::number(low)
                << QStringLiteral("--range-end") << QString::number(high)
                << QStringLiteral("--window-ms") << QString::number(windowMs)
                << QStringLiteral("--output") << outputPath;

    auto *process = new QProcess(this);
    m_converterProcess = process;
    connect(process, &QProcess::finished, this,
            [this, process, outputPath](int exitCode, QProcess::ExitStatus status) {
        const QString stdoutText = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        const QString stderrText = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        if (m_converterProcess != process) {
            process->deleteLater();
            return;
        }
        m_converterProcess = nullptr;
        process->deleteLater();
        updatePlayUi();
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString error = stderrText.isEmpty() ? stdoutText : stderrText;
            appendLog(QStringLiteral("MIDI 转换失败：%1").arg(error));
            emit conversionStateChanged();
            emit conversionFinished(false, outputPath, error);
            return;
        }
        appendLog(stdoutText.isEmpty() ? QStringLiteral("OCF 转换完成：%1").arg(outputPath)
                                       : stdoutText);
        OcfData data;
        QString error;
        if (!OcfFile::readFile(outputPath, data, error)) {
            appendLog(QStringLiteral("OCF 校验失败：%1").arg(error));
            emit conversionStateChanged();
            emit conversionFinished(false, outputPath, error);
            return;
        }
        loadControlFile(outputPath);
        emit conversionStateChanged();
        emit conversionFinished(true, outputPath, QString());
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_converterProcess != process) {
            return;
        }
        const QString message = process->errorString();
        m_converterProcess = nullptr;
        process->deleteLater();
        updatePlayUi();
        appendLog(QStringLiteral("Python 启动失败：%1").arg(message));
        emit conversionStateChanged();
        emit conversionFinished(false, QString(), message);
    });
    process->start(executable, processArgs);
    emit conversionStateChanged();
    m_convertOcfBtn->setEnabled(false);
    appendLog(QStringLiteral("开始转换 MIDI→OCF…"));
}

void MainWindow::inspectOcf()
{
    QString path;
    const QString loadedPath = m_fileEdit->text();
    if (QFileInfo(loadedPath).suffix().compare(QStringLiteral("ocf"), Qt::CaseInsensitive) == 0
        && QFileInfo::exists(loadedPath)) {
        path = loadedPath;
    } else {
        QString initialDir = QFileInfo(loadedPath).exists()
            ? QFileInfo(loadedPath).absolutePath()
            : QCoreApplication::applicationDirPath();
        const QString convertedDir = QDir(initialDir).filePath(QStringLiteral("converted"));
        if (QDir(convertedDir).exists()) {
            initialDir = convertedDir;
        }
        path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择要查看的 OCF"), initialDir,
            QStringLiteral("OCF 位图流 (*.ocf);;All (*.*)"));
    }
    if (path.isEmpty()) {
        return;
    }

    OcfData data;
    QString error;
    if (!OcfFile::readFile(path, data, error)) {
        QMessageBox::warning(this, QStringLiteral("OCF 解析失败"), error);
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("OCF 文本解析 — %1").arg(QFileInfo(path).fileName()));
    dialog.resize(900, 600);
    auto *layout = new QVBoxLayout(&dialog);
    auto *view = new QPlainTextEdit(&dialog);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setPlainText(OcfFile::toText(data, path));
    layout->addWidget(view);
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), &dialog);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    dialog.exec();
}

void MainWindow::onPlay()
{
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("请先打开主板串口。"));
        return;
    }
    m_player->play();
    updatePlayUi();
    appendLog(QStringLiteral("开始自动演奏。"));
}

void MainWindow::onPause()
{
    m_player->pause();
    updatePlayUi();
}

void MainWindow::onStop()
{
    m_player->stop();
    m_aggregator->allOffFlush();
    updatePlayUi();
}

void MainWindow::onTempoChanged(int sliderValue)
{
    m_tempoPercent = sliderValue;
    const double scale = sliderValue / 100.0;
    m_tempoLabel->setText(QStringLiteral("%1x").arg(scale, 0, 'f', 2));
    m_player->setTempoScale(scale);
    emit playbackStateChanged();
}

void MainWindow::onChannelCommand(int channel, bool on)
{
    sendChannel(channel, on);
}

void MainWindow::onBitmapCommand(quint32 bitmap)
{
    m_aggregator->setBitmapAndFlush(bitmap);
}

void MainWindow::onUnmappedNote(int midiNote, bool on)
{
    if (on) {
        appendLog(QStringLiteral("未映射音高 %1 (%2)")
                      .arg(midiNote)
                      .arg(KeyMapping::midiNoteToName(midiNote)));
    }
}

void MainWindow::onPlayerFinished()
{
    m_aggregator->allOffFlush();
    updatePlayUi();
    appendLog(QStringLiteral("自动演奏结束。"));
}

void MainWindow::onPositionChanged(qint64 ms)
{
    m_positionMs = ms;
    m_timeLabel->setText(QStringLiteral("%1 / %2")
                             .arg(formatMs(ms), formatMs(m_durationMs)));
    emit playbackPositionChanged();
}

void MainWindow::updatePlayUi()
{
    const bool hasSong = m_fileEdit && !m_fileEdit->text().isEmpty();
    const bool playing = m_player->isPlaying() && !m_player->isPaused();
    const bool paused = m_player->isPaused();
    m_playBtn->setEnabled(hasSong && !playing);
    m_pauseBtn->setEnabled(playing);
    m_stopBtn->setEnabled(hasSong && (playing || paused));
    m_openFileBtn->setEnabled(!playing);
    m_convertOcfBtn->setEnabled(!playing && m_converterProcess == nullptr);
    m_inspectOcfBtn->setEnabled(true);
    const bool mapLocked = playing || m_midiIn->isOpen();
    m_resetMapBtn->setEnabled(!mapLocked);
    m_mapTable->setEnabled(!mapLocked);
    emit playbackStateChanged();
}

void MainWindow::onDirectPressed(int channel)
{
    sendChannel(channel, true);
}

void MainWindow::onDirectReleased(int channel)
{
    sendChannel(channel, false);
}

void MainWindow::onAllOffClicked()
{
    m_aggregator->allOffFlush();
}

void MainWindow::onSnapshotSent(quint32 bitmap, quint16 seq)
{
    if (!m_debugEnabled) {
        return;
    }
    appendLog(QStringLiteral("[TX] @ST seq=%1 bitmap=0x%2")
                  .arg(seq, 4, 16, QLatin1Char('0'))
                  .arg(bitmap, 8, 16, QLatin1Char('0')));
}

void MainWindow::onResetMapping()
{
    m_mapping.resetToDefault(KeyMapping::kDefaultBaseNote);
    rebuildMappingTable();
}

void MainWindow::onMappingCellChanged(int row, int column)
{
    if (m_blockMapEdit || column != 2) {
        return;
    }
    QTableWidgetItem *item = m_mapTable->item(row, column);
    if (!item) {
        return;
    }
    bool ok = false;
    const int note = item->text().trimmed().toInt(&ok);
    if (!ok || note < 0 || note > 127) {
        QMessageBox::warning(this, QStringLiteral("无效"), QStringLiteral("音高须 0–127。"));
        rebuildMappingTable();
        return;
    }
    for (int ch = 1; ch <= KeyMapping::kChannelCount; ++ch) {
        if (ch != row + 1 && m_mapping.midiNoteForChannel(ch) == note) {
            QMessageBox::warning(this, QStringLiteral("冲突"),
                                 QStringLiteral("音高已占用通道 %1。").arg(ch));
            rebuildMappingTable();
            return;
        }
    }
    m_mapping.setChannelMidiNote(row + 1, note);
    rebuildMappingTable();
}

bool MainWindow::sendChannel(int channel, bool on)
{
    highlightChannel(channel, on);
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("未开串口，忽略通道 %1").arg(channel));
        return false;
    }
    /* 短窗聚合：不逐键发 ON/OFF，窗满后发一张完整位图 */
    m_aggregator->setChannel(channel, on);
    return true;
}

void MainWindow::highlightChannel(int channel, bool on)
{
    if (channel < 1 || channel > m_directButtons.size()) {
        return;
    }
    QPushButton *btn = m_directButtons[channel - 1];
    if (on) {
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 16px; font-weight: bold;"
            " background-color: #4CAF50; color: white; }"));
    } else {
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 16px; font-weight: bold; }"
            "QPushButton:pressed { background-color: #4CAF50; color: white; }"));
    }
}

void MainWindow::appendLog(const QString &text)
{
    m_logs.push_back(text);
    while (m_logs.size() > 800) {
        m_logs.pop_front();
    }
    m_logEdit->appendPlainText(text);
    emit logLinesChanged();
}

int MainWindow::keyToChannel(int key) const
{
    switch (key) {
    case Qt::Key_1: return 1;
    case Qt::Key_2: return 2;
    case Qt::Key_3: return 3;
    case Qt::Key_4: return 4;
    case Qt::Key_5: return 5;
    case Qt::Key_6: return 6;
    case Qt::Key_7: return 7;
    case Qt::Key_8: return 8;
    case Qt::Key_9: return 9;
    case Qt::Key_0: return 10;
    case Qt::Key_Q: return 11;
    case Qt::Key_W: return 12;
    case Qt::Key_E: return 13;
    case Qt::Key_R: return 14;
    case Qt::Key_T: return 15;
    case Qt::Key_Y: return 16;
    default: return 0;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_modeTabs->currentIndex() != 2 || event->isAutoRepeat()) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    const int ch = keyToChannel(event->key());
    if (ch == 0) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    if (m_keyHeld[ch - 1]) {
        return;
    }
    m_keyHeld[ch - 1] = true;
    onDirectPressed(ch);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_modeTabs->currentIndex() != 2 || event->isAutoRepeat()) {
        QMainWindow::keyReleaseEvent(event);
        return;
    }
    const int ch = keyToChannel(event->key());
    if (ch == 0) {
        QMainWindow::keyReleaseEvent(event);
        return;
    }
    if (!m_keyHeld[ch - 1]) {
        return;
    }
    m_keyHeld[ch - 1] = false;
    onDirectReleased(ch);
}

QString MainWindow::formatMs(qint64 ms)
{
    if (ms < 0) {
        ms = 0;
    }
    const qint64 totalSec = ms / 1000;
    return QStringLiteral("%1:%2.%3")
        .arg(totalSec / 60, 2, 10, QLatin1Char('0'))
        .arg(totalSec % 60, 2, 10, QLatin1Char('0'))
        .arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

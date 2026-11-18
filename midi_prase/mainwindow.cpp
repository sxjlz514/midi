#include "mainwindow.h"

#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSet>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>

#include "midiparser.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new QSerialPort(this))
    , m_player(new MidiPlayer(this))
{
    m_player->setMapping(&m_mapping);

    setupUi();
    rebuildMappingTable();
    refreshPortList();
    updateUiConnectedState(false);
    updatePlayUi();

    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialErrorOccurred);

    connect(m_player, &MidiPlayer::channelCommand, this, &MainWindow::onChannelCommand);
    connect(m_player, &MidiPlayer::unmappedNote, this, &MainWindow::onUnmappedNote);
    connect(m_player, &MidiPlayer::finished, this, &MainWindow::onPlayerFinished);
    connect(m_player, &MidiPlayer::stopped, this, &MainWindow::updatePlayUi);
    connect(m_player, &MidiPlayer::positionChanged, this, &MainWindow::onPositionChanged);

    setWindowTitle(QStringLiteral("MIDI 自动演奏（16 路）— midi_prase"));
    resize(900, 640);
}

MainWindow::~MainWindow()
{
    if (m_player) {
        m_player->stop();
    }
    if (m_serial->isOpen()) {
        m_serial->close();
    }
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);

    /* 串口 */
    auto *serialBox = new QGroupBox(QStringLiteral("串口（主板 COM，协议 ON/OFF n）"), central);
    auto *serialLay = new QHBoxLayout(serialBox);
    m_portCombo = new QComboBox(serialBox);
    m_portCombo->setMinimumWidth(140);
    m_baudCombo = new QComboBox(serialBox);
    m_baudCombo->addItems({QStringLiteral("115200"), QStringLiteral("57600"),
                           QStringLiteral("9600"), QStringLiteral("921600")});
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), serialBox);
    m_connectBtn = new QPushButton(QStringLiteral("打开串口"), serialBox);
    m_statusLabel = new QLabel(QStringLiteral("未连接"), serialBox);
    serialLay->addWidget(new QLabel(QStringLiteral("端口:"), serialBox));
    serialLay->addWidget(m_portCombo);
    serialLay->addWidget(new QLabel(QStringLiteral("波特率:"), serialBox));
    serialLay->addWidget(m_baudCombo);
    serialLay->addWidget(m_refreshBtn);
    serialLay->addWidget(m_connectBtn);
    serialLay->addWidget(m_statusLabel, 1);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPortList);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::toggleSerialPort);
    root->addWidget(serialBox);

    /* 文件与播放 */
    auto *playBox = new QGroupBox(QStringLiteral("MIDI 文件 / 自动演奏"), central);
    auto *playLay = new QVBoxLayout(playBox);

    auto *fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(playBox);
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(QStringLiteral("尚未加载 .mid / .midi"));
    m_openBtn = new QPushButton(QStringLiteral("打开 MIDI…"), playBox);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(m_openBtn);
    playLay->addLayout(fileRow);

    auto *ctrlRow = new QHBoxLayout();
    m_playBtn = new QPushButton(QStringLiteral("播放"), playBox);
    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), playBox);
    m_stopBtn = new QPushButton(QStringLiteral("停止"), playBox);
    m_allOffBtn = new QPushButton(QStringLiteral("ALL_OFF"), playBox);
    m_timeLabel = new QLabel(QStringLiteral("00:00.000 / 00:00.000"), playBox);
    ctrlRow->addWidget(m_playBtn);
    ctrlRow->addWidget(m_pauseBtn);
    ctrlRow->addWidget(m_stopBtn);
    ctrlRow->addWidget(m_allOffBtn);
    ctrlRow->addWidget(m_timeLabel, 1);
    playLay->addLayout(ctrlRow);

    auto *tempoRow = new QHBoxLayout();
    tempoRow->addWidget(new QLabel(QStringLiteral("速度:"), playBox));
    m_tempoSlider = new QSlider(Qt::Horizontal, playBox);
    m_tempoSlider->setRange(25, 200); /* 0.25x .. 2.0x，步进 0.01 */
    m_tempoSlider->setValue(100);
    m_tempoLabel = new QLabel(QStringLiteral("1.00x"), playBox);
    m_tempoLabel->setMinimumWidth(48);
    tempoRow->addWidget(m_tempoSlider, 1);
    tempoRow->addWidget(m_tempoLabel);
    playLay->addLayout(tempoRow);

    connect(m_openBtn, &QPushButton::clicked, this, &MainWindow::openMidiFile);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_allOffBtn, &QPushButton::clicked, this, &MainWindow::onAllOff);
    connect(m_tempoSlider, &QSlider::valueChanged, this, &MainWindow::onTempoChanged);
    root->addWidget(playBox);

    /* 映射表 */
    auto *mapBox = new QGroupBox(
        QStringLiteral("键位映射表（MIDI 音高 ↔ 通道 1–16；1–8=N1，9–16=N2）"),
        central);
    auto *mapLay = new QVBoxLayout(mapBox);
    m_mapTable = new QTableWidget(KeyMapping::kChannelCount, 4, mapBox);
    m_mapTable->setHorizontalHeaderLabels({
        QStringLiteral("通道"),
        QStringLiteral("从板"),
        QStringLiteral("MIDI音高"),
        QStringLiteral("音名")
    });
    m_mapTable->horizontalHeader()->setStretchLastSection(true);
    m_mapTable->verticalHeader()->setVisible(false);
    m_mapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mapTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                | QAbstractItemView::EditKeyPressed);
    mapLay->addWidget(m_mapTable);

    auto *mapBtnRow = new QHBoxLayout();
    m_resetMapBtn = new QPushButton(
        QStringLiteral("恢复默认（C4=60 … D#5=75）"), mapBox);
    mapBtnRow->addWidget(m_resetMapBtn);
    mapBtnRow->addStretch(1);
    mapLay->addLayout(mapBtnRow);

    connect(m_mapTable, &QTableWidget::cellChanged,
            this, &MainWindow::onMappingCellChanged);
    connect(m_resetMapBtn, &QPushButton::clicked, this, &MainWindow::onResetMapping);
    root->addWidget(mapBox, 1);

    /* 日志 */
    m_logEdit = new QPlainTextEdit(central);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(800);
    m_logEdit->setPlaceholderText(QStringLiteral("解析 / 演奏 / 串口日志…"));
    root->addWidget(m_logEdit, 1);

    appendLog(QStringLiteral(
        "默认映射：通道1→C4(60)，通道2→C#4(61)，…，通道16→D#5(75)。"
        "双击「MIDI音高」列可改。"));
}

void MainWindow::rebuildMappingTable()
{
    m_blockMapEdit = true;
    for (int i = 0; i < KeyMapping::kChannelCount; ++i) {
        const PitchMapEntry &e = m_mapping.entries().at(i);
        const QString board = (e.channel <= 8)
                                  ? QStringLiteral("N1")
                                  : QStringLiteral("N2");

        auto setCell = [&](int col, const QString &text, bool editable) {
            QTableWidgetItem *item = m_mapTable->item(i, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_mapTable->setItem(i, col, item);
            }
            item->setText(text);
            if (editable) {
                item->setFlags(item->flags() | Qt::ItemIsEditable);
            } else {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        };

        setCell(0, QString::number(e.channel), false);
        setCell(1, board, false);
        setCell(2, QString::number(e.midiNote), true);
        setCell(3, e.noteName, false);
    }
    m_mapTable->resizeColumnsToContents();
    m_blockMapEdit = false;
}

void MainWindow::refreshPortList()
{
    const QString cur = m_portCombo->currentData().toString();
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        const QString label = QStringLiteral("%1 (%2)")
                                  .arg(info.portName(), info.description());
        m_portCombo->addItem(label, info.portName());
    }
    const int idx = m_portCombo->findData(cur);
    if (idx >= 0) {
        m_portCombo->setCurrentIndex(idx);
    }
    appendLog(QStringLiteral("已刷新串口，共 %1 个。").arg(m_portCombo->count()));
}

void MainWindow::toggleSerialPort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        updateUiConnectedState(false);
        appendLog(QStringLiteral("串口已关闭。"));
        return;
    }
    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择串口。"));
        return;
    }
    m_serial->setPortName(portName);
    m_serial->setBaudRate(m_baudCombo->currentText().toInt());
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"),
                              m_serial->errorString());
        return;
    }
    updateUiConnectedState(true);
    appendLog(QStringLiteral("已打开 %1 @ %2")
                  .arg(portName, m_baudCombo->currentText()));
}

void MainWindow::updateUiConnectedState(bool connected)
{
    m_connectBtn->setText(connected ? QStringLiteral("关闭串口")
                                    : QStringLiteral("打开串口"));
    m_statusLabel->setText(connected ? QStringLiteral("已连接")
                                     : QStringLiteral("未连接"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_refreshBtn->setEnabled(!connected);
}

void MainWindow::onSerialReadyRead()
{
    const QByteArray data = m_serial->readAll();
    if (!data.isEmpty()) {
        appendLog(QStringLiteral("[RX] %1")
                      .arg(QString::fromLocal8Bit(data).trimmed()));
    }
}

void MainWindow::onSerialErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) {
        return;
    }
    appendLog(QStringLiteral("串口错误：%1").arg(m_serial->errorString()));
    if (m_serial->isOpen()) {
        m_serial->close();
    }
    updateUiConnectedState(false);
}

void MainWindow::openMidiFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 MIDI 文件"),
        QString(),
        QStringLiteral("MIDI (*.mid *.midi);;All (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    const MidiParseResult r = MidiParser::parseFile(path);
    if (!r.ok) {
        QMessageBox::warning(this, QStringLiteral("解析失败"), r.error);
        appendLog(QStringLiteral("解析失败：%1").arg(r.error));
        return;
    }

    m_player->stop();
    m_player->setEvents(r.events);
    m_durationMs = r.durationMs;
    m_fileEdit->setText(path);

    int mapped = 0;
    int unmapped = 0;
    QSet<int> seenNotes;
    for (const MidiNoteEvent &ev : r.events) {
        if (ev.type != MidiEventType::NoteOn) {
            continue;
        }
        if (seenNotes.contains(ev.midiNote)) {
            continue;
        }
        seenNotes.insert(ev.midiNote);
        if (m_mapping.channelForMidiNote(ev.midiNote).has_value()) {
            ++mapped;
        } else {
            ++unmapped;
        }
    }

    appendLog(QStringLiteral(
                  "已加载：Format=%1 Tracks=%2 TPQ=%3 事件=%4 时长=%5 "
                  "曲中不同音高=%6（映射内%7 / 映射外%8）")
                  .arg(r.format)
                  .arg(r.trackCount)
                  .arg(r.ticksPerQuarter)
                  .arg(r.events.size())
                  .arg(formatMs(r.durationMs))
                  .arg(seenNotes.size())
                  .arg(mapped)
                  .arg(unmapped));

    m_timeLabel->setText(QStringLiteral("%1 / %2")
                             .arg(formatMs(0), formatMs(m_durationMs)));
    updatePlayUi();
}

void MainWindow::onPlay()
{
    if (!m_serial->isOpen()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先打开主板串口再播放。"));
        return;
    }
    m_player->play();
    updatePlayUi();
    appendLog(QStringLiteral("开始播放。"));
}

void MainWindow::onPause()
{
    m_player->pause();
    updatePlayUi();
    appendLog(QStringLiteral("已暂停。"));
}

void MainWindow::onStop()
{
    m_player->stop();
    sendCommandLine(QStringLiteral("ALL_OFF"));
    updatePlayUi();
    appendLog(QStringLiteral("已停止并 ALL_OFF。"));
}

void MainWindow::onAllOff()
{
    sendCommandLine(QStringLiteral("ALL_OFF"));
}

void MainWindow::onResetMapping()
{
    m_mapping.resetToDefault(KeyMapping::kDefaultBaseNote);
    rebuildMappingTable();
    appendLog(QStringLiteral("映射已恢复默认：C4(60)…D#5(75)。"));
}

void MainWindow::onMappingCellChanged(int row, int column)
{
    if (m_blockMapEdit || column != 2) {
        return;
    }
    if (row < 0 || row >= KeyMapping::kChannelCount) {
        return;
    }
    QTableWidgetItem *item = m_mapTable->item(row, column);
    if (!item) {
        return;
    }
    bool ok = false;
    const int note = item->text().trimmed().toInt(&ok);
    if (!ok || note < 0 || note > 127) {
        QMessageBox::warning(this, QStringLiteral("无效音高"),
                             QStringLiteral("MIDI 音高须为 0–127。"));
        rebuildMappingTable();
        return;
    }
    /* 禁止两通道映射同一音高 */
    for (int ch = 1; ch <= KeyMapping::kChannelCount; ++ch) {
        if (ch == row + 1) {
            continue;
        }
        if (m_mapping.midiNoteForChannel(ch) == note) {
            QMessageBox::warning(this, QStringLiteral("冲突"),
                                 QStringLiteral("音高 %1 已映射到通道 %2。")
                                     .arg(note)
                                     .arg(ch));
            rebuildMappingTable();
            return;
        }
    }
    m_mapping.setChannelMidiNote(row + 1, note);
    m_blockMapEdit = true;
    if (QTableWidgetItem *nameItem = m_mapTable->item(row, 3)) {
        nameItem->setText(KeyMapping::midiNoteToName(note));
    }
    m_blockMapEdit = false;
    appendLog(QStringLiteral("通道 %1 → MIDI %2 (%3)")
                  .arg(row + 1)
                  .arg(note)
                  .arg(KeyMapping::midiNoteToName(note)));
}

void MainWindow::onTempoChanged(int value)
{
    const double scale = value / 100.0;
    m_tempoLabel->setText(QStringLiteral("%1x").arg(scale, 0, 'f', 2));
    m_player->setTempoScale(scale);
}

void MainWindow::onChannelCommand(int channel, bool on)
{
    sendChannelState(channel, on);
}

void MainWindow::onUnmappedNote(int midiNote, bool on)
{
    if (on) {
        appendLog(QStringLiteral("跳过未映射音高 NoteOn %1 (%2)")
                      .arg(midiNote)
                      .arg(KeyMapping::midiNoteToName(midiNote)));
    }
}

void MainWindow::onPlayerFinished()
{
    sendCommandLine(QStringLiteral("ALL_OFF"));
    updatePlayUi();
    appendLog(QStringLiteral("播放结束。"));
}

void MainWindow::onPositionChanged(qint64 ms)
{
    m_timeLabel->setText(QStringLiteral("%1 / %2")
                             .arg(formatMs(ms), formatMs(m_durationMs)));
}

void MainWindow::updatePlayUi()
{
    const bool hasSong = !m_fileEdit->text().isEmpty();
    const bool playing = m_player->isPlaying() && !m_player->isPaused();
    const bool paused = m_player->isPaused();
    m_playBtn->setEnabled(hasSong && !playing);
    m_pauseBtn->setEnabled(playing);
    m_stopBtn->setEnabled(hasSong && (playing || paused));
    m_openBtn->setEnabled(!playing);
    m_resetMapBtn->setEnabled(!playing);
    m_mapTable->setEnabled(!playing);
}

bool MainWindow::sendCommandLine(const QString &line)
{
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("未开串口，忽略：%1").arg(line));
        return false;
    }
    const QByteArray payload = (line.trimmed() + QLatin1Char('\n')).toUtf8();
    const qint64 n = m_serial->write(payload);
    m_serial->flush();
    if (n != payload.size()) {
        appendLog(QStringLiteral("发送不完整：%1").arg(line));
        return false;
    }
    appendLog(QStringLiteral("[TX] %1").arg(line.trimmed()));
    return true;
}

void MainWindow::sendChannelState(int channel, bool on)
{
    if (channel < 1 || channel > KeyMapping::kChannelCount) {
        return;
    }
    sendCommandLine(QStringLiteral("%1 %2")
                        .arg(on ? QStringLiteral("ON") : QStringLiteral("OFF"))
                        .arg(channel));
}

void MainWindow::appendLog(const QString &text)
{
    m_logEdit->appendPlainText(text);
}

QString MainWindow::formatMs(qint64 ms)
{
    if (ms < 0) {
        ms = 0;
    }
    const qint64 totalSec = ms / 1000;
    const qint64 m = totalSec / 60;
    const qint64 s = totalSec % 60;
    const qint64 milli = ms % 1000;
    return QStringLiteral("%1:%2.%3")
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(milli, 3, 10, QLatin1Char('0'));
}

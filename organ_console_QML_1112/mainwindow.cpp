/**
 * @file mainwindow.cpp
 * @brief QML 后端实现：串口、MIDI、回放、录制、映射与调试开关。
 *
 * 本文件不再创建任何 QWidget 界面；所有界面由 qml/Main.qml 提供。
 */

#include "mainwindow.h"

#include "midiparser.h"
#include "midirecorder.h"
#include "ocffile.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QProcess>
#include <QSerialPortInfo>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

MainWindow::MainWindow(QObject *parent)
    : QObject(parent)
    , m_serial(new SerialClient(this))
    , m_aggregator(new KeyStateAggregator(m_serial, this))
    , m_midiIn(new MidiInputDevice(this))
    , m_player(new MidiPlayer(this))
    , m_recorder(new MidiRecorder(this))
    , m_keyHeld(KeyMapping::kMaxChannels, false)
{
    m_player->setMapping(&m_mapping);
    m_aggregator->setWindowMs(KeyStateAggregator::kDefaultWindowMs);
    loadSettings();

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

    connect(m_recorder, &MidiRecorder::recordingChanged,
            this, &MainWindow::recordingStateChanged);
    connect(m_recorder, &MidiRecorder::eventCountChanged,
            this, &MainWindow::recordingStateChanged);
    connect(m_recorder, &MidiRecorder::elapsedChanged,
            this, &MainWindow::recordingElapsedChanged);
}

MainWindow::~MainWindow()
{
    if (m_recorder->isRecording()) {
        QString error;
        m_recorder->stopAndSave(defaultRecordingPath(), error);
    }
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
    return m_serialPorts;
}

QStringList MainWindow::midiInputDevices() const
{
    return m_midiDevices;
}

bool MainWindow::midiInputOpen() const
{
    return m_midiIn->isOpen();
}

QString MainWindow::currentFile() const
{
    return m_currentFile;
}

bool MainWindow::playing() const
{
    return m_player->isPlaying() && !m_player->isPaused();
}

bool MainWindow::paused() const
{
    return m_player->isPaused();
}

bool MainWindow::recording() const
{
    return m_recorder && m_recorder->isRecording();
}

qint64 MainWindow::recordingElapsedMs() const
{
    return m_recorder ? m_recorder->elapsedMs() : 0;
}

int MainWindow::recordedEventCount() const
{
    return m_recorder ? m_recorder->eventCount() : 0;
}

bool MainWindow::hasUnsavedRecording() const
{
    return m_recorder && m_recorder->hasUnsaved();
}

QVariantList MainWindow::mappingRows() const
{
    QVariantList rows;
    for (const PitchMapEntry &entry : m_mapping.entries()) {
        QVariantMap row;
        row.insert(QStringLiteral("channel"), entry.channel);
        row.insert(QStringLiteral("board"), boardName(entry.channel));
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
        m_aggregator->allOffFlush();
        m_serial->closePort();
        m_aggregator->resetLocal();
        setActiveBitmap(0);
        appendLog(QStringLiteral("串口已关闭。"));
        return;
    }

    if (!m_serialPorts.contains(portName)) {
        appendLog(QStringLiteral("请选择一个可用串口。"));
        return;
    }
    if (!m_serial->openPort(portName, baudRate)) {
        return;
    }
    m_aggregator->resetLocal();
    m_serial->sendRemoteDebug(m_debugEnabled);
    appendLog(QStringLiteral("已打开 %1，DEBUG=%2，聚合窗=%3ms")
                  .arg(portName,
                       m_debugEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                  .arg(m_aggregator->windowMs()));
}

void MainWindow::setRemoteDebug(bool enabled)
{
    if (m_debugEnabled == enabled) {
        return;
    }
    m_debugEnabled = enabled;
    if (m_serial->isOpen()) {
        m_serial->sendRemoteDebug(enabled);
    }
    appendLog(enabled ? QStringLiteral("调试回报：开（下位机将打印 STATUS/CAN）")
                      : QStringLiteral("调试回报：关（仅指令下发）"));
    saveSettings();
    emit remoteDebugChanged();
}

void MainWindow::refreshMidiInputs()
{
    refreshMidiDevices();
}

void MainWindow::toggleMidiInput(int deviceIndex)
{
    if (m_midiIn->isOpen()) {
        m_midiIn->closeDevice();
        m_aggregator->allOffFlush();
        setActiveBitmap(0);
        updateLiveUi();
        appendLog(QStringLiteral("MIDI 输入已关闭。"));
        return;
    }

    if (deviceIndex < 0 || deviceIndex >= m_midiDevices.size()) {
        appendLog(QStringLiteral("没有可用的 MIDI 输入设备。"));
        return;
    }
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("串口未打开：MIDI 输入仅用于监听/录制，控制帧不会发送。"));
    }
    if (m_midiIn->openDevice(deviceIndex)) {
        appendLog(QStringLiteral("已打开 MIDI：%1").arg(m_midiDevices.at(deviceIndex)));
    }
    updateLiveUi();
}

void MainWindow::chooseControlFile()
{
    openMidiFile();
}

void MainWindow::convertMidiFile(const QString &mappingMode, int rangeStart,
                                 int rangeEnd, int windowMs)
{
    const QString inputPath = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("选择要转换的 MIDI"), QString(),
        QStringLiteral("MIDI (*.mid *.midi);;All (*.*)"));
    if (inputPath.isEmpty()) {
        return;
    }
    startOcfConversion(inputPath, mappingMode, rangeStart, rangeEnd, windowMs);
}

QString MainWindow::readOcfText(const QString &requestedPath)
{
    QString path = requestedPath;
    if (path.isEmpty()) {
        if (QFileInfo(m_currentFile).suffix().compare(QStringLiteral("ocf"),
                                                     Qt::CaseInsensitive) == 0
            && QFileInfo::exists(m_currentFile)) {
            path = m_currentFile;
        } else {
            QString initialDir = QFileInfo(m_currentFile).exists()
                ? QFileInfo(m_currentFile).absolutePath()
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
    m_tempoPercent = qBound(25, percent, 200);
    m_player->setTempoScale(m_tempoPercent / 100.0);
    emit playbackStateChanged();
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
    if (m_recorder->isRecording()) {
        return QStringLiteral("录制中不能修改键位映射，请先停止录制。");
    }
    if (m_player->isPlaying() || m_player->isPaused()) {
        return QStringLiteral("播放中不能修改键位映射，请先停止播放。");
    }
    if (m_midiIn->isOpen()) {
        return QStringLiteral("请先关闭 MIDI 输入，再修改键位映射。");
    }
    if (channel < 1 || channel > m_channelCount || midiNote < 0 || midiNote > 127) {
        return QStringLiteral("通道或 MIDI 音高超出范围。");
    }
    for (int other = 1; other <= m_channelCount; ++other) {
        if (other != channel && m_mapping.midiNoteForChannel(other) == midiNote) {
            return QStringLiteral("MIDI 音高 %1 已分配给通道 %2。").arg(midiNote).arg(other);
        }
    }
    m_mapping.setChannelMidiNote(channel, midiNote);
    saveSettings();
    emit mappingRowsChanged();
    return QString();
}

void MainWindow::resetMapping()
{
    onResetMapping();
}

QString MainWindow::setChannelCount(int count)
{
    if (count < 1 || count > KeyMapping::kMaxChannels) {
        return QStringLiteral("键位数必须在 1..%1 之间。").arg(KeyMapping::kMaxChannels);
    }
    if (count == m_channelCount) {
        return QString();
    }
    if (m_recorder->isRecording()) {
        return QStringLiteral("录制中不能修改键位数，请先停止录制。");
    }
    if (m_player->isPlaying() || m_player->isPaused()) {
        return QStringLiteral("播放中不能修改键位数，请先停止播放。");
    }
    if (m_midiIn->isOpen()) {
        return QStringLiteral("请先关闭 MIDI 输入，再修改键位数。");
    }

    /* 先按旧键位数发一帧全零，避免通道数缩小后高位残留 */
    m_aggregator->allOffFlush();
    if (!m_mapping.setChannelCount(count)) {
        return QStringLiteral("无法为新增通道找到足够的连续空闲音高，键位数未修改。");
    }

    m_channelCount = count;
    m_aggregator->setChannelCount(m_channelCount);
    m_keyHeld = QVector<bool>(m_channelCount, false);
    setActiveBitmap(0);
    saveSettings();
    emit channelCountChanged();
    emit mappingRowsChanged();
    appendLog(QStringLiteral("键位数已改为 %1；已有映射保留，新增通道自动分配连续空闲音高。")
                  .arg(m_channelCount));
    return QString();
}

void MainWindow::activateMode(int modeIndex)
{
    if (modeIndex < -1 || modeIndex > 2 || modeIndex == m_activeMode) {
        return;
    }
    m_activeMode = modeIndex;
    onModeTabChanged(modeIndex);
}

QString MainWindow::startRecording()
{
    if (m_recorder->isRecording()) {
        return QStringLiteral("已经在录制。");
    }
    if (m_player->isPlaying() || m_player->isPaused()) {
        return QStringLiteral("请先停止自动演奏，再开始录制。");
    }
    if (m_recorder->hasUnsaved()) {
        return QStringLiteral("有未保存的录制，请先保存或丢弃。");
    }

    QVector<int> notes;
    notes.reserve(m_channelCount);
    for (const PitchMapEntry &entry : m_mapping.entries()) {
        notes.push_back(entry.midiNote);
    }
    m_recorder->start(m_channelCount, notes);
    appendLog(QStringLiteral("开始录制 MIDI：实时 MIDI 原始音高 + 直接控制映射音高。"));
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("串口未打开，录制继续，但不会发送控制帧。"));
    }
    updatePlayUi();
    return QString();
}

QString MainWindow::stopRecording()
{
    if (!m_recorder->isRecording()) {
        return QStringLiteral("当前没有在录制。");
    }
    m_recorder->stop();
    appendLog(QStringLiteral("录制已停止，共 %1 个事件，等待保存。")
                  .arg(m_recorder->eventCount()));
    updatePlayUi();
    return saveRecording();
}

QString MainWindow::saveRecording()
{
    if (m_recorder->isRecording()) {
        return QStringLiteral("请先停止录制。");
    }
    if (m_recorder->eventCount() == 0) {
        m_recorder->cancel();
        emit recordingStateChanged();
        return QStringLiteral("没有可保存的录制。");
    }

    QString path = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("保存 MIDI 录制"), defaultRecordingPath(),
        QStringLiteral("MIDI 文件 (*.mid *.midi);;All (*.*)"));
    if (path.isEmpty()) {
        appendLog(QStringLiteral("已取消保存，录制保留在内存中，可再次保存或丢弃。"));
        return QString();
    }
    if (!path.endsWith(QStringLiteral(".mid"), Qt::CaseInsensitive)
        && !path.endsWith(QStringLiteral(".midi"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".mid");
    }

    QString error;
    if (!m_recorder->writeFile(path, error)) {
        appendLog(QStringLiteral("保存录制失败：%1").arg(error));
        emit recordingError(error);
        return error;
    }
    appendLog(QStringLiteral("录制已保存：%1").arg(path));
    emit recordingSaved(path);
    updatePlayUi();
    emit recordingStateChanged();
    return QString();
}

void MainWindow::discardRecording()
{
    if (!m_recorder->isRecording() && !m_recorder->hasUnsaved()) {
        return;
    }
    m_recorder->cancel();
    appendLog(QStringLiteral("已丢弃未保存的录制。"));
    updatePlayUi();
    emit recordingStateChanged();
}

void MainWindow::refreshPortList()
{
    m_serialPorts.clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        m_serialPorts.push_back(info.portName());
    }
    appendLog(QStringLiteral("串口列表：%1 个。").arg(m_serialPorts.size()));
    emit serialPortsChanged();
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
    Q_UNUSED(connected);
    emit serialStateChanged();
}

void MainWindow::onModeTabChanged(int index)
{
    Q_UNUSED(index);
    m_player->stop();
    releaseDirectKeys();
    if (m_recorder->isRecording()) {
        appendLog(QStringLiteral("录制中：切换页面保持 MIDI 输入与键位状态。"));
        updatePlayUi();
        updateLiveUi();
        return;
    }
    if (m_midiIn->isOpen()) {
        m_midiIn->closeDevice();
    }
    m_aggregator->allOffFlush();
    setActiveBitmap(0);
    updatePlayUi();
    updateLiveUi();
    appendLog(QStringLiteral("已切换模式，并刷出全关位图。"));
}

void MainWindow::refreshMidiDevices()
{
    m_midiDevices = m_midiIn->listDevices();
    appendLog(QStringLiteral("MIDI 输入设备：%1 个。").arg(m_midiDevices.size()));
    emit midiInputStateChanged();
}

void MainWindow::onLiveNoteOn(int midiNote, int velocity)
{
    if (m_recorder->isRecording()) {
        m_recorder->noteOn(midiNote, velocity);
    }
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
    if (m_recorder->isRecording()) {
        m_recorder->noteOff(midiNote);
    }
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
        m_currentFile = path;
        const int span = static_cast<int>(data.rangeEnd) - static_cast<int>(data.rangeStart) + 1;
        if (span > m_channelCount) {
            appendLog(QStringLiteral("OCF 含 %1 个音高槽，当前 N=%2；超出 N 的位将被忽略。")
                          .arg(span).arg(m_channelCount));
        }
        appendLog(QStringLiteral("已加载 OCF v%1：%2 条位图记录，%3 ms，CRC32 通过。")
                      .arg(data.version).arg(data.frames.size()).arg(data.durationMs));
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
    m_currentFile = path;

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
    updatePlayUi();
    emit fileStateChanged();
    emit playbackPositionChanged();
    return true;
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
    appendLog(QStringLiteral("开始转换 MIDI→OCF…"));
}

void MainWindow::onPlay()
{
    if (m_recorder->isRecording()) {
        appendLog(QStringLiteral("录制中不能开始自动演奏，请先保存或丢弃录制。"));
        return;
    }
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
    setActiveBitmap(0);
    updatePlayUi();
}

void MainWindow::onChannelCommand(int channel, bool on)
{
    sendChannel(channel, on);
}

void MainWindow::onBitmapCommand(quint32 bitmap)
{
    setActiveBitmap(bitmap);
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
    setActiveBitmap(0);
    updatePlayUi();
    appendLog(QStringLiteral("自动演奏结束。"));
}

void MainWindow::onPositionChanged(qint64 ms)
{
    m_positionMs = ms;
    emit playbackPositionChanged();
}

void MainWindow::updatePlayUi()
{
    emit playbackStateChanged();
}

void MainWindow::updateLiveUi()
{
    emit midiInputStateChanged();
}

void MainWindow::onDirectPressed(int channel)
{
    if (m_recorder->isRecording()) {
        const int note = m_mapping.midiNoteForChannel(channel);
        if (note >= 0) {
            m_recorder->noteOn(note, 100);
        }
    }
    sendChannel(channel, true);
}

void MainWindow::onDirectReleased(int channel)
{
    if (m_recorder->isRecording()) {
        const int note = m_mapping.midiNoteForChannel(channel);
        if (note >= 0) {
            m_recorder->noteOff(note);
        }
    }
    sendChannel(channel, false);
}

void MainWindow::onAllOffClicked()
{
    m_aggregator->allOffFlush();
    setActiveBitmap(0);
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
    if (m_recorder->isRecording()) {
        appendLog(QStringLiteral("录制中不能恢复默认映射。"));
        return;
    }
    if (m_player->isPlaying() || m_player->isPaused()) {
        appendLog(QStringLiteral("播放中不能恢复默认映射。"));
        return;
    }
    if (m_midiIn->isOpen()) {
        appendLog(QStringLiteral("请先关闭 MIDI 输入，再恢复默认映射。"));
        return;
    }
    m_mapping.resetToDefault(KeyMapping::kDefaultBaseNote, m_channelCount);
    saveSettings();
    emit mappingRowsChanged();
    appendLog(QStringLiteral("键位映射已恢复为 %1 键，通道1=%2。")
                  .arg(m_channelCount)
                  .arg(KeyMapping::midiNoteToName(m_mapping.midiNoteForChannel(1))));
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
    if (channel < 1 || channel > m_channelCount) {
        return;
    }
    const quint32 bit = 1u << (channel - 1);
    if (on) {
        m_activeBitmap |= bit;
    } else {
        m_activeBitmap &= ~bit;
    }
    m_activeBitmap &= channelMask();
    emit activeBitmapChanged();
}

void MainWindow::appendLog(const QString &text)
{
    m_logs.push_back(text);
    while (m_logs.size() > 800) {
        m_logs.pop_front();
    }
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
    case Qt::Key_U: return 17;
    case Qt::Key_I: return 18;
    case Qt::Key_O: return 19;
    case Qt::Key_P: return 20;
    case Qt::Key_A: return 21;
    case Qt::Key_S: return 22;
    case Qt::Key_D: return 23;
    case Qt::Key_F: return 24;
    case Qt::Key_G: return 25;
    case Qt::Key_H: return 26;
    case Qt::Key_J: return 27;
    case Qt::Key_K: return 28;
    case Qt::Key_L: return 29;
    case Qt::Key_Z: return 30;
    case Qt::Key_X: return 31;
    case Qt::Key_C: return 32;
    case Qt::Key_V: return 33; /**< 超出 32 键，实际不会使用 */
    default: return 0;
    }
}

void MainWindow::releaseDirectKeys()
{
    for (int i = 0; i < m_keyHeld.size(); ++i) {
        if (m_keyHeld[i]) {
            m_keyHeld[i] = false;
            onDirectReleased(i + 1);
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        releaseDirectKeys();
        return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
        return QObject::eventFilter(watched, event);
    }
    if (m_activeMode != 2) {
        return QObject::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return QObject::eventFilter(watched, event);
    }

    const int ch = keyToChannel(keyEvent->key());
    if (ch < 1 || ch > m_channelCount) {
        return QObject::eventFilter(watched, event);
    }
    if (keyEvent->isAutoRepeat()) {
        return true;
    }

    if (event->type() == QEvent::KeyPress) {
        if (!m_keyHeld[ch - 1]) {
            m_keyHeld[ch - 1] = true;
            onDirectPressed(ch);
        }
    } else {
        if (m_keyHeld[ch - 1]) {
            m_keyHeld[ch - 1] = false;
            onDirectReleased(ch);
        }
    }
    return true;
}

QString MainWindow::boardName(int channel)
{
    return QStringLiteral("N%1").arg((channel - 1) / 8 + 1);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    int count = settings.value(QStringLiteral("keyboard/channelCount"),
                               KeyMapping::kDefaultChannelCount).toInt();
    count = qBound(1, count, KeyMapping::kMaxChannels);

    m_mapping.resetToDefault(KeyMapping::kDefaultBaseNote, count);

    const QVariantList savedNotes = settings.value(QStringLiteral("keyboard/notes")).toList();
    bool applied = false;
    if (savedNotes.size() == count) {
        QSet<int> used;
        QVector<int> notes;
        notes.reserve(count);
        bool valid = true;
        for (const QVariant &value : savedNotes) {
            const int note = value.toInt();
            if (note < 0 || note > 127 || used.contains(note)) {
                valid = false;
                break;
            }
            used.insert(note);
            notes.push_back(note);
        }
        if (valid) {
            for (int i = 0; i < notes.size(); ++i) {
                m_mapping.setChannelMidiNote(i + 1, notes.at(i));
            }
            applied = true;
        }
    }
    if (!applied) {
        m_mapping.resetToDefault(KeyMapping::kDefaultBaseNote, count);
    }

    m_channelCount = count;
    m_aggregator->setChannelCount(m_channelCount);
    m_keyHeld = QVector<bool>(m_channelCount, false);
    m_debugEnabled = settings.value(QStringLiteral("debug/remote"), false).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("keyboard/channelCount"), m_channelCount);
    QVariantList notes;
    notes.reserve(m_channelCount);
    for (const PitchMapEntry &entry : m_mapping.entries()) {
        notes.push_back(entry.midiNote);
    }
    settings.setValue(QStringLiteral("keyboard/notes"), notes);
    settings.setValue(QStringLiteral("debug/remote"), m_debugEnabled);
}

QString MainWindow::defaultRecordingPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (base.isEmpty()) {
        base = QCoreApplication::applicationDirPath();
    }
    QString dir = QDir(base).filePath(QStringLiteral("OrganConsole"));
    if (!QDir().mkpath(dir)) {
        dir = QCoreApplication::applicationDirPath();
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return QDir(dir).filePath(QStringLiteral("organ_%1.mid").arg(stamp));
}

quint32 MainWindow::channelMask() const
{
    return m_channelCount >= KeyMapping::kMaxChannels
        ? 0xFFFFFFFFu
        : ((1u << m_channelCount) - 1u);
}

void MainWindow::setActiveBitmap(quint32 bitmap)
{
    bitmap &= channelMask();
    if (bitmap == m_activeBitmap) {
        return;
    }
    m_activeBitmap = bitmap;
    emit activeBitmapChanged();
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

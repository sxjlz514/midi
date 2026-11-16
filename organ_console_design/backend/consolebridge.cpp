/**
 * @file consolebridge.cpp
 * @brief 桥接层实现：串口/MIDI/播放/录制/映射，内部统一走 OCF 位图帧。
 */

#include "consolebridge.h"

#include "miditoocf.h"
#include "ocftomidi.h"
#include "ocfwriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QVariantMap>
#include <algorithm>

namespace {

constexpr int kMaxRecentFiles = 5;
constexpr int kMaxLogLines = 300;
constexpr int kRecordVelocity = 100;
constexpr int kRecordWindowMs = 4;

QString formatMs(qint64 ms)
{
    const qint64 total = qMax<qint64>(0, ms);
    const qint64 minutes = total / 60000;
    const qint64 seconds = (total % 60000) / 1000;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace

ConsoleBridge::ConsoleBridge(QObject *parent)
    : QObject(parent)
    , m_serial(new SerialClient(this))
    , m_aggregator(new KeyStateAggregator(m_serial, this))
    , m_midi(new MidiInputDevice(this))
    , m_player(new MidiPlayer(this))
{
    m_player->setMapping(&m_mapping);
    m_aggregator->setWindowMs(KeyStateAggregator::kDefaultWindowMs);
    m_aggregator->setChannelLimit(m_mapping.channelCount());

    m_recordWindowMs = kRecordWindowMs;
    m_recordFlushTimer.setSingleShot(true);
    m_recordFlushTimer.setInterval(m_recordWindowMs);
    connect(&m_recordFlushTimer, &QTimer::timeout, this, &ConsoleBridge::onRecordFlushTimeout);

    m_recordTick.setInterval(500);
    connect(&m_recordTick, &QTimer::timeout, this, &ConsoleBridge::onRecordTick);

    connect(m_serial, &SerialClient::bytesReceived, this, &ConsoleBridge::onSerialBytes);
    connect(m_serial, &SerialClient::errorOccurred, this, &ConsoleBridge::onSerialError);
    connect(m_serial, &SerialClient::connectionChanged,
            this, &ConsoleBridge::onSerialConnectionChanged);

    connect(m_midi, &MidiInputDevice::noteOn, this, &ConsoleBridge::onLiveNoteOn);
    connect(m_midi, &MidiInputDevice::noteOff, this, &ConsoleBridge::onLiveNoteOff);
    connect(m_midi, &MidiInputDevice::errorOccurred, this, &ConsoleBridge::onSerialError);

    connect(m_player, &MidiPlayer::bitmapCommand, this, &ConsoleBridge::onBitmapCommand);
    connect(m_player, &MidiPlayer::finished, this, &ConsoleBridge::onPlayerFinished);
    connect(m_player, &MidiPlayer::positionChanged, this, &ConsoleBridge::onPositionChanged);

    m_portWatch.setInterval(4000);
    connect(&m_portWatch, &QTimer::timeout, this, &ConsoleBridge::onPortWatch);

    m_boardProbe.setSingleShot(true);
    m_boardProbe.setInterval(1500);
    connect(&m_boardProbe, &QTimer::timeout, this, &ConsoleBridge::onBoardProbeTimeout);

    m_displayCount = m_mapping.channelCount();
    rebuildDisplayMapping();
}

ConsoleBridge::~ConsoleBridge()
{
    m_player->stop();
    m_midi->closeDevice();
    if (m_serial->isOpen()) {
        m_aggregator->allOffFlush();
        m_serial->closePort();
    }
    saveSettings();
}

QString ConsoleBridge::serialStatus() const
{
    if (!m_serial->isOpen()) {
        return QStringLiteral("未连接");
    }
    return m_boardOnline ? QStringLiteral("已连接") : QStringLiteral("等待主板应答");
}

void ConsoleBridge::initialize()
{
    loadSettings();
    refreshSerialPorts();
    refreshMidiDevices();
    setOcfDisplay(false);
    m_portWatch.start();

    QTimer::singleShot(0, this, [this]() {
        if (m_linkedPort.isEmpty()) {
            appendLog(QStringLiteral("没有记录过的串口，等待手动选择。"));
            return;
        }
        if (!m_portNames.contains(m_linkedPort)) {
            appendLog(QStringLiteral("上次的串口 %1 当前不存在，等待手动选择。").arg(m_linkedPort));
            emit toast(QStringLiteral("上次使用的串口 %1 未找到，请手动选择。").arg(m_linkedPort));
            m_linkedPort.clear();
            emit serialStateChanged();
            saveSettings();
            return;
        }
        connectSerial(m_linkedPort, m_baudRate);
    });
}

qint64 ConsoleBridge::livePositionMs() const
{
    if (m_player->isPlaying() || m_player->isPaused()) {
        return m_player->positionMs();
    }
    return m_positionMs;
}

QVariantList ConsoleBridge::mappingRows() const
{
    QVariantList rows;
    const auto &entries = m_mapping.entries();
    for (const PitchMapEntry &entry : entries) {
        QVariantMap row;
        row.insert(QStringLiteral("channel"), entry.channel);
        row.insert(QStringLiteral("board"),
                   KeyMapping::boardNameForChannel(entry.channel));
        row.insert(QStringLiteral("midiNote"), entry.midiNote);
        row.insert(QStringLiteral("noteName"), entry.noteName);
        rows.push_back(row);
    }
    return rows;
}

QVariantList ConsoleBridge::activeChannels() const
{
    QList<int> list = m_activeChannels.values();
    std::sort(list.begin(), list.end());
    QVariantList out;
    for (int channel : list) {
        out.push_back(channel);
    }
    return out;
}

// ---------------------------------------------------------------- 串口

void ConsoleBridge::refreshSerialPorts()
{
    QStringList names;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        names.push_back(info.portName());
    }
    if (m_portNames == names) {
        return;
    }
    m_portNames = names;
    emit serialPortsChanged();
    appendLog(QStringLiteral("串口列表：%1 个。").arg(names.size()));
}

bool ConsoleBridge::connectSerial(const QString &portName, int baud)
{
    if (portName.isEmpty()) {
        setLastError(QStringLiteral("请先选择一个串口。"));
        return false;
    }
    if (m_serial->isOpen()) {
        return true;
    }
    if (!m_serial->openPort(portName, baud)) {
        setLastError(QStringLiteral("无法打开串口 %1（%2）。").arg(portName).arg(baud));
        appendLog(m_lastError);
        return false;
    }
    m_linkedPort = portName;
    m_baudRate = baud;
    m_boardOnline = false;
    m_aggregator->resetLocal();
    emit boardStateChanged();
    emit serialStateChanged();
    saveSettings();

    m_boardProbe.start();
    m_serial->sendLine(QStringLiteral("PING"));
    appendLog(QStringLiteral("已打开 %1（%2），正在确认主板…").arg(portName).arg(baud));
    return true;
}

void ConsoleBridge::disconnectSerial()
{
    if (!m_serial->isOpen()) {
        return;
    }
    m_aggregator->allOffFlush();
    m_serial->closePort();
    m_aggregator->resetLocal();
    m_boardOnline = false;
    m_boardProbe.stop();
    emit boardStateChanged();
    emit serialStateChanged();
    appendLog(QStringLiteral("串口已关闭。"));
}

void ConsoleBridge::setBaudRate(int baud)
{
    if (baud <= 0 || baud == m_baudRate) {
        return;
    }
    m_baudRate = baud;
    saveSettings();
    emit serialStateChanged();
    if (m_serial->isOpen()) {
        appendLog(QStringLiteral("波特率改为 %1，将重新连接。").arg(baud));
        const QString port = m_linkedPort;
        disconnectSerial();
        connectSerial(port, baud);
    }
}

void ConsoleBridge::onSerialBytes(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        return;
    }
    if (t.contains(QStringLiteral("PONG ORGAN"))) {
        if (!m_boardOnline) {
            m_boardOnline = true;
            m_boardProbe.stop();
            emit boardStateChanged();
            appendLog(QStringLiteral("主板已应答。"));
        }
        return;
    }
    if (t.contains(QStringLiteral("@OK"))) {
        if (!m_boardOnline) {
            m_boardOnline = true;
            m_boardProbe.stop();
            emit boardStateChanged();
        }
        return;
    }
    if (t.contains(QStringLiteral("@ERR"))) {
        setLastError(QStringLiteral("主板回报错误：%1").arg(t));
        appendLog(m_lastError);
        return;
    }
}

void ConsoleBridge::onSerialError(const QString &text)
{
    setLastError(text);
    appendLog(QStringLiteral("错误：%1").arg(text));
}

void ConsoleBridge::onSerialConnectionChanged(bool connected)
{
    if (!connected) {
        m_boardOnline = false;
        emit boardStateChanged();
    }
    emit serialStateChanged();
}

void ConsoleBridge::onBoardProbeTimeout()
{
    if (m_boardOnline) {
        return;
    }
    appendLog(QStringLiteral("串口已打开，但主板没有应答 PING。"));
    emit toast(QStringLiteral("串口已打开，但主板没有应答，请检查线缆与波特率。"));
}

void ConsoleBridge::onPortWatch()
{
    if (!m_serial->isOpen()) {
        return;
    }
    const auto ports = QSerialPortInfo::availablePorts();
    bool present = false;
    for (const QSerialPortInfo &info : ports) {
        if (info.portName() == m_linkedPort) {
            present = true;
            break;
        }
    }
    if (!present) {
        appendLog(QStringLiteral("主板串口已断开。"));
        m_aggregator->allOffFlush();
        m_serial->closePort();
        m_aggregator->resetLocal();
        m_boardOnline = false;
        emit boardStateChanged();
        emit serialStateChanged();
        emit toast(QStringLiteral("主板串口已断开。"));
    }
}

// ---------------------------------------------------------------- MIDI 输入

void ConsoleBridge::refreshMidiDevices()
{
    emit midiInputStateChanged();
}

bool ConsoleBridge::connectMidi(int deviceIndex)
{
    if (channelGuard()) {
        setLastError(QStringLiteral("播放期间不能打开 MIDI 输入。"));
        return false;
    }
    if (!m_midi->openDevice(deviceIndex)) {
        setLastError(QStringLiteral("无法打开 MIDI 输入设备。"));
        appendLog(m_lastError);
        return false;
    }
    emit midiInputStateChanged();
    appendLog(QStringLiteral("MIDI 输入已打开。"));
    return true;
}

void ConsoleBridge::disconnectMidi()
{
    if (!m_midi->isOpen()) {
        return;
    }
    m_midi->closeDevice();
    emit midiInputStateChanged();
    appendLog(QStringLiteral("MIDI 输入已关闭。"));
}

void ConsoleBridge::onLiveNoteOn(int midiNote, int velocity)
{
    Q_UNUSED(velocity);
    if (channelGuard() || m_ocfDisplay) {
        emit keyboardLocked(true);
        return;
    }
    const auto channel = m_mapping.channelForMidiNote(midiNote);
    if (!channel.has_value()) {
        return;
    }
    applyChannelState(*channel, true);
}

void ConsoleBridge::onLiveNoteOff(int midiNote, int velocity)
{
    Q_UNUSED(velocity);
    if (channelGuard() || m_ocfDisplay) {
        emit keyboardLocked(false);
        return;
    }
    const auto channel = m_mapping.channelForMidiNote(midiNote);
    if (!channel.has_value()) {
        return;
    }
    applyChannelState(*channel, false);
}

// ---------------------------------------------------------------- 通道与显示

bool ConsoleBridge::channelGuard() const
{
    return m_player->isPlaying() || m_player->isPaused();
}

void ConsoleBridge::applyChannelState(int channel, bool on)
{
    if (channel < 1 || channel > m_mapping.channelCount()) {
        return;
    }
    if (m_recording) {
        markRecordBitmap(channel, on);
    }
    if (m_serial->isOpen()) {
        m_aggregator->setChannel(channel, on);
    }
    setActiveChannel(channel, on);
    emit liveNote(channel, on);
}

void ConsoleBridge::setActiveChannel(int channel, bool on)
{
    const int before = m_activeChannels.size();
    if (on) {
        m_activeChannels.insert(channel);
    } else {
        m_activeChannels.remove(channel);
    }
    if (m_activeChannels.size() != before) {
        emit activeChannelsChanged();
    }
}

void ConsoleBridge::pressChannel(int channel)
{
    if (channelGuard() || m_ocfDisplay) {
        emit keyboardLocked(true);
        return;
    }
    applyChannelState(channel, true);
}

void ConsoleBridge::releaseChannel(int channel)
{
    if (channelGuard() || m_ocfDisplay) {
        emit keyboardLocked(false);
        return;
    }
    applyChannelState(channel, false);
}

void ConsoleBridge::onBitmapCommand(quint32 bitmap)
{
    const int count = qBound(1, m_displayCount, 32);
    const quint32 mask = count >= 32 ? 0xFFFFFFFFu : ((1u << count) - 1u);
    const quint32 masked = bitmap & mask;

    if (m_serial->isOpen()) {
        m_aggregator->setBitmapAndFlush(masked);
    }

    m_activeChannels.clear();
    for (int bit = 0; bit < count; ++bit) {
        if ((masked >> bit) & 1u) {
            m_activeChannels.insert(bit + 1);
        }
    }
    emit activeChannelsChanged();
}

void ConsoleBridge::setOcfDisplay(bool ocf)
{
    m_ocfDisplay = ocf && m_hasCurrent;
    if (m_ocfDisplay) {
        const int span = static_cast<int>(m_current.rangeEnd)
                         - static_cast<int>(m_current.rangeStart) + 1;
        m_displayCount = qBound(1, span, 32);
    } else {
        m_displayCount = m_mapping.channelCount();
    }
    rebuildDisplayMapping();
    emit displayChanged();
}

void ConsoleBridge::rebuildDisplayMapping()
{
    QVariantList list;
    if (m_ocfDisplay && m_hasCurrent) {
        const int low = static_cast<int>(m_current.rangeStart);
        for (int i = 0; i < m_displayCount; ++i) {
            const int note = low + i;
            QVariantMap row;
            row.insert(QStringLiteral("channel"), i + 1);
            row.insert(QStringLiteral("midiNote"), note);
            row.insert(QStringLiteral("noteName"), KeyMapping::midiNoteToName(note));
            row.insert(QStringLiteral("board"),
                       KeyMapping::boardNameForChannel(i + 1));
            list.push_back(row);
        }
    } else {
        for (int channel = 1; channel <= m_displayCount; ++channel) {
            const int note = m_mapping.midiNoteForChannel(channel);
            QVariantMap row;
            row.insert(QStringLiteral("channel"), channel);
            row.insert(QStringLiteral("midiNote"), note);
            row.insert(QStringLiteral("noteName"), KeyMapping::midiNoteToName(note));
            row.insert(QStringLiteral("board"),
                       KeyMapping::boardNameForChannel(channel));
            list.push_back(row);
        }
    }
    m_displayMapping = list;
}

int ConsoleBridge::displayNoteForChannel(int channel) const
{
    if (channel < 1 || channel > m_displayMapping.size()) {
        return -1;
    }
    return m_displayMapping.at(channel - 1).toMap().value(QStringLiteral("midiNote")).toInt();
}

void ConsoleBridge::rebuildWaterfallNotes()
{
    QVariantList notes;
    if (!m_hasCurrent) {
        m_waterfallNotes.clear();
        emit waterfallNotesChanged();
        return;
    }

    const int count = qBound(1, m_displayCount, 32);
    QVector<qint64> starts(count + 1, -1);
    quint32 previous = 0;

    auto closeChannel = [&](int channel, qint64 endMs) {
        if (channel < 1 || channel > count || starts[channel] < 0) {
            return;
        }
        const qint64 start = starts[channel];
        starts[channel] = -1;
        QVariantMap row;
        row.insert(QStringLiteral("channel"), channel);
        row.insert(QStringLiteral("startMs"), start);
        row.insert(QStringLiteral("endMs"), qMax(start + 18, endMs));
        row.insert(QStringLiteral("midiNote"), displayNoteForChannel(channel));
        notes.push_back(row);
    };

    for (const OcfFrame &frame : m_current.frames) {
        const qint64 t = static_cast<qint64>(frame.timeMs);
        const quint32 current = frame.bitmap;
        for (int bit = 0; bit < count; ++bit) {
            const quint32 mask = 1u << bit;
            const bool was = (previous & mask) != 0;
            const bool now = (current & mask) != 0;
            if (!was && now) {
                starts[bit + 1] = t;
            } else if (was && !now) {
                closeChannel(bit + 1, t);
            }
        }
        previous = current;
    }
    for (int channel = 1; channel <= count; ++channel) {
        closeChannel(channel, m_durationMs);
    }

    m_waterfallNotes = notes;
    emit waterfallNotesChanged();
}

// ---------------------------------------------------------------- 文件与播放

QString ConsoleBridge::loadMidiAsOcf(const QString &path, int rangeStart,
                                     int rangeEnd, bool wrap, int windowMs)
{
    if (channelGuard() || m_recording) {
        return QStringLiteral("播放或录制期间不能加载文件。");
    }
    const MidiParseResult parsed = MidiParser::parseFile(path);
    if (!parsed.ok) {
        const QString error = QStringLiteral("MIDI 解析失败：%1").arg(parsed.error);
        appendLog(error);
        return error;
    }
    if (rangeStart < 0 || rangeEnd > 127 || rangeStart > rangeEnd
        || (rangeEnd - rangeStart + 1) > m_mapping.channelCount()) {
        const QString error = QStringLiteral("范围必须从低到高，且跨度不超过当前键位数 %1。")
                                  .arg(m_mapping.channelCount());
        appendLog(error);
        return error;
    }

    MidiToOcf::Options options;
    options.rangeStart = rangeStart;
    options.rangeEnd = rangeEnd;
    options.wrap = wrap;
    options.windowMs = qBound(1, windowMs, 65535);

    OcfData data;
    QString error;
    if (!MidiToOcf::convert(parsed.events, parsed.durationMs, options, data, error)) {
        appendLog(error);
        return error;
    }

    loadFrameData(data, path, false);
    appendLog(QStringLiteral("已加载 %1：%2 帧，%3 ms。")
                  .arg(QFileInfo(path).fileName())
                  .arg(data.frames.size())
                  .arg(data.durationMs));
    return QString();
}

QString ConsoleBridge::loadOcfFile(const QString &path)
{
    if (channelGuard() || m_recording) {
        return QStringLiteral("播放或录制期间不能加载文件。");
    }
    OcfData data;
    QString error;
    if (!OcfFile::readFile(path, data, error)) {
        appendLog(error);
        return error;
    }
    loadFrameData(data, path, false);
    appendLog(QStringLiteral("已加载 OCF：%1，%2 帧。")
                  .arg(QFileInfo(path).fileName())
                  .arg(data.frames.size()));
    return QString();
}

QString ConsoleBridge::openFileAuto(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("ocf")) {
        return loadOcfFile(path);
    }
    if (suffix == QLatin1String("mid") || suffix == QLatin1String("midi")) {
        const int count = m_mapping.channelCount();
        int low = 127;
        int high = 0;
        for (int channel = 1; channel <= count; ++channel) {
            const int note = m_mapping.midiNoteForChannel(channel);
            if (note >= 0) {
                low = qMin(low, note);
                high = qMax(high, note);
            }
        }
        if (low > high) {
            low = m_baseNote;
            high = m_baseNote + count - 1;
        }
        const int start = qMax(0, qMin(low, 127 - (count - 1)));
        const int end = start + count - 1;
        return loadMidiAsOcf(path, start, end, false, 30);
    }
    return QStringLiteral("不支持的文件类型：%1").arg(suffix);
}

void ConsoleBridge::loadFrameData(const OcfData &data, const QString &path, bool unsaved)
{
    m_current = data;
    m_durationMs = static_cast<qint64>(data.durationMs);
    m_hasCurrent = true;
    m_unsaved = unsaved;
    m_currentFile = path;
    m_positionMs = 0;

    setOcfDisplay(true);
    rebuildWaterfallNotes();
    emit fileStateChanged();
    emit playbackPositionChanged();
    if (!path.isEmpty()) {
        addRecent(path);
    }
}

void ConsoleBridge::play()
{
    if (!m_hasCurrent || m_current.frames.isEmpty()) {
        emit toast(QStringLiteral("请先打开文件或录制一段演奏。"));
        return;
    }
    if (!m_serial->isOpen() && !m_virtualPlayback) {
        setLastError(QStringLiteral("没有连接主板，也没有开启虚拟演奏。"));
        appendLog(m_lastError);
        return;
    }
    if (m_midi->isOpen()) {
        m_midi->closeDevice();
        emit midiInputStateChanged();
        appendLog(QStringLiteral("播放期间已关闭 MIDI 输入。"));
    }
    m_player->setFrames(m_current.frames, m_durationMs);
    m_player->play();
    setOcfDisplay(true);
    rebuildWaterfallNotes();
    emit playbackStateChanged();
    appendLog(QStringLiteral("开始播放。"));
}

void ConsoleBridge::pause()
{
    if (!m_player->isPlaying() || m_player->isPaused()) {
        return;
    }
    m_player->pause();
    emit playbackStateChanged();
    emit playbackPositionChanged();
    appendLog(QStringLiteral("已暂停。"));
}

void ConsoleBridge::stop()
{
    const bool was = m_player->isPlaying() || m_player->isPaused();
    m_player->stop();
    m_positionMs = 0;
    if (m_serial->isOpen()) {
        m_aggregator->allOffFlush();
    }
    m_activeChannels.clear();
    emit activeChannelsChanged();
    m_waterfallNotes.clear();
    emit waterfallNotesChanged();
    emit playbackStateChanged();
    emit playbackPositionChanged();
    if (was) {
        appendLog(QStringLiteral("已停止。"));
    }
}

void ConsoleBridge::closeFile()
{
    if (!m_hasCurrent && m_currentFile.isEmpty()) {
        return;
    }
    if (m_player->isPlaying() || m_player->isPaused()) {
        m_player->stop();
    }
    if (m_serial->isOpen()) {
        m_aggregator->allOffFlush();
    }
    m_activeChannels.clear();
    emit activeChannelsChanged();

    m_current = OcfData();
    m_hasCurrent = false;
    m_unsaved = false;
    m_currentFile.clear();
    m_durationMs = 0;
    m_positionMs = 0;
    setOcfDisplay(false);
    m_waterfallNotes.clear();
    emit waterfallNotesChanged();
    emit fileStateChanged();
    emit playbackStateChanged();
    emit playbackPositionChanged();
    appendLog(QStringLiteral("已关闭文件。"));
}

void ConsoleBridge::onPlayerFinished()
{
    m_positionMs = 0;
    m_activeChannels.clear();
    emit activeChannelsChanged();
    if (m_serial->isOpen()) {
        m_aggregator->allOffFlush();
    }
    m_waterfallNotes.clear();
    emit waterfallNotesChanged();
    emit playbackStateChanged();
    emit playbackPositionChanged();
    appendLog(QStringLiteral("演奏结束。"));
}

void ConsoleBridge::onPositionChanged(qint64 ms)
{
    m_positionMs = ms;
    emit playbackPositionChanged();
}

// ---------------------------------------------------------------- 录制

QString ConsoleBridge::startRecording()
{
    if (channelGuard()) {
        return QStringLiteral("播放期间不能录制。");
    }
    if (m_recording) {
        return QStringLiteral("已经在录制中。");
    }

    m_recordFrames.clear();
    m_recordBitmap = 0;
    m_recordLastEmitted = 0;
    m_recordClock.start();
    m_recordElapsedMs = 0;
    m_recording = true;
    m_recordTick.start();
    m_recordFlushTimer.stop();

    m_hasCurrent = false;
    m_unsaved = false;
    m_currentFile.clear();
    m_waterfallNotes.clear();
    setOcfDisplay(false);

    emit recordingChanged();
    emit fileStateChanged();
    emit waterfallNotesChanged();
    appendLog(QStringLiteral("开始录制，聚合窗 %1 ms。").arg(m_recordWindowMs));
    return QString();
}

QString ConsoleBridge::stopRecording()
{
    if (!m_recording) {
        return QString();
    }
    m_recording = false;
    m_recordTick.stop();
    m_recordFlushTimer.stop();

    const qint64 elapsed = qMax<qint64>(0, m_recordClock.elapsed());
    if (m_recordBitmap != m_recordLastEmitted) {
        OcfFrame frame;
        frame.timeMs = static_cast<quint64>(elapsed);
        frame.bitmap = m_recordBitmap;
        m_recordFrames.push_back(frame);
        m_recordLastEmitted = m_recordBitmap;
    }

    const int channelCount = m_mapping.channelCount();
    int minNote = 127;
    for (int channel = 1; channel <= channelCount; ++channel) {
        const int note = m_mapping.midiNoteForChannel(channel);
        if (note >= 0) {
            minNote = qMin(minNote, note);
        }
    }
    if (minNote > 127 - (channelCount - 1)) {
        minNote = 127 - (channelCount - 1);
    }
    minNote = qMax(0, minNote);

    OcfData data;
    data.version = 1;
    data.bitmapBits = 32;
    data.mappingMode = 0;
    data.rangeStart = static_cast<quint8>(minNote);
    data.rangeEnd = static_cast<quint8>(minNote + channelCount - 1);
    data.aggregationWindowMs = static_cast<quint16>(m_recordWindowMs);
    data.durationMs = static_cast<quint64>(elapsed);
    data.frames = m_recordFrames;

    if (data.frames.isEmpty() || data.frames.last().bitmap != 0) {
        OcfFrame tail;
        tail.timeMs = static_cast<quint64>(elapsed);
        tail.bitmap = 0;
        data.frames.push_back(tail);
    }

    m_recordElapsedMs = elapsed;
    m_current = data;
    m_durationMs = elapsed;
    m_hasCurrent = true;
    m_unsaved = true;
    m_currentFile.clear();

    setOcfDisplay(true);
    rebuildWaterfallNotes();
    emit recordingChanged();
    emit fileStateChanged();
    emit playbackPositionChanged();
    appendLog(QStringLiteral("录制结束：%1。").arg(formatMs(elapsed)));
    return QString();
}

void ConsoleBridge::markRecordBitmap(int channel, bool on)
{
    if (!m_recording || channel < 1 || channel > 32) {
        return;
    }
    const quint32 bit = 1u << (channel - 1);
    if (on) {
        m_recordBitmap |= bit;
    } else {
        m_recordBitmap &= ~bit;
    }
    m_recordFlushTimer.start();
}

void ConsoleBridge::onRecordFlushTimeout()
{
    if (!m_recording || m_recordBitmap == m_recordLastEmitted) {
        return;
    }
    OcfFrame frame;
    frame.timeMs = static_cast<quint64>(qMax<qint64>(0, m_recordClock.elapsed()));
    frame.bitmap = m_recordBitmap;
    m_recordFrames.push_back(frame);
    m_recordLastEmitted = m_recordBitmap;
}

void ConsoleBridge::onRecordTick()
{
    if (!m_recording) {
        return;
    }
    m_recordElapsedMs = qMax<qint64>(0, m_recordClock.elapsed());
    emit recordingChanged();
}

// ---------------------------------------------------------------- 保存

QString ConsoleBridge::saveCurrent(const QString &path)
{
    if (!m_hasCurrent) {
        return QStringLiteral("没有可保存的内容。");
    }
    if (m_recording) {
        return QStringLiteral("请先停止录制。");
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    QString error;
    if (suffix == QLatin1String("ocf")) {
        if (!OcfWriter::writeFile(path, m_current, error)) {
            appendLog(error);
            return error;
        }
    } else if (suffix == QLatin1String("mid") || suffix == QLatin1String("midi")) {
        if (!OcfToMidi::exportMidi(path, m_current.frames, m_durationMs,
                                   m_mapping, kRecordVelocity, error)) {
            appendLog(error);
            return error;
        }
    } else {
        return QStringLiteral("请选择 .ocf 或 .mid 后缀。");
    }

    m_currentFile = path;
    m_unsaved = false;
    addRecent(path);
    emit fileStateChanged();
    appendLog(QStringLiteral("已保存：%1").arg(path));
    return QString();
}

// ---------------------------------------------------------------- 映射

QString ConsoleBridge::setMappingNote(int channel, int midiNote, bool cascade)
{
    if (channelGuard() || m_recording || m_midi->isOpen()) {
        return QStringLiteral("播放、录制或 MIDI 输入开启时不能修改映射。");
    }
    const int count = m_mapping.channelCount();
    if (channel < 1 || channel > count) {
        return QStringLiteral("通道超出范围。");
    }
    if (midiNote < 0 || midiNote > 127) {
        return QStringLiteral("音高必须在 0..127。");
    }

    QVector<int> notes(count + 1, -1);
    for (int ch = 1; ch <= count; ++ch) {
        notes[ch] = m_mapping.midiNoteForChannel(ch);
    }

    if (cascade) {
        for (int ch = channel; ch <= count; ++ch) {
            const int note = midiNote + (ch - channel);
            if (note > 127) {
                return QStringLiteral("顺移后音高超过 127，请从更低的音开始。");
            }
            notes[ch] = note;
        }
    } else {
        notes[channel] = midiNote;
    }

    for (int a = 1; a <= count; ++a) {
        for (int b = a + 1; b <= count; ++b) {
            if (notes[a] == notes[b]) {
                return QStringLiteral("音高 %1 已分配给通道 %2。").arg(notes[a]).arg(a);
            }
        }
    }

    for (int ch = 1; ch <= count; ++ch) {
        m_mapping.setChannelMidiNote(ch, notes[ch]);
    }
    m_mappingEdited = true;
    rebuildDisplayMapping();
    if (!m_ocfDisplay) {
        rebuildWaterfallNotes();
    }
    emit mappingRowsChanged();
    emit displayChanged();
    saveSettings();
    appendLog(QStringLiteral("映射已更新：通道 %1 → %2。")
                  .arg(channel)
                  .arg(KeyMapping::midiNoteToName(midiNote)));
    return QString();
}

void ConsoleBridge::setVirtualPlayback(bool enabled)
{
    if (m_virtualPlayback == enabled) {
        return;
    }
    m_virtualPlayback = enabled;
    emit virtualPlaybackChanged();
    saveSettings();
    appendLog(enabled ? QStringLiteral("虚拟演奏：开。") : QStringLiteral("虚拟演奏：关。"));
}

void ConsoleBridge::clearLog()
{
    m_logs.clear();
    emit logLinesChanged();
}

// ---------------------------------------------------------------- 设置与日志

QString ConsoleBridge::settingsFilePath(bool forWrite) const
{
    const QString primary = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("setting.txt"));
    const QString fallback = QDir(QStandardPaths::writableLocation(
                                      QStandardPaths::AppConfigLocation))
                                 .filePath(QStringLiteral("setting.txt"));
    if (!forWrite) {
        if (QFileInfo::exists(primary)) {
            return primary;
        }
        return fallback;
    }
    QFile probe(primary);
    if (probe.open(QIODevice::WriteOnly | QIODevice::Append)) {
        probe.close();
        return primary;
    }
    QDir().mkpath(QFileInfo(fallback).absolutePath());
    return fallback;
}

void ConsoleBridge::loadSettings()
{
    const QString path = settingsFilePath(false);
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 首次运行：按 BaseNote 生成整张映射。
        m_mapping.resetToDefault(m_baseNote);
        m_aggregator->setChannelLimit(m_mapping.channelCount());
        m_displayCount = m_mapping.channelCount();
        rebuildDisplayMapping();
        return;
    }

    QHash<QString, QString> kv;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        kv.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    file.close();

    bool ok = false;
    const int channelCount = kv.value(QStringLiteral("ChannelCount")).toInt(&ok);
    if (ok && channelCount >= 1 && channelCount <= KeyMapping::kMaxChannelCount) {
        m_mapping.setChannelCount(channelCount);
    }

    const int base = kv.value(QStringLiteral("BaseNote")).toInt(&ok);
    if (ok && base >= 0 && base <= 127) {
        m_baseNote = base;
    }

    const QStringList mapParts = kv.value(QStringLiteral("Mapping"))
                                     .split(QLatin1Char(','), Qt::SkipEmptyParts);
    bool mappingApplied = false;
    if (mapParts.size() >= m_mapping.channelCount()) {
        QVector<int> notes(m_mapping.channelCount(), -1);
        bool valid = true;
        for (int i = 0; i < notes.size(); ++i) {
            bool noteOk = false;
            const int note = mapParts.at(i).trimmed().toInt(&noteOk);
            if (!noteOk || note < 0 || note > 127) {
                valid = false;
                break;
            }
            notes[i] = note;
        }
        if (valid) {
            for (int i = 0; i < notes.size(); ++i) {
                if (notes.contains(notes.at(i)) && notes.indexOf(notes.at(i)) != i) {
                    valid = false;
                    break;
                }
            }
        }
        if (valid) {
            for (int i = 0; i < notes.size(); ++i) {
                m_mapping.setChannelMidiNote(i + 1, notes.at(i));
            }
            mappingApplied = true;
            m_mappingEdited = true;
        }
    }
    if (!mappingApplied) {
        m_mapping.resetToDefault(m_baseNote);
    }

    const int baud = kv.value(QStringLiteral("BaudRate")).toInt(&ok);
    if (ok && baud > 0) {
        m_baudRate = baud;
    }
    m_linkedPort = kv.value(QStringLiteral("PortName"));

    const QString virt = kv.value(QStringLiteral("VirtualPlayback")).trimmed().toLower();
    m_virtualPlayback = (virt == QLatin1String("1") || virt == QLatin1String("true")
                         || virt == QLatin1String("yes") || virt == QLatin1String("on"));

    m_recentFiles = kv.value(QStringLiteral("RecentFiles"))
                        .split(QLatin1Char('|'), Qt::SkipEmptyParts);
    while (m_recentFiles.size() > kMaxRecentFiles) {
        m_recentFiles.removeLast();
    }

    m_aggregator->setChannelLimit(m_mapping.channelCount());
    m_displayCount = m_mapping.channelCount();
    rebuildDisplayMapping();
    appendLog(QStringLiteral("已加载设置：%1").arg(QDir::toNativeSeparators(path)));
}

void ConsoleBridge::saveSettings() const
{
    const QString path = settingsFilePath(true);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "# Organ Console settings (auto-saved)\n";
    out << "ChannelCount=" << m_mapping.channelCount() << '\n';
    out << "BaseNote=" << m_baseNote << '\n';
    out << "BaudRate=" << m_baudRate << '\n';
    out << "PortName=" << m_linkedPort << '\n';
    out << "VirtualPlayback=" << (m_virtualPlayback ? 1 : 0) << '\n';
    if (m_mappingEdited) {
        QStringList mapNotes;
        for (int ch = 1; ch <= m_mapping.channelCount(); ++ch) {
            mapNotes.push_back(QString::number(m_mapping.midiNoteForChannel(ch)));
        }
        out << "Mapping=" << mapNotes.join(QLatin1Char(',')) << '\n';
    }
    out << "RecentFiles=" << m_recentFiles.join(QLatin1Char('|')) << '\n';
    file.close();
}

void ConsoleBridge::addRecent(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > kMaxRecentFiles) {
        m_recentFiles.removeLast();
    }
    emit fileStateChanged();
    saveSettings();
}

void ConsoleBridge::appendLog(const QString &text)
{
    m_logs.push_back(text);
    while (m_logs.size() > kMaxLogLines) {
        m_logs.pop_front();
    }
    if (m_logNotifyPending) {
        return;
    }
    m_logNotifyPending = true;
    QTimer::singleShot(80, this, [this]() {
        m_logNotifyPending = false;
        emit logLinesChanged();
    });
}

void ConsoleBridge::setLastError(const QString &text)
{
    m_lastError = text;
    emit lastErrorChanged();
    emit toast(text);
}

#include "mainwindow.h"
#include "devicebar.h"
#include "midifiledecoder.h"
#include "midifileplayer.h"
#include "midiinput.h"
#include "midiinputpanel.h"
#include "midikeyrouter.h"
#include "midirecorder.h"
#include "serialcommand.h"
#include "serialpanel.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

QString formatDuration(quint32 milliseconds)
{
    const quint32 totalSeconds = milliseconds / 1000;

    return QStringLiteral("%1:%2.%3")
        .arg(totalSeconds / 60)
        .arg(
            totalSeconds % 60,
            2,
            10,
            QLatin1Char('0')
            )
        .arg(
            (milliseconds % 1000) / 100,
            1,
            10,
            QLatin1Char('0')
            );
}

}   /* namespace */

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(
        QStringLiteral("SMK25Mini MIDI工具")
        );

    resize(1000, 680);

    m_midi = new MidiInput(this);
    m_serial = new SerialCommand(this);
    m_router = new MidiKeyRouter(this);
    m_player = new MidiFilePlayer(this);
    m_recorder = new MidiRecorder(this);

    m_router->setSerial(m_serial);

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_deviceBar = new DeviceBar(m_midi, m_serial, central);
    centralLayout->addWidget(m_deviceBar);

    m_tabs = new QTabWidget(central);

    m_tabs->addTab(
        createLivePage(),
        QStringLiteral("MIDI输入")
        );

    m_tabs->addTab(
        createRecordPage(),
        QStringLiteral("录制")
        );

    m_serialPanel = new SerialPanel(m_serial, m_router, m_tabs);

    m_tabs->addTab(
        m_serialPanel,
        QStringLiteral("下位机")
        );

    centralLayout->addWidget(m_tabs, 1);

    setCentralWidget(central);

    m_boardStatusLabel =
        new QLabel(QStringLiteral("下位机：未连接"));

    statusBar()->addPermanentWidget(m_boardStatusLabel);

    /* 页签3的映射设置 → 路由器 */
    m_router->setMapping(m_serialPanel->mapping());

    connect(
        m_serialPanel,
        &SerialPanel::mappingChanged,
        this,
        [this](const MidiKeyMapping &mapping) {
            m_router->setMapping(mapping);
        }
        );

    /* 唯一的MIDI输入扇出到：两个日志面板、录制、转发、映射页 */
    connect(
        m_midi,
        &MidiInput::messageReceived,
        m_livePanel,
        &MidiInputPanel::showMessage
        );

    connect(
        m_midi,
        &MidiInput::messageReceived,
        m_recordPanel,
        &MidiInputPanel::showMessage
        );

    connect(
        m_midi,
        &MidiInput::messageReceived,
        this,
        &MainWindow::onLiveMessage
        );

    connect(
        m_midi,
        &MidiInput::messageReceived,
        m_serialPanel,
        &SerialPanel::observeMidiMessage
        );

    /* 文件回放 */
    connect(
        m_player,
        &MidiFilePlayer::eventPlayed,
        this,
        &MainWindow::onFileEvent
        );

    connect(
        m_player,
        &MidiFilePlayer::progressChanged,
        this,
        &MainWindow::onPlaybackProgress
        );

    connect(
        m_player,
        &MidiFilePlayer::stateChanged,
        this,
        &MainWindow::onPlaybackStateChanged
        );

    connect(
        m_player,
        &MidiFilePlayer::finished,
        this,
        &MainWindow::onPlaybackFinished
        );

    /* 顶部急停：交给路由器，顺便清掉本地吸合状态 */
    connect(
        m_deviceBar,
        &DeviceBar::allOffRequested,
        this,
        [this]() {
            m_router->releaseAll();
        }
        );

    /* 按钮 */
    connect(
        m_recordButton,
        &QPushButton::clicked,
        this,
        &MainWindow::toggleRecording
        );

    connect(
        m_openFileButton,
        &QPushButton::clicked,
        this,
        &MainWindow::openFile
        );

    connect(
        m_playButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onPlayRequested
        );

    connect(
        m_pauseButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onPauseRequested
        );

    connect(
        m_stopButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onStopRequested
        );

    /* 转发计数与状态栏 */
    connect(
        m_router,
        &MidiKeyRouter::commandSent,
        this,
        &MainWindow::onRouterCommandSent
        );

    connect(
        m_router,
        &MidiKeyRouter::messageIgnored,
        this,
        &MainWindow::onRouterMessageIgnored
        );

    connect(
        m_router,
        &MidiKeyRouter::pressedKeysChanged,
        this,
        &MainWindow::updateBoardStatusLabel
        );

    connect(
        m_serial,
        &SerialCommand::connectedChanged,
        this,
        &MainWindow::updateBoardStatusLabel
        );

    connect(
        m_serialPanel,
        &SerialPanel::statusReceived,
        this,
        &MainWindow::onBoardStatusReceived
        );

    updateForwardLabels();
    updatePlaybackWidgets();
    updateBoardStatusLabel();
}

MainWindow::~MainWindow()
{
}

QWidget *MainWindow::createLivePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    m_livePanel = new MidiInputPanel(page);
    layout->addWidget(m_livePanel, 1);

    auto *forwardRow = new QHBoxLayout();

    m_forwardLiveCheck =
        new QCheckBox(QStringLiteral("收到MIDI后转发到串口"));

    m_forwardLiveCheck->setChecked(true);

    m_forwardLiveLabel = new QLabel;

    forwardRow->addWidget(m_forwardLiveCheck);
    forwardRow->addWidget(m_forwardLiveLabel);
    forwardRow->addStretch();

    layout->addLayout(forwardRow);

    return page;
}

QWidget *MainWindow::createRecordPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *forwardRow = new QHBoxLayout();

    m_forwardFileCheck =
        new QCheckBox(QStringLiteral("回放时转发到串口"));

    m_forwardFileCheck->setChecked(true);

    m_forwardFileLabel = new QLabel;

    forwardRow->addWidget(m_forwardFileCheck);
    forwardRow->addWidget(m_forwardFileLabel);
    forwardRow->addStretch();

    layout->addLayout(forwardRow);

    /* 现场消息面板：页签2只用来录制与显示，不参与转发 */
    m_recordPanel = new MidiInputPanel(page);
    layout->addWidget(m_recordPanel, 1);

    auto *playGroup = new QGroupBox(QStringLiteral("文件回放"));

    auto *playLayout = new QVBoxLayout(playGroup);

    auto *playRow = new QHBoxLayout();

    m_openFileButton =
        new QPushButton(QStringLiteral("打开MIDI文件"));

    m_playButton = new QPushButton(QStringLiteral("播放"));
    m_pauseButton = new QPushButton(QStringLiteral("暂停"));
    m_stopButton = new QPushButton(QStringLiteral("停止"));

    playRow->addWidget(m_openFileButton);
    playRow->addWidget(m_playButton);
    playRow->addWidget(m_pauseButton);
    playRow->addWidget(m_stopButton);
    playRow->addStretch();

    playLayout->addLayout(playRow);

    m_playFileLabel = new QLabel(QStringLiteral("未加载文件"));
    playLayout->addWidget(m_playFileLabel);

    m_playProgress = new QProgressBar;
    m_playProgress->setRange(0, 1);
    m_playProgress->setValue(0);
    m_playProgress->setFormat(QStringLiteral("%v / %m ms"));

    playLayout->addWidget(m_playProgress);

    layout->addWidget(playGroup);

    auto *recordRow = new QHBoxLayout();

    m_recordButton =
        new QPushButton(QStringLiteral("开始录制"));

    recordRow->addStretch();
    recordRow->addWidget(m_recordButton);

    layout->addLayout(recordRow);

    return page;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_player->isPlaying() ||
        m_player->isPaused()) {

        m_player->stop();
    }

    if (!m_router->pressedKeys().isEmpty()) {
        m_router->releaseAll();
    }

    m_serial->close();

    QMainWindow::closeEvent(event);
}

void MainWindow::onLiveMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    if (m_recorder->isRecording()) {
        const quint32 elapsedMs =
            m_recordTimer.isValid()
                ? quint32(m_recordTimer.elapsed())
                : 0;

        m_recorder->appendMessage(
            status,
            data1,
            data2,
            elapsedMs
            );
    }

    if (!m_forwardLiveCheck->isChecked()) {
        return;
    }

    m_forwardSource = SourceLive;

    m_router->handleMessage(
        status,
        data1,
        data2,
        timestampMs
        );

    m_forwardSource = SourceNone;
}

void MainWindow::onFileEvent(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    if (!m_forwardFileCheck->isChecked()) {
        return;
    }

    m_forwardSource = SourceFile;

    m_router->handleMessage(
        status,
        data1,
        data2,
        timestampMs
        );

    m_forwardSource = SourceNone;
}

void MainWindow::onRouterCommandSent(const QString &line)
{
    Q_UNUSED(line);

    if (m_forwardSource == SourceLive) {
        ++m_liveSentCount;
        updateForwardLabels();

        return;
    }

    if (m_forwardSource == SourceFile) {
        ++m_fileSentCount;
        updateForwardLabels();
    }
}

void MainWindow::onRouterMessageIgnored(const QString &reason)
{
    if (m_forwardSource == SourceLive) {
        ++m_liveIgnoredCount;

        m_forwardLiveLabel->setToolTip(reason);
        updateForwardLabels();

        return;
    }

    if (m_forwardSource == SourceFile) {
        ++m_fileIgnoredCount;

        m_forwardFileLabel->setToolTip(reason);
        updateForwardLabels();
    }
}

void MainWindow::onBoardStatusReceived(quint32 bits)
{
    const QString hexText =
        QStringLiteral("0x%1")
            .arg(bits, 8, 16, QLatin1Char('0'))
            .toUpper();

    statusBar()->showMessage(
        QStringLiteral("下位机上报状态：%1").arg(hexText),
        5000
        );
}

void MainWindow::onPlayRequested()
{
    if (m_recorder->isRecording()) {
        m_recordPanel->appendLog(
            QStringLiteral("[PLAY] 正在录制，已忽略播放")
            );

        return;
    }

    m_player->play();
}

void MainWindow::onPauseRequested()
{
    m_player->pause();
}

void MainWindow::onStopRequested()
{
    m_player->stop();

    /* 停止时把没释放的键一次性放开 */
    if (!m_router->pressedKeys().isEmpty()) {
        m_router->releaseAll();
    }

    updatePlaybackWidgets();
}

void MainWindow::onPlaybackFinished()
{
    if (!m_router->pressedKeys().isEmpty()) {
        m_router->releaseAll();
    }

    updatePlaybackWidgets();
}

void MainWindow::onPlaybackProgress(
    quint32 currentMs,
    quint32 totalMs)
{
    m_playProgress->setRange(
        0,
        int(totalMs > 0 ? totalMs : 1)
        );

    m_playProgress->setValue(int(currentMs));
}

void MainWindow::onPlaybackStateChanged()
{
    updatePlaybackWidgets();
}

void MainWindow::updateForwardLabels()
{
    m_forwardLiveLabel->setText(
        QStringLiteral("已发送 %1 条，忽略 %2 条")
            .arg(m_liveSentCount)
            .arg(m_liveIgnoredCount)
        );

    m_forwardFileLabel->setText(
        QStringLiteral("已发送 %1 条，忽略 %2 条")
            .arg(m_fileSentCount)
            .arg(m_fileIgnoredCount)
        );
}

void MainWindow::updatePlaybackWidgets()
{
    const bool hasFile = m_player->eventCount() > 0;
    const bool recording = m_recorder->isRecording();
    const bool playing = m_player->isPlaying();
    const bool paused = m_player->isPaused();

    m_playButton->setEnabled(hasFile && !recording && !playing);
    m_pauseButton->setEnabled(hasFile && !recording && playing);
    m_stopButton->setEnabled(
        hasFile && !recording && (playing || paused)
        );

    m_openFileButton->setEnabled(!recording);
}

void MainWindow::updateBoardStatusLabel()
{
    if (!m_serial->isOpen()) {
        m_boardStatusLabel->setText(
            QStringLiteral("下位机：未连接")
            );

        return;
    }

    QList<int> keys = m_router->pressedKeys().values();

    std::sort(keys.begin(), keys.end());

    QStringList parts;

    for (int index = 0;
         index < keys.size() && index < 8;
         ++index) {

        parts.append(QString::number(keys.at(index)));
    }

    QString keyText =
        parts.isEmpty()
            ? QStringLiteral("无吸合键")
            : parts.join(QLatin1Char(','));

    if (keys.size() > 8) {
        keyText += QStringLiteral("…");
    }

    m_boardStatusLabel->setText(
        QStringLiteral("下位机：%1 @%2 · %3")
            .arg(m_serial->portName())
            .arg(m_serial->baudRate())
            .arg(keyText)
        );
}

void MainWindow::toggleRecording()
{
    if (!m_recorder->isRecording()) {
        m_recordTimer.start();
        m_recorder->start();

        m_recordButton->setText(
            QStringLiteral("停止录制")
            );

        m_recordPanel->appendLog(
            QStringLiteral("[REC] 开始录制")
            );

        updatePlaybackWidgets();

        return;
    }

    m_recorder->stop();

    m_recordButton->setText(
        QStringLiteral("开始录制")
        );

    const int recordedCount =
        m_recorder->eventCount();

    m_recordPanel->appendLog(
        QStringLiteral(
            "[REC] 停止录制，共%1个音符事件"
            ).arg(recordedCount)
        );

    updatePlaybackWidgets();

    if (recordedCount == 0) {
        m_recordPanel->appendLog(
            QStringLiteral("[REC] 没有音符事件，未保存")
            );

        return;
    }

    QString filePath =
        QFileDialog::getSaveFileName(
            this,
            QStringLiteral("保存MIDI文件"),
            QStringLiteral("recorded.mid"),
            QStringLiteral("MIDI文件 (*.mid)")
            );

    if (filePath.isEmpty()) {
        m_recordPanel->appendLog(
            QStringLiteral("[REC] 已取消保存")
            );

        return;
    }

    if (!filePath.endsWith(
            QStringLiteral(".mid"),
            Qt::CaseInsensitive)) {

        filePath += QStringLiteral(".mid");
    }

    QString errorMessage;

    if (m_recorder->save(
            filePath,
            &errorMessage)) {

        m_recordPanel->appendLog(
            QStringLiteral(
                "[REC] 已保存：%1"
                ).arg(filePath)
            );
    } else {
        m_recordPanel->appendLog(
            QStringLiteral(
                "[REC] 保存失败：%1"
                ).arg(errorMessage)
            );
    }
}

void MainWindow::openFile()
{
    if (m_recorder->isRecording()) {
        m_recordPanel->appendLog(
            QStringLiteral(
                "[FILE] 正在录制，已忽略打开文件"
                )
            );

        return;
    }

    const QString filePath =
        QFileDialog::getOpenFileName(
            this,
            QStringLiteral("打开MIDI文件"),
            QString(),
            QStringLiteral(
                "MIDI文件 (*.mid *.midi);;所有文件 (*)"
                )
            );

    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;

    const QStringList lines =
        MidiFileDecoder::decodeToLines(
            filePath,
            &errorMessage
            );

    m_recordPanel->clearLog();

    if (!errorMessage.isEmpty()) {
        m_recordPanel->appendLog(
            QStringLiteral(
                "[FILE] 解码失败：%1"
                ).arg(errorMessage)
            );

        return;
    }

    m_recordPanel->appendLog(
        QStringLiteral(
            "[FILE] %1  共%2个音符事件"
            )
            .arg(QFileInfo(filePath).fileName())
            .arg(lines.size())
        );

    for (const QString &line : lines) {
        m_recordPanel->appendLog(line);
    }

    QString loadError;

    if (!m_player->load(filePath, &loadError)) {
        m_recordPanel->appendLog(
            QStringLiteral(
                "[PLAY] 回放加载失败：%1"
                ).arg(loadError)
            );

        m_playFileLabel->setText(
            QStringLiteral("未加载文件")
            );

        updatePlaybackWidgets();

        return;
    }

    m_playFileLabel->setText(
        QStringLiteral("%1（%2个音符事件，时长%3）")
            .arg(QFileInfo(filePath).fileName())
            .arg(m_player->eventCount())
            .arg(formatDuration(m_player->totalMs()))
        );

    updatePlaybackWidgets();
}

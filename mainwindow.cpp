#include "mainwindow.h"
#include "midiinputpanel.h"
#include "midirecorder.h"
#include "midifiledecoder.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(
        QStringLiteral("SMK25Mini MIDI工具")
        );

    resize(900, 600);

    m_tabs = new QTabWidget(this);

    m_inputPanel = new MidiInputPanel;
    m_tabs->addTab(
        m_inputPanel,
        QStringLiteral("MIDI输入")
        );

    auto *recordPage = new QWidget;
    auto *recordLayout = new QVBoxLayout(recordPage);

    m_recordPanel = new MidiInputPanel;
    recordLayout->addWidget(m_recordPanel, 1);

    auto *buttonLayout = new QHBoxLayout();

    m_recordButton =
        new QPushButton(QStringLiteral("开始录制"));

    m_openFileButton =
        new QPushButton(QStringLiteral("打开文件"));

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_recordButton);
    buttonLayout->addWidget(m_openFileButton);

    recordLayout->addLayout(buttonLayout);

    m_tabs->addTab(
        recordPage,
        QStringLiteral("录制")
        );

    setCentralWidget(m_tabs);

    m_recorder = new MidiRecorder(this);

    connect(
        m_recordPanel,
        &MidiInputPanel::received,
        this,
        &MainWindow::onRecordMessage
        );

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
}

MainWindow::~MainWindow()
{
}

void MainWindow::onRecordMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32)
{
    if (!m_recorder->isRecording()) {
        return;
    }

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

void MainWindow::toggleRecording()
{
    if (!m_recorder->isRecording()) {
        m_recordTimer.start();
        m_recorder->start();

        m_recordButton->setText(
            QStringLiteral("停止录制")
            );

        m_openFileButton->setEnabled(false);

        m_recordPanel->appendLog(
            QStringLiteral("[REC] 开始录制")
            );

        return;
    }

    m_recorder->stop();

    m_recordButton->setText(
        QStringLiteral("开始录制")
        );

    m_openFileButton->setEnabled(true);

    const int recordedCount =
        m_recorder->eventCount();

    m_recordPanel->appendLog(
        QStringLiteral(
            "[REC] 停止录制，共%1个音符事件"
            ).arg(recordedCount)
        );

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

    if (m_recorder->isRecording()) {
        m_recordPanel->appendLog(
            QStringLiteral(
                "[FILE] 正在录制，已忽略打开文件"
                )
            );

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
            "[FILE] %1  共%2个事件"
            )
            .arg(QFileInfo(filePath).fileName())
            .arg(lines.size())
        );

    for (const QString &line : lines) {
        m_recordPanel->appendLog(line);
    }
}

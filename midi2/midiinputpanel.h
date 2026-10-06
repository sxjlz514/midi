#pragma once

#include <QWidget>
#include <QtGlobal>

class QLabel;
class QPlainTextEdit;

/*
 * MIDI消息显示面板：只负责计数与日志。
 *
 * 设备选择/打开已经上移到窗口顶部的DeviceBar，
 * 消息由MainWindow从唯一的MidiInput实例分发进来。
 */
class MidiInputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MidiInputPanel(QWidget *parent = nullptr);
    ~MidiInputPanel() override;

    void appendLog(const QString &line);

    void clearLog();

public slots:
    void showMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private:
    QLabel *m_countLabel = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    quint64 m_messageCount = 0;
};

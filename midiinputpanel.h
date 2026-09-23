#pragma once

#include <QWidget>
#include <QtGlobal>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class MidiInput;

class MidiInputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MidiInputPanel(QWidget *parent = nullptr);
    ~MidiInputPanel() override;

    QPlainTextEdit *logEdit() const;

    void appendLog(const QString &line);

    void clearLog();

    MidiInput *input() const;

signals:
    void received(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private slots:
    void refreshDevices();
    void toggleMidiDevice();

    void onMidiMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private:
    MidiInput *m_midiInput = nullptr;

    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    quint64 m_messageCount = 0;
};

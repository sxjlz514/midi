#pragma once

#include <QList>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

#include "midikeyrouter.h"

class QComboBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;

class SerialCommand;

/*
 * 页签3「下位机」：手动指令 + 下位机状态查询 + MIDI→键号映射设置。
 *
 * 串口连接本身在窗口顶部的DeviceBar里操作，本页只负责发指令和看回包。
 */
class SerialPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SerialPanel(
        SerialCommand *serial,
        MidiKeyRouter *router,
        QWidget *parent = nullptr
        );

    MidiKeyMapping mapping() const;

    /* 尽力解析状态回包（0x十六进制 / 32位0-1串 / 十进制）；识别不了返回 false */
    static bool parseStatusLine(
        const QString &line,
        quint32 *bits
        );

    void appendLog(const QString &line);

signals:
    void mappingChanged(const MidiKeyMapping &mapping);

    void statusReceived(quint32 bits);

    void connectedChanged(bool connected);

public slots:
    /* 只用来记录"最近收到的音"，方便一键设为基准音 */
    void observeMidiMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private slots:
    void sendOn();
    void sendOff();
    void sendSet();
    void sendAllOff();

    void sendDiscover();
    void sendSync();
    void sendStart();
    void sendStop();
    void sendPause();
    void sendHelp();
    void sendStatus();

    void onMappingWidgetChanged();
    void useLastNoteAsBase();

    void onSerialLine(const QString &line);
    void onNodeReported(
        const QString &node,
        const QString &text
        );
    void onSerialError(const QString &message);
    void onConnectedChanged(bool connected);

private:
    QGroupBox *createManualGroup();
    QGroupBox *createStatusGroup();
    QGroupBox *createMappingGroup();
    QGroupBox *createLogGroup();

    QList<int> checkedKeys() const;
    void clearKeySelection();

    void appendTx(const QString &line, bool ok);
    void maybeAutoQuery();
    void updateMappingHint();
    void updateStateLights(quint32 bits);

    SerialCommand *m_serial = nullptr;
    MidiKeyRouter *m_router = nullptr;

    QVector<QPushButton *> m_keyButtons;
    QVector<QPushButton *> m_commandButtons;
    QVector<QLabel *> m_stateLights;

    QLabel *m_stateSummaryLabel = nullptr;
    QComboBox *m_autoQueryCombo = nullptr;

    QSpinBox *m_baseNoteSpin = nullptr;
    QLabel *m_baseNoteHint = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QLabel *m_lastNoteLabel = nullptr;

    QPlainTextEdit *m_logEdit = nullptr;
    QTimer *m_autoQueryTimer = nullptr;

    int m_lastNote = -1;
};

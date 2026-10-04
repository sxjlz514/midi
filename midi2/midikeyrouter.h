#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QtGlobal>

class SerialCommand;

/* MIDI音符 → 下位机键号 的映射参数 */
struct MidiKeyMapping
{
    int baseNote = 0;   /* 该音符号对应键1；键号 = 音符号 - baseNote + 1 */
    int channel = -1;   /* -1 = 全部通道，1..16 = 只接受该通道 */
};

/*
 * 把 MIDI 消息翻译成下位机的 ON / OFF 指令。
 *
 * 同时跟踪"哪些键已经吸合"，用于丢弃重复 ON / 重复 OFF，
 * 并在停止回放、关闭窗口时用 ALL_OFF 一次释放。
 */
class MidiKeyRouter : public QObject
{
    Q_OBJECT

public:
    explicit MidiKeyRouter(QObject *parent = nullptr);

    void setSerial(SerialCommand *serial);

    void setMapping(const MidiKeyMapping &mapping);
    MidiKeyMapping mapping() const;

    QSet<int> pressedKeys() const;

    int sentCount() const;
    int ignoredCount() const;

    /* 纯函数：翻译一条 MIDI 消息；返回 false 表示不处理，reason 说明原因 */
    static bool translate(
        quint8 status,
        quint8 data1,
        quint8 data2,
        const MidiKeyMapping &mapping,
        int *key,
        bool *pressed,
        QString *reason
        );

public slots:
    /* MIDI 消息入口（来自现场设备或文件回放） */
    void handleMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

    /* 手动指令入口（页签3的手动按钮走这里，保证按键状态一致） */
    bool pressKeys(const QList<int> &keys);
    bool releaseKeys(const QList<int> &keys);
    bool setOnlyKeys(const QList<int> &keys);

    void releaseAll();

signals:
    void commandSent(const QString &line);
    void messageIgnored(const QString &reason);
    void pressedKeysChanged();

private:
    bool sendKeys(
        const QList<int> &keys,
        bool pressed,
        const QString &commandName
        );

    SerialCommand *m_serial = nullptr;

    MidiKeyMapping m_mapping;

    QSet<int> m_pressedKeys;

    int m_sentCount = 0;
    int m_ignoredCount = 0;
};

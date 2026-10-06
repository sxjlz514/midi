#include "midikeyrouter.h"
#include "serialcommand.h"

#include <QStringList>

namespace {

QString keyText(const QList<int> &keys)
{
    QStringList parts;
    parts.reserve(keys.size());

    for (int key : keys) {
        parts.append(QString::number(key));
    }

    return parts.join(QLatin1Char(' '));
}

}   /* namespace */

MidiKeyRouter::MidiKeyRouter(QObject *parent)
    : QObject(parent)
{
}

void MidiKeyRouter::setSerial(SerialCommand *serial)
{
    m_serial = serial;
}

void MidiKeyRouter::setMapping(const MidiKeyMapping &mapping)
{
    m_mapping = mapping;
}

MidiKeyMapping MidiKeyRouter::mapping() const
{
    return m_mapping;
}

QSet<int> MidiKeyRouter::pressedKeys() const
{
    return m_pressedKeys;
}

int MidiKeyRouter::sentCount() const
{
    return m_sentCount;
}

int MidiKeyRouter::ignoredCount() const
{
    return m_ignoredCount;
}

bool MidiKeyRouter::translate(
    quint8 status,
    quint8 data1,
    quint8 data2,
    const MidiKeyMapping &mapping,
    int *key,
    bool *pressed,
    QString *reason)
{
    const quint8 command = status & 0xF0;

    if (command != 0x90 &&
        command != 0x80) {

        if (reason) {
            *reason = QStringLiteral("非音符消息");
        }

        return false;
    }

    const int channel = (status & 0x0F) + 1;

    if (mapping.channel >= 1 &&
        mapping.channel <= 16 &&
        channel != mapping.channel) {

        if (reason) {
            *reason = QStringLiteral("通道%1被过滤").arg(channel);
        }

        return false;
    }

    /* Note On 且力度为 0 时按惯例视为 OFF */
    const bool isPressed =
        (command == 0x90) && (data2 > 0);

    const int keyNumber =
        int(data1) - mapping.baseNote + 1;

    if (keyNumber < SerialCommand::KeyMin ||
        keyNumber > SerialCommand::KeyMax) {

        if (reason) {
            *reason =
                QStringLiteral(
                    "音符%1对应键号%2，超出%3..%4"
                    )
                    .arg(data1)
                    .arg(keyNumber)
                    .arg(SerialCommand::KeyMin)
                    .arg(SerialCommand::KeyMax);
        }

        return false;
    }

    if (key) {
        *key = keyNumber;
    }

    if (pressed) {
        *pressed = isPressed;
    }

    return true;
}

void MidiKeyRouter::handleMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32)
{
    int key = 0;
    bool pressed = false;
    QString reason;

    if (!translate(
            status,
            data1,
            data2,
            m_mapping,
            &key,
            &pressed,
            &reason)) {

        ++m_ignoredCount;

        emit messageIgnored(reason);

        return;
    }

    if (m_pressedKeys.contains(key) == pressed) {
        ++m_ignoredCount;

        emit messageIgnored(
            pressed
                ? QStringLiteral("键%1已吸合，忽略重复ON").arg(key)
                : QStringLiteral("键%1未吸合，忽略重复OFF").arg(key)
            );

        return;
    }

    if (!m_serial || !m_serial->isOpen()) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("串口未打开，键%1未下发").arg(key)
            );

        return;
    }

    const bool ok =
        pressed ? m_serial->on(key) : m_serial->off(key);

    if (!ok) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("键%1下发失败").arg(key)
            );

        return;
    }

    if (pressed) {
        m_pressedKeys.insert(key);
    } else {
        m_pressedKeys.remove(key);
    }

    ++m_sentCount;

    emit commandSent(
        QStringLiteral("%1 %2")
            .arg(pressed
                     ? QStringLiteral("ON")
                     : QStringLiteral("OFF"))
            .arg(key)
        );

    emit pressedKeysChanged();
}

bool MidiKeyRouter::sendKeys(
    const QList<int> &keys,
    bool pressed,
    const QString &commandName)
{
    bool keysOk = true;

    const QList<int> checkedKeys =
        SerialCommand::normalizeKeys(keys, &keysOk);

    if (!keysOk || checkedKeys.isEmpty()) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("%1指令没有合法的键号").arg(commandName)
            );

        return false;
    }

    /* 只下发状态真正改变的键，避免重复指令 */
    QList<int> targets;

    for (int key : checkedKeys) {
        if (m_pressedKeys.contains(key) == pressed) {
            continue;
        }

        targets.append(key);
    }

    if (targets.isEmpty()) {
        return false;
    }

    if (!m_serial || !m_serial->isOpen()) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("串口未打开，%1未下发").arg(commandName)
            );

        return false;
    }

    const bool ok =
        pressed ? m_serial->on(targets) : m_serial->off(targets);

    if (!ok) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("%1下发失败").arg(commandName)
            );

        return false;
    }

    for (int key : targets) {
        if (pressed) {
            m_pressedKeys.insert(key);
        } else {
            m_pressedKeys.remove(key);
        }
    }

    ++m_sentCount;

    emit commandSent(
        QStringLiteral("%1 %2").arg(commandName, keyText(targets))
        );

    emit pressedKeysChanged();

    return true;
}

bool MidiKeyRouter::pressKeys(const QList<int> &keys)
{
    return sendKeys(keys, true, QStringLiteral("ON"));
}

bool MidiKeyRouter::releaseKeys(const QList<int> &keys)
{
    return sendKeys(keys, false, QStringLiteral("OFF"));
}

bool MidiKeyRouter::setOnlyKeys(const QList<int> &keys)
{
    bool keysOk = true;

    const QList<int> checkedKeys =
        SerialCommand::normalizeKeys(keys, &keysOk);

    if (!keysOk || checkedKeys.isEmpty()) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("SET指令没有合法的键号")
            );

        return false;
    }

    if (!m_serial || !m_serial->isOpen()) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("串口未打开，SET未下发")
            );

        return false;
    }

    if (!m_serial->setOnly(checkedKeys)) {
        ++m_ignoredCount;

        emit messageIgnored(
            QStringLiteral("SET下发失败")
            );

        return false;
    }

    m_pressedKeys.clear();

    for (int key : checkedKeys) {
        m_pressedKeys.insert(key);
    }

    ++m_sentCount;

    emit commandSent(
        QStringLiteral("SET %1").arg(keyText(checkedKeys))
        );

    emit pressedKeysChanged();

    return true;
}

void MidiKeyRouter::releaseAll()
{
    m_pressedKeys.clear();

    if (m_serial && m_serial->isOpen()) {
        if (m_serial->allOff()) {
            emit commandSent(QStringLiteral("ALL_OFF"));
        }
    }

    emit pressedKeysChanged();
}

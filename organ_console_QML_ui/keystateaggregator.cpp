/**
 * @file keystateaggregator.cpp
 * @brief 短窗聚合实现：合并按键 → 串口 ST 十六进制位图帧。
 *
 * UART 帧（与主板约定）：
 *   @ST,<seq4>,<t_ms8>,<bitmap8>,<crc4>\n
 * CRC16-CCITT(false)：覆盖 "ST,<seq>,<t>,<bitmap>" 这段 ASCII（不含 @ 与 CRC）。
 */

#include "keystateaggregator.h"
#include "serialclient.h"

#include <QElapsedTimer>
#include <QtGlobal>

namespace {

/** CRC-16/CCITT-FALSE：poly 0x1021，init 0xFFFF。 */
quint16 crc16Ccitt(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (unsigned char b : data) {
        crc ^= static_cast<quint16>(b) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = static_cast<quint16>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<quint16>(crc << 1);
            }
        }
    }
    return crc;
}

QElapsedTimer &monoClock()
{
    static QElapsedTimer t;
    if (!t.isValid()) {
        t.start();
    }
    return t;
}

} // namespace

KeyStateAggregator::KeyStateAggregator(SerialClient *serial, QObject *parent)
    : QObject(parent)
    , m_serial(serial)
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &KeyStateAggregator::onWindowTimeout);

    m_ackTimer.setSingleShot(true);
    m_ackTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_ackTimer, &QTimer::timeout, this, &KeyStateAggregator::onAckTimeout);
}

void KeyStateAggregator::setChannelLimit(int limit)
{
    m_channelLimit = qBound(1, limit, kBitmapBits);
    const quint32 mask = m_channelLimit >= 32
        ? 0xFFFFFFFFu
        : ((1u << m_channelLimit) - 1u);
    m_shadow &= mask;
}

void KeyStateAggregator::setWindowMs(int ms)
{
    if (ms < 1) {
        ms = 1;
    }
    if (ms > 20) {
        ms = 20;
    }
    m_windowMs = ms;
}

void KeyStateAggregator::setAckTimeoutMs(int ms)
{
    m_ackTimeoutMs = qBound(20, ms, 2000);
    if (m_ackTimer.isActive()) {
        m_ackTimer.start(m_ackTimeoutMs);
    }
}

void KeyStateAggregator::setChannel(int channel, bool on)
{
    if (channel < 1 || channel > m_channelLimit) {
        return;
    }
    const quint32 bit = 1u << (channel - 1);
    if (on) {
        m_shadow |= bit;
    } else {
        m_shadow &= ~bit;
    }
    m_dirty = true;
    /* 每次事件刷新窗口，使窗内多次按键合并为一次发送 */
    m_timer.start(m_windowMs);
}

void KeyStateAggregator::allOffFlush()
{
    m_timer.stop();
    m_shadow = 0;
    m_dirty = true;
    flushNow();
}

void KeyStateAggregator::resetLocal()
{
    m_timer.stop();
    m_ackTimer.stop();
    m_shadow = 0;
    m_dirty = false;
    m_inflight = false;
    m_retryCount = 0;
}

void KeyStateAggregator::flushNow()
{
    m_timer.stop();
    if (!m_dirty && m_shadow == m_lastSent) {
        return;
    }
    onWindowTimeout();
}

void KeyStateAggregator::setBitmapAndFlush(quint32 bitmap)
{
    m_timer.stop();
    m_shadow = bitmap;
    m_dirty = true;
    flushNow();
}

void KeyStateAggregator::onWindowTimeout()
{
    if (!m_dirty && m_shadow == m_lastSent) {
        return;
    }
    const quint32 snap = m_shadow;
    m_retryCount = 0; // 新状态帧重新计算重试次数
    if (sendBitmapFrame(snap)) {
        m_lastSent = snap;
        m_dirty = false;
        emit snapshotSent(snap, m_seq);
    }
}

bool KeyStateAggregator::sendBitmapFrame(quint32 bitmap)
{
    if (!m_serial || !m_serial->isOpen()) {
        m_ackTimer.stop();
        m_inflight = false;
        return false;
    }
    ++m_seq;
    const quint32 tMs = static_cast<quint32>(monoClock().elapsed() & 0xFFFFFFFFu);
    const QString body = QStringLiteral("ST,%1,%2,%3")
                             .arg(m_seq, 4, 16, QLatin1Char('0'))
                             .arg(tMs, 8, 16, QLatin1Char('0'))
                             .arg(bitmap, 8, 16, QLatin1Char('0'))
                             .toUpper();
    const quint16 crc = crc16Ccitt(body.toLatin1());
    const QString line = QStringLiteral("@%1,%2")
                             .arg(body)
                             .arg(crc, 4, 16, QLatin1Char('0'))
                             .toUpper();
    if (!m_serial->sendLine(line)) {
        m_ackTimer.stop();
        m_inflight = false;
        return false;
    }
    // 只跟踪最新一帧：位图是绝对状态，旧帧重发会把已松开的键重新点亮。
    m_inflight = true;
    m_inflightSeq = m_seq;
    m_inflightBitmap = bitmap;
    m_ackTimer.start(m_ackTimeoutMs);
    return true;
}

void KeyStateAggregator::onAckReceived(quint16 seq, bool ok, int code)
{
    if (!m_inflight || seq != m_inflightSeq) {
        return; // 已被更新的状态帧取代，或不是当前事务
    }
    m_ackTimer.stop();

    if (ok) {
        m_inflight = false;
        m_retryCount = 0;
        emit frameAcked(seq);
        return;
    }

    emit frameRejected(seq, code);
    if (m_retryCount >= kMaxRetries) {
        failTransmission();
        return;
    }
    ++m_retryCount;
    emit retryScheduled(seq, m_retryCount);
    const quint32 bitmap = m_inflightBitmap;
    if (!sendBitmapFrame(bitmap)) {
        failTransmission();
        return;
    }
    emit snapshotSent(bitmap, m_seq);
}

void KeyStateAggregator::onAckTimeout()
{
    if (!m_inflight) {
        return;
    }
    if (m_retryCount >= kMaxRetries) {
        failTransmission();
        return;
    }
    ++m_retryCount;
    const quint16 seq = m_inflightSeq;
    const quint32 bitmap = m_inflightBitmap;
    emit retryScheduled(seq, m_retryCount);
    if (!sendBitmapFrame(bitmap)) {
        failTransmission();
        return;
    }
    emit snapshotSent(bitmap, m_seq);
}

void KeyStateAggregator::failTransmission()
{
    m_ackTimer.stop();
    const quint16 seq = m_inflightSeq;
    const quint32 bitmap = m_inflightBitmap;
    m_inflight = false;
    m_retryCount = 0;
    // 保留 dirty，下一次按键或 flushNow 会重新发送完整位图。
    m_dirty = true;
    emit transmissionFailed(seq, bitmap);
}

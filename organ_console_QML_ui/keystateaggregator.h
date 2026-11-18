/**
 * @file keystateaggregator.h
 * @brief 短窗聚合：高频通道开关合并为一次完整位图快照再发串口。
 *
 * 按设计稿：窗口内只改 shadowState，窗口到期发送最新 32 位图。
 * 实际参与的键数由 setChannelLimit 限制，最大 32。
 * 默认聚合窗 4 ms（约 2–5 ms 可配）。
 *
 * ACK：每帧带 16 位序号；主板回 @OK/@ERR。上位机只跟踪“最新一帧”，
 * 超时或拒绝时重发最新位图（绝对状态，重发旧帧会把已松开的键又点亮）。
 */

#ifndef KEYSTATEAGGREGATOR_H
#define KEYSTATEAGGREGATOR_H

#include <QObject>
#include <QTimer>
#include <cstdint>

class SerialClient;

class KeyStateAggregator : public QObject
{
    Q_OBJECT

public:
    /** 与固件一致的位图宽度。 */
    static constexpr int kBitmapBits = 32;
    /** 默认短窗毫秒数。 */
    static constexpr int kDefaultWindowMs = 4;
    /** 等主板 @OK/@ERR 的默认超时。 */
    static constexpr int kDefaultAckTimeoutMs = 120;
    /** 同一帧最多重发次数（不含首发）。 */
    static constexpr int kMaxRetries = 3;

    explicit KeyStateAggregator(SerialClient *serial, QObject *parent = nullptr);

    /** 配置聚合窗长度（毫秒），建议 2–5。 */
    void setWindowMs(int ms);

    int windowMs() const { return m_windowMs; }

    /** 配置等待 ACK 的超时（毫秒），建议 80–300。 */
    void setAckTimeoutMs(int ms);

    int ackTimeoutMs() const { return m_ackTimeoutMs; }

    /** 当前是否有一帧正在等主板确认。 */
    bool awaitingAck() const { return m_inflight; }

    /** 只接受 1..limit 的通道，limit 最大 32。 */
    void setChannelLimit(int limit);

    /** 当前 shadow 位图（bit0=键1）。 */
    quint32 shadowState() const { return m_shadow; }

    /**
     * @brief 更新某一通道并启动/刷新短窗（不立刻发串口）。
     * @param channel 1..channelLimit
     * @param on true 置位，false 清位
     */
    void setChannel(int channel, bool on);

    /** 立即清零并刷出 ST 00000000（急停/切模式）。 */
    void allOffFlush();

    /** 丢弃窗口内未发变更，并把 shadow 置 0（不发串口）。 */
    void resetLocal();

    /** 强制立刻发送当前 shadow（取消定时器）。 */
    void flushNow();

    /** 发送预聚合的完整位图快照，不再经过短窗。 */
    void setBitmapAndFlush(quint32 bitmap);

signals:
    /** 已向串口发出一条 ST 帧时的位图（调试日志用）。 */
    void snapshotSent(quint32 bitmap, quint16 seq);
    /** 主板回 @OK，当前在途帧已确认。 */
    void frameAcked(quint16 seq);
    /** 主板回 @ERR，code 为下位机错误码。 */
    void frameRejected(quint16 seq, int code);
    /** 超时或 @ERR 后准备重发。 */
    void retryScheduled(quint16 seq, int attempt);
    /** 达到最大重试次数仍失败。 */
    void transmissionFailed(quint16 seq, quint32 bitmap);

public slots:
    /** 由 SerialClient 解析出的 @OK/@ERR 驱动；非当前在途 seq 会被忽略。 */
    void onAckReceived(quint16 seq, bool ok, int code);

private slots:
    /** 短窗到期：发送最新完整快照。 */
    void onWindowTimeout();
    /** ACK 超时：重发最新快照或宣告失败。 */
    void onAckTimeout();

private:
    bool sendBitmapFrame(quint32 bitmap);
    void failTransmission();

    SerialClient *m_serial = nullptr; /**< 不拥有 */
    QTimer m_timer;                   /**< 单次短窗定时器 */
    QTimer m_ackTimer;                /**< 单次 ACK 超时定时器 */
    quint32 m_shadow = 0;             /**< 目标位图 */
    quint32 m_lastSent = 0;           /**< 上次已发位图（去重） */
    quint16 m_seq = 0;                /**< 事务序号，与 ACK 配对 */
    quint16 m_inflightSeq = 0;        /**< 当前等 ACK 的序号 */
    quint32 m_inflightBitmap = 0;     /**< 当前等 ACK 的位图 */
    int m_windowMs = kDefaultWindowMs;
    int m_ackTimeoutMs = kDefaultAckTimeoutMs;
    int m_channelLimit = 16;
    int m_retryCount = 0;             /**< 当前在途帧已重发次数 */
    bool m_dirty = false;             /**< 窗内是否有未发送变更 */
    bool m_inflight = false;          /**< 是否有帧在等 ACK */
};

#endif // KEYSTATEAGGREGATOR_H

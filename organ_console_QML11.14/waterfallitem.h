/**
 * @file waterfallitem.h
 * @brief 自动演奏瀑布流（QQuickPaintedItem）。
 *
 * 绘制思路参考 pianopi PianoRoll/Roll：
 * 以固定像素/秒把音符条映射到纵向时间轴，定时刷新。
 * 本工程按风琴通道分车道，而不是 88 键钢琴键位。
 */

#ifndef WATERFALLITEM_H
#define WATERFALLITEM_H

#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class WaterfallItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notes READ notes WRITE setNotes NOTIFY notesChanged)
    Q_PROPERTY(int channelCount READ channelCount WRITE setChannelCount NOTIFY channelCountChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs WRITE setPositionMs NOTIFY positionMsChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)

public:
    explicit WaterfallItem(QQuickItem *parent = nullptr);

    QVariantList notes() const { return m_notesRaw; }
    void setNotes(const QVariantList &notes);

    int channelCount() const { return m_channelCount; }
    void setChannelCount(int count);

    qint64 positionMs() const { return m_positionMs; }
    void setPositionMs(qint64 ms);

    bool playing() const { return m_playing; }
    void setPlaying(bool playing);

    void paint(QPainter *painter) override;

signals:
    void notesChanged();
    void channelCountChanged();
    void positionMsChanged();
    void playingChanged();

private:
    struct NoteBar {
        int channel = 0;
        qint64 startMs = 0;
        qint64 endMs = 0;
    };

    void rebuildBars();
    void syncTimer();
    QColor laneColor(int channel) const;

    /** 与 pianopi MSPIX≈200 同量级：每秒下落像素。 */
    static constexpr qreal kPxPerSec = 180.0;

    QVariantList m_notesRaw;
    QVector<NoteBar> m_bars;
    int m_channelCount = 16;
    qint64 m_positionMs = 0;
    bool m_playing = false;
    QTimer m_repaintTimer;
};

#endif // WATERFALLITEM_H

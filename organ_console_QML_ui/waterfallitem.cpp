/**
 * @file waterfallitem.cpp
 * @brief WaterfallItem：通道车道瀑布流绘制。
 */

#include "waterfallitem.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

WaterfallItem::WaterfallItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(false);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    m_repaintTimer.setInterval(16);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() { update(); });
}

void WaterfallItem::setNotes(const QVariantList &notes)
{
    if (m_notesRaw == notes) {
        return;
    }
    m_notesRaw = notes;
    rebuildBars();
    emit notesChanged();
    update();
}

void WaterfallItem::setChannelCount(int count)
{
    count = qBound(1, count, 32);
    if (m_channelCount == count) {
        return;
    }
    m_channelCount = count;
    emit channelCountChanged();
    update();
}

void WaterfallItem::setPositionMs(qint64 ms)
{
    if (m_positionMs == ms) {
        return;
    }
    m_positionMs = ms;
    emit positionMsChanged();
    update();
}

void WaterfallItem::setPlaying(bool playing)
{
    if (m_playing == playing) {
        return;
    }
    m_playing = playing;
    emit playingChanged();
    syncTimer();
    update();
}

void WaterfallItem::syncTimer()
{
    if (m_playing) {
        m_repaintTimer.start();
    } else {
        m_repaintTimer.stop();
    }
}

void WaterfallItem::rebuildBars()
{
    m_bars.clear();
    m_bars.reserve(m_notesRaw.size());
    for (const QVariant &item : m_notesRaw) {
        const QVariantMap map = item.toMap();
        NoteBar bar;
        bar.channel = map.value(QStringLiteral("channel")).toInt();
        bar.startMs = map.value(QStringLiteral("startMs")).toLongLong();
        bar.endMs = map.value(QStringLiteral("endMs")).toLongLong();
        if (bar.channel < 1 || bar.channel > 32) {
            continue;
        }
        if (bar.endMs < bar.startMs + 18) {
            bar.endMs = bar.startMs + 18;
        }
        m_bars.push_back(bar);
    }
}

QColor WaterfallItem::laneColor(int channel) const
{
    static const QColor kColors[] = {
        QColor(0x4F, 0x9F, 0xD6), QColor(0x5B, 0xB0, 0xC9), QColor(0x63, 0xB7, 0x9A), QColor(0x7E, 0xBE, 0x7A),
        QColor(0xB7, 0xC4, 0x6A), QColor(0xD4, 0xB4, 0x5A), QColor(0xD4, 0x9A, 0x5A), QColor(0xD4, 0x87, 0x70),
        QColor(0xC9, 0x7B, 0x9A), QColor(0xB0, 0x7E, 0xBF), QColor(0x8F, 0x88, 0xC9), QColor(0x6F, 0x95, 0xD0),
        QColor(0x4F, 0x9F, 0xD6), QColor(0x5B, 0xB0, 0xC9), QColor(0x63, 0xB7, 0x9A), QColor(0x7E, 0xBE, 0x7A),
        QColor(0xB7, 0xC4, 0x6A), QColor(0xD4, 0xB4, 0x5A), QColor(0xD4, 0x9A, 0x5A), QColor(0xD4, 0x87, 0x70),
        QColor(0xC9, 0x7B, 0x9A), QColor(0xB0, 0x7E, 0xBF), QColor(0x8F, 0x88, 0xC9), QColor(0x6F, 0x95, 0xD0),
        QColor(0x4F, 0x9F, 0xD6), QColor(0x5B, 0xB0, 0xC9), QColor(0x63, 0xB7, 0x9A), QColor(0x7E, 0xBE, 0x7A),
        QColor(0xB7, 0xC4, 0x6A), QColor(0xD4, 0xB4, 0x5A), QColor(0xD4, 0x9A, 0x5A), QColor(0xD4, 0x87, 0x70)
    };
    const int idx = qBound(0, channel - 1, 31);
    return kColors[idx];
}

void WaterfallItem::paint(QPainter *painter)
{
    const qreal w = width();
    const qreal h = height();
    if (w < 2 || h < 2) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, 0, h);
    bg.setColorAt(0.0, QColor(0xF4, 0xFA, 0xFD));
    bg.setColorAt(0.55, QColor(0xEA, 0xF4, 0xFA));
    bg.setColorAt(1.0, QColor(0xDE, 0xEB, 0xF4));
    painter->fillRect(QRectF(0, 0, w, h), bg);

    const qreal pad = 14.0;
    const qreal keyH = qBound(36.0, h * 0.14, 56.0);
    const qreal hitY = h - keyH - 10.0;
    const qreal fallSpan = qMax(48.0, hitY - pad);
    const int lanes = qMax(1, m_channelCount);
    const qreal laneGap = 2.0;
    const qreal usable = w - pad * 2.0;
    const qreal laneW = qMax(4.0, (usable - laneGap * (lanes - 1)) / lanes);
    const qreal ppm = kPxPerSec / 1000.0; // pixels per ms
    const qreal lookMs = fallSpan / ppm;
    const qint64 now = m_positionMs;

    auto laneX = [&](int channel) -> qreal {
        return pad + (channel - 1) * (laneW + laneGap);
    };

    for (int i = 0; i < lanes; ++i) {
        const qreal x = laneX(i + 1);
        painter->fillRect(QRectF(x, pad, laneW, hitY - pad),
                          (i % 2 == 0) ? QColor(255, 255, 255, 90)
                                       : QColor(36, 75, 112, 10));
    }

    painter->setPen(QPen(QColor(36, 75, 112, 28), 1));
    for (qreal t = 0.5; t < 1.01; t += 0.5) {
        const qreal y = hitY - fallSpan * t;
        painter->drawLine(QPointF(pad, y), QPointF(w - pad, y));
    }

    QVector<bool> lit(lanes + 1, false);

    for (const NoteBar &bar : m_bars) {
        if (bar.channel < 1 || bar.channel > lanes) {
            continue;
        }
        if (bar.endMs < now - 60) {
            continue;
        }
        if (bar.startMs > now + static_cast<qint64>(lookMs)) {
            continue;
        }

        // pianopi 风格：条高度 ∝ 时长，底部对齐 start 时刻的击打线位置。
        qreal bottom = hitY - (bar.startMs - now) * ppm;
        qreal top = hitY - (bar.endMs - now) * ppm;
        const bool active = (now >= bar.startMs && now <= bar.endMs);
        if (active) {
            bottom = hitY;
            lit[bar.channel] = true;
        } else if (now > bar.endMs) {
            continue;
        }

        if (bottom <= pad || top >= hitY) {
            continue;
        }
        top = qMax(pad, top);
        bottom = qMin(hitY, bottom);
        const qreal barH = bottom - top;
        if (barH < 2.0) {
            continue;
        }

        const qreal x = laneX(bar.channel) + 1.5;
        const qreal bw = qMax(3.0, laneW - 3.0);
        const QColor color = laneColor(bar.channel);

        QColor glow = color;
        glow.setAlpha(active ? 55 : 35);
        painter->fillRect(QRectF(x - 2.0, top, bw + 4.0, barH), glow);

        QPainterPath path;
        const qreal radius = qMin(5.0, bw * 0.35);
        path.addRoundedRect(QRectF(x, top, bw, barH), radius, radius);
        QColor body = color;
        body.setAlpha(active ? 245 : 200);
        painter->fillPath(path, body);

        painter->fillRect(QRectF(x + bw * 0.42, top + 1.0,
                                 qMax(1.0, bw * 0.16), qMax(1.0, barH - 2.0)),
                          QColor(255, 255, 255, 140));
        if (active) {
            painter->fillRect(QRectF(x, hitY - 3.0, bw, 3.0), QColor(255, 255, 255, 230));
        }
    }

    const qreal pulse = m_playing ? (0.45 + 0.2 * qSin(now / 180.0)) : 0.35;
    QColor hitColor(105, 183, 232, int(255 * pulse));
    painter->setPen(QPen(hitColor, 2));
    painter->drawLine(QPointF(pad - 4.0, hitY), QPointF(w - pad + 4.0, hitY));
    painter->fillRect(QRectF(pad - 4.0, hitY - 1.0, w - pad * 2.0 + 8.0, 2.0),
                      QColor(255, 255, 255, 160));

    for (int i = 0; i < lanes; ++i) {
        const int ch = i + 1;
        const qreal x = laneX(ch);
        const qreal ky = hitY + 6.0;
        const qreal kh = keyH - 8.0;
        const QColor color = laneColor(ch);
        const bool on = lit.value(ch, false);

        QPainterPath key;
        key.addRoundedRect(QRectF(x, ky, laneW, kh), 7, 7);
        painter->fillPath(key, on ? color : QColor(0xF7, 0xFB, 0xFE));
        painter->setPen(QPen(on ? color : QColor(0xD5, 0xE4, 0xEF), 1));
        painter->drawPath(key);

        if (lanes <= 24 || on) {
            painter->setPen(on ? QColor(Qt::white) : QColor(0x5A, 0x73, 0x8A));
            QFont font(QStringLiteral("Segoe UI"), laneW < 18 ? 8 : 10);
            painter->setFont(font);
            painter->drawText(QRectF(x, ky, laneW, kh), Qt::AlignCenter, QString::number(ch));
        }
    }

    if (m_bars.isEmpty()) {
        painter->setPen(QColor(36, 75, 112, 120));
        QFont font(QStringLiteral("Segoe UI"), 13);
        painter->setFont(font);
        painter->drawText(QRectF(0, 0, w, hitY), Qt::AlignCenter,
                          QStringLiteral("载入 MIDI / OCF 后，音符将沿通道落入击打线"));
    }
}

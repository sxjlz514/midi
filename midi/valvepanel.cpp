#include "valvepanel.h"

#include <QPainter>

#include "organstate.h"

ValvePanel::ValvePanel(QWidget *parent)
    : QWidget(parent)
{
    // 初始状态：48 路全关。
    m_bitmap = QByteArray(OrganState::BitmapBytes, '\0');
    setMinimumHeight(180);
}

void ValvePanel::setState(const QByteArray &bitmap)
{
    m_bitmap = bitmap;
    update(); // 请求 Qt 稍后重绘（触发 paintEvent）
}

void ValvePanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0x1f, 0x1f, 0x1f)); // 背景

    const int boardCount = OrganState::ValveCount / OrganState::ChannelsPerBoard; // 3
    const int perRow = OrganState::ChannelsPerBoard;                             // 16

    const int margin = 10;
    const int gap = 4;
    const int cellW = (width() - margin * 2 - gap * (perRow - 1)) / perRow;
    const int cellH = (height() - margin * 2 - gap * (boardCount - 1)) / boardCount;

    for (int board = 0; board < boardCount; ++board) {
        for (int channel = 0; channel < perRow; ++channel) {
            const int valveIndex = board * perRow + channel;

            // 从位图取状态：第 (valveIndex/8) 字节的第 (valveIndex%8) 位。
            bool on = false;
            if (valveIndex / 8 < m_bitmap.size()) {
                const unsigned char byte =
                    static_cast<unsigned char>(m_bitmap.at(valveIndex / 8));
                on = (byte >> (valveIndex % 8)) & 0x1;
            }

            const int x = margin + channel * (cellW + gap);
            const int y = margin + board * (cellH + gap);

            painter.setPen(Qt::NoPen);
            painter.setBrush(on ? QColor(0x3d, 0xd5, 0x6b)   // 开：绿
                                : QColor(0x50, 0x50, 0x50)); // 关：灰
            painter.drawRoundedRect(QRect(x, y, cellW, cellH), 4, 4);

            if (cellW > 22) {
                painter.setPen(on ? QColor(0x10, 0x30, 0x18) : QColor(0xc0, 0xc0, 0xc0));
                painter.drawText(QRect(x, y, cellW, cellH), Qt::AlignCenter,
                                 QString::number(valveIndex));
            }
        }
    }
}

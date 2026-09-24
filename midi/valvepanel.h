#pragma once

#include <QByteArray>
#include <QWidget>

// =============================================================================
//  ValvePanel.h —— 48 路电磁阀状态可视化
// -----------------------------------------------------------------------------
//  每个小方块代表一路电磁阀：灰色 = 关闭，绿色 = 打开。
//  3 个从控板各一行，每行 16 个方块。
//  setState() 接收 OrganState::snapshot() 生成的 6 字节位图。
// =============================================================================

class ValvePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ValvePanel(QWidget *parent = nullptr);

public slots:
    void setState(const QByteArray &bitmap); // 接收新位图并刷新

protected:
    void paintEvent(QPaintEvent *event) override; // 绘制入口

private:
    QByteArray m_bitmap; // 最近一次的 6 字节状态位图
};

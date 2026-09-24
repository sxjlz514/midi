#pragma once

#include <QMainWindow>

#include "midievent.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class AppController;
class MidiInput;
class ValvePanel;

// =============================================================================
//  MainWindow.h —— 主窗口
// -----------------------------------------------------------------------------
//  界面布局：
//    - 顶部：MIDI 设备选择行
//    - 第二行：串口选择行 + “模拟模式”勾选（无硬件调试用）
//    - 中部：48 路电磁阀状态面板
//    - 底部：日志窗口
//  主窗口只负责“显示”和“把用户操作转给 AppController”，不含业务逻辑。
// =============================================================================

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshMidiDevices();   // 刷新 MIDI 设备列表
    void toggleMidiDevice();     // 打开/关闭 MIDI 设备

    void refreshSerialPorts();   // 刷新串口列表
    void toggleSerial();         // 连接/断开串口（或模拟模式）
    void onSimulationToggled(bool on);

    void onMidiEvent(const MidiEvent &event); // 显示 MIDI 事件

    void onTestValve();          // 菜单：手动开关某一路
    void onAllOff();             // 菜单：全部关闭
    void onAbout();              // 菜单：关于

    void appendLog(const QString &text);
    void updateSerialStatus(bool connected);

private:
    void buildUi();      // 创建控件与布局
    void buildMenus();   // 创建菜单

    static QString noteName(int midiNote);

private:
    AppController *m_controller = nullptr;

    // --- MIDI 区 ---
    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QLabel *m_midiStatusLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    quint64 m_messageCount = 0;

    // --- 串口区 ---
    QComboBox *m_serialCombo = nullptr;
    QPushButton *m_serialRefreshButton = nullptr;
    QPushButton *m_serialOpenButton = nullptr;
    QCheckBox *m_simulationCheck = nullptr;
    QLabel *m_serialStatusLabel = nullptr;

    // --- 状态面板与日志 ---
    ValvePanel *m_valvePanel = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
};

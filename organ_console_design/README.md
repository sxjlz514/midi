# Piano Visualizer — UI 演示

钢琴可视化弹奏软件的前端界面工程。Qt 6 + QML，qmake 构建。
仅包含 UI 与演示数据，无后端逻辑（MIDI / 串口 / 文件解析 / 音频均未接入）。

## 构建运行

- **Qt Creator**：打开 `organ_console_design.pro`，选择 MinGW 套件，直接运行。
- **命令行**（本机已验证：Qt 6.10.3 mingw_64 + mingw1310_64）：

```bat
mkdir build
cd build
C:\Qt\6.10.3\mingw_64\bin\qmake.exe ..\organ_console_design.pro
C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -j4
release\organ_console_design.exe
```

> 直接运行 exe 需要 `C:\Qt\6.10.3\mingw_64\bin` 在 PATH 中（或用 windeployqt 拷贝依赖），否则会提示找不到 Qt 运行库。

## 演示操作

- **空载**：静态网格、无音符；网格始终不滚动，只有音符本身滚动。
- **实时弹奏**：无论是否录制，点按琴键都会产生向上滚动的音符流（播放模式下点按不产生实时音符）。
- **进度条**：位于标题栏下方、音符区上方。播放时显示进度与"当前 / 总时长"，录制时右侧显示"已录制 mm:ss"。
- **播放 / 停止**：未加载文件时禁用（灰色）；加载后可用。播放时音符向下落向播放线，左手青色、右手橙色，琴键同步高亮；可暂停 / 继续，曲目循环。
- **录制**：无需文件即可使用。开始录制时清空瀑布流、从头计时；停止录制（再点录制或点停止）弹出保存位置对话框（壳子不写文件）。录制中停止按钮可用、播放按钮禁用。
- **打开文件（拆分按钮）**：左半直接打开（演示为加载 demo.mid，正式版接文件对话框），右半箭头展开最近文件下拉；加载后窗口名显示文件名。
- **系统指示灯**：点击指示灯或"系统正常"文字，展开各子系统状态弹层。
- **琴键**：32 键 MIDI 键盘规格（F2–C5，19 白 13 黑），鼠标点按高亮（仅用于壳子演示，最终通过 MIDI 键盘弹奏，不做电脑键盘映射）。
- **设置**：独立无边框窗口，侧边栏四项。硬件连接页为条状列表：MIDI 设备单击选中、双击连接（演示状态）；串口行右侧可改波特率（9600–115200 下拉），双击连接。
- **窗口**：标题栏拖动、双击最大化、边缘缩放均走系统原生通道
  （`startSystemMove` / `startSystemResize`），Windows 下保留 Aero Snap。

## 文件结构

```
organ_console_design.pro   qmake 工程
main.cpp                   入口，仅加载 QML
qml.qrc                    QML 资源清单
qml/
  Main.qml                 主窗口：布局、模式状态机、边缘缩放
  TitleBar.qml             自绘标题栏：按钮、最近文件下拉、系统状态弹层
  TransportBar.qml         顶部进度条：播放进度 / 录制时长
  NoteField.qml            流式音符区：单 Canvas 批量绘制，时间驱动，双向滚动
  PianoKeyboard.qml        32 键键盘（F2–C5）：高亮、虚拟按键字母、鼠标点按
  SettingsWindow.qml       设置窗口：侧边栏 + 四个内容页，硬件连接为列表式设备管理
  Theme.qml                全局配色（深色硬边几何）
  piano.js                 32 键几何模型与命中测试
```

## 后端接入点（后续）

本工程只是 UI 壳子：不包含、也不预设 MIDI / 串口 / 音频 / 文件读写等后端实现与技术选型。
现有后端接入时，优先复用以下接口，不需要改动界面结构：

- 实时输入事件 → `NoteField.noteOn(midi) / noteOff(midi)`：替换鼠标点按这一演示来源即可，瀑布流与高亮自动生效。
- 播放数据 → 替换 `NoteField.demoNotes()`；`durationSec / timeSec` 已驱动进度条。
- `NoteField.sounding` → `PianoKeyboard.externalActive`：发声键高亮链路已通。
- 录制控制 → `Main.onRecordRequested / onStopRequested`；`NoteField.startRecording()` 负责清空与计时。
- 录制保存 → `Main.saveDialog.onAccepted` 中的 `saveDialog.selectedFile`，壳子未写文件。
- `TitleBar` 信号：`openRequested / fileSelected / playRequested / recordRequested / stopRequested / settingsRequested`。
- 滚动模型：`y = 播放线 ± (音符时间 - 当前时间) × pixelsPerSecond`，双向仅映射方向不同。

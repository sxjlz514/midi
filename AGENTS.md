# AGENTS.md

本文件为 AI 编码代理（及新加入的开发者）提供本仓库的工作指引。

## 项目概述

一个基于 **Qt Widgets** 的 Windows 桌面应用：连接 MIDI 键盘，实时监视并格式化显示 MIDI 消息，支持录制并保存为标准 MIDI 文件（SMF Format 0，`.mid`），也可打开并解析已有的 `.mid` 文件。

- 语言：C++17
- 框架：Qt 6（Widgets 模块），开发环境为 Qt Creator + `Desktop Qt 6.10.1 MinGW 64-bit` Kit
- 构建系统：qmake（`midi.pro`）
- 平台：**仅 Windows**（MIDI 输入依赖 WinMM API，链接 `-lwinmm`）

## 目录结构

```
midi.pro                  qmake 工程文件（SOURCES/HEADERS 清单，win32 链接 winmm）
main.cpp                  程序入口，创建 QApplication 和 MainWindow
mainwindow.{h,cpp,ui}     主窗口：QTabWidget 多页签（输入监视 / 录制），录音与打开文件按钮
midiinput.{h,cpp}         WinMM midiIn* API 封装，枚举/打开设备，回调收到消息后转发为 Qt 信号
midiinputpanel.{h,cpp}    输入面板：设备下拉框、刷新/打开按钮、状态与计数标签、消息日志
midimessageformatter.{h,cpp}  纯静态工具类：音符号 → 音名，状态字节 → 可读描述，组装日志行
midirecorder.{h,cpp}      录制音符事件（Note On/Off），保存为 SMF .mid 文件
midifiledecoder.{h,cpp}   解析 .mid 文件为事件列表 / 可读文本行
qt/                       旧版本工程的重复副本，仅作历史参考，不要修改、不要在其中构建
```

## 构建与运行

### 推荐：Qt Creator

用 Qt Creator 打开 `midi.pro`，选择 `Desktop Qt 6.10.1 MinGW 64-bit` Kit，直接构建运行。构建产物输出到 `build/Desktop_Qt_6_10_1_MinGW_64_bit-{Debug,Release}/`（已在 `.gitignore` 中忽略）。

### 命令行（需要 Qt 的 MinGW 环境在 PATH 中）

```powershell
# 在 Qt 命令行环境（如 "Qt 6.10.1 (MinGW ...)" 快捷方式）中执行
qmake midi.pro -spec win32-g++ "CONFIG+=debug"
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

注意：`qmake` / `mingw32-make` 默认**不在**系统 PATH 中，需先加载 Qt 环境（`qtenv2.bat`）或使用 Qt Creator。

## 代码约定

修改代码时请保持现有风格，与周围代码一致：

- **头文件保护**：统一使用 `#pragma once`。
- **命名**：类用 `PascalCase`（如 `MidiRecorder`）；成员变量带 `m_` 前缀并以 `nullptr`/`{}` 初始化（如 `QPushButton *m_recordButton = nullptr;`）；方法用 `camelCase`；Qt 固定宽度类型（`quint8`、`quint32`）用于 MIDI 字节数据。
- **Qt 习惯**：
  - 字符串字面量用 `QStringLiteral`，避免隐式转换。
  - 头文件中能用前置声明就不用 include（如 `class QPushButton;`），include 放在 .cpp。
  - 对象树管理内存：QObject 子对象指定 parent，不用手动 delete。
  - 信号/槽跨线程传递 MIDI 消息（见下文线程模型）。
- **格式化**：4 空格缩进；长表达式按现有风格换行——运算符/参数拆行、右括号独占一行并对齐（参见 `midimessageformatter.cpp`）。
- **注释与文案**：注释和用户可见字符串使用**中文**（如 `QStringLiteral("未知设备 %1")`）。块注释用 `/* ... */` 解释协议细节。
- **编码**：源文件为 UTF-8（部分文件带 BOM）。编辑时不要改动既有文件的 BOM 状态，确保中文字符串不损坏。

## 架构要点（修改前必读）

### 线程模型

`MidiInput::midiCallback` 是 WinMM 的 `CALLBACK_FUNCTION`，**运行在 WinMM 工作线程而非 Qt GUI 线程**。回调中禁止直接操作 GUI 或发射普通连接的信号——现有做法是通过 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 切回 Qt 线程后再 `emit messageReceived(...)`。任何改动都必须保持这一约束。

### WinMM 短消息打包

WinMM 把一条短 MIDI 消息压缩在一个 `DWORD` 中：bit 0–7 为 Status，bit 8–15 为 Data1，bit 16–23 为 Data2；`parameter2` 为时间戳（毫秒）。解包逻辑在 `midiinput.cpp`。

### 信号数据流

```
MidiInput::messageReceived(status, data1, data2, timestampMs)
    └─> MidiInputPanel::onMidiMessage   （格式化并追加日志，转发 received 信号）
        └─> MainWindow::onRecordMessage （录制中时转发给 MidiRecorder::appendMessage）
```

`MainWindow` 中有**两个** `MidiInputPanel` 实例（监视页 / 录制页），改动面板行为时注意两处都会受影响。

### MIDI 文件读写

- `MidiRecorder::save` 写出 SMF Format 0，只保留音符事件（`isNoteEvent` 过滤），毫秒经 `msToTicks` 转为 tick，delta time 用变长编码（`writeVarLen`），多字节整数大端（`writeUInt32BE`）。
- `MidiFileDecoder` 是独立解析器，与录制器格式约定保持一致；改一侧的编码约定时检查另一侧。

## 修改工程文件时

- 新增源文件/头文件：必须同时加入 `midi.pro` 的 `SOURCES` / `HEADERS` 列表，否则 qmake 不会编译。
- 新增 Qt 模块依赖：加入 `midi.pro` 的 `QT += ...`（当前仅有 `widgets`）。
- 引入新的 Windows 系统库：追加到 `win32:LIBS`（当前为 `-lwinmm`）。

## 测试与验证

- 项目**没有**自动化测试和 CI。验证方式：构建成功后运行程序，连接（或虚拟）MIDI 设备，确认消息实时显示、录制保存的 `.mid` 能被本程序的“打开文件”功能正确解析。
- 提交前至少确认 qmake + MinGW 构建无警告错误，且新增文件已列入 `midi.pro`。
- 不要提交 `*.pro.user`、`build/` 目录等本地产物（见 `.gitignore`）。

## 版本控制

- 提交信息使用中文、简短描述（历史风格：`添加新页面`、`QT软件第一次构建`）。
- 主分支为 `main`，远端为 GitHub `zhchmmx/midi`。

## 常见陷阱

1. `qt/` 子目录是旧副本——在仓库根目录编辑，不要改 `qt/` 里的文件。
2. 在 WinMM 回调线程里碰 GUI 会崩溃——走 `Qt::QueuedConnection`。
3. 忘记把新文件加进 `midi.pro` 会导致“明明写了代码却不编译”。
4. `midiinput.h` 定义 `NOMINMAX` 后才包含 `windows.h`——包含 Windows 头文件的新代码同样注意 `min`/`max` 宏冲突。

# midi_prase — MIDI 解析与 16 路自动演奏

路径：`E:\shengxuejisuan\qt_test\midi_prase_main\midi_prase`

对接主板协议：`ON n\n` / `OFF n\n` / `ALL_OFF\n`（n=1..16）。

## 默认键位映射表（音高 ↔ 通道）

连续半音，起点 **C4 = MIDI 60**：

| 通道 | 从板 | MIDI 音高 | 音名 |
|------|------|-----------|------|
| 1 | N1 | 60 | C4 |
| 2 | N1 | 61 | C#4 |
| 3 | N1 | 62 | D4 |
| 4 | N1 | 63 | D#4 |
| 5 | N1 | 64 | E4 |
| 6 | N1 | 65 | F4 |
| 7 | N1 | 66 | F#4 |
| 8 | N1 | 67 | G4 |
| 9 | N2 | 68 | G#4 |
| 10 | N2 | 69 | A4 |
| 11 | N2 | 70 | A#4 |
| 12 | N2 | 71 | B4 |
| 13 | N2 | 72 | C5 |
| 14 | N2 | 73 | C#5 |
| 15 | N2 | 74 | D5 |
| 16 | N2 | 75 | D#5 |

程序内表格可双击「MIDI音高」列修改；未映射音高会被跳过并写日志。

## 使用

1. Qt Creator 打开 `midi_prase.pro`，勾选 `serialport`，编译运行  
2. 打开主板 COM（默认 115200）  
3. 打开 `.mid` / `.midi` → 播放 / 暂停 / 停止  
4. 停止或曲终会发 `ALL_OFF`

## 模块

| 文件 | 作用 |
|------|------|
| `keymapping.*` | 16 路映射表 |
| `midiparser.*` | SMF Format 0/1 解析（Note/Tempo） |
| `midiplayer.*` | 按时间轴发通道开关 |
| `mainwindow.*` | 串口 + UI |

## 说明

- 支持 Format 0/1、变长量、Running Status、Tempo(0x51)  
- 不支持 Format 2 / SMPTE 时基  
- 曲目音域请落在映射表内，或改表对齐你的 MIDI  

# organ_console_QML — QML 管风琴控制台

这是独立的 QML 界面版本，保留串口控制、实时 MIDI、自动演奏和 32 位键盘映射能力。界面采用古蓝、天蓝与白色为主色，并以淡绿和淡黄区分状态。

左侧分为六个工作页：控制总览、实时 MIDI、自动演奏、直接控制、键位映射和运行日志。自动演奏页可加载 MIDI/OCF、设置映射范围与聚合窗、转换 MIDI 为 OCF，并查看 OCF 的可读文本解析。

用 Qt Creator 打开 `organ_console.pro` 可直接构建。命令行构建需要 Qt Quick、Qt Quick Controls 2、Qt Serial Port 和对应的 Qt 编译器；MIDI 转 OCF 还需要系统可调用 Python 3。

---

## 控制器与位图协议

## 本次改动（对齐设计稿）

### 1. 短窗聚合（Qt）
- `KeyStateAggregator`：按键只改 shadow，**默认 4 ms** 窗满后发一张完整位图
- 不再每键一条 `ON/OFF`（多键同时按下合并为 1 次 UART）

### 2. UART 帧
```
@ST,<seq4>,<t_ms8>,<bitmap8>,<crc4>\n
```
- `bitmap`：32 位十六进制，bit0=键1（当前硬件用低 16 位）
- CRC16-CCITT-FALSE，覆盖 `ST,seq,t,bitmap`
- 主板成功发 CAN 后回：`@OK,<seq>,0`；失败：`@ERR,<seq>,code`

### 3. CAN 仲裁与单帧广播
| ID | 用途 | 优先级 |
|----|------|--------|
| 0x000 | ALL_OFF | 最高 |
| 0x100 DLC=4 | 键位位图（大端） | 高 |
| 0x100 其它 DLC | DISCOVER/SYNC/SYS | 同 ID 靠 DLC 区分 |
| 0x201+ | 从板上报 | 低（不与键位抢总线） |

载荷示例 `bitmap=0x00000103` → DATA `00 00 01 03`
N1 读 `data[3]`，N2 读 `data[2]`。

### 4. Bus-Off 恢复（主板）
- 使能 TWAI alerts；`INVALID_STATE`/Bus-Off 时 `initiate_recovery` + `twai_start` 并重试发送

## 烧录 / 编译
1. 重新 flash：`can_master`、`can_slave1`、`can_slave2`
2. 重新编译运行 `organ_console.pro`

## 调试
勾选「调试回报」可看到 `[TX] @ST ...` 与主板 `@OK`/`KEY=...`。

## OCF 位图流

自动演奏页可将 MIDI 转为二进制 `.ocf` 文件，也可打开 OCF 播放或查看文本解析结果。

- 默认映射为 MIDI 音高 `60..91`（首尾包含，正好 32 键）；范围外音符忽略。取余模式按范围宽度循环映射。
- 默认聚合窗为 30 ms。事件静默达到窗口后，转换器记录一帧完整的 32 位键状态。
- 输出默认位于输入 MIDI 同级的 `converted` 目录；OCF 含版本、映射、聚合窗、帧时间、位图和 CRC32。
- 转换脚本随 Qt 资源嵌入；运行转换功能需要系统安装 Python 3，并可通过 `python`、`python3` 或 `py -3` 调用。脚本本身不依赖第三方包。
- OCF 帧按文件中的聚合时间直接发送，不会再经过实时短窗聚合。软件的位图帧为 32 位；当前项目配套固件仍注明只使用低 16 位。

OCF v1 使用小端序：24 字节头部依次包含 `OCF1`、版本、位图宽度、映射模式、范围首尾、保留位、聚合窗、总时长和帧数；其后每帧为 64 位毫秒时间戳与 32 位位图，文件尾是覆盖头部和帧数据的 CRC32。位图 bit0 对应目标通道 1。

最小演示文件：`examples/minimal_test.mid`。其默认转换结果是 `examples/converted/minimal_test.ocf`，包含 30 ms 时通道 1 按下，以及 530 ms 时释放。

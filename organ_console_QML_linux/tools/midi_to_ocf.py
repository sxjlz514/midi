#!/usr/bin/env python3
"""Convert a Standard MIDI file into organ_console's binary OCF bitmap stream."""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from collections import defaultdict
from itertools import groupby
from pathlib import Path


HEADER = struct.Struct("<4sBBBBBBHQI")
FRAME = struct.Struct("<QI")
MAGIC = b"OCF1"
VERSION = 1


class MidiError(ValueError):
    pass


def read_vlq(data: bytes, pos: int, end: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if pos >= end:
            raise MidiError("MIDI 变长数量越界")
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, pos
    raise MidiError("MIDI 变长数量超过 4 字节")


def parse_midi(path: Path) -> tuple[list[tuple[int, int, int, int, int]], int]:
    data = path.read_bytes()
    if len(data) < 14 or data[:4] != b"MThd":
        raise MidiError("缺少有效的 MThd 文件头")
    header_len = int.from_bytes(data[4:8], "big")
    if header_len < 6 or 8 + header_len > len(data):
        raise MidiError("MIDI 文件头长度非法")
    fmt = int.from_bytes(data[8:10], "big")
    track_count = int.from_bytes(data[10:12], "big")
    division = int.from_bytes(data[12:14], "big")
    if fmt not in (0, 1):
        raise MidiError("只支持 MIDI Format 0/1")
    if division == 0 or division & 0x8000:
        raise MidiError("不支持 SMPTE 时基或零 tick 时基")

    pos = 8 + header_len
    raw_events: list[tuple[int, int, int, int, int, int]] = []
    tempos: list[tuple[int, int, int]] = []
    order = 0

    for _track in range(track_count):
        if pos + 8 > len(data) or data[pos:pos + 4] != b"MTrk":
            raise MidiError("缺少有效的 MTrk 数据块")
        length = int.from_bytes(data[pos + 4:pos + 8], "big")
        pos += 8
        end = pos + length
        if end > len(data):
            raise MidiError("MTrk 数据块超出文件长度")

        tick = 0
        running_status = 0
        while pos < end:
            delta, pos = read_vlq(data, pos, end)
            tick += delta
            if pos >= end:
                raise MidiError("MIDI 事件越界")

            first = data[pos]
            if first & 0x80:
                status = first
                pos += 1
                if 0x80 <= status <= 0xEF:
                    running_status = status
            else:
                if running_status == 0:
                    raise MidiError("MIDI 事件缺少 running status")
                status = running_status

            if status == 0xFF:
                if pos >= end:
                    raise MidiError("Meta 事件越界")
                meta_type = data[pos]
                pos += 1
                meta_len, pos = read_vlq(data, pos, end)
                if pos + meta_len > end:
                    raise MidiError("Meta 数据越界")
                payload = data[pos:pos + meta_len]
                pos += meta_len
                if meta_type == 0x51 and len(payload) == 3:
                    us_per_quarter = int.from_bytes(payload, "big")
                    tempos.append((tick, us_per_quarter or 500000, order))
                    order += 1
                if meta_type == 0x2F:
                    pos = end
                continue

            if status in (0xF0, 0xF7):
                sysex_len, pos = read_vlq(data, pos, end)
                if pos + sysex_len > end:
                    raise MidiError("SysEx 数据越界")
                pos += sysex_len
                running_status = 0
                continue

            event_type = status & 0xF0
            channel = status & 0x0F
            data_len = 1 if event_type in (0xC0, 0xD0) else 2
            if event_type < 0x80 or event_type > 0xE0 or pos + data_len > end:
                raise MidiError(f"不支持或不完整的 MIDI 状态字节 0x{status:02X}")
            first_data = data[pos]
            second_data = data[pos + 1] if data_len == 2 else 0
            pos += data_len
            if event_type in (0x80, 0x90):
                is_on = event_type == 0x90 and second_data > 0
                raw_events.append((tick, 1 if is_on else 0,
                                   first_data, channel, order, second_data))
                order += 1

        pos = end

    if not raw_events:
        return [], 0

    # Resolve same-tick tempo changes deterministically; the last event wins.
    tempo_by_tick: dict[int, tuple[int, int]] = {0: (500000, -1)}
    for tick, tempo, tempo_order in tempos:
        current = tempo_by_tick.get(tick)
        if current is None or tempo_order >= current[1]:
            tempo_by_tick[tick] = (tempo, tempo_order)
    tempo_points = sorted((tick, item[0]) for tick, item in tempo_by_tick.items())

    def ticks_to_ms(target_tick: int) -> int:
        elapsed = 0.0
        for i, (start_tick, tempo) in enumerate(tempo_points):
            if target_tick <= start_tick:
                break
            end_tick = tempo_points[i + 1][0] if i + 1 < len(tempo_points) else target_tick
            segment_end = min(target_tick, end_tick)
            if segment_end > start_tick:
                elapsed += ((segment_end - start_tick) * tempo) / (division * 1000.0)
            if segment_end >= target_tick:
                break
        return int(elapsed + 0.5)

    # OFF before ON at an equal timestamp, matching MidiParser's stable ordering.
    ordered = sorted(raw_events, key=lambda e: (ticks_to_ms(e[0]), e[1], e[4]))
    events = [(ticks_to_ms(tick), kind, note, channel, velocity)
              for tick, kind, note, channel, _order, velocity in ordered]
    return events, max(event[0] for event in events)


def encode_ocf(events: list[tuple[int, int, int, int, int]], duration_ms: int,
               mode: str, low: int, high: int, window_ms: int) -> bytes:
    span = high - low + 1
    if not 0 <= low <= high <= 127:
        raise ValueError("音高范围必须满足 0 <= 起始音高 <= 结束音高 <= 127")
    if span > 32:
        raise ValueError("32 位位图最多映射 32 个音高；范围按首尾都包含计算")
    if not 1 <= window_ms <= 65535:
        raise ValueError("聚合窗必须在 1..65535 ms 之间")

    def slot_for(note: int) -> int | None:
        if mode == "range":
            return note - low if low <= note <= high else None
        return (note - low) % span

    # ref_by_source preserves overlapping/unison notes, including folded notes.
    ref_by_source: dict[tuple[int, int], int] = defaultdict(int)
    refs_by_slot = [0] * 32
    bitmap = 0
    last_emitted = 0
    frames: list[tuple[int, int]] = []
    pending = False
    deadline = 0

    def flush(at_ms: int) -> None:
        nonlocal pending, last_emitted
        if not pending:
            return
        if bitmap != last_emitted:
            frames.append((at_ms, bitmap))
            last_emitted = bitmap
        pending = False

    grouped = groupby(events, key=lambda event: event[0])
    last_event_ms = 0
    for event_ms, group in grouped:
        group_events = list(group)
        if pending and event_ms > deadline:
            flush(deadline)

        did_mapped_event = False
        for _time, kind, note, channel, _velocity in group_events:
            slot = slot_for(note)
            if slot is None:
                continue
            did_mapped_event = True
            source = (channel, note)
            bit = 1 << slot
            if kind == 1:
                ref_by_source[source] += 1
                refs_by_slot[slot] += 1
                bitmap |= bit
            elif ref_by_source[source] > 0:
                ref_by_source[source] -= 1
                refs_by_slot[slot] -= 1
                if refs_by_slot[slot] == 0:
                    bitmap &= ~bit

        if did_mapped_event:
            pending = True
            deadline = event_ms + window_ms
        last_event_ms = max(last_event_ms, event_ms)

    # MidiPlayer releases any notes still held when the source file ends.
    if any(ref_by_source.values()):
        if pending and duration_ms > deadline:
            flush(deadline)
        for slot, count in enumerate(refs_by_slot):
            if count:
                refs_by_slot[slot] = 0
                bitmap &= ~(1 << slot)
        pending = True
        deadline = max(duration_ms, last_event_ms) + window_ms

    if pending:
        flush(deadline)
    if not frames or frames[-1][1] != 0:
        frames.append((max(duration_ms, frames[-1][0] if frames else 0), 0))

    file_duration = max(duration_ms, frames[-1][0])
    header = HEADER.pack(MAGIC, VERSION, 32, 0 if mode == "range" else 1,
                         low, high, 0, window_ms, file_duration, len(frames))
    payload = header + b"".join(FRAME.pack(time_ms, value) for time_ms, value in frames)
    return payload + struct.pack("<I", zlib.crc32(payload) & 0xFFFFFFFF)


def main() -> int:
    parser = argparse.ArgumentParser(description="MIDI 转 organ_console OCF 32 位位图流")
    parser.add_argument("midi", type=Path, help="输入 .mid/.midi 文件")
    parser.add_argument("--mapping", choices=("range", "wrap"), default="range",
                        help="range=范围外忽略；wrap=按范围长度循环映射")
    parser.add_argument("--range-start", type=int, default=60)
    parser.add_argument("--range-end", type=int, default=91,
                        help="包含结束音高，默认 60..91 共 32 键")
    parser.add_argument("--window-ms", type=int, default=30)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    try:
        events, duration = parse_midi(args.midi)
        data = encode_ocf(events, duration, args.mapping, args.range_start,
                          args.range_end, args.window_ms)
        output = args.output or (args.midi.parent / "converted" / (args.midi.stem + ".ocf"))
        if output.resolve() == args.midi.resolve():
            raise ValueError("输出文件不能覆盖 MIDI 输入文件")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
    except (OSError, MidiError, ValueError, struct.error) as exc:
        print(f"转换失败：{exc}", file=sys.stderr)
        return 2

    print(f"OCF 已生成：{output}")
    print(f"映射：{args.mapping} {args.range_start}..{args.range_end}；聚合窗：{args.window_ms} ms")
    print(f"位图记录：{len(data) and struct.unpack_from('<I', data, 20)[0]}；文件大小：{len(data)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

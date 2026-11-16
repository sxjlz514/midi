.pragma library

// 通道布局：按通道画键，黑白由该通道映射的音高决定。
// 绘制、命中、音符横坐标共用同一套结果，保证不重叠且点击准确。

var BLACK_RATIO = 0.6;        // 黑键宽 / 白键宽
var BLACK_HEIGHT_RATIO = 0.62;

function isBlackNote(note) {
    var s = ((note % 12) + 12) % 12;
    return s === 1 || s === 3 || s === 6 || s === 8 || s === 10;
}

// notes: 每通道的映射音高（数组长度即通道数）
// 返回按通道顺序排列的 [{x, w, black, slotMode}]，索引 i 对应通道 i+1
function buildLayout(notes, width) {
    var result = [];
    var n = notes ? notes.length : 0;
    if (n <= 0 || width <= 0) return result;

    var flags = [];
    var whiteCount = 0;
    for (var i = 0; i < n; i++) {
        var black = isBlackNote(notes[i]);
        flags.push(black);
        if (!black) whiteCount++;
    }

    // 全黑：退化为等宽槽，只保留深浅
    if (whiteCount === 0) {
        var slot = width / n;
        for (var s = 0; s < n; s++) {
            result.push({ x: s * slot, w: slot, black: true, slotMode: true });
        }
        return result;
    }

    var unit = width / whiteCount;
    var bw = unit * BLACK_RATIO;

    // 分组：一个白键 + 其后连续的黑键
    var groups = [];
    var k = 0;
    while (k < n) {
        var g = { start: k, blackCount: 0 };
        k++; // 白键
        while (k < n && flags[k]) {
            g.blackCount++;
            k++;
        }
        var overflow = g.blackCount > 0 ? (g.blackCount * bw + (g.blackCount - 1)) : 0;
        g.need = Math.max(unit, overflow);
        groups.push(g);
    }

    var sumNeed = 0;
    for (var a = 0; a < groups.length; a++) sumNeed += groups[a].need;

    var cursor = 0;
    for (var b = 0; b < groups.length; b++) {
        var gg = groups[b];
        var gw = gg.need / sumNeed * width;
        result.push({ x: cursor, w: gw, black: false, slotMode: false });
        for (var j = 0; j < gg.blackCount; j++) {
            var rightIndex = gg.blackCount - j;  // 从右往左数第几个黑键
            var bx = cursor + gw - rightIndex * bw - (rightIndex - 1);
            result.push({ x: bx, w: bw, black: true, slotMode: false });
        }
        cursor += gw;
    }
    return result;
}

// 命中测试：返回通道号（1 起），无则 -1
function channelAt(x, y, layout, height) {
    if (!layout || layout.length === 0) return -1;

    if (layout[0].slotMode) {
        var idx = Math.floor(x / layout[0].w);
        return (idx >= 0 && idx < layout.length) ? idx + 1 : -1;
    }

    var blackH = height * BLACK_HEIGHT_RATIO;
    var i;
    if (y <= blackH) {
        for (i = 0; i < layout.length; i++) {
            var rb = layout[i];
            if (!rb.black) continue;
            if (x >= rb.x && x < rb.x + rb.w) return i + 1;
        }
    }
    for (i = 0; i < layout.length; i++) {
        var rw = layout[i];
        if (rw.black) continue;
        if (x >= rw.x && x < rw.x + rw.w) return i + 1;
    }
    return -1;
}

// 音符横坐标与宽度
function centerX(channel, layout) {
    if (!layout || channel < 1 || channel > layout.length) return -1;
    var r = layout[channel - 1];
    return r.x + r.w / 2;
}

function barWidth(channel, layout) {
    if (!layout || channel < 1 || channel > layout.length) return 2;
    var r = layout[channel - 1];
    return Math.max(3, r.w * 0.86);
}

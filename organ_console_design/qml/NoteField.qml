import QtQuick 2.15
import "piano.js" as Piano

// 音符区：
// - 非播放态：实时弹奏的音符从下往上滑动（无论是否录制）
// - 播放态：文件音符从上往下滑向播放线
// 网格静止不滚动，只有音符滚动。
Item {
    id: root

    property var mappingNotes: []
    property var notes: []            // 播放音符：{channel, startMs, endMs}
    property bool playbackMode: false
    property real positionMs: 0
    property real pixelsPerSecond: 96
    property bool running: true
    property real nowMs: 0

    property var liveNotes: []        // {channel, start, end}，end<0 表示仍按住

    Theme { id: theme }

    property var layout: Piano.buildLayout(root.mappingNotes, width)

    onMappingNotesChanged: canvas.requestPaint()
    onNotesChanged: canvas.requestPaint()
    onPlaybackModeChanged: {
        if (playbackMode) {
            positionMs = 0;
        } else {
            nowMs = 0;
            liveNotes = [];
        }
        canvas.requestPaint();
    }
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    // 开始录制：清空实时瀑布流，重新计时
    function startRecording() {
        liveNotes = [];
        nowMs = 0;
        canvas.requestPaint();
    }

    function noteOn(channel) {
        if (playbackMode) return;
        liveNotes = liveNotes.concat([{ channel: channel, start: nowMs, end: -1 }]);
        canvas.requestPaint();
    }

    function noteOff(channel) {
        if (playbackMode) return;
        var list = liveNotes.slice();
        for (var i = list.length - 1; i >= 0; i--) {
            if (list[i].channel === channel && list[i].end < 0) {
                list[i] = { channel: channel, start: list[i].start, end: nowMs };
                break;
            }
        }
        liveNotes = list;
        canvas.requestPaint();
    }

    function prune() {
        if (liveNotes.length === 0) return;
        var visibleMs = (height / Math.max(1, pixelsPerSecond)) * 1000 + 300;
        var kept = [];
        for (var i = 0; i < liveNotes.length; i++) {
            var n = liveNotes[i];
            if (n.end < 0 || (nowMs - n.end) <= visibleMs) kept.push(n);
        }
        if (kept.length !== liveNotes.length) liveNotes = kept;
    }

    function rgba(hex, a) {
        hex = String(hex);
        return "rgba(" + parseInt(hex.substr(1, 2), 16) + ","
                + parseInt(hex.substr(3, 2), 16) + ","
                + parseInt(hex.substr(5, 2), 16) + "," + a + ")";
    }

    function clamp01(v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

    Timer {
        interval: 16
        running: root.running
        repeat: true
        onTriggered: {
            if (root.playbackMode) {
                root.positionMs = console.livePositionMs();
            } else {
                root.nowMs += interval;
                root.prune();
            }
            canvas.requestPaint();
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            var W = width, H = height;
            ctx.clearRect(0, 0, W, H);
            ctx.fillStyle = theme.bg;
            ctx.fillRect(0, 0, W, H);

            var lay = root.layout;
            root.drawGrid(ctx, W, H, lay);
            if (lay && lay.length > 0) {
                if (root.playbackMode) root.drawPlayback(ctx, W, H, lay);
                else root.drawLive(ctx, W, H, lay);
            }

            // 播放线
            ctx.fillStyle = root.playbackMode ? theme.accent : root.rgba(theme.accent, 0.25);
            ctx.fillRect(0, H - 2, W, 2);
        }
    }

    function drawGrid(ctx, W, H, lay) {
        if (!lay || lay.length === 0) return;
        ctx.strokeStyle = rgba(theme.border, 0.45);
        ctx.lineWidth = 1;
        ctx.beginPath();
        for (var i = 1; i < lay.length; i++) {
            var x = Math.round(lay[i].x) + 0.5;
            ctx.moveTo(x, 0);
            ctx.lineTo(x, H);
        }
        ctx.stroke();

        // 水平线：固定不动，只有音符滚
        var spacing = Math.max(12, root.pixelsPerSecond * 0.5);
        ctx.strokeStyle = rgba(theme.border, 0.6);
        ctx.beginPath();
        for (var y = H; y > 0; y -= spacing) {
            ctx.moveTo(0, Math.round(y) + 0.5);
            ctx.lineTo(W, Math.round(y) + 0.5);
        }
        ctx.stroke();
    }

    function drawPlayback(ctx, W, H, lay) {
        var pps = root.pixelsPerSecond;
        var pos = root.positionMs;
        for (var i = 0; i < root.notes.length; i++) {
            var n = root.notes[i];
            if (n.channel < 1 || n.channel > lay.length) continue;
            var headY = H - (n.startMs - pos) * pps / 1000;
            var tailY = H - (n.endMs - pos) * pps / 1000;
            if (headY < 0 || tailY > H) continue;
            var top = Math.max(tailY, 0);
            var bottom = Math.min(headY, H);
            if (bottom - top < 1) continue;
            var alpha = 0.3 + 0.7 * clamp01(headY / H);
            var warm = n.channel * 2 > lay.length;
            ctx.fillStyle = rgba(warm ? theme.accentWarm : theme.accent, alpha);
            var x = Piano.centerX(n.channel, lay);
            var nw = Piano.barWidth(n.channel, lay);
            ctx.fillRect(x - nw / 2, top, nw, bottom - top);
        }
    }

    function drawLive(ctx, W, H, lay) {
        var pps = root.pixelsPerSecond;
        var now = root.nowMs;
        for (var i = 0; i < root.liveNotes.length; i++) {
            var n = root.liveNotes[i];
            if (n.channel < 1 || n.channel > lay.length) continue;
            if (now < n.start) continue;
            var headY = H - (now - n.start) * pps / 1000;
            var tailY = n.end >= 0 ? H - (now - n.end) * pps / 1000 : H;
            if (tailY < 0 || headY > H) continue;
            var top = Math.max(headY, 0);
            var bottom = Math.min(tailY, H);
            if (bottom - top < 1) continue;
            var alpha = 0.15 + 0.85 * clamp01(headY / H);
            var warm = n.channel * 2 > lay.length;
            ctx.fillStyle = rgba(warm ? theme.accentWarm : theme.accent, alpha);
            var x = Piano.centerX(n.channel, lay);
            var nw = Piano.barWidth(n.channel, lay);
            ctx.fillRect(x - nw / 2, top, nw, bottom - top);
        }
    }
}

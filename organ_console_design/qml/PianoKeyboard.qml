import QtQuick 2.15
import "piano.js" as Piano

// 底部键盘：键数 = 通道数，黑白由该通道映射的音高决定。
// 播放期间 interactive=false，禁止任何鼠标点击。
Item {
    id: root

    property var mappingNotes: []      // 每通道映射音高
    property var activeChannels: []    // 当前发声通道
    property bool interactive: true

    signal channelPressed(int channel)
    signal channelReleased(int channel)
    signal inputBlocked(bool pressed)

    Theme { id: theme }

    property var layout: Piano.buildLayout(root.mappingNotes, width)

    function isActive(channel) {
        return root.activeChannels.indexOf(channel) >= 0;
    }

    onMappingNotesChanged: canvas.requestPaint()
    onActiveChannelsChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

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
            if (!lay || lay.length === 0) return;

            var blackH = lay[0].slotMode ? H : H * Piano.BLACK_HEIGHT_RATIO;
            var i, r, active;

            // 白键（或退化槽）
            for (i = 0; i < lay.length; i++) {
                r = lay[i];
                if (r.black && !r.slotMode) continue;
                active = root.isActive(i + 1);
                ctx.fillStyle = active ? theme.accent
                                       : (r.slotMode ? "#2A2F36" : "#E8EBEF");
                ctx.fillRect(r.x + 0.5, 0, Math.max(1, r.w - 1), H - 1);
            }

            // 黑键
            for (i = 0; i < lay.length; i++) {
                r = lay[i];
                if (!r.black || r.slotMode) continue;
                active = root.isActive(i + 1);
                ctx.fillStyle = active ? theme.accent : "#101318";
                ctx.fillRect(r.x, 0, Math.max(1, r.w - 0.5), blackH);
                ctx.strokeStyle = theme.border;
                ctx.lineWidth = 1;
                ctx.strokeRect(r.x + 0.5, 0.5, Math.max(1, r.w - 1), blackH - 1);
            }

            // 底部分线
            ctx.fillStyle = theme.border;
            ctx.fillRect(0, H - 1, W, 1);
        }
    }

    MouseArea {
        anchors.fill: parent
        property int pressedChannel: -1
        property bool blockedPress: false

        function reset() {
            if (pressedChannel >= 0) {
                root.channelReleased(pressedChannel);
                pressedChannel = -1;
            }
        }

        onPressed: function(mouse) {
            if (!root.interactive) {
                blockedPress = true;
                root.inputBlocked(true);
                return;
            }
            var channel = Piano.channelAt(mouse.x, mouse.y, root.layout, height);
            if (channel >= 1) {
                pressedChannel = channel;
                root.channelPressed(channel);
            }
        }
        onReleased: {
            if (blockedPress) {
                blockedPress = false;
                root.inputBlocked(false);
                return;
            }
            reset();
        }
        onCanceled: {
            if (blockedPress) {
                blockedPress = false;
                root.inputBlocked(false);
            }
            reset();
        }
        onExited: {
            if (blockedPress) {
                blockedPress = false;
                root.inputBlocked(false);
            }
            reset();
        }
    }
}

import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Dialogs

// 主窗口：无边框 + 自绘标题栏 + 进度条 + 音符区 + 键盘
Window {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1000
    minimumHeight: 640
    title: "Organ Console"
    color: "#0F1216"
    flags: Qt.Window | Qt.FramelessWindowHint

    Theme { id: theme }

    property var mappingNotes: {
        var list = [];
        var src = console.displayMapping;
        for (var i = 0; i < src.length; i++) list.push(src[i].midiNote);
        return list;
    }

    property string pendingMidiPath: ""
    property bool pendingRecordAfterSave: false
    property bool pendingCloseAfterSave: false
    property bool lockBarOn: false
    property bool lockHeld: false

    function baseName(path) {
        var slash = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"));
        return slash >= 0 ? path.substring(slash + 1) : path;
    }

    function localPath(url) {
        var s = String(url);
        if (s.indexOf("file:///") === 0) s = s.substring(8);
        else if (s.indexOf("file://") === 0) s = s.substring(7);
        try { s = decodeURIComponent(s); } catch (e) { }
        return s;
    }

    function showToast(message) {
        if (!message || message.length === 0) return;
        toastText.text = message;
        toastBar.visible = true;
        toastTimer.restart();
    }

    function openPath(path) {
        if (!path || path.length === 0) return;
        var lower = path.toLowerCase();
        if (lower.endsWith(".mid") || lower.endsWith(".midi")) {
            pendingMidiPath = path;
            var rows = console.mappingRows;
            var minNote = 127;
            var maxNote = 0;
            for (var i = 0; i < rows.length; i++) {
                minNote = Math.min(minNote, rows[i].midiNote);
                maxNote = Math.max(maxNote, rows[i].midiNote);
            }
            transformDialog.defaultStart = rows.length > 0 ? minNote : 60;
            transformDialog.defaultEnd = rows.length > 0 ? maxNote : 91;
            transformDialog.channelLimit = console.channelCount;
            transformDialog.fileName = baseName(path);
            transformDialog.shown = true;
        } else {
            var err = console.loadOcfFile(path);
            if (err.length > 0) showToast(err);
        }
    }

    function beginRecord() {
        var err = console.startRecording();
        if (err.length > 0) {
            showToast(err);
            return;
        }
        noteField.startRecording();
    }

    // 键盘锁定提示：按下出现，按住保持，松开 1.5 秒后消失
    function showLockBar(pressed) {
        if (!console.fileLoaded) {
            lockHeld = false;
            lockBarOn = false;
            return;
        }
        lockHeld = pressed;
        lockHideTimer.stop();
        lockBarOn = true;
        if (!pressed) lockHideTimer.start();
    }

    function requestCloseFile() {
        if (!console.fileLoaded) return;
        if (console.unsavedRecording) {
            closeDialog.shown = true;
            return;
        }
        console.closeFile();
    }

    function requestRecord() {
        if (console.recording) {
            stopRecording();
            return;
        }
        if (console.unsavedRecording && console.canSave) {
            unsavedDialog.shown = true;
            return;
        }
        beginRecord();
    }

    function stopRecording() {
        console.stopRecording();
        if (console.canSave) {
            saveDialog.selectedFile = "";
            saveDialog.open();
        }
    }

    Column {
        anchors.fill: parent

        TitleBar {
            id: titleBar
            width: parent.width
            z: 10
            onOpenRequested: openDialog.open()
            onRecentSelected: function(path) { root.openPath(path) }
            onCloseFileRequested: root.requestCloseFile()
            onSaveRequested: {
                saveDialog.selectedFile = "";
                saveDialog.open();
            }
            onPlayRequested: {
                if (console.playing && !console.paused) {
                    console.pause();
                } else if (console.paused) {
                    console.play();
                } else {
                    console.play();
                }
            }
            onStopRequested: {
                if (console.recording) root.stopRecording();
                else console.stop();
            }
            onRecordRequested: root.requestRecord()
            onSettingsRequested: settingsWindow.show()
            onMinimizeRequested: root.showMinimized()
            onMaximizeRequested: {
                if (root.visibility === Window.Maximized) root.showNormal();
                else root.showMaximized();
            }
            onCloseRequested: root.close()
        }

        TransportBar {
            id: transportBar
            width: parent.width
            active: console.playing || console.paused
            recording: console.recording
            positionMs: console.positionMs
            durationMs: console.durationMs
            recordElapsedMs: console.recordElapsedMs
        }

        NoteField {
            id: noteField
            width: parent.width
            height: parent.height - titleBar.height - transportBar.height - keyboard.height
            mappingNotes: root.mappingNotes
            notes: console.waterfallNotes
            playbackMode: console.playing || console.paused
            running: true
        }

        PianoKeyboard {
            id: keyboard
            width: parent.width
            height: 132
            mappingNotes: root.mappingNotes
            activeChannels: console.activeChannels
            interactive: !console.fileLoaded
            onChannelPressed: function(channel) { console.pressChannel(channel) }
            onChannelReleased: function(channel) { console.releaseChannel(channel) }
            onInputBlocked: function(pressed) { root.showLockBar(pressed) }
        }
    }

    Connections {
        target: console
        function onLiveNote(channel, isOn) {
            if (isOn) noteField.noteOn(channel);
            else noteField.noteOff(channel);
        }
        function onToast(message) {
            root.showToast(message);
        }
        function onKeyboardLocked(pressed) {
            root.showLockBar(pressed);
        }
        function onFileStateChanged() {
            if (!console.fileLoaded) {
                root.lockHeld = false;
                root.lockBarOn = false;
            }
        }
    }

    // 打开文件
    FileDialog {
        id: openDialog
        title: "打开曲目"
        fileMode: FileDialog.OpenFile
        nameFilters: ["MIDI / OCF (*.mid *.midi *.ocf)", "所有文件 (*)"]
        onAccepted: root.openPath(root.localPath(selectedFile))
    }

    // 保存（按后缀写 OCF 或 MID）
    FileDialog {
        id: saveDialog
        title: "保存曲目"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "ocf"
        nameFilters: ["OCF 曲目 (*.ocf)", "MIDI 文件 (*.mid)"]
        onAccepted: {
            var path = root.localPath(selectedFile);
            if (!/\.(ocf|mid|midi)$/i.test(path)) {
                var filter = String(selectedNameFilter);
                path += filter.indexOf("MIDI") >= 0 ? ".mid" : ".ocf";
            }
            var err = console.saveCurrent(path);
            if (err.length > 0) {
                root.showToast(err);
                return;
            }
            if (root.pendingRecordAfterSave) {
                root.pendingRecordAfterSave = false;
                root.beginRecord();
            }
            if (root.pendingCloseAfterSave) {
                root.pendingCloseAfterSave = false;
                console.closeFile();
            }
        }
        onRejected: root.pendingRecordAfterSave = false
    }

    TransformDialog {
        id: transformDialog
        onAccepted: function(rangeStart, rangeEnd, wrap, windowMs) {
            shown = false;
            var err = console.loadMidiAsOcf(root.pendingMidiPath, rangeStart, rangeEnd, wrap, windowMs);
            if (err.length > 0) root.showToast(err);
        }
        onCanceled: shown = false
    }

    ConfirmDialog {
        id: unsavedDialog
        title: "有未保存的录制"
        message: "当前录制的曲目还没有保存。是否先保存，再开始新的录制？"
        confirmLabel: "先保存"
        cancelLabel: "直接新录"
        onConfirmed: {
            shown = false;
            root.pendingRecordAfterSave = true;
            saveDialog.selectedFile = "";
            saveDialog.open();
        }
        onCanceled: {
            shown = false;
            root.beginRecord();
        }
    }

    ConfirmDialog {
        id: closeDialog
        title: "有未保存的录制"
        message: "关闭文件前，是否先保存当前录制的曲目？"
        confirmLabel: "先保存"
        cancelLabel: "不保存"
        onConfirmed: {
            shown = false;
            root.pendingCloseAfterSave = true;
            saveDialog.selectedFile = "";
            saveDialog.open();
        }
        onCanceled: {
            shown = false;
            console.closeFile();
        }
    }

    // 键盘锁定提示：按下出现，按住保持，松开 1.5 秒后消失
    Timer {
        id: lockHideTimer
        interval: 1500
        repeat: false
        onTriggered: {
            if (!root.lockHeld) root.lockBarOn = false;
        }
    }

    Rectangle {
        id: lockBar
        z: 15
        enabled: root.lockBarOn
        width: lockRow.implicitWidth + 20
        height: 22
        x: 16
        // Column 铺满 contentItem，键盘与本条的 y 处于同一坐标系
        y: keyboard.y - height - 2
        visible: root.lockBarOn
        opacity: root.lockBarOn ? 1 : 0
        color: Qt.rgba(0.086, 0.102, 0.125, 0.94)
        border.color: theme.border
        border.width: 1

        Row {
            id: lockRow
            anchors.centerIn: parent
            spacing: 8

            Canvas {
                width: 11
                height: 14
                anchors.verticalCenter: parent.verticalCenter
                Component.onCompleted: requestPaint()
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = theme.accent;
                    ctx.fillStyle = theme.accent;
                    ctx.lineWidth = 1.4;
                    ctx.beginPath();
                    ctx.arc(width / 2, 5, 3, Math.PI, Math.PI * 2);
                    ctx.stroke();
                    ctx.fillRect(0.5, 5.5, width - 1, height - 6);
                }
            }

            Text {
                text: "播放模式 键盘锁定"
                color: theme.accent
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Toast
    Rectangle {
        id: toastBar
        visible: false
        z: 300
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 176
        width: Math.min(parent.width - 80, toastText.implicitWidth + 32)
        height: 34
        radius: 4
        color: theme.panel
        border.color: theme.border
        border.width: 1
        Text {
            id: toastText
            anchors.centerIn: parent
            color: theme.text
            font.pixelSize: 12
        }
        Timer {
            id: toastTimer
            interval: 2600
            onTriggered: toastBar.visible = false
        }
    }

    SettingsWindow {
        id: settingsWindow
    }

    // 边缘缩放
    component ResizeEdge: MouseArea {
        property int edges
        onPressed: root.startSystemResize(edges)
    }

    ResizeEdge {
        edges: Qt.LeftEdge
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        z: 20
    }
    ResizeEdge {
        edges: Qt.RightEdge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        z: 20
    }
    ResizeEdge {
        edges: Qt.TopEdge
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 5
        cursorShape: Qt.SizeVerCursor
        z: 20
    }
    ResizeEdge {
        edges: Qt.BottomEdge
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 5
        cursorShape: Qt.SizeVerCursor
        z: 20
    }
    ResizeEdge {
        edges: Qt.LeftEdge | Qt.TopEdge
        anchors.left: parent.left
        anchors.top: parent.top
        width: 8
        height: 8
        cursorShape: Qt.SizeFDiagCursor
        z: 21
    }
    ResizeEdge {
        edges: Qt.RightEdge | Qt.BottomEdge
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 8
        height: 8
        cursorShape: Qt.SizeFDiagCursor
        z: 21
    }
    ResizeEdge {
        edges: Qt.RightEdge | Qt.TopEdge
        anchors.right: parent.right
        anchors.top: parent.top
        width: 8
        height: 8
        cursorShape: Qt.SizeBDiagCursor
        z: 21
    }
    ResizeEdge {
        edges: Qt.LeftEdge | Qt.BottomEdge
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 8
        height: 8
        cursorShape: Qt.SizeBDiagCursor
        z: 21
    }
}

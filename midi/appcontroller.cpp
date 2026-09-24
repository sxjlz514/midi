#include "appcontroller.h"

#include "midiinput.h"
#include "seriallink.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_serial(new SerialLink(this))
    , m_midi(new MidiInput(this))
{
    // MIDI 事件 -> 本类处理
    connect(m_midi, &MidiInput::midiEvent,
            this, &AppController::handleMidiEvent);
    connect(m_midi, &MidiInput::logMessage,
            this, &AppController::logMessage);

    // 串口帧 / 日志 / 连接状态 -> 转发
    connect(m_serial, &SerialLink::frameReceived,
            this, &AppController::handleFrameReceived);
    connect(m_serial, &SerialLink::logMessage,
            this, &AppController::logMessage);
    connect(m_serial, &SerialLink::connectionChanged,
            this, [this](bool connected) {
                if (!connected) {
                    allOff(); // 断线时安全起见全部关闭
                }
            });

    pushState(); // 初始全关推送给界面
}

void AppController::connectSerial(const QString &portName)
{
    m_serial->open(portName);
}

void AppController::disconnectSerial()
{
    m_serial->close();
}

void AppController::setSimulationMode(bool on)
{
    m_serial->setSimulationMode(on);
}

bool AppController::isSimulationMode() const
{
    return m_serial->isSimulationMode();
}

void AppController::testValve(int valveIndex, bool on)
{
    m_state.setValve(valveIndex, on);
    pushState();
}

void AppController::allOff()
{
    m_state.allOff();
    pushState();
}

void AppController::setBaseNote(int baseNote)
{
    m_mapper.setBaseNote(baseNote);
    emit logMessage(tr("起始音符已设为 %1").arg(baseNote));
}

void AppController::handleMidiEvent(const MidiEvent &event)
{
    switch (event.type) {
    case MidiEventType::NoteOn: {
        const int valveIndex = m_mapper.noteToValve(event.note);
        if (valveIndex >= 0) {
            m_state.setValve(valveIndex, true);
            pushState();
        }
        break;
    }
    case MidiEventType::NoteOff: {
        const int valveIndex = m_mapper.noteToValve(event.note);
        if (valveIndex >= 0) {
            m_state.setValve(valveIndex, false);
            pushState();
        }
        break;
    }
    case MidiEventType::ControlChange:
        emit logMessage(tr("收到控制变化 CC%1 = %2")
                            .arg(event.controller).arg(event.value));
        break;
    case MidiEventType::Other:
        break;
    }
}

void AppController::handleFrameReceived(Command cmd, const QByteArray &payload)
{
    switch (cmd) {
    case Command::Pong:
        emit logMessage(tr("收到 PONG（主控在线）"));
        break;
    case Command::Status:
        emit logMessage(tr("收到状态上报：%1")
                            .arg(QString::fromLatin1(payload.toHex(' '))));
        break;
    default:
        emit logMessage(tr("收到帧，命令=0x%1")
                            .arg(toByte(cmd), 2, 16, QLatin1Char('0')));
        break;
    }
}

void AppController::pushState()
{
    const QByteArray bitmap = m_state.snapshot();

    emit valveStateChanged(bitmap); // 刷新界面

    if (m_serial->isOpen()) {       // 串口已连接才发送
        m_serial->send(Command::SetState, bitmap);
    }
}

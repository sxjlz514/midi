#include "midiinput.h"

#include <QMetaObject>

MidiInput::MidiInput(QObject *parent)
    : QObject(parent)
{
    // 让 MidiEvent 可被 Qt 的元对象系统识别（跨线程队列连接时需要）。
    qRegisterMetaType<MidiEvent>("MidiEvent");
}

MidiInput::~MidiInput()
{
    close();
}

QStringList MidiInput::devices() const
{
    QStringList result;

    const UINT count = midiInGetNumDevs();

    for (UINT index = 0; index < count; ++index) {
        MIDIINCAPSW capabilities{};

        const MMRESULT mmResult = midiInGetDevCapsW(
            index, &capabilities, sizeof(capabilities));

        if (mmResult == MMSYSERR_NOERROR) {
            result.append(QString::fromWCharArray(capabilities.szPname));
        } else {
            result.append(QStringLiteral("未知设备 %1").arg(index));
        }
    }

    return result;
}

bool MidiInput::open(unsigned int deviceIndex, QString *errorMessage)
{
    close();

    MMRESULT result = midiInOpen(
        &m_handle,
        deviceIndex,
        reinterpret_cast<DWORD_PTR>(&MidiInput::midiCallback),
        reinterpret_cast<DWORD_PTR>(this),
        CALLBACK_FUNCTION);

    if (result != MMSYSERR_NOERROR) {
        if (errorMessage) {
            *errorMessage = midiErrorText(result);
        }
        m_handle = nullptr;
        return false;
    }

    result = midiInStart(m_handle);

    if (result != MMSYSERR_NOERROR) {
        if (errorMessage) {
            *errorMessage = midiErrorText(result);
        }
        midiInClose(m_handle);
        m_handle = nullptr;
        return false;
    }

    return true;
}

void MidiInput::close()
{
    if (!m_handle) {
        return;
    }

    midiInStop(m_handle);
    midiInReset(m_handle);
    midiInClose(m_handle);

    m_handle = nullptr;
}

bool MidiInput::isOpen() const
{
    return m_handle != nullptr;
}

void CALLBACK MidiInput::midiCallback(
    HMIDIIN,
    UINT message,
    DWORD_PTR instance,
    DWORD_PTR parameter1,
    DWORD_PTR parameter2)
{
    if (message != MIM_DATA && message != MIM_MOREDATA) {
        return;
    }

    auto *self = reinterpret_cast<MidiInput *>(instance);
    if (!self) {
        return;
    }

    // WinMM 把一条短 MIDI 消息压缩在一个 DWORD 中：
    //   Bits 0~7   : Status
    //   Bits 8~15  : Data 1
    //   Bits 16~23 : Data 2
    const quint32 packedMessage = static_cast<quint32>(parameter1);

    const quint8 status = packedMessage & 0xFF;
    const quint8 data1 = (packedMessage >> 8) & 0xFF;
    const quint8 data2 = (packedMessage >> 16) & 0xFF;
    const quint32 timestampMs = static_cast<quint32>(parameter2);

    // 回调不在 Qt GUI 线程：投递回 Qt 线程后再发信号。
    QMetaObject::invokeMethod(
        self,
        [self, status, data1, data2, timestampMs]() {
            if (!self->isOpen()) {
                return;
            }
            emit self->rawMessageReceived(status, data1, data2, timestampMs);
            emit self->midiEvent(parseMessage(status, data1, data2, timestampMs));
        },
        Qt::QueuedConnection);
}

MidiEvent MidiInput::parseMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    MidiEvent event;
    event.status = status;
    event.channel = (status & 0x0F) + 1; // MIDI 通道 1~16
    event.timestampMs = timestampMs;

    switch (status & 0xF0) {
    case 0x90: // Note On（力度为 0 时等价于 Note Off）
        if (data2 == 0) {
            event.type = MidiEventType::NoteOff;
            event.note = data1;
            event.velocity = 0;
        } else {
            event.type = MidiEventType::NoteOn;
            event.note = data1;
            event.velocity = data2;
        }
        break;

    case 0x80: // Note Off
        event.type = MidiEventType::NoteOff;
        event.note = data1;
        event.velocity = data2;
        break;

    case 0xB0: // Control Change
        event.type = MidiEventType::ControlChange;
        event.controller = data1;
        event.value = data2;
        break;

    default:
        event.type = MidiEventType::Other;
        break;
    }

    return event;
}

QString MidiInput::midiErrorText(MMRESULT result)
{
    wchar_t buffer[MAXERRORLENGTH]{};

    if (midiInGetErrorTextW(result, buffer, MAXERRORLENGTH) == MMSYSERR_NOERROR) {
        return QString::fromWCharArray(buffer);
    }

    return QStringLiteral("MIDI错误代码：%1").arg(result);
}

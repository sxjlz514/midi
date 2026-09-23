#include "midimessageformatter.h"

QString MidiMessageFormatter::noteName(int midiNote)
{
    static const char *names[] = {
        "C", "C#", "D", "D#",
        "E", "F", "F#", "G",
        "G#", "A", "A#", "B"
    };

    if (midiNote < 0 || midiNote > 127) {
        return QStringLiteral("Invalid");
    }

    const int octave =
        midiNote / 12 - 1;

    const int noteIndex =
        midiNote % 12;

    return QStringLiteral("%1%2 (MIDI %3)")
        .arg(
            QString::fromLatin1(
                names[noteIndex]
                )
            )
        .arg(octave)
        .arg(midiNote);
}

QString MidiMessageFormatter::describe(
    quint8 status,
    quint8 data1,
    quint8 data2)
{
    const quint8 command =
        status & 0xF0;

    QString description;

    switch (command) {
    case 0x80:
        description = QStringLiteral(
                          "Note Off  %1  Velocity=%2"
                          )
                          .arg(noteName(data1))
                          .arg(data2);
        break;

    case 0x90:
        if (data2 == 0) {
            description = QStringLiteral(
                              "Note Off  %1  Velocity=0"
                              ).arg(noteName(data1));
        } else {
            description = QStringLiteral(
                              "Note On   %1  Velocity=%2"
                              )
                              .arg(noteName(data1))
                              .arg(data2);
        }
        break;

    case 0xA0:
        description = QStringLiteral(
                          "Poly Aftertouch  Note=%1 Value=%2"
                          )
                          .arg(noteName(data1))
                          .arg(data2);
        break;

    case 0xB0:
        description = QStringLiteral(
                          "Control Change  CC=%1 Value=%2"
                          )
                          .arg(data1)
                          .arg(data2);
        break;

    case 0xC0:
        description = QStringLiteral(
                          "Program Change  Program=%1"
                          ).arg(data1);
        break;

    case 0xD0:
        description = QStringLiteral(
                          "Channel Pressure  Value=%1"
                          ).arg(data1);
        break;

    case 0xE0: {
        const int rawValue =
            (static_cast<int>(data2) << 7) |
            data1;

        const int signedValue =
            rawValue - 8192;

        description = QStringLiteral(
                          "Pitch Bend  Value=%1"
                          ).arg(signedValue);
        break;
    }

    default:
        description = QStringLiteral(
            "System/Unknown Message"
            );
        break;
    }

    return description;
}

QString MidiMessageFormatter::formatLine(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    const int channel =
        (status & 0x0F) + 1;

    const QString description =
        describe(status, data1, data2);

    const QString line = QStringLiteral(
                             "[%1 ms] CH=%2  %3  RAW=%4 %5 %6"
                             )
                             .arg(timestampMs, 8)
                             .arg(channel, 2)
                             .arg(description)
                             .arg(status, 2, 16, QChar('0'))
                             .arg(data1, 2, 16, QChar('0'))
                             .arg(data2, 2, 16, QChar('0'))
                             .toUpper();

    return line;
}

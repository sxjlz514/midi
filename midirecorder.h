#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct MidiRecordEvent
{
    quint32 timestampMs = 0;
    quint8 status = 0;
    quint8 data1 = 0;
    quint8 data2 = 0;
};

class MidiRecorder : public QObject
{
    Q_OBJECT

public:
    explicit MidiRecorder(QObject *parent = nullptr);
    ~MidiRecorder() override;

    void start();

    void stop();

    bool isRecording() const;

    int eventCount() const;

    void clear();

    bool save(
        const QString &filePath,
        QString *errorMessage = nullptr
        );

public slots:
    void appendMessage(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private:
    static bool isNoteEvent(
        quint8 status,
        quint8 data1,
        quint8 data2
        );

    static void writeUInt32BE(
        QByteArray &out,
        quint32 value
        );

    static void writeVarLen(
        QByteArray &out,
        quint32 value
        );

    static quint32 msToTicks(quint32 timestampMs);

private:
    bool m_recording = false;

    QVector<MidiRecordEvent> m_events;
};

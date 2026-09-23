#pragma once

#include <QObject>
#include <QStringList>
#include <QtGlobal>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmsystem.h>

class MidiInput : public QObject
{
    Q_OBJECT

public:
    explicit MidiInput(QObject *parent = nullptr);
    ~MidiInput() override;

    QStringList devices() const;

    bool open(
        unsigned int deviceIndex,
        QString *errorMessage = nullptr
        );

    void close();

    bool isOpen() const;

signals:
    void messageReceived(
        quint8 status,
        quint8 data1,
        quint8 data2,
        quint32 timestampMs
        );

private:
    static void CALLBACK midiCallback(
        HMIDIIN midiHandle,
        UINT message,
        DWORD_PTR instance,
        DWORD_PTR parameter1,
        DWORD_PTR parameter2
        );

    static QString midiErrorText(MMRESULT result);

private:
    HMIDIIN m_handle = nullptr;
};

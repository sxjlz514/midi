/**
 * @file midirecorder.h
 * @brief 记录演奏为标准 MIDI：同一键消抖，不同键在短窗内对齐为同时。
 */

#ifndef MIDIRECORDER_H
#define MIDIRECORDER_H

#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

class PerformanceRecorder
{
public:
    void setWindowMs(int ms);
    void setDebounceMs(int ms);
    int windowMs() const { return m_windowMs; }
    int debounceMs() const { return m_debounceMs; }

    void start();
    bool isRecording() const { return m_recording; }

    /** 记入一个音高边沿。未到消抖时间的反向抖动会被丢掉。 */
    void note(int midiNote, bool on, int velocity);

    /** 把仍按住的音记为松开，便于结束录制。 */
    void releaseOpenNotes();

    bool saveSmf(const QString &path, QString &error);
    void clear();

private:
    struct NoteState {
        bool on = false;
        bool hasPending = false;
        bool pendingOn = false;
        int pendingVelocity = 0;
        qint64 pendingUs = 0;
    };
    struct Event {
        qint64 timeMs = 0;
        int note = 0;
        bool on = false;
        int velocity = 0;
        int order = 0;
    };

    void flushPending(qint64 nowUs);
    void commit(int midiNote, bool on, int velocity, qint64 timeUs);
    qint64 quantizeUs(qint64 timeUs) const;

    bool m_recording = false;
    int m_windowMs = 10;
    int m_debounceMs = 5;
    qint64 m_originNs = 0;
    int m_order = 0;
    QHash<int, NoteState> m_notes;
    QVector<Event> m_events;
};

#endif // MIDIRECORDER_H

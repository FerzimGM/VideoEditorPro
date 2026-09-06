#ifndef AUDIOPLAYBACKENGINE_H
#define AUDIOPLAYBACKENGINE_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDateTime>

class Timeline;

// Push-mode audio playback engine built on QAudioSink. A timer periodically
// pulls a mixed audio chunk from the Timeline and writes it into the sink's
// buffer, keeping the buffer topped up just ahead of what the hardware is
// currently playing.
class AudioPlaybackEngine : public QObject
{
    Q_OBJECT
public:
    static const int SAMPLE_RATE = 44100;
    static const int CHANNELS = 2;
    static const int FEED_INTERVAL_MS = 20;   // 20ms ticks =~ 50 feeds/sec
    static const int BUFFER_MS = 500;         // Sized generously to avoid underruns

    explicit AudioPlaybackEngine(Timeline* timeline, QObject* parent = nullptr);
    ~AudioPlaybackEngine();

    void startPlayback(double fromTime, double speed = 1.0,
                       double totalDuration = 1e9);
    void stopPlayback();
    void setVolume(float vol);
    void setTrackMuted(int track, bool muted);

    double getCurrentAudioTime() const;
    double getWriteHead() const { return m_writeHead; }
    bool   isPlaying() const { return m_playing; }

signals:
    void timeUpdated(double time);
    void playbackEnded();

private slots:
    void onFeedTimer();

private:
    Timeline* m_timeline;
    QAudioSink* m_sink = nullptr;
    QIODevice* m_sinkDevice = nullptr;   // Push-mode write target
    QTimer* m_feedTimer;

    double m_startStreamTime = 0.0;
    double m_playStartTime = 0.0;
    qint64 m_playStartMs = 0;
    double m_speed = 1.0;
    double m_writeHead = 0.0;
    bool m_playing = false;
    bool m_track1Muted = false;
    bool m_track2Muted = false;
    int m_silentChunks = 0;
    double m_totalDuration = 1e9;

    void createSink();
    void destroySink();
};

#endif // AUDIOPLAYBACKENGINE_H


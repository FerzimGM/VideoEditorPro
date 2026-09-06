#ifndef DECODERTHREAD_H
#define DECODERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QDateTime>
#include "FrameCache.h"
#include "mediadecoder.h"

// DecoderThread runs decoding in the background so the UI thread never
// blocks on FFmpeg calls. It decodes frames ahead of the playback position
// and stores them in a FrameCache; one thread is spawned per distinct
// (file, track) pair.
class DecoderThread : public QThread
{
    Q_OBJECT

public:
    DecoderThread(const QString& filepath, double fps, FrameCache* cache, QObject* parent = nullptr)
        : QThread(parent)
        , m_filepath(filepath)
        , m_fps(fps > 0 ? fps : 25.0)
        , m_cache(cache)
        , m_running(false)
        , m_seekRequested(false)
        , m_seekTime(0.0)
    {}

    // Requests a seek to a new position. Safe to call from the UI thread.
    void seekTo(double time)
    {
        QMutexLocker lock(&m_mutex);
        m_seekTime      = time;
        // Reset the tracked play position along with the seek target.
        // Without this, a forward seek followed by a backward seek would
        // leave m_playPosition at its old (larger) value, so the decode
        // loop's "stay within PREFETCH_AHEAD" check would never trigger —
        // it would keep decoding forward and fill the cache with frames
        // from the middle of the file, evicting the frames actually
        // needed and producing a black screen.
        m_playPosition  = time;
        m_seekRequested = true;
        m_condition.wakeAll();
    }

    void updatePlayPosition(double time)
    {
        QMutexLocker lock(&m_mutex);
        // Trigger a real seek on large jumps (e.g. switching between two
        // clips from the same source file on the timeline). Previously
        // this only advanced m_playPosition and let the thread decode
        // sequentially from the old position to the new one; for two
        // clips sharing a source file on track 2 (e.g. source time
        // 25s -> 35s) that meant decoding through the entire 10s gap,
        // causing cache misses, a frozen video frame while audio kept
        // playing, and audio/video desync. A jump larger than 2s now
        // clears the cache and reseeks the decoder directly to the
        // target position.
        if (time > m_playPosition + 2.0)
        {
            m_seekTime      = time;
            m_playPosition  = time;
            m_seekRequested = true;
        }
        else if (time > m_playPosition)
        {
            m_playPosition = time;
        }
        m_condition.wakeAll();
    }

    // Stops the thread and blocks until it exits (up to 2s).
    void stop()
    {
        {
            QMutexLocker lock(&m_mutex);
            m_running = false;
        }
        m_condition.wakeAll();
        wait(2000);
    }

    // Needed by Timeline::getCurrentFrameAt to compute the frame number.
    double getFps() const { return m_fps; }

signals:
    // Emitted when a newly decoded frame is available in the cache.
    void frameReady(int frameNumber);

protected:
    void run() override {
        MediaDecoder decoder;
        // Preview mode: frames are decoded at half resolution (a quarter
        // of the pixel count). DecoderThread is only used for live
        // playback — file export uses its own decoders without this flag.
        decoder.setPreviewMode(true);
        if (!decoder.openFile(m_filepath))
        {
            return;
        }
#ifndef QT_NO_DEBUG
        if (decoder.isUsingGPU())
            qDebug() << "DecoderThread: GPU decode active for" << m_filepath;
        else
            qDebug() << "DecoderThread: CPU decode for" << m_filepath;
#endif

        // Set m_running=true before entering the loop and initialize
        // m_seekTime to match the starting currentTime.
        {
            QMutexLocker lock(&m_mutex);
            m_running  = true;
            m_seekTime = 0.0;
        }

        double currentTime = 0.0;
        double prefetchBase = 0.0;

        // Prefetch window, reduced from 8.0s to 2.5s. At 30fps, 8 seconds
        // of buffer meant decoding 240 frames ahead per thread, which
        // amplified both the signal-storm issue (rate-limited emits) and
        // memory pressure. 2.5 seconds is enough headroom for smooth
        // playback even at 2x speed.
        const double PREFETCH_AHEAD = 2.5;

        bool needSeek = true;  // Perform one seek to the start on launch.

        while (true) {
            // Check for a pending seek request.
            {
                QMutexLocker lock(&m_mutex);
                if (!m_running) break;

                if (m_seekRequested)
                {
                    currentTime = m_seekTime;
                    prefetchBase = m_seekTime;
                    m_seekRequested = false;
                    // Clear the cache on every real seek (scrubbing, clip
                    // change). Otherwise stale frames from the previous
                    // position remain cached and getNearest() can return
                    // one of them, causing a visible flicker. Redundant
                    // seeks on pause->play are avoided upstream in
                    // startPlayback(), which checks cache->contains()
                    // before calling seekTo().
                    m_cache->clear();
                    m_cache->setPlayPosition((int)(m_seekTime * m_fps + 0.5));
                    needSeek = true;
                }
            }

            // Seek only on a position change; subsequent frames use
            // getNextFrame() for sequential decoding.
            if (needSeek)
            {
                // seekAndDecode() seeks and skips frames up to the target
                // PTS without running sws_scale on the intermediate
                // frames (about 5x faster than getFrameAt()). After this
                // call the decoder is positioned correctly, so the
                // following getNextFrame() returns the next frame with
                // the right PTS.
                QImage seekFrame = decoder.seekAndDecode(currentTime);
                if (!seekFrame.isNull())
                {
                    int seekFrameNum = (int)(currentTime * m_fps + 0.5);
                    m_cache->put(seekFrameNum, seekFrame);
                    emitFrameReady(seekFrameNum);
                }
                needSeek = false;
            }

            int frameNum = (int)(currentTime * m_fps + 0.5);

            // Decode the next frame.
            QImage cached;
            if (!m_cache->get(frameNum, cached))
            {
                // getNextFrame() reads the next packet sequentially.
                QImage frame = decoder.getNextFrame();
                if (!frame.isNull())
                {
                    m_cache->put(frameNum, frame);
                    emitFrameReady(frameNum);
                    // Report the current play position to the cache so
                    // eviction doesn't drop frames still needed for
                    // playback.
                    {
                        QMutexLocker lock(&m_mutex);
                        m_cache->setPlayPosition((int)(m_playPosition * m_fps + 0.5));
                    }
                } else
                {
                    // End of file or a read error — wait for the next seek.
                    QMutexLocker lock(&m_mutex);
                    m_condition.wait(&m_mutex, 100);
                    continue;
                }
            }

            currentTime += 1.0 / m_fps;

            // Throttle if we've decoded too far ahead of playback.
            {
                QMutexLocker lock(&m_mutex);
                if (!m_running) break;

                if (m_playPosition > prefetchBase)
                    prefetchBase = m_playPosition;

                double aheadOf = currentTime - prefetchBase;

                if (!m_seekRequested && aheadOf > PREFETCH_AHEAD)
                {
                    m_condition.wait(&m_mutex, 30);
                }
            }
        }
        decoder.closeFile();
    }

private:
    // Rate-limits frameReady emissions. DecoderThread can decode at
    // 200+ fps; without a limit that's 200 signals/sec x up to 5 threads
    // queued on the UI event loop, which backs up and causes visible lag.
    // Pausing drains the queue, so this only bites during active
    // decoding. A 40ms limit (~25fps) is still fast enough for smooth
    // preview updates while paused, without overloading the queue during
    // playback.
    void emitFrameReady(int frameNum)
    {
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastEmitMs >= 40)
        {
            m_lastEmitMs = nowMs;
            emit frameReady(frameNum);
        }
    }

    QString m_filepath;
    double m_fps;
    FrameCache* m_cache;

    bool m_running;
    bool m_seekRequested;
    double m_seekTime;
    double m_playPosition = 0.0;
    qint64 m_lastEmitMs = 0;
    QMutex m_mutex;
    QWaitCondition m_condition;
};

#endif // DECODERTHREAD_H

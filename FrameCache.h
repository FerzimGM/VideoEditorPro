#ifndef FRAMECACHE_H
#define FRAMECACHE_H

// FrameCache — a thread-safe sliding-window cache of decoded video frames.
//
// Eviction policy:
//   Frames are only evicted from behind the current playback position
//   (older than playPos - KEEP_BEHIND). Frames ahead of playback — the
//   prefetch buffer — are never touched while eviction candidates exist
//   behind the play head.
//
// Capacity:
//   MAX_FRAMES = 150, sized to hold PREFETCH_AHEAD (2.5s x 30fps = ~75
//   frames ahead) plus KEEP_BEHIND (30 frames behind) plus headroom for
//   decoder stalls/bursts.
//   Memory footprint: 150 x 1920x1080 x 3 bytes =~ 935 MB per track;
//   with two concurrent tracks, ~1.9 GB, which is acceptable on a
//   32 GB machine.
//
// Design note:
//   An earlier eviction rule fell back to erasing the most recently
//   decoded frame whenever no frame existed far enough behind the play
//   position. Because that frame was still needed for imminent playback,
//   the decoder thread would immediately re-decode and re-insert it,
//   producing a repeating stall roughly every 20 seconds. The current
//   rule only evicts the furthest-ahead frame once it exceeds
//   playPos + KEEP_BEHIND, so the decoder thread naturally stops
//   prefetching before that point and the frame is not re-requested.

#include <QMap>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

struct FrameCache {
    static const int MAX_FRAMES  = 150;
    static const int KEEP_BEHIND = 30;

    void put(int frameNumber, const QImage& image)
    {
        QMutexLocker lock(&m_mutex);
        m_frames[frameNumber] = image;

        while (m_frames.size() > MAX_FRAMES)
        {
            auto first = m_frames.begin();
            if (first.key() < m_playPosition - KEEP_BEHIND)
            {
                // Old enough behind the play head — safe to drop.
                m_frames.erase(first);
            }
            else
            {
                // No eviction candidate behind the play head: prefetch has
                // run too far ahead. Drop the furthest-ahead frame instead;
                // the decoder thread's PREFETCH_AHEAD limit stops it from
                // immediately re-decoding the same frame.
                auto last = m_frames.end(); --last;
                if (last.key() > m_playPosition + KEEP_BEHIND)
                    m_frames.erase(last);
                else
                    break; // Every cached frame is within the working set.
            }
        }
    }

    bool get(int frameNumber, QImage& out)
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_frames.find(frameNumber);
        if (it != m_frames.end()) { out = it.value(); return true; }
        return false;
    }

    // Returns the cached frame closest to frameNumber within maxDistance.
    // Default maxDistance = 2 (+-67ms at 30fps). A larger tolerance (e.g. 5)
    // was tried previously and produced visible judder, since it could
    // return a frame slightly ahead of the intended position whenever the
    // decoder was a little ahead of playback.
    bool getNearest(int frameNumber, QImage& out, int maxDistance = 2)
    {
        QMutexLocker lock(&m_mutex);
        if (m_frames.isEmpty()) return false;

        auto it = m_frames.find(frameNumber);
        if (it != m_frames.end()) { out = it.value(); return true; }

        auto upper = m_frames.lowerBound(frameNumber);
        int bestDist = INT_MAX;
        QMap<int,QImage>::iterator best = m_frames.end();

        if (upper != m_frames.end()) {
            int d = upper.key() - frameNumber;
            if (d <= maxDistance && d < bestDist) { bestDist = d; best = upper; }
        }
        if (upper != m_frames.begin()) {
            auto lower = upper; --lower;
            int d = frameNumber - lower.key();
            if (d <= maxDistance && d < bestDist) { bestDist = d; best = lower; }
        }
        if (best != m_frames.end()) { out = best.value(); return true; }
        return false;
    }

    void setPlayPosition(int frameNum)
    {
        QMutexLocker lock(&m_mutex);
        m_playPosition = frameNum;
    }

    void clear()
    {
        QMutexLocker lock(&m_mutex);
        m_frames.clear();
        m_playPosition = 0;
    }

    int size()            { QMutexLocker l(&m_mutex); return m_frames.size(); }
    bool contains(int n)  { QMutexLocker l(&m_mutex); return m_frames.contains(n); }

private:
    QMap<int, QImage> m_frames;
    int m_playPosition = 0;
    QMutex m_mutex;
};

#endif // FRAMECACHE_H

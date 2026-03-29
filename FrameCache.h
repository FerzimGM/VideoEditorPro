#ifndef FRAMECACHE_H
#define FRAMECACHE_H

// FrameCache — скользящее окно кэша кадров.
//
// ПРАВИЛО ВЫТЕСНЕНИЯ:
//   Удаляем только кадры ПОЗАДИ (playPos - KEEP_BEHIND).
//   Кадры впереди (prefetch-буфер) — не трогаем никогда.
//
// РАЗМЕР:
//   MAX_FRAMES = 150: PREFETCH_AHEAD=2.5с × 30fps = 75 кадров вперёд
//                   + KEEP_BEHIND = 30 кадров позади
//                   + 45 кадров запаса при рывках декодера.
//   Память: 150 × 1920×1080 × 3б ≈ 935 МБ на поток.
//   При 2 потоках ≈ 1.9 ГБ — приемлемо для 32 ГБ RAM.
//
// СТАРАЯ ОШИБКА (причина циклических зависаний каждые ~20с):
//   if (first.key() < m_playPosition - 10) erase(first)
//   else erase(last)   ← удалял только что задекодированный кадр ВПЕРЕДИ
//   → DecoderThread перепрефетчивал его → снова удалялся → бесконечный цикл.

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
                // Кадр достаточно далеко позади — удаляем
                m_frames.erase(first);
            }
            else
            {
                // Нет старых кадров позади.
                // Prefetch ушёл слишком далеко вперёд — удаляем самый дальний.
                // DecoderThread остановится через PREFETCH_AHEAD и не будет
                // перепрефетчировать его (в отличие от старого кода).
                auto last = m_frames.end(); --last;
                if (last.key() > m_playPosition + KEEP_BEHIND)
                    m_frames.erase(last);
                else
                    break; // все кадры в нужной зоне — выходим
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

    // maxDistance по умолчанию = 2 (±67мс при 30fps).
    // Старое значение 5 (±167мс) давало визуальное дёргание:
    // getNearest возвращал кадр из будущего когда декодер чуть опережал.
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

#ifndef FRAMECACHE_H
#define FRAMECACHE_H

// FrameCache — скользящее окно кэша кадров.
//
// ПРЕЖНЯЯ ОШИБКА (причина зависаний каждые ~3с):
//   Старый код при переполнении проверял first.key() < m_playPosition - 10.
//   Если кадры позади не попадали в этот диапазон — удалял последний кадр ВПЕРЕДИ.
//   DecoderThread тут же его перепрефетчивал → снова удалялся → бесконечный цикл.
//   Кадры нужные для воспроизведения постоянно вытеснялись → зависание.
//
// ПРАВИЛЬНАЯ СТРАТЕГИЯ:
//   Удалять только кадры ПОЗАДИ playPos (они уже воспроизведены).
//   Кадры впереди — prefetch-буфер — не трогать никогда.
//   m_playPosition обновляется из getCurrentFrameAt при каждом тике → всегда актуален.

#include <QMap>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

struct FrameCache {
    // 120 кадров = 4с при 30fps.
    // PREFETCH_AHEAD = 2.5с = ~75 кадров + 45 кадров хвоста позади.
    // Память: 120 × 1920×1080 × 3б ≈ 750 МБ на поток.
    // При 2 потоках ≈ 1.5 ГБ — нормально для современного ПК.
    // Старое значение 90 было на грани: prefetch 75 кадров + хвост 15 = ровно 90,
    // при любом рассинхроне кадры впереди начинали вытесняться → зависания.
    static const int MAX_FRAMES  = 120;
    static const int KEEP_BEHIND = 20; // кадров позади которые не трогаем

    void put(int frameNumber, const QImage& image)
    {
        QMutexLocker lock(&m_mutex);
        m_frames[frameNumber] = image;

        // Вытесняем только кадры ПОЗАДИ playPos.
        // Цикл while: если позади несколько старых кадров — удаляем все лишние за раз.
        while (m_frames.size() > MAX_FRAMES)
        {
            auto first = m_frames.begin();
            if (first.key() < m_playPosition - KEEP_BEHIND)
            {
                // Кадр достаточно далеко позади — безопасно удалить
                m_frames.erase(first);
            }
            else
            {
                // Нет кадров позади для вытеснения.
                // Это значит prefetch ушёл слишком далеко вперёд (>MAX_FRAMES кадров).
                // Удаляем самый дальний кадр ВПЕРЕДИ — DecoderThread сам притормозит
                // через условие aheadOf > PREFETCH_AHEAD и переспрефетчировать не будет.
                auto last = m_frames.end(); --last;
                if (last.key() > m_playPosition + KEEP_BEHIND)
                    m_frames.erase(last);
                else
                    break; // все кадры в нужной зоне — не трогаем
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

    bool getNearest(int frameNumber, QImage& out, int maxDistance = 5)
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

    int size()   { QMutexLocker lock(&m_mutex); return m_frames.size(); }
    bool contains(int n) { QMutexLocker lock(&m_mutex); return m_frames.contains(n); }

private:
    QMap<int, QImage> m_frames;
    int m_playPosition = 0;
    QMutex m_mutex;
};

#endif // FRAMECACHE_H

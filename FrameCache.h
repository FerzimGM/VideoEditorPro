#ifndef FRAMECACHE_H
#define FRAMECACHE_H

// FrameCache — скользящее окно кэша кадров.
// Хранит кадры в окне [playPos - BEHIND .. playPos + AHEAD].
// При переполнении удаляет кадр ДАЛЬШЕ ВСЕГО позади текущей позиции.
// Это гарантирует что текущие и будущие кадры не вытесняются старыми.

#include <QMap>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

struct FrameCache {
    // 400 кадров при 30fps = ~13с буфера
    static const int MAX_FRAMES = 400;

    void put(int frameNumber, const QImage& image) {
        QMutexLocker lock(&m_mutex);
        m_frames[frameNumber] = image;

        if (m_frames.size() > MAX_FRAMES) {
            // Удаляем кадр дальше всего позади текущей позиции.
            // begin() = минимальный frameNum = самый старый кадр в прошлом.
            // Никогда не удаляем кадры ВПЕРЕДИ позиции — они нужны для воспроизведения.
            auto first = m_frames.begin();
            if (first.key() < m_playPosition - 10) {
                // Кадр позади с запасом — можно удалять
                m_frames.erase(first);
            } else {
                // Все кадры рядом с текущей позицией — удаляем самый дальний впереди
                // (DecoderThread перепрефетчил слишком далеко)
                auto last = m_frames.end();
                --last;
                m_frames.erase(last);
            }
        }
    }

    bool get(int frameNumber, QImage& out) {
        QMutexLocker lock(&m_mutex);
        auto it = m_frames.find(frameNumber);
        if (it != m_frames.end()) {
            out = it.value();
            return true;
        }
        return false;
    }

    // Ищет ближайший кадр в радиусе maxDistance.
    // При скруббинге и cache miss во время воспроизведения показываем соседний кадр.
    bool getNearest(int frameNumber, QImage& out, int maxDistance = 5) {
        QMutexLocker lock(&m_mutex);
        if (m_frames.isEmpty()) return false;

        // Сначала точное совпадение
        auto it = m_frames.find(frameNumber);
        if (it != m_frames.end()) { out = it.value(); return true; }

        // Ищем ближайший в обе стороны
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

    // Сообщаем текущую позицию воспроизведения (в frameNum).
    // Используется при вытеснении — не удаляем нужные кадры.
    void setPlayPosition(int frameNum) {
        QMutexLocker lock(&m_mutex);
        m_playPosition = frameNum;
    }

    void clear() {
        QMutexLocker lock(&m_mutex);
        m_frames.clear();
        m_playPosition = 0;
    }

    int size() {
        QMutexLocker lock(&m_mutex);
        return m_frames.size();
    }

    bool contains(int frameNumber) {
        QMutexLocker lock(&m_mutex);
        return m_frames.contains(frameNumber);
    }

private:
    QMap<int, QImage> m_frames;
    int               m_playPosition = 0;
    QMutex            m_mutex;
};

#endif // FRAMECACHE_H

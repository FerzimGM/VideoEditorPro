#ifndef FRAMECACHE_H
#define FRAMECACHE_H

// FrameCache — хранит сырые RGB кадры без эффектов
// Структура (header-only), один экземпляр на каждый уникальный filepath
//
// АРХИТЕКТУРА:
//   VideoFile → MediaDecoder → FrameCache (сырые кадры, без эффектов)
//                                    ↓
//                              applyEffects(frame, clip.effects)  ← на лету
//                                    ↓
//                              VideoPlayer (отображение)
//
// Почему сырые кадры: при удалении/смене эффекта берём кадр из кэша
// и применяем новый список эффектов — кэш чистить не нужно.

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

struct FrameCache {
    // Сколько кадров держим в памяти на одно видео
    // 150 кадров × ~2MB (720p RGB) = ~300MB макс
    // Можно уменьшить до 60 если мало RAM
    static const int MAX_FRAMES = 150;

    // Сохранить кадр (ключ = номер кадра = time * fps)
    void put(int frameNumber, const QImage& image) {
        QMutexLocker lock(&m_mutex);
        // Вытесняем старые кадры если кэш заполнен
        if (m_frames.size() >= MAX_FRAMES) {
            // Удаляем самый ранний кадр
           m_frames.erase(m_frames.begin()); // вытесняем самый старый
        }
        m_frames[frameNumber] = image;
    }

    // Получить кадр. Возвращает true если нашли в кэше.
    bool get(int frameNumber, QImage& out) {
        QMutexLocker lock(&m_mutex);
        auto it = m_frames.find(frameNumber);
        if (it != m_frames.end()) {
            out = it.value();
            return true;
        }
        return false;
    }

    // Получить ближайший кадр (±maxDistance).
    // При скруббинге показываем соседний вместо чёрного экрана,
    // пока нужный кадр ещё не декодирован фоновым потоком.
    bool getNearest(int frameNumber, QImage& out, int maxDistance = 3) {
        QMutexLocker lock(&m_mutex);
        for (int d = 0; d <= maxDistance; d++) {
            auto it = m_frames.find(frameNumber + d);
            if (it != m_frames.end()) { out = it.value(); return true; }
            it = m_frames.find(frameNumber - d);
            if (it != m_frames.end()) { out = it.value(); return true; }
        }
        return false;
    }

    void clear() {
        QMutexLocker lock(&m_mutex);
        m_frames.clear();
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
    QHash<int, QImage> m_frames;
    QMutex m_mutex;// mutable — можно локировать в const методах
};

#endif // FRAMECACHE_H

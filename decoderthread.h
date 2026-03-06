#ifndef DECODERTHREAD_H
#define DECODERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include "FrameCache.h"
#include "mediadecoder.h"

// DecoderThread — фоновый поток декодирования.
// Декодирует кадры заранее и кладёт в FrameCache. Не блокирует UI.
// Один поток на каждый уникальный видеофайл.
class DecoderThread : public QThread {
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

    // Попросить перемотаться к новому времени
    // Вызывается из UI-потока (потокобезопасно)
    void seekTo(double time) {
        QMutexLocker lock(&m_mutex);
        m_seekTime = time;
        m_seekRequested = true;
        m_condition.wakeAll();  // Будим поток если он спал
    }

    void updatePlayPosition(double time) {
        QMutexLocker lock(&m_mutex);
        if (time > m_playPosition) m_playPosition = time;
        m_condition.wakeAll();
    }

    // Остановить поток (блокирует до завершения, макс 2с)
    void stop() {
        {
            QMutexLocker lock(&m_mutex);
            m_running = false;
        }
        m_condition.wakeAll();
        wait(2000);
    }

    // Нужен Timeline::getCurrentFrameAt для вычисления frameNum
    double getFps() const { return m_fps; }

signals:
    // Испускается когда новый кадр готов в кэше
    void frameReady(int frameNumber);

protected:
    void run() override {
        MediaDecoder decoder;
        if (!decoder.openFile(m_filepath)) {
            return;
        }

        // ИСПРАВЛЕНИЕ 1: ставим m_running=true ДО входа в цикл
        // и синхронизируем m_seekTime с начальным currentTime
        {
            QMutexLocker lock(&m_mutex);
            m_running  = true;
            m_seekTime = 0.0;  // явная инициализация
        }

        double currentTime  = 0.0;
        double prefetchBase = 0.0;
        const double PREFETCH_AHEAD = 5.0;
        bool needSeek = true;  // при старте делаем один seek на начало

        while (true) {
            // ── Проверяем запрос перемотки ────────────────────────────
            {
                QMutexLocker lock(&m_mutex);
                if (!m_running) break;

                if (m_seekRequested) {
                    currentTime     = m_seekTime;
                    prefetchBase    = m_seekTime;
                    m_seekRequested = false;
                    m_cache->clear();
                    needSeek = true;  // после seek нужно позиционировать декодер
                }
            }

            // Одиночный seek только при смене позиции, дальше — getNextFrame()
            if (needSeek) {
                decoder.seekTo(currentTime);
                needSeek = false;
            }

            int frameNum = (int)(currentTime * m_fps);

            // ── Декодируем следующий кадр (sequential — БЕЗ seek на каждый кадр) ──
            QImage cached;
            if (!m_cache->get(frameNum, cached)) {
                // getNextFrame() читает следующий пакет без seek — в 10-100x быстрее
                QImage frame = decoder.getNextFrame();
                if (!frame.isNull()) {
                    m_cache->put(frameNum, frame);
                    emit frameReady(frameNum);
                } else {
                    // Конец файла или ошибка чтения — ждём seek
                    QMutexLocker lock(&m_mutex);
                    m_condition.wait(&m_mutex, 100);
                    continue;
                }
            }

            currentTime += 1.0 / m_fps;

            // ── Ждём если ушли далеко вперёд ─────────────────────────
            {
                QMutexLocker lock(&m_mutex);
                if (!m_running) break;

                if (m_playPosition > prefetchBase)
                    prefetchBase = m_playPosition;
                double aheadOf = currentTime - prefetchBase;
                if (!m_seekRequested && aheadOf > PREFETCH_AHEAD) {
                    m_condition.wait(&m_mutex, 30);
                }
            }
        }
        decoder.closeFile();
    }

private:
    QString m_filepath;
    double m_fps;
    FrameCache* m_cache;

    bool m_running;
    bool m_seekRequested;
    double m_seekTime;
    double m_playPosition = 0.0;
    QMutex m_mutex;
    QWaitCondition m_condition;
};

#endif // DECODERTHREAD_H



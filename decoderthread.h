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

    void stop() {
        m_running = false;
        m_condition.wakeAll();
        wait(2000);  // Ждём завершения максимум 2 сек
    }

signals:
    // Испускается когда новый кадр готов в кэше
    void frameReady(int frameNumber);

protected:
    void run() override {
        MediaDecoder decoder;
        if (!decoder.openFile(m_filepath)) {
            //qWarning() << "DecoderThread: не могу открыть" << m_filepath;
            return;
        }

        {
            QMutexLocker lock(&m_mutex);
            m_running = true;
        }//32423fwee4g434

        double currentTime = 0.0;
        // Декодируем на 3 секунды вперёд от текущей позиции
        const double PREFETCH_AHEAD = 3.0;

        //while (m_running) {
        while (true) {
            // Проверяем запрос перемотки
            {
                QMutexLocker lock(&m_mutex);
                if (m_seekRequested) {
                    currentTime = m_seekTime;
                    m_seekRequested = false;
                    // Очищаем кэш — старые кадры больше не нужны
                    m_cache->clear();
                }
            }

            int frameNum = (int)(currentTime * m_fps);

            // Проверяем: уже есть в кэше?
            QImage cached;
            if (!m_cache->get(frameNum, cached)) {
                // Нет — декодируем
                QImage frame = decoder.getFrameAt(currentTime);
                if (!frame.isNull()) {
                    m_cache->put(frameNum, frame);
                    emit frameReady(frameNum);
                }
            }

            currentTime += 1.0 / m_fps;

            // Проверяем: не ушли ли слишком далеко вперёд?
            {
                QMutexLocker lock(&m_mutex);
                double aheadOf = currentTime - m_seekTime;
                if (!m_seekRequested && aheadOf > PREFETCH_AHEAD) {
                    m_condition.wait(&m_mutex, 50);
                }
            }
        }
    }

private:
    QString m_filepath;
    double m_fps;
    FrameCache* m_cache;

    bool m_running;
    bool m_seekRequested;
    double m_seekTime;
    QMutex m_mutex;
    QWaitCondition m_condition;
};

#endif // DECODERTHREAD_H

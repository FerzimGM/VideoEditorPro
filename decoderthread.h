#ifndef DECODERTHREAD_H
#define DECODERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QDateTime>
#include "FrameCache.h"
#include "mediadecoder.h"

// DecoderThread — фоновый поток декодирования.
// Декодирует кадры заранее и кладёт в FrameCache. Не блокирует UI.
// Один поток на каждый уникальный видеофайл.
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

    // перемотаться к новому времени
    // Вызывается из UI-потока (потокобезопасно)
    void seekTo(double time)
    {
        QMutexLocker lock(&m_mutex);
        m_seekTime      = time;
        m_playPosition  = time; // ВАЖНО: сброс позиции при seek!
        // Иначе после forward-seek и backward-seek
        // m_playPosition остаётся на старом (большом) значении
        //  thread декодирует без PREFETCH_AHEAD-тормоза
        //  заполняет весь кэш кадрами из середины файла
        //  нужные кадры вытесняются → чёрный экран
        m_seekRequested = true;
        m_condition.wakeAll();
    }

    void updatePlayPosition(double time)
    {
        QMutexLocker lock(&m_mutex);
        // ── КЛЮЧЕВОЙ ФИКС: seek при прыжке между клипами ─────────────
        // Раньше: только увеличивали m_playPosition, thread декодировал
        // последовательно от старой позиции до новой. Для двух клипов
        // из одного файла на дорожке 2 (source 25→35) thread тратил
        // сотни мс на decode 10с промежутка → cache miss → видео замирало
        // пока аудио играло → рассинхрон.
        // Теперь: если прыжок > 2с — делаем seek, кэш очищается,
        // thread начинает декодировать с нужной позиции сразу.
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

    // Остановить поток (блокирует до завершения, макс 2с)
    void stop()
    {
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
        // Превью-режим: кадры в половинном разрешении (в 4 раза меньше пикселей).
        // DecoderThread используется только для живого воспроизведения —
        // рендер в файл использует собственные декодеры без этого флага.
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

        // ИСПРАВЛЕНИЕ 1: ставим m_running=true ДО входа в цикл
        // и синхронизируем m_seekTime с начальным currentTime
        {
            QMutexLocker lock(&m_mutex);
            m_running  = true;
            m_seekTime = 0.0;  // явная инициализация
        }

        double currentTime = 0.0;
        double prefetchBase = 0.0;

        // ФИКС БАГ 3: снижено с 8.0 до 2.5 секунд.
        // 8 секунд буфера при 30fps = 240 кадров на поток заранее.
        // Это разгоняло Баг 1 (сигнал-шторм) и Баг 2 (память).
        // 2.5 секунды достаточно для плавного воспроизведения даже при 2x скорости.
        const double PREFETCH_AHEAD = 2.5;

        bool needSeek = true;  // при старте делаем один seek на начало

        while (true) {
            // Проверяем запрос перемотки
            {
                QMutexLocker lock(&m_mutex);
                if (!m_running) break;

                if (m_seekRequested)
                {
                    currentTime = m_seekTime;
                    prefetchBase = m_seekTime;
                    m_seekRequested = false;
                    // Очищаем кэш при реальном seek (перемотка, смена клипа).
                    // Без этого старые кадры из предыдущей позиции остаются в кэше
                    // и getNearest может вернуть стухший кадр → мерцание.
                    // Защита от ненужной очистки при pause→play — в startPlayback:
                    // он проверяет cache->contains() и НЕ вызывает seekTo если кадр есть.
                    m_cache->clear();
                    m_cache->setPlayPosition((int)(m_seekTime * m_fps + 0.5));
                    needSeek = true;
                }
            }

            // Одиночный seek только при смене позиции, дальше — getNextFrame()
            if (needSeek)
            {
                // seekAndDecode: seek + пропуск кадров до нужного PTS
                // БЕЗ sws_scale на промежуточных кадрах (в 5x быстрее getFrameAt).
                // После неё декодер стоит на правильной позиции →
                // getNextFrame() вернёт следующий кадр с правильным PTS.
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

            // Декодируем следующий кадр
            QImage cached;
            if (!m_cache->get(frameNum, cached))
            {
                // getNextFrame() читает следующий пакет
                QImage frame = decoder.getNextFrame();
                if (!frame.isNull())
                {
                    m_cache->put(frameNum, frame);
                    emitFrameReady(frameNum);
                    // Сообщаем кэшу текущую позицию воспроизведения
                    // чтобы при вытеснении не удалялись нужные кадры
                    {
                        QMutexLocker lock(&m_mutex);
                        m_cache->setPlayPosition((int)(m_playPosition * m_fps + 0.5));
                    }
                } else
                {
                    // Конец файла или ошибка чтения — ждём seek
                    QMutexLocker lock(&m_mutex);
                    m_condition.wait(&m_mutex, 100);
                    continue;
                }
            }

            currentTime += 1.0 / m_fps;

            // Ждём если ушли далеко вперёд
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
    // ФИКС БАГ 1: rate-limit на emit frameReady.
    // DecoderThread может декодировать 200+ fps — без лимита это
    // 200 сигналов/сек × 5 потоков = 1000 ивентов в очереди UI.
    // Очередь Qt переполняется → лаг нарастает. Пауза дренирует → норм.
    // Лимит 40мс (~25fps) — достаточно для обновления превью на паузе
    // и не перегружает очередь во время воспроизведения.
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

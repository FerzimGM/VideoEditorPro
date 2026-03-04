#ifndef RENDERENGINE_H
#define RENDERENGINE_H

#include <QObject>
#include <QThread>
#include <QImage>
#include <QList>
#include <QMap>
#include "timelineclip.h"

class MediaDecoder;
class MediaEncoder;

/**
 * RenderEngine — композитинг + кодирование всего таймлайна.
 *
 * АРХИТЕКТУРА:
 *   Таймлайн → [Track 1 (основная, поверх)] + [Track 2 (фоновая)]
 *
 *   Для каждого момента времени:
 *     1. Видео: Track1 поверх Track2.
 *        Если Track1 скрыт или пуст → берём Track2.
 *        Если оба пусты → чёрный кадр.
 *     2. Аудио: миксуем оба трека (с учётом mute/audioHidden).
 *     3. Применяем эффекты к видео.
 *     4. Записываем в энкодер.
 *
 * ОПТИМИЗАЦИИ:
 *   - Декодеры кэшируются по filepath (один декодер на файл)
 *   - Не открываем/закрываем файл каждый кадр
 *   - Рендер выполняется в отдельном потоке (не блокирует UI)
 */

// ===== РАБОЧИЙ ОБЪЕКТ (выполняется в QThread) =====
class RenderWorker : public QObject {
    Q_OBJECT

public:
    explicit RenderWorker(QObject* parent = nullptr);
    ~RenderWorker();

    void setClips(const QList<TimelineClip>& clips)       { m_clips = clips; }
    void setOutputPath(const QString& path)                { m_outputPath = path; }
    void setOutputResolution(int w, int h)                 { m_outputWidth = w; m_outputHeight = h; }
    void setFps(double fps)                                { m_fps = fps; }
    void setBitrate(int bitrate)                           { m_bitrate = bitrate; }

public slots:
    void process();  // Основной метод — запускается в потоке
    void cancel();

signals:
    void progressChanged(int percent);
    void renderFinished(bool success);
    void errorOccurred(const QString& message);

private:
    QList<TimelineClip> m_clips;
    QString m_outputPath;
    int m_outputWidth;
    int m_outputHeight;
    double m_fps;
    int m_bitrate;
    bool m_cancelled;

    // РАЗДЕЛЬНЫЕ кэши декодеров для видео и аудио.
    // Нельзя использовать один декодер на файл: av_read_frame читает пакеты
    // обоих потоков вперемешку. Если аудиодекодер читает аудиопакеты,
    // он пропускает (выбрасывает) видеопакеты — следующий getNextFrame()
    // получает неверную позицию и возвращает только keyframes.
    QMap<QString, MediaDecoder*> m_videoDecoders;  // только видео
    QMap<QString, MediaDecoder*> m_audioDecoders;  // только аудио

    // Позиция последнего декодированного видеокадра (для sequential read)
    QMap<QString, double> m_videoPositions;

    // Получить или создать декодер для файла
    MediaDecoder* getVideoDecoder(const QString& filepath);
    MediaDecoder* getAudioDecoder(const QString& filepath);

    // Закрыть все кэшированные декодеры
    void closeAllDecoders();

    // ===== КОМПОЗИТИНГ =====

    // Найти активный клип на данной дорожке в данное время
    TimelineClip* findActiveClip(double time, int trackIndex);

    // Получить видеокадр для момента времени (композитинг двух дорожек)
    QImage compositeVideoAt(double time);

    // Получить аудио для момента времени (микширование двух дорожек)
    QVector<float> mixAudioAt(double time, double frameDuration);

    // Получить видеокадр одного клипа
    QImage decodeVideoFrame(TimelineClip* clip, double timelineTime);

    // Получить аудио одного клипа
    QVector<float> decodeAudioChunk(TimelineClip* clip, double timelineTime,
                                    double duration);

    // ===== ЭФФЕКТЫ =====
    QImage applyClipEffects(const QImage& frame, const TimelineClip& clip);
    QImage applyBrightness(const QImage& frame, double value);
    QImage applyContrast(const QImage& frame, double value);
    QImage applySaturation(const QImage& frame, double value);
    QImage applyGrayscale(const QImage& frame);
    QImage applyBlur(const QImage& frame, double radius);
    QImage applySharpness(const QImage& frame, double strength);
};


// ===== МЕНЕДЖЕР РЕНДЕРИНГА (создаёт поток, управляет жизненным циклом) =====
class RenderEngine : public QObject
{
    Q_OBJECT

public:
    explicit RenderEngine(QObject *parent = nullptr);
    ~RenderEngine();

    // Настройка
    void setClips(const QList<TimelineClip>& clips);
    void setOutputPath(const QString& path);
    void setOutputResolution(int width, int height);
    void setOutputCodec(const QString& codec);
    void setOutputFormat(const QString& format);
    void setFps(double fps);
    void setBitrate(int bitrate);

    // Запуск / отмена
    bool startRender();   // Запускает рендер в отдельном потоке
    void cancel();

    // Статические эффекты (для превью, без потока)
    static QImage applyBrightness(const QImage& frame, double value);
    static QImage applyContrast(const QImage& frame, double value);
    static QImage applySaturation(const QImage& frame, double value);
    static QImage applyGrayscale(const QImage& frame);
    static QImage applyBlur(const QImage& frame, double radius);
    static QImage applySharpness(const QImage& frame, double strength);

signals:
    void progressChanged(int percent);
    void renderFinished(bool success);
    void error(const QString& message);

private:
    QList<TimelineClip> m_clips;
    QString m_outputPath;
    int m_outputWidth;
    int m_outputHeight;
    QString m_codec;
    QString m_format;
    double m_fps;
    int m_bitrate;

    QThread* m_thread;
    RenderWorker* m_worker;
};

#endif // RENDERENGINE_H

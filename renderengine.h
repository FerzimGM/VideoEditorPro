#ifndef RENDERENGINE_H
#define RENDERENGINE_H

#include <QObject>
#include <QThread>
#include <QImage>
#include <QList>
#include <QHash>
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

    void setClips(const QList<TimelineClip>& clips)  { m_clips = clips; }
    void setOutputPath(const QString& path)           { m_outputPath = path; }
    void setOutputResolution(int w, int h)            { m_outputWidth = w; m_outputHeight = h; }
    void setFps(double fps)                           { m_fps = fps; }
    void setBitrate(int bitrate)                      { m_bitrate = bitrate; }

public slots:
    void process();
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

    QMap<QString, MediaDecoder*> m_videoDecoders;
    QMap<QString, MediaDecoder*> m_audioDecoders;
    QMap<QString, double>        m_videoPositions;
    QHash<QString, QVector<float>> m_audioDelayBufs;
    QHash<QString, int>            m_audioDelayPos;

    MediaDecoder*  getVideoDecoder(const QString& filepath);
    MediaDecoder*  getAudioDecoder(const QString& filepath);
    void           closeAllDecoders();

    TimelineClip*  findActiveClip(double time, int trackIndex);
    QImage         compositeVideoAt(double time);
    QVector<float> mixAudioAt(double time, double frameDuration);
    QImage         decodeVideoFrame(TimelineClip* clip, double timelineTime);
    QVector<float> decodeAudioChunk(TimelineClip* clip, double timelineTime, double duration);

    QImage applyClipEffects(const QImage& frame, const TimelineClip& clip);
    QImage applyBrightness(const QImage& frame, double value);
    QImage applyContrast(const QImage& frame, double value);
    QImage applySaturation(const QImage& frame, double value);
    QImage applyGrayscale(const QImage& frame);
};


// ===== МЕНЕДЖЕР РЕНДЕРИНГА =====
class RenderEngine : public QObject
{
    Q_OBJECT
public:
    explicit RenderEngine(QObject *parent = nullptr);
    ~RenderEngine();

    void setClips(const QList<TimelineClip>& clips);
    void setOutputPath(const QString& path);
    void setOutputResolution(int width, int height);
    void setOutputCodec(const QString& codec);
    void setOutputFormat(const QString& format);
    void setFps(double fps);
    void setBitrate(int bitrate);

    bool startRender();
    void cancel();

    // ── Статические эффекты для превью и live-display ────────────────────
    // Применяет весь стек эффектов из effects-карты к кадру.
    static QImage applyEffectsToFrame(const QImage& frame,
                                      const QMap<QString, double>& effects,
                                      int frameIndex = 0);

    static QImage applyBrightness(const QImage& frame, double value);
    static QImage applyContrast(const QImage& frame, double value);
    static QImage applySaturation(const QImage& frame, double value);
    static QImage applyGrayscale(const QImage& frame);
    static QImage applyBlur(const QImage& frame, double radius);
    static QImage applySharpness(const QImage& frame, double strength);
    static QImage applyHue(const QImage& frame, double degrees);
    static QImage applySepia(const QImage& frame, double intensity);
    static QImage applyVignette(const QImage& frame, double strength);
    static QImage applyInvert(const QImage& frame);
    static QImage applyPosterize(const QImage& frame, double levels);
    static QImage applyPixelate(const QImage& frame, double blockSize);
    static QImage applyTemperature(const QImage& frame, double value);
    static QImage applyTint(const QImage& frame, double hue, double strength);
    static QImage applyGrain(const QImage& frame, double strength, int frameIndex = 0);
    static QImage applyChromaKey(const QImage& frame, double threshold, double smoothness);

    static QImage applyTransition(const QImage& frameFrom, const QImage& frameTo,
                                  int type, float progress);

    static int transitionNameToCode(const QString& name) {
        if (name == "fade_in"  || name == "fade_out") return 1;
        if (name == "wipe_right") return 2;
        if (name == "wipe_left")  return 3;
        if (name == "zoom_in")    return 4;
        if (name == "zoom_out")   return 5;
        if (name == "flash")      return 6;
        return 0;
    }

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

    QThread*      m_thread;
    RenderWorker* m_worker;
};

#endif // RENDERENGINE_H

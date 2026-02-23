#ifndef RENDERENGINE_H
#define RENDERENGINE_H

#include <QObject>
#include <QImage>
#include <QList>
#include "timelineclip.h"

// Forward declarations
class MediaDecoder;
class MediaEncoder;

/**
 * RenderEngine - композитинг клипов и применение эффектов
 *
 * Ответственность:
 * - Композитинг нескольких клипов в один timeline
 * - Применение видео эффектов (brightness, contrast, saturation)
 * - Микширование аудио из нескольких источников
 * - Управление процессом рендеринга
 */
class RenderEngine : public QObject
{
    Q_OBJECT

public:
    explicit RenderEngine(QObject *parent = nullptr);
    ~RenderEngine();

    // ===== НАСТРОЙКА =====
    void setClips(const QList<TimelineClip>& clips);
    void setOutputPath(const QString& path);
    void setOutputResolution(int width, int height);
    void setOutputCodec(const QString& codec);  // "h264", "h265", etc.
    void setOutputFormat(const QString& format); // "mp4", "avi", "mov"

    // ===== РЕНДЕРИНГ =====
    bool render();  // Запустить рендеринг
    void cancel();  // Отменить рендеринг

    // ===== ПРИМЕНЕНИЕ ЭФФЕКТОВ К КАДРУ =====
    QImage applyEffects(const QImage& frame, const QMap<QString, double>& effects);

    // Отдельные эффекты:
    QImage applyBrightness(const QImage& frame, double value);   // 0.5 - 2.0
    QImage applyContrast(const QImage& frame, double value);     // 0.5 - 2.0
    QImage applySaturation(const QImage& frame, double value);   // 0.0 - 2.0
    QImage applyGrayscale(const QImage& frame);

signals:
    void progressChanged(int percent);  // 0-100
    void renderFinished(bool success);
    void error(const QString& message);

private:
    QList<TimelineClip> m_clips;
    QString m_outputPath;
    int m_outputWidth;
    int m_outputHeight;
    QString m_codec;
    QString m_format;
    bool m_cancelled;

    // ===== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ =====

    // Получить клип который активен в указанное время на дорожке
    TimelineClip* getActiveClip(double time, int trackIndex);

    // Получить кадр для указанного времени
    // Учитывает композитинг нескольких дорожек
    QImage renderFrameAt(double time);

    // Применить все эффекты клипа к кадру
    QImage applyClipEffects(const QImage& frame, const TimelineClip& clip);

    // Смиксовать аудио из всех активных клипов
    QByteArray mixAudioAt(double time, double duration);
};

#endif // RENDERENGINE_H

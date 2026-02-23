#include "renderengine.h"
#include "mediadecoder.h"
#include "mediaencoder.h"
#include <QDebug>
#include <QColor>
#include <QRgb>
#include <cmath>

RenderEngine::RenderEngine(QObject *parent)
    : QObject(parent)
    , m_outputWidth(1920)
    , m_outputHeight(1080)
    , m_codec("h264")
    , m_format("mp4")
    , m_cancelled(false)
{
    qDebug() << "🎬 RenderEngine создан";
}

RenderEngine::~RenderEngine() {
    qDebug() << "🎬 RenderEngine уничтожен";
}

// ===== НАСТРОЙКА =====
void RenderEngine::setClips(const QList<TimelineClip>& clips) {
    m_clips = clips;
    qDebug() << "📋 RenderEngine: установлено" << clips.size() << "клипов";
}

void RenderEngine::setOutputPath(const QString& path) {
    m_outputPath = path;
    qDebug() << "📁 Output path:" << path;
}

void RenderEngine::setOutputResolution(int width, int height) {
    m_outputWidth = width;
    m_outputHeight = height;
    qDebug() << "📐 Resolution:" << width << "x" << height;
}

void RenderEngine::setOutputCodec(const QString& codec) {
    m_codec = codec;
    qDebug() << "🎞️ Codec:" << codec;
}

void RenderEngine::setOutputFormat(const QString& format) {
    m_format = format;
    qDebug() << "📦 Format:" << format;
}

// ===== РЕНДЕРИНГ =====
bool RenderEngine::render() {
    qDebug() << "🎬 RenderEngine::render() START";
    qDebug() << "   Клипов:" << m_clips.size();
    qDebug() << "   Выход:" << m_outputPath;
    qDebug() << "   Разрешение:" << m_outputWidth << "x" << m_outputHeight;

    if (m_clips.isEmpty()) {
        qWarning() << "❌ Нет клипов для рендеринга";
        emit error("Нет клипов");
        emit renderFinished(false);
        return false;
    }

    // 1. Найти общую длительность timeline
    double totalDuration = 0.0;
    for (const TimelineClip& clip : m_clips) {
        double end = clip.endTime();
        if (end > totalDuration) {
            totalDuration = end;
        }
    }

    qDebug() << "⏱️ Общая длительность:" << totalDuration << "сек";

    // 2. Создать MediaEncoder для записи
    MediaEncoder encoder;

    if (!encoder.createOutputFile(m_outputPath, m_outputWidth, m_outputHeight)) {
        qWarning() << "❌ Не могу создать выходной файл";
        emit error("Не могу создать файл");
        emit renderFinished(false);
        return false;
    }

    encoder.setCodec(m_codec);
    encoder.setFormat(m_format);

    // 3. Рендерить по кадрам
    double fps = 30.0;  // TODO: брать из настроек
    double frameTime = 1.0 / fps;
    int totalFrames = static_cast<int>(totalDuration * fps);
    int currentFrame = 0;

    qDebug() << "🎞️ Всего кадров:" << totalFrames;

    m_cancelled = false;

    for (double time = 0.0; time < totalDuration; time += frameTime) {
        // Проверка на отмену
        if (m_cancelled) {
            qDebug() << "⛔ Рендеринг отменён";
            encoder.finish();
            emit renderFinished(false);
            return false;
        }

        // Получить кадр для текущего времени
        QImage frame = renderFrameAt(time);

        if (frame.isNull()) {
            qWarning() << "⚠️ Пустой кадр на времени" << time;
            // Создать чёрный кадр
            frame = QImage(m_outputWidth, m_outputHeight, QImage::Format_RGB888);
            frame.fill(Qt::black);
        }

        // Записать кадр
        if (!encoder.writeVideoFrame(frame)) {
            qWarning() << "❌ Ошибка записи кадра";
            emit error("Ошибка записи кадра");
            encoder.finish();
            emit renderFinished(false);
            return false;
        }

        // Обновить прогресс
        currentFrame++;
        int percent = (currentFrame * 100) / totalFrames;
        emit progressChanged(percent);

        if (currentFrame % 30 == 0) {  // Лог каждую секунду
            qDebug() << "📊 Прогресс:" << percent << "% (" << currentFrame << "/" << totalFrames << ")";
        }
    }

    // 4. Завершить запись
    encoder.finish();

    qDebug() << "✅ Рендеринг завершён успешно!";
    emit renderFinished(true);
    return true;
}

void RenderEngine::cancel() {
    qDebug() << "⛔ Отмена рендеринга";
    m_cancelled = true;
}

// ===== ПОЛУЧИТЬ КАДР ДЛЯ ВРЕМЕНИ =====
QImage RenderEngine::renderFrameAt(double time) {
    // Найти все активные клипы на всех дорожках в это время
    QList<TimelineClip*> activeClips;

    for (int i = 0; i < m_clips.size(); ++i) {
        TimelineClip& clip = m_clips[i];

        if (time >= clip.startTime && time < clip.endTime()) {
            activeClips.append(&clip);
        }
    }

    if (activeClips.isEmpty()) {
        // Нет активных клипов - чёрный кадр
        return QImage();
    }

    // Композитинг: накладываем клипы от нижней дорожки к верхней
    // Сортируем по trackIndex (верхние дорожки поверх нижних)
    std::sort(activeClips.begin(), activeClips.end(), [](TimelineClip* a, TimelineClip* b) {
        return a->trackIndex > b->trackIndex;  // Больший индекс = нижняя дорожка
    });

    QImage resultFrame;

    for (TimelineClip* clip : activeClips) {
        // Вычислить время внутри клипа
        double clipTime = time - clip->startTime + clip->trimStart;

        // Открыть декодер для этого клипа
        MediaDecoder decoder;

        if (!decoder.openFile(clip->filepath)) {
            qWarning() << "⚠️ Не могу открыть" << clip->filepath;
            continue;
        }

        // Получить кадр
        QImage frame = decoder.getFrameAt(clipTime);
        decoder.closeFile();

        if (frame.isNull()) {
            qWarning() << "⚠️ Не могу декодировать кадр";
            continue;
        }

        // Применить эффекты клипа
        frame = applyClipEffects(frame, *clip);

        // Масштабировать до выходного разрешения
        if (frame.width() != m_outputWidth || frame.height() != m_outputHeight) {
            frame = frame.scaled(m_outputWidth, m_outputHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        // Композитинг (пока просто берём первый непустой кадр)
        // TODO: поддержка прозрачности и blend modes
        if (resultFrame.isNull()) {
            resultFrame = frame;
        }
    }

    return resultFrame;
}

// ===== ПРИМЕНИТЬ ЭФФЕКТЫ КЛИПА =====
QImage RenderEngine::applyClipEffects(const QImage& frame, const TimelineClip& clip) {
    if (clip.effects.isEmpty()) {
        return frame;  // Нет эффектов
    }

    QImage result = frame;

    // Применить каждый эффект
    for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it) {
        QString effectName = it.key();
        double value = it.value();

        if (effectName == "brightness") {
            result = applyBrightness(result, value);
        } else if (effectName == "contrast") {
            result = applyContrast(result, value);
        } else if (effectName == "saturation") {
            result = applySaturation(result, value);
        } else if (effectName == "grayscale" && value > 0.5) {
            result = applyGrayscale(result);
        }
    }

    return result;
}

// ===== ЭФФЕКТЫ =====

QImage RenderEngine::applyBrightness(const QImage& frame, double value) {
    // value: 0.5 = темнее, 1.0 = без изменений, 2.0 = светлее
    QImage result = frame.convertToFormat(QImage::Format_RGB888);

    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = line[x];

            int r = qBound(0, static_cast<int>(qRed(pixel) * value), 255);
            int g = qBound(0, static_cast<int>(qGreen(pixel) * value), 255);
            int b = qBound(0, static_cast<int>(qBlue(pixel) * value), 255);

            line[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QImage RenderEngine::applyContrast(const QImage& frame, double value) {
    // value: 0.5 = меньше контраста, 1.0 = без изменений, 2.0 = больше
    QImage result = frame.convertToFormat(QImage::Format_RGB888);

    double factor = (259.0 * (value * 255.0 + 255.0)) / (255.0 * (259.0 - value * 255.0));

    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = line[x];

            int r = qBound(0, static_cast<int>(factor * (qRed(pixel) - 128) + 128), 255);
            int g = qBound(0, static_cast<int>(factor * (qGreen(pixel) - 128) + 128), 255);
            int b = qBound(0, static_cast<int>(factor * (qBlue(pixel) - 128) + 128), 255);

            line[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QImage RenderEngine::applySaturation(const QImage& frame, double value) {
    // value: 0.0 = grayscale, 1.0 = без изменений, 2.0 = насыщеннее
    QImage result = frame.convertToFormat(QImage::Format_RGB888);

    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = line[x];

            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            // Luminance
            int gray = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);

            // Интерполяция между grayscale и оригиналом
            r = qBound(0, static_cast<int>(gray + (r - gray) * value), 255);
            g = qBound(0, static_cast<int>(gray + (g - gray) * value), 255);
            b = qBound(0, static_cast<int>(gray + (b - gray) * value), 255);

            line[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QImage RenderEngine::applyGrayscale(const QImage& frame) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);

    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = line[x];

            int gray = static_cast<int>(
                0.299 * qRed(pixel) +
                0.587 * qGreen(pixel) +
                0.114 * qBlue(pixel)
                );

            line[x] = qRgb(gray, gray, gray);
        }
    }

    return result;
}

// ===== TODO: АУДИО =====
QByteArray RenderEngine::mixAudioAt(double time, double duration) {
    qDebug() << "⚠️ TODO: mixAudioAt" << time << duration;
    return QByteArray();
}

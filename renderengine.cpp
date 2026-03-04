#include "renderengine.h"
#include "mediadecoder.h"
#include "mediaencoder.h"
#include <QDebug>
#include <QColor>
//#include <cmath>

// =====================================================================
//  RenderWorker — выполняется в фоновом потоке
// =====================================================================

RenderWorker::RenderWorker(QObject* parent)
    : QObject(parent)
    , m_outputWidth(1920)
    , m_outputHeight(1080)
    , m_fps(30.0)
    , m_bitrate(5000000)
    , m_cancelled(false)
{
}

RenderWorker::~RenderWorker() {
    closeAllDecoders();
}

// ===== ОСНОВНОЙ ПРОЦЕСС РЕНДЕРИНГА =====
void RenderWorker::process() {
    qDebug() << "🎬 RenderWorker::process() START";
    qDebug() << "   Клипов:" << m_clips.size();
    qDebug() << "   Выход:" << m_outputPath;
    qDebug() << "   Разрешение:" << m_outputWidth << "x" << m_outputHeight;
    qDebug() << "   FPS:" << m_fps;

    m_cancelled = false;

    if (m_clips.isEmpty()) {
        emit errorOccurred("Нет клипов для рендеринга");
        emit renderFinished(false);
        return;
    }

    // 1. Вычислить общую длительность
    double totalDuration = 0.0;
    for (const TimelineClip& clip : m_clips) {
        double end = clip.endTime();
        if (end > totalDuration) totalDuration = end;
    }

    qDebug() << "⏱️ Общая длительность:" << totalDuration << "с";

    // 2. Создать энкодер
    MediaEncoder encoder;
    encoder.setFrameRate(m_fps);
    encoder.setBitrate(m_bitrate);
    encoder.setAudioEnabled(true);

    if (!encoder.createOutputFile(m_outputPath, m_outputWidth, m_outputHeight)) {
        emit errorOccurred("Не могу создать выходной файл");
        emit renderFinished(false);
        return;
    }

    // 3. Рендер кадр за кадром
    double frameTime = 1.0 / m_fps;
    int totalFrames  = static_cast<int>(totalDuration * m_fps);
    int currentFrame = 0;
    int lastPercent  = -1;

    for (double time = 0.0; time < totalDuration && !m_cancelled; time += frameTime)
    {
        // ── ВИДЕО ──────────────────────────────────────────────
        QImage frame = compositeVideoAt(time);

        if (frame.isNull()) {
            // Чёрный кадр
            frame = QImage(m_outputWidth, m_outputHeight,
                           QImage::Format_RGB888);
            frame.fill(Qt::black);
        }

        if (!encoder.writeVideoFrame(frame)) {
            emit errorOccurred("Ошибка записи видеокадра");
            encoder.finish();
            closeAllDecoders();
            emit renderFinished(false);
            return;
        }

        // ── АУДИО ──────────────────────────────────────────────
        QVector<float> audio = mixAudioAt(time, frameTime);

        if (!audio.isEmpty()) {
            encoder.writeAudioSamples(audio);
        }

        // ── ПРОГРЕСС ───────────────────────────────────────────
        currentFrame++;
        int percent = (totalFrames > 0)
                          ? (currentFrame * 100) / totalFrames
                          : 0;

        if (percent != lastPercent) {
            lastPercent = percent;
            emit progressChanged(percent);
        }

        if (currentFrame % static_cast<int>(m_fps) == 0) {
            qDebug() << "📊 Рендер:" << percent << "% ("
                     << currentFrame << "/" << totalFrames << ")";
        }
    }

    // 4. Проверка отмены
    if (m_cancelled) {
        qDebug() << "⛔ Рендеринг отменён";
        encoder.finish();
        closeAllDecoders();
        emit renderFinished(false);
        return;
    }

    // 5. Завершить
    encoder.finish();
    closeAllDecoders();

    qDebug() << "✅ Рендеринг завершён! Кадров:" << currentFrame;
    emit progressChanged(100);
    emit renderFinished(true);
}

void RenderWorker::cancel() {
    m_cancelled = true;
}

// ===== КЭШ ДЕКОДЕРОВ (раздельный для видео и аудио) =====
MediaDecoder* RenderWorker::getVideoDecoder(const QString& filepath) {
    if (m_videoDecoders.contains(filepath))
        return m_videoDecoders[filepath];

    auto* decoder = new MediaDecoder();
    if (!decoder->openFile(filepath)) {
        qWarning() << "⚠️ Не могу открыть (video):" << filepath;
        delete decoder;
        return nullptr;
    }
    m_videoDecoders[filepath] = decoder;
    return decoder;
}

MediaDecoder* RenderWorker::getAudioDecoder(const QString& filepath) {
    if (m_audioDecoders.contains(filepath))
        return m_audioDecoders[filepath];

    auto* decoder = new MediaDecoder();
    if (!decoder->openFile(filepath)) {
        qWarning() << "⚠️ Не могу открыть (audio):" << filepath;
        delete decoder;
        return nullptr;
    }
    m_audioDecoders[filepath] = decoder;
    return decoder;
}

void RenderWorker::closeAllDecoders() {
    for (auto* d : m_videoDecoders) { d->closeFile(); delete d; }
    for (auto* d : m_audioDecoders) { d->closeFile(); delete d; }
    m_videoDecoders.clear();
    m_audioDecoders.clear();
    m_videoPositions.clear();
}


// ===== НАЙТИ АКТИВНЫЙ КЛИП =====
TimelineClip* RenderWorker::findActiveClip(double time, int trackIndex) {
    for (int i = 0; i < m_clips.size(); ++i) {
        TimelineClip& clip = m_clips[i];
        if (clip.trackIndex == trackIndex && clip.isActiveAt(time)) {
            return &clip;
        }
    }
    return nullptr;
}

// ===== КОМПОЗИТИНГ ВИДЕО =====
// Логика приоритетов:
//   1. Track 1 (основная) — всегда поверх
//   2. Track 2 (фоновая) — показывается только если Track 1 пуст/скрыт
//   3. Оба пусты → чёрный кадр (null QImage)
QImage RenderWorker::compositeVideoAt(double time) {
    // Попробовать Track 1 (основная дорожка)
    TimelineClip* clip1 = findActiveClip(time, 1);
    if (clip1 && !clip1->isVideoHidden) {
        QImage frame = decodeVideoFrame(clip1, time);
        if (!frame.isNull()) {
            frame = applyClipEffects(frame, *clip1);
            // Масштабировать до выходного разрешения
            if (frame.width() != m_outputWidth ||
                frame.height() != m_outputHeight) {
                frame = frame.scaled(m_outputWidth, m_outputHeight,
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
                // Если aspect ratio не совпадает — вставить в чёрный фон
                if (frame.width() != m_outputWidth ||
                    frame.height() != m_outputHeight) {
                    QImage canvas(m_outputWidth, m_outputHeight,
                                  QImage::Format_RGB888);
                    canvas.fill(Qt::black);

                    int dx = (m_outputWidth  - frame.width())  / 2;
                    int dy = (m_outputHeight - frame.height()) / 2;

                    // Копируем пиксели вручную
                    for (int y = 0; y < frame.height(); ++y) {
                        const uchar* src = frame.constScanLine(y);
                        uchar* dst = canvas.scanLine(y + dy);
                        int bpp = frame.depth() / 8;
                        memcpy(dst + dx * bpp, src, frame.width() * bpp);
                    }
                    frame = canvas;
                }
            }
            return frame;
        }
    }

    // Попробовать Track 2 (фоновая дорожка)
    TimelineClip* clip2 = findActiveClip(time, 2);
    if (clip2 && !clip2->isVideoHidden) {
        QImage frame = decodeVideoFrame(clip2, time);
        if (!frame.isNull()) {
            frame = applyClipEffects(frame, *clip2);
            if (frame.width() != m_outputWidth ||
                frame.height() != m_outputHeight) {
                frame = frame.scaled(m_outputWidth, m_outputHeight,
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
                if (frame.width() != m_outputWidth ||
                    frame.height() != m_outputHeight) {
                    QImage canvas(m_outputWidth, m_outputHeight,
                                  QImage::Format_RGB888);
                    canvas.fill(Qt::black);
                    int dx = (m_outputWidth  - frame.width())  / 2;
                    int dy = (m_outputHeight - frame.height()) / 2;
                    for (int y = 0; y < frame.height(); ++y) {
                        const uchar* src = frame.constScanLine(y);
                        uchar* dst = canvas.scanLine(y + dy);
                        int bpp = frame.depth() / 8;
                        memcpy(dst + dx * bpp, src, frame.width() * bpp);
                    }
                    frame = canvas;
                }
            }
            return frame;
        }
    }

    return QImage();  // Чёрный кадр
}

// ===== ДЕКОДИРОВАТЬ ВИДЕОКАДР КЛИПА =====
// Использует отдельный видеодекодер (не смешанный с аудио).
// Читает последовательно — seek только при старте или прыжке.
QImage RenderWorker::decodeVideoFrame(TimelineClip* clip, double timelineTime) {
    MediaDecoder* decoder = getVideoDecoder(clip->filepath);
    if (!decoder || !decoder->hasVideo()) return QImage();

    double sourceTime = clip->sourceTimeAt(timelineTime);
    double fps = decoder->getFrameRate();
    double frameDur = (fps > 0) ? (1.0 / fps) : 0.04;

    QString key = clip->filepath;
    double lastPos = m_videoPositions.value(key, -1.0);

    bool needSeek = (lastPos < 0.0) ||
                    (sourceTime < lastPos - frameDur * 0.5) ||
                    (sourceTime > lastPos + frameDur * 5.0);

    QImage frame;
    if (needSeek) {
        frame = decoder->getFrameAt(sourceTime);
    } else {
        frame = decoder->getNextFrame();
    }

    if (!frame.isNull())
        m_videoPositions[key] = sourceTime;

    return frame;
}

// ===== МИКШИРОВАНИЕ АУДИО =====
// Миксует аудио с обоих треков. Тишина если оба muted/hidden/пусты.
QVector<float> RenderWorker::mixAudioAt(double time, double frameDuration) {
    int numSamples = static_cast<int>(
        frameDuration * MediaDecoder::OUTPUT_SAMPLE_RATE);
    int totalFloats = numSamples * MediaDecoder::OUTPUT_CHANNELS;

    QVector<float> mixed(totalFloats, 0.0f);
    bool hasAudio = false;

    // Track 1
    TimelineClip* clip1 = findActiveClip(time, 1);
    if (clip1 && !clip1->isMuted && !clip1->isAudioHidden) {
        QVector<float> audio1 = decodeAudioChunk(clip1, time, frameDuration);
        if (!audio1.isEmpty()) {
            hasAudio = true;
            int len = qMin(mixed.size(), audio1.size());
            for (int i = 0; i < len; ++i) {
                mixed[i] += audio1[i];
            }
        }
    }

    // Track 2
    TimelineClip* clip2 = findActiveClip(time, 2);
    if (clip2 && !clip2->isMuted && !clip2->isAudioHidden) {
        QVector<float> audio2 = decodeAudioChunk(clip2, time, frameDuration);
        if (!audio2.isEmpty()) {
            hasAudio = true;
            int len = qMin(mixed.size(), audio2.size());
            for (int i = 0; i < len; ++i) {
                mixed[i] += audio2[i];
            }
        }
    }

    if (!hasAudio) return mixed;  // Тишина

    // Клиппинг: ограничить [-1.0, 1.0]
    for (int i = 0; i < mixed.size(); ++i) {
        if (mixed[i] > 1.0f)       mixed[i] = 1.0f;
        else if (mixed[i] < -1.0f) mixed[i] = -1.0f;
    }

    return mixed;
}

// ===== ДЕКОДИРОВАТЬ АУДИО КУСОК КЛИПА =====
QVector<float> RenderWorker::decodeAudioChunk(TimelineClip* clip,
                                              double timelineTime,
                                              double duration) {
    MediaDecoder* decoder = getAudioDecoder(clip->filepath);
    if (!decoder || !decoder->hasAudio()) return QVector<float>();

    double sourceTime = clip->sourceTimeAt(timelineTime);
    QVector<float> audio = decoder->decodeAudioRange(sourceTime, duration);

    // Применяем громкость если задана
    auto it = clip->effects.find("volume");
    if (it != clip->effects.end()) {
        float vol = static_cast<float>(it.value());
        if (qAbs(vol - 1.0f) > 0.01f) {
            for (float& sample : audio)
                sample = qBound(-1.0f, sample * vol, 1.0f);
        }
    }

    return audio;
}

// ===== ЭФФЕКТЫ =====
QImage RenderWorker::applyClipEffects(const QImage& frame,
                                      const TimelineClip& clip) {
    if (clip.effects.isEmpty()) return frame;

    QImage result = frame;

    for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it) {
        const QString& name = it.key();
        double value = it.value();

        if      (name == "brightness")                   result = applyBrightness(result, value);
        else if (name == "contrast")                     result = applyContrast(result, value);
        else if (name == "saturation")                   result = applySaturation(result, value);
        else if (name == "grayscale" && value > 0.5)     result = applyGrayscale(result);
        else if (name == "blur"      && value > 0.0)     result = applyBlur(result, value);
        else if (name == "sharpness" && value > 0.0)     result = applySharpness(result, value);
        // "volume" применяется к аудио в decodeAudioChunk, не к кадру
    }

    return result;
}

// Яркость: value в диапазоне -1.0 .. +1.0 (аддитивный сдвиг ±255)
QImage RenderWorker::applyBrightness(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int shift = static_cast<int>(value * 255.0);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width() * 3; ++x) {
            int v = static_cast<int>(line[x]) + shift;
            line[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return result;
}

// Контраст: value = 0..3 (1.0 = оригинал)
QImage RenderWorker::applyContrast(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width() * 3; ++x) {
            int v = static_cast<int>((line[x] - 128) * value + 128);
            line[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return result;
}

// Насыщенность: value = 0..2 (1.0 = оригинал, 0 = ч/б)
QImage RenderWorker::applySaturation(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            int idx = x * 3;
            int r = line[idx], g = line[idx+1], b = line[idx+2];
            int gray = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
            line[idx]   = static_cast<uchar>(qBound(0, gray + (int)((r - gray) * value), 255));
            line[idx+1] = static_cast<uchar>(qBound(0, gray + (int)((g - gray) * value), 255));
            line[idx+2] = static_cast<uchar>(qBound(0, gray + (int)((b - gray) * value), 255));
        }
    }
    return result;
}

// Ч/Б
QImage RenderWorker::applyGrayscale(const QImage& frame) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            int idx = x * 3;
            int gray = static_cast<int>(0.299*line[idx] + 0.587*line[idx+1] + 0.114*line[idx+2]);
            line[idx] = line[idx+1] = line[idx+2] = static_cast<uchar>(gray);
        }
    }
    return result;
}

// Размытие: box blur, radius = 0..10
QImage RenderWorker::applyBlur(const QImage& frame, double radius) {
    QImage src = frame.convertToFormat(QImage::Format_RGB888);
    QImage result = src.copy();
    int r = qMax(1, static_cast<int>(radius));
    int w = src.width(), h = src.height();

    // Горизонтальный проход
    QImage temp = src.copy();
    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = src.constScanLine(y);
        uchar* dstLine = temp.scanLine(y);
        for (int x = 0; x < w; ++x) {
            int sumR=0, sumG=0, sumB=0, count=0;
            for (int dx = -r; dx <= r; ++dx) {
                int nx = qBound(0, x+dx, w-1);
                sumR += srcLine[nx*3];
                sumG += srcLine[nx*3+1];
                sumB += srcLine[nx*3+2];
                ++count;
            }
            dstLine[x*3]   = sumR/count;
            dstLine[x*3+1] = sumG/count;
            dstLine[x*3+2] = sumB/count;
        }
    }
    // Вертикальный проход
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            int sumR=0, sumG=0, sumB=0, count=0;
            for (int dy = -r; dy <= r; ++dy) {
                int ny = qBound(0, y+dy, h-1);
                const uchar* l = temp.constScanLine(ny);
                sumR += l[x*3]; sumG += l[x*3+1]; sumB += l[x*3+2];
                ++count;
            }
            uchar* out = result.scanLine(y);
            out[x*3]   = sumR/count;
            out[x*3+1] = sumG/count;
            out[x*3+2] = sumB/count;
        }
    }
    return result;
}

// Резкость: unsharp mask — усиливаем разницу с размытым
QImage RenderWorker::applySharpness(const QImage& frame, double strength) {
    QImage blurred = applyBlur(frame, 1.0);
    QImage src = frame.convertToFormat(QImage::Format_RGB888);
    QImage blurSrc = blurred.convertToFormat(QImage::Format_RGB888);
    QImage result = src.copy();
    int w = src.width(), h = src.height();
    for (int y = 0; y < h; ++y) {
        const uchar* sLine  = src.constScanLine(y);
        const uchar* bLine  = blurSrc.constScanLine(y);
        uchar*       dLine  = result.scanLine(y);
        for (int x = 0; x < w*3; ++x) {
            int diff = static_cast<int>(sLine[x]) - static_cast<int>(bLine[x]);
            int v    = static_cast<int>(sLine[x]) + static_cast<int>(diff * strength);
            dLine[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return result;
}


// =====================================================================
//  RenderEngine — менеджер (главный поток)
// =====================================================================

RenderEngine::RenderEngine(QObject *parent)
    : QObject(parent)
    , m_outputWidth(1920)
    , m_outputHeight(1080)
    , m_fps(30.0)
    , m_bitrate(5000000)
    , m_thread(nullptr)
    , m_worker(nullptr)
{
}

RenderEngine::~RenderEngine() {
    cancel();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
        delete m_thread;
    }
}

void RenderEngine::setClips(const QList<TimelineClip>& clips) {
    m_clips = clips;
}

void RenderEngine::setOutputPath(const QString& path) {
    m_outputPath = path;
}

void RenderEngine::setOutputResolution(int w, int h) {
    m_outputWidth = w;
    m_outputHeight = h;
}

void RenderEngine::setOutputCodec(const QString& codec) {
    m_codec = codec;
}

void RenderEngine::setOutputFormat(const QString& format) {
    m_format = format;
}

void RenderEngine::setFps(double fps) {
    m_fps = fps;
}

void RenderEngine::setBitrate(int bitrate) {
    m_bitrate = bitrate;
}

// ===== ЗАПУСТИТЬ РЕНДЕР В ОТДЕЛЬНОМ ПОТОКЕ =====
bool RenderEngine::startRender() {
    if (m_clips.isEmpty()) {
        emit error("Нет клипов для рендеринга");
        emit renderFinished(false);
        return false;
    }

    if (m_outputPath.isEmpty()) {
        emit error("Не указан путь для сохранения");
        emit renderFinished(false);
        return false;
    }

    // Убить предыдущий поток если есть
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
        delete m_thread;
        m_thread = nullptr;
    }

    // Создать worker и thread
    m_thread = new QThread();
    m_worker = new RenderWorker();
    m_worker->moveToThread(m_thread);

    // Передать параметры
    m_worker->setClips(m_clips);
    m_worker->setOutputPath(m_outputPath);
    m_worker->setOutputResolution(m_outputWidth, m_outputHeight);
    m_worker->setFps(m_fps);
    m_worker->setBitrate(m_bitrate);

    // Подключить сигналы
    connect(m_thread, &QThread::started,
            m_worker, &RenderWorker::process);

    connect(m_worker, &RenderWorker::progressChanged,
            this,     &RenderEngine::progressChanged);

    connect(m_worker, &RenderWorker::renderFinished,
            this,     &RenderEngine::renderFinished);

    connect(m_worker, &RenderWorker::errorOccurred,
            this,     &RenderEngine::error);

    // Автоочистка
    connect(m_worker, &RenderWorker::renderFinished,
            m_thread, &QThread::quit);

    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    // Запустить
    qDebug() << "🚀 Запуск рендеринга в отдельном потоке...";
    m_thread->start();

    return true;
}

void RenderEngine::cancel() {
    if (m_worker) {
        m_worker->cancel();
    }
}

// ===== СТАТИЧЕСКИЕ ЭФФЕКТЫ (для превью) =====
QImage RenderEngine::applyBrightness(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int shift = static_cast<int>(value * 255.0);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width() * 3; ++x) {
            int v = static_cast<int>(line[x]) + shift;
            line[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return result;
}

QImage RenderEngine::applyContrast(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width() * 3; ++x) {
            int v = static_cast<int>((line[x] - 128) * value + 128);
            line[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return result;
}

QImage RenderEngine::applySaturation(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            int idx = x * 3;
            int r = line[idx], g = line[idx+1], b = line[idx+2];
            int gray = static_cast<int>(0.299*r + 0.587*g + 0.114*b);
            line[idx]   = static_cast<uchar>(qBound(0, gray + (int)((r-gray)*value), 255));
            line[idx+1] = static_cast<uchar>(qBound(0, gray + (int)((g-gray)*value), 255));
            line[idx+2] = static_cast<uchar>(qBound(0, gray + (int)((b-gray)*value), 255));
        }
    }
    return result;
}

QImage RenderEngine::applyGrayscale(const QImage& frame) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y) {
        uchar* line = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            int idx = x * 3;
            int gray = static_cast<int>(
                0.299 * line[idx] + 0.587 * line[idx+1] + 0.114 * line[idx+2]);
            line[idx] = line[idx+1] = line[idx+2] = static_cast<uchar>(gray);
        }
    }
    return result;
}

QImage RenderEngine::applyBlur(const QImage& frame, double radius) {
    QImage src = frame.convertToFormat(QImage::Format_RGB888);
    QImage result = src.copy();
    int r = qMax(1, static_cast<int>(radius));
    int w = src.width(), h = src.height();
    QImage temp = src.copy();
    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = src.constScanLine(y);
        uchar* dstLine = temp.scanLine(y);
        for (int x = 0; x < w; ++x) {
            int sumR=0,sumG=0,sumB=0,count=0;
            for (int dx=-r; dx<=r; ++dx) { int nx=qBound(0,x+dx,w-1); sumR+=srcLine[nx*3]; sumG+=srcLine[nx*3+1]; sumB+=srcLine[nx*3+2]; ++count; }
            dstLine[x*3]=sumR/count; dstLine[x*3+1]=sumG/count; dstLine[x*3+2]=sumB/count;
        }
    }
    for (int x=0;x<w;++x) for (int y=0;y<h;++y) {
            int sumR=0,sumG=0,sumB=0,count=0;
            for (int dy=-r;dy<=r;++dy){int ny=qBound(0,y+dy,h-1);const uchar*l=temp.constScanLine(ny);sumR+=l[x*3];sumG+=l[x*3+1];sumB+=l[x*3+2];++count;}
            uchar*out=result.scanLine(y); out[x*3]=sumR/count; out[x*3+1]=sumG/count; out[x*3+2]=sumB/count;
        }
    return result;
}

QImage RenderEngine::applySharpness(const QImage& frame, double strength) {
    QImage blurred = applyBlur(frame, 1.0);
    QImage src = frame.convertToFormat(QImage::Format_RGB888);
    QImage blurSrc = blurred.convertToFormat(QImage::Format_RGB888);
    QImage result = src.copy();
    for (int y=0;y<src.height();++y) {
        const uchar*s=src.constScanLine(y); const uchar*b=blurSrc.constScanLine(y); uchar*d=result.scanLine(y);
        for (int x=0;x<src.width()*3;++x) { int v=(int)s[x]+(int)((s[x]-b[x])*strength); d[x]=(uchar)qBound(0,v,255); }
    }
    return result;
}



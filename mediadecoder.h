#ifndef MEDIADECODER_H
#define MEDIADECODER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>  // GPU-декодирование: AVHWDeviceContext
}

/**
 * MediaDecoder — универсальный декодер видео и аудио.
 *
 * Поддерживает:
 * - Любой формат через FFmpeg (mp4, avi, mov, mkv, webm, flv...)
 * - Декодирование видеокадров (QImage)
 * - Декодирование аудио (interleaved float PCM, 44100Hz, stereo)
 * - Seek по времени
 * - Последовательное чтение (эффективно для рендеринга)
 *
 * ОПТИМИЗАЦИИ:
 * - setPreviewMode(true): sws_scale отдаёт кадры в половинном разрешении.
 *   Нагрузка на CPU падает в 4 раза. Для рендера в файл НЕ включать.
 * - GPU-декодирование: initializeVideo пробует D3D11VA → DXVA2 → CPU.
 *   Fallback автоматический — если GPU недоступен, работает как раньше.
 */
class MediaDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MediaDecoder(QObject *parent = nullptr);
    ~MediaDecoder();

    // ===== ОТКРЫТИЕ / ЗАКРЫТИЕ =====
    bool openFile(const QString& filepath);
    void closeFile();

    // ===== РЕЖИМ ПРЕВЬЮ (половинное разрешение) =====
    // Включать только для DecoderThread (живое воспроизведение).
    // RenderWorker всегда работает в полном разрешении.
    void setPreviewMode(bool enabled) { m_previewMode = enabled; }
    bool isPreviewMode() const { return m_previewMode; }

    // ===== ИНФОРМАЦИЯ О ФАЙЛЕ =====
    double getDuration() const;
    int getVideoWidth() const;
    int getVideoHeight() const;
    double getFrameRate() const;
    int getAudioSampleRate() const;
    int getAudioChannels() const;
    bool hasVideo() const { return m_videoStreamIndex >= 0; }
    bool hasAudio() const { return m_audioStreamIndex >= 0; }

    // ===== ДЕКОДИРОВАНИЕ ВИДЕО =====
    QImage getFrameAt(double timestamp);
    QImage seekAndDecode(double timestamp);
    QImage getNextFrame();
    double getLastVideoPts() const { return m_lastVideoPts; }

    // ===== ДЕКОДИРОВАНИЕ АУДИО =====
    QVector<float> decodeAudioRange(double startTime, double duration);

    // ===== SEEK =====
    bool seekTo(double timestamp);

    // ===== СОСТОЯНИЕ =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getFilepath() const { return m_filepath; }
    bool isUsingGPU() const { return m_hwDeviceCtx != nullptr; }

    // ===== КОНСТАНТЫ АУДИО ВЫВОДА =====
    static const int OUTPUT_SAMPLE_RATE = 44100;
    static const int OUTPUT_CHANNELS    = 2;

signals:
    void error(const QString& message);

private:
    QString m_filepath;

    // ===== РЕЖИМ ПРЕВЬЮ =====
    // Если true — sws_scale масштабирует кадр в width/2 × height/2.
    // QML растягивает результат на весь экран — разницы почти не видно,
    // но пикселей в 4 раза меньше → CPU нагрузка в 4 раза ниже.
    bool m_previewMode = false;

    // FFmpeg контексты
    AVFormatContext* m_formatContext;

    // Видео
    AVCodecContext* m_videoCodecContext;
    AVStream* m_videoStream;
    int m_videoStreamIndex;
    SwsContext* m_swsContext;

    // GPU-декодирование (аппаратный контекст)
    // nullptr если GPU недоступен или не поддерживается — автоматический fallback на CPU.
    // Тип: D3D11VA (Windows 8+) или DXVA2 (Windows 7+).
    AVBufferRef* m_hwDeviceCtx = nullptr;
    // Хранит тип аппаратного декодера чтобы правильно передать кадр при transfer
    AVHWDeviceType m_hwDeviceType = AV_HWDEVICE_TYPE_NONE;
    // Формат пикселей GPU-кадра (до копирования в RAM)
    AVPixelFormat m_hwPixFmt = AV_PIX_FMT_NONE;

    // Аудио
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;
    int m_audioStreamIndex;
    SwrContext* m_swrContext;

    // Временные буферы
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    AVPacket* m_packet;

    // Позиция последнего декодированного аудио (для sequential)
    double m_lastAudioPos;

    // PTS последнего декодированного видеокадра
    double m_lastVideoPts = -1.0;

    // Буфер переполнения AAC (см. decodeAudioRange)
    QVector<float> m_audioOverflow;
    bool m_skipDone = false;

    // Кэшированные параметры SwsContext
    AVPixelFormat m_cachedSwsFmt;
    int m_cachedSwsW;
    int m_cachedSwsH;
    // Целевой размер sws (зависит от m_previewMode)
    int m_cachedDstW = 0;
    int m_cachedDstH = 0;

    // Вспомогательные функции
    bool initializeVideo();
    bool tryInitHardwareDecoder(const AVCodec* codec); // GPU fallback
    bool initializeAudio();
    bool initializeSwrContext();
    QImage avFrameToQImage(AVFrame* frame);
    void freeResources();
};

#endif // MEDIADECODER_H






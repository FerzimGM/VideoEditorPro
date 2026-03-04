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
    QImage getNextFrame();

    // ===== ДЕКОДИРОВАНИЕ АУДИО =====
    // Декодировать аудио в диапазоне [startTime, startTime+duration]
    // Возвращает interleaved float PCM, стерео, 44100Hz
    QVector<float> decodeAudioRange(double startTime, double duration);

    // Legacy-обёртка (обратная совместимость)
    QByteArray getAudioSamples(double startTime, double endTime);

    // ===== SEEK =====
    bool seekTo(double timestamp);

    // ===== СОСТОЯНИЕ =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getFilepath() const { return m_filepath; }

    // ===== КОНСТАНТЫ АУДИО ВЫВОДА =====
    static const int OUTPUT_SAMPLE_RATE = 44100;
    static const int OUTPUT_CHANNELS    = 2;

signals:
    void error(const QString& message);

private:
    QString m_filepath;

    // FFmpeg контексты
    AVFormatContext* m_formatContext;

    // Видео
    AVCodecContext* m_videoCodecContext;
    AVStream* m_videoStream;
    int m_videoStreamIndex;
    SwsContext* m_swsContext;

    // Аудио
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;
    int m_audioStreamIndex;
    SwrContext* m_swrContext;

    // Временные буферы
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    AVPacket* m_packet;

    // Вспомогательные функции
    bool initializeVideo();
    bool initializeAudio();
    bool initializeSwrContext();
    QImage avFrameToQImage(AVFrame* frame);
    void freeResources();
};

#endif // MEDIADECODER_H


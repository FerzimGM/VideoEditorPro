#ifndef MEDIAENCODER_H
#define MEDIAENCODER_H

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
 * MediaEncoder — универсальный энкодер видео + аудио.
 *
 * Поддерживает:
 * - Видео: H.264 (libx264) → YUV420P
 * - Аудио: AAC → float planar
 * - Контейнеры: MP4, AVI, MOV, MKV (автоопределение по расширению)
 * - Настраиваемые разрешение, битрейт, FPS
 *
 * Порядок вызова:
 *   1. createOutputFile(path, w, h)
 *   2. writeVideoFrame(image) + writeAudioSamples(samples) в цикле
 *   3. finish()
 */
class MediaEncoder : public QObject
{
    Q_OBJECT

public:
    explicit MediaEncoder(QObject *parent = nullptr);
    ~MediaEncoder();

    // ===== СОЗДАНИЕ ФАЙЛА =====
    // Автоопределяет контейнер по расширению файла
    bool createOutputFile(const QString& filepath, int width, int height);

    // ===== НАСТРОЙКИ (вызывать ДО createOutputFile) =====
    void setCodec(const QString& codecName);
    void setFormat(const QString& format);
    void setBitrate(int bitrate);
    void setFrameRate(double fps);
    void setPixelFormat(const QString& format);
    void setAudioEnabled(bool enabled) { m_audioEnabled = enabled; }

    // ===== ЗАПИСЬ =====
    // Записать один видеокадр (QImage → RGB → YUV420P → H.264)
    bool writeVideoFrame(const QImage& frame);

    // Записать аудио сэмплы (interleaved float, stereo, 44100Hz)
    // Внутренне буферизует до размера AAC-фрейма (1024 сэмпла)
    bool writeAudioSamples(const QVector<float>& samples);

    // ===== ЗАВЕРШЕНИЕ =====
    bool finish();

    // ===== СОСТОЯНИЕ =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getOutputPath() const { return m_outputPath; }

    // ===== КОНСТАНТЫ =====
    static const int AUDIO_SAMPLE_RATE = 44100;
    static const int AUDIO_CHANNELS    = 2;

signals:
    void error(const QString& message);

private:
    QString m_outputPath;

    // FFmpeg контексты
    AVFormatContext* m_formatContext;

    // Видео
    AVCodecContext*  m_videoCodecContext;
    AVStream*       m_videoStream;
    const AVCodec*  m_videoCodec;
    SwsContext*     m_swsContext;   // RGB → YUV

    // Аудио
    AVCodecContext*  m_audioCodecContext;
    AVStream*       m_audioStream;
    const AVCodec*  m_audioCodec;
    SwrContext*     m_swrContext;   // float interleaved → codec format

    // Параметры
    int    m_width;
    int    m_height;
    double m_fps;
    int    m_bitrate;
    bool   m_audioEnabled;

    // Счётчики
    int64_t m_videoFrameCount;
    int64_t m_audioSampleCount;  // Общее число записанных аудио-сэмплов

    // Видео буферы
    AVFrame*  m_videoFrame;     // YUV420P frame для кодирования
    AVPacket* m_videoPacket;

    // Аудио буферы
    AVFrame*  m_audioFrame;     // Аудио фрейм для кодирования
    AVPacket* m_audioPacket;
    QVector<float> m_audioBuffer;  // Буфер накопления сэмплов до frame_size

    // Инициализация
    bool initializeVideo();
    bool initializeAudio();

    // Запись буферизованного аудио
    bool flushAudioBuffer();
    bool encodeAudioFrame(AVFrame* frame);

    void freeResources();
};

#endif // MEDIAENCODER_H

#ifndef MEDIAENCODER_H
#define MEDIAENCODER_H

#include <QObject>
#include <QString>
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

/**
 * MediaEncoder - класс для кодирования и записи видео
 *
 * Ответственность:
 * - Создание выходного файла
 * - Настройка кодека (H.264, H.265, etc.)
 * - Настройка формата (MP4, AVI, MOV)
 * - Настройка разрешения
 * - Кодирование кадров
 * - Запись аудио
 * - Финализация файла
 */
class MediaEncoder : public QObject
{
    Q_OBJECT

public:
    explicit MediaEncoder(QObject *parent = nullptr);
    ~MediaEncoder();

    // ===== СОЗДАНИЕ ВЫХОДНОГО ФАЙЛА =====
    bool createOutputFile(const QString& filepath, int width, int height);

    // ===== НАСТРОЙКИ =====
    void setCodec(const QString& codecName);    // "h264", "h265", "mpeg4"
    void setFormat(const QString& format);       // "mp4", "avi", "mov"
    void setBitrate(int bitrate);                // В bps
    void setFrameRate(double fps);
    void setPixelFormat(const QString& format); // "yuv420p", "rgb24"

    // ===== ЗАПИСЬ =====
    bool writeVideoFrame(const QImage& frame);
    bool writeAudioSamples(const QByteArray& samples);

    // ===== ЗАВЕРШЕНИЕ =====
    bool finish();  // Финализировать файл

    // ===== СОСТОЯНИЕ =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getOutputPath() const { return m_outputPath; }

signals:
    void error(const QString& message);

private:
    QString m_outputPath;

    // FFmpeg контексты
    AVFormatContext* m_formatContext;

    // Видео
    AVCodecContext* m_videoCodecContext;
    AVStream* m_videoStream;
    const AVCodec* m_videoCodec;
    SwsContext* m_swsContext;  // Для конвертации RGB → YUV

    // Аудио
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;

    // Параметры
    int m_width;
    int m_height;
    double m_fps;
    int m_bitrate;
    int64_t m_frameCount;

    // Временные буферы
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    AVPacket* m_packet;

    // Вспомогательные методы
    bool initializeVideo();
    bool initializeAudio();
    QImage qImageToAVFrame(const QImage& image, AVFrame* frame);
    void freeResources();
};

#endif // MEDIAENCODER_H

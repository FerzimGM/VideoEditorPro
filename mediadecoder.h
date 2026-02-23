#ifndef MEDIADECODER_H
#define MEDIADECODER_H

#include <QObject>
#include <QString>
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}

/**
 * MediaDecoder - класс для декодирования видео и аудио
 *
 * Ответственность:
 * - Открывает видеофайл
 * - Декодирует видео кадры
 * - Декодирует аудио сэмплы
 * - Конвертирует форматы
 */
class MediaDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MediaDecoder(QObject *parent = nullptr);
    ~MediaDecoder();

    // ===== ОТКРЫТИЕ ФАЙЛА =====
    bool openFile(const QString& filepath);
    void closeFile();

    // ===== ИНФОРМАЦИЯ О ФАЙЛЕ =====
    double getDuration() const;  // Длительность в секундах
    int getVideoWidth() const;
    int getVideoHeight() const;
    double getFrameRate() const;
    int getAudioSampleRate() const;
    int getAudioChannels() const;

    // ===== ДЕКОДИРОВАНИЕ ВИДЕО =====
    // Получить кадр в указанное время
    QImage getFrameAt(double timestamp);

    // Получить следующий кадр (для последовательного чтения)
    QImage getNextFrame();

    // ===== ДЕКОДИРОВАНИЕ АУДИО =====
    // Получить аудио сэмплы от startTime до endTime
    QByteArray getAudioSamples(double startTime, double endTime);

    // ===== SEEK (ПЕРЕМОТКА) =====
    bool seekTo(double timestamp);

    // ===== СОСТОЯНИЕ =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getFilepath() const { return m_filepath; }

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
    SwsContext* m_swsContext;  // Для конвертации форматов

    // Аудио
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;
    int m_audioStreamIndex;
    SwrContext* m_swrContext;  // Для resample аудио

    // Временные буферы
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    AVPacket* m_packet;

    // Вспомогательные функции
    bool initializeVideo();
    bool initializeAudio();
    QImage avFrameToQImage(AVFrame* frame);
    void freeResources();
};

#endif // MEDIADECODER_H

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
 * MediaEncoder — a general-purpose video + audio encoder built on FFmpeg.
 *
 * Supports:
 * - Video: H.264 (libx264) -> YUV420P
 * - Audio: AAC -> planar float
 * - Containers: MP4, AVI, MOV, MKV (auto-detected from the file extension)
 * - Configurable resolution, bitrate, and fps
 *
 * Call order:
 *   1. createOutputFile(path, w, h)
 *   2. writeVideoFrame(image) + writeAudioSamples(samples) in a loop
 *   3. finish()
 */
class MediaEncoder : public QObject
{
    Q_OBJECT

public:
    explicit MediaEncoder(QObject *parent = nullptr);
    ~MediaEncoder();

    // ===== FILE CREATION =====
    // Auto-detects the container from the file extension.
    bool createOutputFile(const QString& filepath, int width, int height);

    // ===== SETTINGS (call before createOutputFile) =====
    void setCodec(const QString& codecName);
    void setFormat(const QString& format);
    void setBitrate(int bitrate);
    void setFrameRate(double fps);
    void setPixelFormat(const QString& format);
    void setAudioEnabled(bool enabled) { m_audioEnabled = enabled; }

    // ===== WRITING =====
    // Writes one video frame (QImage -> RGB -> YUV420P -> H.264).
    bool writeVideoFrame(const QImage& frame);

    // Writes interleaved float stereo samples at 44100Hz.
    // Internally buffers input until a full AAC frame (1024 samples) is
    // available.
    bool writeAudioSamples(const QVector<float>& samples);

    // ===== FINALIZATION =====
    bool finish();

    // ===== STATE =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getOutputPath() const { return m_outputPath; }

    // ===== CONSTANTS =====
    static const int AUDIO_SAMPLE_RATE = 44100;
    static const int AUDIO_CHANNELS    = 2;

signals:
    void error(const QString& message);

private:
    QString m_outputPath;

    // FFmpeg contexts
    AVFormatContext* m_formatContext;

    // Video
    AVCodecContext* m_videoCodecContext;
    AVStream* m_videoStream;
    const AVCodec* m_videoCodec;
    SwsContext* m_swsContext;   // RGB -> YUV

    // Audio
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;
    const AVCodec* m_audioCodec;
    SwrContext* m_swrContext;   // interleaved float -> codec's native format

    // Parameters
    QString m_filepath;
    int m_width;
    int m_height;
    double m_fps;
    int m_bitrate;
    bool m_audioEnabled;
    bool m_headerWritten = false;
    QString m_usedFormat; // May differ from m_format after a container fallback
    QString m_format;     // "MP4", "AVI", "MOV", "MKV", "WebM"

    // Counters
    int64_t m_videoFrameCount;
    int64_t m_audioSampleCount;  // Total audio samples written so far

    // Video buffers
    AVFrame*  m_videoFrame;     // YUV420P frame ready for encoding
    AVPacket* m_videoPacket;

    // Audio buffers
    AVFrame*  m_audioFrame;     // Audio frame ready for encoding
    AVPacket* m_audioPacket;
    QVector<float> m_audioBuffer;  // Accumulates samples until frame_size is reached

    // Initialization
    bool initializeVideo();
    bool initializeAudio();

    // Buffered audio flush
    bool flushAudioBuffer();
    bool encodeAudioFrame(AVFrame* frame);

    void freeResources();
};

#endif // MEDIAENCODER_H


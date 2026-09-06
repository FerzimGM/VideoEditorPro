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
#include <libavutil/hwcontext.h>  // Hardware decoding: AVHWDeviceContext
}

/**
 * MediaDecoder — a general-purpose video/audio decoder built on FFmpeg.
 *
 * Supports:
 * - Any container/codec combination FFmpeg can read (mp4, avi, mov, mkv, webm, flv...)
 * - Video frame decoding to QImage
 * - Audio decoding to interleaved float PCM, 44100Hz stereo
 * - Seeking by timestamp
 * - Sequential reads (used for efficient rendering/export)
 *
 * Optimizations:
 * - setPreviewMode(true): sws_scale outputs frames at half resolution,
 *   cutting CPU load roughly 4x. Not used for file export.
 * - GPU decoding: initializeVideo() tries D3D11VA -> DXVA2 -> CPU in order,
 *   falling back automatically whenever a GPU path is unavailable.
 */
class MediaDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MediaDecoder(QObject *parent = nullptr);
    ~MediaDecoder();

    // ===== OPEN / CLOSE =====
    bool openFile(const QString& filepath);
    void closeFile();

    // ===== PREVIEW MODE (half resolution) =====
    // Only enable for DecoderThread (live playback). RenderWorker always
    // runs at full resolution.
    void setPreviewMode(bool enabled) { m_previewMode = enabled; }
    bool isPreviewMode() const { return m_previewMode; }

    // ===== FILE INFO =====
    double getDuration() const;
    int getVideoWidth() const;
    int getVideoHeight() const;
    double getFrameRate() const;
    int getAudioSampleRate() const;
    int getAudioChannels() const;
    bool hasVideo() const { return m_videoStreamIndex >= 0; }
    bool hasAudio() const { return m_audioStreamIndex >= 0; }

    // ===== VIDEO DECODING =====
    QImage getFrameAt(double timestamp);
    QImage seekAndDecode(double timestamp);
    QImage getNextFrame();
    double getLastVideoPts() const { return m_lastVideoPts; }

    // ===== AUDIO DECODING =====
    QVector<float> decodeAudioRange(double startTime, double duration);

    // ===== SEEK =====
    bool seekTo(double timestamp);

    // ===== STATE =====
    bool isOpen() const { return m_formatContext != nullptr; }
    QString getFilepath() const { return m_filepath; }
    bool isUsingGPU() const { return m_hwDeviceCtx != nullptr; }

    // ===== OUTPUT AUDIO CONSTANTS =====
    static const int OUTPUT_SAMPLE_RATE = 44100;
    static const int OUTPUT_CHANNELS    = 2;

signals:
    void error(const QString& message);

private:
    QString m_filepath;

    // ===== PREVIEW MODE =====
    // When true, sws_scale downsamples frames to width/2 x height/2.
    // QML stretches the result to full display size — the difference is
    // barely visible, but there's 4x fewer pixels to push through the
    // pipeline, so CPU load drops accordingly.
    bool m_previewMode = false;

    // FFmpeg contexts
    AVFormatContext* m_formatContext;

    // Video
    AVCodecContext* m_videoCodecContext;
    AVStream* m_videoStream;
    int m_videoStreamIndex;
    SwsContext* m_swsContext;

    // GPU decoding context.
    // nullptr if the GPU path is unavailable or unsupported — decoding
    // falls back to CPU automatically in that case.
    // Type: D3D11VA (Windows 8+) or DXVA2 (Windows 7+).
    AVBufferRef* m_hwDeviceCtx = nullptr;
    // Tracks which hardware decoder type is active so frames are
    // transferred correctly.
    AVHWDeviceType m_hwDeviceType = AV_HWDEVICE_TYPE_NONE;
    // Pixel format of the GPU frame before it's copied into system RAM.
    AVPixelFormat m_hwPixFmt = AV_PIX_FMT_NONE;

    // Audio
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;
    int m_audioStreamIndex;
    SwrContext* m_swrContext;

    // Scratch buffers
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    AVPacket* m_packet;

    // Position of the last sequentially decoded audio sample
    double m_lastAudioPos;

    // PTS of the last decoded video frame
    double m_lastVideoPts = -1.0;

    // AAC overflow buffer (see decodeAudioRange for details)
    QVector<float> m_audioOverflow;
    bool m_skipDone = false;

    // Cached SwsContext parameters, used to detect when the context
    // needs to be rebuilt.
    AVPixelFormat m_cachedSwsFmt;
    int m_cachedSwsW;
    int m_cachedSwsH;
    // Target sws output size (depends on m_previewMode)
    int m_cachedDstW = 0;
    int m_cachedDstH = 0;

    // Helpers
    bool initializeVideo();
    bool tryInitHardwareDecoder(const AVCodec* codec); // GPU fallback chain
    bool initializeAudio();
    bool initializeSwrContext();
    QImage avFrameToQImage(AVFrame* frame);
    void freeResources();
};

#endif // MEDIADECODER_H



#include "mediadecoder.h"
#include <QDebug>

MediaDecoder::MediaDecoder(QObject *parent)
    : QObject(parent)
    , m_formatContext(nullptr)
    , m_videoCodecContext(nullptr)
    , m_videoStream(nullptr)
    , m_videoStreamIndex(-1)
    , m_swsContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_audioStream(nullptr)
    , m_audioStreamIndex(-1)
    , m_swrContext(nullptr)
    , m_frame(nullptr)
    , m_rgbFrame(nullptr)
    , m_packet(nullptr)
    , m_lastAudioPos(-1.0)
    , m_cachedSwsFmt(AV_PIX_FMT_NONE)
    , m_cachedSwsW(0)
    , m_cachedSwsH(0)
{}

MediaDecoder::~MediaDecoder()
{
    closeFile();
}

// ===== OPEN FILE =====
bool MediaDecoder::openFile(const QString& filepath)
{
    closeFile();
    m_filepath = filepath;

    // 1. Open the file.
    if (avformat_open_input(&m_formatContext, filepath.toUtf8().constData(),
                            nullptr, nullptr) != 0)
    {
        qWarning() << "Failed to open file:" << filepath;
        emit error("Failed to open file");
        return false;
    }

    // 2. Read stream information.
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
    {
        qWarning() << "Failed to read stream info";
        emit error("Failed to read stream info");
        closeFile();
        return false;
    }

    // 3. Initialize video and audio streams.
    bool videoOk = initializeVideo();
    bool audioOk = initializeAudio();

    if (!videoOk && !audioOk)
    {
        qWarning() << "No video or audio streams found";
        emit error("No video or audio streams found");
        closeFile();
        return false;
    }

    // 4. Allocate scratch buffers.
    m_frame    = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_packet   = av_packet_alloc();

    if (!m_frame || !m_rgbFrame || !m_packet)
    {
        qWarning() << "Failed to allocate buffers";
        emit error("Failed to allocate buffers");
        closeFile();
        return false;
    }

    // 5. Initialize the audio resampler, if an audio stream is present.
    if (audioOk)
    {
        initializeSwrContext();
    }

#ifndef QT_NO_DEBUG
    qDebug() << "File opened:" << filepath
             << "V:" << videoOk << "A:" << audioOk
             << "Dur:" << getDuration() << "s";
#endif

    return true;
}

// ===== CLOSE FILE =====
void MediaDecoder::closeFile()
{
    freeResources();

    if (m_swrContext)
    {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
    }
    if (m_swsContext)
    {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    if (m_videoCodecContext)
    {
        avcodec_free_context(&m_videoCodecContext);
    }
    if (m_audioCodecContext)
    {
        avcodec_free_context(&m_audioCodecContext);
    }
    if (m_formatContext)
    {
        avformat_close_input(&m_formatContext);
    }
    // Release the hardware decoding context after the codec is closed.
    if (m_hwDeviceCtx)
    {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
        m_hwDeviceType = AV_HWDEVICE_TYPE_NONE;
        m_hwPixFmt = AV_PIX_FMT_NONE;
    }

    m_videoStream = nullptr;
    m_audioStream = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_lastAudioPos = -1.0;
    m_lastVideoPts = -1.0;
    m_audioOverflow.clear();
    m_skipDone = false;
    m_cachedSwsFmt = AV_PIX_FMT_NONE;
    m_cachedSwsW = 0;
    m_cachedSwsH = 0;
    m_cachedDstW = 0;
    m_cachedDstH = 0;
}

// ===== VIDEO INITIALIZATION =====
// Hardware decoding fallback chain (Windows only):
//   1. D3D11VA — DirectX 11, Windows 8+, all modern GPUs
//   2. DXVA2   — DirectX 9, Windows 7+, older GPUs
//   3. CPU     — always available, final fallback
//
// If hardware decoding is unavailable (old driver, VM, no GPU), CPU
// decoding is used automatically and playback continues normally.
bool MediaDecoder::initializeVideo()
{
    m_videoStreamIndex = av_find_best_stream(
        m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (m_videoStreamIndex < 0) return false;

    m_videoStream = m_formatContext->streams[m_videoStreamIndex];

    const AVCodec* codec = avcodec_find_decoder(
        m_videoStream->codecpar->codec_id);
    if (!codec) return false;

    // Try hardware decoding first (skip for audio-only files).
    if (m_videoStream->codecpar->width > 0)
    {
        if (tryInitHardwareDecoder(codec))
        {
#ifndef QT_NO_DEBUG
            qDebug() << "GPU decoder active:"
                     << av_hwdevice_get_type_name(m_hwDeviceType);
#endif
            return true;
        }
    }

    // Fallback: CPU decoding (the standard path).
    m_videoCodecContext = avcodec_alloc_context3(codec);
    if (!m_videoCodecContext) return false;

    if (avcodec_parameters_to_context(m_videoCodecContext,
                                      m_videoStream->codecpar) < 0)
    {
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

    // Multi-threaded CPU decoding.
    m_videoCodecContext->thread_count = 4;

    if (avcodec_open2(m_videoCodecContext, codec, nullptr) < 0)
    {
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

#ifndef QT_NO_DEBUG
    qDebug() << "CPU decoder (software):" << codec->name;
#endif
    return true;
}

// ===== HARDWARE DECODING: initialization attempt =====
// Returns true if a GPU decoder was created successfully. On any failure
// all resources are released and false is returned, so the caller can
// fall back to CPU decoding.
//
// How it works:
// FFmpeg represents the GPU and its decoding context as an
// AVHWDeviceContext ("hardware device context"). av_hwdevice_ctx_create()
// creates it; FFmpeg handles the underlying DirectX interop internally.
//
// Once the context exists, we look up the GPU frame's pixel format
// (m_hwPixFmt). Decoded frames live in GPU memory — avFrameToQImage()
// copies them into system RAM via av_hwframe_transfer_data() before
// building a QImage.
bool MediaDecoder::tryInitHardwareDecoder(const AVCodec* codec)
{
    // GPU decoder types in priority order (Windows only).
    static const AVHWDeviceType hwTypes[] = {
        AV_HWDEVICE_TYPE_D3D11VA,  // DirectX 11, Windows 8+
        AV_HWDEVICE_TYPE_DXVA2,    // DirectX 9,  Windows 7+
        AV_HWDEVICE_TYPE_NONE      // sentinel
    };

    for (int i = 0; hwTypes[i] != AV_HWDEVICE_TYPE_NONE; ++i)
    {
        AVHWDeviceType hwType = hwTypes[i];

        // Check whether this codec supports the given hardware type.
        AVPixelFormat hwFmt = AV_PIX_FMT_NONE;
        for (int cfg = 0; ; ++cfg)
        {
            const AVCodecHWConfig* config = avcodec_get_hw_config(codec, cfg);
            if (!config) break;
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX
                && config->device_type == hwType)
            {
                hwFmt = config->pix_fmt;
                break;
            }
        }
        if (hwFmt == AV_PIX_FMT_NONE) continue; // Codec doesn't support this device type.

        // Create the hardware device context.
        AVBufferRef* hwCtx = nullptr;
        if (av_hwdevice_ctx_create(&hwCtx, hwType, nullptr, nullptr, 0) < 0)
            continue; // Device unavailable — try the next type.

        // Create an AVCodecContext bound to the hardware context.
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) { av_buffer_unref(&hwCtx); continue; }

        if (avcodec_parameters_to_context(codecCtx, m_videoStream->codecpar) < 0)
        {
            avcodec_free_context(&codecCtx);
            av_buffer_unref(&hwCtx);
            continue;
        }

        codecCtx->hw_device_ctx = av_buffer_ref(hwCtx);
        codecCtx->thread_count  = 1; // Decoding runs on the GPU; CPU threads aren't needed.

        if (avcodec_open2(codecCtx, codec, nullptr) < 0)
        {
            avcodec_free_context(&codecCtx);
            av_buffer_unref(&hwCtx);
            continue;
        }

        // Success — keep the context.
        m_videoCodecContext = codecCtx;
        m_hwDeviceCtx       = hwCtx;
        m_hwDeviceType      = hwType;
        m_hwPixFmt          = hwFmt;
        return true;
    }

    return false; // Every hardware option failed — caller falls back to CPU.
}

// ===== AUDIO INITIALIZATION =====
bool MediaDecoder::initializeAudio()
{
    m_audioStreamIndex = av_find_best_stream(
        m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (m_audioStreamIndex < 0) return false;

    m_audioStream = m_formatContext->streams[m_audioStreamIndex];

    const AVCodec* codec = avcodec_find_decoder(
        m_audioStream->codecpar->codec_id);
    if (!codec) return false;

    m_audioCodecContext = avcodec_alloc_context3(codec);
    if (!m_audioCodecContext) return false;

    if (avcodec_parameters_to_context(m_audioCodecContext,
                                      m_audioStream->codecpar) < 0)
    {
        avcodec_free_context(&m_audioCodecContext);
        return false;
    }

    if (avcodec_open2(m_audioCodecContext, codec, nullptr) < 0)
    {
        avcodec_free_context(&m_audioCodecContext);
        return false;
    }

    return true;
}

// ===== AUDIO RESAMPLER INITIALIZATION =====
// Converts whatever format the source uses to interleaved float, 44100Hz, stereo.
bool MediaDecoder::initializeSwrContext()
{
    if (!m_audioCodecContext) return false;

    // Output layout: stereo.
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, OUTPUT_CHANNELS);

    // Input layout, as reported by the source file.
    AVChannelLayout inLayout;
    av_channel_layout_copy(&inLayout, &m_audioCodecContext->ch_layout);

    int ret = swr_alloc_set_opts2(
        &m_swrContext,
        &outLayout,                              // output: stereo
        AV_SAMPLE_FMT_FLT,                       // output: float
        OUTPUT_SAMPLE_RATE,                       // output: 44100
        &inLayout,                                // input: from the file
        m_audioCodecContext->sample_fmt,           // input: source sample format
        m_audioCodecContext->sample_rate,           // input: source sample rate
        0, nullptr
        );

    av_channel_layout_uninit(&outLayout);
    av_channel_layout_uninit(&inLayout);

    if (ret < 0 || !m_swrContext)
    {
        qWarning() << "Failed to create SwrContext";
        return false;
    }

    if (swr_init(m_swrContext) < 0)
    {
        qWarning() << "Failed to initialize SwrContext";
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
        return false;
    }

    return true;
}

QImage MediaDecoder::getFrameAt(double timestamp)
{
    if (!m_videoCodecContext || !m_videoStream) return QImage();
    if (!seekTo(timestamp)) return QImage();

    double timeBase = av_q2d(m_videoStream->time_base);
    double fps = getFrameRate();
    double frameDur = (fps > 0) ? (1.0 / fps) : 0.04;
    QImage lastGood;

    for (int decoded = 0; decoded < 300; ++decoded)
    {
        if (av_read_frame(m_formatContext, m_packet) < 0) break;
        if (m_packet->stream_index != m_videoStreamIndex)
        {
            av_packet_unref(m_packet); continue;
        }
        int ret = avcodec_send_packet(m_videoCodecContext, m_packet);
        av_packet_unref(m_packet);
        if (ret < 0) continue;

        ret = avcodec_receive_frame(m_videoCodecContext, m_frame);
        if (ret != 0) continue;

        double pts = 0.0;
        if (m_frame->best_effort_timestamp != AV_NOPTS_VALUE)
            pts = m_frame->best_effort_timestamp * timeBase;
        else if (m_frame->pts != AV_NOPTS_VALUE)
            pts = m_frame->pts * timeBase;

        QImage img = avFrameToQImage(m_frame);
        if (!img.isNull()) lastGood = img;
        if (pts >= timestamp - frameDur * 0.5) return lastGood;
    }
    return lastGood;
}

// ===== FAST SEEK + DECODE =====
// Similar to getFrameAt(), but skips avFrameToQImage() for intermediate
// frames:
//   getFrameAt():    seek -> decode+convert every frame (5ms x 150 =~ 750ms
//                     for a 5s keyframe interval)
//   seekAndDecode():  seek -> decode every frame (1ms) -> convert only the
//                     target frame (=~ 160ms)
// Used by DecoderThread for fast seeking without stalling playback.
QImage MediaDecoder::seekAndDecode(double timestamp)
{
    if (!m_videoCodecContext || !m_videoStream) return QImage();
    if (!seekTo(timestamp)) return QImage();

    double timeBase = av_q2d(m_videoStream->time_base);
    double fps = getFrameRate();
    double frameDur = (fps > 0) ? (1.0 / fps) : 0.04;

    for (int decoded = 0; decoded < 300; ++decoded)
    {
        if (av_read_frame(m_formatContext, m_packet) < 0) break;
        if (m_packet->stream_index != m_videoStreamIndex)
        {
            av_packet_unref(m_packet); continue;
        }
        int ret = avcodec_send_packet(m_videoCodecContext, m_packet);
        av_packet_unref(m_packet);
        if (ret < 0) continue;

        ret = avcodec_receive_frame(m_videoCodecContext, m_frame);
        if (ret != 0) continue;

        double pts = 0.0;
        if (m_frame->best_effort_timestamp != AV_NOPTS_VALUE)
            pts = m_frame->best_effort_timestamp * timeBase;
        else if (m_frame->pts != AV_NOPTS_VALUE)
            pts = m_frame->pts * timeBase;

        if (pts >= timestamp - frameDur * 0.5)
        {
            // Target frame found — convert only this one.
            return avFrameToQImage(m_frame);
        }
        // Intermediate frame — skip without sws_scale/QImage conversion.
    }
    return QImage();
}

// ===== GET NEXT VIDEO FRAME =====
QImage MediaDecoder::getNextFrame() {
    if (!m_videoCodecContext) return QImage();

    double timeBase = m_videoStream ? av_q2d(m_videoStream->time_base) : 0.0;

    while (av_read_frame(m_formatContext, m_packet) >= 0)
    {
        if (m_packet->stream_index != m_videoStreamIndex)
        {
            av_packet_unref(m_packet);
            continue;
        }

        int ret = avcodec_send_packet(m_videoCodecContext, m_packet);
        av_packet_unref(m_packet);

        if (ret < 0) continue;

        ret = avcodec_receive_frame(m_videoCodecContext, m_frame);
        if (ret == 0)
        {
            // Track PTS for render-time A/V synchronization.
            if (timeBase > 0.0 && m_frame->best_effort_timestamp != AV_NOPTS_VALUE)
                m_lastVideoPts = m_frame->best_effort_timestamp * timeBase;
            else if (timeBase > 0.0 && m_frame->pts != AV_NOPTS_VALUE)
                m_lastVideoPts = m_frame->pts * timeBase;

            return avFrameToQImage(m_frame);
        }
    }

    return QImage();
}

// ===== AVFrame -> QImage CONVERSION =====
// Two paths:
//   CPU frame: goes straight into sws_scale.
//   GPU frame: av_hwframe_transfer_data() copies GPU RAM -> CPU RAM first,
//              then sws_scale runs as usual.
//
// Preview mode (m_previewMode == true):
//   sws_scale downsamples the frame in a single pass. QML then stretches
//   the smaller QImage to fill the display (PreserveAspectFit). Full
//   resolution (m_previewMode == false) is used for file export.
QImage MediaDecoder::avFrameToQImage(AVFrame* frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) return QImage();

    AVFrame* swFrame = frame;
    AVFrame* transferred = nullptr;

    // GPU -> CPU: if the frame currently lives in GPU video memory.
    if (frame->format == m_hwPixFmt && m_hwDeviceCtx)
    {
        transferred = av_frame_alloc();
        if (!transferred) return QImage();

        // Copy the frame from GPU RAM into system RAM. Afterwards,
        // transferred->format is AV_PIX_FMT_NV12 or YUV420P (depends on
        // the GPU driver) — sws_scale handles both.
        if (av_hwframe_transfer_data(transferred, frame, 0) < 0)
        {
            av_frame_free(&transferred);
            return QImage();
        }
        transferred->width  = frame->width;
        transferred->height = frame->height;
        swFrame = transferred;
    }

    int srcWidth  = swFrame->width;
    int srcHeight = swFrame->height;
    AVPixelFormat srcFmt = (AVPixelFormat)swFrame->format;

    // Target size: 3/4 scale in preview mode (a good balance of CPU cost
    // vs. quality), full size otherwise. A 1/2 (quarter-pixel-count)
    // scale was tried first but produced a visibly softer image; 3/4
    // still cuts pixel count by about 1.8x with almost no perceptible
    // quality loss.
    int dstWidth  = m_previewMode ? (srcWidth  * 3 / 4) : srcWidth;
    int dstHeight = m_previewMode ? (srcHeight * 3 / 4) : srcHeight;
    // Round down to an even size — sws_scale requires even dimensions for YUV.
    dstWidth  = (dstWidth  / 2) * 2;
    dstHeight = (dstHeight / 2) * 2;
    if (dstWidth  < 2) dstWidth  = 2;
    if (dstHeight < 2) dstHeight = 2;

    // Rebuild the SwsContext whenever the format, source size, or target
    // size (preview vs. full) changes.
    if (!m_swsContext
        || srcFmt   != m_cachedSwsFmt
        || srcWidth != m_cachedSwsW
        || srcHeight!= m_cachedSwsH
        || dstWidth != m_cachedDstW
        || dstHeight!= m_cachedDstH)
    {
        if (m_swsContext) { sws_freeContext(m_swsContext); m_swsContext = nullptr; }

        // SWS_BILINEAR is fast and good enough for preview playback.
        // SWS_LANCZOS would give sharper results when scaling but runs
        // 3-5x slower, which isn't worth it at 30fps.
        m_swsContext = sws_getContext(
            srcWidth, srcHeight, srcFmt,
            dstWidth, dstHeight, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!m_swsContext)
        {
            if (transferred) av_frame_free(&transferred);
            return QImage();
        }
        m_cachedSwsFmt = srcFmt;
        m_cachedSwsW   = srcWidth;
        m_cachedSwsH   = srcHeight;
        m_cachedDstW   = dstWidth;
        m_cachedDstH   = dstHeight;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, dstWidth, dstHeight, 32);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!buffer)
    {
        if (transferred) av_frame_free(&transferred);
        return QImage();
    }

    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize,
                         buffer, AV_PIX_FMT_RGB24, dstWidth, dstHeight, 32);

    sws_scale(m_swsContext,
              (const uint8_t* const*)swFrame->data, swFrame->linesize,
              0, srcHeight, m_rgbFrame->data, m_rgbFrame->linesize);

    QImage image(m_rgbFrame->data[0], dstWidth, dstHeight,
                 m_rgbFrame->linesize[0], QImage::Format_RGB888);
    QImage result = image.copy();

    av_free(buffer);
    if (transferred) av_frame_free(&transferred);
    return result;
}

// ===== DECODE AN AUDIO RANGE =====
// Returns interleaved float PCM: [L0, R0, L1, R1, ...], always at
// OUTPUT_SAMPLE_RATE (44100) / OUTPUT_CHANNELS (2).
QVector<float> MediaDecoder::decodeAudioRange(double startTime, double duration)
{
    if (!m_audioCodecContext || !m_swrContext || !m_formatContext)
        return QVector<float>();

    // Round rather than truncate the sample count. Truncating
    // (int)(duration * 44100) drops a sample whenever the division isn't
    // exact — e.g. duration = 1470/44100 = 0.0333...s truncates to 1469
    // samples instead of 1470. Over ~1800 frames of playback that adds
    // up to a ~0.04s drift plus small gaps, audible as crackling.
    int totalSamples = static_cast<int>(duration * OUTPUT_SAMPLE_RATE + 0.5);
    int totalFloats  = totalSamples * OUTPUT_CHANNELS;

    // Only reseek on an actual position jump. The forward threshold is a
    // flat 1.5s rather than duration*8: with ~20ms chunks, duration*8 is
    // only 160ms, which is too tight and triggered a reseek at almost
    // every clip boundary. 1.5s tolerates normal pauses without forcing
    // an unnecessary seek.
    double fwdThreshold = qMax(1.5, duration * 3.0);
    bool needSeek = (m_lastAudioPos < 0.0)                         ||
                    (startTime < m_lastAudioPos - 0.02)             ||
                    (startTime > m_lastAudioPos + fwdThreshold);

    if (needSeek)
    {
        m_audioOverflow.clear();
        m_skipDone = false;

        int64_t t = static_cast<int64_t>(startTime * AV_TIME_BASE);
        av_seek_frame(m_formatContext, -1, t, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(m_audioCodecContext);

        // Properly drain the resampler's internal buffer.
        // swr_convert(ctx, nullptr, 0, nullptr, 0) is a no-op; a real
        // drain call needs a valid output buffer with a null input:
        // swr_convert(ctx, &outbuf, N, NULL, 0). Without this step the
        // resampler retains up to ~23ms of audio from the previous
        // position, which then leaks into the start of the new chunk as
        // audible crackling.
        {
            uint8_t* drainBuf = nullptr;
            int drainMax = 4096;
            av_samples_alloc(&drainBuf, nullptr,
                             OUTPUT_CHANNELS, drainMax, AV_SAMPLE_FMT_FLT, 0);
            if (drainBuf)
            {
                // Drain until the resampler's internal buffer is empty.
                while (swr_convert(m_swrContext, &drainBuf, drainMax, nullptr, 0) > 0) {}
                av_freep(&drainBuf);
            }
        }

        m_lastAudioPos = startTime;
    }
    else if (!m_audioOverflow.isEmpty() &&
             qAbs(startTime - m_lastAudioPos) > 0.015)
    {
        // Leftover audio from the previous call doesn't line up with the
        // current request — discard it, otherwise audio from the wrong
        // position would end up at the start of this chunk.
        m_audioOverflow.clear();
    }

    // ── Result = leftover from the previous call + newly decoded samples ──
    QVector<float> result;
    result.reserve(totalFloats + 4096);

    // Start with whatever was carried over from the last call.
    if (!m_audioOverflow.isEmpty())
    {
        result = m_audioOverflow;
        m_audioOverflow.clear();
    }

    // ── Read new packets until we have enough samples ─────────────────────
    AVFrame*  audioFrame = av_frame_alloc();
    AVPacket* pkt        = av_packet_alloc();

    while (result.size() < totalFloats)
    {
        if (av_read_frame(m_formatContext, pkt) < 0) break;

        if (pkt->stream_index != m_audioStreamIndex)
        {
            av_packet_unref(pkt);
            continue;
        }

        int ret = avcodec_send_packet(m_audioCodecContext, pkt);
        av_packet_unref(pkt);
        if (ret < 0) continue;

        while (avcodec_receive_frame(m_audioCodecContext, audioFrame) == 0)
        {
            // best_effort_timestamp is more reliable than pts alone in
            // the presence of B-frames.
            double frameTimeBase = av_q2d(m_audioStream->time_base);
            double framePts = -1.0;
            if (audioFrame->best_effort_timestamp != AV_NOPTS_VALUE)
                framePts = audioFrame->best_effort_timestamp * frameTimeBase;
            else if (audioFrame->pts != AV_NOPTS_VALUE)
                framePts = audioFrame->pts * frameTimeBase;

            double frameDur = (double)audioFrame->nb_samples
                              / m_audioCodecContext->sample_rate;

            // Skip frames that fall entirely before our target window.
            // A -0.001s tolerance (rather than -0.005s) keeps this
            // precise and avoids discarding samples we actually need.
            if (framePts >= 0.0 && startTime > 0.05)
            {
                double frameEnd = framePts + frameDur;
                if (frameEnd < startTime - 0.001)
                {
                    av_frame_unref(audioFrame);
                    continue;
                }
            }

            // Resample to the output format.
            int maxOut = swr_get_out_samples(m_swrContext, audioFrame->nb_samples);
            if (maxOut <= 0) maxOut = audioFrame->nb_samples * 2 + 256;

            uint8_t* outBuf   = nullptr;
            int      outLSize = 0;
            av_samples_alloc(&outBuf, &outLSize,
                             OUTPUT_CHANNELS, maxOut, AV_SAMPLE_FMT_FLT, 0);

            int converted = swr_convert(m_swrContext,
                                        &outBuf, maxOut,
                                        (const uint8_t**)audioFrame->data,
                                        audioFrame->nb_samples);

            if (converted > 0)
            {
                const float* p = reinterpret_cast<const float*>(outBuf);

                // ── Precise post-seek trim ────────────────────────────────
                // AVSEEK_FLAG_BACKWARD lands on the nearest video keyframe,
                // which can be 2-5s before startTime. We need to skip
                // every frame before startTime, not just the first one.
                //
                // m_skipDone only becomes true once we've actually taken
                // part of a frame. If an entire frame is skipped
                // (skipFloats == converted * channels), the next frame is
                // still before startTime and needs the same check.
                //
                // Without this: trimStart=25s, seek lands at 22s -> the
                // first frame (22.0s) is skipped whole, m_skipDone is set
                // true too early, and the next ~130 frames (22.0-25.0s)
                // get appended without skipping -> ~3s of unwanted audio
                // -> audible garbage at the clip boundary.
                int skipFloats = 0;
                if (!m_skipDone && framePts >= 0.0 && framePts < startTime - 0.001)
                {
                    double skipSec = startTime - framePts;
                    int skipSamples = static_cast<int>(skipSec * OUTPUT_SAMPLE_RATE + 0.5);
                    skipFloats = qMin(skipSamples * OUTPUT_CHANNELS,
                                      converted   * OUTPUT_CHANNELS);
                    // Only mark skipping as done once part of a frame was kept.
                    if (skipFloats < converted * OUTPUT_CHANNELS)
                        m_skipDone = true;
                    // Otherwise the whole frame was skipped, so the next
                    // one still needs to be checked.
                }

                for (int i = skipFloats; i < converted * OUTPUT_CHANNELS; ++i)
                    result.append(p[i]);
            }
            if (outBuf) av_freep(&outBuf);

            av_frame_unref(audioFrame);
        }
    }

    av_frame_free(&audioFrame);
    av_packet_free(&pkt);

    // ── Flush the resampler's remaining delayed samples ──
    {
        int delayed = swr_get_delay(m_swrContext, OUTPUT_SAMPLE_RATE);
        if (delayed > 0 && result.size() < totalFloats)
        {
            int maxOut = delayed + 256;
            uint8_t* outBuf   = nullptr;
            int      outLSize = 0;
            av_samples_alloc(&outBuf, &outLSize,
                             OUTPUT_CHANNELS, maxOut, AV_SAMPLE_FMT_FLT, 0);
            int flushed = swr_convert(m_swrContext, &outBuf, maxOut, nullptr, 0);
            if (flushed > 0)
            {
                const float* p = reinterpret_cast<const float*>(outBuf);
                for (int i = 0; i < flushed * OUTPUT_CHANNELS; ++i)
                    result.append(p[i]);
            }
            if (outBuf) av_freep(&outBuf);
        }
    }

    m_lastAudioPos = startTime + duration;

    // ── Micro fade-in after a seek, to smooth the discontinuity ───────────
    // A seek jumps to the nearest keyframe and the trim above discards
    // the leading samples, but the boundary between "last discarded
    // sample" and "first kept sample" can still have a sharp amplitude
    // jump, audible as a click. With trimmed clips (trimStart > 0), every
    // clip transition involves a seek, so without this fade those clicks
    // would repeat as an audible crackle. A ~3ms fade-in (132 samples x
    // 2 channels) removes the discontinuity.
    if (needSeek && !result.isEmpty())
    {
        const int FADE_SAMPLES = 132; // =~ 3ms at 44100Hz
        int fadeFloats = qMin(FADE_SAMPLES * OUTPUT_CHANNELS, result.size());
        for (int i = 0; i < fadeFloats; ++i)
        {
            float t = (float)i / (float)fadeFloats; // 0.0 -> 1.0
            result[i] *= t;
        }
    }

    // ── Keep any overflow for next time rather than discarding it ──
    if (result.size() > totalFloats) {
        m_audioOverflow = result.mid(totalFloats);
        result.resize(totalFloats);
    } else {
        // Pad with silence if we came up short (only happens near EOF).
        while (result.size() < totalFloats)
            result.append(0.0f);
    }

    return result;
}

// ===== SEEK =====
bool MediaDecoder::seekTo(double timestamp)
{
    if (!m_formatContext) return false;

    int64_t seekTarget = static_cast<int64_t>(timestamp * AV_TIME_BASE);

    if (av_seek_frame(m_formatContext, -1, seekTarget,
                      AVSEEK_FLAG_BACKWARD) < 0)
    {
        qWarning() << "Seek failed for" << timestamp;
        return false;
    }

    if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);
    if (m_audioCodecContext) avcodec_flush_buffers(m_audioCodecContext);

    return true;
}

// ===== FILE INFO =====
double MediaDecoder::getDuration() const
{
    if (!m_formatContext || m_formatContext->duration == AV_NOPTS_VALUE)
        return 0.0;
    return (double)m_formatContext->duration / AV_TIME_BASE;
}

int MediaDecoder::getVideoWidth() const
{
    return m_videoCodecContext ? m_videoCodecContext->width : 0;
}

int MediaDecoder::getVideoHeight() const
{
    return m_videoCodecContext ? m_videoCodecContext->height : 0;
}

double MediaDecoder::getFrameRate() const
{
    if (!m_videoStream) return 0.0;
    AVRational fps = m_videoStream->avg_frame_rate;
    return (fps.den == 0) ? 0.0 : (double)fps.num / fps.den;
}

int MediaDecoder::getAudioSampleRate() const
{
    return m_audioCodecContext ? m_audioCodecContext->sample_rate : 0;
}

int MediaDecoder::getAudioChannels() const
{
    return m_audioCodecContext ? m_audioCodecContext->ch_layout.nb_channels : 0;
}

// ===== RELEASE RESOURCES =====
void MediaDecoder::freeResources()
{
    if (m_frame) av_frame_free(&m_frame);
    if (m_rgbFrame) av_frame_free(&m_rgbFrame);
    if (m_packet) av_packet_free(&m_packet);
}

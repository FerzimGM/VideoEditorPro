#include "mediaencoder.h"
#include <QDebug>

MediaEncoder::MediaEncoder(QObject *parent)
    : QObject(parent)
    , m_formatContext(nullptr)
    , m_videoCodecContext(nullptr)
    , m_videoStream(nullptr)
    , m_videoCodec(nullptr)
    , m_swsContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_audioStream(nullptr)
    , m_audioCodec(nullptr)
    , m_swrContext(nullptr)
    , m_width(1920)
    , m_height(1080)
    , m_fps(30.0)
    , m_bitrate(10000000)
    , m_audioEnabled(true)
    , m_videoFrameCount(0)
    , m_audioSampleCount(0)
    , m_videoFrame(nullptr)
    , m_videoPacket(nullptr)
    , m_audioFrame(nullptr)
    , m_audioPacket(nullptr)
{}

MediaEncoder::~MediaEncoder()
{
    finish();
}

// ===== CREATE OUTPUT FILE =====
bool MediaEncoder::createOutputFile(const QString& filepath, int width, int height)
{
#ifndef QT_NO_DEBUG
    qDebug() << "MediaEncoder::createOutputFile:" << filepath
             << width << "x" << height;
#endif

    m_outputPath = filepath;
    m_width  = width;
    m_height = height;

    m_filepath  = filepath;
    m_usedFormat = m_format;

    // Resolve the format hint FFmpeg needs for the container. By default
    // FFmpeg would guess from the file extension, but we want explicit
    // control here.
    const char* fmtHint = nullptr;
    QString fmtLower = m_format.toLower();
    if (fmtLower == "mp4") fmtHint = "mp4";
    else if (fmtLower == "mkv") fmtHint = "matroska";
    else if (fmtLower == "avi") fmtHint = "avi";
    else if (fmtLower == "mov") fmtHint = "mov";
    else if (fmtLower == "webm") fmtHint = "webm";
    // else: nullptr — let FFmpeg guess from the extension.

    avformat_alloc_output_context2(&m_formatContext, nullptr, fmtHint,
                                   filepath.toUtf8().constData());
    if (!m_formatContext) {
        qWarning() << "Failed to create output context";
        emit error("Failed to create output context");
        return false;
    }

    // 2. Video stream.
    if (!initializeVideo())
    {
        qWarning() << "Failed to initialize video";
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }

    // 3. Audio stream (if enabled).
    if (m_audioEnabled)
    {
        if (!initializeAudio())
        {
            qWarning() << "Audio initialization failed — exporting without sound";
            m_audioEnabled = false;
        }
    }

    // 4. Open the file for writing.
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&m_formatContext->pb,
                      filepath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0)
        {
            qWarning() << "Failed to open file:" << filepath;
            emit error("Failed to open file");
            avformat_free_context(m_formatContext); m_formatContext = nullptr;
            return false;
        }
    }

    // 5. Write the container header.
    AVDictionary* opts = nullptr;
    if (filepath.endsWith(".mp4", Qt::CaseInsensitive))
    {
        av_dict_set(&opts, "movflags", "faststart", 0);
    }

    if (avformat_write_header(m_formatContext, &opts) < 0)
    {
        qWarning() << "Failed to write header";
        emit error("Failed to write header");
        av_dict_free(&opts);
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) avio_closep(&m_formatContext->pb);
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }
    av_dict_free(&opts);
    m_headerWritten = true;

    // 6. Video frame buffer.
    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = m_videoCodecContext->pix_fmt;
    m_videoFrame->width  = m_width;
    m_videoFrame->height = m_height;
    if (av_frame_get_buffer(m_videoFrame, 0) < 0)
    {
        qWarning() << "Failed to allocate video frame buffer";
        finish();
        return false;
    }

    m_videoPacket = av_packet_alloc();

    // 7. Audio buffers.
    if (m_audioEnabled && m_audioCodecContext)
    {
        m_audioFrame = av_frame_alloc();
        m_audioFrame->format      = m_audioCodecContext->sample_fmt;
        av_channel_layout_copy(&m_audioFrame->ch_layout,
                               &m_audioCodecContext->ch_layout);
        m_audioFrame->sample_rate = m_audioCodecContext->sample_rate;
        m_audioFrame->nb_samples  = m_audioCodecContext->frame_size;

        if (av_frame_get_buffer(m_audioFrame, 0) < 0)
        {
            qWarning() << "Failed to allocate audio frame buffer";
            m_audioEnabled = false;
        }

        m_audioPacket = av_packet_alloc();
    }

    m_videoFrameCount  = 0;
    m_audioSampleCount = 0;
    m_audioBuffer.clear();

#ifndef QT_NO_DEBUG
    qDebug() << "Output file created. Audio:" << m_audioEnabled;
#endif
    return true;
}

// ===== VIDEO INITIALIZATION =====
bool MediaEncoder::initializeVideo() {
    bool isWebM = !m_format.isEmpty()
    ? (m_format.compare("WebM", Qt::CaseInsensitive) == 0)
    : m_filepath.endsWith(".webm", Qt::CaseInsensitive);

    AVCodecID videoCodecId = AV_CODEC_ID_H264;

    if (isWebM)
    {
        bool found = false;
        for (AVCodecID cid : {AV_CODEC_ID_VP9, AV_CODEC_ID_VP8})
        {
            const AVCodec* c = avcodec_find_encoder(cid);
            if (!c) { qDebug() << "WebM: no encoder" << cid; continue;
            }
            AVCodecContext* t = avcodec_alloc_context3(c); if (!t) continue;
            t->width = m_width; t->height = m_height;
            t->time_base = {1,(int)m_fps}; t->pix_fmt = AV_PIX_FMT_YUV420P;
            t->bit_rate = m_bitrate;
            AVDictionary* td = nullptr;
            if (cid == AV_CODEC_ID_VP9){av_dict_set(&td,"crf","33",0);av_dict_set(&td,"b","0",0);}
            int r = avcodec_open2(t, c, &td);
            av_dict_free(&td); avcodec_free_context(&t);
            if (r >= 0) { videoCodecId = cid; found = true;
#ifndef QT_NO_DEBUG
                qDebug() << "WebM: using" << c->name; break;
#endif
            }

#ifndef QT_NO_DEBUG
            qDebug() << "WebM:" << c->name << "open2 failed" << r;
#endif
        }
        if (!found)
        {
            // Fall back to MKV+H264 automatically — a container/codec
            // combination that always works. Recreate the AVFormatContext
            // with the matroska format instead of webm.
            qWarning() << "WebM VP9/VP8 unavailable, falling back to MKV+H264.";
            emit error("WebM codecs are unavailable. The file will be saved as MKV (H.264) instead.");
            // Recreate the context using the matroska container.
            avformat_free_context(m_formatContext);
            m_formatContext = nullptr;
            avformat_alloc_output_context2(&m_formatContext, nullptr, "matroska",
                                           m_filepath.toUtf8().constData());
            if (!m_formatContext) { emit error("Failed to create MKV context"); return false; }
            m_usedFormat = "MKV"; // Switched to MKV.
            videoCodecId = AV_CODEC_ID_H264;
            isWebM = false; // Now MKV — the full codec probing chain applies.
        }
    }

    // ── Codec probing chain: H264(libx264) -> H264(no tuning options) -> MPEG-4 ──
    // Qt's bundled FFmpeg on Windows doesn't ship libx264. h264_mf is
    // unreliable (fails with error 80004005 in some environments). mpeg4
    // is guaranteed to be available in any FFmpeg build, so it's the
    // final fallback.
    struct CodecCandidate { AVCodecID id; bool withOpts; };
    CodecCandidate candidates[] = {
        { AV_CODEC_ID_H264,  true  },   // libx264 with tuning options
        { AV_CODEC_ID_H264,  false },   // h264_mf without options
        { AV_CODEC_ID_MPEG4, false },   // Fallback — always available
    };
    if (isWebM) {
        candidates[0] = { videoCodecId, false };
        candidates[1] = { AV_CODEC_ID_NONE, false };
        candidates[2] = { AV_CODEC_ID_NONE, false };
    }

    bool codecFound = false;
    for (auto& cand : candidates) {
        if (cand.id == AV_CODEC_ID_NONE) continue;
        const AVCodec* tryCodec = avcodec_find_encoder(cand.id);
        if (!tryCodec) continue;
        bool isLibx264try = (qstrcmp(tryCodec->name, "libx264") == 0 ||
                             qstrcmp(tryCodec->name, "libx264rgb") == 0);
        AVCodecContext* testCtx = avcodec_alloc_context3(tryCodec);
        if (!testCtx) continue;
        testCtx->width = m_width; testCtx->height = m_height;
        testCtx->time_base = AVRational{1, static_cast<int>(m_fps)};
        testCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        testCtx->bit_rate = m_bitrate; testCtx->gop_size = 12;
        AVDictionary* td = nullptr;
        if (cand.withOpts && isLibx264try) {
            av_dict_set(&td, "preset", "medium", 0);
            av_dict_set(&td, "tune", "film", 0);
        }
        int r = avcodec_open2(testCtx, tryCodec, &td);
        av_dict_free(&td); avcodec_free_context(&testCtx);
        if (r < 0) {
#ifndef QT_NO_DEBUG
            qDebug() << "Codec" << tryCodec->name << "failed to open:" << r;
#endif
            continue;
        }
        m_videoCodec = tryCodec;
        videoCodecId = cand.id;
#ifndef QT_NO_DEBUG
        qDebug() << "Using video codec:" << tryCodec->name;
#endif
        codecFound = true;
        break;
    }

    if (!codecFound) {
        qWarning() << "No usable video codec was found";
        emit error("No usable video codec was found.");
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) return false;
    m_videoStream->id = m_formatContext->nb_streams - 1;

    m_videoCodecContext = avcodec_alloc_context3(m_videoCodec);
    if (!m_videoCodecContext) return false;

    m_videoCodecContext->codec_id  = videoCodecId;
    m_videoCodecContext->width     = m_width;
    m_videoCodecContext->height    = m_height;

    // ── Correct time_base for fractional frame rates ──────────────────────
    // A naive time_base = {1, (int)m_fps} truncates: for fps = 29.97,
    // (int)29.97 == 29, so each frame is timed as 1/29 = 34.48ms instead
    // of the correct 33.37ms. Over 1800 frames that puts the video track
    // at 62.07s while the audio track stays at 60s, so the audio track
    // ends first. For fps = 23.976, the same truncation drifts by 4.2%,
    // or about 2.5s per minute.
    //
    // Fix: use the standard rational frame rates for NTSC-derived values,
    // and a high-denominator rational (rounded, not truncated) otherwise.
    if (qAbs(m_fps - 29.97) < 0.03)
        m_videoCodecContext->time_base = AVRational{1001, 30000};   // 29.97fps
    else if (qAbs(m_fps - 23.976) < 0.03)
        m_videoCodecContext->time_base = AVRational{1001, 24000};   // 23.976fps
    else if (qAbs(m_fps - 59.94) < 0.06)
        m_videoCodecContext->time_base = AVRational{1001, 60000};   // 59.94fps
    else
        m_videoCodecContext->time_base = AVRational{1, static_cast<int>(m_fps + 0.5)}; // round, not truncate
    // GOP length = 2 seconds: a good balance between random-access seeking
    // and compression efficiency. Too short a GOP inflates file size; too
    // long makes seeking slow.
    m_videoCodecContext->gop_size  = static_cast<int>(m_fps * 2);
    m_videoCodecContext->pix_fmt   = AV_PIX_FMT_YUV420P;
    // max_b_frames = 2 gives roughly 20% better compression with no
    // visible quality loss.
    m_videoCodecContext->max_b_frames = 2;

    m_videoStream->time_base = m_videoCodecContext->time_base;

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        m_videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    bool isLibx264final = (qstrcmp(m_videoCodec->name, "libx264") == 0 ||
                           qstrcmp(m_videoCodec->name, "libx264rgb") == 0);
    AVDictionary* opts = nullptr;
    if (isLibx264final) {
        // CRF 18: high quality, visually close to lossless. The encoder
        // allocates bits per scene automatically — busy scenes get more,
        // static scenes get less — which beats a fixed bitrate for
        // perceived quality.
        av_dict_set(&opts, "preset", "slow",   0); // Slower encoding, better compression.
        av_dict_set(&opts, "tune",   "film",   0);
        av_dict_set(&opts, "crf",    "18",     0); // 0 = lossless, 18 = high quality, 28 = medium.
        m_videoCodecContext->bit_rate = 0; // CRF governs quality here, not a target bitrate.
    } else {
        // h264_mf, mpeg4, VP9 — these need an explicit target bitrate.
        // VBV (Video Buffer Verifier) caps the peak bitrate at 2x target
        // to prevent sudden oversized frames in highly detailed scenes.
        m_videoCodecContext->bit_rate       = m_bitrate;
        m_videoCodecContext->rc_max_rate    = m_bitrate * 2;
        m_videoCodecContext->rc_buffer_size = m_bitrate * 2;
        if (videoCodecId == AV_CODEC_ID_VP9) {
            av_dict_set(&opts, "crf", "20", 0); // VP9 also supports CRF-based rate control.
            av_dict_set(&opts, "b",   "0",  0);
        }
    }

    int ret = avcodec_open2(m_videoCodecContext, m_videoCodec, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        qWarning() << "Failed to open the video codec (final attempt)";
        return false;
    }
    avcodec_parameters_from_context(m_videoStream->codecpar,
                                    m_videoCodecContext);

#ifndef QT_NO_DEBUG
    qDebug() << "Video: H.264" << m_width << "x" << m_height
             << m_fps << "fps" << m_bitrate/1000 << "kbps";
#endif

    return true;
}

// ===== AUDIO INITIALIZATION =====
bool MediaEncoder::initializeAudio()
{
    // WebM requires Opus or Vorbis; MP4/MKV use AAC.
    // m_usedFormat may have been changed in initializeVideo() by the
    // WebM -> MKV fallback.
    bool isWebM = (m_usedFormat.compare("WebM", Qt::CaseInsensitive) == 0);
    AVCodecID audioCodecId = isWebM ? AV_CODEC_ID_OPUS : AV_CODEC_ID_AAC;

    m_audioCodec = avcodec_find_encoder(audioCodecId);
    if (!m_audioCodec && isWebM)
    {
        m_audioCodec = avcodec_find_encoder(AV_CODEC_ID_VORBIS);
        if (m_audioCodec) audioCodecId = AV_CODEC_ID_VORBIS;
    }
    if (!m_audioCodec)
    {
        qWarning() << "No audio codec available";
        return false;
    }

    m_audioStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream) return false;
    m_audioStream->id = m_formatContext->nb_streams - 1;

    m_audioCodecContext = avcodec_alloc_context3(m_audioCodec);
    if (!m_audioCodecContext) return false;

    m_audioCodecContext->codec_id = audioCodecId;
    m_audioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;  // AAC requires planar float.
    m_audioCodecContext->sample_rate = AUDIO_SAMPLE_RATE;
    // 256 kbps is the practical upper bound for "transparent" AAC audio.
    // At 192 kbps some high-frequency artifacts (hiss, crackle) can
    // appear; at 256 kbps AAC is essentially indistinguishable from
    // lossless on typical material.
    m_audioCodecContext->bit_rate = 256000;

    // Stereo channel layout.
    AVChannelLayout stereo;
    av_channel_layout_default(&stereo, AUDIO_CHANNELS);
    av_channel_layout_copy(&m_audioCodecContext->ch_layout, &stereo);
    av_channel_layout_uninit(&stereo);

    m_audioCodecContext->time_base = AVRational{1, AUDIO_SAMPLE_RATE};
    m_audioStream->time_base      = m_audioCodecContext->time_base;

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_audioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(m_audioCodecContext, m_audioCodec, nullptr) < 0) {
        qWarning() << "Failed to open the AAC codec";
        avcodec_free_context(&m_audioCodecContext);
        m_audioCodecContext = nullptr;
        return false;
    }

    avcodec_parameters_from_context(m_audioStream->codecpar,
                                    m_audioCodecContext);

    // SwrContext: interleaved float -> planar float (required by AAC).
    AVChannelLayout outLayout;
    av_channel_layout_copy(&outLayout, &m_audioCodecContext->ch_layout);

    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, AUDIO_CHANNELS);

    swr_alloc_set_opts2(
        &m_swrContext,
        &outLayout,                  // output: whatever layout the codec expects
        AV_SAMPLE_FMT_FLTP,         // output: planar float (AAC)
        AUDIO_SAMPLE_RATE,
        &inLayout,                   // input: our own data
        AV_SAMPLE_FMT_FLT,          // input: interleaved float
        AUDIO_SAMPLE_RATE,
        0, nullptr
        );

    av_channel_layout_uninit(&outLayout);
    av_channel_layout_uninit(&inLayout);

    if (!m_swrContext || swr_init(m_swrContext) < 0)
    {
        qWarning() << "Failed to initialize the audio resampler";
        return false;
    }

#ifndef QT_NO_DEBUG
    qDebug() << "Audio: AAC stereo 44100Hz 256kbps, frame_size="
             << m_audioCodecContext->frame_size;
#endif
    return true;
}

// ===== WRITE A VIDEO FRAME =====
bool MediaEncoder::writeVideoFrame(const QImage& image)
{
    if (!m_videoCodecContext || !m_videoFrame) return false;

    // Lazily create the RGB->YUV conversion context.
    if (!m_swsContext)
    {
        // SWS_LANCZOS gives the best quality for this RGB->YUV color
        // conversion. SWS_BILINEAR is fine for resizing, but LANCZOS
        // produces noticeably fewer artifacts on the color-space
        // conversion itself.
        m_swsContext = sws_getContext(
            m_width, m_height, AV_PIX_FMT_RGB24,
            m_width, m_height, AV_PIX_FMT_YUV420P,
            SWS_LANCZOS, nullptr, nullptr, nullptr
            );
        if (!m_swsContext) return false;
    }

    // Skip resizing if the frame already matches the output resolution.
    // RenderWorker::compositeVideoAt() already scales frames via
    // scaleFrame(); scaling twice would degrade quality, since each
    // bilinear pass adds a bit more blur. Only scale here when the sizes
    // actually differ.
    QImage rgb;
    if (image.width() == m_width && image.height() == m_height) {
        rgb = image.convertToFormat(QImage::Format_RGB888);
    } else {
        rgb = image.scaled(m_width, m_height,
                           Qt::IgnoreAspectRatio,
                           Qt::SmoothTransformation)
                  .convertToFormat(QImage::Format_RGB888);
    }

    // Wrap the QImage data for sws_scale.
    const uint8_t* srcData[1]    = { rgb.constBits() };
    int srcLinesize[1] = { static_cast<int>(rgb.bytesPerLine()) };

    // RGB -> YUV420P.
    av_frame_make_writable(m_videoFrame);
    sws_scale(m_swsContext,
              srcData, srcLinesize, 0, m_height,
              m_videoFrame->data, m_videoFrame->linesize);

    // PTS.
    m_videoFrame->pts = m_videoFrameCount++;

    // Encode.
    int ret = avcodec_send_frame(m_videoCodecContext, m_videoFrame);
    if (ret < 0) return false;

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(m_videoCodecContext, m_videoPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return false;

        av_packet_rescale_ts(m_videoPacket,
                             m_videoCodecContext->time_base,
                             m_videoStream->time_base);
        m_videoPacket->stream_index = m_videoStream->index;

        av_interleaved_write_frame(m_formatContext, m_videoPacket);
        av_packet_unref(m_videoPacket);
    }

    return true;
}

// ===== WRITE AUDIO SAMPLES =====
// Accepts interleaved float [L0, R0, L1, R1, ...] at 44100Hz stereo.
// Buffers input and encodes in chunks of frame_size samples (typically
// 1024 for AAC).
bool MediaEncoder::writeAudioSamples(const QVector<float>& samples)
{
    if (!m_audioEnabled || !m_audioCodecContext) return true;  // Silently no-op.

    // Append to the buffer.
    m_audioBuffer.append(samples);

    // Encode every full frame available.
    int frameSize = m_audioCodecContext->frame_size;  // Samples per channel.
    int frameSamplesTotal = frameSize * AUDIO_CHANNELS;

    while (m_audioBuffer.size() >= frameSamplesTotal)
    {
        if (!flushAudioBuffer()) return false;
    }

    return true;
}

// ===== ENCODE ONE AUDIO FRAME FROM THE BUFFER =====
bool MediaEncoder::flushAudioBuffer()
{
    if (!m_audioCodecContext || !m_audioFrame) return false;

    int frameSize = m_audioCodecContext->frame_size;
    int frameSamplesTotal = frameSize * AUDIO_CHANNELS;

    if (m_audioBuffer.size() < frameSamplesTotal) return true;

    // Wrap the interleaved input data.
    const uint8_t* inData[1] = {
        reinterpret_cast<const uint8_t*>(m_audioBuffer.constData())
};

// Convert interleaved -> planar via the resampler.
av_frame_make_writable(m_audioFrame);
m_audioFrame->nb_samples = frameSize;

int converted = swr_convert(
    m_swrContext,
    m_audioFrame->data, frameSize,
    inData, frameSize
    );

if (converted <= 0)
{
    qWarning() << "Audio conversion failed";
    m_audioBuffer.remove(0, frameSamplesTotal);
    return false;
}

// PTS uses the number of samples actually converted rather than
// frameSize — on the final frame (padded with silence), converted
// can come in below frameSize.
m_audioFrame->nb_samples = converted;
m_audioFrame->pts = m_audioSampleCount;
m_audioSampleCount += converted;

// Encode.
bool ok = encodeAudioFrame(m_audioFrame);

// Drop the consumed samples from the buffer.
m_audioBuffer.remove(0, frameSamplesTotal);

return ok;
}

// ===== ENCODE AN AUDIO FRAME =====
bool MediaEncoder::encodeAudioFrame(AVFrame* frame)
{
    int ret = avcodec_send_frame(m_audioCodecContext, frame);
    if (ret < 0) return false;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_audioCodecContext, m_audioPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return false;

        av_packet_rescale_ts(m_audioPacket,
                             m_audioCodecContext->time_base,
                             m_audioStream->time_base);
        m_audioPacket->stream_index = m_audioStream->index;

        av_interleaved_write_frame(m_formatContext, m_audioPacket);
        av_packet_unref(m_audioPacket);
    }

    return true;
}

// ===== FINALIZE =====
bool MediaEncoder::finish()
{
    if (!m_formatContext) return true;

#ifndef QT_NO_DEBUG
    qDebug() << "MediaEncoder::finish()";
#endif

    if (!m_headerWritten) {
        freeResources();
        if (m_swsContext) {
            sws_freeContext(m_swsContext);
            m_swsContext = nullptr;
        }
        if (m_swrContext) {
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
        }
        if (m_videoCodecContext)
        {
            avcodec_free_context(&m_videoCodecContext);
        }
        if (m_audioCodecContext)
        {
            avcodec_free_context(&m_audioCodecContext);
        }
        avformat_free_context(m_formatContext);

        m_formatContext = nullptr;

        return false;
    }

    // Flush the remaining buffered audio, padded with silence up to a
    // full frame.
    if (m_audioEnabled && m_audioCodecContext)
    {
        int frameSize = m_audioCodecContext->frame_size;
        int need = frameSize * AUDIO_CHANNELS;

        // ── Compensate for AAC priming delay and B-frame video buffering ──
        // The AAC encoder inserts an initial_padding of roughly 2048
        // samples (~46ms) at the start of the stream. With
        // max_b_frames=2, the video encoder buffers about 2 frames
        // (~66ms) before emitting output. Without compensating for this,
        // the audio track in the container ends up roughly 110ms shorter
        // than the video track, and some players cut off the tail of the
        // video once audio runs out. Appending two full AAC frames worth
        // of silence (~46ms) ensures the audio track comfortably covers
        // the entire video track.
        int paddingFrames = 2;
        for (int p = 0; p < paddingFrames; ++p)
        {
            for (int i = 0; i < need; ++i)
                m_audioBuffer.append(0.0f);
        }

        // Encode every remaining full frame.
        while (m_audioBuffer.size() >= need)
        {
            flushAudioBuffer();
        }
    }

    // Flush the video encoder.
    if (m_videoCodecContext) {
        avcodec_send_frame(m_videoCodecContext, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_videoCodecContext, m_videoPacket);
            if (ret != 0) break;
            av_packet_rescale_ts(m_videoPacket,
                                 m_videoCodecContext->time_base,
                                 m_videoStream->time_base);
            m_videoPacket->stream_index = m_videoStream->index;
            av_interleaved_write_frame(m_formatContext, m_videoPacket);
            av_packet_unref(m_videoPacket);
        }
    }

    // Flush the audio encoder.
    if (m_audioEnabled && m_audioCodecContext)
    {
        avcodec_send_frame(m_audioCodecContext, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_audioCodecContext, m_audioPacket);
            if (ret != 0) break;
            av_packet_rescale_ts(m_audioPacket,
                                 m_audioCodecContext->time_base,
                                 m_audioStream->time_base);
            m_audioPacket->stream_index = m_audioStream->index;
            av_interleaved_write_frame(m_formatContext, m_audioPacket);
            av_packet_unref(m_audioPacket);
        }
    }

    // Write the trailer.
    av_write_trailer(m_formatContext);

    // Close the file.
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&m_formatContext->pb);
    }

    // Release resources.
    freeResources();

    if (m_swsContext) { sws_freeContext(m_swsContext); m_swsContext = nullptr;
    }
    if (m_swrContext) { swr_free(&m_swrContext); m_swrContext = nullptr;
    }

    if (m_videoCodecContext) avcodec_free_context(&m_videoCodecContext);
    if (m_audioCodecContext) avcodec_free_context(&m_audioCodecContext);

    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;

#ifndef QT_NO_DEBUG
    qDebug() << "File finalized:" << m_outputPath
             << "V:" << m_videoFrameCount << "frames"
             << "A:" << m_audioSampleCount << "samples";
#endif

    return true;
}

void MediaEncoder::freeResources()
{
    if (m_videoFrame)  av_frame_free(&m_videoFrame);
    if (m_videoPacket) av_packet_free(&m_videoPacket);
    if (m_audioFrame)  av_frame_free(&m_audioFrame);
    if (m_audioPacket) av_packet_free(&m_audioPacket);
    m_audioBuffer.clear();
}

// ===== SETTINGS =====
void MediaEncoder::setCodec(const QString& codecName)
{
#ifndef QT_NO_DEBUG
    qDebug() << "setCodec:" << codecName;
#endif
}

void MediaEncoder::setFormat(const QString& format)
{
    m_format = format;
#ifndef QT_NO_DEBUG
    qDebug() << "setFormat:" << format;
#endif
}

void MediaEncoder::setBitrate(int bitrate)
{
    m_bitrate = bitrate;
}

void MediaEncoder::setFrameRate(double fps)
{
    m_fps = fps;
}

void MediaEncoder::setPixelFormat(const QString& format)
{
    Q_UNUSED(format);
}

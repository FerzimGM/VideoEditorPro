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

// ===== ОТКРЫТЬ ФАЙЛ =====
bool MediaDecoder::openFile(const QString& filepath)
{
    closeFile();
    m_filepath = filepath;

    // 1. Открыть файл
    if (avformat_open_input(&m_formatContext, filepath.toUtf8().constData(),
                            nullptr, nullptr) != 0)
    {
        qWarning() << "❌ Не могу открыть файл:" << filepath;
        emit error("Не могу открыть файл");
        return false;
    }

    // 2. Прочитать информацию о потоках
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
    {
        qWarning() << "❌ Не могу получить информацию о потоках";
        emit error("Не могу получить информацию о потоках");
        closeFile();
        return false;
    }

    // 3. Инициализировать видео и аудио
    bool videoOk = initializeVideo();
    bool audioOk = initializeAudio();

    if (!videoOk && !audioOk)
    {
        qWarning() << "❌ Не найдено ни видео ни аудио потоков";
        emit error("Не найдено видео/аудио потоков");
        closeFile();
        return false;
    }

    // 4. Выделить память для буферов
    m_frame    = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_packet   = av_packet_alloc();

    if (!m_frame || !m_rgbFrame || !m_packet)
    {
        qWarning() << "❌ Не могу выделить память";
        emit error("Не могу выделить память");
        closeFile();
        return false;
    }

    // 5. Инициализировать ресемплер аудио (если есть аудио)
    if (audioOk)
    {
        initializeSwrContext();
    }

#ifndef QT_NO_DEBUG
    qDebug() << "✅ Файл открыт:" << filepath
             << "V:" << videoOk << "A:" << audioOk
             << "Dur:" << getDuration() << "s";
#endif

    return true;
}

// ===== ЗАКРЫТЬ ФАЙЛ =====
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

    m_videoStream = nullptr;
    m_audioStream = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_lastAudioPos = -1.0;
    m_audioOverflow.clear();
    m_cachedSwsFmt = AV_PIX_FMT_NONE;
    m_cachedSwsW = 0;
    m_cachedSwsH = 0;
}

// ===== ИНИЦИАЛИЗАЦИЯ ВИДЕО =====
bool MediaDecoder::initializeVideo()
{
    m_videoStreamIndex = av_find_best_stream(
        m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (m_videoStreamIndex < 0) return false;

    m_videoStream = m_formatContext->streams[m_videoStreamIndex];

    const AVCodec* codec = avcodec_find_decoder(
        m_videoStream->codecpar->codec_id);
    if (!codec) return false;

    m_videoCodecContext = avcodec_alloc_context3(codec);
    if (!m_videoCodecContext) return false;

    if (avcodec_parameters_to_context(m_videoCodecContext,
                                      m_videoStream->codecpar) < 0)
    {
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

    // Многопоточное декодирование для ускорения
    m_videoCodecContext->thread_count = 4;

    if (avcodec_open2(m_videoCodecContext, codec, nullptr) < 0)
    {
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

    return true;
}

// ===== ИНИЦИАЛИЗАЦИЯ АУДИО =====
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

// ===== ИНИЦИАЛИЗАЦИЯ РЕСЕМПЛЕРА АУДИО =====
// Конвертирует любой формат аудио → float interleaved, 44100Hz, stereo
bool MediaDecoder::initializeSwrContext()
{
    if (!m_audioCodecContext) return false;

    // Выходной layout: стерео
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, OUTPUT_CHANNELS);

    // Входной layout из файла
    AVChannelLayout inLayout;
    av_channel_layout_copy(&inLayout, &m_audioCodecContext->ch_layout);

    int ret = swr_alloc_set_opts2(
        &m_swrContext,
        &outLayout,                              // выход: стерео
        AV_SAMPLE_FMT_FLT,                       // выход: float
        OUTPUT_SAMPLE_RATE,                       // выход: 44100
        &inLayout,                                // вход: из файла
        m_audioCodecContext->sample_fmt,           // вход: формат из файла
        m_audioCodecContext->sample_rate,           // вход: частота из файла
        0, nullptr
        );

    av_channel_layout_uninit(&outLayout);
    av_channel_layout_uninit(&inLayout);

    if (ret < 0 || !m_swrContext)
    {
        qWarning() << "❌ Не могу создать SwrContext";
        return false;
    }

    if (swr_init(m_swrContext) < 0)
    {
        qWarning() << "❌ Не могу инициализировать SwrContext";
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

// ===== ПОЛУЧИТЬ СЛЕДУЮЩИЙ ВИДЕОКАДР =====
QImage MediaDecoder::getNextFrame() {
    if (!m_videoCodecContext) return QImage();

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
            return avFrameToQImage(m_frame);
        }
    }

    return QImage();
}

QImage MediaDecoder::avFrameToQImage(AVFrame* frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) return QImage();

    int width = frame->width;
    int height = frame->height;
    AVPixelFormat srcFmt = (AVPixelFormat)frame->format;

    if (!m_swsContext ||
        srcFmt != m_cachedSwsFmt ||
        width != m_cachedSwsW   ||
        height != m_cachedSwsH)
    {
        if (m_swsContext) { sws_freeContext(m_swsContext); m_swsContext = nullptr;
        }
        m_swsContext = sws_getContext(
            width, height, srcFmt,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_swsContext) return QImage();
        m_cachedSwsFmt = srcFmt;
        m_cachedSwsW = width;
        m_cachedSwsH = height;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 32);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!buffer) return QImage();

    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize,
                         buffer, AV_PIX_FMT_RGB24, width, height, 32);

    sws_scale(m_swsContext,
              (const uint8_t* const*)frame->data, frame->linesize,
              0, height, m_rgbFrame->data, m_rgbFrame->linesize);

    QImage image(m_rgbFrame->data[0], width, height,
                 m_rgbFrame->linesize[0], QImage::Format_RGB888);
    QImage result = image.copy();
    av_free(buffer);
    return result;
}

// ===== ДЕКОДИРОВАНИЕ АУДИО ДИАПАЗОНА =====
// Возвращает interleaved float PCM: [L0, R0, L1, R1, ...]
// Всегда OUTPUT_SAMPLE_RATE (44100), OUTPUT_CHANNELS (2)
QVector<float> MediaDecoder::decodeAudioRange(double startTime, double duration)
{
    if (!m_audioCodecContext || !m_swrContext || !m_formatContext)
        return QVector<float>();

    int totalSamples = static_cast<int>(duration * OUTPUT_SAMPLE_RATE);
    int totalFloats  = totalSamples * OUTPUT_CHANNELS;

    // ── Seek только при прыжке ────────────────────────────────────────────
    bool needSeek = (m_lastAudioPos < 0.0) ||
                    (startTime < m_lastAudioPos - 0.05) ||
                    (startTime > m_lastAudioPos + duration * 8.0);

    if (needSeek)
    {
        m_audioOverflow.clear();
        int64_t t = static_cast<int64_t>(startTime * AV_TIME_BASE);
        av_seek_frame(m_formatContext, -1, t, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(m_audioCodecContext);
        swr_convert(m_swrContext, nullptr, 0, nullptr, 0); // сброс задержки swr
        m_lastAudioPos = startTime;
    }

    // ── Результат = остаток с прошлого вызова + новые сэмплы ─────────────
    QVector<float> result;
    result.reserve(totalFloats + 4096);

    // Сначала берём то что осталось с прошлого раза
    if (!m_audioOverflow.isEmpty())
    {
        result = m_audioOverflow;
        m_audioOverflow.clear();
    }

    // ── Читаем новые пакеты пока не наберём достаточно ───────────────────
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
            // Пропускаем кадры до начала нашего окна.
            // При startTime близком к 0 НЕ пропускаем — AAC/MP3 кодеки дают
            // отрицательный или нулевой PTS для первых кадров, иначе result пустой.
            if (audioFrame->pts != AV_NOPTS_VALUE && startTime > 0.1)
            {
                double frameEnd = audioFrame->pts * av_q2d(m_audioStream->time_base)
                + (double)audioFrame->nb_samples
                    / m_audioCodecContext->sample_rate;
                if (frameEnd < startTime - 0.005)
                {
                    av_frame_unref(audioFrame);
                    continue;
                }
            }

            // Ресемплирование
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
                for (int i = 0; i < converted * OUTPUT_CHANNELS; ++i)
                    result.append(p[i]);
            }
            if (outBuf) av_freep(&outBuf);

            av_frame_unref(audioFrame);
        }
    }

    av_frame_free(&audioFrame);
    av_packet_free(&pkt);

    // ── Flush задержки swr ────
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

    // ── Сохраняем переполнение, не выбрасываем ───
    // сохраняем лишнее в m_audioOverflow для следующего вызова.
    if (result.size() > totalFloats) {
        m_audioOverflow = result.mid(totalFloats);
        result.resize(totalFloats);
    } else {
        // Дополнить тишиной если не хватило (только у конца файла)
        while (result.size() < totalFloats)
            result.append(0.0f);
    }

    return result;
}

// ===== ПЕРЕМОТКА =====
bool MediaDecoder::seekTo(double timestamp)
{
    if (!m_formatContext) return false;

    int64_t seekTarget = static_cast<int64_t>(timestamp * AV_TIME_BASE);

    if (av_seek_frame(m_formatContext, -1, seekTarget,
                      AVSEEK_FLAG_BACKWARD) < 0)
    {
        qWarning() << "❌ Ошибка перемотки к" << timestamp;
        return false;
    }

    if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);
    if (m_audioCodecContext) avcodec_flush_buffers(m_audioCodecContext);

    return true;
}

// ===== ИНФОРМАЦИЯ =====
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

// ===== ОСВОБОДИТЬ РЕСУРСЫ =====
void MediaDecoder::freeResources()
{
    if (m_frame) av_frame_free(&m_frame);
    if (m_rgbFrame) av_frame_free(&m_rgbFrame);
    if (m_packet) av_packet_free(&m_packet);
}

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
    // Освобождаем GPU-контекст после закрытия кодека
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

// ===== ИНИЦИАЛИЗАЦИЯ ВИДЕО =====
// Порядок попыток GPU-декодирования (только Windows):
//   1. D3D11VA  — DirectX 11, Windows 8+, все современные GPU
//   2. DXVA2    — DirectX 9, Windows 7+, старые GPU
//   3. CPU      — всегда работает, fallback
//
// Если GPU-декодирование недоступно (старый драйвер, VM, нет GPU) —
// автоматически используется CPU. Программа не падает.
bool MediaDecoder::initializeVideo()
{
    m_videoStreamIndex = av_find_best_stream(
        m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (m_videoStreamIndex < 0) return false;

    m_videoStream = m_formatContext->streams[m_videoStreamIndex];

    const AVCodec* codec = avcodec_find_decoder(
        m_videoStream->codecpar->codec_id);
    if (!codec) return false;

    // Пробуем GPU-декодирование (только если не аудио-только файл)
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

    // Fallback: CPU декодирование (стандартный путь)
    m_videoCodecContext = avcodec_alloc_context3(codec);
    if (!m_videoCodecContext) return false;

    if (avcodec_parameters_to_context(m_videoCodecContext,
                                      m_videoStream->codecpar) < 0)
    {
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

    // Многопоточное CPU-декодирование
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

// ===== GPU-ДЕКОДИРОВАНИЕ: попытка инициализации =====
// Возвращает true если GPU-декодер успешно создан.
// При любой ошибке освобождает ресурсы и возвращает false —
// вызывающий код переходит к CPU.
//
// КАК РАБОТАЕТ:
// FFmpeg использует концепцию "hardware device context" (AVHWDeviceContext).
// Это объект который представляет GPU и его контекст декодирования.
// av_hwdevice_ctx_create создаёт его — FFmpeg сам занимается DirectX.
//
// После создания контекста мы находим формат пикселей GPU-кадра (m_hwPixFmt).
// Декодированные кадры живут в памяти GPU — avFrameToQImage копирует их
// в RAM через av_hwframe_transfer_data прежде чем создать QImage.
bool MediaDecoder::tryInitHardwareDecoder(const AVCodec* codec)
{
    // Список GPU-декодеров по приоритету (Windows-only)
    static const AVHWDeviceType hwTypes[] = {
        AV_HWDEVICE_TYPE_D3D11VA,  // DirectX 11, Windows 8+
        AV_HWDEVICE_TYPE_DXVA2,    // DirectX 9,  Windows 7+
        AV_HWDEVICE_TYPE_NONE      // sentinel
    };

    for (int i = 0; hwTypes[i] != AV_HWDEVICE_TYPE_NONE; ++i)
    {
        AVHWDeviceType hwType = hwTypes[i];

        // Проверяем поддерживает ли кодек этот тип GPU
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
        if (hwFmt == AV_PIX_FMT_NONE) continue; // кодек не поддерживает

        // Создаём GPU-контекст
        AVBufferRef* hwCtx = nullptr;
        if (av_hwdevice_ctx_create(&hwCtx, hwType, nullptr, nullptr, 0) < 0)
            continue; // GPU недоступен, пробуем следующий

        // Создаём AVCodecContext с GPU-контекстом
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) { av_buffer_unref(&hwCtx); continue; }

        if (avcodec_parameters_to_context(codecCtx, m_videoStream->codecpar) < 0)
        {
            avcodec_free_context(&codecCtx);
            av_buffer_unref(&hwCtx);
            continue;
        }

        codecCtx->hw_device_ctx = av_buffer_ref(hwCtx);
        codecCtx->thread_count  = 1; // GPU декодирует сам, CPU-потоки не нужны

        if (avcodec_open2(codecCtx, codec, nullptr) < 0)
        {
            avcodec_free_context(&codecCtx);
            av_buffer_unref(&hwCtx);
            continue;
        }

        // Успех — сохраняем
        m_videoCodecContext = codecCtx;
        m_hwDeviceCtx       = hwCtx;
        m_hwDeviceType      = hwType;
        m_hwPixFmt          = hwFmt;
        return true;
    }

    return false; // все попытки провалились → CPU fallback
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

// ===== БЫСТРЫЙ SEEK + DECODE =====
// Как getFrameAt, но НЕ вызывает avFrameToQImage для промежуточных кадров.
// getFrameAt: seek → decode+convert каждый кадр (5мс × 150 = 750мс при 5с keyframe gap)
// seekAndDecode: seek → decode каждый (1мс) → convert только целевой = ~160мс
// Используется DecoderThread для быстрого seek без притормаживаний.
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
            // Целевой кадр найден — конвертируем ТОЛЬКО его
            return avFrameToQImage(m_frame);
        }
        // Промежуточный кадр — пропускаем БЕЗ sws_scale/QImage (быстро)
    }
    return QImage();
}

// ===== ПОЛУЧИТЬ СЛЕДУЮЩИЙ ВИДЕОКАДР =====
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
            // Отслеживаем PTS для синхронизации в рендере
            if (timeBase > 0.0 && m_frame->best_effort_timestamp != AV_NOPTS_VALUE)
                m_lastVideoPts = m_frame->best_effort_timestamp * timeBase;
            else if (timeBase > 0.0 && m_frame->pts != AV_NOPTS_VALUE)
                m_lastVideoPts = m_frame->pts * timeBase;

            return avFrameToQImage(m_frame);
        }
    }

    return QImage();
}

// ===== КОНВЕРТАЦИЯ AVFrame → QImage =====
// Два режима:
//   CPU-кадр: сразу в sws_scale
//   GPU-кадр: сначала av_hwframe_transfer_data (GPU RAM → CPU RAM), потом sws_scale
//
// ПРЕВЬЮ-РЕЖИМ (m_previewMode = true):
//   sws_scale масштабирует кадр в width/2 × height/2 за один проход.
//   Это в 4 раза меньше пикселей → в 4 раза меньше нагрузки на CPU.
//   QML растягивает маленький QImage на весь экран (PreserveAspectFit).
//   Для рендера в файл m_previewMode = false → полное разрешение.
QImage MediaDecoder::avFrameToQImage(AVFrame* frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) return QImage();

    AVFrame* swFrame = frame;
    AVFrame* transferred = nullptr;

    // GPU → CPU: если кадр находится в видеопамяти GPU
    if (frame->format == m_hwPixFmt && m_hwDeviceCtx)
    {
        transferred = av_frame_alloc();
        if (!transferred) return QImage();

        // Копируем кадр из GPU RAM в CPU RAM.
        // После этого transferred->format = AV_PIX_FMT_NV12 или YUV420P
        // (зависит от GPU-драйвера) — sws_scale умеет работать с обоими.
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

    // Целевой размер: 3/4 в preview-режиме (меньше нагрузки, лучше качество чем 1/4),
    // полный иначе. Делитель 4 давал слишком заметное ухудшение качества.
    // 3/4 = компромисс: пикселей в ~1.8 раза меньше, качество почти не теряется.
    int dstWidth  = m_previewMode ? (srcWidth  * 3 / 4) : srcWidth;
    int dstHeight = m_previewMode ? (srcHeight * 3 / 4) : srcHeight;
    // Гарантируем чётность (sws_scale требует чётные размеры для YUV)
    dstWidth  = (dstWidth  / 2) * 2;
    dstHeight = (dstHeight / 2) * 2;
    if (dstWidth  < 2) dstWidth  = 2;
    if (dstHeight < 2) dstHeight = 2;

    // Пересоздаём SwsContext если изменился формат, размер или режим превью
    if (!m_swsContext
        || srcFmt   != m_cachedSwsFmt
        || srcWidth != m_cachedSwsW
        || srcHeight!= m_cachedSwsH
        || dstWidth != m_cachedDstW
        || dstHeight!= m_cachedDstH)
    {
        if (m_swsContext) { sws_freeContext(m_swsContext); m_swsContext = nullptr; }

        // SWS_BILINEAR — быстро и достаточно качественно для превью.
        // SWS_LANCZOS дал бы лучше качество при масштабировании,
        // но в 3-5 раз медленнее — не нужно для 30fps воспроизведения.
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

// ===== ДЕКОДИРОВАНИЕ АУДИО ДИАПАЗОНА =====
// Возвращает interleaved float PCM: [L0, R0, L1, R1, ...]
// Всегда OUTPUT_SAMPLE_RATE (44100), OUTPUT_CHANNELS (2)
QVector<float> MediaDecoder::decodeAudioRange(double startTime, double duration)
{
    if (!m_audioCodecContext || !m_swrContext || !m_formatContext)
        return QVector<float>();

    // ── ROUND вместо TRUNCATE ─────────────────────────────────────────────
    // Старый код: (int)(duration * 44100) → при duration=1470/44100=0.0333...
    // → 0.0333... * 44100 = 1469.999... → (int) = 1469 → теряем 1 сэмпл!
    // За 1800 кадров: 1800 сэмплов = 0.04с дрейф + микро-gaps = дребезжание.
    int totalSamples = static_cast<int>(duration * OUTPUT_SAMPLE_RATE + 0.5);
    int totalFloats  = totalSamples * OUTPUT_CHANNELS;

    // ── Seek только при прыжке ────────────────────────────────────────────
    // Порог forward: 1.5с вместо duration*8.
    // При 20мс чанках duration*8=160мс — слишком мало, каждый стык клипов
    // вызывал лишний seek. 1.5с — достаточно для нормальных пауз.
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

        // ── Правильный drain ресемплера ──────────────────────────────────
        // Старый код: swr_convert(ctx, nullptr, 0, nullptr, 0) — это NO-OP!
        // FFmpeg drain = swr_convert(ctx, &outbuf, N, NULL, 0) — null INPUT, реальный OUTPUT.
        // Без drain ресемплер хранит ~23мс аудио от ПРЕДЫДУЩЕЙ позиции →
        // эти сэмплы попадают в начало нового чанка → шуршание/хрипение.
        {
            uint8_t* drainBuf = nullptr;
            int drainMax = 4096;
            av_samples_alloc(&drainBuf, nullptr,
                             OUTPUT_CHANNELS, drainMax, AV_SAMPLE_FMT_FLT, 0);
            if (drainBuf)
            {
                // Drain до полной очистки внутреннего буфера
                while (swr_convert(m_swrContext, &drainBuf, drainMax, nullptr, 0) > 0) {}
                av_freep(&drainBuf);
            }
        }

        m_lastAudioPos = startTime;
    }
    else if (!m_audioOverflow.isEmpty() &&
             qAbs(startTime - m_lastAudioPos) > 0.015)
    {
        // Overflow с предыдущего вызова не соответствует текущей позиции —
        // сбрасываем, иначе звук из другого места попадёт в начало чанка.
        m_audioOverflow.clear();
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
            // Получаем PTS кадра (best_effort_timestamp точнее pts для B-frames)
            double frameTimeBase = av_q2d(m_audioStream->time_base);
            double framePts = -1.0;
            if (audioFrame->best_effort_timestamp != AV_NOPTS_VALUE)
                framePts = audioFrame->best_effort_timestamp * frameTimeBase;
            else if (audioFrame->pts != AV_NOPTS_VALUE)
                framePts = audioFrame->pts * frameTimeBase;

            double frameDur = (double)audioFrame->nb_samples
                              / m_audioCodecContext->sample_rate;

            // Пропускаем кадры полностью ДО нашего окна.
            // Порог -0.001 вместо -0.005: точнее, меньше пропускается лишнего.
            if (framePts >= 0.0 && startTime > 0.05)
            {
                double frameEnd = framePts + frameDur;
                if (frameEnd < startTime - 0.001)
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

                // ── ФИКС РАССИНХРОНА: точный skip после seek ─────────────────
                // После AVSEEK_FLAG_BACKWARD seek уезжает к видео-keyframe,
                // который может быть на 2-5с ДО startTime.
                // Пропускаем ВСЕ кадры до startTime, не только первый.
                //
                // m_skipDone = true только когда мы ВЗЯЛИ часть кадра.
                // Если весь кадр пропущен (skipFloats == converted*CH) —
                // следующий кадр тоже ДО startTime и тоже нужен skip.
                //
                // Без этого: trimStart=25с, seek к 22с → первый кадр (22.0с)
                // пропущен целиком, m_skipDone=true → следующие 130 кадров
                // (22.0-25.0с) добавляются БЕЗ skip → 3с мусора → дребезжание.
                int skipFloats = 0;
                if (!m_skipDone && framePts >= 0.0 && framePts < startTime - 0.001)
                {
                    double skipSec = startTime - framePts;
                    int skipSamples = static_cast<int>(skipSec * OUTPUT_SAMPLE_RATE + 0.5);
                    skipFloats = qMin(skipSamples * OUTPUT_CHANNELS,
                                      converted   * OUTPUT_CHANNELS);
                    // Ставим done ТОЛЬКО если взяли хоть часть кадра
                    if (skipFloats < converted * OUTPUT_CHANNELS)
                        m_skipDone = true;
                    // Иначе — весь кадр пропущен, следующий тоже нужно проверить
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

    // ── Микро fade-in после seek: сглаживание разрыва ────────────────────
    // При seek av_seek_frame прыгает к keyframe, skip обрезает сэмплы,
    // но на стыке "последний пропущенный → первый реальный" — резкий скачок
    // амплитуды → щелчок. При разрезанном видео (trimStart>0) каждый
    // переход между клипами = seek = щелчок → "шуршание".
    // Fade-in первых ~3мс (132 сэмпла * 2 канала) убирает скачок.
    if (needSeek && !result.isEmpty())
    {
        const int FADE_SAMPLES = 132; // ~3мс при 44100Hz
        int fadeFloats = qMin(FADE_SAMPLES * OUTPUT_CHANNELS, result.size());
        for (int i = 0; i < fadeFloats; ++i)
        {
            float t = (float)i / (float)fadeFloats; // 0.0 → 1.0
            result[i] *= t;
        }
    }

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

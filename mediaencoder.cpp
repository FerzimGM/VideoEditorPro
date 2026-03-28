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

// СОЗДАТЬ ВЫХОДНОЙ ФАЙЛ
bool MediaEncoder::createOutputFile(const QString& filepath, int width, int height)
{
#ifndef QT_NO_DEBUG
    qDebug() << "📂 MediaEncoder::createOutputFile:" << filepath
             << width << "x" << height;
#endif

    m_outputPath = filepath;
    m_width  = width;
    m_height = height;

    m_filepath  = filepath;
    m_usedFormat = m_format;

    // Определяем format-hint для FFmpeg контейнера.
    // По умолчанию FFmpeg угадывает по расширению файла, но нам нужен явный контроль.
    const char* fmtHint = nullptr;
    QString fmtLower = m_format.toLower();
    if (fmtLower == "mp4") fmtHint = "mp4";
    else if (fmtLower == "mkv") fmtHint = "matroska";
    else if (fmtLower == "avi") fmtHint = "avi";
    else if (fmtLower == "mov") fmtHint = "mov";
    else if (fmtLower == "webm") fmtHint = "webm";
    // else: nullptr = FFmpeg угадывает по расширению

    avformat_alloc_output_context2(&m_formatContext, nullptr, fmtHint,
                                   filepath.toUtf8().constData());
    if (!m_formatContext) {
        qWarning() << "❌ Не могу создать output context";
        emit error("Не могу создать output context");
        return false;
    }

    // 2 Видео поток
    if (!initializeVideo())
    {
        qWarning() << "❌ Не могу инициализировать видео";
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }

    // 3 Аудио поток (если включён)
    if (m_audioEnabled)
    {
        if (!initializeAudio())
        {
            qWarning() << "⚠️ Аудио не инициализировано — экспорт без звука";
            m_audioEnabled = false;
        }
    }

    // 4 Открыть файл для записи
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&m_formatContext->pb,
                      filepath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0)
        {
            qWarning() << "❌ Не могу открыть файл:" << filepath;
            emit error("Не могу открыть файл");
            avformat_free_context(m_formatContext); m_formatContext = nullptr;
            return false;
        }
    }

    // 5 Записать header контейнера
    AVDictionary* opts = nullptr;
    if (filepath.endsWith(".mp4", Qt::CaseInsensitive))
    {
        av_dict_set(&opts, "movflags", "faststart", 0);
    }

    if (avformat_write_header(m_formatContext, &opts) < 0)
    {
        qWarning() << "❌ Не могу записать header";
        emit error("Не могу записать header");
        av_dict_free(&opts);
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) avio_closep(&m_formatContext->pb);
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }
    av_dict_free(&opts);
    m_headerWritten = true;

    // 6 Видео frame буфер
    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = m_videoCodecContext->pix_fmt;
    m_videoFrame->width  = m_width;
    m_videoFrame->height = m_height;
    if (av_frame_get_buffer(m_videoFrame, 0) < 0)
    {
        qWarning() << "❌ Не могу выделить видео буфер";
        finish();
        return false;
    }

    m_videoPacket = av_packet_alloc();

    // 7 Аудио буферы
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
            qWarning() << "⚠️ Не могу выделить аудио буфер";
            m_audioEnabled = false;
        }

        m_audioPacket = av_packet_alloc();
    }

    m_videoFrameCount  = 0;
    m_audioSampleCount = 0;
    m_audioBuffer.clear();

#ifndef QT_NO_DEBUG
    qDebug() << "✅ Выходной файл создан. Audio:" << m_audioEnabled;
#endif
    return true;
}

// ===== ИНИЦИАЛИЗАЦИЯ ВИДЕО =====
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
            // Автоматически переключаемся на MKV+H264 — универсальный fallback.
            // Пересоздаём AVFormatContext с форматом matroska вместо webm.
            qWarning() << "WebM VP9/VP8 недоступны. Используем MKV+H264 fallback.";
            emit error("WebM кодеки недоступны. Файл будет сохранён в формате MKV (H.264).");
            // Пересоздаём контекст с matroska форматом
            avformat_free_context(m_formatContext);
            m_formatContext = nullptr;
            avformat_alloc_output_context2(&m_formatContext, nullptr, "matroska",
                                           m_filepath.toUtf8().constData());
            if (!m_formatContext) { emit error("Не могу создать MKV контекст"); return false; }
            m_usedFormat = "MKV"; // переключились на MKV
            videoCodecId = AV_CODEC_ID_H264;
            isWebM = false; // теперь MKV — полный probing chain доступен
        }
    }

    // ── Probing chain: H264(libx264) → H264(без опций) → MPEG-4 ──────────
    // Qt FFmpeg на Windows не имеет libx264. h264_mf нестабилен (80004005).
    // mpeg4 гарантированно работает в любой сборке FFmpeg.
    struct CodecCandidate { AVCodecID id; bool withOpts; };
    CodecCandidate candidates[] = {
        { AV_CODEC_ID_H264,  true  },   // libx264 с опциями
        { AV_CODEC_ID_H264,  false },   // h264_mf без опций
        { AV_CODEC_ID_MPEG4, false },   // fallback — всегда доступен
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
            qDebug() << "Кодек" << tryCodec->name << "не открылся:" << r;
#endif
            continue;
        }
        m_videoCodec = tryCodec;
        videoCodecId = cand.id;
#ifndef QT_NO_DEBUG
        qDebug() << "Используем видео кодек:" << tryCodec->name;
#endif
        codecFound = true;
        break;
    }

    if (!codecFound) {
        qWarning() << "❌ Ни один видео кодек не открылся";
        emit error("Не удалось найти рабочий видео кодек.");
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

    // ── КРИТИЧЕСКИЙ ФИКС: правильный time_base для дробных fps ──────────
    // Старый код: time_base = {1, (int)m_fps}
    //   При fps=29.97 → (int)29.97 = 29 → кадр длится 1/29=34.48мс вместо 33.37мс
    //   За 1800 кадров: видео=62.07с, аудио=60с → аудио кончается раньше!
    //   При fps=23.976 → (int)23.976 = 23 → дрейф 4.2% → 2.5с за минуту.
    //
    // Фикс: используем стандартные rational fps для NTSC, иначе высокий знаменатель.
    if (qAbs(m_fps - 29.97) < 0.03)
        m_videoCodecContext->time_base = AVRational{1001, 30000};   // 29.97fps
    else if (qAbs(m_fps - 23.976) < 0.03)
        m_videoCodecContext->time_base = AVRational{1001, 24000};   // 23.976fps
    else if (qAbs(m_fps - 59.94) < 0.06)
        m_videoCodecContext->time_base = AVRational{1001, 60000};   // 59.94fps
    else
        m_videoCodecContext->time_base = AVRational{1, static_cast<int>(m_fps + 0.5)}; // round, not truncate
    // GOP = 2 секунды: хороший баланс между случайным доступом и сжатием.
    // Слишком маленький GOP (напр. 1) даёт большой файл, слишком большой — долгий seek.
    m_videoCodecContext->gop_size  = static_cast<int>(m_fps * 2);
    m_videoCodecContext->pix_fmt   = AV_PIX_FMT_YUV420P;
    // max_b_frames=2: B-кадры дают ~20% экономии без потери качества
    m_videoCodecContext->max_b_frames = 2;

    m_videoStream->time_base = m_videoCodecContext->time_base;

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        m_videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    bool isLibx264final = (qstrcmp(m_videoCodec->name, "libx264") == 0 ||
                           qstrcmp(m_videoCodec->name, "libx264rgb") == 0);
    AVDictionary* opts = nullptr;
    if (isLibx264final) {
        // CRF=18: высокое качество, визуально близко к lossless.
        // Кодек сам выбирает битрейт под каждую сцену — динамичные сцены
        // получают больше бит, статичные меньше. Это лучше фиксированного битрейта.
        av_dict_set(&opts, "preset", "slow",   0); // медленнее → лучше сжатие
        av_dict_set(&opts, "tune",   "film",   0);
        av_dict_set(&opts, "crf",    "18",     0); // 0=lossless, 18=высокое, 28=среднее
        m_videoCodecContext->bit_rate = 0; // CRF управляет качеством, не битрейт
    } else {
        // h264_mf, mpeg4, VP9 — задаём явный битрейт.
        // VBV (Video Buffer Verifier): ограничиваем пиковый битрейт × 2
        // чтобы не было внезапных огромных кадров при высокой детализации.
        m_videoCodecContext->bit_rate       = m_bitrate;
        m_videoCodecContext->rc_max_rate    = m_bitrate * 2;
        m_videoCodecContext->rc_buffer_size = m_bitrate * 2;
        if (videoCodecId == AV_CODEC_ID_VP9) {
            av_dict_set(&opts, "crf", "20", 0); // VP9 тоже поддерживает CRF
            av_dict_set(&opts, "b",   "0",  0);
        }
    }

    int ret = avcodec_open2(m_videoCodecContext, m_videoCodec, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        qWarning() << "❌ Не могу открыть видео кодек (финальный)";
        return false;
    }
    avcodec_parameters_from_context(m_videoStream->codecpar,
                                    m_videoCodecContext);

#ifndef QT_NO_DEBUG
    qDebug() << "✅ Видео: H.264" << m_width << "x" << m_height
             << m_fps << "fps" << m_bitrate/1000 << "kbps";
#endif

    return true;
}

// ИНИЦИАЛИЗАЦИЯ АУДИО
bool MediaEncoder::initializeAudio()
{
    // WebM требует Opus или Vorbis; MP4/MKV - AAC
    // m_usedFormat может быть изменён в initializeVideo (WebM-MKV fallback)
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
        qWarning() << "❌ Аудио кодек не найден";
        return false;
    }

    m_audioStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream) return false;
    m_audioStream->id = m_formatContext->nb_streams - 1;

    m_audioCodecContext = avcodec_alloc_context3(m_audioCodec);
    if (!m_audioCodecContext) return false;

    m_audioCodecContext->codec_id = audioCodecId;
    m_audioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;  // AAC требует float planar
    m_audioCodecContext->sample_rate = AUDIO_SAMPLE_RATE;
    // 256 kbps: верхняя граница «прозрачного» звучания для AAC.
    // При 192 kbps возможны артефакты на высоких частотах (шипение, треск).
    // При 256 kbps AAC практически неотличим от lossless на любом материале.
    m_audioCodecContext->bit_rate = 256000;

    // Стерео layout
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
        qWarning() << "❌ Не могу открыть AAC кодек";
        avcodec_free_context(&m_audioCodecContext);
        m_audioCodecContext = nullptr;
        return false;
    }

    avcodec_parameters_from_context(m_audioStream->codecpar,
                                    m_audioCodecContext);

    // SwrContext: float interleaved → float planar (для AAC)
    AVChannelLayout outLayout;
    av_channel_layout_copy(&outLayout, &m_audioCodecContext->ch_layout);

    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, AUDIO_CHANNELS);

    swr_alloc_set_opts2(
        &m_swrContext,
        &outLayout,                  // выход: как кодек хочет
        AV_SAMPLE_FMT_FLTP,         // выход: float planar (AAC)
        AUDIO_SAMPLE_RATE,
        &inLayout,                   // вход: наши данные
        AV_SAMPLE_FMT_FLT,          // вход: float interleaved
        AUDIO_SAMPLE_RATE,
        0, nullptr
        );

    av_channel_layout_uninit(&outLayout);
    av_channel_layout_uninit(&inLayout);

    if (!m_swrContext || swr_init(m_swrContext) < 0)
    {
        qWarning() << "❌ Не могу инициализировать аудио ресемплер";
        return false;
    }

#ifndef QT_NO_DEBUG
    qDebug() << "✅ Аудио: AAC stereo 44100Hz 128kbps, frame_size="
             << m_audioCodecContext->frame_size;
#endif
    return true;
}

// ЗАПИСАТЬ ВИДЕОКАДР
bool MediaEncoder::writeVideoFrame(const QImage& image)
{
    if (!m_videoCodecContext || !m_videoFrame) return false;

    // Создать SwsContext для RGB-YUV (lazy init)
    if (!m_swsContext)
    {
        // SWS_LANCZOS: лучшее качество при конвертации RGB→YUV.
        // SWS_BILINEAR достаточен для масштабирования, но при цветовом
        // преобразовании LANCZOS даёт заметно меньше артефактов.
        m_swsContext = sws_getContext(
            m_width, m_height, AV_PIX_FMT_RGB24,
            m_width, m_height, AV_PIX_FMT_YUV420P,
            SWS_LANCZOS, nullptr, nullptr, nullptr
            );
        if (!m_swsContext) return false;
    }

    // Если кадр уже нужного размера — не масштабируем повторно.
    // RenderWorker::compositeVideoAt() уже масштабировал через scaleFrame().
    // Двойное масштабирование деградирует качество: каждый проход bilinear
    // добавляет размытие. При несовпадении размеров — масштабируем один раз.
    QImage rgb;
    if (image.width() == m_width && image.height() == m_height) {
        rgb = image.convertToFormat(QImage::Format_RGB888);
    } else {
        rgb = image.scaled(m_width, m_height,
                           Qt::IgnoreAspectRatio,
                           Qt::SmoothTransformation)
                  .convertToFormat(QImage::Format_RGB888);
    }

    // Заполнить данные из QImage
    const uint8_t* srcData[1]    = { rgb.constBits() };
    int srcLinesize[1] = { static_cast<int>(rgb.bytesPerLine()) };

    // RGB - YUV420P
    av_frame_make_writable(m_videoFrame);
    sws_scale(m_swsContext,
              srcData, srcLinesize, 0, m_height,
              m_videoFrame->data, m_videoFrame->linesize);

    // PTS
    m_videoFrame->pts = m_videoFrameCount++;

    // Кодировать
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

// ЗАПИСАТЬ АУДИО СЭМПЛЫ
// Принимает interleaved float [L0, R0, L1, R1, ...] (44100Hz, stereo)
// Буферизует и кодирует по frame_size сэмплов (обычно 1024 для AAC)
bool MediaEncoder::writeAudioSamples(const QVector<float>& samples)
{
    if (!m_audioEnabled || !m_audioCodecContext) return true;  // молча пропускаем

    // Добавить в буфер
    m_audioBuffer.append(samples);

    // Кодировать полные фреймы
    int frameSize = m_audioCodecContext->frame_size;  // сэмплов на канал
    int frameSamplesTotal = frameSize * AUDIO_CHANNELS;

    while (m_audioBuffer.size() >= frameSamplesTotal)
    {
        if (!flushAudioBuffer()) return false;
    }

    return true;
}

// КОДИРОВАТЬ ОДИН АУДИО ФРЕЙМ ИЗ БУФЕРА
bool MediaEncoder::flushAudioBuffer()
{
    if (!m_audioCodecContext || !m_audioFrame) return false;

    int frameSize = m_audioCodecContext->frame_size;
    int frameSamplesTotal = frameSize * AUDIO_CHANNELS;

    if (m_audioBuffer.size() < frameSamplesTotal) return true;

    // Подготовить входные данные (interleaved float)
    const uint8_t* inData[1] ={
        reinterpret_cast<const uint8_t*>(m_audioBuffer.constData())
};

// Конвертировать interleaved → planar (через SwrContext)
av_frame_make_writable(m_audioFrame);
m_audioFrame->nb_samples = frameSize;

int converted = swr_convert(
    m_swrContext,
    m_audioFrame->data, frameSize,
    inData, frameSize
    );

if (converted <= 0)
{
    qWarning() << "❌ Ошибка конвертации аудио";
    m_audioBuffer.remove(0, frameSamplesTotal);
    return false;
}

// PTS: используем реально конвертированные сэмплы, не frameSize.
// При последнем фрейме (добитом нулями) converted может быть < frameSize.
m_audioFrame->nb_samples = converted;
m_audioFrame->pts = m_audioSampleCount;
m_audioSampleCount += converted;

// Кодировать
bool ok = encodeAudioFrame(m_audioFrame);

// Убрать использованные сэмплы из буфера
m_audioBuffer.remove(0, frameSamplesTotal);

return ok;
}

//  КОДИРОВАТЬ АУДИО ФРЕЙМ
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

// ЗАВЕРШЕНИЕ
bool MediaEncoder::finish()
{
    if (!m_formatContext) return true;

#ifndef QT_NO_DEBUG
    qDebug() << "🔚 MediaEncoder::finish()";
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

    // Flush оставшиеся аудио сэмплы (дополняем тишиной до frame_size)
    if (m_audioEnabled && m_audioCodecContext && !m_audioBuffer.isEmpty())
    {
        int frameSize = m_audioCodecContext->frame_size;
        int need = frameSize * AUDIO_CHANNELS;
        while (m_audioBuffer.size() < need)
        {
            m_audioBuffer.append(0.0f);
        }
        flushAudioBuffer();
    }

    // Flush видео кодера
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

    // Flush аудио кодера
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

    // Trailer
    av_write_trailer(m_formatContext);

    // Закрыть файл
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&m_formatContext->pb);
    }

    // Освободить ресурсы
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
    qDebug() << "✅ Файл финализирован:" << m_outputPath
             << "V:" << m_videoFrameCount << "кадров"
             << "A:" << m_audioSampleCount << "сэмплов";
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

// НАСТРОЙКИ
void MediaEncoder::setCodec(const QString& codecName)
{
#ifndef QT_NO_DEBUG
    qDebug() << "🎞️ setCodec:" << codecName;
#endif
}

void MediaEncoder::setFormat(const QString& format)
{
    m_format = format;
#ifndef QT_NO_DEBUG
    qDebug() << "📦 setFormat:" << format;
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

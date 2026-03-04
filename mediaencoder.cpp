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
    , m_bitrate(5000000)
    , m_audioEnabled(true)
    , m_videoFrameCount(0)
    , m_audioSampleCount(0)
    , m_videoFrame(nullptr)
    , m_videoPacket(nullptr)
    , m_audioFrame(nullptr)
    , m_audioPacket(nullptr)
{
}

MediaEncoder::~MediaEncoder() {
    finish();
}

// ===== СОЗДАТЬ ВЫХОДНОЙ ФАЙЛ =====
bool MediaEncoder::createOutputFile(const QString& filepath, int width, int height) {
    qDebug() << "📂 MediaEncoder::createOutputFile:" << filepath
             << width << "x" << height;

    m_outputPath = filepath;
    m_width  = width;
    m_height = height;

    // 1. Выделить output context (формат определяется по расширению)
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr,
                                   filepath.toUtf8().constData());
    if (!m_formatContext) {
        qWarning() << "❌ Не могу создать output context";
        emit error("Не могу создать output context");
        return false;
    }

    // 2. Видео поток
    if (!initializeVideo()) {
        qWarning() << "❌ Не могу инициализировать видео";
        emit error("Не могу инициализировать видео");
        finish();
        return false;
    }

    // 3. Аудио поток (если включён)
    if (m_audioEnabled) {
        if (!initializeAudio()) {
            qWarning() << "⚠️ Аудио не инициализировано — экспорт без звука";
            m_audioEnabled = false;
        }
    }

    // 4. Открыть файл для записи
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatContext->pb,
                      filepath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            qWarning() << "❌ Не могу открыть файл:" << filepath;
            emit error("Не могу открыть файл");
            finish();
            return false;
        }
    }

    // 5. Записать header контейнера
    AVDictionary* opts = nullptr;
    // Для MP4: faststart для быстрого начала воспроизведения
    if (filepath.endsWith(".mp4", Qt::CaseInsensitive)) {
        av_dict_set(&opts, "movflags", "faststart", 0);
    }

    if (avformat_write_header(m_formatContext, &opts) < 0) {
        qWarning() << "❌ Не могу записать header";
        emit error("Не могу записать header");
        av_dict_free(&opts);
        finish();
        return false;
    }
    av_dict_free(&opts);

    // 6. Видео frame буфер
    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = m_videoCodecContext->pix_fmt;
    m_videoFrame->width  = m_width;
    m_videoFrame->height = m_height;
    if (av_frame_get_buffer(m_videoFrame, 0) < 0) {
        qWarning() << "❌ Не могу выделить видео буфер";
        finish();
        return false;
    }

    m_videoPacket = av_packet_alloc();

    // 7. Аудио буферы
    if (m_audioEnabled && m_audioCodecContext) {
        m_audioFrame = av_frame_alloc();
        m_audioFrame->format      = m_audioCodecContext->sample_fmt;
        av_channel_layout_copy(&m_audioFrame->ch_layout,
                               &m_audioCodecContext->ch_layout);
        m_audioFrame->sample_rate = m_audioCodecContext->sample_rate;
        m_audioFrame->nb_samples  = m_audioCodecContext->frame_size;

        if (av_frame_get_buffer(m_audioFrame, 0) < 0) {
            qWarning() << "⚠️ Не могу выделить аудио буфер";
            m_audioEnabled = false;
        }

        m_audioPacket = av_packet_alloc();
    }

    m_videoFrameCount  = 0;
    m_audioSampleCount = 0;
    m_audioBuffer.clear();

    qDebug() << "✅ Выходной файл создан. Audio:" << m_audioEnabled;
    return true;
}

// ===== ИНИЦИАЛИЗАЦИЯ ВИДЕО =====
bool MediaEncoder::initializeVideo() {
    // H.264 кодек
    m_videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!m_videoCodec) {
        qWarning() << "❌ H.264 кодек не найден";
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) return false;
    m_videoStream->id = m_formatContext->nb_streams - 1;

    m_videoCodecContext = avcodec_alloc_context3(m_videoCodec);
    if (!m_videoCodecContext) return false;

    m_videoCodecContext->codec_id  = AV_CODEC_ID_H264;
    m_videoCodecContext->bit_rate  = m_bitrate;
    m_videoCodecContext->width     = m_width;
    m_videoCodecContext->height    = m_height;
    m_videoCodecContext->time_base = AVRational{1, static_cast<int>(m_fps)};
    m_videoCodecContext->gop_size  = 12;
    m_videoCodecContext->pix_fmt   = AV_PIX_FMT_YUV420P;

    m_videoStream->time_base = m_videoCodecContext->time_base;

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Настройки x264: preset medium для баланса скорость/качество
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "medium", 0);
    av_dict_set(&opts, "tune", "film", 0);

    int ret = avcodec_open2(m_videoCodecContext, m_videoCodec, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        qWarning() << "❌ Не могу открыть видео кодек";
        return false;
    }

    avcodec_parameters_from_context(m_videoStream->codecpar,
                                    m_videoCodecContext);

    qDebug() << "✅ Видео: H.264" << m_width << "x" << m_height
             << m_fps << "fps" << m_bitrate/1000 << "kbps";
    return true;
}

// ===== ИНИЦИАЛИЗАЦИЯ АУДИО =====
bool MediaEncoder::initializeAudio() {
    // AAC кодек
    m_audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!m_audioCodec) {
        qWarning() << "❌ AAC кодек не найден";
        return false;
    }

    m_audioStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream) return false;
    m_audioStream->id = m_formatContext->nb_streams - 1;

    m_audioCodecContext = avcodec_alloc_context3(m_audioCodec);
    if (!m_audioCodecContext) return false;

    m_audioCodecContext->codec_id    = AV_CODEC_ID_AAC;
    m_audioCodecContext->sample_fmt  = AV_SAMPLE_FMT_FLTP;  // AAC требует float planar
    m_audioCodecContext->sample_rate = AUDIO_SAMPLE_RATE;
    m_audioCodecContext->bit_rate    = 192000;  // 192 kbps — хорошее качество

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

    if (!m_swrContext || swr_init(m_swrContext) < 0) {
        qWarning() << "❌ Не могу инициализировать аудио ресемплер";
        return false;
    }

    qDebug() << "✅ Аудио: AAC stereo 44100Hz 128kbps, frame_size="
             << m_audioCodecContext->frame_size;
    return true;
}

// ===== ЗАПИСАТЬ ВИДЕОКАДР =====
bool MediaEncoder::writeVideoFrame(const QImage& image) {
    if (!m_videoCodecContext || !m_videoFrame) return false;

    // Создать SwsContext для RGB→YUV (lazy init)
    if (!m_swsContext) {
        m_swsContext = sws_getContext(
            m_width, m_height, AV_PIX_FMT_RGB24,
            m_width, m_height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
            );
        if (!m_swsContext) return false;
    }

    // QImage → RGB24
    QImage rgb = image.scaled(m_width, m_height,
                              Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_RGB888);

    // Заполнить данные из QImage
    const uint8_t* srcData[1]    = { rgb.constBits() };
    int            srcLinesize[1] = { static_cast<int>(rgb.bytesPerLine()) };

    // RGB → YUV420P
    av_frame_make_writable(m_videoFrame);
    sws_scale(m_swsContext,
              srcData, srcLinesize, 0, m_height,
              m_videoFrame->data, m_videoFrame->linesize);

    // PTS
    m_videoFrame->pts = m_videoFrameCount++;

    // Кодировать
    int ret = avcodec_send_frame(m_videoCodecContext, m_videoFrame);
    if (ret < 0) return false;

    while (ret >= 0) {
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

// ===== ЗАПИСАТЬ АУДИО СЭМПЛЫ =====
// Принимает interleaved float [L0, R0, L1, R1, ...] (44100Hz, stereo)
// Буферизует и кодирует по frame_size сэмплов (обычно 1024 для AAC)
bool MediaEncoder::writeAudioSamples(const QVector<float>& samples) {
    if (!m_audioEnabled || !m_audioCodecContext) return true;  // молча пропускаем

    // Добавить в буфер
    m_audioBuffer.append(samples);

    // Кодировать полные фреймы
    int frameSize = m_audioCodecContext->frame_size;  // сэмплов на канал
    int frameSamplesTotal = frameSize * AUDIO_CHANNELS;

    while (m_audioBuffer.size() >= frameSamplesTotal) {
        if (!flushAudioBuffer()) return false;
    }

    return true;
}

// ===== КОДИРОВАТЬ ОДИН АУДИО ФРЕЙМ ИЗ БУФЕРА =====
bool MediaEncoder::flushAudioBuffer() {
    if (!m_audioCodecContext || !m_audioFrame) return false;

    int frameSize = m_audioCodecContext->frame_size;
    int frameSamplesTotal = frameSize * AUDIO_CHANNELS;

    if (m_audioBuffer.size() < frameSamplesTotal) return true;

    // Подготовить входные данные (interleaved float)
    const uint8_t* inData[1] = {
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

if (converted <= 0) {
    qWarning() << "❌ Ошибка конвертации аудио";
    m_audioBuffer.remove(0, frameSamplesTotal);
    return false;
}

// PTS
m_audioFrame->pts = m_audioSampleCount;
m_audioSampleCount += frameSize;

// Кодировать
bool ok = encodeAudioFrame(m_audioFrame);

// Убрать использованные сэмплы из буфера
m_audioBuffer.remove(0, frameSamplesTotal);

return ok;
}

// ===== КОДИРОВАТЬ АУДИО ФРЕЙМ =====
bool MediaEncoder::encodeAudioFrame(AVFrame* frame) {
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

// ===== ЗАВЕРШЕНИЕ =====
bool MediaEncoder::finish() {
    if (!m_formatContext) return true;

    qDebug() << "🔚 MediaEncoder::finish()";

    // Flush оставшиеся аудио сэмплы (дополняем тишиной до frame_size)
    if (m_audioEnabled && m_audioCodecContext && !m_audioBuffer.isEmpty()) {
        int frameSize = m_audioCodecContext->frame_size;
        int need = frameSize * AUDIO_CHANNELS;
        while (m_audioBuffer.size() < need) {
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
    if (m_audioEnabled && m_audioCodecContext) {
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
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&m_formatContext->pb);
    }

    // Освободить ресурсы
    freeResources();

    if (m_swsContext) { sws_freeContext(m_swsContext); m_swsContext = nullptr; }
    if (m_swrContext) { swr_free(&m_swrContext); m_swrContext = nullptr; }

    if (m_videoCodecContext) avcodec_free_context(&m_videoCodecContext);
    if (m_audioCodecContext) avcodec_free_context(&m_audioCodecContext);

    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;

    qDebug() << "✅ Файл финализирован:" << m_outputPath
             << "V:" << m_videoFrameCount << "кадров"
             << "A:" << m_audioSampleCount << "сэмплов";

    return true;
}

void MediaEncoder::freeResources() {
    if (m_videoFrame)  av_frame_free(&m_videoFrame);
    if (m_videoPacket) av_packet_free(&m_videoPacket);
    if (m_audioFrame)  av_frame_free(&m_audioFrame);
    if (m_audioPacket) av_packet_free(&m_audioPacket);
    m_audioBuffer.clear();
}

// ===== НАСТРОЙКИ =====
void MediaEncoder::setCodec(const QString& codecName) {
    qDebug() << "🎞️ setCodec:" << codecName;
}

void MediaEncoder::setFormat(const QString& format) {
    qDebug() << "📦 setFormat:" << format;
}

void MediaEncoder::setBitrate(int bitrate) {
    m_bitrate = bitrate;
}

void MediaEncoder::setFrameRate(double fps) {
    m_fps = fps;
}

void MediaEncoder::setPixelFormat(const QString& format) {
    Q_UNUSED(format);
}

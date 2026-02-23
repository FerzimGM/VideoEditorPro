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
    , m_width(1920)
    , m_height(1080)
    , m_fps(30.0)
    , m_bitrate(5000000)  // 5 Mbps
    , m_frameCount(0)
    , m_frame(nullptr)
    , m_rgbFrame(nullptr)
    , m_packet(nullptr)
{
    qDebug() << "📹 MediaEncoder создан";
}

MediaEncoder::~MediaEncoder() {
    finish();
    qDebug() << "📹 MediaEncoder уничтожен";
}

// ===== СОЗДАТЬ ВЫХОДНОЙ ФАЙЛ =====
bool MediaEncoder::createOutputFile(const QString& filepath, int width, int height) {
    qDebug() << "📂 MediaEncoder::createOutputFile:" << filepath;
    qDebug() << "   Разрешение:" << width << "x" << height;

    m_outputPath = filepath;
    m_width = width;
    m_height = height;

    // 1. Выделить контекст для выходного файла
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, filepath.toUtf8().constData());

    if (!m_formatContext) {
        qWarning() << "❌ Не могу создать output context";
        emit error("Не могу создать output context");
        return false;
    }

    // 2. Инициализировать видео поток
    if (!initializeVideo()) {
        qWarning() << "❌ Не могу инициализировать видео";
        emit error("Не могу инициализировать видео");
        finish();
        return false;
    }

    // 3. TODO: Инициализировать аудио поток
    // initializeAudio();

    // 4. Открыть выходной файл
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatContext->pb, filepath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            qWarning() << "❌ Не могу открыть выходной файл:" << filepath;
            emit error("Не могу открыть файл");
            finish();
            return false;
        }
    }

    // 5. Записать header
    if (avformat_write_header(m_formatContext, nullptr) < 0) {
        qWarning() << "❌ Не могу записать header";
        emit error("Не могу записать header");
        finish();
        return false;
    }

    // 6. Выделить буферы
    m_frame = av_frame_alloc();
    m_frame->format = m_videoCodecContext->pix_fmt;
    m_frame->width = m_width;
    m_frame->height = m_height;

    if (av_frame_get_buffer(m_frame, 0) < 0) {
        qWarning() << "❌ Не могу выделить буфер для кадра";
        emit error("Не могу выделить буфер");
        finish();
        return false;
    }

    m_packet = av_packet_alloc();

    qDebug() << "✅ Выходной файл создан";
    return true;
}

// ===== ИНИЦИАЛИЗАЦИЯ ВИДЕО =====
bool MediaEncoder::initializeVideo() {
    // 1. Найти кодек (H.264)
    m_videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);

    if (!m_videoCodec) {
        qWarning() << "❌ H.264 кодек не найден";
        return false;
    }

    qDebug() << "✅ Кодек найден:" << m_videoCodec->name;

    // 2. Создать поток
    m_videoStream = avformat_new_stream(m_formatContext, nullptr);

    if (!m_videoStream) {
        qWarning() << "❌ Не могу создать видео поток";
        return false;
    }

    m_videoStream->id = m_formatContext->nb_streams - 1;

    // 3. Создать контекст кодека
    m_videoCodecContext = avcodec_alloc_context3(m_videoCodec);

    if (!m_videoCodecContext) {
        qWarning() << "❌ Не могу создать codec context";
        return false;
    }

    // 4. Настроить параметры
    m_videoCodecContext->codec_id = AV_CODEC_ID_H264;
    m_videoCodecContext->bit_rate = m_bitrate;
    m_videoCodecContext->width = m_width;
    m_videoCodecContext->height = m_height;

    // Timebase: 1/fps
    m_videoStream->time_base = AVRational{1, static_cast<int>(m_fps)};
    m_videoCodecContext->time_base = m_videoStream->time_base;

    m_videoCodecContext->gop_size = 12;  // Keyframe каждые 12 кадров
    m_videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;

    // Настройки для MP4
    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 5. Открыть кодек
    if (avcodec_open2(m_videoCodecContext, m_videoCodec, nullptr) < 0) {
        qWarning() << "❌ Не могу открыть кодек";
        return false;
    }

    // 6. Скопировать параметры в поток
    if (avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecContext) < 0) {
        qWarning() << "❌ Не могу скопировать параметры";
        return false;
    }

    qDebug() << "✅ Видео поток инициализирован";
    qDebug() << "   Разрешение:" << m_width << "x" << m_height;
    qDebug() << "   FPS:" << m_fps;
    qDebug() << "   Bitrate:" << m_bitrate / 1000 << "kbps";

    return true;
}

// ===== ЗАПИСАТЬ ВИДЕО КАДР =====
bool MediaEncoder::writeVideoFrame(const QImage& image) {
    if (!m_videoCodecContext) {
        qWarning() << "❌ Видео кодек не инициализирован";
        return false;
    }

    // 1. Конвертировать QImage в AVFrame

    // Создать SwsContext для конвертации RGB → YUV420P
    if (!m_swsContext) {
        m_swsContext = sws_getContext(
            m_width, m_height, AV_PIX_FMT_RGB24,
            m_width, m_height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
            );

        if (!m_swsContext) {
            qWarning() << "❌ Не могу создать SwsContext";
            return false;
        }
    }

    // Конвертировать QImage в RGB24 формат
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);

    // Создать временный AVFrame для RGB
    AVFrame* rgbFrame = av_frame_alloc();
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width = m_width;
    rgbFrame->height = m_height;

    // Заполнить данными из QImage
    av_image_fill_arrays(
        rgbFrame->data, rgbFrame->linesize,
        rgb.bits(), AV_PIX_FMT_RGB24,
        m_width, m_height, 1
        );

    // 2. Конвертировать RGB → YUV420P
    sws_scale(
        m_swsContext,
        rgbFrame->data, rgbFrame->linesize, 0, m_height,
        m_frame->data, m_frame->linesize
        );

    av_frame_free(&rgbFrame);

    // 3. Установить PTS (presentation timestamp)
    m_frame->pts = m_frameCount++;

    // 4. Отправить кадр в кодер
    int ret = avcodec_send_frame(m_videoCodecContext, m_frame);

    if (ret < 0) {
        qWarning() << "❌ Ошибка отправки кадра в кодер";
        return false;
    }

    // 5. Получить закодированные пакеты
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_videoCodecContext, m_packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;  // Нужно больше данных или конец
        } else if (ret < 0) {
            qWarning() << "❌ Ошибка кодирования";
            return false;
        }

        // Настроить timebase для пакета
        av_packet_rescale_ts(m_packet, m_videoCodecContext->time_base, m_videoStream->time_base);
        m_packet->stream_index = m_videoStream->index;

        // Записать пакет в файл
        ret = av_interleaved_write_frame(m_formatContext, m_packet);

        if (ret < 0) {
            qWarning() << "❌ Ошибка записи пакета";
            av_packet_unref(m_packet);
            return false;
        }

        av_packet_unref(m_packet);
    }

    return true;
}

// ===== ЗАВЕРШЕНИЕ =====
bool MediaEncoder::finish() {
    if (!m_formatContext) {
        return true;  // Уже закрыто
    }

    qDebug() << "🔚 MediaEncoder::finish()";

    // 1. Отправить NULL кадр для flush кодера
    if (m_videoCodecContext) {
        avcodec_send_frame(m_videoCodecContext, nullptr);

        // Получить оставшиеся пакеты
        int ret;
        while ((ret = avcodec_receive_packet(m_videoCodecContext, m_packet)) >= 0) {
            av_packet_rescale_ts(m_packet, m_videoCodecContext->time_base, m_videoStream->time_base);
            m_packet->stream_index = m_videoStream->index;
            av_interleaved_write_frame(m_formatContext, m_packet);
            av_packet_unref(m_packet);
        }
    }

    // 2. Записать trailer
    av_write_trailer(m_formatContext);

    // 3. Закрыть файл
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&m_formatContext->pb);
    }

    // 4. Освободить ресурсы
    freeResources();

    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_videoCodecContext) {
        avcodec_free_context(&m_videoCodecContext);
    }

    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;

    qDebug() << "✅ Файл финализирован:" << m_outputPath;
    qDebug() << "   Всего кадров:" << m_frameCount;

    return true;
}

// ===== ОСВОБОДИТЬ РЕСУРСЫ =====
void MediaEncoder::freeResources() {
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
}

// ===== НАСТРОЙКИ =====
void MediaEncoder::setCodec(const QString& codecName) {
    qDebug() << "🎞️ setCodec:" << codecName;
    // TODO: поддержка других кодеков
}

void MediaEncoder::setFormat(const QString& format) {
    qDebug() << "📦 setFormat:" << format;
}

void MediaEncoder::setBitrate(int bitrate) {
    m_bitrate = bitrate;
    qDebug() << "📊 setBitrate:" << bitrate / 1000 << "kbps";
}

void MediaEncoder::setFrameRate(double fps) {
    m_fps = fps;
    qDebug() << "🎞️ setFrameRate:" << fps;
}

void MediaEncoder::setPixelFormat(const QString& format) {
    qDebug() << "🎨 setPixelFormat:" << format;
}

// ===== TODO: АУДИО =====
bool MediaEncoder::initializeAudio() {
    qDebug() << "⚠️ TODO: initializeAudio";
    return false;
}

bool MediaEncoder::writeAudioSamples(const QByteArray& samples) {
    qDebug() << "⚠️ TODO: writeAudioSamples";
    return false;
}

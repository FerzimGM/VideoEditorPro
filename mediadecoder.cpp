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
{
    qDebug() << "🎥 MediaDecoder создан";
}

MediaDecoder::~MediaDecoder() {
    closeFile();
    qDebug() << "🎥 MediaDecoder уничтожен";
}

// ===== ОТКРЫТЬ ФАЙЛ =====
bool MediaDecoder::openFile(const QString& filepath) {
    qDebug() << "📂 MediaDecoder::openFile:" << filepath;

    // Закрыть предыдущий файл если был открыт
    closeFile();

    m_filepath = filepath;

    // 1. Открыть файл
    if (avformat_open_input(&m_formatContext, filepath.toUtf8().constData(), nullptr, nullptr) != 0) {
        qWarning() << "❌ Не могу открыть файл:" << filepath;
        emit error("Не могу открыть файл");
        return false;
    }

    // 2. Прочитать информацию о потоках
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        qWarning() << "❌ Не могу получить информацию о потоках";
        emit error("Не могу получить информацию о потоках");
        closeFile();
        return false;
    }

    // 3. Найти видео и аудио потоки
    bool videoOk = initializeVideo();
    bool audioOk = initializeAudio();

    if (!videoOk && !audioOk) {
        qWarning() << "❌ Не найдено ни видео ни аудио потоков";
        emit error("Не найдено видео/аудио потоков");
        closeFile();
        return false;
    }

    // 4. Выделить память для кадров и пакетов
    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_packet = av_packet_alloc();

    if (!m_frame || !m_rgbFrame || !m_packet) {
        qWarning() << "❌ Не могу выделить память";
        emit error("Не могу выделить память");
        closeFile();
        return false;
    }

    qDebug() << "✅ Файл открыт успешно";
    qDebug() << "   Видео:" << (videoOk ? "Да" : "Нет");
    qDebug() << "   Аудио:" << (audioOk ? "Да" : "Нет");
    qDebug() << "   Длительность:" << getDuration() << "сек";

    if (videoOk) {
        qDebug() << "   Разрешение:" << getVideoWidth() << "x" << getVideoHeight();
        qDebug() << "   FPS:" << getFrameRate();
    }

    return true;
}

// ===== ЗАКРЫТЬ ФАЙЛ =====
void MediaDecoder::closeFile() {
    freeResources();

    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_videoCodecContext) {
        avcodec_free_context(&m_videoCodecContext);
    }

    if (m_audioCodecContext) {
        avcodec_free_context(&m_audioCodecContext);
    }

    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }

    m_videoStream = nullptr;
    m_audioStream = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;

    qDebug() << "🔚 Файл закрыт";
}

// ===== ИНИЦИАЛИЗАЦИЯ ВИДЕО =====
bool MediaDecoder::initializeVideo() {
    // Найти видео поток
    m_videoStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (m_videoStreamIndex < 0) {
        qDebug() << "⚠️ Видео поток не найден";
        return false;
    }

    m_videoStream = m_formatContext->streams[m_videoStreamIndex];

    // Найти декодер
    const AVCodec* codec = avcodec_find_decoder(m_videoStream->codecpar->codec_id);
    if (!codec) {
        qWarning() << "❌ Декодер не найден";
        return false;
    }

    // Создать контекст декодера
    m_videoCodecContext = avcodec_alloc_context3(codec);
    if (!m_videoCodecContext) {
        qWarning() << "❌ Не могу создать контекст декодера";
        return false;
    }

    // Скопировать параметры
    if (avcodec_parameters_to_context(m_videoCodecContext, m_videoStream->codecpar) < 0) {
        qWarning() << "❌ Не могу скопировать параметры кодека";
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

    // Открыть декодер
    if (avcodec_open2(m_videoCodecContext, codec, nullptr) < 0) {
        qWarning() << "❌ Не могу открыть декодер";
        avcodec_free_context(&m_videoCodecContext);
        return false;
    }

    qDebug() << "✅ Видео декодер инициализирован";
    return true;
}

// ===== ИНИЦИАЛИЗАЦИЯ АУДИО =====
bool MediaDecoder::initializeAudio() {
    m_audioStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (m_audioStreamIndex < 0) {
        qDebug() << "⚠️ Аудио поток не найден";
        return false;
    }

    m_audioStream = m_formatContext->streams[m_audioStreamIndex];

    const AVCodec* codec = avcodec_find_decoder(m_audioStream->codecpar->codec_id);
    if (!codec) {
        qWarning() << "❌ Аудио декодер не найден";
        return false;
    }

    m_audioCodecContext = avcodec_alloc_context3(codec);
    if (!m_audioCodecContext) {
        qWarning() << "❌ Не могу создать аудио контекст";
        return false;
    }

    if (avcodec_parameters_to_context(m_audioCodecContext, m_audioStream->codecpar) < 0) {
        qWarning() << "❌ Не могу скопировать аудио параметры";
        avcodec_free_context(&m_audioCodecContext);
        return false;
    }

    if (avcodec_open2(m_audioCodecContext, codec, nullptr) < 0) {
        qWarning() << "❌ Не могу открыть аудио декодер";
        avcodec_free_context(&m_audioCodecContext);
        return false;
    }

    qDebug() << "✅ Аудио декодер инициализирован";
    return true;
}

// ===== ПОЛУЧИТЬ КАДР В УКАЗАННОЕ ВРЕМЯ =====
QImage MediaDecoder::getFrameAt(double timestamp) {
    if (!m_videoCodecContext) {
        qWarning() << "❌ Видео декодер не инициализирован";
        return QImage();
    }

    // Перемотать к нужной позиции
    if (!seekTo(timestamp)) {
        qWarning() << "❌ Не могу перемотать к" << timestamp;
        return QImage();
    }

    // Читать кадр
    return getNextFrame();
}

// ===== ПОЛУЧИТЬ СЛЕДУЮЩИЙ КАДР =====
QImage MediaDecoder::getNextFrame() {
    if (!m_videoCodecContext) {
        qWarning() << "❌ Видео декодер не инициализирован";
        return QImage();
    }

    while (av_read_frame(m_formatContext, m_packet) >= 0) {
        // Проверить что это видео пакет
        if (m_packet->stream_index != m_videoStreamIndex) {
            av_packet_unref(m_packet);
            continue;
        }

        // Отправить пакет в декодер
        int ret = avcodec_send_packet(m_videoCodecContext, m_packet);
        av_packet_unref(m_packet);

        if (ret < 0) {
            qWarning() << "❌ Ошибка отправки пакета";
            continue;
        }

        // Получить декодированный кадр
        ret = avcodec_receive_frame(m_videoCodecContext, m_frame);
        if (ret == 0) {
            // Кадр получен!
            return avFrameToQImage(m_frame);
        }
    }

    qWarning() << "⚠️ Больше нет кадров";
    return QImage();
}

// ===== КОНВЕРТАЦИЯ AVFrame → QImage =====
QImage MediaDecoder::avFrameToQImage(AVFrame* frame) {
    if (!frame) {
        return QImage();
    }

    int width = frame->width;
    int height = frame->height;

    // Создать SwsContext для конвертации в RGB
    if (!m_swsContext) {
        m_swsContext = sws_getContext(
            width, height, (AVPixelFormat)frame->format,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
            );
    }

    if (!m_swsContext) {
        qWarning() << "❌ Не могу создать SwsContext";
        return QImage();
    }

    // Выделить буфер для RGB кадра
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, buffer, AV_PIX_FMT_RGB24, width, height, 1);

    // Конвертировать
    sws_scale(m_swsContext, frame->data, frame->linesize, 0, height, m_rgbFrame->data, m_rgbFrame->linesize);

    // Создать QImage
    QImage image(m_rgbFrame->data[0], width, height, m_rgbFrame->linesize[0], QImage::Format_RGB888);
    QImage result = image.copy();  // Создать копию

    av_free(buffer);

    return result;
}

// ===== ПЕРЕМОТКА =====
bool MediaDecoder::seekTo(double timestamp) {
    if (!m_formatContext || !m_videoStream) {
        return false;
    }

    // Конвертировать timestamp в единицы времени потока
    int64_t seekTarget = static_cast<int64_t>(timestamp * AV_TIME_BASE);

    if (av_seek_frame(m_formatContext, -1, seekTarget, AVSEEK_FLAG_BACKWARD) < 0) {
        qWarning() << "❌ Ошибка перемотки";
        return false;
    }

    // Сбросить декодеры
    if (m_videoCodecContext) {
        avcodec_flush_buffers(m_videoCodecContext);
    }
    if (m_audioCodecContext) {
        avcodec_flush_buffers(m_audioCodecContext);
    }

    return true;
}

// ===== ПОЛУЧИТЬ ИНФОРМАЦИЮ =====
double MediaDecoder::getDuration() const {
    if (!m_formatContext || m_formatContext->duration == AV_NOPTS_VALUE) {
        return 0.0;
    }
    return (double)m_formatContext->duration / AV_TIME_BASE;
}

int MediaDecoder::getVideoWidth() const {
    return m_videoCodecContext ? m_videoCodecContext->width : 0;
}

int MediaDecoder::getVideoHeight() const {
    return m_videoCodecContext ? m_videoCodecContext->height : 0;
}

double MediaDecoder::getFrameRate() const {
    if (!m_videoStream) return 0.0;
    AVRational fps = m_videoStream->avg_frame_rate;
    if (fps.den == 0) return 0.0;
    return (double)fps.num / fps.den;
}

int MediaDecoder::getAudioSampleRate() const {
    return m_audioCodecContext ? m_audioCodecContext->sample_rate : 0;
}

int MediaDecoder::getAudioChannels() const {
    return m_audioCodecContext ? m_audioCodecContext->ch_layout.nb_channels : 0;
}

// ===== ОСВОБОДИТЬ РЕСУРСЫ =====
void MediaDecoder::freeResources() {
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
}

// ===== АУДИО (TODO) =====
QByteArray MediaDecoder::getAudioSamples(double startTime, double endTime) {
    qDebug() << "⚠️ TODO: getAudioSamples" << startTime << "-" << endTime;
    // TODO: Реализовать декодирование аудио
    return QByteArray();
}

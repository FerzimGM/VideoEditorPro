#include "AudioPlaybackEngine.h"
#include "timeline.h"
#include <QDebug>
#include <QMediaDevices>
#include <QAudioDevice>
#include <cstring>

AudioPlaybackEngine::AudioPlaybackEngine(Timeline* timeline, QObject* parent)
    : QObject(parent), m_timeline(timeline)
{
    m_feedTimer = new QTimer(this);
    m_feedTimer->setInterval(FEED_INTERVAL_MS);
    m_feedTimer->setTimerType(Qt::PreciseTimer);
    connect(m_feedTimer, &QTimer::timeout, this, &AudioPlaybackEngine::onFeedTimer);
}

AudioPlaybackEngine::~AudioPlaybackEngine() { destroySink(); }

void AudioPlaybackEngine::createSink() {
    destroySink();

    QAudioFormat fmt;
    fmt.setSampleRate(SAMPLE_RATE);
    fmt.setChannelCount(CHANNELS);
    fmt.setSampleFormat(QAudioFormat::Float);

    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    m_sink = new QAudioSink(dev, fmt, this);

    // PUSH-режим: start() без аргумента → пишем напрямую в QIODevice*
    // Pull-режим (start(QIODevice*)) на Windows WASAPI зависает:
    // sink не читает данные пока внутренний буфер не заполнится целиком.
    m_sinkDevice = m_sink->start();

    if (m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();

    qDebug() << "AudioPlaybackEngine: sink state=" << m_sink->state()
             << "bufSize=" << m_sink->bufferSize()
             << "device=" << dev.description()
             << "sinkDev=" << (m_sinkDevice ? "OK" : "NULL");
}

void AudioPlaybackEngine::destroySink() {
    m_feedTimer->stop();
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink       = nullptr;
        m_sinkDevice = nullptr;
    }
    m_playing = false;
}

void AudioPlaybackEngine::startPlayback(double fromTime, double speed, double totalDuration) {
    destroySink();
    createSink();

    m_startStreamTime  = fromTime;
    m_writeHead        = fromTime;
    m_speed            = speed;
    m_startProcessedUs = m_sink ? m_sink->processedUSecs() : 0;
    m_playing          = true;
    m_silentChunks     = 0;
    m_totalDuration    = totalDuration;

    // Немедленно пишем первый чанк чтобы sink вышел из IdleState
    onFeedTimer();

    m_feedTimer->start();
    qDebug() << "AudioPlaybackEngine: startPlayback from" << fromTime
             << "speed=" << speed << "totalDur=" << totalDuration;
}

void AudioPlaybackEngine::stopPlayback() {
    destroySink();
    qDebug() << "AudioPlaybackEngine: stopped";
}

void AudioPlaybackEngine::setVolume(float vol) {
    if (m_sink) m_sink->setVolume(vol);
}

void AudioPlaybackEngine::setTrackMuted(int track, bool muted) {
    if (track == 1) m_track1Muted = muted;
    else            m_track2Muted = muted;
}

double AudioPlaybackEngine::getCurrentAudioTime() const {
    if (!m_sink || !m_playing) return m_startStreamTime;
    qint64 playedUs = m_sink->processedUSecs() - m_startProcessedUs;
    // m_speed: при x2 аудио читается вдвое быстрее → время идёт вдвое быстрее
    return m_startStreamTime + (playedUs / 1e6) * m_speed;
}

void AudioPlaybackEngine::onFeedTimer() {
    if (!m_playing || !m_sink || !m_sinkDevice) return;

    if (m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();

    // Сколько байт sink готов принять (push-режим)
    qint64 bytesFree = m_sink->bytesFree();
    if (bytesFree < (qint64)(sizeof(float) * CHANNELS * 64)) {
        // Буфер почти полный — только обновляем время
        emit timeUpdated(getCurrentAudioTime());
        return;
    }

    // Размер чанка: не более 150мс, не более свободного места
    const double MAX_CHUNK_SEC = 0.15;
    int maxFloats  = (int)(MAX_CHUNK_SEC * SAMPLE_RATE * CHANNELS);
    int freeFloats = (int)(bytesFree / sizeof(float));
    int wantFloats = qMin(freeFloats, maxFloats);
    double chunkDur = (double)wantFloats / (SAMPLE_RATE * CHANNELS);

    // При speed != 1.0: читаем аудио с той же скоростью что и время идёт.
    // writeHead движется в единицах времени таймлайна.
    // При x2 за 150мс реального времени нужно прочитать 300мс аудио.
    double readDur = chunkDur * m_speed;

    QVector<float> audio = m_timeline->getMixedAudio(
        m_writeHead, readDur, m_track1Muted, m_track2Muted);

    QVector<float> toWrite;
    if (!audio.isEmpty()) {
        // Ресемплируем под нужный размер чанка если speed != 1.0
        if (qAbs(m_speed - 1.0) > 0.01 && audio.size() != wantFloats) {
            // Простой ресемплинг линейной интерполяцией
            toWrite.resize(wantFloats);
            double ratio = (double)audio.size() / wantFloats;
            for (int i = 0; i < wantFloats; i += CHANNELS) {
                double srcPos = (i / CHANNELS) * ratio * CHANNELS;
                int src0 = ((int)(srcPos / CHANNELS)) * CHANNELS;
                int src1 = qMin(src0 + CHANNELS, audio.size() - CHANNELS);
                double frac = (srcPos - src0) / CHANNELS;
                for (int c = 0; c < CHANNELS; c++) {
                    float s0 = (src0 + c < audio.size()) ? audio[src0 + c] : 0.0f;
                    float s1 = (src1 + c < audio.size()) ? audio[src1 + c] : 0.0f;
                    toWrite[i + c] = s0 + (s1 - s0) * (float)frac;
                }
            }
        } else {
            toWrite = audio;
            toWrite.resize(wantFloats, 0.0f);
        }
        m_writeHead += readDur;
        m_silentChunks = 0;
    } else {
        toWrite.resize(wantFloats, 0.0f);
        m_writeHead += readDur;
        ++m_silentChunks;
    }

    // Пишем в sink напрямую (push)
    m_sinkDevice->write(
        reinterpret_cast<const char*>(toWrite.constData()),
        toWrite.size() * sizeof(float));

    emit timeUpdated(getCurrentAudioTime());

    // Детекция конца
    bool pastEnd     = (m_writeHead >= m_totalDuration - 0.05);
    bool longSilence = (m_silentChunks >= 30)
                       && (m_writeHead - m_startStreamTime > 5.0);

    if (pastEnd || longSilence) {
        qDebug() << "AudioPlaybackEngine: end detected, writeHead=" << m_writeHead
                 << "totalDur=" << m_totalDuration;
        QTimer::singleShot(300, this, [this]() {
            if (m_playing) {
                stopPlayback();
                emit playbackEnded();
            }
        });
    }
}



#include "AudioPlaybackEngine.h"
#include "timeline.h"
#include <QDebug>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDateTime>

AudioPlaybackEngine::AudioPlaybackEngine(Timeline* timeline, QObject* parent)
    : QObject(parent), m_timeline(timeline)
{
    m_feedTimer = new QTimer(this);
    m_feedTimer->setInterval(FEED_INTERVAL_MS);
    m_feedTimer->setTimerType(Qt::PreciseTimer);
    connect(m_feedTimer, &QTimer::timeout, this, &AudioPlaybackEngine::onFeedTimer);
}

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    destroySink();
}
// createSink вызывается ОДИН РАЗ при первом startPlayback.
// Повторные startPlayback переиспользуют существующий sink через reset().
// Это устраняет утечку WASAPI-потоков при частых перезапусках.
void AudioPlaybackEngine::createSink()
{
    QAudioFormat fmt;
    fmt.setSampleRate(SAMPLE_RATE);
    fmt.setChannelCount(CHANNELS);
    fmt.setSampleFormat(QAudioFormat::Float);

    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    m_sink = new QAudioSink(dev, fmt, this);
    m_sinkDevice = m_sink->start();

    if (m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();

#ifndef QT_NO_DEBUG
    qDebug() << "AudioPlaybackEngine: sink state=" << m_sink->state()
             << "bufSize=" << m_sink->bufferSize()
             << "device=" << dev.description()
             << "sinkDev=" << (m_sinkDevice ? "OK" : "NULL");
#endif
}

void AudioPlaybackEngine::destroySink()
{
    m_feedTimer->stop();
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
        m_sinkDevice = nullptr;
    }
    m_playing = false;
}

void AudioPlaybackEngine::startPlayback(double fromTime, double speed, double totalDuration)
{
    m_feedTimer->stop();

    // Создаём sink только при первом вызове или если был уничтожен.
    // НЕ пересоздаём на каждый startPlayback — это создаёт новый WASAPI-поток.
    // Windows лимит ~64 потока → после ~20 перемоток: AvSetMmThreadCharacteristics failed.
    if (!m_sink)
    {
        createSink();
    }
    else
    {
        // Сбрасываем буфер sink без пересоздания потока.
        // reset() → StoppedState, start() → снова push-режим.
        m_sink->reset();
        m_sinkDevice = m_sink->start();
        if (m_sink->state() == QAudio::SuspendedState)
            m_sink->resume();
    }

    m_startStreamTime = fromTime;
    m_writeHead = fromTime;
    m_speed = speed;
    m_playing = true;
    m_silentChunks = 0;
    m_totalDuration = totalDuration;
    m_playStartMs = QDateTime::currentMSecsSinceEpoch();
    m_playStartTime = fromTime;

    // Предзаполнение буфера: 3 итерации = ~240ms запас до первого тика таймера.
    // Без этого первые 20-40мс воспроизведения могут дать щелчок/тишину.
    for (int i = 0; i < 3; ++i) onFeedTimer();
    m_feedTimer->start();

#ifndef QT_NO_DEBUG
    qDebug() << "AudioPlaybackEngine: startPlayback from" << fromTime
             << "speed=" << speed << "totalDur=" << totalDuration;
#endif
}

void AudioPlaybackEngine::stopPlayback()
{
    m_feedTimer->stop();
    m_playing = false;
    // Не уничтожаем sink — только приостанавливаем.
    // Sink переиспользуется при следующем startPlayback через reset()+start().
    if (m_sink && m_sink->state() == QAudio::ActiveState)
        m_sink->suspend();
#ifndef QT_NO_DEBUG
    qDebug() << "AudioPlaybackEngine: stopped";
#endif
}

void AudioPlaybackEngine::setVolume(float vol)
{
    if (m_sink) m_sink->setVolume(vol);
}

void AudioPlaybackEngine::setTrackMuted(int track, bool muted)
{
    if (track == 1) m_track1Muted = muted;
    else m_track2Muted = muted;
}

double AudioPlaybackEngine::getCurrentAudioTime() const
{
    if (!m_playing) return m_startStreamTime;

    if (!m_sink)
    {
        qint64 ms = QDateTime::currentMSecsSinceEpoch() - m_playStartMs;
        return m_playStartTime + (ms / 1000.0) * m_speed;
    }

    // m_writeHead = сколько секунд timeline-аудио отправлено в sink.
    // sink буферизует часть данных, которые ещё не проиграны динамиком.
    // Реальная слышимая позиция = writeHead − буфер_sink (в timeline-секундах).
    qint64 bufferedBytes = m_sink->bufferSize() - m_sink->bytesFree();
    if (bufferedBytes < 0) bufferedBytes = 0;
    double bufferedSec = (double)bufferedBytes
                         / (double)(sizeof(float) * CHANNELS * SAMPLE_RATE);

    double audibleTime = m_writeHead - bufferedSec * m_speed;
    return qMax(m_startStreamTime, audibleTime);
}

void AudioPlaybackEngine::onFeedTimer()
{
    if (!m_playing || !m_sink || !m_sinkDevice) return;

    if (m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();

    qint64 bytesFree = m_sink->bytesFree();
    if (bytesFree < (qint64)(sizeof(float) * CHANNELS * 512)) // мин. 11ms запаса
    {
        emit timeUpdated(getCurrentAudioTime());
        return;
    }

    const double MAX_CHUNK_SEC = 0.08; // 80ms — равномернее чем 150ms
    int maxFloats  = (int)(MAX_CHUNK_SEC * SAMPLE_RATE * CHANNELS);
    int freeFloats = (int)(bytesFree / sizeof(float));
    int wantFloats = qMin(freeFloats, maxFloats);
    double chunkDur = (double)wantFloats / (SAMPLE_RATE * CHANNELS);
    double readDur  = chunkDur * m_speed;

    QVector<float> audio = m_timeline->getMixedAudio(
        m_writeHead, readDur, m_track1Muted, m_track2Muted);

    QVector<float> toWrite;
    if (!audio.isEmpty())
    {
        if (qAbs(m_speed - 1.0) > 0.01 && audio.size() != wantFloats)
        {
            toWrite.resize(wantFloats);
            double ratio = (double)audio.size() / wantFloats;
            for (int i = 0; i < wantFloats; i += CHANNELS)
            {
                double srcPos = (i / CHANNELS) * ratio * CHANNELS;
                int src0 = ((int)(srcPos / CHANNELS)) * CHANNELS;
                int src1 = qMin(src0 + CHANNELS, audio.size() - CHANNELS);
                double frac = (srcPos - src0) / CHANNELS;

                for (int c = 0; c < CHANNELS; c++)
                {
                    float s0 = (src0 + c < audio.size()) ? audio[src0 + c] : 0.0f;
                    float s1 = (src1 + c < audio.size()) ? audio[src1 + c] : 0.0f;
                    toWrite[i + c] = s0 + (s1 - s0) * (float)frac;
                }
            }
        }
        else
        {
            toWrite = audio;
            toWrite.resize(wantFloats, 0.0f);
        }
        m_writeHead += readDur;
        m_silentChunks = 0;
    }
    else
    {
        toWrite.resize(wantFloats, 0.0f);
        m_writeHead += readDur;
        ++m_silentChunks;
    }

    m_sinkDevice->write(
        reinterpret_cast<const char*>(toWrite.constData()),
        toWrite.size() * sizeof(float));

    emit timeUpdated(getCurrentAudioTime());

    bool pastEnd     = (m_writeHead >= m_totalDuration - 0.05);
    bool longSilence = (m_silentChunks >= 30)
                       && (m_writeHead - m_startStreamTime > 5.0);

    if (pastEnd || longSilence)
    {
#ifndef QT_NO_DEBUG
        qDebug() << "AudioPlaybackEngine: end detected, writeHead=" << m_writeHead;
#endif
        QTimer::singleShot(300, this, [this]()
                           {
                               if (m_playing)
                               {
                                   stopPlayback();
                                   emit playbackEnded();
                               }
                           });
    }
}





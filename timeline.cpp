#include "timeline.h"

// Тяжёлые include
#include "FrameCache.h"
#include "decoderthread.h"
#include "mediadecoder.h"
#include "renderengine.h"
#include "audioplaybackengine.h"
#include "EffectImageProvider.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QSet>
#include <QDateTime>
#include <cmath>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Timeline::Timeline(QObject *parent)
    : QObject(parent)
    , m_currentTime(0.0)
    , m_renderEngine(nullptr)
{
#ifndef QT_NO_DEBUG
    qDebug() << "Timeline constructor";
#endif
}

Timeline::~Timeline()
{
    cancelRender();

    if (m_audioEngine)
    {
        m_audioEngine->stopPlayback();
        delete m_audioEngine;
    }
    for (auto* thread : m_decoderThreads.values())
    {
        thread->stop();
        delete thread;
    }
    for (auto* d : m_audioDecoders.values())
    {
        d->closeFile();
        delete d;
    }
    qDeleteAll(m_frameCaches);

#ifndef QT_NO_DEBUG
    qDebug() << "Timeline destructor";
#endif
}


//  ПОЛУЧИТЬ КЛИПЫ ДЛЯ ДОРОЖКИ (для QML)

// Найти индекс клипа по стабильному UID (хранится в effects["_uid"])
// Если UID не найден — возвращает -1

static int findClipIndex(const QList<TimelineClip>& clips, int uid) {
    for (int i = 0; i < clips.size(); ++i) {
        if (static_cast<int>(clips[i].effects.value("_uid", -1)) == uid)
            return i;
    }
    return -1;
}

// Вспомогательная: разрешить "uid-or-index" —
// если clip.effects["_uid"] существует → ищем по uid, иначе — по индексу (обратная совместимость)
static int resolveIndex(const QList<TimelineClip>& clips, int uidOrIndex)
{
    // Сначала ищем как UID
    int byUid = findClipIndex(clips, uidOrIndex);
    if (byUid >= 0) return byUid;
    // Fallback: прямой индекс
    if (uidOrIndex >= 0 && uidOrIndex < clips.size()) return uidOrIndex;
    return -1;
}

QVariantList Timeline::getClipsForTrack(int trackIndex)
{
    QVariantList result;

    for (int i = 0; i < m_clips.size(); ++i) {
        const TimelineClip& clip = m_clips[i];

        if (clip.trackIndex == trackIndex)
        {
            QVariantMap clipMap;

            // Стабильный UID (если есть) или индекс как fallback
            int uid = static_cast<int>(clip.effects.value("_uid", -1));
            clipMap["id"] = (uid >= 0) ? uid : i;
            clipMap["filepath"] = clip.filepath;
            clipMap["startTime"] = clip.startTime;
            clipMap["duration"] = clip.duration;

            QFileInfo fileInfo(clip.filepath);
            clipMap["filename"] = fileInfo.fileName();

            clipMap["trimStart"] = clip.trimStart;
            clipMap["trimEnd"] = clip.trimEnd;

            clipMap["audioOffset"] = clip.audioOffset;
            clipMap["isMuted"] = clip.isMuted;

            QVariantMap effectsMap;
            for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it)
            {
                effectsMap[it.key()] = it.value();
            }
            clipMap["effects"] = effectsMap;

            clipMap["selected"] = false;
            clipMap["thumbnailPath"] = "";

            result.append(clipMap);
        }
    }

    return result;
}

// Добавить клип на timeline
bool Timeline::addClip(const QString& filepath, int trackIndex, double startTime)
{
#ifndef QT_NO_DEBUG
    qDebug() << "   Timeline::addClip!";
#endif
#ifndef QT_NO_DEBUG
    qDebug() << "   filepath:" << filepath;
#endif
#ifndef QT_NO_DEBUG
    qDebug() << "   trackIndex:" << trackIndex;
#endif
#ifndef QT_NO_DEBUG
    qDebug() << "   startTime:" << startTime;
#endif

    // 1. Проверить, существует ли файл
    if (!QFile::exists(filepath))
    {
        qWarning() << "File not found:" << filepath;
        return false;
    }

    // 2. Используем MediaDecoder для получения метаданных
    MediaDecoder decoder;

    if (!decoder.openFile(filepath))
    {
        qWarning() << "MediaDecoder failed to open file";
        return false;
    }

    double sourceDuration = decoder.getDuration();
    double fps = decoder.getFrameRate();

    if (sourceDuration <= 0) {
        qWarning() << "Cannot get video duration:" << filepath;
        decoder.closeFile();
        return false;
    }

    // КЭШИРУЕМ метаданные
    if (!m_clipMeta.contains(filepath)) {
        ClipMeta meta;
        meta.width  = decoder.getVideoWidth();
        meta.height = decoder.getVideoHeight();
        meta.fps    = fps;
        m_clipMeta[filepath] = meta;
    }

    decoder.closeFile();

#ifndef QT_NO_DEBUG
    qDebug() << "Video duration:" << sourceDuration << "sec";
#endif

    // 3 Создать новый клип
    TimelineClip newClip;
    newClip.filepath = filepath;
    newClip.trackIndex = trackIndex;
    newClip.startTime = startTime;
    newClip.duration = sourceDuration;
    newClip.trimStart = 0.0;
    newClip.trimEnd = 0.0;

    // 4 Проверить пересечения
    if (!canAddClip(trackIndex, startTime, sourceDuration))
    {
        qWarning() << "Clip overlap on track" << trackIndex;
        return false;
    }

    // 5 Добавить клип в список
    // Назначаем стабильный UID (не меняется при sortClips)
    newClip.effects["_uid"] = static_cast<double>(++m_nextUid);
    m_clips.append(newClip);

    // Запустить поток декодирования — один на каждый filepath|trackIndex
    QString dk = decoderKey(filepath, trackIndex);
    if (!m_decoderThreads.contains(dk))
    {
        auto* cache = new FrameCache();
        auto* thread = new DecoderThread(filepath, fps, cache, this);

        m_frameCaches[dk] = cache;
        m_decoderThreads[dk] = thread;

        connect(thread, &DecoderThread::frameReady, this, [this](int /*frameNum*/) {
            emit frameReady(QImage(), m_currentTime);
        });

        thread->start();
#ifndef QT_NO_DEBUG
        qDebug() << "DecoderThread started for:" << filepath;
#endif
    }

    sortClips();

    int newIndex = m_clips.size() - 1;

#ifndef QT_NO_DEBUG
    qDebug() << "Clip added! Index:" << newIndex << "Total:" << m_clips.size();
#endif

    emit clipsChanged();
    emit clipAdded(newIndex);
    emit totalDurationChanged();

    return true;
}

// УДАЛИТЬ КЛИП
bool Timeline::removeClip(int uidOrIndex)
{
    int index = resolveIndex(m_clips, uidOrIndex);
#ifndef QT_NO_DEBUG
    qDebug() << "removeClip: uid/idx=" << uidOrIndex << "→ index=" << index;
#endif

    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    m_clips.removeAt(index);

    emit clipsChanged();
    emit clipRemoved(index);
    emit totalDurationChanged();

    // Показать актуальный кадр после удаления клипа
    QMetaObject::invokeMethod(this, [this]() {
        // Если идёт воспроизведение — перезапускаем с точного аудио-времени.
        // Иначе AudioPlaybackEngine работает со старой структурой клипов
        if (m_audioEngine && m_audioEngine->isPlaying())
        {
            double exactTime = m_audioEngine->getCurrentAudioTime();
            stopPlayback();
            startPlayback(exactTime, m_playbackSpeed);
        }
        else
        {
            requestFrameForDisplay(m_currentTime);
        }
    }, Qt::QueuedConnection);

#ifndef QT_NO_DEBUG
    qDebug() << "Clip removed. Remaining:" << m_clips.size();
#endif
    return true;
}

// ===== ПЕРЕМЕСТИТЬ КЛИП =====
bool Timeline::moveClip(int uidOrIndex, int newTrackIndex, double newStartTime)
{
    int index = resolveIndex(m_clips, uidOrIndex);
#ifndef QT_NO_DEBUG
    qDebug() << "moveClip: uid/idx=" << uidOrIndex << "→ index=" << index
             << "-> track" << newTrackIndex << "time" << newStartTime;
#endif

    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    TimelineClip& clip = m_clips[index];

    if (!canAddClip(newTrackIndex, newStartTime, clip.duration, index))
    {
        qWarning() << "Overlap on move";
        return false;
    }

    clip.trackIndex = newTrackIndex;
    clip.startTime = newStartTime;

    sortClips();

    emit clipsChanged();
    emit clipModified(index);

    // Сброс аудио-декодеров: m_lastAudioPos устарел после изменения структуры клипов.
    // Декодер с устаревшей позицией читает последовательно с неверного места → шуршание.
    qDeleteAll(m_audioDecoders);
    m_audioDecoders.clear();

    QMetaObject::invokeMethod(this, [this]() {
        if (m_audioEngine && m_audioEngine->isPlaying())
        {
            double t = m_audioEngine->getCurrentAudioTime();
            stopPlayback(); startPlayback(t, m_playbackSpeed);
        }
        else
        {
            requestFrameForDisplay(m_currentTime);
        }
    }, Qt::QueuedConnection);

#ifndef QT_NO_DEBUG
    qDebug() << "Clip moved";
#endif
    return true;
}

// ===== РАЗРЕЗАТЬ КЛИП =====
bool Timeline::splitClip(int uidOrIndex, double splitTime)
{
    int index = resolveIndex(m_clips, uidOrIndex);
#ifndef QT_NO_DEBUG
    qDebug() << "splitClip: uid/idx=" << uidOrIndex << "→ index=" << index << "at" << splitTime;
#endif

    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    if (splitTime <= m_clips[index].startTime || splitTime >= m_clips[index].endTime())
    {
        qWarning() << "splitTime outside clip bounds";
        return false;
    }

    TimelineClip firstClip  = m_clips[index];
    TimelineClip secondClip = m_clips[index];

    double cutOffset = splitTime - firstClip.startTime;

    firstClip.duration = cutOffset;
    firstClip.trimEnd  = secondClip.trimEnd + (secondClip.duration - cutOffset);

    secondClip.startTime = splitTime;
    secondClip.duration  = secondClip.duration - cutOffset;
    secondClip.trimStart = secondClip.trimStart + cutOffset;

    // Part A сохраняет ОРИГИНАЛЬНЫЙ UID — QML не теряет ссылку после разреза.
    // Part B получает новый UID.
    secondClip.effects["_uid"] = static_cast<double>(++m_nextUid);

    // Переходы: Part A сохраняет вход, Part B сохраняет выход
    // (разрез посередине не должен наследовать оба перехода)
    firstClip.effects.remove("transition_out");   // вход у первой части остаётся
    secondClip.effects.remove("transition_in");   // выход у второй части остаётся
    // duration оставляем у обоих (если нужно вернуть переходы вручную)

    m_clips[index] = firstClip;
    m_clips.append(secondClip);
    sortClips();

#ifndef QT_NO_DEBUG
    qDebug() << "Clip split into 2 parts";
#endif
#ifndef QT_NO_DEBUG
    qDebug() << "   Part A: start=" << firstClip.startTime
             << "dur=" << firstClip.duration
             << "trimS=" << firstClip.trimStart
             << "trimE=" << firstClip.trimEnd;
#endif
#ifndef QT_NO_DEBUG
    qDebug() << "   Part B: start=" << secondClip.startTime
             << "dur=" << secondClip.duration
             << "trimS=" << secondClip.trimStart
             << "trimE=" << secondClip.trimEnd;
#endif

    // После разреза secondClip имеет новый trimStart — кэш невалиден
    const QString& fp = firstClip.filepath;
    QString dk = decoderKey(fp, firstClip.trackIndex);
    if (m_frameCaches.contains(dk))
        m_frameCaches[dk]->clear();
    if (m_decoderThreads.contains(dk))
        m_decoderThreads[dk]->seekTo(toSourceTime(fp, firstClip.trackIndex, m_currentTime));

    emit clipsChanged();

    // Сброс аудио-декодеров: m_lastAudioPos устарел после изменения структуры клипов.
    // Декодер с устаревшей позицией читает последовательно с неверного места → шуршание.
    qDeleteAll(m_audioDecoders);
    m_audioDecoders.clear();

    QMetaObject::invokeMethod(this, [this]() {
        // Если идёт воспроизведение — перезапускаем с точного аудио-времени.
        // Иначе AudioPlaybackEngine работает со старой структурой клипов
        if (m_audioEngine && m_audioEngine->isPlaying())
        {
            double exactTime = m_audioEngine->getCurrentAudioTime();
            stopPlayback();
            startPlayback(exactTime, m_playbackSpeed);
        }
        else
        {
            requestFrameForDisplay(m_currentTime);
        }
    }, Qt::QueuedConnection);

    return true;
}

// ОБРЕЗАТЬ КЛИП
bool Timeline::trimClip(int uidOrIndex, double newTrimStart, double newTrimEnd)
{
    int index = resolveIndex(m_clips, uidOrIndex);
#ifndef QT_NO_DEBUG
    qDebug() << "trimClip: uid/idx=" << uidOrIndex << "→ index=" << index
             << "trim" << newTrimStart << "-" << newTrimEnd;
#endif

    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    TimelineClip& clip = m_clips[index];

    clip.trimStart = newTrimStart;
    clip.trimEnd = newTrimEnd;

    MediaDecoder decoder;
    if (decoder.openFile(clip.filepath))
    {
        double sourceDuration = decoder.getDuration();
        clip.duration = sourceDuration - newTrimStart - newTrimEnd;
        decoder.closeFile();
    }

    if (clip.duration <= 0) {
        qWarning() << "Trim too large";
        return false;
    }

    // Кэш содержит кадры по старым номерам (вычисленным из старого trimStart).
    // После изменения trimStart frameNum = clipTime*fps даёт другой номер -
    // кэш промахивается - чёрный экран.
    if (m_frameCaches.contains(decoderKey(clip.filepath, clip.trackIndex)))
        m_frameCaches[decoderKey(clip.filepath, clip.trackIndex)]->clear();
    if (m_decoderThreads.contains(decoderKey(clip.filepath, clip.trackIndex)))
        m_decoderThreads[decoderKey(clip.filepath, clip.trackIndex)]->seekTo(toSourceTime(clip.filepath, clip.trackIndex, m_currentTime));

    emit clipModified(index);
    emit totalDurationChanged();
    emit clipsChanged();

    // Обновить превью после обрезки
    // Сброс аудио-декодеров: m_lastAudioPos устарел после изменения структуры клипов.
    // Декодер с устаревшей позицией читает последовательно с неверного места → шуршание.
    qDeleteAll(m_audioDecoders);
    m_audioDecoders.clear();

    QMetaObject::invokeMethod(this, [this]() {
        // Если идёт воспроизведение — перезапускаем с точного аудио-времени.
        // Иначе AudioPlaybackEngine работает со старой структурой клипов
        if (m_audioEngine && m_audioEngine->isPlaying())
        {
            double exactTime = m_audioEngine->getCurrentAudioTime();
            stopPlayback();
            startPlayback(exactTime, m_playbackSpeed);
        } else {
            requestFrameForDisplay(m_currentTime);
        }
    }, Qt::QueuedConnection);

#ifndef QT_NO_DEBUG
    qDebug() << "Clip trimmed. New duration:" << clip.duration;
#endif
    return true;
}

// ОБРЕЗКА ЛЕВОГО КРАЯ КЛИПА
bool Timeline::setClipLeftTrim(int uidOrIndex, double newStartTime, double newTrimStart)
{
    int index = resolveIndex(m_clips, uidOrIndex);
#ifndef QT_NO_DEBUG
    qDebug() << "setClipLeftTrim: uid/idx=" << uidOrIndex << "→ index=" << index
             << "newStart=" << newStartTime
             << "newTrimStart=" << newTrimStart;
#endif

    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    if (newTrimStart < 0.0) newTrimStart = 0.0;

    double oldEndTimeline = m_clips[index].startTime + m_clips[index].duration;
    double newDuration = oldEndTimeline - newStartTime;

    if (newDuration < 0.1)
    {
        qWarning() << "Clip too short:" << newDuration;
        return false;
    }

    m_clips[index].startTime = newStartTime;
    m_clips[index].trimStart = newTrimStart;
    m_clips[index].duration  = newDuration;

    const QString& fp = m_clips[index].filepath;
    int trk = m_clips[index].trackIndex;
    QString dk = decoderKey(fp, trk);
    if (m_frameCaches.contains(dk))
        m_frameCaches[dk]->clear();
    if (m_decoderThreads.contains(dk))
        m_decoderThreads[dk]->seekTo(toSourceTime(fp, trk, m_currentTime));

    emit clipsChanged();
    emit totalDurationChanged();

    // Сброс аудио-декодеров: m_lastAudioPos устарел после изменения структуры клипов.
    // Декодер с устаревшей позицией читает последовательно с неверного места → шуршание.
    qDeleteAll(m_audioDecoders);
    m_audioDecoders.clear();

    QMetaObject::invokeMethod(this, [this]() {
        // Если идёт воспроизведение — перезапускаем с точного аудио-времени.
        // Иначе AudioPlaybackEngine работает со старой структурой клипов
        if (m_audioEngine && m_audioEngine->isPlaying()) {
            double exactTime = m_audioEngine->getCurrentAudioTime();
            stopPlayback();
            startPlayback(exactTime, m_playbackSpeed);
        } else {
            requestFrameForDisplay(m_currentTime);
        }
    }, Qt::QueuedConnection);

#ifndef QT_NO_DEBUG
    qDebug() << "setClipLeftTrim: start=" << m_clips[index].startTime
             << "trimStart=" << m_clips[index].trimStart
             << "duration=" << m_clips[index].duration;
#endif
    return true;
}

// ===== ПРИМЕНИТЬ ЭФФЕКТ =====
bool Timeline::applyEffect(int uidOrIndex, const QString& effectName, double value)
{
    int index = resolveIndex(m_clips, uidOrIndex);
#ifndef QT_NO_DEBUG
    qDebug() << "applyEffect: uid/idx=" << uidOrIndex << "→ index=" << index << effectName << "=" << value;
#endif

    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    m_clips[index].effects[effectName] = value;
    emit clipModified(index);

#ifndef QT_NO_DEBUG
    qDebug() << "Effect applied";
#endif
    return true;
}

bool Timeline::removeEffect(int uidOrIndex, const QString& effectName)
{
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size()) return false;
    m_clips[index].effects.remove(effectName);
    emit clipModified(index);
    return true;
}

QVariantMap Timeline::getClipEffects(int uidOrIndex) const
{
    QVariantMap result;
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size()) return result;
    const auto& effects = m_clips[index].effects;
    for (auto it = effects.begin(); it != effects.end(); ++it)
    {
        result[it.key()] = it.value();
    }
    return result;
}

// ===== ПРОВЕРКА ПЕРЕСЕЧЕНИЙ =====
bool Timeline::canAddClip(int trackIndex, double startTime, double duration, int excludeIndex) const
{
    double endTime = startTime + duration;

    for (int i = 0; i < m_clips.size(); ++i)
    {
        if (i == excludeIndex) continue;
        const TimelineClip& existing = m_clips[i];
        if (existing.trackIndex != trackIndex) continue;
        if (!(endTime <= existing.startTime || startTime >= existing.endTime()))
        {
            return false;
        }
    }
    return true;
}

// СОРТИРОВКА
void Timeline::sortClips()
{
    std::sort(m_clips.begin(), m_clips.end(), [](const TimelineClip& a, const TimelineClip& b)
              {
                  if (a.trackIndex != b.trackIndex) return a.trackIndex < b.trackIndex;
                  return a.startTime < b.startTime;
              });
}

//  ПОЛУЧИТЬ ОБЩУЮ ДЛИТЕЛЬНОСТЬ
double Timeline::totalDuration() const
{
    if (m_clips.isEmpty()) return 100.0;

    double maxEnd = 0.0;
    for (const TimelineClip& clip : m_clips)
    {
        double end = clip.endTime();
        if (end > maxEnd) maxEnd = end;
    }
    return maxEnd + 20.0;
}

//  СЕТТЕР ВРЕМЕНИ
void Timeline::setCurrentTime(double time)
{
    if (qAbs(m_currentTime - time) < 0.01) return;
    m_currentTime = time;
    bool playing = m_audioEngine && m_audioEngine->isPlaying();
    if (!playing)
    {
        for (auto it = m_decoderThreads.begin(); it != m_decoderThreads.end(); ++it) {
            QString fp; int trk;
            parseDecoderKey(it.key(), fp, trk);
            it.value()->seekTo(toSourceTime(fp, trk, time));
        }
    }
    emit currentTimeChanged();
}

// ПОЛУЧИТЬ КЛИП ПО ИНДЕКСУ
TimelineClip* Timeline::getClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return nullptr;
    return &m_clips[index];
}

//  ПОЛУЧИТЬ КЛИП В ПОЗИЦИИ
TimelineClip* Timeline::getClipAt(double time, int trackIndex)
{
    for (int i = 0; i < m_clips.size(); ++i)
    {
        TimelineClip& clip = m_clips[i];
        if (clip.trackIndex == trackIndex &&
            time >= clip.startTime &&
            time < clip.endTime())
        {
            return &clip;
        }
    }
    return nullptr;
}

//  ПОЛУЧИТЬ КАДР ДЛЯ PREVIEW
QImage Timeline::getCurrentFrameAt(double time, int trackIndex)
{
    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return QImage();

    double clipTime = time - clip->startTime + clip->trimStart;
    double fps = 25.0;
    QString dk = decoderKey(clip->filepath, clip->trackIndex);

    if (m_decoderThreads.contains(dk))
    {
        fps = m_decoderThreads[dk]->getFps();
    }
    else if (m_clipMeta.contains(clip->filepath) && m_clipMeta[clip->filepath].fps > 0)
    {
        fps = m_clipMeta[clip->filepath].fps;
    }

    int frameNum = (int)(clipTime * fps + 0.5); // round вместо floor — согласованно с DecoderThread

    // Сообщаем текущую позицию чтобы вытеснение
    // не удаляло кадры рядом с текущей позицией воспроизведения.
    if (m_frameCaches.contains(dk))
    {
        m_frameCaches[dk]->setPlayPosition(frameNum);
        QImage cached;
        if (m_frameCaches[dk]->getNearest(frameNum, cached))
        {
            return cached;
        }
    }

    //  Промах кэша:
    // Во время воспроизведения sync decode блокирует UI - запрещаем.
    // Исключение: m_forceNextFrame=true — первый кадр после startPlayback/seek.
    // В этот момент намеренно блокируем UI на 1 кадр чтобы не было чёрного.
    bool playingNow = m_audioEngine && m_audioEngine->isPlaying();
    if (playingNow && !m_forceNextFrame) return QImage();
    m_forceNextFrame = false; // сбрасываем после первого использования

    // На паузе - sync decode
#ifndef QT_NO_DEBUG
    qDebug() << "Cache miss for time=" << clipTime << "- sync decode";
#endif
    MediaDecoder decoder;
    if (!decoder.openFile(clip->filepath)) return QImage();
    QImage frame = decoder.getFrameAt(clipTime);
    decoder.closeFile();

    if (!frame.isNull() && m_frameCaches.contains(dk))
    {
        m_frameCaches[dk]->put(frameNum, frame);
    }

    return frame;
}


// ===== СОХРАНЕНИЕ ПРОЕКТА =====
bool Timeline::saveProject(const QString& filepath)
{
    QString cleanPath = filepath;
    if (cleanPath.startsWith("file:///"))
    {
        cleanPath = cleanPath.mid(8);
    }

#ifndef QT_NO_DEBUG
    qDebug() << "saveProject:" << filepath;
#endif

    QJsonObject json = toJson();
    QJsonDocument doc(json);
    QFile file(filepath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "Cannot open file for writing:" << filepath;
        return false;
    }

    file.write(doc.toJson());
    file.close();

#ifndef QT_NO_DEBUG
    qDebug() << "Project saved:" << filepath;
#endif
    return true;
}

// ===== ЗАГРУЗКА ПРОЕКТА =====
bool Timeline::loadProject(const QString& filepath)
{
    QString cleanPath = filepath;
    if (cleanPath.startsWith("file:///"))
    {
        cleanPath = cleanPath.mid(8);
    }

#ifndef QT_NO_DEBUG
    qDebug() << "loadProject:" << filepath;
#endif

    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot open file for reading:" << filepath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qWarning() << "Invalid JSON";
        return false;
    }

    bool success = fromJson(doc.object());

    if (success)
    {
        for (const TimelineClip& clip : m_clips)
        {
            if (!m_decoderThreads.contains(decoderKey(clip.filepath, clip.trackIndex)) && QFile::exists(clip.filepath))
            {
                MediaDecoder dec;
                double fps = 25.0;
                if (dec.openFile(clip.filepath))
                {
                    fps = dec.getFrameRate();

                    if (!m_clipMeta.contains(clip.filepath))
                    {
                        ClipMeta meta;
                        meta.width  = dec.getVideoWidth();
                        meta.height = dec.getVideoHeight();
                        meta.fps    = fps;
                        m_clipMeta[clip.filepath] = meta;
                    }

                    dec.closeFile();
                }

                auto* cache = new FrameCache();
                auto* thread = new DecoderThread(clip.filepath, fps, cache, this);
                QString dk = decoderKey(clip.filepath, clip.trackIndex);
                m_frameCaches[dk] = cache;
                m_decoderThreads[dk] = thread;

                connect(thread, &DecoderThread::frameReady, this, [this](int)
                        {
                            emit frameReady(QImage(), m_currentTime);
                        });

                thread->start();
#ifndef QT_NO_DEBUG
                qDebug() << "DecoderThread started for:" << clip.filepath;
#endif
            }
        }

        emit clipsChanged();
        emit totalDurationChanged();
#ifndef QT_NO_DEBUG
        qDebug() << "Project loaded:" << cleanPath;
#endif
    }

    return success;
}

//  КОНВЕРТАЦИЯ В JSON
QJsonObject Timeline::toJson() const
{
    QJsonObject json;
    json["version"] = "1.0";
    json["currentTime"] = m_currentTime;

    QJsonArray clipsArray;
    for (const TimelineClip& clip : m_clips)
    {
        QJsonObject clipObj;
        clipObj["filepath"] = clip.filepath;
        clipObj["trackIndex"] = clip.trackIndex;
        clipObj["startTime"] = clip.startTime;
        clipObj["duration"] = clip.duration;
        clipObj["trimStart"] = clip.trimStart;
        clipObj["trimEnd"] = clip.trimEnd;
        clipObj["audioOffset"] = clip.audioOffset;
        clipObj["isMuted"] = clip.isMuted;

        QJsonObject effectsObj;
        for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it)
        {
            effectsObj[it.key()] = it.value();
        }
        clipObj["effects"] = effectsObj;

        clipsArray.append(clipObj);
    }
    json["clips"] = clipsArray;

    return json;
}

//  КОНВЕРТАЦИЯ ИЗ JSON
bool Timeline::fromJson(const QJsonObject& json)
{
    if (!json.contains("clips")) return false;

    m_clips.clear();
    m_currentTime = json["currentTime"].toDouble();

    QJsonArray clipsArray = json["clips"].toArray();
    for (const QJsonValue& value : clipsArray)
    {
        QJsonObject clipObj = value.toObject();

        TimelineClip clip;
        clip.filepath = clipObj["filepath"].toString();
        clip.trackIndex = clipObj["trackIndex"].toInt();
        clip.startTime = clipObj["startTime"].toDouble();
        clip.duration = clipObj["duration"].toDouble();
        clip.trimStart = clipObj["trimStart"].toDouble();
        clip.trimEnd = clipObj["trimEnd"].toDouble();
        clip.audioOffset = clipObj["audioOffset"].toDouble();
        clip.isMuted = clipObj["isMuted"].toBool();

        QJsonObject effectsObj = clipObj["effects"].toObject();
        for (auto it = effectsObj.begin(); it != effectsObj.end(); ++it)
        {
            clip.effects[it.key()] = it.value().toDouble();
        }

        // Восстанавливаем UID или назначаем новый
        if (!clip.effects.contains("_uid"))
        {
            clip.effects["_uid"] = static_cast<double>(++m_nextUid);
        } else
        {
            int existingUid = static_cast<int>(clip.effects.value("_uid"));
            if (existingUid > m_nextUid) m_nextUid = existingUid;
        }
        m_clips.append(clip);
    }

    return true;
}


// МЕТАДАННЫЕ КЛИПА

QVariantMap Timeline::getClipInfoAt(double time, int trackIndex)
{
    QVariantMap info;
    info["width"] = 0;
    info["height"] = 0;
    info["fps"] = 0.0;
    info["startTime"] = 0.0;
    info["trimStart"] = 0.0;
    info["duration"] = 0.0;

    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return info;

    info["startTime"] = clip->startTime;
    info["trimStart"] = clip->trimStart;
    info["duration"]  = clip->duration;

    if (m_clipMeta.contains(clip->filepath))
    {
        const ClipMeta& meta = m_clipMeta[clip->filepath];
        info["width"] = meta.width;
        info["height"] = meta.height;
        info["fps"] = meta.fps;
    } else {
        MediaDecoder decoder;
        if (decoder.openFile(clip->filepath))
        {
            ClipMeta meta;
            meta.width  = decoder.getVideoWidth();
            meta.height = decoder.getVideoHeight();
            meta.fps    = decoder.getFrameRate();
            m_clipMeta[clip->filepath] = meta;

            info["width"]  = meta.width;
            info["height"] = meta.height;
            info["fps"]    = meta.fps;
            decoder.closeFile();
        }
    }

    return info;
}


// splitClipAt, setClipMuted, getTrackEndTime


bool Timeline::splitClipAt(double time, int trackIndex)
{
    for (int i = 0; i < m_clips.size(); ++i)
    {
        const TimelineClip& c = m_clips[i];
        if (c.trackIndex == trackIndex
            && time > c.startTime
            && time < c.endTime())
        {
#ifndef QT_NO_DEBUG
            qDebug() << "splitClipAt: found clip" << i << "time=" << time;
#endif
            return splitClip(i, time);
        }
    }
    qWarning() << "splitClipAt: no clip at time=" << time << "track=" << trackIndex;
    return false;
}

bool Timeline::setClipMuted(int uidOrIndex, bool muted)
{
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size())
    {
        qWarning() << "setClipMuted: invalid index" << index;
        return false;
    }
    m_clips[index].isMuted = muted;
    emit clipModified(index);
#ifndef QT_NO_DEBUG
    qDebug() << "Clip uid/idx=" << uidOrIndex << "→" << index << (muted ? "muted" : "unmuted");
#endif
    return true;
}

double Timeline::getTrackEndTime(int trackIndex) const
{
    double maxEnd = 0.0;
    for (const TimelineClip& c : m_clips)
    {
        if (c.trackIndex == trackIndex)
        {
            double e = c.endTime();
            if (e > maxEnd) maxEnd = e;
        }
    }
    return maxEnd;
}

// ВИДИМОСТЬ КЛИПОВ (для рендеринга)


void Timeline::setClipVideoHidden(int uidOrIndex, bool hidden)
{
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index >= 0 && index < m_clips.size())
    {
        m_clips[index].isVideoHidden = hidden;
    }
}

void Timeline::setClipAudioHidden(int uidOrIndex, bool hidden)
{
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index >= 0 && index < m_clips.size())
    {
        m_clips[index].isAudioHidden = hidden;
    }
}

void Timeline::syncClipStatesForRender(QVariantMap hiddenMap, QVariantMap mutedMap) {
#ifndef QT_NO_DEBUG
    qDebug() << "syncClipStatesForRender:" << hiddenMap.size() << "hidden,"
             << mutedMap.size() << "muted";
#endif

    for (int i = 0; i < m_clips.size(); ++i)
    {
        // Пробуем ключи по UID (новый формат) и по индексу (обратная совместимость)
        int uid = static_cast<int>(m_clips[i].effects.value("_uid", -1));
        QString uidStr   = (uid >= 0) ? QString::number(uid) : QString();
        QString idxStr   = QString::number(i);

        // Video hidden
        QString vKeyUid = uidStr + "_v";
        QString vKeyIdx = idxStr + "_v";
        if (!uidStr.isEmpty() && hiddenMap.contains(vKeyUid))
            m_clips[i].isVideoHidden = hiddenMap.value(vKeyUid).toBool();
        else
            m_clips[i].isVideoHidden = hiddenMap.value(vKeyIdx, false).toBool();

        // Audio hidden
        QString aKeyUid = uidStr + "_a";
        QString aKeyIdx = idxStr + "_a";
        if (!uidStr.isEmpty() && hiddenMap.contains(aKeyUid))
            m_clips[i].isAudioHidden = hiddenMap.value(aKeyUid).toBool();
        else
            m_clips[i].isAudioHidden = hiddenMap.value(aKeyIdx, false).toBool();

        // Muted
        if (!uidStr.isEmpty() && mutedMap.contains(uidStr))
            m_clips[i].isMuted = mutedMap.value(uidStr).toBool();
        else if (mutedMap.contains(idxStr))
            m_clips[i].isMuted = mutedMap.value(idxStr).toBool();
    }
}

//  РЕНДЕРИНГ — запуск в отдельном потоке через RenderEngine


bool Timeline::renderToFile(const QString& outputPath, int width, int height, const QString& format)
{
#ifndef QT_NO_DEBUG
    qDebug() << "renderToFile:" << outputPath << width << "x" << height << "format:" << format;
#endif

    QString cleanPath = outputPath;
    if (cleanPath.startsWith("file:///"))
    {
        cleanPath = cleanPath.mid(8);
        // Windows: /C:/path -> C:/path
        if (cleanPath.length() > 2 && cleanPath[0] == '/' &&
            cleanPath[2] == ':')
        {
            cleanPath = cleanPath.mid(1);
        }
    }

    if (m_clips.isEmpty())
    {
        qWarning() << "No clips for rendering";
        emit renderFinished(false);
        return false;
    }

    // Убить предыдущий рендер если был
    cancelRender();

    // Создать RenderEngine
    m_renderEngine = new RenderEngine(this);
    m_renderEngine->setClips(m_clips);
    m_renderEngine->setOutputPath(cleanPath);
    m_renderEngine->setOutputResolution(width, height);
    m_renderEngine->setOutputFormat(format);

    // ── Битрейт видео зависит от разрешения ──────────────────────────────────
    // Используем ВЕРХНИЙ предел рекомендованного диапазона — лучше качество.
    // libx264 с CRF=18 переопределит битрейт автоматически (bit_rate=0 при CRF).
    // Для h264_mf / mpeg4 битрейт — единственный регулятор качества.
    {
        int pixels = width * height;
        int vbr;
        if      (pixels <= 640  * 360)  vbr =  2500000;  //  360p →  2.5 Mbps
        else if (pixels <= 1280 * 720)  vbr =  8000000;  //  720p →  8 Mbps
        else if (pixels <= 1920 * 1080) vbr = 12000000;  // 1080p → 12 Mbps
        else if (pixels <= 2560 * 1440) vbr = 25000000;  // 1440p → 25 Mbps
        else                            vbr = 40000000;  //   4K  → 40 Mbps
        m_renderEngine->setBitrate(vbr);
    }

    // Определить FPS из первого клипа
    double fps = 30.0;
    if (!m_clips.isEmpty() && m_clipMeta.contains(m_clips[0].filepath))
    {
        double cfps = m_clipMeta[m_clips[0].filepath].fps;
        if (cfps > 0) fps = cfps;
    }
    m_renderEngine->setFps(fps);

    // Подключить сигналы
    connect(m_renderEngine, &RenderEngine::progressChanged,
            this,           &Timeline::renderProgress);

    connect(m_renderEngine, &RenderEngine::renderFinished,
            this,           [this](bool success)
            {
#ifndef QT_NO_DEBUG
                qDebug() << (success ? "Render finished OK" : "Render FAILED");
#endif
                emit renderFinished(success);

                // Автоочистка
                if (m_renderEngine) {
                    m_renderEngine->deleteLater();
                    m_renderEngine = nullptr;
                }
            });

    // Запустить!
    return m_renderEngine->startRender();
}

void Timeline::cancelRender()
{
    if (m_renderEngine) {
        m_renderEngine->cancel();
        m_renderEngine->deleteLater();
        m_renderEngine = nullptr;
    }
}

void Timeline::playSystemBeep()
{
#ifdef Q_OS_WIN
    MessageBeep(MB_ICONASTERISK);
#else
    fprintf(stderr, "\a");
    fflush(stderr);
#endif
}

//  setImageProvider — вызвать из main.cpp ПОСЛЕ регистрации провайдера

void Timeline::setImageProvider(EffectImageProvider* provider)
{
    m_imageProvider = provider;
}


//  getOrCreateAudioDecoder — per-clip декодер аудио
//  clipKey уникален для каждого клипа (filepath|startTime|trimStart).
//  Без этого два клипа из одного файла делили один декодер →
//  m_audioOverflow и m_lastAudioPos одного портили аудио другого.

MediaDecoder* Timeline::getOrCreateAudioDecoder(const QString& clipKey, const QString& filepath)
{
    if (m_audioDecoders.contains(clipKey))
        return m_audioDecoders[clipKey];

    auto* dec = new MediaDecoder();
    if (!dec->openFile(filepath))
    {
        delete dec;
        return nullptr;
    }
    m_audioDecoders[clipKey] = dec;
    return dec;
}


//  getMixedAudio — аудио-микс двух дорожек для live воспроизведения

QVector<float> Timeline::getMixedAudio(double time, double duration,
                                       bool t1muted, bool t2muted)
{
    const int SR = 44100, CH = 2;
    int totalFloats = static_cast<int>(duration * SR * CH);
    if (totalFloats <= 0) return {};

    QVector<float> mixed(totalFloats, 0.0f);
    bool hasAudio = false;

    auto mixTrack = [&](int trackIndex, bool muted)
    {
        if (muted) return;
        TimelineClip* clip = getClipAt(time, trackIndex);
        if (!clip || clip->isMuted || clip->isAudioHidden) return;

        // Клип есть на дорожке — двигатель должен продолжать работать
        // даже если декодер ещё не вернул данные (прогрев)
        hasAudio = true;

        // ── PER-CLIP KEY — уникален для каждого разрезанного клипа ────
        QString clipKey = clip->filepath
                          + "|" + QString::number(clip->startTime, 'f', 4)
                          + "|" + QString::number(clip->trimStart, 'f', 4);

        MediaDecoder* dec = getOrCreateAudioDecoder(clipKey, clip->filepath);
        if (!dec || !dec->hasAudio()) return;

        double srcTime = clip->sourceTimeAt(time);

        // ── КРИТИЧНО: ограничиваем duration до конца клипа ───────────────────
        // Без этого последний 20мс-чанк может заехать за trimEnd.
        // Пример: clip.endTime()=17.5, time=17.49, duration=0.02 →
        // srcTime=17.99, читаем до 18.01 — это уже за trimEnd (18.00).
        // Слышим 10мс удалённой части источника — "аудио из обрезанного".
        double timeToClipEnd = clip->endTime() - time;
        double readDuration  = qMin(duration, timeToClipEnd);
        if (readDuration <= 0.0) return;

        QVector<float> audio = dec->decodeAudioRange(srcTime, readDuration);
        // Если запрошенный чанк длиннее прочитанного (конец клипа) — добиваем тишиной
        if (audio.isEmpty()) return;
        int wantFloats = static_cast<int>(duration * 44100 * 2);
        if (audio.size() < wantFloats)
            audio.resize(wantFloats, 0.0f); // тишина за концом клипа

        const QMap<QString,double>& eff = clip->effects;
        const int SR = 44100, CH = 2;

        // 1 Громкость
        float vol = (float)eff.value("volume", 1.0);
        if (qAbs(vol - 1.0f) > 0.01f)
            for (float& s : audio) s = qBound(-1.0f, s * vol, 1.0f);

        // 2 Моно
        if (eff.value("mono", 0.0) > 0.5)
            for (int i = 0; i + 1 < audio.size(); i += 2)
            {
                float m = (audio[i]+audio[i+1])*0.5f;
                audio[i] = audio[i+1] = m;
            }

        // 3 Расширение стерео
        if (eff.value("stereo_widen", 0.0) > 0.01)
        {
            float w = (float)eff.value("stereo_widen", 0.0);
            float sg = 1.0f + w*2.5f;
            for (int i = 0; i+1 < audio.size(); i += 2)
            {
                float mid  = (audio[i]+audio[i+1])*0.5f;
                float side = (audio[i]-audio[i+1])*0.5f;
                audio[i]   = qBound(-1.0f, mid + side*sg, 1.0f);
                audio[i+1] = qBound(-1.0f, mid - side*sg, 1.0f);
            }
        }

        // 4 Реверберация
        if (eff.value("reverb", 0.0) > 0.01)
        {
            double room = eff.value("reverb", 0.0);
            int D = qMax(CH*2, SR/1000*(int)(25+room*75)*CH);
            float g=(float)(room*0.65), wet=(float)(room*0.45), dry=1.0f-wet*0.6f;
            auto& buf = m_audioDelayBufs[clipKey+"_reverb"];
            auto& wp = m_audioDelayPos[clipKey+"_reverb"];
            if ((int)buf.size()!=D){buf.assign(D,0.0f);wp=0;}
            for (int i=0;i<audio.size();++i)
            {
                float del=buf[wp], rev=audio[i]+del*g;
                buf[wp]=rev;
                audio[i]=qBound(-1.0f,audio[i]*dry+rev*wet,1.0f);
                wp=(wp+1)%D;
            }
        }

        // 5 Эхо
        if (eff.value("echo", 0.0) > 0.01)
        {
            double str = eff.value("echo", 0.0);
            int D = qMax(CH*2, SR/1000*(int)(150+str*350)*CH);
            float fb=(float)(str*0.55);
            auto& buf = m_audioDelayBufs[clipKey+"_echo"];
            auto& wp  = m_audioDelayPos[clipKey+"_echo"];

            if ((int)buf.size()!=D)
            {
                buf.assign(D,0.0f);
                wp=0;
            }

            for (int i=0;i<audio.size();++i)
            {
                float del=buf[wp], out=audio[i]+del*fb;
                buf[wp]=qBound(-1.0f,out,1.0f);
                audio[i]=qBound(-1.0f,out,1.0f);
                wp=(wp+1)%D;
            }
        }

        // 6 Питч — ресэмплинг
        if (qAbs(eff.value("pitch",0.0)) > 0.1)
        {
            double ratio = std::pow(2.0, eff.value("pitch",0.0)/12.0);
            QVector<float> shifted(audio.size(),0.0f);
            for (int i=0;i<audio.size();++i)
            {
                double si=i*ratio; int s0=(int)si;
                int s1=qMin(s0+1,(int)audio.size()-1);
                if(s0>=(int)audio.size())break;
                float t=(float)(si-s0);
                shifted[i]=audio[s0]*(1.0f-t)+audio[s1]*t;
            }
            audio=shifted;
        }

        // 7. Нормализация
        if (eff.value("normalize",0.0) > 0.01)
        {
            float target=(float)eff.value("normalize",0.0), peak=0.0f;
            for (float s:audio) peak=qMax(peak,qAbs(s));
            if (peak>1e-5f){float g2=qMin(target/peak,6.0f);for(float&s:audio)s=qBound(-1.0f,s*g2,1.0f);}
        }

        // 8. Fade in/out
        if (eff.value("fade_in",0.0)>0.01)
        {
            double fe=clip->duration*eff.value("fade_in",0.0), ps=time-clip->startTime;
            if(ps<fe)for(int i=0;i<audio.size();++i)
                {
                    double t=ps+(double)i/(SR*CH);
                    audio[i]=qBound(-1.0f,audio[i]*(float)qBound(0.0,t/fe,1.0),1.0f);
                }
        }
        if (eff.value("fade_out",0.0)>0.01)
        {
            double fs=clip->duration*(1.0-eff.value("fade_out",0.0)), ps=time-clip->startTime;
            if(ps+duration>fs)for(int i=0;i<audio.size();++i)
                {
                    double t=ps+(double)i/(SR*CH), fl=clip->duration-fs;
                    float g3=(fl>0)?(float)qBound(0.0,1.0-(t-fs)/fl,1.0):0.0f;
                    audio[i]=qBound(-1.0f,audio[i]*g3,1.0f);
                }
        }

        int len = qMin(mixed.size(), audio.size());
        for (int i = 0; i < len; ++i)
            mixed[i] += audio[i];
    };

    mixTrack(1, t1muted);
    mixTrack(2, t2muted);

    if (!hasAudio) return {};

    // Зажать и очистить NaN/Inf перед отдачей в QAudioSink и AAC-энкодер.
    // Суммирование двух треков или эффекты pitch/normalize могут дать >1.0.
    // NaN от деления на ноль в normalize создаёт треск и шипение при кодировании.
    for (float& s : mixed)
    {
        if (!std::isfinite(s)) s = 0.0f;
        s = qBound(-1.0f, s, 1.0f);
    }
    return mixed;
}



//  alphaComposite — Porter-Duff «src over» для хромакея

QImage Timeline::alphaComposite(const QImage& fg, const QImage& bg)
{
    QImage fgA = fg.convertToFormat(QImage::Format_ARGB32);
    QImage bgA = bg.convertToFormat(QImage::Format_ARGB32);
    QImage result(qMin(fgA.width(),  bgA.width()),
                  qMin(fgA.height(), bgA.height()),
                  QImage::Format_RGB888);

    for (int y = 0; y < result.height(); ++y)
    {
        const QRgb* fgLine = reinterpret_cast<const QRgb*>(fgA.constScanLine(y));
        const QRgb* bgLine = reinterpret_cast<const QRgb*>(bgA.constScanLine(y));
        uchar* dstLine = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x)
        {
            float a = qAlpha(fgLine[x]) / 255.0f;
            float ia = 1.0f - a;
            dstLine[x*3] = (uchar)(qRed(fgLine[x]) * a + qRed(bgLine[x]) * ia);
            dstLine[x*3+1] = (uchar)(qGreen(fgLine[x]) * a + qGreen(bgLine[x]) * ia);
            dstLine[x*3+2] = (uchar)(qBlue(fgLine[x]) * a + qBlue(bgLine[x]) * ia);
        }
    }
    return result;
}


//  getCompositeFrame — декодируем оба трека + применяем эффекты

QImage Timeline::getCompositeFrame(double time,
                                   int selectedClipId,
                                   const QVariantMap& previewEffects)
{
    ++m_previewFrameIndex;

    auto getEffectsFor = [&](const TimelineClip* clip) -> QMap<QString, double>
    {
        QMap<QString, double> eff = clip->effects;

        // Если это выделенный клип И есть preview-эффекты — переопределяем
        if (selectedClipId >= 0 && !previewEffects.isEmpty())
        {
            int uid = static_cast<int>(clip->effects.value("_uid", -1));
            if (uid == selectedClipId)
            {
                for (auto it = previewEffects.constBegin();
                     it != previewEffects.constEnd(); ++it)
                {
                    eff[it.key()] = it.value().toDouble();
                }
            }
        }
        return eff;
    };

    QImage frame1, frame2;
    bool isPlayingNow = m_audioEngine && m_audioEngine->isPlaying();

    // Вспомогательная: применить эффекты + превью fade-переходов
    auto renderClip = [&](QImage& out, TimelineClip* clip)
    {
        QImage raw = getCurrentFrameAt(time, clip->trackIndex);
        if (raw.isNull()) return;

        auto eff = getEffectsFor(clip);

        // Быстрый путь при воспроизведении без эффектов
        bool hasRealEffects = false;
        for (auto it = eff.constBegin(); it != eff.constEnd(); ++it)
            if (it.key() != "_uid")
            {
                hasRealEffects = true; break;
            }

        QImage processed = (isPlayingNow && !hasRealEffects)
                               ? raw.convertToFormat(QImage::Format_RGB888)
                               : RenderEngine::applyEffectsToFrame(raw, eff, m_previewFrameIndex);

        //  Превью fade_in / fade_out
        // Значения хранятся как доля длины клипа (0.0–1.0)
        double fadeIn     = eff.value("fade_in",  0.0);
        double fadeOut    = eff.value("fade_out", 0.0);
        double posInClip  = time - clip->startTime;
        double posFromEnd = clip->endTime() - time;
        double fadeInSec  = fadeIn  * clip->duration;
        double fadeOutSec = fadeOut * clip->duration;

        auto applyDim = [](QImage& img, float alpha)
        {
            QImage a = img.convertToFormat(QImage::Format_RGB888);
            for (int y = 0; y < a.height(); ++y)
            {
                uchar* line = a.scanLine(y);
                for (int x = 0; x < a.width() * 3; ++x)
                    line[x] = (uchar)(line[x] * alpha);
            }
            img = a;
        };

        if (fadeInSec > 0.001 && posInClip < fadeInSec)
            applyDim(processed, (float)(posInClip / fadeInSec));

        if (fadeOutSec > 0.001 && posFromEnd < fadeOutSec)
            applyDim(processed, (float)(posFromEnd / fadeOutSec));

        out = processed;
    };

    // Дорожка 1
    TimelineClip* clip1 = getClipAt(time, 1);
    if (clip1 && !clip1->isVideoHidden) renderClip(frame1, clip1);

    //  Дорожка 2
    TimelineClip* clip2 = getClipAt(time, 2);
    if (clip2 && !clip2->isVideoHidden) renderClip(frame2, clip2);

    // Переходы применяются к каждому кадру ДО композитинга

    auto applyClipTransition = [&](QImage& fr, TimelineClip* clip)
    {
        if (!clip || fr.isNull()) return;
        double posInClip  = time - clip->startTime;
        double posFromEnd = clip->endTime() - time;
        double transDur = clip->effects.value("transition_duration", 0.5);
        int typeIn = (int)clip->effects.value("transition_in",  0.0);
        int typeOut = (int)clip->effects.value("transition_out", 0.0);
        if (typeIn > 0 && posInClip >= 0 && posInClip < transDur) {
            float prog = (float)(posInClip / transDur);
            QImage black(fr.size(), QImage::Format_RGB888); black.fill(Qt::black);
            fr = RenderEngine::applyTransition(black, fr, typeIn, prog);
        }
        if (typeOut > 0 && posFromEnd >= 0 && posFromEnd < transDur)
        {
            float prog = 1.0f - (float)(posFromEnd / transDur);
            QImage black(fr.size(), QImage::Format_RGB888); black.fill(Qt::black);
            fr = RenderEngine::applyTransition(fr, black, typeOut, prog);
        }
    };
    applyClipTransition(frame1, clip1);
    applyClipTransition(frame2, clip2);

    //  Композитинг
    if (!frame1.isNull() && !frame2.isNull())
    {
        if (frame1.format() == QImage::Format_ARGB32)
            return alphaComposite(frame1, frame2);
        return frame1.convertToFormat(QImage::Format_RGB888);
    }
    if (!frame1.isNull())
        return frame1.convertToFormat(QImage::Format_RGB888);
    if (!frame2.isNull())
        return frame2.convertToFormat(QImage::Format_RGB888);

    // Оба кадра null. Почему?
    // 1) Нет клипов в этот момент → чёрный кадр (правильно)
    // 2) Клипы есть, но cache miss → return null чтобы видео-таймер
    //    сохранил предыдущий кадр вместо чёрного мерцания.
    //    Без этого: cache miss → чёрный → аудио играет → мерцание + рассинхрон.
    bool hasActiveClip = (clip1 && !clip1->isVideoHidden)
                      || (clip2 && !clip2->isVideoHidden);
    if (hasActiveClip)
    {
        // Cache miss на активном клипе — сохраняем предыдущий кадр
        return {};
    }

    // Нет активных клипов — чёрный кадр
    if (m_clipMeta.isEmpty()) return {};
    auto& meta = m_clipMeta.constBegin().value();
    if (meta.width > 0 && meta.height > 0) {
        QImage black(meta.width, meta.height, QImage::Format_RGB888);
        black.fill(Qt::black);
        return black;
    }
    return {};
}


//  requestFrameForDisplay — основной публичный метод обновления превью

void Timeline::requestFrameForDisplay(double time, int selectedClipId,
                                      const QVariantMap& previewEffects)
{
    if (!m_imageProvider) return;

    QImage frame = getCompositeFrame(time, selectedClipId, previewEffects);
    if (frame.isNull()) {
        // Пустой таймлайн — чёрный кадр 1280×720
        frame = QImage(1280, 720, QImage::Format_RGB888);
        frame.fill(Qt::black);
    }
    m_imageProvider->setFrame(frame);
    emit frameReadyForDisplay();
}


//  Управление воспроизведением

//  Конвертация timeline-времени → source-время файла (с учётом trimStart)
//  DecoderThread работает в source-координатах, все внешние вызовы
//  передают timeline-время → нужна конвертация.

double Timeline::toSourceTime(const QString& filepath, int trackIndex, double timelineTime) const
{
    // Ищем клип с этим файлом НА КОНКРЕТНОЙ ДОРОЖКЕ, который содержит timelineTime
    for (const auto& clip : m_clips)
    {
        if (clip.filepath == filepath &&
            clip.trackIndex == trackIndex &&
            timelineTime >= clip.startTime &&
            timelineTime < clip.endTime())
        {
            return timelineTime - clip.startTime + clip.trimStart;
        }
    }
    // Если попали в зазор между клипами — берём ближайший клип НА ЭТОЙ ДОРОЖКЕ
    double best = 1e18;
    double result = timelineTime;
    for (const auto& clip : m_clips)
    {
        if (clip.filepath != filepath || clip.trackIndex != trackIndex) continue;
        double dist = qMin(qAbs(timelineTime - clip.startTime),
                           qAbs(timelineTime - clip.endTime()));
        if (dist < best)
        {
            best = dist;
            // Вычисляем source-время: зажимаем в границы клипа
            double clamped = qBound(clip.startTime, timelineTime, clip.endTime());
            result = clamped - clip.startTime + clip.trimStart;
        }
    }
    return result;
}

void Timeline::startPlayback(double fromTime, double speed)
{
    qDeleteAll(m_audioDecoders);
    m_audioDecoders.clear();

    m_playbackSpeed = speed;

    // Seek DecoderThreads — передаём SOURCE-время, не timeline-время!
    // DecoderThread хранит кадры по frameNum = sourceTime*fps.
    // Если передать timeline-время (0), а trimStart=47.5 — декодер читает
    // кадры с 0сек источника, а кэш ищет frameNum 47.5*fps=1187 → промах.
    //
    // КЛЮЧЕВОЙ ФИКС: НЕ очищаем кэш если нужный кадр уже в нём.
    // При pause → play кэш содержит кадры вокруг текущей позиции.
    // Старый код ВСЕГДА вызывал seekTo → cache->clear() → уничтожал все
    // 400 кадров → sync-decode давал 1 кадр → 0.5-2с cache miss'ов →
    // видео чёрное/замершее, аудио играет → рассинхрон.
    for (auto it = m_decoderThreads.begin(); it != m_decoderThreads.end(); ++it) {
        QString fp; int trk;
        parseDecoderKey(it.key(), fp, trk);
        double srcTime = toSourceTime(fp, trk, fromTime);
        int frameNum = (int)(srcTime * it.value()->getFps() + 0.5);
        FrameCache* cache = m_frameCaches.value(it.key(), nullptr);

        if (cache && cache->contains(frameNum))
        {
            // Кадр уже в кэше — НЕ очищаем! Только обновляем позицию.
            it.value()->updatePlayPosition(srcTime);
        }
        else
        {
            // Кадра нет — нужен полный seek (очистка кэша + перемотка декодера)
            it.value()->seekTo(srcTime);
            it.value()->updatePlayPosition(srcTime);
        }
    }

    //  Sync-decode первого кадра
    // seekTo очищает FrameCache. Устанавливаем m_forceNextFrame=true чтобы
    // getCurrentFrameAt разрешил sync-decode даже если audioEngine isPlaying().
    // Это нужно при смене скорости и seek во время воспроизведения — иначе
    // isPlaying()==true блокирует sync-decode → чёрный кадр до заполнения кэша.
    {
        m_forceNextFrame = true;
        QImage firstFrame = getCompositeFrame(fromTime);
        m_forceNextFrame = false;
        if (!firstFrame.isNull() && m_imageProvider) {
            m_imageProvider->setFrame(firstFrame);
            emit frameReadyForDisplay();
        }
    }

    // Создаём AudioPlaybackEngine один раз
    if (!m_audioEngine)
    {
        m_audioEngine = new AudioPlaybackEngine(this, this);

        connect(m_audioEngine, &AudioPlaybackEngine::timeUpdated,
                this, [this](double t)
                {
                    m_currentTime = t;
                    emit currentTimeChanged();
                    emit playbackTimeUpdated(t);
                    // Сообщаем DecoderThread текущую SOURCE-позицию для prefetch
                    for (auto it = m_decoderThreads.begin(); it != m_decoderThreads.end(); ++it) {
                        QString fp; int trk;
                        parseDecoderKey(it.key(), fp, trk);
                        it.value()->updatePlayPosition(toSourceTime(fp, trk, t));
                    }
                    // Видео обновляется отдельным m_videoTimer (не здесь)
                });

        connect(m_audioEngine, &AudioPlaybackEngine::playbackEnded,
                this, &Timeline::playbackEnded);
    }

    //  Отдельный видео-таймер ~30fps
    // singleShot: перезапускается в начале колбэка — до любых return,
    // чтобы случайный return не остановил воспроизведение навсегда.
    if (!m_videoTimer) {
        m_videoTimer = new QTimer(this);
        m_videoTimer->setSingleShot(true);
        m_videoTimer->setTimerType(Qt::PreciseTimer);
        connect(m_videoTimer, &QTimer::timeout, this, [this]() {
            // Перезапуск ПЕРВЫМ — до любых return
            if (m_audioEngine && m_audioEngine->isPlaying())
                m_videoTimer->start(33);

            if (!m_audioEngine || !m_audioEngine->isPlaying()) return;
            if (!m_imageProvider) return;

            // Живое время аудио-клока + компенсация QML latency
            double audioNow = m_audioEngine->getCurrentAudioTime();
            const double RENDER_LATENCY = 0.016;
            double renderTime = qMin(audioNow + RENDER_LATENCY * m_playbackSpeed,
                                     totalDuration());

            QImage frame = getCompositeFrame(renderTime);
            if (frame.isNull())
            {
                // ── Cache miss: sync decode с коротким cooldown ──────────
                // Раньше cooldown = 500мс → видео замирало на полсекунды
                // пока аудио играло → рассинхрон на дорожке 2.
                // 80мс = ~2.5 кадра при 30fps — заметный, но терпимый
                // стоп, и видео быстро догоняет аудио.
                qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (now - m_lastSyncDecodeMs > 80)
                {
                    m_lastSyncDecodeMs = now;
                    m_forceNextFrame = true;
                    frame = getCompositeFrame(renderTime);
                    m_forceNextFrame = false;
                }
                if (frame.isNull()) return;
            }

            m_imageProvider->setFrame(frame);
            emit frameReadyForDisplay();
        });
    }
    m_videoTimer->start(33);

    m_currentTime = fromTime;
    // Передаём РЕАЛЬНЫЙ конец последнего клипа, без +20с UI-падинга.
    // Старый totalDuration() добавлял +20с → pastEnd срабатывал через 20с
    // после конца клипов → воспроизведение тянулось в тишину.
    double actualEnd = 0.0;
    for (const auto& c : m_clips)
        if (c.endTime() > actualEnd) actualEnd = c.endTime();
    if (actualEnd < 0.1) actualEnd = totalDuration();  // fallback

    m_audioEngine->startPlayback(fromTime, speed, actualEnd);
}

void Timeline::stopPlayback()
{
    if (m_stopping) return;
    m_stopping = true;

    if (m_videoTimer) m_videoTimer->stop();

    if (m_audioEngine && m_audioEngine->isPlaying())
        m_currentTime = m_audioEngine->getCurrentAudioTime();

    if (m_audioEngine) m_audioEngine->stopPlayback();

    QMetaObject::invokeMethod(this, [this]()
                              {
                                  m_stopping = false;
                                  emit currentTimeChanged();
                                  requestFrameForDisplay(m_currentTime);
                              }, Qt::QueuedConnection);
}

void Timeline::setPlaybackVolume(double volume)
{
    if (m_audioEngine) m_audioEngine->setVolume((float)volume);
}

void Timeline::setTrackAudioMuted(int track, bool muted)
{
    if (m_audioEngine) m_audioEngine->setTrackMuted(track, muted);
}

void Timeline::setTrackVideoHidden(int track, bool hidden)
{
    for (auto& clip : m_clips) {
        if (clip.trackIndex == track)
            clip.isVideoHidden = hidden;
    }
    if (!m_audioEngine || !m_audioEngine->isPlaying())
        requestFrameForDisplay(m_currentTime);
}

double Timeline::getPlaybackTime() const
{
    if (m_audioEngine && m_audioEngine->isPlaying())
        return m_audioEngine->getCurrentAudioTime();
    return m_currentTime;
}

// ПОЛУЧИТЬ МЕТАДАННЫЕ КЛИПА ПО UID/ИНДЕКСУ
// Используется в Track.qml snap для получения реальной длины клипа
QVariantMap Timeline::getClipInfoById(int uidOrIndex)
{
    QVariantMap info;
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size()) return info;
    const TimelineClip& clip = m_clips[index];
    info["startTime"] = clip.startTime;
    info["duration"] = clip.duration;
    info["trimStart"] = clip.trimStart;
    info["trimEnd"] = clip.trimEnd;
    return info;
}

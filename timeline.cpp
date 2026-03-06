#include "timeline.h"

// Тяжёлые include только здесь
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
#include <QStandardPaths>

Timeline::Timeline(QObject *parent)
    : QObject(parent)
    , m_currentTime(0.0)
    , m_renderEngine(nullptr)
{
    qDebug() << "Timeline constructor";
}

Timeline::~Timeline() {
    cancelRender();

    if (m_audioEngine) { m_audioEngine->stopPlayback(); delete m_audioEngine; }
    for (auto* thread : m_decoderThreads) {
        thread->stop();
        delete thread;
    }
    for (auto* d : m_audioDecoders) { d->closeFile(); delete d; }
    qDeleteAll(m_frameCaches);

    qDebug() << "Timeline destructor";
}

// ===== ВСПОМОГАТЕЛЬНЫЙ: запустить поток декодирования =====
void Timeline::startDecoderThread(const QString& filepath, double fps) {
    if (m_decoderThreads.contains(filepath)) return;

    auto* cache  = new FrameCache();
    auto* thread = new DecoderThread(filepath, fps, cache, this);

    m_frameCaches[filepath]    = cache;
    m_decoderThreads[filepath] = thread;

    connect(thread, &DecoderThread::frameReady, this, [this](int /*frameNum*/) {
        emit frameReady(QImage(), m_currentTime);
    });

    thread->start();
    qDebug() << "DecoderThread started for:" << filepath << "FPS:" << fps;
}

// ===== ПОЛУЧИТЬ КЛИПЫ ДЛЯ ДОРОЖКИ (для QML) =====

// ─────────────────────────────────────────────────────────────────
// Найти индекс клипа по стабильному UID (хранится в effects["_uid"])
// Если UID не найден — возвращает -1
// ─────────────────────────────────────────────────────────────────
static int findClipIndex(const QList<TimelineClip>& clips, int uid) {
    for (int i = 0; i < clips.size(); ++i) {
        if (static_cast<int>(clips[i].effects.value("_uid", -1)) == uid)
            return i;
    }
    return -1;
}

// Вспомогательная: разрешить "uid-or-index" —
// если clip.effects["_uid"] существует → ищем по uid, иначе — по индексу (обратная совместимость)
static int resolveIndex(const QList<TimelineClip>& clips, int uidOrIndex) {
    // Сначала ищем как UID
    int byUid = findClipIndex(clips, uidOrIndex);
    if (byUid >= 0) return byUid;
    // Fallback: прямой индекс
    if (uidOrIndex >= 0 && uidOrIndex < clips.size()) return uidOrIndex;
    return -1;
}

QVariantList Timeline::getClipsForTrack(int trackIndex) {
    QVariantList result;

    for (int i = 0; i < m_clips.size(); ++i) {
        const TimelineClip& clip = m_clips[i];

        if (clip.trackIndex == trackIndex) {
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
            for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it) {
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
bool Timeline::addClip(const QString& filepath, int trackIndex, double startTime) {
    qDebug() << "   Timeline::addClip!";
    qDebug() << "   filepath:" << filepath;
    qDebug() << "   trackIndex:" << trackIndex;
    qDebug() << "   startTime:" << startTime;

    // 1. Проверить, существует ли файл
    if (!QFile::exists(filepath)) {
        qWarning() << "File not found:" << filepath;
        return false;
    }

    // 2. Используем MediaDecoder для получения метаданных
    MediaDecoder decoder;

    if (!decoder.openFile(filepath)) {
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

    qDebug() << "Video duration:" << sourceDuration << "sec";

    // 3. Создать новый клип
    TimelineClip newClip;
    newClip.filepath = filepath;
    newClip.trackIndex = trackIndex;
    newClip.startTime = startTime;
    newClip.duration = sourceDuration;
    newClip.trimStart = 0.0;
    newClip.trimEnd = 0.0;

    // 4. Проверить пересечения
    if (!canAddClip(trackIndex, startTime, sourceDuration)) {
        qWarning() << "Clip overlap on track" << trackIndex;
        return false;
    }

    // 5. Добавить клип в список
    // Назначаем стабильный UID (не меняется при sortClips)
    newClip.effects["_uid"] = static_cast<double>(++m_nextUid);
    m_clips.append(newClip);

    // Запустить поток декодирования
    if (!m_decoderThreads.contains(filepath)) {
        auto* cache = new FrameCache();
        auto* thread = new DecoderThread(filepath, fps, cache, this);

        m_frameCaches[filepath] = cache;
        m_decoderThreads[filepath] = thread;

        connect(thread, &DecoderThread::frameReady, this, [this](int /*frameNum*/) {
            emit frameReady(QImage(), m_currentTime);
        });

        thread->start();
        qDebug() << "DecoderThread started for:" << filepath;
    }

    sortClips();

    int newIndex = m_clips.size() - 1;

    qDebug() << "Clip added! Index:" << newIndex << "Total:" << m_clips.size();

    emit clipsChanged();
    emit clipAdded(newIndex);
    emit totalDurationChanged();

    return true;
}

// ===== УДАЛИТЬ КЛИП =====
bool Timeline::removeClip(int uidOrIndex) {
    int index = resolveIndex(m_clips, uidOrIndex);
    qDebug() << "removeClip: uid/idx=" << uidOrIndex << "→ index=" << index;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    m_clips.removeAt(index);

    emit clipsChanged();
    emit clipRemoved(index);
    emit totalDurationChanged();

    qDebug() << "Clip removed. Remaining:" << m_clips.size();
    return true;
}

// ===== ПЕРЕМЕСТИТЬ КЛИП =====
bool Timeline::moveClip(int uidOrIndex, int newTrackIndex, double newStartTime) {
    int index = resolveIndex(m_clips, uidOrIndex);
    qDebug() << "moveClip: uid/idx=" << uidOrIndex << "→ index=" << index
             << "-> track" << newTrackIndex << "time" << newStartTime;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    TimelineClip& clip = m_clips[index];

    if (!canAddClip(newTrackIndex, newStartTime, clip.duration, index)) {
        qWarning() << "Overlap on move";
        return false;
    }

    clip.trackIndex = newTrackIndex;
    clip.startTime = newStartTime;

    sortClips();

    emit clipsChanged();
    emit clipModified(index);

    qDebug() << "Clip moved";
    return true;
}

// ===== РАЗРЕЗАТЬ КЛИП =====
bool Timeline::splitClip(int uidOrIndex, double splitTime) {
    int index = resolveIndex(m_clips, uidOrIndex);
    qDebug() << "splitClip: uid/idx=" << uidOrIndex << "→ index=" << index << "at" << splitTime;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    if (splitTime <= m_clips[index].startTime || splitTime >= m_clips[index].endTime()) {
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
    // firstClip.effects["_uid"] уже содержит оригинальный UID — не трогаем.
    secondClip.effects["_uid"] = static_cast<double>(++m_nextUid);

    // Переходы: Part A сохраняет вход, Part B сохраняет выход
    // (разрез посередине не должен наследовать оба перехода)
    firstClip.effects.remove("transition_out");   // вход у первой части остаётся
    secondClip.effects.remove("transition_in");   // выход у второй части остаётся
    // duration оставляем у обоих (если нужно вернуть переходы вручную)

    m_clips[index] = firstClip;
    m_clips.append(secondClip);
    sortClips();

    qDebug() << "Clip split into 2 parts";
    qDebug() << "   Part A: start=" << firstClip.startTime
             << "dur=" << firstClip.duration
             << "trimS=" << firstClip.trimStart
             << "trimE=" << firstClip.trimEnd;
    qDebug() << "   Part B: start=" << secondClip.startTime
             << "dur=" << secondClip.duration
             << "trimS=" << secondClip.trimStart
             << "trimE=" << secondClip.trimEnd;

    emit clipsChanged();
    return true;
}

// ===== ОБРЕЗАТЬ КЛИП =====
bool Timeline::trimClip(int uidOrIndex, double newTrimStart, double newTrimEnd) {
    int index = resolveIndex(m_clips, uidOrIndex);
    qDebug() << "trimClip: uid/idx=" << uidOrIndex << "→ index=" << index
             << "trim" << newTrimStart << "-" << newTrimEnd;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    TimelineClip& clip = m_clips[index];

    clip.trimStart = newTrimStart;
    clip.trimEnd = newTrimEnd;

    MediaDecoder decoder;
    if (decoder.openFile(clip.filepath)) {
        double sourceDuration = decoder.getDuration();
        clip.duration = sourceDuration - newTrimStart - newTrimEnd;
        decoder.closeFile();
    }

    if (clip.duration <= 0) {
        qWarning() << "Trim too large";
        return false;
    }

    emit clipModified(index);
    emit totalDurationChanged();

    qDebug() << "Clip trimmed. New duration:" << clip.duration;
    return true;
}

// ===== ОБРЕЗКА ЛЕВОГО КРАЯ КЛИПА =====
bool Timeline::setClipLeftTrim(int uidOrIndex, double newStartTime, double newTrimStart)
{
    int index = resolveIndex(m_clips, uidOrIndex);
    qDebug() << "setClipLeftTrim: uid/idx=" << uidOrIndex << "→ index=" << index
             << "newStart=" << newStartTime
             << "newTrimStart=" << newTrimStart;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    if (newTrimStart < 0.0) newTrimStart = 0.0;

    double oldEndTimeline = m_clips[index].startTime + m_clips[index].duration;
    double newDuration    = oldEndTimeline - newStartTime;

    if (newDuration < 0.1) {
        qWarning() << "Clip too short:" << newDuration;
        return false;
    }

    m_clips[index].startTime = newStartTime;
    m_clips[index].trimStart = newTrimStart;
    m_clips[index].duration  = newDuration;

    emit clipsChanged();
    emit totalDurationChanged();

    qDebug() << "setClipLeftTrim: start=" << m_clips[index].startTime
             << "trimStart=" << m_clips[index].trimStart
             << "duration=" << m_clips[index].duration;
    return true;
}

// ===== ПРИМЕНИТЬ ЭФФЕКТ =====
bool Timeline::applyEffect(int uidOrIndex, const QString& effectName, double value) {
    int index = resolveIndex(m_clips, uidOrIndex);
    qDebug() << "applyEffect: uid/idx=" << uidOrIndex << "→ index=" << index << effectName << "=" << value;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    m_clips[index].effects[effectName] = value;
    emit clipModified(index);

    qDebug() << "Effect applied";
    return true;
}

bool Timeline::removeEffect(int uidOrIndex, const QString& effectName) {
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size()) return false;
    m_clips[index].effects.remove(effectName);
    emit clipModified(index);
    return true;
}

QVariantMap Timeline::getClipEffects(int uidOrIndex) const {
    QVariantMap result;
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size()) return result;
    const auto& effects = m_clips[index].effects;
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        result[it.key()] = it.value();
    }
    return result;
}

// ===== ПРОВЕРКА ПЕРЕСЕЧЕНИЙ =====
bool Timeline::canAddClip(int trackIndex, double startTime, double duration, int excludeIndex) const {
    double endTime = startTime + duration;

    for (int i = 0; i < m_clips.size(); ++i) {
        if (i == excludeIndex) continue;
        const TimelineClip& existing = m_clips[i];
        if (existing.trackIndex != trackIndex) continue;
        if (!(endTime <= existing.startTime || startTime >= existing.endTime())) {
            return false;
        }
    }
    return true;
}

// ===== СОРТИРОВКА =====
void Timeline::sortClips() {
    std::sort(m_clips.begin(), m_clips.end(), [](const TimelineClip& a, const TimelineClip& b) {
        if (a.trackIndex != b.trackIndex) return a.trackIndex < b.trackIndex;
        return a.startTime < b.startTime;
    });
}

// ===== ПОЛУЧИТЬ ОБЩУЮ ДЛИТЕЛЬНОСТЬ =====
double Timeline::totalDuration() const {
    if (m_clips.isEmpty()) return 100.0;

    double maxEnd = 0.0;
    for (const TimelineClip& clip : m_clips) {
        double end = clip.endTime();
        if (end > maxEnd) maxEnd = end;
    }
    return maxEnd + 20.0;
}

// ===== СЕТТЕР ВРЕМЕНИ =====
void Timeline::setCurrentTime(double time) {
    if (qAbs(m_currentTime - time) < 0.01) return;
    m_currentTime = time;
    // Во время воспроизведения НЕ делаем seekTo — это сбрасывает кэш и вызывает Cache miss
    bool playing = m_audioEngine && m_audioEngine->isPlaying();
    if (!playing) {
        for (auto* thread : m_decoderThreads) {
            thread->seekTo(time);
        }
    }
    emit currentTimeChanged();
}

// ===== ПОЛУЧИТЬ КЛИП ПО ИНДЕКСУ =====
TimelineClip* Timeline::getClip(int index) {
    if (index < 0 || index >= m_clips.size()) return nullptr;
    return &m_clips[index];
}

// ===== ПОЛУЧИТЬ КЛИП В ПОЗИЦИИ =====
TimelineClip* Timeline::getClipAt(double time, int trackIndex) {
    for (int i = 0; i < m_clips.size(); ++i) {
        TimelineClip& clip = m_clips[i];
        if (clip.trackIndex == trackIndex &&
            time >= clip.startTime &&
            time < clip.endTime()) {
            return &clip;
        }
    }
    return nullptr;
}

// ===== ПОЛУЧИТЬ КАДР ДЛЯ PREVIEW =====
QImage Timeline::getCurrentFrameAt(double time, int trackIndex) {
    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return QImage();

    double clipTime = time - clip->startTime + clip->trimStart;
    double fps = 25.0;

    if (m_decoderThreads.contains(clip->filepath)) {
        fps = m_decoderThreads[clip->filepath]->getFps();
    } else if (m_clipMeta.contains(clip->filepath) && m_clipMeta[clip->filepath].fps > 0) {
        fps = m_clipMeta[clip->filepath].fps;
    }

    int frameNum = (int)(clipTime * fps);

    // 1. Пробуем кэш
    if (m_frameCaches.contains(clip->filepath)) {
        QImage cached;
        if (m_frameCaches[clip->filepath]->getNearest(frameNum, cached)) {
            return cached;
        }
    }

    // 2. Промах кэша:
    // Во время воспроизведения — не делаем sync decode, он блокирует UI
    bool playingNow = m_audioEngine && m_audioEngine->isPlaying();
    if (playingNow) return QImage();

    // На паузе — sync decode
    qDebug() << "Cache miss for time=" << clipTime << "- sync decode";
    MediaDecoder decoder;
    if (!decoder.openFile(clip->filepath)) return QImage();
    QImage frame = decoder.getFrameAt(clipTime);
    decoder.closeFile();

    if (!frame.isNull() && m_frameCaches.contains(clip->filepath)) {
        m_frameCaches[clip->filepath]->put(frameNum, frame);
    }

    return frame;
}

void Timeline::requestFrame(double time, int trackIndex) {
    QImage frame = getCurrentFrameAt(time, trackIndex);
    emit frameReady(frame, time);
}

QString Timeline::getFramePathAt(double time, int trackIndex) {
    QImage frame = getCurrentFrameAt(time, trackIndex);
    if (frame.isNull()) return QString();

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);

    QString tempPath = tempDir + "/videoframe_" + QString::number(qRound(time * 100)) + ".png";

    if (frame.save(tempPath)) {
        return "file:///" + tempPath;
    }

    qWarning() << "Failed to save frame";
    return QString();
}

// ===== СОХРАНЕНИЕ ПРОЕКТА =====
bool Timeline::saveProject(const QString& filepath) {
    QString cleanPath = filepath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    }

    qDebug() << "saveProject:" << filepath;

    QJsonObject json = toJson();
    QJsonDocument doc(json);
    QFile file(filepath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open file for writing:" << filepath;
        return false;
    }

    file.write(doc.toJson());
    file.close();

    qDebug() << "Project saved:" << filepath;
    return true;
}

// ===== ЗАГРУЗКА ПРОЕКТА =====
bool Timeline::loadProject(const QString& filepath) {
    QString cleanPath = filepath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    }

    qDebug() << "loadProject:" << filepath;

    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for reading:" << filepath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Invalid JSON";
        return false;
    }

    bool success = fromJson(doc.object());

    if (success) {
        for (const TimelineClip& clip : m_clips) {
            if (!m_decoderThreads.contains(clip.filepath) && QFile::exists(clip.filepath)) {
                MediaDecoder dec;
                double fps = 25.0;
                if (dec.openFile(clip.filepath)) {
                    fps = dec.getFrameRate();

                    if (!m_clipMeta.contains(clip.filepath)) {
                        ClipMeta meta;
                        meta.width  = dec.getVideoWidth();
                        meta.height = dec.getVideoHeight();
                        meta.fps    = fps;
                        m_clipMeta[clip.filepath] = meta;
                    }

                    dec.closeFile();
                }

                auto* cache  = new FrameCache();
                auto* thread = new DecoderThread(clip.filepath, fps, cache, this);
                m_frameCaches[clip.filepath]    = cache;
                m_decoderThreads[clip.filepath] = thread;

                connect(thread, &DecoderThread::frameReady, this, [this](int) {
                    emit frameReady(QImage(), m_currentTime);
                });

                thread->start();
                qDebug() << "DecoderThread started for:" << clip.filepath;
            }
        }

        emit clipsChanged();
        emit totalDurationChanged();
        qDebug() << "Project loaded:" << cleanPath;
    }

    return success;
}

// ===== КОНВЕРТАЦИЯ В JSON =====
QJsonObject Timeline::toJson() const {
    QJsonObject json;
    json["version"] = "1.0";
    json["currentTime"] = m_currentTime;

    QJsonArray clipsArray;
    for (const TimelineClip& clip : m_clips) {
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
        for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it) {
            effectsObj[it.key()] = it.value();
        }
        clipObj["effects"] = effectsObj;

        clipsArray.append(clipObj);
    }
    json["clips"] = clipsArray;

    return json;
}

// ===== КОНВЕРТАЦИЯ ИЗ JSON =====
bool Timeline::fromJson(const QJsonObject& json) {
    if (!json.contains("clips")) return false;

    m_clips.clear();
    m_currentTime = json["currentTime"].toDouble();

    QJsonArray clipsArray = json["clips"].toArray();
    for (const QJsonValue& value : clipsArray) {
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
        for (auto it = effectsObj.begin(); it != effectsObj.end(); ++it) {
            clip.effects[it.key()] = it.value().toDouble();
        }

        // Восстанавливаем UID или назначаем новый
        if (!clip.effects.contains("_uid")) {
            clip.effects["_uid"] = static_cast<double>(++m_nextUid);
        } else {
            int existingUid = static_cast<int>(clip.effects.value("_uid"));
            if (existingUid > m_nextUid) m_nextUid = existingUid;
        }
        m_clips.append(clip);
    }

    return true;
}

// ============================================================
// МЕТАДАННЫЕ КЛИПА
// ============================================================
QVariantMap Timeline::getClipInfoAt(double time, int trackIndex) {
    QVariantMap info;
    info["width"]     = 0;
    info["height"]    = 0;
    info["fps"]       = 0.0;
    info["startTime"] = 0.0;
    info["trimStart"] = 0.0;
    info["duration"]  = 0.0;

    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return info;

    info["startTime"] = clip->startTime;
    info["trimStart"] = clip->trimStart;
    info["duration"]  = clip->duration;

    if (m_clipMeta.contains(clip->filepath)) {
        const ClipMeta& meta = m_clipMeta[clip->filepath];
        info["width"]  = meta.width;
        info["height"] = meta.height;
        info["fps"]    = meta.fps;
    } else {
        MediaDecoder decoder;
        if (decoder.openFile(clip->filepath)) {
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

// ===== ПУТЬ К ФАЙЛУ ДЛЯ QMEDIAPLAYER =====
QString Timeline::getActiveClipPath(double time, int trackIndex) {
    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return QString();
    return "file:///" + clip->filepath;
}

// ============================================================
// splitClipAt, setClipMuted, getTrackEndTime
// ============================================================

bool Timeline::splitClipAt(double time, int trackIndex) {
    for (int i = 0; i < m_clips.size(); ++i) {
        const TimelineClip& c = m_clips[i];
        if (c.trackIndex == trackIndex
            && time > c.startTime
            && time < c.endTime())
        {
            qDebug() << "splitClipAt: found clip" << i << "time=" << time;
            return splitClip(i, time);
        }
    }
    qWarning() << "splitClipAt: no clip at time=" << time << "track=" << trackIndex;
    return false;
}

bool Timeline::setClipMuted(int uidOrIndex, bool muted) {
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "setClipMuted: invalid index" << index;
        return false;
    }
    m_clips[index].isMuted = muted;
    emit clipModified(index);
    qDebug() << "Clip uid/idx=" << uidOrIndex << "→" << index << (muted ? "muted" : "unmuted");
    return true;
}

double Timeline::getTrackEndTime(int trackIndex) const {
    double maxEnd = 0.0;
    for (const TimelineClip& c : m_clips) {
        if (c.trackIndex == trackIndex) {
            double e = c.endTime();
            if (e > maxEnd) maxEnd = e;
        }
    }
    return maxEnd;
}

// ============================================================
// ВИДИМОСТЬ КЛИПОВ (для рендеринга)
// ============================================================

void Timeline::setClipVideoHidden(int uidOrIndex, bool hidden) {
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index >= 0 && index < m_clips.size()) {
        m_clips[index].isVideoHidden = hidden;
    }
}

void Timeline::setClipAudioHidden(int uidOrIndex, bool hidden) {
    int index = resolveIndex(m_clips, uidOrIndex);
    if (index >= 0 && index < m_clips.size()) {
        m_clips[index].isAudioHidden = hidden;
    }
}

void Timeline::syncClipStatesForRender(QVariantMap hiddenMap, QVariantMap mutedMap) {
    qDebug() << "syncClipStatesForRender:" << hiddenMap.size() << "hidden,"
             << mutedMap.size() << "muted";

    for (int i = 0; i < m_clips.size(); ++i) {
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

// ================================================================
//  РЕНДЕРИНГ — запуск в отдельном потоке через RenderEngine
// ================================================================

bool Timeline::renderToFile(const QString& outputPath, int width, int height, const QString& format) {
    qDebug() << "renderToFile:" << outputPath << width << "x" << height << "format:" << format;

    // Очистить file:/// prefix
    QString cleanPath = outputPath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
        // Windows: /C:/path -> C:/path
        if (cleanPath.length() > 2 && cleanPath[0] == '/' &&
            cleanPath[2] == ':') {
            cleanPath = cleanPath.mid(1);
        }
    }

    if (m_clips.isEmpty()) {
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

    // Определить FPS из первого клипа
    double fps = 30.0;
    if (!m_clips.isEmpty() && m_clipMeta.contains(m_clips[0].filepath)) {
        double cfps = m_clipMeta[m_clips[0].filepath].fps;
        if (cfps > 0) fps = cfps;
    }
    m_renderEngine->setFps(fps);

    // Подключить сигналы
    connect(m_renderEngine, &RenderEngine::progressChanged,
            this,           &Timeline::renderProgress);

    connect(m_renderEngine, &RenderEngine::renderFinished,
            this,           [this](bool success) {
                qDebug() << (success ? "Render finished OK" : "Render FAILED");
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

void Timeline::cancelRender() {
    if (m_renderEngine) {
        m_renderEngine->cancel();
        m_renderEngine->deleteLater();
        m_renderEngine = nullptr;
    }
}
// ═══════════════════════════════════════════════════════════════════════
//  setImageProvider — вызвать из main.cpp ПОСЛЕ регистрации провайдера
// ═══════════════════════════════════════════════════════════════════════
void Timeline::setImageProvider(EffectImageProvider* provider) {
    m_imageProvider = provider;
}

// ═══════════════════════════════════════════════════════════════════════
//  getOrCreateAudioDecoder — ленивое создание декодера аудио
// ═══════════════════════════════════════════════════════════════════════
MediaDecoder* Timeline::getOrCreateAudioDecoder(const QString& filepath) {
    if (m_audioDecoders.contains(filepath))
        return m_audioDecoders[filepath];

    auto* dec = new MediaDecoder();
    if (!dec->openFile(filepath)) {
        delete dec;
        return nullptr;
    }
    m_audioDecoders[filepath] = dec;
    return dec;
}

// ═══════════════════════════════════════════════════════════════════════
//  getMixedAudio — аудио-микс двух дорожек для live воспроизведения
// ═══════════════════════════════════════════════════════════════════════
QVector<float> Timeline::getMixedAudio(double time, double duration,
                                       bool t1muted, bool t2muted)
{
    const int SR = 44100, CH = 2;
    int totalFloats = static_cast<int>(duration * SR * CH);
    if (totalFloats <= 0) return {};

    QVector<float> mixed(totalFloats, 0.0f);
    bool hasAudio = false;

    auto mixTrack = [&](int trackIndex, bool muted) {
        if (muted) return;
        TimelineClip* clip = getClipAt(time, trackIndex);
        if (!clip || clip->isMuted || clip->isAudioHidden) return;

        // Клип есть на дорожке — двигатель должен продолжать работать
        // даже если декодер ещё не вернул данные (прогрев)
        hasAudio = true;

        MediaDecoder* dec = getOrCreateAudioDecoder(clip->filepath);
        if (!dec || !dec->hasAudio()) return;

        double srcTime = clip->sourceTimeAt(time);
        QVector<float> audio = dec->decodeAudioRange(srcTime, duration);
        if (audio.isEmpty()) return;

        double vol = clip->effects.value("volume", 1.0);
        int len = qMin(mixed.size(), audio.size());
        for (int i = 0; i < len; ++i)
            mixed[i] += audio[i] * (float)vol;
    };

    mixTrack(1, t1muted);
    mixTrack(2, t2muted);

    if (!hasAudio) return {};
    for (float& s : mixed) s = qBound(-1.0f, s, 1.0f);
    return mixed;
}

// ═══════════════════════════════════════════════════════════════════════
//  alphaComposite — Porter-Duff «src over» для хромакея
// ═══════════════════════════════════════════════════════════════════════
QImage Timeline::alphaComposite(const QImage& fg, const QImage& bg) {
    QImage fgA = fg.convertToFormat(QImage::Format_ARGB32);
    QImage bgA = bg.convertToFormat(QImage::Format_ARGB32);
    QImage result(qMin(fgA.width(),  bgA.width()),
                  qMin(fgA.height(), bgA.height()),
                  QImage::Format_RGB888);

    for (int y = 0; y < result.height(); ++y) {
        const QRgb* fgLine = reinterpret_cast<const QRgb*>(fgA.constScanLine(y));
        const QRgb* bgLine = reinterpret_cast<const QRgb*>(bgA.constScanLine(y));
        uchar*      dstLine = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            float a  = qAlpha(fgLine[x]) / 255.0f;
            float ia = 1.0f - a;
            dstLine[x*3]   = (uchar)(qRed(fgLine[x])   * a + qRed(bgLine[x])   * ia);
            dstLine[x*3+1] = (uchar)(qGreen(fgLine[x]) * a + qGreen(bgLine[x]) * ia);
            dstLine[x*3+2] = (uchar)(qBlue(fgLine[x])  * a + qBlue(bgLine[x])  * ia);
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  getCompositeFrame — декодируем оба трека + применяем эффекты
// ═══════════════════════════════════════════════════════════════════════
QImage Timeline::getCompositeFrame(double time,
                                   int selectedClipId,
                                   const QVariantMap& previewEffects)
{
    ++m_previewFrameIndex;

    auto getEffectsFor = [&](const TimelineClip* clip) -> QMap<QString, double> {
        QMap<QString, double> eff = clip->effects;

        // Если это выделенный клип И есть preview-эффекты — переопределяем
        if (selectedClipId >= 0 && !previewEffects.isEmpty()) {
            int uid = static_cast<int>(clip->effects.value("_uid", -1));
            if (uid == selectedClipId) {
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
    bool playingFast = m_audioEngine && m_audioEngine->isPlaying();

    auto renderClip = [&](QImage& out, int track) {
        TimelineClip* clip = getClipAt(time, track);
        if (!clip || clip->isVideoHidden) return;
        QImage raw = getCurrentFrameAt(time, track);
        if (raw.isNull()) return;
        auto eff = getEffectsFor(clip);
        bool hasEffects = false;
        for (auto it = eff.constBegin(); it != eff.constEnd(); ++it)
            if (it.key() != "_uid") { hasEffects = true; break; }
        if (playingFast && !hasEffects)
            out = raw.convertToFormat(QImage::Format_RGB888);
        else
            out = RenderEngine::applyEffectsToFrame(raw, eff, m_previewFrameIndex);
    };

    renderClip(frame1, 1);
    renderClip(frame2, 2);

    // ── Композитинг ───────────────────────────────────────────────────
    if (!frame1.isNull() && !frame2.isNull()) {
        // Хромакей на дорожке 1 → ARGB32 → альфа-наложение на дорожку 2
        if (frame1.format() == QImage::Format_ARGB32)
            return alphaComposite(frame1, frame2);
        return frame1.convertToFormat(QImage::Format_RGB888);
    }
    if (!frame1.isNull())
        return frame1.convertToFormat(QImage::Format_RGB888);
    if (!frame2.isNull())
        return frame2.convertToFormat(QImage::Format_RGB888);

    // Нет клипов — чёрный кадр
    if (m_clipMeta.isEmpty()) return {};
    auto& meta = m_clipMeta.constBegin().value();
    if (meta.width > 0 && meta.height > 0) {
        QImage black(meta.width, meta.height, QImage::Format_RGB888);
        black.fill(Qt::black);
        return black;
    }
    return {};
}

// ═══════════════════════════════════════════════════════════════════════
//  requestFrameForDisplay — основной публичный метод обновления превью
// ═══════════════════════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════════════════════
//  Управление воспроизведением
// ═══════════════════════════════════════════════════════════════════════
void Timeline::startPlayback(double fromTime, double speed) {
    qDeleteAll(m_audioDecoders);
    m_audioDecoders.clear();

    for (auto* thread : m_decoderThreads)
        thread->seekTo(fromTime);

    // Пересоздаём engine каждый раз — гарантирует правильный connect и сброс состояния
    if (m_audioEngine) {
        m_audioEngine->stopPlayback();
        delete m_audioEngine;
        m_audioEngine = nullptr;
    }
    m_audioEngine = new AudioPlaybackEngine(this, this);
    m_lastVideoTime = -1.0;

    connect(m_audioEngine, &AudioPlaybackEngine::timeUpdated,
            this, [this](double t) {
                m_currentTime = t;
                emit currentTimeChanged();
                emit playbackTimeUpdated(t);
                for (auto* thread : m_decoderThreads)
                    thread->updatePlayPosition(t);
                if (t - m_lastVideoTime >= 1.0 / 25.0) {
                    m_lastVideoTime = t;
                    requestFrameForDisplay(t);
                }
            });

    connect(m_audioEngine, &AudioPlaybackEngine::playbackEnded,
            this, &Timeline::playbackEnded);

    m_currentTime = fromTime;
    double dur = totalDuration() - 20.0;
    if (dur <= 0) dur = totalDuration();
    m_audioEngine->startPlayback(fromTime, speed, dur);

    requestFrameForDisplay(fromTime);
}

void Timeline::stopPlayback() {
    if (m_audioEngine) m_audioEngine->stopPlayback();
}

void Timeline::setPlaybackVolume(double volume) {
    if (m_audioEngine) m_audioEngine->setVolume((float)volume);
}

void Timeline::setTrackAudioMuted(int track, bool muted) {
    if (m_audioEngine) m_audioEngine->setTrackMuted(track, muted);
}

void Timeline::setTrackVideoHidden(int track, bool hidden) {
    for (auto& clip : m_clips) {
        if (clip.trackIndex == track)
            clip.isVideoHidden = hidden;
    }
    if (!m_audioEngine || !m_audioEngine->isPlaying())
        requestFrameForDisplay(m_currentTime);
}

double Timeline::getPlaybackTime() const {
    if (m_audioEngine && m_audioEngine->isPlaying())
        return m_audioEngine->getCurrentAudioTime();
    return m_currentTime;
}

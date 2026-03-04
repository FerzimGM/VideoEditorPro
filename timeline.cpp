#include "timeline.h"

// Тяжёлые include только здесь
#include "FrameCache.h"
#include "decoderthread.h"
#include "mediadecoder.h"
#include "renderengine.h"

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

    for (auto* thread : m_decoderThreads) {
        thread->stop();
        delete thread;
    }
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
QVariantList Timeline::getClipsForTrack(int trackIndex) {
    QVariantList result;

    for (int i = 0; i < m_clips.size(); ++i) {
        const TimelineClip& clip = m_clips[i];

        if (clip.trackIndex == trackIndex) {
            QVariantMap clipMap;

            clipMap["id"] = i;
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
bool Timeline::removeClip(int index) {
    qDebug() << "removeClip:" << index;

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
bool Timeline::moveClip(int index, int newTrackIndex, double newStartTime) {
    qDebug() << "moveClip:" << index << "-> track" << newTrackIndex << "time" << newStartTime;

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
bool Timeline::splitClip(int index, double splitTime) {
    qDebug() << "splitClip:" << index << "at" << splitTime;

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
bool Timeline::trimClip(int index, double newTrimStart, double newTrimEnd) {
    qDebug() << "trimClip:" << index << "trim" << newTrimStart << "-" << newTrimEnd;

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
bool Timeline::setClipLeftTrim(int index, double newStartTime, double newTrimStart)
{
    qDebug() << "setClipLeftTrim: index=" << index
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
bool Timeline::applyEffect(int index, const QString& effectName, double value) {
    qDebug() << "applyEffect:" << index << effectName << "=" << value;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "Invalid index:" << index;
        return false;
    }

    m_clips[index].effects[effectName] = value;
    emit clipModified(index);

    qDebug() << "Effect applied";
    return true;
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
    for (auto* thread : m_decoderThreads) {
        thread->seekTo(time);
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

    // 2. Промах кэша
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

bool Timeline::setClipMuted(int index, bool muted) {
    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "setClipMuted: invalid index" << index;
        return false;
    }
    m_clips[index].isMuted = muted;
    emit clipModified(index);
    qDebug() << "Clip" << index << (muted ? "muted" : "unmuted");
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

void Timeline::setClipVideoHidden(int index, bool hidden) {
    if (index >= 0 && index < m_clips.size()) {
        m_clips[index].isVideoHidden = hidden;
    }
}

void Timeline::setClipAudioHidden(int index, bool hidden) {
    if (index >= 0 && index < m_clips.size()) {
        m_clips[index].isAudioHidden = hidden;
    }
}

void Timeline::syncClipStatesForRender(QVariantMap hiddenMap, QVariantMap mutedMap) {
    qDebug() << "syncClipStatesForRender:" << hiddenMap.size() << "hidden,"
             << mutedMap.size() << "muted";

    for (int i = 0; i < m_clips.size(); ++i) {
        QString vKey = QString::number(i) + "_v";
        QString aKey = QString::number(i) + "_a";
        QString mKey = QString::number(i);

        m_clips[i].isVideoHidden = hiddenMap.value(vKey, false).toBool();
        m_clips[i].isAudioHidden = hiddenMap.value(aKey, false).toBool();

        if (mutedMap.contains(mKey)) {
            m_clips[i].isMuted = mutedMap.value(mKey, false).toBool();
        }
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

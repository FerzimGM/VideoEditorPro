#include "timeline.h"

// *** ВСЕ ТЯЖЁЛЫЕ INCLUDE ТОЛЬКО ЗДЕСЬ, не в timeline.h! ***
// Так каждый .cpp файл проекта не будет тянуть FFmpeg хедеры.
#include "FrameCache.h"
#include "decoderthread.h"
#include "mediadecoder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
//#include <algorithm>

Timeline::Timeline(QObject *parent)
    : QObject(parent)
    , m_currentTime(0.0)
{
    qDebug() << "✅ Timeline конструктор вызван";
}

Timeline::~Timeline() {
    // Останавливаем потоки декодирования
    for (auto* thread : m_decoderThreads) {
        thread->stop();
        delete thread;
    }
    qDeleteAll(m_frameCaches);

    qDebug() << "🔚 Timeline деструктор вызван";
}

// ===== ВСПОМОГАТЕЛЬНЫЙ: запустить поток декодирования =====
void Timeline::startDecoderThread(const QString& filepath, double fps) {
    if (m_decoderThreads.contains(filepath)) return;  // уже запущен

    auto* cache  = new FrameCache();
    auto* thread = new DecoderThread(filepath, fps, cache, this);

    m_frameCaches[filepath]    = cache;
    m_decoderThreads[filepath] = thread;

    connect(thread, &DecoderThread::frameReady, this, [this](int /*frameNum*/) {
        emit frameReady(QImage(), m_currentTime);
    });

    thread->start();
    qDebug() << "🎬 DecoderThread запущен для:" << filepath << "FPS:" << fps;
}

// ===== ПОЛУЧИТЬ КЛИПЫ ДЛЯ ДОРОЖКИ (для QML) =====
QVariantList Timeline::getClipsForTrack(int trackIndex) {
    QVariantList result;

    for (int i = 0; i < m_clips.size(); ++i) {
        const TimelineClip& clip = m_clips[i];

        if (clip.trackIndex == trackIndex) {
            QVariantMap clipMap;

            // ID для QML
            clipMap["id"] = i;

            // Основные данные
            clipMap["filepath"] = clip.filepath;
            clipMap["startTime"] = clip.startTime;
            clipMap["duration"] = clip.duration;

            // Имя файла (без пути)
            QFileInfo fileInfo(clip.filepath);
            clipMap["filename"] = fileInfo.fileName();

            // Trim
            clipMap["trimStart"] = clip.trimStart;
            clipMap["trimEnd"] = clip.trimEnd;

            // Аудио
            clipMap["audioOffset"] = clip.audioOffset;
            clipMap["isMuted"] = clip.isMuted;

            // Эффекты
            QVariantMap effectsMap;
            for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it) {
                effectsMap[it.key()] = it.value();
            }
            clipMap["effects"] = effectsMap;

            // Состояние
            clipMap["selected"] = false;  // TODO: хранить в Timeline
            clipMap["thumbnailPath"] = "";  // TODO: генерировать превью

            result.append(clipMap);
        }
    }

    return result;
}

// Добавить клип на timeline
bool Timeline::addClip(const QString& filepath, int trackIndex, double startTime) {
    qDebug() << "   Timeline::addClip вызвана!";
    qDebug() << "   filepath:" << filepath;
    qDebug() << "   trackIndex:" << trackIndex;
    qDebug() << "   startTime:" << startTime;


    // 1. Проверить, существует ли файл
    if (!QFile::exists(filepath)) {
        qWarning() << "Фаил не найден:" << filepath;
        return false;
    }

    // 2. ПРАВИЛЬНО: Используем MediaDecoder для получения метаданных!
    MediaDecoder decoder;

    if (!decoder.openFile(filepath)) {
        qWarning() << "❌ MediaDecoder не смог открыть файл";
        return false;
    }

    // 2. Получить длительность исходного видео через FFmpeg
    double sourceDuration = decoder.getDuration();
    double fps = decoder.getFrameRate();

    if (sourceDuration <= 0) {
        qWarning() << "Не возможно получить длительность видео:" << filepath;
        decoder.closeFile();
        return false;
    }

    // КЭШИРУЕМ метаданные — чтобы getClipInfoAt не открывал декодер снова!
    // Это устраняет спам "MediaDecoder создан/уничтожен" при каждом скруббинге.
    if (!m_clipMeta.contains(filepath)) {
        ClipMeta meta;
        meta.width  = decoder.getVideoWidth();
        meta.height = decoder.getVideoHeight();
        meta.fps    = fps;
        m_clipMeta[filepath] = meta;
    }

    // Закрыть декодер - метаданные получены
    decoder.closeFile();

    qDebug() << "✅ Длительность видео:" << sourceDuration << "секунд";

    // 3. Создать новый клип
    TimelineClip newClip;
    newClip.filepath = filepath;
    newClip.trackIndex = trackIndex;
    newClip.startTime = startTime;
    newClip.duration = sourceDuration;  // По умолчанию — вся длина видео
    newClip.trimStart = 0.0;
    newClip.trimEnd = 0.0;

    // 4. Проверить пересечения с существующими клипами
    if (!canAddClip(trackIndex, startTime, sourceDuration)) {
        qWarning() << "Внимание! Пересечение клипа!" << trackIndex;
        // Можно добавить несмотря на пересечение
        return false;
    }

    // 5. Добавить клип в список
    m_clips.append(newClip);

    // Запустить поток декодирования для нового файла (если ещё не запущен)
    if (!m_decoderThreads.contains(filepath)) {
        auto* cache = new FrameCache();
        auto* thread = new DecoderThread(filepath, fps, cache, this);

        m_frameCaches[filepath] = cache;
        m_decoderThreads[filepath] = thread;

        // Когда кадр готов — говорим Timeline обновить превью
        connect(thread, &DecoderThread::frameReady, this, [this](int /*frameNum*/) {
            emit frameReady(QImage(), m_currentTime);  // сигнал VideoPlayer-у
        });

        thread->start();
        qDebug() << "🎬 DecoderThread запущен для:" << filepath;
    }

    sortClips();  // Отсортировать по startTime для удобства

    // 6. Уведомить UI
    int newIndex = m_clips.size() - 1;

    qDebug() << "✅ Клип добавлен! Индекс:" << newIndex << "Всего клипов:" << m_clips.size();

    emit clipsChanged();
    emit clipAdded(newIndex);
    emit totalDurationChanged();

    return true;
}

// ===== УДАЛИТЬ КЛИП =====
bool Timeline::removeClip(int index) {
    qDebug() << "️ removeClip:" << index;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << " Неверный индекс:" << index;
        return false;
    }

    m_clips.removeAt(index);

    emit clipsChanged();
    emit clipRemoved(index);
    emit totalDurationChanged();

    qDebug() << " Клип удалён. Осталось:" << m_clips.size();
    return true;
}

// ===== ПЕРЕМЕСТИТЬ КЛИП =====
bool Timeline::moveClip(int index, int newTrackIndex, double newStartTime) {
    qDebug() << "🔀 moveClip:" << index << "→ track" << newTrackIndex << "time" << newStartTime;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "❌ Неверный индекс:" << index;
        return false;
    }

    TimelineClip& clip = m_clips[index];

    // Проверить пересечения (исключая сам клип)
    if (!canAddClip(newTrackIndex, newStartTime, clip.duration, index)) {
        qWarning() << "⚠️ Пересечение при перемещении";
        return false;
    }

    clip.trackIndex = newTrackIndex;
    clip.startTime = newStartTime;

    sortClips();

    emit clipsChanged();
    emit clipModified(index);

    qDebug() << "✅ Клип перемещён";
    return true;
}

// ===== РАЗРЕЗАТЬ КЛИП =====
bool Timeline::splitClip(int index, double splitTime) {
    qDebug() << "✂️ splitClip:" << index << "at" << splitTime;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "❌ Неверный индекс:" << index;
        return false;
    }

    // *** КРИТИЧНО: НЕ используем ссылку TimelineClip& originalClip = m_clips[index] ***
    // m_clips.append() может перевыделить память QList → ссылка становится висячей
    // (dangling reference) → UB: startTime = -1.45682e+144 в логах.
    // Решение: работаем только через индекс или делаем полные копии ДО append().

    // Проверить что splitTime внутри клипа
    if (splitTime <= m_clips[index].startTime || splitTime >= m_clips[index].endTime()) {
        qWarning() << "❌ splitTime вне границ клипа";
        return false;
    }

    // Снимаем полные копии ДО любых модификаций списка
    TimelineClip firstClip  = m_clips[index];  // копия оригинала
    TimelineClip secondClip = m_clips[index];  // копия для второй части

    // Сколько секунд от начала клипа до точки разреза (в единицах таймлайна)
    double cutOffset = splitTime - firstClip.startTime;

    // ── Первая часть: [startTime, splitTime) ─────────────────────────────
    // duration сокращается до cutOffset, trimEnd увеличивается на остаток.
    // trimStart не трогаем — начало клипа не изменилось.
    firstClip.duration = cutOffset;
    firstClip.trimEnd  = secondClip.trimEnd + (secondClip.duration - cutOffset);

    // ── Вторая часть: [splitTime, endTime) ───────────────────────────────
    // startTime сдвигается на splitTime.
    // trimStart += cutOffset (пропускаем уже показанные секунды исходника).
    // trimEnd остаётся — правый край не менялся.
    secondClip.startTime = splitTime;
    secondClip.duration  = secondClip.duration - cutOffset;
    secondClip.trimStart = secondClip.trimStart + cutOffset;

    // Обновляем оригинальный элемент через индекс (не через ссылку!)
    m_clips[index] = firstClip;

    // Теперь append() — список может перевыделиться, но firstClip уже скопирован
    m_clips.append(secondClip);
    sortClips();

    // *** КРИТИЧНО: qDebug() ДО emit clipsChanged() ***
    // Если напечатать ПОСЛЕ emit — QML синхронно обрабатывает сигнал,
    // пересоздаёт делегаты Repeater, Component.onCompleted вызывается пока
    // firstClip ещё на стеке C++. MSVC в Debug заполняет освобождённую память
    // 0xCC → при интерпретации как double получается -1.45682e+144.
    qDebug() << "✅ Клип разрезан на 2 части";
    qDebug() << "   Часть A: startTime=" << firstClip.startTime
             << "duration=" << firstClip.duration
             << "trimStart=" << firstClip.trimStart
             << "trimEnd=" << firstClip.trimEnd;
    qDebug() << "   Часть B: startTime=" << secondClip.startTime
             << "duration=" << secondClip.duration
             << "trimStart=" << secondClip.trimStart
             << "trimEnd=" << secondClip.trimEnd;

    emit clipsChanged();
    return true;
}

//===== ОБРЕЗАТЬ КЛИП =====
bool Timeline::trimClip(int index, double newTrimStart, double newTrimEnd) {
    qDebug() << "✂️ trimClip:" << index << "trim" << newTrimStart << "-" << newTrimEnd;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "❌ Неверный индекс:" << index;
        return false;
    }

    TimelineClip& clip = m_clips[index];

    clip.trimStart = newTrimStart;
    clip.trimEnd = newTrimEnd;

    // Получить исходную длительность через MediaDecoder
    MediaDecoder decoder;
    if (decoder.openFile(clip.filepath)) {
        double sourceDuration = decoder.getDuration();
        clip.duration = sourceDuration - newTrimStart - newTrimEnd;
        decoder.closeFile();
    }

    if (clip.duration <= 0) {
        qWarning() << "❌ Обрезка слишком большая";
        return false;
    }

    emit clipModified(index);
    emit totalDurationChanged();

    qDebug() << "✅ Клип обрезан. Новая длительность:" << clip.duration;
    return true;
}

// ===== ОБРЕЗКА ЛЕВОГО КРАЯ КЛИПА =====
// Вызывается когда пользователь тянет левый resize-хэндл клипа.
// Атомарно: startTime + trimStart + duration — правый край не двигается.
//
// *** ВАЖНО: НЕ используем TimelineClip& ref после append() — висячая ссылка! ***
bool Timeline::setClipLeftTrim(int index, double newStartTime, double newTrimStart)
{
    qDebug() << "✂️ setClipLeftTrim: index=" << index
             << "newStart=" << newStartTime
             << "newTrimStart=" << newTrimStart;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "❌ Неверный индекс:" << index;
        return false;
    }

    if (newTrimStart < 0.0) newTrimStart = 0.0;

    // Правый край таймлайна неподвижен = startTime + duration
    double oldEndTimeline = m_clips[index].startTime + m_clips[index].duration;
    double newDuration    = oldEndTimeline - newStartTime;

    if (newDuration < 0.1) {
        qWarning() << "❌ Клип стал слишком коротким:" << newDuration;
        return false;
    }

    m_clips[index].startTime = newStartTime;
    m_clips[index].trimStart = newTrimStart;
    m_clips[index].duration  = newDuration;

    emit clipsChanged();
    emit totalDurationChanged();

    qDebug() << "✅ setClipLeftTrim: startTime=" << m_clips[index].startTime
             << "trimStart=" << m_clips[index].trimStart
             << "duration=" << m_clips[index].duration;
    return true;
}

// ===== ПРИМЕНИТЬ ЭФФЕКТ =====
bool Timeline::applyEffect(int index, const QString& effectName, double value) {
    qDebug() << "✨ applyEffect:" << index << effectName << "=" << value;

    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "❌ Неверный индекс:" << index;
        return false;
    }

    m_clips[index].effects[effectName] = value;

    emit clipModified(index);

    qDebug() << "✅ Эффект применён";
    return true;
}

// ===== ПРОВЕРКА ПЕРЕСЕЧЕНИЙ =====
bool Timeline::canAddClip(int trackIndex, double startTime, double duration, int excludeIndex) const {
    double endTime = startTime + duration;

    for (int i = 0; i < m_clips.size(); ++i) {
        // Пропускаем сам клип
        if (i == excludeIndex) continue;

        const TimelineClip& existing = m_clips[i];

        // Проверяем только клипы на той же дорожке
        if (existing.trackIndex != trackIndex) continue;

        // Проверка пересечения
        if (!(endTime <= existing.startTime || startTime >= existing.endTime())) {
            return false;  // Пересечение!
        }
    }

    return true;  // Нет пересечений
}

// ===== СОРТИРОВКА =====
void Timeline::sortClips() {
    std::sort(m_clips.begin(), m_clips.end(), [](const TimelineClip& a, const TimelineClip& b) {
        if (a.trackIndex != b.trackIndex) {
            return a.trackIndex < b.trackIndex;
        }
        return a.startTime < b.startTime;
    });
}

// ===== ПОЛУЧИТЬ ОБЩУЮ ДЛИТЕЛЬНОСТЬ =====
double Timeline::totalDuration() const {
    if (m_clips.isEmpty()) {
        return 100.0;  // Минимум 100 секунд
    }

    double maxEnd = 0.0;
    for (const TimelineClip& clip : m_clips) {
        double end = clip.endTime();
        if (end > maxEnd) {
            maxEnd = end;
        }
    }

    return maxEnd + 20.0;  // +20 секунд буфер
}

// ===== СЕТТЕР ВРЕМЕНИ =====
void Timeline::setCurrentTime(double time) {
    if (qAbs(m_currentTime - time) < 0.01) {
        return;  // Не изменилось
    }
    m_currentTime = time;
    // Уведомляем все потоки декодирования
    for (auto* thread : m_decoderThreads) {
        thread->seekTo(time);
    }

    emit currentTimeChanged();
}

// ===== ПОЛУЧИТЬ КЛИП ПО ИНДЕКСУ =====
TimelineClip* Timeline::getClip(int index) {
    if (index < 0 || index >= m_clips.size()) {
        return nullptr;
    }

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
    // Найти активный клип на этом времени
    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) {
        return QImage();  // Нет клипа
    }

    double clipTime = time - clip->startTime + clip->trimStart;
    double fps = 25.0;  // fallback

    // FIX: берём реальный fps из DecoderThread (у него есть геттер getFps())
    // Раньше здесь стоял TODO-комментарий, fps всегда был 25.0 → неправильный frameNum
    // → постоянные cache miss для видео с нестандартным fps (например 23.976)
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
            // Здесь можно применить эффекты перед возвратом:
            // return applyEffects(cached, clip->effects);
            return cached;
        }
    }

    // 2. Промах кэша — декодируем синхронно (медленно, но надёжно)
    qDebug() << "⚠️ Cache miss для time=" << clipTime << "— синхронное декодирование";
    MediaDecoder decoder;
    if (!decoder.openFile(clip->filepath)) return QImage();
    QImage frame = decoder.getFrameAt(clipTime);
    decoder.closeFile();

    // TODO: Применить эффекты через RenderEngine
    // RenderEngine engine;
    // frame = engine.applyEffects(frame, clip->effects);
    // Сохраняем в кэш чтобы следующий раз был быстрее
    if (!frame.isNull() && m_frameCaches.contains(clip->filepath)) {
        m_frameCaches[clip->filepath]->put(frameNum, frame);
    }

    return frame;
}

void Timeline::requestFrame(double time, int trackIndex) {
    QImage frame = getCurrentFrameAt(time, trackIndex);
    emit frameReady(frame, time);
}

// *** КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ ***
// QML не умеет работать с QImage напрямую.
// Сохраняем кадр во временный файл и возвращаем путь.
// QML использует этот путь как source для Image { }
QString Timeline::getFramePathAt(double time, int trackIndex) {
    QImage frame = getCurrentFrameAt(time, trackIndex);
    if (frame.isNull()) {
        return QString();  // Нет клипа — возвращаем пустую строку
    }

    // Папка для временных файлов
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);

    // Имя файла включает время — чтобы Image перезагрузился при смене времени
    // Округляем до 2 знаков чтобы не создавать тысячи файлов
    QString tempPath = tempDir + "/videoframe_" + QString::number(qRound(time * 100)) + ".png";

    if (frame.save(tempPath)) {
        qDebug() << "✅ Кадр сохранён:" << tempPath;
        return "file:///" + tempPath;
    }

    qWarning() << "❌ Не удалось сохранить кадр";
    return QString();
}

// ===== СОХРАНЕНИЕ ПРОЕКТА =====
bool Timeline::saveProject(const QString& filepath) {

    /* Убираем prefix file:/// если передан из QML*/
    QString cleanPath = filepath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    }

    qDebug() << "💾 saveProject:" << filepath;

    QJsonObject json = toJson();

    QJsonDocument doc(json);
    QFile file(filepath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "❌ Не могу открыть файл для записи:" << filepath;
        return false;
    }

    file.write(doc.toJson());
    file.close();

    qDebug() << "✅ Проект сохранён:" << filepath;
    return true;
}

// ===== ЗАГРУЗКА ПРОЕКТА =====
bool Timeline::loadProject(const QString& filepath) {

    QString cleanPath = filepath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    }

    qDebug() << "📂 loadProject:" << filepath;

    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "❌ Не могу открыть файл для чтения:" << filepath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "❌ Неверный JSON";
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
                qDebug() << "🎬 DecoderThread запущен для:" << clip.filepath;
            }
        }

        emit clipsChanged();
        emit totalDurationChanged();
        qDebug() << "✅ Проект загружен:" << cleanPath;
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

        // Эффекты
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
    if (!json.contains("clips")) {
        return false;
    }

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

        // Эффекты
        QJsonObject effectsObj = clipObj["effects"].toObject();
        for (auto it = effectsObj.begin(); it != effectsObj.end(); ++it) {
            clip.effects[it.key()] = it.value().toDouble();
        }

        m_clips.append(clip);
    }

    return true;
}

// ===== РЕНДЕРИНГ =====
bool Timeline::renderToFile(const QString& outputPath) {
    qDebug() << "🎬 renderToFile:" << outputPath;
    qDebug() << "⚠️ Делегируем в RenderEngine!";

    // TODO: Создать RenderEngine и запустить
    // RenderEngine engine;
    // engine.setClips(m_clips);
    // return engine.render(outputPath);

    // Пока заглушка:
    for (int i = 0; i <= 100; i += 10) {
        emit renderProgress(i);
    }

    emit renderFinished(true);

    qDebug() << "✅ Рендеринг завершён (DEMO)";
    return true;
}

// ============================================================
// Эти два метода объявлены в timeline.h через Q_INVOKABLE,
// но отсутствуют в .cpp → LNK2019.
// ============================================================

// ===== МЕТАДАННЫЕ КЛИПА (разрешение + FPS) =====
// Возвращает QVariantMap { "width", "height", "fps", "startTime", "trimStart" }
// VideoPlayer использует это для отображения разрешения/FPS
// и для синхронизации QMediaPlayer при старте воспроизведения.
QVariantMap Timeline::getClipInfoAt(double time, int trackIndex) {
    QVariantMap info;
    info["width"]     = 0;
    info["height"]    = 0;
    info["fps"]       = 0.0;
    info["startTime"] = 0.0;
    info["trimStart"] = 0.0;
    info["duration"]  = 0.0;  // ← QML findNextOnTrack использует это для эффективного шага

    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return info;

    info["startTime"] = clip->startTime;
    info["trimStart"] = clip->trimStart;
    info["duration"]  = clip->duration;  // ← реальная длина клипа на таймлайне (с учётом trim)

    // FIX: Используем кэш метаданных вместо открытия нового MediaDecoder!
    // Кэш заполняется в addClip() — один раз при добавлении клипа.
    // Раньше здесь открывался новый MediaDecoder при КАЖДОМ скруббинге → спам.
    if (m_clipMeta.contains(clip->filepath)) {
        const ClipMeta& meta = m_clipMeta[clip->filepath];
        info["width"]  = meta.width;
        info["height"] = meta.height;
        info["fps"]    = meta.fps;
    } else {
        // Фоллбэк: открываем декодер только если кэша нет (например, после loadProject)
        MediaDecoder decoder;
        if (decoder.openFile(clip->filepath)) {
            ClipMeta meta;
            meta.width  = decoder.getVideoWidth();
            meta.height = decoder.getVideoHeight();
            meta.fps    = decoder.getFrameRate();
            m_clipMeta[clip->filepath] = meta;  // кэшируем на будущее

            info["width"]  = meta.width;
            info["height"] = meta.height;
            info["fps"]    = meta.fps;
            decoder.closeFile();
        }
    }

    return info;
}

// ===== ПУТЬ К ФАЙЛУ ДЛЯ QMEDIAPLAYER =====
// QMediaPlayer принимает URI вида "file:///C:/path/video.mp4"
// Возвращает пустую строку если в данной позиции нет клипа.
QString Timeline::getActiveClipPath(double time, int trackIndex) {
    TimelineClip* clip = getClipAt(time, trackIndex);
    if (!clip) return QString();

    // На Windows нужно три слеша: file:///C:/...
    // На Linux два: file:///home/...
    // Qt это обрабатывает автоматически через QUrl::fromLocalFile,
    // но мы возвращаем строку напрямую для простоты
    return "file:///" + clip->filepath;
}

// ============================================================
// Три новых метода: splitClipAt, setClipMuted, getTrackEndTime
// ============================================================

// ===== РАЗРЕЗАТЬ ПО АБСОЛЮТНОМУ ВРЕМЕНИ ТАЙМЛАЙНА =====
// QML вызывает: cppTimeline.splitClipAt(playbackManager.currentTime)
// Удобнее чем splitClip(index, time) — не нужно знать индекс
bool Timeline::splitClipAt(double time, int trackIndex) {
    for (int i = 0; i < m_clips.size(); ++i) {
        const TimelineClip& c = m_clips[i];
        if (c.trackIndex == trackIndex
            && time > c.startTime
            && time < c.endTime())
        {
            qDebug() << "✂️ splitClipAt: нашёл клип" << i << "time=" << time;
            return splitClip(i, time);
        }
    }
    qWarning() << "⚠️ splitClipAt: нет клипа в time=" << time << "track=" << trackIndex;
    return false;
}

// ===== ВКЛЮЧИТЬ / ВЫКЛЮЧИТЬ ЗВУК =====
// Track.qml вызывает: cppTimeline.setClipMuted(id, muted)
bool Timeline::setClipMuted(int index, bool muted) {
    if (index < 0 || index >= m_clips.size()) {
        qWarning() << "❌ setClipMuted: неверный индекс" << index;
        return false;
    }
    m_clips[index].isMuted = muted;
    emit clipModified(index);
    emit clipsChanged();  // ← QML перечитает getClipsForTrack → isMuted обновится в VideoClip
    qDebug() << (muted ? "🔇" : "🔊") << "Клип" << index << (muted ? "заглушён" : "включён");
    return true;
}

// ===== ПОЛУЧИТЬ ВРЕМЯ КОНЦА ПОСЛЕДНЕГО КЛИПА НА ДОРОЖКЕ =====
// QML использует для добавления нового видео "в конец":
//   var endTime = cppTimeline.getTrackEndTime(1)
//   cppTimeline.addClip(filepath, 1, endTime)
double Timeline::getTrackEndTime(int trackIndex) const {
    double maxEnd = 0.0;
    for (const TimelineClip& c : m_clips) {
        if (c.trackIndex == trackIndex) {
            double e = c.endTime();
            if (e > maxEnd) maxEnd = e;
        }
    }
    return maxEnd;  // 0.0 если дорожка пуста
}

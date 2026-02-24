#ifndef TIMELINE_H
#define TIMELINE_H

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonObject>
#include <QImage>
#include <QVariantList>
#include <QVariantMap>
#include "timelineclip.h"
#include <QMap>

// *** ВАЖНО: НЕ включаем FrameCache.h и DecoderThread.h здесь!
// Это тяжёлые хедеры (QHash+QImage+FFmpeg). Если включить их в .h,
// каждый .cpp файл проекта будет компилировать их снова и снова →
// MSVC C1060 "not enough heap space".
//
// Решение: forward declaration здесь, полные include ТОЛЬКО в timeline.cpp

struct FrameCache;      // forward declaration
class DecoderThread;    // forward declaration

class Timeline : public QObject
{
    Q_OBJECT
    // ===== PROPERTIES для QML =====
    Q_PROPERTY(double currentTime READ currentTime WRITE setCurrentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY totalDurationChanged)
    Q_PROPERTY(int clipCount READ clipCount NOTIFY clipsChanged)

public:
    explicit Timeline(QObject *parent = nullptr);
    ~Timeline();

    // ===== ГЕТТЕРЫ =====
    double currentTime() const { return m_currentTime; }
    double totalDuration() const;  // Вычисляется как max(clip.endTime())
    int clipCount() const { return m_clips.size(); }

    // Получить список всех клипов (для рендеринга)
    const QList<TimelineClip>& clips() const { return m_clips; }

    // Получить клип по индексу
    TimelineClip* getClip(int index);

    // Найти клип на определённой позиции timeline и дорожке
    TimelineClip* getClipAt(double time, int trackIndex);

    // ===== СЕТТЕРЫ =====
    void setCurrentTime(double time);

    // ===== УПРАВЛЕНИЕ КЛИПАМИ =====

    // Получить кадр для указанного времени (для внутреннего использования)
    Q_INVOKABLE QImage getCurrentFrameAt(double time, int trackIndex = 1);

    // *** КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: возвращает путь к файлу кадра для QML ***
    // QML не умеет работать с QImage напрямую — нужен путь к файлу
    Q_INVOKABLE QString getFramePathAt(double time, int trackIndex = 1);

    // Добавить клип (возвращает true, если успешно)
    Q_INVOKABLE bool addClip(const QString& filepath, int trackIndex, double startTime);

    // Удалить клип по индексу
    Q_INVOKABLE bool removeClip(int index);

    // Переместить клип на новую позицию/дорожку
    Q_INVOKABLE bool moveClip(int index, int newTrackIndex, double newStartTime);

    // Разрезать клип в указанной позиции (создаёт два новых клипа)
    Q_INVOKABLE bool splitClip(int index, double splitTime);

    // Обрезать клип (изменить trimStart/trimEnd)
    Q_INVOKABLE bool trimClip(int index, double newTrimStart, double newTrimEnd);

    // Применить эффект к клипу
    Q_INVOKABLE bool applyEffect(int index, const QString& effectName, double value);

    // И метод для запроса кадра:
    Q_INVOKABLE void requestFrame(double time, int trackIndex = 1);

    // ===== ПОЛУЧЕНИЕ КЛИПОВ ДЛЯ QML =====

    // Получить все клипы для указанной дорожки (для отображения в QML)
    Q_INVOKABLE QVariantList getClipsForTrack(int trackIndex);

    // ===== ПРОВЕРКА ПЕРЕСЕЧЕНИЙ =====

    // Можно ли добавить клип без пересечений?
    bool canAddClip(int trackIndex, double startTime, double duration, int excludeIndex = -1) const;

    // ===== JSON СОХРАНЕНИЕ/ЗАГРУЗКА =====

    Q_INVOKABLE bool saveProject(const QString& filepath);
    Q_INVOKABLE bool loadProject(const QString& filepath);

    // Конвертация в/из JSON
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // ===== РЕНДЕРИНГ =====

    // Рендерить весь timeline в видеофайл
    Q_INVOKABLE bool renderToFile(const QString& outputPath);

signals:
    // Сигналы для обновления UI в QML
    void currentTimeChanged();
    void totalDurationChanged();
    void clipsChanged();
    void clipAdded(int index);
    void clipRemoved(int index);
    void clipModified(int index);
    void renderProgress(int percent);  // 0-100
    void renderFinished(bool success);
    void frameReady(const QImage &frame, double time);

private:
    // Кэш и поток декодирования — по одному на каждый уникальный filepath
    QMap<QString, FrameCache*>    m_frameCaches;
    QMap<QString, DecoderThread*> m_decoderThreads;

    QList<TimelineClip> m_clips;
    double m_currentTime;

    // Вспомогательные методы
    void sortClips();  // Сортировать клипы по startTime
    double getClipSourceDuration(const QString& filepath);  // Узнать длину видео через FFmpeg
};

#endif // TIMELINE_H

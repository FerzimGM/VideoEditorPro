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

// Forward declarations (тяжёлые хедеры только в .cpp)
struct FrameCache;
class DecoderThread;
class RenderEngine;

class Timeline : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double currentTime READ currentTime WRITE setCurrentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY totalDurationChanged)
    Q_PROPERTY(int clipCount READ clipCount NOTIFY clipsChanged)

public:
    explicit Timeline(QObject *parent = nullptr);
    ~Timeline();

    // ===== ГЕТТЕРЫ =====
    double currentTime() const { return m_currentTime; }
    double totalDuration() const;
    int clipCount() const { return m_clips.size(); }
    const QList<TimelineClip>& clips() const { return m_clips; }
    TimelineClip* getClip(int index);
    TimelineClip* getClipAt(double time, int trackIndex);

    // ===== СЕТТЕРЫ =====
    void setCurrentTime(double time);

    // ===== КАДРЫ ДЛЯ PREVIEW =====
    Q_INVOKABLE QImage getCurrentFrameAt(double time, int trackIndex = 1);
    Q_INVOKABLE QString getFramePathAt(double time, int trackIndex = 1);
    Q_INVOKABLE void requestFrame(double time, int trackIndex = 1);

    // ===== МЕТАДАННЫЕ =====
    Q_INVOKABLE QVariantMap getClipInfoAt(double time, int trackIndex = 1);
    Q_INVOKABLE QString getActiveClipPath(double time, int trackIndex = 1);

    // ===== УПРАВЛЕНИЕ КЛИПАМИ =====
    Q_INVOKABLE bool addClip(const QString& filepath, int trackIndex, double startTime);
    Q_INVOKABLE bool removeClip(int index);
    Q_INVOKABLE bool moveClip(int index, int newTrackIndex, double newStartTime);
    Q_INVOKABLE bool splitClipAt(double time, int trackIndex = 1);
    Q_INVOKABLE bool splitClip(int index, double splitTime);
    Q_INVOKABLE bool trimClip(int index, double newTrimStart, double newTrimEnd);
    Q_INVOKABLE bool setClipLeftTrim(int index, double newStartTime, double newTrimStart);
    Q_INVOKABLE bool applyEffect(int index, const QString& effectName, double value);
    Q_INVOKABLE bool setClipMuted(int index, bool muted);
    Q_INVOKABLE double getTrackEndTime(int trackIndex) const;
    Q_INVOKABLE QVariantList getClipsForTrack(int trackIndex);

    // ===== ВИДИМОСТЬ КЛИПОВ (синхронизация из QML для рендера) =====
    // QML вызывает перед рендером чтобы передать состояние из clipStates
    Q_INVOKABLE void setClipVideoHidden(int index, bool hidden);
    Q_INVOKABLE void setClipAudioHidden(int index, bool hidden);

    // ===== СИНХРОНИЗАЦИЯ ВСЕХ СОСТОЯНИЙ ПЕРЕД РЕНДЕРОМ =====
    // QML передаёт объект { "clipId_v": true, "clipId_a": false, ... }
    // и карту muted { clipId: true/false }
    Q_INVOKABLE void syncClipStatesForRender(QVariantMap hiddenMap, QVariantMap mutedMap);

    // ===== ПРОВЕРКА ПЕРЕСЕЧЕНИЙ =====
    bool canAddClip(int trackIndex, double startTime, double duration, int excludeIndex = -1) const;

    // ===== СОХРАНЕНИЕ / ЗАГРУЗКА =====
    Q_INVOKABLE bool saveProject(const QString& filepath);
    Q_INVOKABLE bool loadProject(const QString& filepath);
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // ===== РЕНДЕРИНГ =====
    // outputPath — путь к файлу
    // width, height — разрешение (из exportDialog)
    Q_INVOKABLE bool renderToFile(const QString& outputPath,
                                  int width = 1920, int height = 1080);

    // Отменить текущий рендеринг
    Q_INVOKABLE void cancelRender();

signals:
    void currentTimeChanged();
    void totalDurationChanged();
    void clipsChanged();
    void clipAdded(int index);
    void clipRemoved(int index);
    void clipModified(int index);
    void renderProgress(int percent);
    void renderFinished(bool success);
    void frameReady(const QImage &frame, double time);

private:
    QMap<QString, FrameCache*>    m_frameCaches;
    QMap<QString, DecoderThread*> m_decoderThreads;

    struct ClipMeta {
        int    width  = 0;
        int    height = 0;
        double fps    = 0.0;
    };
    QMap<QString, ClipMeta> m_clipMeta;

    QList<TimelineClip> m_clips;
    double m_currentTime;

    // Рендер-движок (живёт пока идёт рендеринг)
    RenderEngine* m_renderEngine;

    void sortClips();
    double getClipSourceDuration(const QString& filepath);
    void startDecoderThread(const QString& filepath, double fps);
};

#endif // TIMELINE_H

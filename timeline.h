#ifndef TIMELINE_H
#define TIMELINE_H

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonObject>
#include <QImage>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QHash>
#include <vector>
#include "timelineclip.h"
#include <QMap>

// Forward declarations
struct FrameCache;
class DecoderThread;
class RenderEngine;
class EffectImageProvider;
class AudioPlaybackEngine;
class MediaDecoder;

class Timeline : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double currentTime   READ currentTime   WRITE setCurrentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY totalDurationChanged)
    Q_PROPERTY(int    clipCount     READ clipCount     NOTIFY clipsChanged)

public:
    explicit Timeline(QObject *parent = nullptr);
    ~Timeline();

    double currentTime()  const { return m_currentTime; }
    double totalDuration() const;
    int clipCount() const { return m_clips.size(); }
    const QList<TimelineClip>& clips() const { return m_clips; }
    TimelineClip* getClip(int index);
    TimelineClip* getClipAt(double time, int trackIndex);

    void setCurrentTime(double time);

    // ── Image Provider ────────────────────────────────────────────────────
    void setImageProvider(EffectImageProvider* provider);

    // ── Live preview ──────────────────────────────────────────────────────
    Q_INVOKABLE void requestFrameForDisplay(double time,
                                            int selectedClipId = -1,
                                            const QVariantMap& previewEffects = {});

    // ── Совместимость ─────────────────────────────────────────────────────
    Q_INVOKABLE QImage  getCurrentFrameAt(double time, int trackIndex = 1);

    Q_INVOKABLE QVariantMap getClipInfoAt(double time, int trackIndex = 1);
    Q_INVOKABLE QVariantMap getClipInfoById(int uidOrIndex);

    // ── Воспроизведение ───────────────────────────────────────────────────
    Q_INVOKABLE void startPlayback(double fromTime, double speed = 1.0);
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void setPlaybackVolume(double volume);
    Q_INVOKABLE void setTrackAudioMuted(int track, bool muted);
    Q_INVOKABLE void setTrackVideoHidden(int track, bool hidden);
    Q_INVOKABLE double getPlaybackTime() const;

    // ── Аудио-микс для AudioPlaybackEngine ───────────────────────────────
    QVector<float> getMixedAudio(double time, double duration,
                                 bool t1muted = false, bool t2muted = false);

    // ── Клипы ─────────────────────────────────────────────────────────────
    Q_INVOKABLE bool addClip(const QString& filepath, int trackIndex, double startTime);
    Q_INVOKABLE bool removeClip(int index);
    Q_INVOKABLE bool moveClip(int index, int newTrackIndex, double newStartTime);
    Q_INVOKABLE bool splitClipAt(double time, int trackIndex = 1);
    Q_INVOKABLE bool splitClip(int index, double splitTime);
    Q_INVOKABLE bool trimClip(int index, double newTrimStart, double newTrimEnd);
    Q_INVOKABLE bool setClipLeftTrim(int index, double newStartTime, double newTrimStart);
    Q_INVOKABLE bool applyEffect(int index, const QString& effectName, double value);
    Q_INVOKABLE bool removeEffect(int index, const QString& effectName);
    Q_INVOKABLE QVariantMap getClipEffects(int index) const;
    Q_INVOKABLE bool setClipMuted(int index, bool muted);
    Q_INVOKABLE double getTrackEndTime(int trackIndex) const;
    Q_INVOKABLE QVariantList getClipsForTrack(int trackIndex);

    Q_INVOKABLE void setClipVideoHidden(int index, bool hidden);
    Q_INVOKABLE void setClipAudioHidden(int index, bool hidden);
    Q_INVOKABLE void syncClipStatesForRender(QVariantMap hiddenMap, QVariantMap mutedMap);

    bool canAddClip(int trackIndex, double startTime, double duration, int excludeIndex = -1) const;

    Q_INVOKABLE bool saveProject(const QString& filepath);
    Q_INVOKABLE bool loadProject(const QString& filepath);
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    Q_INVOKABLE bool renderToFile(const QString& outputPath,
                                  int width = 1920, int height = 1080,
                                  const QString& format = "MP4");
    Q_INVOKABLE void cancelRender();
    Q_INVOKABLE void playSystemBeep();

signals:
    void currentTimeChanged();
    void totalDurationChanged();
    void clipsChanged();
    void clipAdded(int index);
    void clipRemoved(int index);
    void clipModified(int index);
    void renderProgress(int percent);
    void renderFinished(bool success);
    void frameReadyForDisplay();
    void playbackTimeUpdated(double time);
    void playbackEnded();
    void frameReady(const QImage &frame, double time);

private:
    QMap<QString, FrameCache*> m_frameCaches;
    QMap<QString, DecoderThread*> m_decoderThreads;
    QMap<QString, MediaDecoder*> m_audioDecoders;

    struct ClipMeta {
        int width  = 0;
        int height = 0;
        double fps = 0.0;
    };
    QMap<QString, ClipMeta> m_clipMeta;

    QList<TimelineClip> m_clips;
    double m_currentTime = 0.0;
    bool m_frameProcessing = false;  // защита от накопления кадров
    int m_nextUid = 0;
    int m_previewFrameIndex = 0;

    RenderEngine* m_renderEngine  = nullptr;
    EffectImageProvider* m_imageProvider = nullptr;
    AudioPlaybackEngine* m_audioEngine = nullptr;
    QTimer* m_videoTimer = nullptr;
    double m_playbackSpeed = 1.0;
    bool m_stopping = false;
    bool m_forceNextFrame = false;
    // Персистентные кольцевые буферы для аудиоэффектов (reverb/echo) в live-режиме
    QHash<QString, std::vector<float>> m_audioDelayBufs;
    QHash<QString, int> m_audioDelayPos;
    qint64 m_lastSyncDecodeMs = 0; // разрешить sync-decode при следующем cache miss

    void sortClips();
    double getClipSourceDuration(const QString& filepath);
    double toSourceTime(const QString& filepath, double timelineTime) const;
    // clipKey = "filepath|startTime|trimStart" — уникален для каждого разрезанного клипа
    MediaDecoder* getOrCreateAudioDecoder(const QString& clipKey, const QString& filepath);

    QImage getCompositeFrame(double time,
                             int selectedClipId = -1,
                             const QVariantMap& previewEffects = {});
    QImage alphaComposite(const QImage& fg, const QImage& bg);
};

#endif // TIMELINE_H




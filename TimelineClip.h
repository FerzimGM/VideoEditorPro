#ifndef TIMELINECLIP_H
#define TIMELINECLIP_H

#include <QString>
#include <QMap>

struct TimelineClip {
    // ===== ОСНОВНЫЕ ДАННЫЕ =====
    QString filepath;        // Путь к исходному видеофайлу
    int trackIndex;          // Номер дорожки (1 = верхняя/основная, 2 = нижняя/фоновая)
    double startTime;        // Начало клипа на timeline (в секундах)
    double duration;         // Длительность клипа на timeline (в секундах)

    // ===== ОБРЕЗКА (TRIMMING) =====
    double trimStart;        // Сколько секунд обрезать с начала исходного видео (default: 0.0)
    double trimEnd;          // Сколько секунд обрезать с конца исходного видео (default: 0.0)

    // ===== ЭФФЕКТЫ =====
    QMap<QString, double> effects;

    // ===== АУДИО =====
    double audioOffset;      // Смещение аудио относительно видео
    bool isMuted;            // Выключен ли звук?

    // ===== ВИДИМОСТЬ (для рендеринга) =====
    // Синхронизируются из QML clipStates перед рендером
    bool isVideoHidden;      // Скрыто ли видео этого клипа?
    bool isAudioHidden;      // Скрыто ли аудио этого клипа?

    // ===== КОНСТРУКТОР =====
    TimelineClip()
        : trackIndex(0)
        , startTime(0.0)
        , duration(0.0)
        , trimStart(0.0)
        , trimEnd(0.0)
        , audioOffset(0.0)
        , isMuted(false)
        , isVideoHidden(false)
        , isAudioHidden(false)
    {}

    // ===== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ =====
    double endTime() const {
        return startTime + duration;
    }

    bool overlaps(const TimelineClip& other) const {
        if (trackIndex != other.trackIndex) return false;
        return !(endTime() <= other.startTime || startTime >= other.endTime());
    }

    double getSourceDuration() const {
        return duration;
    }

    // Время в исходном файле для данного момента на таймлайне
    double sourceTimeAt(double timelineTime) const {
        return timelineTime - startTime + trimStart;
    }

    // Активен ли клип в данный момент на таймлайне?
    bool isActiveAt(double time) const {
        return time >= startTime && time < endTime();
    }
};

#endif // TIMELINECLIP_H


#ifndef TIMELINECLIP_H
#define TIMELINECLIP_H

#include <QString>
#include <QMap>

struct TimelineClip {
    // ===== ОСНОВНЫЕ ДАННЫЕ =====
    QString filepath;        // Путь к исходному видеофайлу (абсолютный или относительный)
    int trackIndex;          // Номер дорожки (0 = верхняя, 1 = следующая, etc.)
    double startTime;        // Начало клипа на timeline (в секундах)
    double duration;         // Длительность клипа на timeline (в секундах)

    // ===== ОБРЕЗКА (TRIMMING) =====
    // Важно! Это НЕ влияет на duration — это внутреннее смещение в исходном файле
    double trimStart;        // Сколько секунд обрезать с начала исходного видео (default: 0.0)
    double trimEnd;          // Сколько секунд обрезать с конца исходного видео (default: 0.0)

    // ===== ЭФФЕКТЫ =====
    // Ключ = название эффекта ("brightness", "contrast", etc.)
    // Значение = параметр эффекта (например, brightness: 1.5 означает +50%)
    QMap<QString, double> effects;

    // ===== АУДИО (опционально) =====
    double audioOffset;      // Смещение аудио относительно видео (в секундах, может быть отрицательным)
    bool isMuted;            // Выключен ли звук на этом клипе?

    // ===== КОНСТРУКТОР ПО УМОЛЧАНИЮ =====
    TimelineClip()
        : trackIndex(0)
        , startTime(0.0)
        , duration(0.0)
        , trimStart(0.0)
        , trimEnd(0.0)
        , audioOffset(0.0)
        , isMuted(false)
    {}

    // ===== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ =====

    // Конечное время клипа на timeline (startTime + duration)
    double endTime() const {
        return startTime + duration;
    }

    // Проверка: пересекается ли этот клип с другим на той же дорожке?
    bool overlaps(const TimelineClip& other) const {
        // Клипы на разных дорожках не пересекаются
        if (trackIndex != other.trackIndex) {
            return false;
        }

        // Проверка пересечения временных интервалов
        // [startTime, endTime) vs [other.startTime, other.endTime)
        return !(endTime() <= other.startTime || startTime >= other.endTime());
    }

    // Реальная длительность исходного видео (с учётом обрезки)
    // Это нужно при декодировании: мы читаем только [trimStart, sourceDuration - trimEnd]
    double getSourceDuration() const {
        // duration уже учитывает обрезку, поэтому просто возвращаем его
        return duration;
    }
};

#endif // TIMELINECLIP_H

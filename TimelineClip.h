#ifndef TIMELINECLIP_H
#define TIMELINECLIP_H

#include <QString>
#include <QMap>

// A single clip placed on the timeline: a reference to a source file plus
// its position, trim points, effects, and visibility/audio state.
struct TimelineClip {
    // ===== Core data =====
    QString filepath;        // Path to the source media file
    int trackIndex;          // Track number (1 = top/primary, 2 = bottom/background)
    double startTime;        // Start position on the timeline, in seconds
    double duration;         // Duration on the timeline, in seconds

    // ===== Trimming =====
    double trimStart;        // Seconds trimmed from the start of the source (default 0.0)
    double trimEnd;          // Seconds trimmed from the end of the source (default 0.0)

    // ===== Effects =====
    QMap<QString, double> effects;

    // ===== Audio =====
    double audioOffset;      // Audio offset relative to video
    bool isMuted;            // Whether audio is muted for this clip

    // ===== Visibility (used for rendering) =====
    // Synced from the QML clip-state maps before a render pass.
    bool isVideoHidden;      // Whether this clip's video is hidden
    bool isAudioHidden;      // Whether this clip's audio is hidden

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

    // ===== Helpers =====
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

    // Maps a timeline moment to the corresponding time in the source file.
    double sourceTimeAt(double timelineTime) const {
        return timelineTime - startTime + trimStart;
    }

    // Whether the clip is active (visible on the timeline) at a given time.
    bool isActiveAt(double time) const {
        return time >= startTime && time < endTime();
    }
};

#endif // TIMELINECLIP_H

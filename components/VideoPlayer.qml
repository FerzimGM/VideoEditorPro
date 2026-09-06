// VideoPlayer.qml — Live-превью через FFmpeg + EffectImageProvider
/**
 * VideoPlayer
 * -----------
 * The video preview area. This component doesn't decode or render frames
 * itself — it only asks C++ (cppTimeline) to prepare the next frame given
 * the current position, selected clip and active effects, then displays
 * the result through a custom image provider (image://effects/frame_N).
 *
 * Two independent frame-update paths:
 *   1) Playback (isPlaying=true) — C++ drives the frames on its own and
 *      fires frameReadyForDisplay() for every decoded frame; the QML side
 *      doesn't run any timers of its own.
 *   2) Paused/scrubbing — any change to currentTime, effects, or the
 *      selected clip schedules a single frame request through debounce
 *      timers (scrubTimer/effectTimer), so rapid slider dragging doesn't
 *      flood C++ with a burst of requests.
 *
 * The Image's source changes via incrementing _frameId rather than a
 * direct path reference — this guarantees Qt re-reads the image from the
 * provider even when the URL would otherwise look "the same" as a string.
 */
import QtQuick
import QtQuick.Controls

Rectangle {
    id: videoPlayer
    color: "#0a0a0a"

    // Внешний интерфейс
    property bool isPlaying: false
    property double currentTime: 0.0
    property double duration: 0.0
    property real playbackSpeed: 1.0
    property double volume: 1.0

    // Скрытие видео/аудио
    property bool hideVideo: false
    property bool hideTrack1Video: false
    property bool hideTrack2Video: false
    property bool hideAudio1: false
    property bool hideAudio2: false

    // Метаданные видео
    property int videoWidth: 0
    property int videoHeight: 0
    property real videoFps: 30.0

    // Эффекты для live-превью
    property int selectedClipId: -1

    property real effectBrightness: 0.0
    property real effectContrast: 1.0
    property real effectSaturation: 1.0
    property bool effectGrayscale: false
    property real effectBlur: 0.0
    property real effectSharpness: 0.0
    property real effectHue: 0.0
    property real effectSepia: 0.0
    property real effectVignette: 0.0
    property bool effectInvert: false
    property real effectPosterize: 0.0
    property real effectPixelate: 0.0
    property real effectTemperature: 0.0
    property real effectTintHue: 0.0
    property real effectTintStr: 0.0
    property real effectGrain: 0.0
    property real effectAutoEnhance: 0.0
    property bool effectChromaKey: false
    property real effectChromaThreshold: 0.35
    property real effectChromaSmoothness: 0.1

    // Сигналы
    signal playbackStopped
    signal timePositionChanged(real time)

    /**
     * Seek the player to position time.
     * During playback, fully restarts C++ playback from the new position
     * (volume/mute need to be reapplied before starting). While paused,
     * it just defers a frame request via scrubTimer, so a burst of rapid
     * seek() calls (e.g. while dragging the playhead) doesn't overload
     * the C++ decoder with redundant requests.
     */
    function seek(time) {
        var t = Math.max(0, time)
        if (isPlaying) {
            cppTimeline.setPlaybackVolume(volume)
            cppTimeline.setTrackAudioMuted(1, hideAudio1)
            cppTimeline.setTrackAudioMuted(2, hideAudio2)
            cppTimeline.startPlayback(t, playbackSpeed)
        } else {
            scrubTimer.restart()
        }
        timePositionChanged(t)
    }

    // Frame counter — incremented on every new frame; used as part of the
    // image provider's URL to force a re-read of the image (Image won't
    // update if source stays the same string)
    property int _frameId: 0

    // Video display: the frame comes not from a file but from C++ through
    // a custom "effects" image provider (see Effectimageprovider.h in core)
    Image {
        id: videoDisplay
        anchors.fill: parent
        cache: false
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: false
        visible: !videoPlayer.hideVideo
        source: "image://effects/frame_" + videoPlayer._frameId
    }

    // Placeholder, shown while there isn't a single frame yet (_frameId
    // === 0, i.e. no clips on the timeline yet) or when video is explicitly hidden
    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: videoPlayer._frameId === 0 || videoPlayer.hideVideo

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "▶"
            font.pixelSize: 48
            color: "#333"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Добавьте видео на таймлайн"
            font.pixelSize: 14
            color: "#555"
        }
    }

    // Connections to C++: react to decoder/playback events
    Connections {
        target: cppTimeline

        // A newly decoded frame is ready — just bump the counter, the
        // Image will pick up the new source and re-request the picture
        function onFrameReadyForDisplay() {
            videoPlayer._frameId++
        }

        // Do NOT write videoPlayer.currentTime = time — it would break the binding in main.qml
        function onPlaybackTimeUpdated(time) {
            videoPlayer.timePositionChanged(time)
        }

        function onPlaybackEnded() {
            // Signal only — main.qml resets isPlaying and time itself
            videoPlayer.playbackStopped()
        }

        // If the set of clips on the timeline changed while paused —
        // the preview needs to be refreshed (e.g. the clip under the playhead changed)
        function onClipsChanged() {
            if (!videoPlayer.isPlaying)
                Qt.callLater(videoPlayer.requestPreview)
        }
    }

    //  Play / Pause: on start, sync volume/mute before starting playback
    // (C++ doesn't persist them between playback sessions); on stop,
    // request one final frame (freeze-frame)
    onIsPlayingChanged: {
        if (isPlaying) {
            cppTimeline.setPlaybackVolume(volume)
            cppTimeline.setTrackAudioMuted(1, hideAudio1)
            cppTimeline.setTrackAudioMuted(2, hideAudio2)
            cppTimeline.startPlayback(currentTime, playbackSpeed)
        } else {
            cppTimeline.stopPlayback()
            Qt.callLater(requestPreview) // показать стоп-кадр
        }
    }

    // Changing speed on the fly: C++ doesn't support "reconfigure speed
    // without stopping", so we have to read the exact current position,
    // stop, and restart playback at the new speed — otherwise audio and
    // video could drift out of sync
    onPlaybackSpeedChanged: {
        if (isPlaying) {
            var exactTime = cppTimeline.getPlaybackTime()
            cppTimeline.stopPlayback()
            cppTimeline.startPlayback(exactTime, videoPlayer.playbackSpeed)
        }
    }

    // Volume and track mute states are pushed to C++ immediately on
    // change — regardless of whether playback is running
    onVolumeChanged: if (cppTimeline)
                         cppTimeline.setPlaybackVolume(volume)
    onHideAudio1Changed: if (cppTimeline)
                             cppTimeline.setTrackAudioMuted(1, hideAudio1)
    onHideAudio2Changed: if (cppTimeline)
                             cppTimeline.setTrackAudioMuted(2, hideAudio2)

    // Hiding a video track: pushed to C++, and while paused also forces
    // a preview refresh (via effectTimer), since the visible picture
    // should change immediately rather than waiting for the next playback frame
    onHideTrack1VideoChanged: {
        if (cppTimeline)
            cppTimeline.setTrackVideoHidden(1, hideTrack1Video)
        if (!isPlaying)
            effectTimer.restart()
    }
    onHideTrack2VideoChanged: {
        if (cppTimeline)
            cppTimeline.setTrackVideoHidden(2, hideTrack2Video)
        if (!isPlaying)
            effectTimer.restart()
    }

    // Scrubbing: moving the playhead while paused shouldn't trigger a
    // decode on every micro-change of currentTime — the timer batches a
    // burst of rapid changes into a single final frame request after 50ms of idle
    onCurrentTimeChanged: {
        if (!isPlaying)
            scrubTimer.restart()
    }

    Timer {
        id: scrubTimer
        interval: 50
        repeat: false
        onTriggered: if (!videoPlayer.isPlaying)
                         videoPlayer.requestPreview()
    }

    // requestPreview — the single entry point for requesting a frame from
    // the C++ core. Called during scrubbing, after the effects debounce,
    // and after the selected clip changes — anywhere a static frame needs a refresh
    function requestPreview() {
        if (!cppTimeline)
            return
        cppTimeline.requestFrameForDisplay(currentTime, selectedClipId,
                                           buildEffectsMap())
    }

    // Builds a map of active effects to pass to C++.
    // Each effect is only included in the map if its value noticeably
    // deviates from the neutral one (thresholds chosen empirically) —
    // this saves the C++ renderer work: it never processes effects absent
    // from the map, instead of checking each one for "neutrality" itself
    function buildEffectsMap() {
        var m = {}
        if (Math.abs(effectBrightness) > 0.001)
            m["brightness"] = effectBrightness
        if (Math.abs(effectContrast - 1.0) > 0.001)
            m["contrast"] = effectContrast
        if (Math.abs(effectSaturation - 1.0) > 0.001)
            m["saturation"] = effectSaturation
        if (effectGrayscale)
            m["grayscale"] = 1.0
        if (effectBlur > 0.1)
            m["blur"] = effectBlur
        if (effectSharpness > 0.1)
            m["sharpness"] = effectSharpness
        if (Math.abs(effectHue) > 0.1)
            m["hue"] = effectHue
        if (effectSepia > 0.01)
            m["sepia"] = effectSepia
        if (effectVignette > 0.01)
            m["vignette"] = effectVignette
        if (effectInvert)
            m["invert"] = 1.0
        if (effectPosterize > 0.5)
            m["posterize"] = effectPosterize
        if (effectPixelate > 1.0)
            m["pixelate"] = effectPixelate
        if (Math.abs(effectTemperature) > 0.01)
            m["temperature"] = effectTemperature
        if (effectTintStr > 0.01)
            m["tint_hue"] = effectTintHue
        if (effectTintStr > 0.01)
            m["tint_strength"] = effectTintStr
        if (effectGrain > 0.01)
            m["grain"] = effectGrain
        if (effectAutoEnhance > 0.0)
            m["auto_enhance"] = effectAutoEnhance
        if (effectChromaKey) {
            m["chroma_key"] = 1.0
            m["chroma_threshold"] = effectChromaThreshold
            m["chroma_smoothness"] = effectChromaSmoothness
        }
        return m
    }

    // Effect sliders (80ms debounce): most effects produce a continuous
    // stream of changes while dragging a slider, so the frame request is
    // deferred through effectTimer to avoid decoding on every micro-change.
    // Binary effects (grayscale, invert, chromaKey) toggle instantly with
    // no debounce — they're plain checkboxes that don't "jitter" on interaction
    onEffectBrightnessChanged: if (!isPlaying)
                                   effectTimer.restart()
    onEffectContrastChanged: if (!isPlaying)
                                 effectTimer.restart()
    onEffectSaturationChanged: if (!isPlaying)
                                   effectTimer.restart()
    onEffectGrayscaleChanged: if (!isPlaying)
                                  requestPreview()
    onEffectBlurChanged: if (!isPlaying)
                             effectTimer.restart()
    onEffectSharpnessChanged: if (!isPlaying)
                                  effectTimer.restart()
    onEffectHueChanged: if (!isPlaying)
                            effectTimer.restart()
    onEffectSepiaChanged: if (!isPlaying)
                              effectTimer.restart()
    onEffectVignetteChanged: if (!isPlaying)
                                 effectTimer.restart()
    onEffectInvertChanged: if (!isPlaying)
                               requestPreview()
    onEffectPosterizeChanged: if (!isPlaying)
                                  effectTimer.restart()
    onEffectPixelateChanged: if (!isPlaying)
                                 effectTimer.restart()
    onEffectTemperatureChanged: if (!isPlaying)
                                    effectTimer.restart()
    onEffectTintHueChanged: if (!isPlaying)
                                effectTimer.restart()
    onEffectTintStrChanged: if (!isPlaying)
                                effectTimer.restart()
    onEffectGrainChanged: if (!isPlaying)
                              effectTimer.restart()
    onEffectAutoEnhanceChanged: if (!isPlaying)
                                    effectTimer.restart()
    onEffectChromaKeyChanged: if (!isPlaying)
                                  requestPreview()
    onEffectChromaThresholdChanged: if (!isPlaying)
                                        effectTimer.restart()
    onEffectChromaSmoothnessChanged: if (!isPlaying)
                                         effectTimer.restart()

    // Single shared debounce timer for all "smooth" (non-binary) effects
    Timer {
        id: effectTimer
        interval: 80
        repeat: false
        onTriggered: videoPlayer.requestPreview()
    }

    onSelectedClipIdChanged: if (!isPlaying)
                                 Qt.callLater(requestPreview)

    // Request the first frame right after the component is created, so
    // the app doesn't show an empty black screen longer than necessary at startup
    Component.onCompleted: Qt.callLater(requestPreview)
}

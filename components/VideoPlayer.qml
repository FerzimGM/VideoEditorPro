// VideoPlayer.qml — Live-превью через FFmpeg + EffectImageProvider
// Расположение: components/VideoPlayer.qml
import QtQuick
import QtQuick.Controls

Rectangle {
    id: videoPlayer
    color: "#0a0a0a"

    // ── Внешний интерфейс ─────────────────────────────────────────────────
    property bool isPlaying: false
    property double currentTime: 0.0
    property double duration: 0.0 // полная длина таймлайна
    property real playbackSpeed: 1.0
    property double volume: 1.0

    // Скрытие видео/аудио
    property bool hideVideo: false // чёрный экран если оба трека пусты
    property bool hideTrack1Video: false
    property bool hideTrack2Video: false
    property bool hideAudio1: false
    property bool hideAudio2: false

    // Метаданные видео
    property int videoWidth: 0
    property int videoHeight: 0
    property real videoFps: 30.0

    // ── Эффекты для live-превью ───────────────────────────────────────────
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

    // ── Сигналы ───────────────────────────────────────────────────────────
    signal playbackStopped
    signal timePositionChanged(real time)

    // Публичный метод seek
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

    // ── Счётчик кадров ────────────────────────────────────────────────────
    property int _frameId: 0

    // ── Видео-дисплей ─────────────────────────────────────────────────────
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

    // Плейсхолдер
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

    // ── Соединения с C++ ──────────────────────────────────────────────────
    Connections {
        target: cppTimeline

        function onFrameReadyForDisplay() {
            videoPlayer._frameId++
        }

        // НЕ пишем videoPlayer.currentTime = time — сломает binding в main.qml
        function onPlaybackTimeUpdated(time) {
            videoPlayer.timePositionChanged(time)
        }

        function onPlaybackEnded() {
            // Только сигнал — main.qml сам сбросит isPlaying и время
            videoPlayer.playbackStopped()
        }

        function onClipsChanged() {
            if (!videoPlayer.isPlaying)
                Qt.callLater(videoPlayer.requestPreview)
        }
    }

    // ── Play / Pause ──────────────────────────────────────────────────────
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

    onPlaybackSpeedChanged: {
        if (isPlaying) {
            speedDebounceTimer.restart()
        }
    }

    Timer {
        id: speedDebounceTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (videoPlayer.isPlaying) {
                var exactTime = cppTimeline.getPlaybackTime()
                cppTimeline.stopPlayback()
                cppTimeline.startPlayback(exactTime, videoPlayer.playbackSpeed)
            }
        }
    }

    onVolumeChanged: if (cppTimeline)
                         cppTimeline.setPlaybackVolume(volume)
    onHideAudio1Changed: if (cppTimeline)
                             cppTimeline.setTrackAudioMuted(1, hideAudio1)
    onHideAudio2Changed: if (cppTimeline)
                             cppTimeline.setTrackAudioMuted(2, hideAudio2)

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

    // ── Скруббинг ─────────────────────────────────────────────────────────
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

    // ── requestPreview ────────────────────────────────────────────────────
    function requestPreview() {
        if (!cppTimeline)
            return
        cppTimeline.requestFrameForDisplay(currentTime, selectedClipId,
                                           buildEffectsMap())
    }

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
            m["tint_str"] = effectTintStr
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

    // ── Ползунки эффектов (debounce 80мс) ────────────────────────────────
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

    Timer {
        id: effectTimer
        interval: 80
        repeat: false
        onTriggered: videoPlayer.requestPreview()
    }

    onSelectedClipIdChanged: if (!isPlaying)
                                 Qt.callLater(requestPreview)

    Component.onCompleted: Qt.callLater(requestPreview)
}

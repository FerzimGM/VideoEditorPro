import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
// ← QMediaPlayer + AudioOutput + VideoOutput
import "../theme.js" as Theme

Rectangle {
    id: videoPlayer
    color: Theme.backgroundDark
    radius: Theme.borderRadius
    border.color: Theme.borderLight
    border.width: 1

    // ===== СВОЙСТВА =====
    property real currentTime: 0
    property real duration: cppTimeline ? Math.max(
                                              60,
                                              cppTimeline.totalDuration) : 60
    property bool isPlaying: false
    property real playbackSpeed: 1.0
    property real volume: 1.0
    property bool hideVideo: false
    property bool hideTrack1Video: false // скрыть только track1, track2 остаётся видим
    property bool hideAudio: false // legacy, не используется
    property bool hideAudio1: false // глушить track1 (mediaPlayer1)
    property bool hideAudio2: false // глушить track2 (mediaPlayer2)

    // ===== ЭФФЕКТЫ =====
    property real brightness: 1.0
    property real contrast: 1.0
    property real saturation: 1.0
    property bool grayscale: false

    // ===== ВНУТРЕННИЕ =====
    property string currentClipUrl: ""
    property string currentFrameSource: ""
    property bool _resyncing: false // guard против binding loop

    // ===== МЕТАДАННЫЕ =====
    property int videoWidth: 0
    property int videoHeight: 0
    property real videoFps: 0.0

    // Кэш метаданных по дорожкам
    QtObject {
        id: track1Cache
        property real clipStartTime: 0.0
        property real clipTrimStart: 0.0
        // *** FIX: позиция файла на которую делаем seek при старте клипа ***
        // Во время seek mp.position сообщает промежуточные значения (0→target).
        // Если обработать их как «реальную» позицию — boundary-check убьёт клип
        // (t = low_pos + startTime - trim < startTime → clipPathAt="" → stop).
        // Решение: пропускаем boundary-check и обновления времени пока
        // mp.position < expectedStartPos (всё ещё в процессе перемотки).
        property real expectedStartPos: 0.0
    }
    QtObject {
        id: track2Cache
        property real clipStartTime: 0.0
        property real clipTrimStart: 0.0
        property real expectedStartPos: 0.0
    }

    // ===== GUARD: предотвращает рекурсивный перезапуск трека =====
    // Проблема (бесконечный цикл):
    //   tryNextOnTrack() → mp.stop() + source="" → onPlaybackStateChanged(Stopped)
    //   → tryNextOnTrack() снова → loop
    // Решение: флаг "явно останавливаю" сбрасывается через Qt.callLater (1 тик).
    // onCurrentTimeChanged НЕ блокируется (нет time-guard) → трек запустится
    // на следующем тике как только playhead дойдёт до нового клипа.
    property bool _stoppingTrack1: false
    property bool _stoppingTrack2: false

    // *** FIX: отметка времени последнего запуска каждой дорожки ***
    // После mp.play() позиция ещё 0ms несколько тиков → resync-шторм.
    // Подавляем ресинхронизацию 500мс после каждого startTrack().
    property real _track1StartedAt: 0
    property real _track2StartedAt: 0

    // *** FIX: храним «сырой» путь клипа вместо mp.source.toString() ***
    // Проблема: QMediaPlayer.source URL-кодирует путь (кириллица → %D0%...,
    // пробелы → %20), а clipPathAt() возвращает сырую строку.
    // Сравнение mp.source.toString() !== p ВСЕГДА true даже для того же файла!
    // Результат: gapWatchTimer каждые 300ms думает что файл изменился → перезапускает.
    // Решение: сохраняем сырой путь здесь при startTrack, сравниваем с ним.
    property string _track1Path: ""
    property string _track2Path: ""

    // *** FIX: поколение ресинхронизации — защита от устаревших Qt.callLater ***
    // Проблема: Qt.callLater(startPlayback) ставится в очередь при resync,
    // но может выполниться спустя несколько секунд (после 5-6 resyncs),
    // когда всё уже нормально играет → внезапно останавливает оба трека.
    // Решение: каждый новый resync увеличивает счётчик поколений.
    // Deferred-вызов сверяет своё поколение — если устарел, не запускается.
    property int _resyncGeneration: 0

    // Сигнал: обновить playhead
    signal timePositionChanged(real newTime)
    // VideoPlayer НИКОГДА не пишет isPlaying = false напрямую!
    // isPlaying: playbackManager.isPlaying — QML binding в main.qml.
    // Прямое присваивание рвёт binding → кнопка "играет", полоска стоит.
    signal playbackStopped

    // ===== ЭФФЕКТЫ API =====
    function applyBrightness(v) {
        brightness = v
    }
    function applyContrast(v) {
        contrast = v
    }
    function applySaturation(v) {
        saturation = v
    }
    function applyGrayscale(v) {
        grayscale = v
    }
    function resetEffects() {
        brightness = 1.0
        contrast = 1.0
        saturation = 1.0
        grayscale = false
    }

    // ===== SCRUB FRAME =====
    function updateScrubFrame() {
        if (!cppTimeline) {
            currentFrameSource = ""
            return
        }
        var fp = cppTimeline.getFramePathAt(currentTime, 1)
        if (!fp || fp === "")
            fp = cppTimeline.getFramePathAt(currentTime, 2)
        currentFrameSource = (fp && fp !== "") ? fp + "?t=" + Date.now() : ""
        var info = cppTimeline.getClipInfoAt(currentTime, 1)
        if (!info || !info.width)
            info = cppTimeline.getClipInfoAt(currentTime, 2)
        if (info && info.width > 0) {
            videoWidth = info.width
            videoHeight = info.height
            videoFps = info.fps
        }
    }

    // ===== ХЕЛПЕРЫ =====
    function clipPathAt(t, track) {
        var p = cppTimeline.getActiveClipPath(t, track)
        return (p && p !== "") ? p : ""
    }
    function clipInfoAt(t, track) {
        var i = cppTimeline.getClipInfoAt(t, track)
        return (i && i.startTime !== undefined) ? i : null
    }
    function findNextOnTrack(fromTime, track) {
        if (!cppTimeline)
            return null
        var maxT = (cppTimeline.totalDuration || 120) + 2.0
        var step = 0.05 // ← Уменьшаем шаг для точности
        var t = fromTime + step

        // *** FIX: Ищем с небольшим допуском назад (0.1s) ***
        // Это нужно чтобы найти клип который начинается прямо сейчас
        while (t <= maxT) {
            var p = clipPathAt(t, track)
            if (p !== "") {
                var info = clipInfoAt(t, track)
                if (info && info.startTime > fromTime - 1.0)
                    return {
                        "path": p,
                        "startTime": info.startTime,
                        "trimStart": info.trimStart || 0.0,
                        "duration": info.duration || 0.0
                    }
                // Переходим к концу текущего клипа
                t = (info && info.startTime !== undefined
                     && info.duration) ? info.startTime + info.duration + step : t + step
            } else {
                t += step
            }
        }
        return null
    }

    // ===== ЗАПУСК ОДНОЙ ДОРОЖКИ =====
    function startTrack(track, t) {
        var mp = (track === 1) ? mediaPlayer1 : mediaPlayer2
        var cache = (track === 1) ? track1Cache : track2Cache

        var p = clipPathAt(t, track)
        if (p === "") {
            mp.stop()
            mp.source = ""
            return
        }

        var info = clipInfoAt(t, track)
        var st = info ? (info.startTime || 0.0) : 0.0
        var trim = info ? (info.trimStart || 0.0) : 0.0
        var posMs = Math.max(0, (t - st + trim) * 1000)

        // Не перезапускаем если уже играем тот же клип примерно в той же позиции
        // *** ВАЖНО: используем _track1Path/_track2Path, НЕ mp.source.toString()! ***
        // mp.source.toString() URL-кодирует кириллицу/пробелы → не совпадает с p.
        var storedPath = (track === 1) ? videoPlayer._track1Path : videoPlayer._track2Path
        if (storedPath === p && mp.playbackState === MediaPlayer.PlayingState
                && Math.abs(mp.position - posMs) < 1500) {
            return
        }

        cache.clipStartTime = st
        cache.clipTrimStart = trim
        cache.expectedStartPos = posMs // ← защита от seek-артефактов в onPositionChanged

        // Запоминаем момент запуска — подавляем ложный resync пока position не обновится
        if (track === 1) {
            videoPlayer._track1StartedAt = Date.now()
            videoPlayer._track1Path = p // ← сырой путь, без URL-кодирования
        } else {
            videoPlayer._track2StartedAt = Date.now()
            videoPlayer._track2Path = p
        }

        // Устанавливаем флаг ПЕРЕД stop() — иначе onPlaybackStateChanged(Stopped)
        // запустит nextClipTimer → tryNextOnTrack убьёт трек сразу после старта
        if (track === 1)
            videoPlayer._stoppingTrack1 = true
        else
            videoPlayer._stoppingTrack2 = true

        mp.stop()
        mp.source = p
        mp.position = posMs
        mp.playbackRate = videoPlayer.playbackSpeed
        mp.play()

        // Сбрасываем флаг после play() через callLater
        Qt.callLater(function () {
            if (track === 1)
                videoPlayer._stoppingTrack1 = false
            else
                videoPlayer._stoppingTrack2 = false
        })
        console.log("▶ Track", track, ":", p.split("/").pop(), "pos:",
                    posMs.toFixed(0), "ms")
    }

    // ===== ВОСПРОИЗВЕДЕНИЕ =====
    function startPlayback() {
        if (!cppTimeline) {
            videoPlayer.playbackStopped()
            return
        }

        // Останавливаем таймеры ПЕРЕД stop() чтобы избежать каскада
        gapClockTimer.stop()
        nextClipTimer1.stop()
        nextClipTimer2.stop()

        // Сбрасываем guard'ы блокировки — playback стартует заново
        videoPlayer._stoppingTrack1 = false
        videoPlayer._stoppingTrack2 = false

        // НЕ сбрасываем _resyncing здесь! Флаг живёт до resyncClearTimer (300ms).
        // Если сбросить здесь — binding loop может перезапустить startPlayback
        // в той же очереди событий до того как resyncClearTimer успел сработать.
        var t = currentTime
        // Если под playhead совсем нет клипов — ищем первый
        if (clipPathAt(t, 1) === "" && clipPathAt(t, 2) === "") {
            var n1 = findNextOnTrack(-0.1, 1)
            var n2 = findNextOnTrack(-0.1, 2)
            if (!n1 && !n2) {
                console.log("⚠️ Нет клипов")
                videoPlayer.playbackStopped()
                return
            }
            var firstT = Math.min(n1 ? n1.startTime : 999,
                                  n2 ? n2.startTime : 999)
            videoPlayer.timePositionChanged(firstT)
            t = firstT
        }

        startTrack(1, t)
        startTrack(2, t)
    }

    function pausePlayback() {
        gapClockTimer.stop()
        nextClipTimer1.stop()
        nextClipTimer2.stop()
        mediaPlayer1.pause()
        mediaPlayer2.pause()
        updateScrubFrame()
    }

    // ===== ПЕРЕХОД К СЛЕДУЮЩЕМУ КЛИПУ =====
    function tryNextOnTrack(track) {
        if (!cppTimeline || !videoPlayer.isPlaying)
            return

        if (track === 1 && videoPlayer._stoppingTrack1)
            return
        if (track === 2 && videoPlayer._stoppingTrack2)
            return

        var mp = (track === 1) ? mediaPlayer1 : mediaPlayer2

        // *** FIX: Ищем следующий клип от ТЕКУЩЕГО времени + небольшой допуск ***
        var next = findNextOnTrack(videoPlayer.currentTime + 0.05, track)

        if (next) {
            // *** FIX: Увеличиваем допуск до 2.0s для надежного перехода ***
            if (next.startTime <= videoPlayer.currentTime + 1.5) {
                videoPlayer.startTrack(track, next.startTime)
                console.log("⏭ Track", track, "next (immediate):",
                            next.path.split("/").pop())
            } else {
                // Клип далеко — останавливаем и ждем gapClock
                var mp = (track === 1) ? mediaPlayer1 : mediaPlayer2
                if (track === 1) {
                    videoPlayer._stoppingTrack1 = true
                    videoPlayer._track1Path = ""
                } else {
                    videoPlayer._stoppingTrack2 = true
                    videoPlayer._track2Path = ""
                }
                mp.stop()
                mp.source = ""
                Qt.callLater(function () {
                    if (track === 1)
                        videoPlayer._stoppingTrack1 = false
                    else
                        videoPlayer._stoppingTrack2 = false
                })
                console.log("⏸ Track", track, "gap until",
                            next.startTime.toFixed(2),
                            "s — waiting for gapClock")
            }
        } else {
            // *** FIX: Проверяем ДРУГУЮ дорожку перед тем как сказать "конец" ***
            var otherTrack = (track === 1) ? 2 : 1
            var otherNext = findNextOnTrack(videoPlayer.currentTime + 0.05,
                                            otherTrack)

            if (otherNext
                    && otherNext.startTime <= videoPlayer.currentTime + 2.0) {
                // На другой дорожке есть клип — запускаем его
                videoPlayer.startTrack(otherTrack, otherNext.startTime)
                console.log("⏭ Track", otherTrack, "bridge:",
                            otherNext.path.split("/").pop())
                return
            }

            // Действительно конец
            if (track === 1) {
                videoPlayer._stoppingTrack1 = true
                videoPlayer._track1Path = ""
                Qt.callLater(function () {
                    videoPlayer._stoppingTrack1 = false
                })
            } else {
                videoPlayer._stoppingTrack2 = true
                videoPlayer._track2Path = ""
                Qt.callLater(function () {
                    videoPlayer._stoppingTrack2 = false
                })
            }
            mp.stop()
            mp.source = ""

            var other = (track === 1) ? mediaPlayer2 : mediaPlayer1
            var startedOther = false

            if (other.playbackState !== MediaPlayer.PlayingState) {
                var pOtherNow = videoPlayer.clipPathAt(videoPlayer.currentTime,
                                                       otherTrack)
                if (pOtherNow !== "") {
                    videoPlayer.startTrack(otherTrack, videoPlayer.currentTime)
                    startedOther = true
                }
            }

            if (!startedOther) {
                var otherBusy = other.playbackState === MediaPlayer.PlayingState
                var otherHasNow = videoPlayer.clipPathAt(
                            videoPlayer.currentTime, otherTrack) !== ""
                var nextOther = findNextOnTrack(videoPlayer.currentTime + 0.05,
                                                otherTrack)

                if (!otherBusy && !otherHasNow && !nextOther) {
                    console.log("⏹ Конец таймлайна")
                    nextClipTimer1.stop()
                    nextClipTimer2.stop()
                    Qt.callLater(function () {
                        videoPlayer.playbackStopped()
                        videoPlayer.timePositionChanged(0.0)
                        videoPlayer.updateScrubFrame()
                    })
                }
            }
        }
    }

    // ===== РЕАКЦИИ =====
    onIsPlayingChanged: {
        if (isPlaying) {
            _resyncing = false
            startPlayback()
        } else {
            pausePlayback()
        }
    }
    onPlaybackSpeedChanged: {
        mediaPlayer1.playbackRate = playbackSpeed
        mediaPlayer2.playbackRate = playbackSpeed
    }

    onCurrentTimeChanged: {
        if (!isPlaying) {
            updateScrubFrame()
            return
        }
        if (_resyncing)
            return

        // Запуск дорожки 2 если она остановлена, но клип под playhead появился.
        // _stoppingTrack2 защищает только от рекурсии (один тик), не блокирует надолго.
        if (mediaPlayer2.playbackState !== MediaPlayer.PlayingState
                && !_stoppingTrack2) {
            var p2 = clipPathAt(currentTime, 2)
            if (p2 !== "" && videoPlayer._track2Path !== p2) {
                startTrack(2, currentTime)
            }
        }

        // Запуск дорожки 1 если она стоит, но клип под playhead появился.
        if (mediaPlayer1.playbackState !== MediaPlayer.PlayingState
                && !_stoppingTrack1) {
            var p1now = clipPathAt(currentTime, 1)
            if (p1now !== "" && videoPlayer._track1Path !== p1now) {
                startTrack(1, currentTime)
                return
            }
        }

        // Проверяем дорожку 1 для ресинхронизации
        var info1 = clipInfoAt(currentTime, 1)
        if (!info1)
            return
        // Гэп на дорожке 1 — ресинхронизация не нужна

        // Дорожка 1 в гэпе — ничего делать не нужно
        if (mediaPlayer1.playbackState !== MediaPlayer.PlayingState) {
            return
        }

        // Ресинхронизация: дорожка 1 играет, но сильно отстала (>2s)
        // ВАЖНО: Qt.callLater разрывает синхронный вызов в change-handler.
        // Без callLater: onCurrentTimeChanged → startPlayback() → mp.play() →
        // → positionChanged → timePositionChanged → currentTime меняется снова →
        // → onCurrentTimeChanged → binding loop detected!
        var exp1 = (currentTime - info1.startTime + (info1.trimStart
                                                     || 0)) * 1000
        // Порог ресинхронизации: 3s * скорость воспроизведения
        // При 2x скорости позиция меняется быстро → допускаем больший дрейф
        var resyncThreshold = 3000 * Math.max(1.0, videoPlayer.playbackSpeed)

        // *** FIX: подавляем resync первые 500мс после startTrack(1) ***
        // Сразу после mp.play() позиция = 0ms несколько тиков.
        // |exp1=22500 – pos=0| = 22500 >> threshold=3000 → ложный resync-шторм.
        var msSinceStart1 = Date.now() - videoPlayer._track1StartedAt
        if (msSinceStart1 < 500)
            return

        if (Math.abs(exp1 - mediaPlayer1.position) > resyncThreshold) {
            console.log("🔄 Ресинхронизация t=", currentTime.toFixed(2))
            _resyncing = true
            // *** FIX: поколение resync — deferred-вызов проверит актуальность ***
            var gen = ++videoPlayer._resyncGeneration
            Qt.callLater(function () {
                if (gen === videoPlayer._resyncGeneration)
                    startPlayback()
            })
            resyncClearTimer.restart()
        }
    }

    // Обновляем scrub frame при изменении duration (новые клипы добавлены)
    onDurationChanged: {
        if (!isPlaying)
            Qt.callLater(updateScrubFrame)
    }

    onVolumeChanged: {
        audioOut1.volume = videoPlayer.hideAudio1 ? 0.0 : volume
        audioOut2.volume = videoPlayer.hideAudio2 ? 0.0 : volume
    }

    Connections {
        target: cppTimeline
        function onClipsChanged() {
            if (!videoPlayer.isPlaying) {
                // При паузе — обновляем scrub frame по новым данным
                Qt.callLater(videoPlayer.updateScrubFrame)
                return
            }

            // При воспроизведении: клип мог быть УДАЛЁН, ОБРЕЗАН или ПЕРЕМЕЩЁН.
            // ─── ВАЖНО: почему принудительно останавливаем оба плеера ───────
            // При обрезке путь к файлу НЕ меняется — clipPathAt может вернуть
            // тот же path. Если просто проверить clipPathAt(t)==="" и оставить
            // играть дальше, MediaPlayer продолжит за новую trim-границу, потому
            // что он не знает о нашем trimEnd (знает только длину полного файла).
            // Решение: стоп → source="" → startPlayback() с нуля от currentTime.
            // startPlayback вычислит правильный posMs=(t-startTime+trimStart)*1000.
            // Минус: ~1 кадр чёрного при обрезке во время воспроизведения.
            // Это лучше, чем играть обрезанное видео ещё несколько секунд.
            mediaPlayer1.stop()
            mediaPlayer1.source = ""
            mediaPlayer2.stop()
            mediaPlayer2.source = ""
            videoPlayer._track1Path = ""
            videoPlayer._track2Path = ""

            videoPlayer._resyncing = true
            videoPlayer._resyncGeneration++
            var clipsChangedGen = videoPlayer._resyncGeneration
            Qt.callLater(function () {
                if (clipsChangedGen === videoPlayer._resyncGeneration
                        && videoPlayer.isPlaying)
                    videoPlayer.startPlayback()
            })
            resyncClearTimer.restart()
        }
    }

    // Сбрасываем флаг ресинхронизации через 300мс
    Timer {
        id: resyncClearTimer
        interval: 300
        repeat: false
        onTriggered: videoPlayer._resyncing = false
    }

    // ── Watchdog: каждые 300ms проверяем что ничего не "застряло" ──
    // Решает мёртвую зону: оба плеера стоят в промежутке между клипами,
    // никто не двигает playhead, но клипы на треках ещё есть.
    // Отдельно проверяем: клип появился под playhead но плеер не запустился.
    Timer {
        id: gapWatchTimer
        interval: 300
        repeat: true
        running: videoPlayer.isPlaying
        onTriggered: {
            if (!videoPlayer.isPlaying || videoPlayer._resyncing)
                return

            var t = videoPlayer.currentTime
            var p1busy = mediaPlayer1.playbackState === MediaPlayer.PlayingState
            var p2busy = mediaPlayer2.playbackState === MediaPlayer.PlayingState

            // Если клип есть под playhead но плеер не запущен — запускаем
            if (!p1busy) {
                var p1 = videoPlayer.clipPathAt(t, 1)
                if (p1 !== "" && videoPlayer._track1Path !== p1
                        && !videoPlayer._stoppingTrack1) {
                    console.log("👁 Watchdog: запускаем T1 при t=",
                                t.toFixed(2))
                    videoPlayer.startTrack(1, t)
                }
            }
            if (!p2busy) {
                var p2 = videoPlayer.clipPathAt(t, 2)
                if (p2 !== "" && videoPlayer._track2Path !== p2
                        && !videoPlayer._stoppingTrack2) {
                    console.log("👁 Watchdog: запускаем T2 при t=",
                                t.toFixed(2))
                    videoPlayer.startTrack(2, t)
                }
            }

            // Если ОБА плеера стоят — gap clock плавно двигает время до следующего клипа
            if (!p1busy && !p2busy) {
                var n1 = videoPlayer.findNextOnTrack(t, 1)
                var n2 = videoPlayer.findNextOnTrack(t, 2)
                if (n1 || n2) {
                    if (!gapClockTimer.running) {
                        gapClockTimer._lastWallTime = Date.now() / 1000.0
                        gapClockTimer.start()
                    }
                }
            }
        }
    }

    // ===== GAP CLOCK — плавно двигает playhead в гэпах между клипами =====
    Timer {
        id: gapClockTimer
        interval: 40
        repeat: true
        property real _lastWallTime: 0.0
        onTriggered: {
            if (!videoPlayer.isPlaying) {
                stop()
                return
            }
            if (mediaPlayer1.playbackState === MediaPlayer.PlayingState
                    || mediaPlayer2.playbackState === MediaPlayer.PlayingState) {
                stop()
                return
            }
            var now = Date.now() / 1000.0
            var elapsed = Math.min(
                        0.2, now - _lastWallTime) * videoPlayer.playbackSpeed
            _lastWallTime = now
            var newTime = videoPlayer.currentTime + elapsed

            // *** FIX: Лог для отладки ***
            console.log("⏱ gapClock:", videoPlayer.currentTime.toFixed(2), "→",
                        newTime.toFixed(2))

            var p1atNew = videoPlayer.clipPathAt(newTime, 1)
            var p2atNew = videoPlayer.clipPathAt(newTime, 2)
            if (p1atNew !== "" || p2atNew !== "") {
                stop()
                videoPlayer.timePositionChanged(newTime)
                if (p1atNew !== ""
                        && mediaPlayer1.playbackState !== MediaPlayer.PlayingState)
                    videoPlayer.startTrack(1, newTime)
                if (p2atNew !== ""
                        && mediaPlayer2.playbackState !== MediaPlayer.PlayingState)
                    videoPlayer.startTrack(2, newTime)
                return
            }
            if (!videoPlayer.findNextOnTrack(newTime, 1)
                    && !videoPlayer.findNextOnTrack(newTime, 2)) {
                stop()
                Qt.callLater(function () {
                    videoPlayer.playbackStopped()
                    videoPlayer.timePositionChanged(0.0)
                    videoPlayer.updateScrubFrame()
                })
                return
            }
            videoPlayer.timePositionChanged(newTime)
        }
    }

    // ===== MEDIAPLAYER 1 — ДОРОЖКА 1 (основная, поверх) =====
    MediaPlayer {
        id: mediaPlayer1
        videoOutput: videoOutput1
        audioOutput: AudioOutput {
            id: audioOut1
            // НЕ используем muted: — изменение muted на живом MediaPlayer
            // вызывает внутренний stop/seek в Qt Multimedia → перемотка в начало.
            // Просто обнуляем volume — тихо, без побочных эффектов на воспроизведение.
            volume: videoPlayer.hideAudio1 ? 0.0 : videoPlayer.volume
        }
        onErrorOccurred: function (e, s) {
            console.log("❌ T1:", s)
        }

        onPositionChanged: {
            if (!videoPlayer.isPlaying)
                return
            if (mediaPlayer1.source === "")
                return
            if (mediaPlayer1.playbackState !== MediaPlayer.PlayingState)
                return

            // Игнорируем позиции ДО завершения seek
            if (mediaPlayer1.position < track1Cache.expectedStartPos)
                return

            var t = mediaPlayer1.position / 1000.0 + track1Cache.clipStartTime
                    - track1Cache.clipTrimStart

            // Клип обрезан/удалён
            if (videoPlayer.clipPathAt(t, 1) === "") {
                mediaPlayer1.stop()
                mediaPlayer1.source = ""
                videoPlayer._track1Path = ""
                if (mediaPlayer2.playbackState !== MediaPlayer.PlayingState)
                    Qt.callLater(videoPlayer.startPlayback)
                return
            }

            // *** FIX: Проверяем что время не уходит НАЗАД больше чем на 0.5s ***
            // Если уходит — это признак что плеер "запутался", нужно ресинхронизировать
            if (t < videoPlayer.currentTime - 0.5) {
                console.log("⚠️ T1: время уходит назад (", t.toFixed(2), "<",
                            videoPlayer.currentTime.toFixed(2),
                            ") — игнорируем")
                return
            }

            // Обновляем время только если оно ВПЕРЕДИ или близко к текущему
            if (t >= videoPlayer.currentTime - 0.1) {
                videoPlayer.timePositionChanged(t)

                // Подхватываем дорожку 2
                if (mediaPlayer2.playbackState !== MediaPlayer.PlayingState) {
                    var p2 = videoPlayer.clipPathAt(t, 2)
                    if (p2 !== "" && videoPlayer._track2Path !== p2)
                        videoPlayer.startTrack(2, t)
                }
            }
        }

        onPlaybackStateChanged: {
            if (mediaPlayer1.playbackState === MediaPlayer.StoppedState
                    && videoPlayer.isPlaying)
                nextClipTimer1.restart()
        }
    }

    Timer {
        id: nextClipTimer1
        interval: 50
        repeat: false
        onTriggered: {
            if (mediaPlayer1.playbackState === MediaPlayer.StoppedState
                    && videoPlayer.isPlaying && !videoPlayer._stoppingTrack1)
                videoPlayer.tryNextOnTrack(1)
        }
    }

    // ===== MEDIAPLAYER 2 — ДОРОЖКА 2 (фоновая, снизу) =====
    MediaPlayer {
        id: mediaPlayer2
        videoOutput: videoOutput2
        audioOutput: AudioOutput {
            id: audioOut2
            // Аналогично audioOut1 — только volume, без muted.
            volume: videoPlayer.hideAudio2 ? 0.0 : videoPlayer.volume
        }
        onErrorOccurred: function (e, s) {
            console.log("❌ T2:", s)
        }

        // Дорожка 2 НЕ двигает playhead — только когда дорожка 1 пустая
        onPositionChanged: {
            if (!videoPlayer.isPlaying)
                return
            if (mediaPlayer2.source === "")
                return
            if (mediaPlayer2.playbackState !== MediaPlayer.PlayingState)
                return

            // *** FIX: игнорируем позиции ДО завершения seek (см. onPositionChanged T1) ***
            if (mediaPlayer2.position < track2Cache.expectedStartPos)
                return

            // дорожка 1 — мастер позиции
            var t = mediaPlayer2.position / 1000.0 + track2Cache.clipStartTime
                    - track2Cache.clipTrimStart

            // ─── Проверка границы ВСЕГДА — даже если трек 1 играет ───────
            if (videoPlayer.clipPathAt(t, 2) === "") {
                mediaPlayer2.stop()
                mediaPlayer2.source = ""
                videoPlayer._track2Path = ""
                if (mediaPlayer1.playbackState !== MediaPlayer.PlayingState)
                    Qt.callLater(videoPlayer.startPlayback)
                return
            }

            // Трек 1 управляет позицией playhead — дальше ничего не делаем
            if (mediaPlayer1.playbackState === MediaPlayer.PlayingState)
                return

            // *** FIX: защита от прыжка назад (аналог T1, см. выше) ***
            if (t < videoPlayer.currentTime - 0.1)
                return

            videoPlayer.timePositionChanged(t)

            // FIX: если дорожка 1 имеет клип, но не играет — запускаем её
            if (mediaPlayer1.playbackState !== MediaPlayer.PlayingState) {
                var p1 = videoPlayer.clipPathAt(t, 1)
                // *** ВАЖНО: сравниваем с _track1Path, не с URL-кодированным source! ***
                if (p1 !== "" && videoPlayer._track1Path !== p1)
                    videoPlayer.startTrack(1, t)
            }
        }

        onPlaybackStateChanged: {
            if (mediaPlayer2.playbackState === MediaPlayer.StoppedState
                    && videoPlayer.isPlaying)
                nextClipTimer2.restart()
        }
    }

    Timer {
        id: nextClipTimer2
        interval: 50
        repeat: false
        onTriggered: {
            if (mediaPlayer2.playbackState === MediaPlayer.StoppedState
                    && videoPlayer.isPlaying && !videoPlayer._stoppingTrack2)
                videoPlayer.tryNextOnTrack(2)
        }
    }

    // Градиентная рамка
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.width: 2
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.rubyGradientStart
            }
            GradientStop {
                position: 0.5
                color: "transparent"
            }
            GradientStop {
                position: 1.0
                color: Theme.rubyGradientEnd
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacing

        // ===== ОБЛАСТЬ ВИДЕО =====
        Rectangle {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#000000"
            radius: Theme.borderRadius
            clip: true

            // Дорожка 2 — фон (z:1)
            VideoOutput {
                id: videoOutput2
                anchors.fill: parent
                z: 1
                visible: !videoPlayer.hideVideo && videoPlayer.isPlaying
                         && mediaPlayer2.source !== ""
                         && mediaPlayer2.playbackState === MediaPlayer.PlayingState
            }
            // Дорожка 1 — поверх (z:2) + шейдер эффектов
            VideoOutput {
                id: videoOutput1
                anchors.fill: parent
                z: 2
                visible: !videoPlayer.hideVideo && !videoPlayer.hideTrack1Video
                         && videoPlayer.isPlaying
                         && (mediaPlayer1.playbackState === MediaPlayer.PlayingState
                             || (mediaPlayer1.source !== ""
                                 && videoPlayer.clipPathAt(
                                     videoPlayer.currentTime, 1) !== ""))
                layer.enabled: videoPlayer.brightness !== 1.0
                               || videoPlayer.contrast !== 1.0
                               || videoPlayer.saturation !== 1.0
                               || videoPlayer.grayscale
                layer.effect: ShaderEffect {
                    property real brightness: videoPlayer.brightness
                    property real contrast: videoPlayer.contrast
                    property real saturation: videoPlayer.saturation
                    property bool grayscale: videoPlayer.grayscale
                    fragmentShader: "
uniform lowp sampler2D source;
uniform lowp float brightness; uniform lowp float contrast;
uniform lowp float saturation; uniform bool grayscale;
varying highp vec2 qt_TexCoord0;
void main() {
vec4 c = texture2D(source, qt_TexCoord0);
c.rgb *= brightness; c.rgb = (c.rgb - 0.5) * contrast + 0.5;
float g = dot(c.rgb, vec3(0.299,0.587,0.114));
c.rgb = mix(vec3(g), c.rgb, saturation);
if (grayscale) c.rgb = vec3(g);
gl_FragColor = vec4(clamp(c.rgb,0.0,1.0),1.0); }"
                }
            }

            Image {
                id: scrubFrame
                anchors.fill: parent
                z: 3
                source: videoPlayer.currentFrameSource
                fillMode: Image.PreserveAspectFit
                cache: false
                asynchronous: true
                visible: !videoPlayer.hideVideo && !videoPlayer.hideTrack1Video
                         && !videoPlayer.isPlaying
                         && videoPlayer.currentFrameSource !== ""
                BusyIndicator {
                    anchors.centerIn: parent
                    running: scrubFrame.status === Image.Loading
                    visible: running
                    width: 32
                    height: 32
                    palette.dark: Theme.rubyPrimary
                }
                layer.enabled: videoPlayer.brightness !== 1.0
                               || videoPlayer.contrast !== 1.0
                               || videoPlayer.saturation !== 1.0
                               || videoPlayer.grayscale
                layer.effect: ShaderEffect {
                    property real brightness: videoPlayer.brightness
                    property real contrast: videoPlayer.contrast
                    property real saturation: videoPlayer.saturation
                    property bool grayscale: videoPlayer.grayscale
                    fragmentShader: "
uniform lowp sampler2D source;
uniform lowp float brightness; uniform lowp float contrast;
uniform lowp float saturation; uniform bool grayscale;
varying highp vec2 qt_TexCoord0;
void main() {
vec4 c = texture2D(source, qt_TexCoord0);
c.rgb *= brightness; c.rgb = (c.rgb - 0.5) * contrast + 0.5;
float g = dot(c.rgb, vec3(0.299,0.587,0.114));
c.rgb = mix(vec3(g), c.rgb, saturation);
if (grayscale) c.rgb = vec3(g);
gl_FragColor = vec4(clamp(c.rgb,0.0,1.0),1.0);
}"
                }
            }

            // Плейсхолдер — два состояния
            // 1) Кадр грузится (есть клип, но FFmpeg ещё не декодировал)
            BusyIndicator {
                id: frameLoadingIndicator
                anchors.centerIn: parent
                z: 1
                width: 48
                height: 48
                palette.dark: Theme.rubyPrimary
                // Видим: не играем, нет готового кадра, НО есть клип под playhead
                visible: !videoPlayer.isPlaying
                         && videoPlayer.currentFrameSource === ""
                         && (cppTimeline ? (cppTimeline.getActiveClipPath(
                                                videoPlayer.currentTime,
                                                1) !== ""
                                            || cppTimeline.getActiveClipPath(
                                                videoPlayer.currentTime,
                                                2) !== "") : false)
                         && !videoOutput1.visible && !videoOutput2.visible
            }

            // 2) Реально пустой таймлайн или позиция без клипов
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingLarge
                z: 0
                visible: !videoOutput1.visible && !videoOutput2.visible
                         && !scrubFrame.visible
                         && !frameLoadingIndicator.visible
                Text {
                    text: "▶"
                    color: Theme.rubyPrimary
                    font.pixelSize: 72
                    opacity: 0.3
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: videoPlayer.currentTime > 0 ? "Нет видео в этой позиции" : "Видеоплеер"
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLarge
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: videoPlayer.currentTime > 0 ? "Переместите playhead на клип или нажмите ▶" : "Добавьте видео на таймлайн"
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Временной код
            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.spacing
                width: tcText.width + Theme.spacing * 2
                height: tcText.height + Theme.spacingSmall * 2
                color: Qt.rgba(0, 0, 0, 0.7)
                radius: Theme.borderRadius
                Text {
                    id: tcText
                    anchors.centerIn: parent
                    text: formatTime(
                              videoPlayer.currentTime) + " / " + formatTime(
                              videoPlayer.duration)
                    color: Theme.rubyLight
                    font.family: "Consolas, monospace"
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                }
            }

            // Индикатор скорости
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.spacing
                width: spdText.width + 12
                height: spdText.height + 6
                radius: Theme.borderRadius
                color: Qt.rgba(0, 0, 0, 0.7)
                visible: videoPlayer.playbackSpeed !== 1.0
                Text {
                    id: spdText
                    anchors.centerIn: parent
                    text: videoPlayer.playbackSpeed + "x"
                    color: Theme.rubyPrimary
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                }
            }
        }

        // ===== МЕТАДАННЫЕ + ГРОМКОСТЬ =====
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLarge

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 1
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSmall
                    Text {
                        text: "Разрешение"
                        color: Theme.rubyLight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: videoPlayer.videoWidth > 0 ? videoPlayer.videoWidth + " × "
                                                           + videoPlayer.videoHeight : "— × —"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 1
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSmall
                    Text {
                        text: "Частота кадров"
                        color: Theme.rubyLight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: videoPlayer.videoFps > 0 ? videoPlayer.videoFps.toFixed(
                                                             3) + " FPS" : "— FPS"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 60
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: videoPlayer.volume
                                  === 0 ? "🔇" : (videoPlayer.volume < 0.5 ? "🔉" : "🔊")
                            font.pixelSize: 16
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: videoPlayer.volume = videoPlayer.volume > 0 ? 0 : 1.0
                            }
                        }
                        Text {
                            text: "Громкость"
                            color: Theme.rubyLight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                        Text {
                            text: Math.round(videoPlayer.volume * 100) + "%"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                    Slider {
                        id: volSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        value: videoPlayer.volume
                        stepSize: 0.01
                        onValueChanged: videoPlayer.volume = value
                        background: Rectangle {
                            x: volSlider.leftPadding
                            y: volSlider.topPadding + (volSlider.availableHeight - height) / 2
                            width: volSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.backgroundDark
                            border.color: Theme.borderLight
                            border.width: 1
                            Rectangle {
                                width: volSlider.visualPosition * parent.width
                                height: parent.height
                                radius: parent.radius
                                color: Theme.rubyPrimary
                            }
                        }
                        handle: Rectangle {
                            x: volSlider.leftPadding + volSlider.visualPosition
                               * (volSlider.availableWidth - width)
                            y: volSlider.topPadding + (volSlider.availableHeight - height) / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: Theme.rubyLight
                            border.color: Theme.rubyPrimary
                            border.width: 1
                        }
                    }
                }
            }
        }
    }

    function formatTime(s) {
        var h = Math.floor(
                    s / 3600), m = Math.floor(
                                   (s % 3600) / 60), ss = Math.floor(s % 60)
        return h > 0 ? pad(h) + ":" + pad(m) + ":" + pad(ss) : pad(
                           m) + ":" + pad(ss)
    }
    function pad(n) {
        return n < 10 ? "0" + n : String(n)
    }
}

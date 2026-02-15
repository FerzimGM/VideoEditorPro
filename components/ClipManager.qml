// ClipManager.qml - Менеджер клипов для фронтенда
// Позже будет подключен к C++ бэкенду через сигналы/слоты

import QtQuick

QtObject {
    id: clipManager
    
    // Список всех клипов на таймлайне
    property var clips: []
    property int revision: 0
    
    // Сигналы для будущего бэкенда
    signal clipAdded(string filepath, int trackNumber, real startTime)
    signal clipMoved(int clipId, int newTrack, real newTime)
    signal clipRemoved(int clipId)
    signal clipSelected(int clipId)
    
    // Добавить клип (пока mock с фейковой длительностью)
    function addClip(filepath, trackNumber, startTime, duration) {
        var clipId = clips.length
        
        // Mock: случайная длительность 10-60 сек (как будто FFmpeg вернул)
        var mockDuration = duration || (Math.random() * 50 + 10)
        
        var clip = {
            id: clipId,
            filepath: filepath,
            filename: filepath.split('/').pop().split('\\').pop(),
            trackNumber: trackNumber,
            startTime: startTime,
            duration: mockDuration,  // Mock длительность!
            thumbnailPath: "",  // TODO: FFmpeg will generate thumbnail
            selected: false,
            videoPath: filepath,
            audioPath: filepath,
            hasVideo: true,
            hasAudio: true
        }
        
        clips.push(clip)
        revision++
        clipsChanged()
        
        clipAdded(filepath, trackNumber, startTime)
        
        // TODO: Request thumbnail from FFmpeg
        // cppFFmpeg.generateThumbnail(filepath, 0.0)
        // → signal thumbnailReady(filepath, thumbnailPath)
        // → updateClipThumbnail(clipId, thumbnailPath)
        
        console.log("Mock клип добавлен:", filepath, "длительность:", mockDuration.toFixed(1), "сек")
        return clipId
    }
    
    // Переместить клип
    function moveClip(clipId, newTrack, newTime) {
        for (var i = 0; i < clips.length; i++) {
            if (clips[i].id === clipId) {
                clips[i].trackNumber = newTrack
                clips[i].startTime = newTime
                revision++
                clipsChanged()
                clipMoved(clipId, newTrack, newTime)
                break
            }
        }
    }
    
    // Удалить клип
    function removeClip(clipId) {
        for (var i = 0; i < clips.length; i++) {
            if (clips[i].id === clipId) {
                clips.splice(i, 1)
                revision++
                clipsChanged()
                clipRemoved(clipId)
                break
            }
        }
    }
    
    // Выбрать клип
    function selectClip(clipId) {
        for (var i = 0; i < clips.length; i++) {
            clips[i].selected = (clips[i].id === clipId)
        }
        revision++
        clipsChanged()
        clipSelected(clipId)
    }
    
    // Получить клипы для дорожки
    function getClipsForTrack(trackNumber) {
        var result = []
        for (var i = 0; i < clips.length; i++) {
            if (clips[i].trackNumber === trackNumber) {
                result.push(clips[i])
            }
        }
        return result
    }
    
    // Очистить всё
    function clearAll() {
        clips = []
        clipsChanged()
    }
}

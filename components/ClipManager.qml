// ClipManager.qml - Менеджер клипов для фронтенда
// Позже будет подключен к C++ бэкенду через сигналы/слоты

import QtQuick

QtObject {
    id: clipManager
    
    // Список всех клипов на таймлайне
    property var clips: []
    property int revision: 0  // Счётчик изменений для триггера обновления
    
    // Сигналы для будущего бэкенда
    signal clipAdded(string filepath, int trackNumber, real startTime)
    signal clipMoved(int clipId, int newTrack, real newTime)
    signal clipRemoved(int clipId)
    signal clipSelected(int clipId)
    
    // Добавить клип
    function addClip(filepath, trackNumber, startTime, duration) {
        var clip = {
            id: clips.length,
            filepath: filepath,
            filename: filepath.split('/').pop().split('\\').pop(),  // Windows путь
            trackNumber: trackNumber,
            startTime: startTime,
            duration: duration || 10.0,
            selected: false,
            videoPath: filepath,
            audioPath: filepath,
            hasVideo: true,
            hasAudio: true
        }
        
        clips.push(clip)
        revision++  // Триггер обновления
        clipsChanged()
        
        clipAdded(filepath, trackNumber, startTime)
        
        console.log("Клип добавлен:", filepath, "на дорожку", trackNumber, "revision:", revision)
        return clip.id
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

/****************************************************************************
** Meta object code from reading C++ file 'timeline.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../timeline.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'timeline.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN8TimelineE_t {};
} // unnamed namespace

template <> constexpr inline auto Timeline::qt_create_metaobjectdata<qt_meta_tag_ZN8TimelineE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Timeline",
        "currentTimeChanged",
        "",
        "totalDurationChanged",
        "clipsChanged",
        "clipAdded",
        "index",
        "clipRemoved",
        "clipModified",
        "renderProgress",
        "percent",
        "renderFinished",
        "success",
        "frameReadyForDisplay",
        "playbackTimeUpdated",
        "time",
        "playbackEnded",
        "frameReady",
        "QImage",
        "frame",
        "requestFrameForDisplay",
        "selectedClipId",
        "QVariantMap",
        "previewEffects",
        "getCurrentFrameAt",
        "trackIndex",
        "getClipInfoAt",
        "getClipInfoById",
        "uidOrIndex",
        "startPlayback",
        "fromTime",
        "speed",
        "stopPlayback",
        "setPlaybackVolume",
        "volume",
        "setTrackAudioMuted",
        "track",
        "muted",
        "setTrackVideoHidden",
        "hidden",
        "getPlaybackTime",
        "addClip",
        "filepath",
        "startTime",
        "removeClip",
        "moveClip",
        "newTrackIndex",
        "newStartTime",
        "splitClipAt",
        "splitClip",
        "splitTime",
        "trimClip",
        "newTrimStart",
        "newTrimEnd",
        "setClipLeftTrim",
        "applyEffect",
        "effectName",
        "value",
        "removeEffect",
        "getClipEffects",
        "setClipMuted",
        "getTrackEndTime",
        "getClipsForTrack",
        "QVariantList",
        "setClipVideoHidden",
        "setClipAudioHidden",
        "syncClipStatesForRender",
        "hiddenMap",
        "mutedMap",
        "saveProject",
        "loadProject",
        "renderToFile",
        "outputPath",
        "width",
        "height",
        "format",
        "cancelRender",
        "currentTime",
        "totalDuration",
        "clipCount"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'currentTimeChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'totalDurationChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'clipsChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'clipAdded'
        QtMocHelpers::SignalData<void(int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Signal 'clipRemoved'
        QtMocHelpers::SignalData<void(int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Signal 'clipModified'
        QtMocHelpers::SignalData<void(int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Signal 'renderProgress'
        QtMocHelpers::SignalData<void(int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Signal 'renderFinished'
        QtMocHelpers::SignalData<void(bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 },
        }}),
        // Signal 'frameReadyForDisplay'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playbackTimeUpdated'
        QtMocHelpers::SignalData<void(double)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 15 },
        }}),
        // Signal 'playbackEnded'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'frameReady'
        QtMocHelpers::SignalData<void(const QImage &, double)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 18, 19 }, { QMetaType::Double, 15 },
        }}),
        // Method 'requestFrameForDisplay'
        QtMocHelpers::MethodData<void(double, int, const QVariantMap &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 15 }, { QMetaType::Int, 21 }, { 0x80000000 | 22, 23 },
        }}),
        // Method 'requestFrameForDisplay'
        QtMocHelpers::MethodData<void(double, int)>(20, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Double, 15 }, { QMetaType::Int, 21 },
        }}),
        // Method 'requestFrameForDisplay'
        QtMocHelpers::MethodData<void(double)>(20, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Double, 15 },
        }}),
        // Method 'getCurrentFrameAt'
        QtMocHelpers::MethodData<QImage(double, int)>(24, 2, QMC::AccessPublic, 0x80000000 | 18, {{
            { QMetaType::Double, 15 }, { QMetaType::Int, 25 },
        }}),
        // Method 'getCurrentFrameAt'
        QtMocHelpers::MethodData<QImage(double)>(24, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 18, {{
            { QMetaType::Double, 15 },
        }}),
        // Method 'getClipInfoAt'
        QtMocHelpers::MethodData<QVariantMap(double, int)>(26, 2, QMC::AccessPublic, 0x80000000 | 22, {{
            { QMetaType::Double, 15 }, { QMetaType::Int, 25 },
        }}),
        // Method 'getClipInfoAt'
        QtMocHelpers::MethodData<QVariantMap(double)>(26, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 22, {{
            { QMetaType::Double, 15 },
        }}),
        // Method 'getClipInfoById'
        QtMocHelpers::MethodData<QVariantMap(int)>(27, 2, QMC::AccessPublic, 0x80000000 | 22, {{
            { QMetaType::Int, 28 },
        }}),
        // Method 'startPlayback'
        QtMocHelpers::MethodData<void(double, double)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 30 }, { QMetaType::Double, 31 },
        }}),
        // Method 'startPlayback'
        QtMocHelpers::MethodData<void(double)>(29, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Double, 30 },
        }}),
        // Method 'stopPlayback'
        QtMocHelpers::MethodData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'setPlaybackVolume'
        QtMocHelpers::MethodData<void(double)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 34 },
        }}),
        // Method 'setTrackAudioMuted'
        QtMocHelpers::MethodData<void(int, bool)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 36 }, { QMetaType::Bool, 37 },
        }}),
        // Method 'setTrackVideoHidden'
        QtMocHelpers::MethodData<void(int, bool)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 36 }, { QMetaType::Bool, 39 },
        }}),
        // Method 'getPlaybackTime'
        QtMocHelpers::MethodData<double() const>(40, 2, QMC::AccessPublic, QMetaType::Double),
        // Method 'addClip'
        QtMocHelpers::MethodData<bool(const QString &, int, double)>(41, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 42 }, { QMetaType::Int, 25 }, { QMetaType::Double, 43 },
        }}),
        // Method 'removeClip'
        QtMocHelpers::MethodData<bool(int)>(44, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 },
        }}),
        // Method 'moveClip'
        QtMocHelpers::MethodData<bool(int, int, double)>(45, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Int, 46 }, { QMetaType::Double, 47 },
        }}),
        // Method 'splitClipAt'
        QtMocHelpers::MethodData<bool(double, int)>(48, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Double, 15 }, { QMetaType::Int, 25 },
        }}),
        // Method 'splitClipAt'
        QtMocHelpers::MethodData<bool(double)>(48, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::Double, 15 },
        }}),
        // Method 'splitClip'
        QtMocHelpers::MethodData<bool(int, double)>(49, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Double, 50 },
        }}),
        // Method 'trimClip'
        QtMocHelpers::MethodData<bool(int, double, double)>(51, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Double, 52 }, { QMetaType::Double, 53 },
        }}),
        // Method 'setClipLeftTrim'
        QtMocHelpers::MethodData<bool(int, double, double)>(54, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Double, 47 }, { QMetaType::Double, 52 },
        }}),
        // Method 'applyEffect'
        QtMocHelpers::MethodData<bool(int, const QString &, double)>(55, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::QString, 56 }, { QMetaType::Double, 57 },
        }}),
        // Method 'removeEffect'
        QtMocHelpers::MethodData<bool(int, const QString &)>(58, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::QString, 56 },
        }}),
        // Method 'getClipEffects'
        QtMocHelpers::MethodData<QVariantMap(int) const>(59, 2, QMC::AccessPublic, 0x80000000 | 22, {{
            { QMetaType::Int, 6 },
        }}),
        // Method 'setClipMuted'
        QtMocHelpers::MethodData<bool(int, bool)>(60, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Bool, 37 },
        }}),
        // Method 'getTrackEndTime'
        QtMocHelpers::MethodData<double(int) const>(61, 2, QMC::AccessPublic, QMetaType::Double, {{
            { QMetaType::Int, 25 },
        }}),
        // Method 'getClipsForTrack'
        QtMocHelpers::MethodData<QVariantList(int)>(62, 2, QMC::AccessPublic, 0x80000000 | 63, {{
            { QMetaType::Int, 25 },
        }}),
        // Method 'setClipVideoHidden'
        QtMocHelpers::MethodData<void(int, bool)>(64, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { QMetaType::Bool, 39 },
        }}),
        // Method 'setClipAudioHidden'
        QtMocHelpers::MethodData<void(int, bool)>(65, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { QMetaType::Bool, 39 },
        }}),
        // Method 'syncClipStatesForRender'
        QtMocHelpers::MethodData<void(QVariantMap, QVariantMap)>(66, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 22, 67 }, { 0x80000000 | 22, 68 },
        }}),
        // Method 'saveProject'
        QtMocHelpers::MethodData<bool(const QString &)>(69, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 42 },
        }}),
        // Method 'loadProject'
        QtMocHelpers::MethodData<bool(const QString &)>(70, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 42 },
        }}),
        // Method 'renderToFile'
        QtMocHelpers::MethodData<bool(const QString &, int, int, const QString &)>(71, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 72 }, { QMetaType::Int, 73 }, { QMetaType::Int, 74 }, { QMetaType::QString, 75 },
        }}),
        // Method 'renderToFile'
        QtMocHelpers::MethodData<bool(const QString &, int, int)>(71, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 72 }, { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'renderToFile'
        QtMocHelpers::MethodData<bool(const QString &, int)>(71, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 72 }, { QMetaType::Int, 73 },
        }}),
        // Method 'renderToFile'
        QtMocHelpers::MethodData<bool(const QString &)>(71, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 72 },
        }}),
        // Method 'cancelRender'
        QtMocHelpers::MethodData<void()>(76, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'currentTime'
        QtMocHelpers::PropertyData<double>(77, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'totalDuration'
        QtMocHelpers::PropertyData<double>(78, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'clipCount'
        QtMocHelpers::PropertyData<int>(79, QMetaType::Int, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Timeline, qt_meta_tag_ZN8TimelineE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Timeline::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8TimelineE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8TimelineE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8TimelineE_t>.metaTypes,
    nullptr
} };

void Timeline::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Timeline *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentTimeChanged(); break;
        case 1: _t->totalDurationChanged(); break;
        case 2: _t->clipsChanged(); break;
        case 3: _t->clipAdded((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->clipRemoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->clipModified((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->renderProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->renderFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->frameReadyForDisplay(); break;
        case 9: _t->playbackTimeUpdated((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 10: _t->playbackEnded(); break;
        case 11: _t->frameReady((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 12: _t->requestFrameForDisplay((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 13: _t->requestFrameForDisplay((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 14: _t->requestFrameForDisplay((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 15: { QImage _r = _t->getCurrentFrameAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QImage*>(_a[0]) = std::move(_r); }  break;
        case 16: { QImage _r = _t->getCurrentFrameAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QImage*>(_a[0]) = std::move(_r); }  break;
        case 17: { QVariantMap _r = _t->getClipInfoAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 18: { QVariantMap _r = _t->getClipInfoAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 19: { QVariantMap _r = _t->getClipInfoById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 20: _t->startPlayback((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 21: _t->startPlayback((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 22: _t->stopPlayback(); break;
        case 23: _t->setPlaybackVolume((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 24: _t->setTrackAudioMuted((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 25: _t->setTrackVideoHidden((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 26: { double _r = _t->getPlaybackTime();
            if (_a[0]) *reinterpret_cast<double*>(_a[0]) = std::move(_r); }  break;
        case 27: { bool _r = _t->addClip((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 28: { bool _r = _t->removeClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 29: { bool _r = _t->moveClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 30: { bool _r = _t->splitClipAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 31: { bool _r = _t->splitClipAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 32: { bool _r = _t->splitClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 33: { bool _r = _t->trimClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 34: { bool _r = _t->setClipLeftTrim((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 35: { bool _r = _t->applyEffect((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 36: { bool _r = _t->removeEffect((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 37: { QVariantMap _r = _t->getClipEffects((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 38: { bool _r = _t->setClipMuted((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 39: { double _r = _t->getTrackEndTime((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<double*>(_a[0]) = std::move(_r); }  break;
        case 40: { QVariantList _r = _t->getClipsForTrack((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 41: _t->setClipVideoHidden((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 42: _t->setClipAudioHidden((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 43: _t->syncClipStatesForRender((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[2]))); break;
        case 44: { bool _r = _t->saveProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 45: { bool _r = _t->loadProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 46: { bool _r = _t->renderToFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 47: { bool _r = _t->renderToFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 48: { bool _r = _t->renderToFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 49: { bool _r = _t->renderToFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 50: _t->cancelRender(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)()>(_a, &Timeline::currentTimeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)()>(_a, &Timeline::totalDurationChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)()>(_a, &Timeline::clipsChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(int )>(_a, &Timeline::clipAdded, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(int )>(_a, &Timeline::clipRemoved, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(int )>(_a, &Timeline::clipModified, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(int )>(_a, &Timeline::renderProgress, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(bool )>(_a, &Timeline::renderFinished, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)()>(_a, &Timeline::frameReadyForDisplay, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(double )>(_a, &Timeline::playbackTimeUpdated, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)()>(_a, &Timeline::playbackEnded, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(const QImage & , double )>(_a, &Timeline::frameReady, 11))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<double*>(_v) = _t->currentTime(); break;
        case 1: *reinterpret_cast<double*>(_v) = _t->totalDuration(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->clipCount(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setCurrentTime(*reinterpret_cast<double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *Timeline::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Timeline::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8TimelineE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Timeline::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 51)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 51;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 51)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 51;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void Timeline::currentTimeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Timeline::totalDurationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Timeline::clipsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Timeline::clipAdded(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void Timeline::clipRemoved(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void Timeline::clipModified(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void Timeline::renderProgress(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void Timeline::renderFinished(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void Timeline::frameReadyForDisplay()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Timeline::playbackTimeUpdated(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void Timeline::playbackEnded()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void Timeline::frameReady(const QImage & _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2);
}
QT_WARNING_POP

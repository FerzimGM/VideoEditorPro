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
        "frameReady",
        "QImage",
        "frame",
        "time",
        "getCurrentFrameAt",
        "trackIndex",
        "getFramePathAt",
        "requestFrame",
        "getClipInfoAt",
        "QVariantMap",
        "getActiveClipPath",
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
        "setClipMuted",
        "muted",
        "getTrackEndTime",
        "getClipsForTrack",
        "QVariantList",
        "saveProject",
        "loadProject",
        "renderToFile",
        "outputPath",
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
        // Signal 'frameReady'
        QtMocHelpers::SignalData<void(const QImage &, double)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 14, 15 }, { QMetaType::Double, 16 },
        }}),
        // Method 'getCurrentFrameAt'
        QtMocHelpers::MethodData<QImage(double, int)>(17, 2, QMC::AccessPublic, 0x80000000 | 14, {{
            { QMetaType::Double, 16 }, { QMetaType::Int, 18 },
        }}),
        // Method 'getCurrentFrameAt'
        QtMocHelpers::MethodData<QImage(double)>(17, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 14, {{
            { QMetaType::Double, 16 },
        }}),
        // Method 'getFramePathAt'
        QtMocHelpers::MethodData<QString(double, int)>(19, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Double, 16 }, { QMetaType::Int, 18 },
        }}),
        // Method 'getFramePathAt'
        QtMocHelpers::MethodData<QString(double)>(19, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::QString, {{
            { QMetaType::Double, 16 },
        }}),
        // Method 'requestFrame'
        QtMocHelpers::MethodData<void(double, int)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 16 }, { QMetaType::Int, 18 },
        }}),
        // Method 'requestFrame'
        QtMocHelpers::MethodData<void(double)>(20, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Double, 16 },
        }}),
        // Method 'getClipInfoAt'
        QtMocHelpers::MethodData<QVariantMap(double, int)>(21, 2, QMC::AccessPublic, 0x80000000 | 22, {{
            { QMetaType::Double, 16 }, { QMetaType::Int, 18 },
        }}),
        // Method 'getClipInfoAt'
        QtMocHelpers::MethodData<QVariantMap(double)>(21, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 22, {{
            { QMetaType::Double, 16 },
        }}),
        // Method 'getActiveClipPath'
        QtMocHelpers::MethodData<QString(double, int)>(23, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Double, 16 }, { QMetaType::Int, 18 },
        }}),
        // Method 'getActiveClipPath'
        QtMocHelpers::MethodData<QString(double)>(23, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::QString, {{
            { QMetaType::Double, 16 },
        }}),
        // Method 'addClip'
        QtMocHelpers::MethodData<bool(const QString &, int, double)>(24, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 25 }, { QMetaType::Int, 18 }, { QMetaType::Double, 26 },
        }}),
        // Method 'removeClip'
        QtMocHelpers::MethodData<bool(int)>(27, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 },
        }}),
        // Method 'moveClip'
        QtMocHelpers::MethodData<bool(int, int, double)>(28, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Int, 29 }, { QMetaType::Double, 30 },
        }}),
        // Method 'splitClipAt'
        QtMocHelpers::MethodData<bool(double, int)>(31, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Double, 16 }, { QMetaType::Int, 18 },
        }}),
        // Method 'splitClipAt'
        QtMocHelpers::MethodData<bool(double)>(31, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::Double, 16 },
        }}),
        // Method 'splitClip'
        QtMocHelpers::MethodData<bool(int, double)>(32, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Double, 33 },
        }}),
        // Method 'trimClip'
        QtMocHelpers::MethodData<bool(int, double, double)>(34, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Double, 35 }, { QMetaType::Double, 36 },
        }}),
        // Method 'setClipLeftTrim'
        QtMocHelpers::MethodData<bool(int, double, double)>(37, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Double, 30 }, { QMetaType::Double, 35 },
        }}),
        // Method 'applyEffect'
        QtMocHelpers::MethodData<bool(int, const QString &, double)>(38, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::QString, 39 }, { QMetaType::Double, 40 },
        }}),
        // Method 'setClipMuted'
        QtMocHelpers::MethodData<bool(int, bool)>(41, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 6 }, { QMetaType::Bool, 42 },
        }}),
        // Method 'getTrackEndTime'
        QtMocHelpers::MethodData<double(int) const>(43, 2, QMC::AccessPublic, QMetaType::Double, {{
            { QMetaType::Int, 18 },
        }}),
        // Method 'getClipsForTrack'
        QtMocHelpers::MethodData<QVariantList(int)>(44, 2, QMC::AccessPublic, 0x80000000 | 45, {{
            { QMetaType::Int, 18 },
        }}),
        // Method 'saveProject'
        QtMocHelpers::MethodData<bool(const QString &)>(46, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 25 },
        }}),
        // Method 'loadProject'
        QtMocHelpers::MethodData<bool(const QString &)>(47, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 25 },
        }}),
        // Method 'renderToFile'
        QtMocHelpers::MethodData<bool(const QString &)>(48, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 49 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'currentTime'
        QtMocHelpers::PropertyData<double>(50, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'totalDuration'
        QtMocHelpers::PropertyData<double>(51, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'clipCount'
        QtMocHelpers::PropertyData<int>(52, QMetaType::Int, QMC::DefaultPropertyFlags, 2),
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
        case 8: _t->frameReady((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 9: { QImage _r = _t->getCurrentFrameAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QImage*>(_a[0]) = std::move(_r); }  break;
        case 10: { QImage _r = _t->getCurrentFrameAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QImage*>(_a[0]) = std::move(_r); }  break;
        case 11: { QString _r = _t->getFramePathAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 12: { QString _r = _t->getFramePathAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 13: _t->requestFrame((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 14: _t->requestFrame((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 15: { QVariantMap _r = _t->getClipInfoAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 16: { QVariantMap _r = _t->getClipInfoAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 17: { QString _r = _t->getActiveClipPath((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 18: { QString _r = _t->getActiveClipPath((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 19: { bool _r = _t->addClip((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 20: { bool _r = _t->removeClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 21: { bool _r = _t->moveClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 22: { bool _r = _t->splitClipAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 23: { bool _r = _t->splitClipAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 24: { bool _r = _t->splitClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 25: { bool _r = _t->trimClip((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 26: { bool _r = _t->setClipLeftTrim((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 27: { bool _r = _t->applyEffect((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 28: { bool _r = _t->setClipMuted((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 29: { double _r = _t->getTrackEndTime((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<double*>(_a[0]) = std::move(_r); }  break;
        case 30: { QVariantList _r = _t->getClipsForTrack((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 31: { bool _r = _t->saveProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 32: { bool _r = _t->loadProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 33: { bool _r = _t->renderToFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
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
        if (QtMocHelpers::indexOfMethod<void (Timeline::*)(const QImage & , double )>(_a, &Timeline::frameReady, 8))
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
        if (_id < 34)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 34)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 34;
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
void Timeline::frameReady(const QImage & _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}
QT_WARNING_POP

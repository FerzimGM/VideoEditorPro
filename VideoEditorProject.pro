QT = core quick quickcontrols2 multimedia widgets concurrent

CONFIG += c++17

TARGET = VideoEditor

TEMPLATE = app

RESOURCES += qml.qrc

SOURCES += \
    audioplaybackengine.cpp \
    main.cpp \
    mediadecoder.cpp \
    mediaencoder.cpp \
    renderengine.cpp \
    timeline.cpp

HEADERS += \
    Effectimageprovider.h \
    FrameCache.h \
    TimelineClip.h \
    audioplaybackengine.h \
    decoderthread.h \
    mediadecoder.h \
    mediaencoder.h \
    renderengine.h \
    timeline.h

RC_ICONS = VideoEditorPro.ico

INCLUDEPATH += C:/FFmpegForQt/include

LIBS += -LC:/FFmpegForQt/lib \
        -lavcodec -lavformat -lavutil -lswscale -lswresample -lUser32

win32 {
    FFMPEG_BIN = C:/FFmpegForQt/bin
    OUT_DIR = $$OUT_PWD/debug

    QMAKE_POST_LINK += $$quote(cmd /c xcopy /Y /Q C:\FFmpegForQt\bin\*.dll $$OUT_DIR)
}

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

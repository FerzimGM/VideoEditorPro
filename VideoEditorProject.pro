QT = core quick quickcontrols2 multimedia widgets

CONFIG += c++17

TARGET = VideoEditor

TEMPLATE = app

RESOURCES += qml.qrc

SOURCES += \
    main.cpp \
    mediadecoder.cpp \
    mediaencoder.cpp \
    renderengine.cpp \
    timeline.cpp

INCLUDEPATH += C:/FFmpegForQt/include

LIBS += C:/FFmpegForQt/lib/avcodec.lib
LIBS += C:/FFmpegForQt/lib/avformat.lib
LIBS += C:/FFmpegForQt/lib/avutil.lib
LIBS += C:/FFmpegForQt/lib/swscale.lib
LIBS += C:/FFmpegForQt/lib/swresample.lib
LIBS += -lUser32

FFMPEG_BIN = C:/FFmpegForQt/bin
win32 {
    DESTDIR = $$OUT_PWD/debug
    QMAKE_POST_LINK += $$quote(xcopy /Y /D "$$FFMPEG_BIN\*.dll" "$$DESTDIR" $$escape_expand(\n\t))
}

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

HEADERS += \
    FrameCache.h \
    TimelineClip.h \
    decoderthread.h \
    mediadecoder.h \
    mediaencoder.h \
    renderengine.h \
    timeline.h

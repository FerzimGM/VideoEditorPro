QT = core quick quickcontrols2 multimedia widgets

CONFIG += c++17

TARGET = VideoEditor

TEMPLATE = app

RESOURCES += qml.qrc \
    qml.qrc \
    qml.qrc

SOURCES += \
    main.cpp \
    mediadecoder.cpp \
    mediaencoder.cpp \
    renderengine.cpp \
    timeline.cpp


INCLUDEPATH += C:/FFmpegForQt/include

LIBS += -LC:/FFmpegForQt/lib #pc

LIBS += C:/FFmpegForQt/lib/avcodec.lib #pc

LIBS += C:/FFmpegForQt/lib/avformat.lib #pc

LIBS += C:/FFmpegForQt/lib/avutil.lib #pc

LIBS += C:/FFmpegForQt/lib/swscale.lib #pc

LIBS += -lswscale

LIBS += -lUser32

FFMPEG_BIN = C:/FFmpegForQt/bin

win32 {
    DESTDIR = $$OUT_PWD/debug
    QMAKE_POST_LINK += $$quote(xcopy /Y /D "$$FFMPEG_BIN\*.dll" "$$DESTDIR" $$escape_expand(\n\t))
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

HEADERS += \
    TimelineClip.h \
    mediadecoder.h \
    mediaencoder.h \
    renderengine.h \
    timeline.h

TEMPLATE = app

QT += qml quick widgets multimedia opengl
CONFIG += c++11

HEADERS += \
    bufimage.h \
    mainwindowqml.h \
    mainwindow.h \
#    videooutput.h \
    videoprovider.h \
    glwidget.h \
    progressslider.h \
    accuratetimer.h \
#    audiooutput_qaudiooutput.h \
    smartmutex.h \
    avdecodercore.h \
    avplaycontrol.h \
    videodecoderbuffer.h \
    audioplayerbase.h \
    audioplayer_directsound.h \
    audiodecoderbuffer.h

SOURCES += main.cpp \
    bufimage.cpp \
    mainwindowqml.cpp \
    mainwindow.cpp \
    videoprovider.cpp \
    glwidget.cpp \
    progressslider.cpp \
    accuratetimer.cpp \
#    audiooutput_qaudiooutput.cpp \
    avdecodercore.cpp \
#    videooutput.cpp \
    avplaycontrol.cpp \
    videodecoderbuffer.cpp \
    audioplayerbase.cpp \
    audioplayer_directsound.cpp \
    audiodecoderbuffer.cpp

win32: {
HEADERS += \

SOURCES += \
}

#unix: {
#HEADERS += \
#    audiooutput_alsa.h \
#    audiooutput_sdl2.h \

#SOURCES += \
#    audiooutput_alsa.cpp \
#    audiooutput_sdl2.cpp \
#}

RESOURCES += qml.qrc \
    shader.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

win32: {
INCLUDEPATH += D:\workspace\dependency\c_c++\include \

LIBS += -LD:\workspace\dependency\c_c++\lib -LD:\workspace\dependency\c_c++\bin

LIBS += -ldsound -ldxguid -lVersion
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale -lswresample

unix: LIBS += -lasound -lSDL2

FORMS += \
    mainwindow.ui

DISTFILES += \
    shader.fsh \
    shader.vsh

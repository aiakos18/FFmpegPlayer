#include <QtCore>
#include <QtWidgets>

#include "mainwindowqml.h"
#include "mainwindow.h"

#undef main

//#define USE_QML

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef USE_QML
    MainWindowQml mw;
    mw.init();
    mw.play(file);
#else
    MainWindow mw;
    mw.setWindowTitle("ffmpeg player");
    mw.init();
    mw.show();
    mw.resize(800, 480);
#endif

    return app.exec();
}

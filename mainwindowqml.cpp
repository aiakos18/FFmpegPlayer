#include "mainwindowqml.h"
#include "bufimage.h"

MainWindowQml::MainWindowQml()
{

}

void MainWindowQml::init()
{
    m_avPlayer.init();
    connect(&m_avPlayer, SIGNAL(videoUpdated(QImage)), this, SIGNAL(imgProduced(QImage)));

    qmlRegisterType<BufImage>("ShincoShell",1,0,"BufImage");

    rootContext()->setContextProperty("mainwindow", this);

    VideoProvider *videoProvider = new VideoProvider(QQmlImageProviderBase::Image);
    videoProvider->setVideoProducer(&m_avPlayer);
    engine()->addImageProvider("videoprovider", videoProvider);

//    setFlags(Qt::Window|Qt::FramelessWindowHint);
    setSource(QString("qrc:/main.qml"));
    show();
    showMaximized();
    showNormal();
}

bool MainWindowQml::play(const QString &file)
{
    return m_avPlayer.load(file);
}

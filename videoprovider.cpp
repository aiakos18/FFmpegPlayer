#include "videoprovider.h"
#include "avplaycontrol.h"

VideoProvider::VideoProvider(ImageType type, Flags flags)
    : QQuickImageProvider(type, flags)
    , m_avPlayer(0)
{

}

VideoProvider::~VideoProvider()
{

}

QImage VideoProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
//    qDebug() << "requestImage:" << id;
//    QImage img = m_videoPlayer->getImage();
//    *size = img.size();
//    return img.scaled(requestedSize);
    return QImage();
}

void VideoProvider::setVideoProducer(AVPlayControl *player)
{
    m_avPlayer = player;
}

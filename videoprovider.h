#ifndef VIDEOPROVIDER_H
#define VIDEOPROVIDER_H

#include <QtCore>
#include <QtQuick>

class AVPlayControl;

class VideoProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    VideoProvider(ImageType type, Flags flags = Flags());
    ~VideoProvider();

    virtual QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize);

    void setVideoProducer(AVPlayControl *player);

private:
    AVPlayControl *m_avPlayer;
};

#endif // VIDEOPROVIDER_H

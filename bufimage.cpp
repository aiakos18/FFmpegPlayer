#include "bufimage.h"

#if(QT_VERSION>=0x050000)
BufImage::BufImage(QQuickItem *parent) :
    QQuickPaintedItem(parent)
#else
BufImage::BufImage(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
#endif
{
#if(QT_VERSION<0x050000)
    setFlag(QGraphicsItem::ItemHasNoContents, false);
#endif
    m_pixmap = NULL;
    m_status = Null;
    m_defaultSize = QSize(0,0);
    m_sourceSize = QSize(0,0);
    m_xRadius = 0;
    m_yRadius = 0;
    m_grayscale = false;
}

BufImage::~BufImage()
{
    if (m_pixmap != NULL) {
        delete m_pixmap;
    }
}

#if(QT_VERSION>=0x050000)
void BufImage::paint(QPainter *painter)
#else
void BufImage::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
#endif
{
    if(m_pixmap == NULL || m_pixmap->isNull())
    {
        return;
    }

    if(smooth())
    {
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
    }

    QPainterPath painterPath;

    if (m_xRadius || m_yRadius)
    {
        painterPath.addRoundedRect(QRectF(boundingRect().toRect()), m_xRadius, m_yRadius);
        painter->setClipPath(painterPath);
    }
    painter->drawPixmap (boundingRect().toRect(), *m_pixmap);
}

BufImage::State BufImage::status() const
{
    return m_status;
}

void BufImage::setStatus(BufImage::State status)
{
    if (status == m_status) {
        return;
    }
    m_status = status;
    emit statusChanged(status);
}

QImage BufImage::source() const
{
    return m_source;
}

void BufImage::setSource(const QImage &source)
{
//    if (source == m_source) {
//        return;
//    }
    m_source = source;
    reLoad();
    emit sourceChanged(source);
}

QSize BufImage::defaultSize() const
{
    return m_defaultSize;
}

void BufImage::setDefaultSize(QSize size)
{
    if (size == m_defaultSize) {
        return;
    }
    m_defaultSize = size;
    emit defaultSizeChanged(size);
}

QSize BufImage::sourceSize() const
{
    return m_sourceSize;
}

void BufImage::setSourceSize(QSize size)
{
    if (size == m_sourceSize) {
        return;
    }
    m_sourceSize = size;
    emit sourceSizeChanged(size);
    reLoad();
}

bool BufImage::grayscale() const
{
    return m_grayscale;
}

void BufImage::setGrayscale(bool arg)
{
    if (arg == m_grayscale) {
        return;
    }
    m_grayscale = arg;
    emit grayscaleChanged(arg);
    reLoad();
}

qreal BufImage::getXRadius()
{
    return m_xRadius;
}

void BufImage::setXRadius(qreal radius)
{
    if (radius == m_xRadius) {
        return;
    }
    m_xRadius = radius;
    emit xRadiusChanged(m_yRadius);
    reLoad();
}

qreal BufImage::getYRadius()
{
    return m_yRadius;
}

void BufImage::setYRadius(qreal radius)
{
    if (radius == m_yRadius) {
        return;
    }
    m_yRadius = radius;
    emit yRadiusChanged(m_yRadius);
    reLoad();
}

void BufImage::reLoad()
{
//    if (m_source.isEmpty()) {
//        if (m_pixmap != NULL) {
//            delete m_pixmap;
//            m_pixmap = NULL;
//        }
//        update();
//        return;
//    }

    setStatus(Loading);

    QImage &image = m_source;
//    image.loadFromData(m_source);

    setDefaultSize(image.size());

    QSize temp_size = m_sourceSize;

    if (temp_size == QSize(0,0)) {
        temp_size = m_defaultSize;
    }
    else if (temp_size.width() == 0) {
        temp_size.setWidth((double)temp_size.height()/m_defaultSize.height()*m_defaultSize.width());
    }
    else if(temp_size.height() == 0) {
        temp_size.setHeight((double)temp_size.width()/m_defaultSize.width()*m_defaultSize.height());
    }

    if(m_defaultSize != temp_size) {
        image = image.scaled(temp_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    //如果为黑白
    if (m_grayscale) {
        chromaticToGrayscale(image);//转换为黑白图
    }

    if (m_pixmap != NULL) {
        delete m_pixmap;
    }
    m_pixmap = new QPixmap(QPixmap::fromImage(image));

    setImplicitWidth(image.width());
    setImplicitHeight(image.height());

    setWidth(image.width());
    setHeight(image.height());

    update();
    setStatus(Ready);
}

void BufImage::chromaticToGrayscale(QImage &image)
{
    if (image.isNull() || image.isGrayscale ()) {
        return;
    }
    for (int i = 0; i < image.width(); ++i){
        for(int j = 0; j < image.height(); ++j){
            QRgb pixel = image.pixel(i, j);
            int a = qAlpha(pixel);
            pixel = qGray (pixel);
            pixel = qRgba(pixel, pixel, pixel, a);
            image.setPixel(i, j, pixel);
        }
    }
}

QString BufImage::imageFormatToString(const QByteArray &array)
{
    QByteArray str = array.toHex ();
    if(str.mid (2,6)=="504e47") {
        return "png";
    }
    if(str.mid (12,8)=="4a464946") {
        return "jpg";
    }
    if(str.left (6)=="474946") {
        return "gif";
    }
    if(str.left (4)=="424d") {
        return "bmp";
    }
    return "";
}

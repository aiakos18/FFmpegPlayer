#ifndef BUFIMAGE_H
#define BUFIMAGE_H

#include <QtCore>
#include <QtGui>

#if(QT_VERSION>=0x050000)
#include <QQuickPaintedItem>
#else
#include <QDeclarativeItem>
#endif

#if(QT_VERSION>=0x050000)
class BufImage : public QQuickPaintedItem
#else
class BufImage : public QDeclarativeItem
#endif
{
    Q_OBJECT
    Q_PROPERTY(QImage source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool grayscale READ grayscale WRITE setGrayscale NOTIFY grayscaleChanged)
    Q_PROPERTY(State status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)
    Q_PROPERTY(QSize defaultSize READ defaultSize NOTIFY defaultSizeChanged FINAL)
    Q_PROPERTY(qreal xRadius READ getXRadius WRITE setXRadius NOTIFY xRadiusChanged)
    Q_PROPERTY(qreal yRadius READ getYRadius WRITE setYRadius NOTIFY yRadiusChanged)
    Q_ENUMS(State)

public:
    enum State{
        Null,
        Loading,
        Ready,
        Error
    };

#if(QT_VERSION>=0x050000)
    explicit BufImage(QQuickItem *parent = 0);
#else
    explicit BufImage(QDeclarativeItem *parent = 0);
#endif
    ~BufImage();

#if(QT_VERSION>=0x050000)
    virtual void paint(QPainter * painter);
#else
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *new_style, QWidget *new_widget=0);
#endif

    State status() const;
    void setStatus(State status);

    QImage source() const;
    void setSource(const QImage &source);

    bool sourceIsValid() const;

    QSize defaultSize() const;
    void setDefaultSize(QSize size);

    QSize sourceSize() const;
    void setSourceSize(QSize size);

    bool grayscale() const;
    void setGrayscale(bool arg);

    qreal getXRadius();
    void setXRadius(qreal radius);

    qreal getYRadius();
    void setYRadius(qreal radius);

signals:
    void statusChanged(State arg);
    void sourceChanged(QImage arg);
    void defaultSizeChanged(QSize arg);
    void sourceSizeChanged(QSize arg);
    void xRadiusChanged(qreal xRadius);
    void yRadiusChanged(qreal yRadius);
    void grayscaleChanged(bool arg);

private:
    void reLoad();

    void chromaticToGrayscale(QImage &image);

    static QString imageFormatToString(const QByteArray& array);

private:
    QPixmap *m_pixmap;

    State   m_status;

    QImage  m_source;

    QSize   m_defaultSize;
    QSize   m_sourceSize;
    qreal   m_xRadius;
    qreal   m_yRadius;
    bool    m_grayscale;
};

#endif // BUFIMAGE_H

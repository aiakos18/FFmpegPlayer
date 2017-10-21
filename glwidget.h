#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QtCore>
#include <QtOpenGL>

#include "avdecoder.h"

//#undef QT_VERSION
//#define QT_VERSION 0x050201

class GLWidget
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
        : public QOpenGLWidget
#else
        : public QGLWidget
#endif
//        , public QOpenGLFunctions
{
    Q_OBJECT
public:
    GLWidget(QWidget *parent = 0);
    ~GLWidget();

public slots:
    void showImage(const QImage &img);

    void showVideoFrame(const SPAVFrame &frame);

    void clear();

    void increaseWidth();
    void decreaseWidth();
    void resetWidth();

    void increaseHeight();
    void decreaseHeight();
    void resetHeight();

    void increaseSize();
    void decreaseSize();
    void resetSize();

    void increaseXDeviation();
    void decreaseXDeviation();
    void resetXDeviation();

    void increaseYDeviation();
    void decreaseYDeviation();
    void resetYDeviation();

signals:
    void fullScreenChanged(bool fullScreen);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event);

    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);

    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    void setFullScreen(bool on);

    void initTexture();
    void initShader();
    void setTexType(int type);
    QRgb *extractPixelDataFromImage(const QImage &img);

private:
    QOpenGLFunctions *m_openGLFun;

    QImage m_img;
    GLuint m_tid;
    QRgb   *m_pixelBuffer;

    SPAVFrame m_frame;
    GLuint m_tex0, m_tex1, m_tex2; // Texture id
    uint8_t *m_grayData;
    GLuint m_texGrayU, m_texGrayV;
    GLuint m_texTypeLoc;
    int m_texType;

    double m_widthScale, m_heightScale;
    bool m_wPressed, m_hPressed, m_sPressed;
    double m_xDeviation, m_yDeviation;
};

#endif // GLWIDGET_H

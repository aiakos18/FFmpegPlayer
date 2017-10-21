#include "glwidget.h"

//#define DOUBLE_SCREEN

GLWidget::GLWidget(QWidget *parent)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    : QOpenGLWidget(parent)
#else
    : QGLWidget(parent)
#endif
    , m_openGLFun(0)
    , m_tid(0)
    , m_pixelBuffer(0)
    , m_grayData(0)
    , m_texGrayU(-1)
    , m_texGrayV(-1)
    , m_texTypeLoc(-1)
    , m_texType(0)
    , m_widthScale(1.0)
    , m_heightScale(1.0)
    , m_wPressed(false)
    , m_hPressed(false)
    , m_sPressed(false)
    , m_xDeviation(0.0)
    , m_yDeviation(0.0)
{
}

GLWidget::~GLWidget()
{
    if (m_pixelBuffer) {
        delete[] m_pixelBuffer;
    }
}

void GLWidget::showImage(const QImage &img)
{
    m_img = img;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    update();
#else
    updateGL();
#endif
}

void GLWidget::showVideoFrame(const SPAVFrame &frame)
{
    m_frame = frame;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    update();
#else
    updateGL();
#endif
}

void GLWidget::clear()
{
    m_img = QImage();
    m_frame.clear();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    update();
#else
    updateGL();
#endif
}

void GLWidget::increaseWidth()
{
    m_widthScale += 0.01;
}

void GLWidget::decreaseWidth()
{
    if (m_widthScale > 0.01) {
        m_widthScale -= 0.01;
    }
}

void GLWidget::resetWidth()
{
    m_widthScale = 1.0;
}

void GLWidget::increaseHeight()
{
    m_heightScale += 0.01;
}

void GLWidget::decreaseHeight()
{
    if (m_heightScale > 0.01) {
        m_heightScale -= 0.01;
    }
}

void GLWidget::resetHeight()
{
    m_heightScale = 1.0;
}

void GLWidget::increaseSize()
{
    if (m_widthScale > m_heightScale) {
        m_widthScale *= (m_heightScale+0.01)/m_heightScale;
        m_heightScale += 0.01;
    }
    else if (m_widthScale < m_heightScale) {
        m_heightScale *= (m_widthScale+0.01)/m_widthScale;
        m_widthScale += 0.01;
    }
    else {
        m_widthScale += 0.01;
        m_heightScale += 0.01;
    }
}

void GLWidget::decreaseSize()
{
    if (m_widthScale > m_heightScale) {
        if (m_heightScale > 0.01) {
            m_widthScale *= (m_heightScale-0.01)/m_heightScale;
            m_heightScale -= 0.01;
        }
    }
    else if (m_widthScale < m_heightScale) {
        if (m_widthScale > 0.01) {
            m_heightScale *= (m_widthScale-0.01)/m_widthScale;
            m_widthScale -= 0.01;
        }
    }
    else {
        if (m_widthScale > 0.01) {
            m_widthScale -= 0.01;
            m_heightScale -= 0.01;
        }
    }
}

void GLWidget::resetSize()
{
    m_widthScale = 1.0;
    m_heightScale = 1.0;
}

void GLWidget::increaseXDeviation()
{
    if (m_xDeviation >= 1.0 - 0.01) {
        return;
    }
    m_xDeviation += 0.01;
}

void GLWidget::decreaseXDeviation()
{
    if (m_xDeviation <= -1.0 + 0.01) {
        return;
    }
    m_xDeviation -= 0.01;
}

void GLWidget::resetXDeviation()
{
    m_xDeviation = 0.0;
}

void GLWidget::increaseYDeviation()
{
    if (m_yDeviation >= 1.0 - 0.01) {
        return;
    }
    m_yDeviation += 0.01;
}

void GLWidget::decreaseYDeviation()
{
    if (m_yDeviation <= -1.0 + 0.01) {
        return;
    }
    m_yDeviation -= 0.01;
}

void GLWidget::resetYDeviation()
{
    m_yDeviation = 0.0;
}

void GLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (isFullScreen()) {
            setFullScreen(false);
        }
        else {
            setFullScreen(true);
        }
    }
}

void GLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        setFullScreen(false);
    }
    else if (event->key() == Qt::Key_W) {
//        qDebug() << __PRETTY_FUNCTION__ << "W key pressed";
        m_wPressed = true;
    }
    else if (event->key() == Qt::Key_H) {
//        qDebug() << __PRETTY_FUNCTION__ << "H key pressed";
        m_hPressed = true;
    }
    else if (event->key() == Qt::Key_S) {
//        qDebug() << __PRETTY_FUNCTION__ << "H key pressed";
        m_sPressed = true;
    }
    else if (event->key() == Qt::Key_Left) {
        decreaseXDeviation();
    }
    else if (event->key() == Qt::Key_Right) {
        increaseXDeviation();
    }
    else if (event->key() == Qt::Key_Up) {
        increaseYDeviation();
    }
    else if (event->key() == Qt::Key_Down) {
        decreaseYDeviation();
    }
}

void GLWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W) {
//        qDebug() << __PRETTY_FUNCTION__ << "W key released";
        m_wPressed = false;
    }
    else if (event->key() == Qt::Key_H) {
//        qDebug() << __PRETTY_FUNCTION__ << "H key released";
        m_hPressed = false;
    }
    else if (event->key() == Qt::Key_S) {
//        qDebug() << __PRETTY_FUNCTION__ << "H key released";
        m_sPressed = false;
    }
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0) {
//        qDebug() << __PRETTY_FUNCTION__ << "delta > 0";
        if (m_wPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "++++WWWW";
            increaseWidth();
        }
        if (m_hPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "++++HHHH";
            increaseHeight();
        }
        if (m_sPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "++++SSSS";
            increaseSize();
        }
    }
    else if (event->delta() < 0) {
//        qDebug() << __PRETTY_FUNCTION__ << "delta < 0";
        if (m_wPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "----WWWW";
            decreaseWidth();
        }
        if (m_hPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "----HHHH";
            decreaseHeight();
        }
        if (m_sPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "----SSSS";
            decreaseSize();
        }
    }
}

void GLWidget::initializeGL()
{
    m_openGLFun = this->context()->functions();

    m_openGLFun->initializeOpenGLFunctions();

//    initTexture();

    initShader();
}

void GLWidget::resizeGL(int w, int h)
{
//    m_openGLFun->glViewport(0, 0, w, h);
}

void GLWidget::paintGL()
{
    m_openGLFun->glClearColor(0.0,0.0,0.0,0.0);
    m_openGLFun->glClear(GL_COLOR_BUFFER_BIT);

    if (!m_img.isNull()) {
//        QRgb *pd = extractPixelDataFromImage(m_img);
//        if (!pd) {
//            return;
//        }
//        m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_img.width(), m_img.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, pd);
//        glBegin(GL_QUADS);
//            glTexCoord2f(1, 1);
//            glVertex2f(1, 1);

//            glTexCoord2f(0, 1);
//            glVertex2f(-1, 1);

//            glTexCoord2f(0, 0);
//            glVertex2f(-1, -1);

//            glTexCoord2f(1, 0);
//            glVertex2f(1, -1);
//        glEnd();

        m_img = QImage();
    }
    else if (!m_frame.isNull()) {

        double w = this->width(), h = this->height();
        double fw = m_frame->width, fh = m_frame->height;
        if (fw * h > fh * w) {
            fh = fh * w / fw;
            fw = w;
        }
        else {
            fw = fw * h / fh;
            fh = h;
        }
        double scaledWidth = fw * m_widthScale, scaledHeight = fh * m_heightScale;
        m_openGLFun->glViewport((w - scaledWidth)/2 + w*m_xDeviation, (h - scaledHeight)/2 + h*m_yDeviation, scaledWidth, scaledHeight);

//        qDebug() << m_frame->width << m_frame->height
//                 << m_frame->linesize[0] << m_frame->linesize[1] << m_frame->linesize[2];
//        if (m_frame->linesize[0] < 0) {
//            m_frame->linesize[0] = -m_frame->linesize[0];
//        }
        static char *s_data = new char[4096*2160*4];

        if (m_frame->format == AV_PIX_FMT_YUV420P || m_frame->format == AV_PIX_FMT_YUVJ420P) {
            setTexType(0);

            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);
            int yWidth = m_frame->width, yHeight = m_frame->height;
            int expectYWidth = yWidth % 4 ? (yWidth / 4 * 4 + 4) : yWidth;
            if (m_frame->linesize[0] > expectYWidth) {
                for (int i = 0; i < yHeight; ++i) {
                    memcpy(s_data + i * expectYWidth, m_frame->data[0] + i * m_frame->linesize[0], expectYWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectYWidth, yHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[0], yHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }

            m_openGLFun->glActiveTexture(GL_TEXTURE1);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex1);
            int uWidth = m_frame->width / 2, uHeight = m_frame->height / 2;
            int expectUWidth = uWidth % 4 ? (uWidth / 4 * 4 + 4) : uWidth;
            if (m_frame->linesize[1] > expectUWidth) {
                for (int i = 0; i < uHeight; ++i) {
                    memcpy(s_data + i * expectUWidth, m_frame->data[1] + i * m_frame->linesize[1], expectUWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectUWidth, uHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[1], uHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[1]);
            }

            m_openGLFun->glActiveTexture(GL_TEXTURE2);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex2);
            int vWidth = m_frame->width / 2, vHeight = m_frame->height / 2;
            int expectVWidth = vWidth % 4 ? (vWidth / 4 * 4 + 4) : vWidth;
            if (m_frame->linesize[2] > expectVWidth) {
                for (int i = 0; i < vHeight; ++i) {
                    memcpy(s_data + i * expectVWidth, m_frame->data[2] + i * m_frame->linesize[2], expectVWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectVWidth, vHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[2], vHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[2]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_YUV422P || m_frame->format == AV_PIX_FMT_YUVJ422P) {
            setTexType(0);

            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);
            int yWidth = m_frame->width, yHeight = m_frame->height;
            int expectYWidth = yWidth % 4 ? (yWidth / 4 * 4 + 4) : yWidth;
            if (m_frame->linesize[0] > expectYWidth) {
                for (int i = 0; i < yHeight; ++i) {
                    memcpy(s_data + i * expectYWidth, m_frame->data[0] + i * m_frame->linesize[0], expectYWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectYWidth, yHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[0], yHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }

            m_openGLFun->glActiveTexture(GL_TEXTURE1);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex1);
            int uWidth = m_frame->width / 2, uHeight = m_frame->height;
            int expectUWidth = uWidth % 4 ? (uWidth / 4 * 4 + 4) : uWidth;
            if (m_frame->linesize[1] > expectUWidth) {
                for (int i = 0; i < uHeight; ++i) {
                    memcpy(s_data + i * expectUWidth, m_frame->data[1] + i * m_frame->linesize[1], expectUWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectUWidth, uHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[1], uHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[1]);
            }

            m_openGLFun->glActiveTexture(GL_TEXTURE2);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex2);
            int vWidth = m_frame->width / 2, vHeight = m_frame->height;
            int expectVWidth = vWidth % 4 ? (vWidth / 4 * 4 + 4) : vWidth;
            if (m_frame->linesize[2] > expectVWidth) {
                for (int i = 0; i < vHeight; ++i) {
                    memcpy(s_data + i * expectVWidth, m_frame->data[2] + i * m_frame->linesize[2], expectVWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectVWidth, vHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[2], vHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[2]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_YUV444P || m_frame->format == AV_PIX_FMT_YUVJ444P) {
            setTexType(0);

            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);
            int yWidth = m_frame->width, yHeight = m_frame->height;
            int expectYWidth = yWidth % 4 ? (yWidth / 4 * 4 + 4) : yWidth;
            if (m_frame->linesize[0] > expectYWidth) {
                for (int i = 0; i < yHeight; ++i) {
                    memcpy(s_data + i * expectYWidth, m_frame->data[0] + i * m_frame->linesize[0], expectYWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectYWidth, yHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[0], yHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }

            m_openGLFun->glActiveTexture(GL_TEXTURE1);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex1);
            int uWidth = m_frame->width, uHeight = m_frame->height;
            int expectUWidth = uWidth % 4 ? (uWidth / 4 * 4 + 4) : uWidth;
            if (m_frame->linesize[1] > expectUWidth) {
                for (int i = 0; i < uHeight; ++i) {
                    memcpy(s_data + i * expectUWidth, m_frame->data[1] + i * m_frame->linesize[1], expectUWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectUWidth, uHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[1], uHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[1]);
            }

            m_openGLFun->glActiveTexture(GL_TEXTURE2);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex2);
            int vWidth = m_frame->width, vHeight = m_frame->height;
            int expectVWidth = vWidth % 4 ? (vWidth / 4 * 4 + 4) : vWidth;
            if (m_frame->linesize[2] > expectVWidth) {
                for (int i = 0; i < vHeight; ++i) {
                    memcpy(s_data + i * expectVWidth, m_frame->data[2] + i * m_frame->linesize[2], expectVWidth);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             expectVWidth, vHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             m_frame->linesize[2], vHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame->data[2]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_RGBA) {
            setTexType(1);
            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);

            int dataPixelBytes = 4, outPixelBytes = 4;
            int dataWidth = m_frame->linesize[0] / dataPixelBytes, height = m_frame->height;
            int expectWidth = m_frame->width % 4 ? (m_frame->width / 4 * 4 + 4) : m_frame->width;
            if (expectWidth < dataWidth) {
                for (int i = 0; i < height; ++i) {
                    memcpy(s_data + i * expectWidth * outPixelBytes,
                           m_frame->data[0] + i * dataWidth * dataPixelBytes,
                           expectWidth * outPixelBytes);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, expectWidth, height,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dataWidth, height,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_BGRA) {
            setTexType(1);
            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);

            int dataPixelBytes = 4, outPixelBytes = 4;
            int dataWidth = m_frame->linesize[0] / dataPixelBytes, height = m_frame->height;
            int expectWidth = m_frame->width % 4 ? (m_frame->width / 4 * 4 + 4) : m_frame->width;
            if (expectWidth < dataWidth) {
                for (int i = 0; i < height; ++i) {
                    memcpy(s_data + i * expectWidth * outPixelBytes,
                           m_frame->data[0] + i * dataWidth * dataPixelBytes,
                           expectWidth * outPixelBytes);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, expectWidth, height,
                             0, GL_BGRA, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dataWidth, height,
                             0, GL_BGRA, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_RGB24) {
            setTexType(1);
            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);

            int dataPixelBytes = 3, outPixelBytes = 3;
            int dataWidth = m_frame->linesize[0] / dataPixelBytes, height = m_frame->height;
            int expectWidth = m_frame->width % 4 ? (m_frame->width / 4 * 4 + 4) : m_frame->width;
            if (expectWidth < dataWidth) {
                for (int i = 0; i < height; ++i) {
                    memcpy(s_data + i * expectWidth * outPixelBytes,
                           m_frame->data[0] + i * dataWidth * dataPixelBytes,
                           expectWidth * outPixelBytes);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, expectWidth, height,
                             0, GL_RGB, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dataWidth, height,
                             0, GL_RGB, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_BGR24) {
            setTexType(1);
            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);

            int dataPixelBytes = 3, outPixelBytes = 3;
            int dataWidth = m_frame->linesize[0] / dataPixelBytes, height = m_frame->height;
            int expectWidth = m_frame->width % 4 ? (m_frame->width / 4 * 4 + 4) : m_frame->width;
            if (expectWidth < dataWidth) {
                for (int i = 0; i < height; ++i) {
                    memcpy(s_data + i * expectWidth * outPixelBytes,
                           m_frame->data[0] + i * dataWidth * dataPixelBytes,
                           expectWidth * outPixelBytes);
                }
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, expectWidth, height,
                             0, GL_BGR, GL_UNSIGNED_BYTE, s_data);
            }
            else {
                m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dataWidth, height,
                             0, GL_BGR, GL_UNSIGNED_BYTE, m_frame->data[0]);
            }
        }
        else if (m_frame->format == AV_PIX_FMT_PAL8) {
            setTexType(1);
            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);

            int dataPixelBytes = 1, outPixelBytes = 4;
            int dataWidth = m_frame->linesize[0] / dataPixelBytes, height = m_frame->height;
            int expectWidth = m_frame->width % 4 ? (m_frame->width / 4 * 4 + 4) : m_frame->width;
            for (int i = 0; i < height; ++i) {
                for (int j = 0; j < expectWidth; ++j) {
                    unsigned char *pcolor = m_frame->data[0] + (i * dataWidth + j) * dataPixelBytes;
                    memcpy(s_data + (i * expectWidth + j) * outPixelBytes, m_frame->data[1] + pcolor[0] * 4, 4);
                }
            }
            m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, expectWidth, height,
                         0, GL_BGRA, GL_UNSIGNED_BYTE, s_data);
        }
        else if (m_frame->format == AV_PIX_FMT_GBRP) {
            setTexType(1);
            m_openGLFun->glActiveTexture(GL_TEXTURE0);
            m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);

            int outPixelBytes = 3;
            unsigned char *dataR = m_frame->data[2], *dataG = m_frame->data[0], *dataB = m_frame->data[1];
            int dataRWidth = m_frame->linesize[2], dataGWidth = m_frame->linesize[0], dataBWidth = m_frame->linesize[1];
            int expectWidth = m_frame->width % 4 ? (m_frame->width / 4 * 4 + 4) : m_frame->width;
            int height = m_frame->height;
            for (int i = 0; i < height; ++i) {
                for (int j = 0; j < expectWidth; ++j) {
                    s_data[(i * expectWidth + j) * outPixelBytes + 0] = dataR[i * dataRWidth + j];
                    s_data[(i * expectWidth + j) * outPixelBytes + 1] = dataG[i * dataGWidth + j];
                    s_data[(i * expectWidth + j) * outPixelBytes + 2] = dataB[i * dataBWidth + j];
                }
            }

            m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, expectWidth, height,
                         0, GL_RGB, GL_UNSIGNED_BYTE, s_data);
        }

        // Draw
#ifdef DOUBLE_SCREEN
        m_openGLFun->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

//        m_openGLFun->glActiveTexture(GL_TEXTURE1);
//        m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_texGrayU);

//        m_openGLFun->glActiveTexture(GL_TEXTURE2);
//        m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_texGrayU);

        m_openGLFun->glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
#else
        m_openGLFun->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#endif
    }

    m_openGLFun->glFlush();
}

void GLWidget::setFullScreen(bool on)
{
    static int width = this->width(), height = this->height();
    if (on) {
        if (isFullScreen()) {
            return;
        }
        width = this->width();
        height = this->height();
//        qDebug() << __PRETTY_FUNCTION__ << width << height;

        setWindowFlags(Qt::Window);
        showFullScreen();
        emit fullScreenChanged(true);
    }
    else {
        if (!isFullScreen()) {
            return;
        }
        setWindowFlags(Qt::SubWindow);
        showNormal();

//        qDebug() << __PRETTY_FUNCTION__ << width << height;
        resize(width, height);

        emit fullScreenChanged(false);
    }
}

void GLWidget::initTexture()
{
    GLint max;
    m_openGLFun->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
    qDebug() << "max tex:" << max;

    m_openGLFun->glEnable(GL_TEXTURE_2D);

//    m_openGLFun->glGenTextures(1, &m_tid);
//    qDebug("texture id: %d", m_tid);

//    GLint last_texture_ID;
//    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture_ID);
//    qDebug() << "last_texture_ID:" << last_texture_ID;

//    m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tid);
    m_openGLFun->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    m_openGLFun->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    m_openGLFun->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_openGLFun->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//    m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, pd);

//    m_openGLFun->glBindTexture(GL_TEXTURE_2D, last_texture_ID);
}

void GLWidget::initShader()
{
    GLint v = m_openGLFun->glCreateShader(GL_VERTEX_SHADER);
    QFile vshFile(":/vertex.shader");
    if (!vshFile.open(QFile::ReadOnly)) {
        qDebug() << "cannot open vsh shader";
        return;
    }
    QByteArray vshCode = vshFile.readAll();
    const char* data = vshCode.data();
    m_openGLFun->glShaderSource(v, 1, &data, NULL);
    m_openGLFun->glCompileShader(v);
    GLint vertCompiled;
    m_openGLFun->glGetShaderiv(v, GL_COMPILE_STATUS, &vertCompiled);
    if (vertCompiled != GL_TRUE) {
        QByteArray info;
        info.resize(1024);
        m_openGLFun->glGetShaderInfoLog(v, 1024, 0, info.data());
        qDebug("failed to compile v shader, status: %d, msg: %s", vertCompiled, info.data());
        return;
    }

    GLint f = m_openGLFun->glCreateShader(GL_FRAGMENT_SHADER);
    QFile fshFile(":/fragment.shader");
    if (!fshFile.open(QFile::ReadOnly)) {
        qDebug() << "cannot open fsh shader";
        return;
    }
    QByteArray fshCode = fshFile.readAll();
    data = fshCode.data();
    m_openGLFun->glShaderSource(f, 1, &data, NULL);
    m_openGLFun->glCompileShader(f);
    m_openGLFun->glGetShaderiv(f, GL_COMPILE_STATUS, &vertCompiled);
    if (vertCompiled != GL_TRUE) {
        QByteArray info;
        info.resize(1024);
        m_openGLFun->glGetShaderInfoLog(f, 1024, 0, info.data());
        qDebug("failed to compile f shader, status: %d, msg: %s", vertCompiled, info.data());
        return;
    }

    GLuint p = m_openGLFun->glCreateProgram();
    m_openGLFun->glAttachShader(p, v);
    m_openGLFun->glAttachShader(p, f);

    m_openGLFun->glLinkProgram(p);

    m_openGLFun->glDeleteShader(v);
    m_openGLFun->glDeleteShader(f);

    GLint linked;
    m_openGLFun->glGetProgramiv(p, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        QByteArray info;
        info.resize(1024);
        m_openGLFun->glGetProgramInfoLog(p, 1024, 0, info.data());
        qDebug("failed to link program, status: %d, msg: %s", linked, info.data());
        return;
    }

    m_openGLFun->glValidateProgram(p);
    GLint valid;
    m_openGLFun->glGetProgramiv(p, GL_VALIDATE_STATUS, &valid);
    if (valid != GL_TRUE) {
        QByteArray info;
        info.resize(1024);
        m_openGLFun->glGetProgramInfoLog(p, 1024, 0, info.data());
        qDebug("failed to validate program, status: %d, msg: %s", valid, info.data());
        return;
    }

    m_openGLFun->glUseProgram(p);


    static const GLfloat vertexVertices[] = {
    #ifdef DOUBLE_SCREEN
        -1, 1,
        0, 1,
        -1, 0,
        0, 0,

        0, 1,
        1, 1,
        0, 0,
        1, 0
    #else
        -1, 1,
        1, 1,
        -1, -1,
        1, -1
    #endif
    };
    GLint vertexInAttrLocation = m_openGLFun->glGetAttribLocation(p, "vertexIn");
//    qDebug("attribute [vertexIn] location: %d", vertexInAttrLocation);
    m_openGLFun->glVertexAttribPointer(vertexInAttrLocation, 2, GL_FLOAT, 0, 0, vertexVertices);
    m_openGLFun->glEnableVertexAttribArray(vertexInAttrLocation);

    static const GLfloat textureYVertices[] = {
    #ifdef DOUBLE_SCREEN
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,

        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    #else
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    #endif
    };
    GLint textureInYAttrLocation = m_openGLFun->glGetAttribLocation(p, "textureIn0");
//    qDebug("attribute [textureIn0] location: %d", textureInYAttrLocation);
    m_openGLFun->glVertexAttribPointer(textureInYAttrLocation, 2, GL_FLOAT, 0, 0, textureYVertices);
    m_openGLFun->glEnableVertexAttribArray(textureInYAttrLocation);

    static const GLfloat textureUVertices[] = {
    #ifdef DOUBLE_SCREEN
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,

        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    #else
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    #endif
    };
    GLint textureInUAttrLocation = m_openGLFun->glGetAttribLocation(p, "textureIn1");
//    qDebug("attribute [textureIn1] location: %d", textureInUAttrLocation);
    m_openGLFun->glVertexAttribPointer(textureInUAttrLocation, 2, GL_FLOAT, 0, 0, textureUVertices);
    m_openGLFun->glEnableVertexAttribArray(textureInUAttrLocation);

    static const GLfloat textureVVertices[] = {
    #ifdef DOUBLE_SCREEN
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,

        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    #else
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    #endif
    };
    GLint textureInVAttrLocation = m_openGLFun->glGetAttribLocation(p, "textureIn2");
//    qDebug("attribute [textureIn2] location: %d", textureInVAttrLocation);
    m_openGLFun->glVertexAttribPointer(textureInVAttrLocation, 2, GL_FLOAT, 0, 0, textureVVertices);
    m_openGLFun->glEnableVertexAttribArray(textureInVAttrLocation);


    //Init Texture
    m_openGLFun->glGenTextures(1, &m_tex0);
    m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex0);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_openGLFun->glGenTextures(1, &m_tex1);
    m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex1);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_openGLFun->glGenTextures(1, &m_tex2);
    m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_tex2);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    int size = 1920*1080;
    m_grayData = new uint8_t[size];
    for (int i = 0; i < size; ++i) {
        m_grayData[i] = 0x80;
    }

    m_openGLFun->glGenTextures(1, &m_texGrayU);
    m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_texGrayU);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_grayData);

    m_openGLFun->glGenTextures(1, &m_texGrayV);
    m_openGLFun->glBindTexture(GL_TEXTURE_2D, m_texGrayV);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_openGLFun->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_grayData);


    // set texture level
    GLuint texyLocation = m_openGLFun->glGetUniformLocation(p, "tex0");
    GLuint texuLocation = m_openGLFun->glGetUniformLocation(p, "tex1");
    GLuint texvLocation = m_openGLFun->glGetUniformLocation(p, "tex2");
//    qDebug("uniform [tex0] location: %d, uniform [tex1] location: %d, uniform [tex2] location: %d",
//           texyLocation, texuLocation, texvLocation);
    m_openGLFun->glUniform1i(texyLocation, 0);
    m_openGLFun->glUniform1i(texuLocation, 1);
    m_openGLFun->glUniform1i(texvLocation, 2);

    m_texTypeLoc = m_openGLFun->glGetUniformLocation(p, "tex_type");
    //    qDebug("uniform [tex_type] location: %d", m_texTypeLoc);
}

void GLWidget::setTexType(int type)
{
    if (type == m_texType) {
        return;
    }
    if (m_texTypeLoc == -1) {
        return;
    }
    m_openGLFun->glUniform1i(m_texTypeLoc, type);
    m_texType = type;
}

QRgb *GLWidget::extractPixelDataFromImage(const QImage &img)
{
    if (img.width() <= 0 || img.height() <= 0) {
        return 0;
    }

    if (!m_pixelBuffer) {
        m_pixelBuffer = new QRgb[4096*2160];
    }

    if (img.colorCount() > 0) {
        for (int i = 0; i < img.height(); ++i) {
            uchar *sl = (uchar*)img.constScanLine(img.height() - 1 - i);
            for (int j = 0; j < img.width(); j++) {
                if (img.format() == QImage::Format_Mono) {
                    m_pixelBuffer[i * img.width() + j] = img.color(sl[j/8] & (0x80 >> (j%8)) ? 1 : 0);
                }
                else if (img.format() == QImage::Format_MonoLSB) {
                    m_pixelBuffer[i * img.width() + j] = img.color(sl[j/8] & (0x01 << (j%8)) ? 1 : 0);
                }
                else {
                    m_pixelBuffer[i * img.width() + j] = img.color(sl[j]);
                }
            }
        }
    }
    else {
        for (int i = 0; i < img.height(); ++i) {
            QRgb *sl = (QRgb*)img.constScanLine(img.height() - 1 - i);
            for (int j = 0; j < img.width(); j++) {
                m_pixelBuffer[i * img.width() + j] = sl[j];
            }
        }
    }
    return m_pixelBuffer;
}

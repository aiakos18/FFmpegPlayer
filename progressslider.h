#ifndef PROGRESSSLIDER_H
#define PROGRESSSLIDER_H

#include <QtCore>
#include <QtWidgets>

class ProgressSlider : public QSlider
{
    Q_OBJECT
public:
    ProgressSlider(QWidget *parent);

    bool pressed();

signals:
    void pressedChanged(bool pressed);

protected slots:

protected:
    virtual void mousePressEvent(QMouseEvent *ev);
    virtual void mouseReleaseEvent(QMouseEvent *ev);
    virtual void mouseMoveEvent(QMouseEvent *ev);

    void setPressed(bool pressed);

private:
    bool m_pressed;
};

#endif // PROGRESSSLIDER_H

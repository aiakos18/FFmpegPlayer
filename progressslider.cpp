#include "progressslider.h"

ProgressSlider::ProgressSlider(QWidget *parent)
    : QSlider(parent)
    , m_pressed(false)
{
}

bool ProgressSlider::pressed()
{
    return m_pressed;
}

void ProgressSlider::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        setPressed(true);
        this->setValue(maximum() * ev->x() / width());
    }
}

void ProgressSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        setPressed(false);
    }
}

void ProgressSlider::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_pressed) {
        this->setValue(maximum() * ev->x() / width());
    }
}

void ProgressSlider::setPressed(bool pressed)
{
    if (pressed == m_pressed) {
        return;
    }
    m_pressed = pressed;
    emit pressedChanged(pressed);
}

#ifndef SMARTMUTEX_H
#define SMARTMUTEX_H

#include <QtCore>

class SmartMutex
{
public:
    SmartMutex(QMutex *mtx) :m_mtx(mtx) {
        if (m_mtx) {
            m_mtx->lock();
        }
    }
    ~SmartMutex() {
        if (m_mtx) {
            m_mtx->unlock();
        }
    }

private:
    QMutex *m_mtx;
};

#endif // SMARTMUTEX_H

#include "accuratetimer.h"

AccurateTimer::AccurateTimer(QObject *parent)
    : QObject(parent)
    , m_interval(1)
    , m_allElapsed(0)
    , m_allInterval(0)
    , m_adjust(1.0)
{
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimerOut()));
}

void AccurateTimer::start()
{
    int i = m_interval;
    if (m_adjust > 0.3 && m_adjust < 3) {
        i *= m_adjust;
    }
    m_timer.start(i);
    m_time.start();
}

void AccurateTimer::start(int interval)
{
    int i = interval;
    if (m_adjust > 0.3 && m_adjust < 3) {
        i *= m_adjust;
    }
    m_interval = interval;
    m_timer.start(i);
    m_time.start();
}

void AccurateTimer::stop()
{
    m_timer.stop();
}

bool AccurateTimer::isActive()
{
    return m_timer.isActive();
}

void AccurateTimer::setInterval(int interval)
{
    m_interval = interval;
}

void AccurateTimer::onTimerOut()
{
    int elapsed = m_time.elapsed();
    double r = 1.0 * m_interval / elapsed;
    if (m_interval > 0 && elapsed > 0 && r > 0.3 && r < 3) {
        m_allInterval += m_interval;
        m_allElapsed += elapsed;
        m_intervalList.push_back((uint)m_interval);
        m_elapsedList.push_back((uint)elapsed);
        if (m_intervalList.count() > 100) {
            m_allInterval -= m_intervalList.front();
            m_intervalList.pop_front();
            m_allElapsed -= m_elapsedList.front();
            m_elapsedList.pop_front();
        }
        m_adjust = 1.0 * m_allInterval / m_allElapsed;
//        qDebug() << m_intervalList.count() << m_allInterval << m_allElapsed << m_adjust;
    }

    emit timeout();
}

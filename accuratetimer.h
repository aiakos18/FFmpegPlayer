#ifndef ACCURATETIMER_H
#define ACCURATETIMER_H

#include <QtCore>

class AccurateTimer : public QObject
{
    Q_OBJECT
public:
    AccurateTimer(QObject *parent = 0);

    void start();

    void start(int interval);

    void stop();

    bool isActive();

    int interval();

    void setInterval(int interval);

signals:
    void timeout();

private slots:
    void onTimerOut();

private:
    QTimer m_timer;
    int m_interval;
    QTime m_time;
    QList<uint> m_elapsedList, m_intervalList;
    uint m_allElapsed;
    uint m_allInterval;
    double m_adjust;
};

#endif // ACCURATETIMER_H

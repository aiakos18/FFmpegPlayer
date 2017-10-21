#ifndef MAINWINDOWQML_H
#define MAINWINDOWQML_H

#include <QtCore>
#include <QtQuick>

#include "avplaycontrol.h"
#include "videoprovider.h"

class MainWindowQml : public QQuickView
{
    Q_OBJECT
public:
    MainWindowQml();

    void init();

    bool play(const QString &file);

signals:
    void imgProduced(QImage img);
    void videoUpdated();

protected slots:

private:
    AVPlayControl m_avPlayer;
};

#endif // MAINWINDOWQML_H

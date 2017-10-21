#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore>
#include <QtWidgets>

#include "avplaycontrol.h"
#include "glwidget.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void init();

    bool play(const QString &file);

signals:

protected slots:
    void onActionSelectAudioStreamTriggered();

    void onHorizontalSliderPressedChanged(bool pressed);
    void onHorizontalSliderValueChanged(int value);

    void onPlayerFileChanged(const QString &file);
    void onPlayerplaybackStateChanged(bool isPlaying);
    void onPlayerPositionChanged(double pos);

    void onVideoUpdated(QImage img);
    void onVideoFrameUpdated(const SPAVFrame &frame);

protected:
    void resizeEvent(QResizeEvent *e);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);

    void move2DesktopCenter();
    QSize getAdjustSize(int maxw, int maxh, int minw, int minh, int w, int h);
    void resizeOpenglWidgetSize(int w, int h);

    QAction *getAction(const QString &text);
    QMenu *getMenu(QMenu *menu, const QString &text);

private slots:
    void on_actionOpenFile_triggered();

    void on_playPauseBtn_clicked();

    void on_stopBtn_clicked();

    // popup menu
    void on_actionResetVideoWidthHeight();

private:
    Ui::MainWindow *ui;
    AVPlayControl m_player;
    QMenu *m_menu;
    bool m_wPressed, m_hPressed, m_sPressed;
};

#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_menu(new QMenu(this))
    , m_wPressed(false)
    , m_hPressed(false)
    , m_sPressed(false)
{
    ui->setupUi(this);

    ui->horizontalSlider->setOrientation(Qt::Horizontal);
    ui->horizontalSlider->setMaximum(1000);
    setAcceptDrops(true);

    m_menu->addAction("打开文件", this, SLOT(on_actionOpenFile_triggered()));
    m_menu->addSeparator();
    m_menu->addMenu("选择音轨");
    m_menu->addSeparator();
    QMenu *menu = m_menu->addMenu("重置显示");
    menu->addAction("重置大小", ui->openGLWidget, SLOT(resetSize()));
    menu->addAction("重置宽度", ui->openGLWidget, SLOT(resetWidth()));
    menu->addAction("重置高度", ui->openGLWidget, SLOT(resetHeight()));
    menu->addAction("重置左右偏移", ui->openGLWidget, SLOT(resetXDeviation()));
    menu->addAction("重置上下偏移", ui->openGLWidget, SLOT(resetYDeviation()));
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_menu;
}

void MainWindow::init()
{
    connect(ui->horizontalSlider, SIGNAL(pressedChanged(bool)),
            this, SLOT(onHorizontalSliderPressedChanged(bool)));
    connect(ui->horizontalSlider, SIGNAL(valueChanged(int)),
            this, SLOT(onHorizontalSliderValueChanged(int)));

    m_player.init();
    connect(&m_player, SIGNAL(fileChanged(QString)),
            this, SLOT(onPlayerFileChanged(QString)));
    connect(&m_player, SIGNAL(playbackStateChanged(bool)),
            this, SLOT(onPlayerplaybackStateChanged(bool)));
    connect(&m_player, SIGNAL(positionChanged(double)),
            this, SLOT(onPlayerPositionChanged(double)));
    connect(&m_player, SIGNAL(videoFrameUpdated(SPAVFrame)),
            this, SLOT(onVideoFrameUpdated(SPAVFrame)));
}

bool MainWindow::play(const QString &file)
{
    if (!QFile::exists(file)) {
        return false;
    }
    if (file == m_player.getFile()) {
        return true;
    }
    if (!m_player.load(file)) {
        return false;
    }

    ui->horizontalSlider->setMaximum(m_player.getDuration());

    if (m_player.isVideoAvailable()) {
        resizeOpenglWidgetSize(m_player.getVideoWidth(), m_player.getVideoHeight());
    }
    else {
        if (m_player.hasCover()) {
            SPAVFrame cover;
            if (m_player.getCover(0, cover)) {
                ui->openGLWidget->showVideoFrame(cover);
                resizeOpenglWidgetSize(cover->width, cover->height);
            }
        }
    }

    m_player.play();

    if (m_player.hasAudioStream()) {
        QMenu *m = getMenu(m_menu, "选择音轨");
        m->clear();
        for (int i = 0; i < m_player.getAudioStreamCount(); ++i) {
            QString title = m_player.getAudioStreamTitle(i);
            if (!title.isEmpty()) {
                m->addAction(title, this, SLOT(onActionSelectAudioStreamTriggered()));
            }
        }
    }

    return true;
}

void MainWindow::onActionSelectAudioStreamTriggered()
{
    QAction *action = dynamic_cast<QAction*>(sender());
    if (action == 0) {
        return;
    }
    m_player.changeAudioStream(action->text());
}

void MainWindow::onHorizontalSliderPressedChanged(bool pressed)
{
    if (!pressed) {
        if (!m_player.isLoaded()) {
            return;
        }
        double duration = m_player.getDuration();
        double pos = duration * ui->horizontalSlider->value() / ui->horizontalSlider->maximum();
//        double spos;
//        if (!m_player.getSeekPos(pos, spos)) {
//            return;
//        }
        m_player.seek(pos);
    }
}

void MainWindow::onHorizontalSliderValueChanged(int value)
{
//    qDebug() << __PRETTY_FUNCTION__ << "value:" << value;
    if (ui->horizontalSlider->pressed()) {
        if (!m_player.isLoaded()) {
            return;
        }

        double duration = m_player.getDuration();
        double pos = duration * value / ui->horizontalSlider->maximum();
        double spos;
        if (!m_player.getSeekPos(pos, spos)) {
            return;
        }
//        qDebug() << __PRETTY_FUNCTION__ << "pos:" << pos << "seek pos:" << spos;

        int iduration = duration, ipos = spos;
        QString text;
//        text.sprintf("%d/%d", ipos, iduration);
        text.sprintf("%d:%02d:%02d/%d:%02d:%02d",
                     ipos/3600, ipos%3600/60, ipos%3600%60,
                     iduration/3600, iduration%3600/60, iduration%3600%60);
        ui->label->setText(text);
    }
}

void MainWindow::onPlayerFileChanged(const QString &file)
{
    if (file.isEmpty()) {
        setWindowTitle("ffmpeg player");
    }
    else {
        setWindowTitle(file);
    }
}

void MainWindow::onPlayerplaybackStateChanged(bool isPlaying)
{
    ui->playPauseBtn->setText(isPlaying ? "暂停" : "播放");
}

void MainWindow::onPlayerPositionChanged(double pos)
{
//    qDebug() << __PRETTY_FUNCTION__ << pos;
    if (!ui->horizontalSlider->pressed() && !m_player.isSeeking()) {
        double duration = m_player.getDuration();
        int ipos = pos, iduration = duration;
        QString text;
//        text.sprintf("%d/%d", ipos, iduration);
        text.sprintf("%d:%02d:%02d/%d:%02d:%02d",
                     ipos/3600, ipos%3600/60, ipos%3600%60,
                     iduration/3600, iduration%3600/60, iduration%3600%60);
        ui->label->setText(text);
        ui->horizontalSlider->setValue(ui->horizontalSlider->maximum() * pos / duration);
    }
}

void MainWindow::onVideoUpdated(QImage img)
{
}

void MainWindow::onVideoFrameUpdated(const SPAVFrame &frame)
{
    ui->openGLWidget->showVideoFrame(frame);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    ui->openGLWidget->setGeometry(0, 0, width(), ui->centralwidget->height() - ui->horizontalSlider->height() - ui->horizontalLayoutWidget->height());
    ui->label_subtitle->setGeometry(ui->openGLWidget->x(), ui->openGLWidget->y() + ui->openGLWidget->height() - 100, ui->openGLWidget->width(), 100);
    ui->horizontalSlider->setGeometry(ui->openGLWidget->x(), ui->openGLWidget->y() + ui->openGLWidget->height(),
                                      width() - 120, ui->horizontalSlider->height());
    ui->label->setGeometry(ui->horizontalSlider->x() + ui->horizontalSlider->width(), ui->horizontalSlider->y(),
                           120, ui->horizontalSlider->height());
    ui->horizontalLayoutWidget->setGeometry(ui->openGLWidget->x(), ui->horizontalSlider->y() + ui->horizontalSlider->height(),
                                            width(), ui->horizontalLayoutWidget->height());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list")) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }
    play(urls.front().toLocalFile());
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        m_menu->exec(QCursor::pos());
    }
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
//        if (isFullScreen()) {
//            showNormal();
//        }
//        else {
//            showFullScreen();
//        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W) {
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
        qDebug() << QApplication::keyboardModifiers();
        ui->openGLWidget->decreaseXDeviation();
    }
    else if (event->key() == Qt::Key_Right) {
        ui->openGLWidget->increaseXDeviation();
    }
    else if (event->key() == Qt::Key_Up) {
        ui->openGLWidget->increaseYDeviation();
    }
    else if (event->key() == Qt::Key_Down) {
        ui->openGLWidget->decreaseYDeviation();
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
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

void MainWindow::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0) {
//        qDebug() << __PRETTY_FUNCTION__ << "delta > 0";
        if (m_wPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "++++WWWW";
            ui->openGLWidget->increaseWidth();
        }
        if (m_hPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "++++HHHH";
            ui->openGLWidget->increaseHeight();
        }
        if (m_sPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "++++SSSS";
            ui->openGLWidget->increaseSize();
        }
    }
    else if (event->delta() < 0) {
//        qDebug() << __PRETTY_FUNCTION__ << "delta < 0";
        if (m_wPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "----WWWW";
            ui->openGLWidget->decreaseWidth();
        }
        if (m_hPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "----HHHH";
            ui->openGLWidget->decreaseHeight();
        }
        if (m_sPressed) {
            qDebug() << __PRETTY_FUNCTION__ << "----SSSS";
            ui->openGLWidget->decreaseSize();
        }
    }
}

void MainWindow::move2DesktopCenter()
{
    QDesktopWidget dw;
    const QRect g = dw.availableGeometry();
    move((g.width() - frameSize().width())/2 + g.x(),
         (g.height() - frameSize().height())/2 + g.y());
}

QSize MainWindow::getAdjustSize(int maxw, int maxh, int minw, int minh, int w, int h)
{
    if (w > maxw || h > maxh) {
        if (1.0 * w / h > 1.0 * maxw / maxh) {
            h = 1.0 * h * maxw / w;
            w = maxw;
        }
        else if (1.0 * w / h < 1.0 * maxw / maxh) {
            w = 1.0 * w * maxh / h;
            h = maxh;
        }
        else {
            w = maxw;
            h = maxh;
        }
    }
    if (w < minw || h < minh) {
        if (1.0 * w / h > 1.0 * minw / minh) {
            w = 1.0 * w * minh / h;
            h = minh;
        }
        else if (1.0 * w / h < 1.0 * minw / minh) {
            h = 1.0 * h * minw / w;
            w = minw;
        }
        else {
            w = minw;
            h = minh;
        }
    }
    return QSize(w, h);
}

void MainWindow::resizeOpenglWidgetSize(int w, int h)
{
    QDesktopWidget dw;
    const QRect g = dw.availableGeometry();
    QSize glSize = getAdjustSize(g.width()/2, g.height()/2, 200, 120, w, h);
    ui->openGLWidget->resize(glSize.width(), glSize.height());
    resize(glSize.width(), ui->menuBar->height() + glSize.height() + ui->horizontalSlider->height() + ui->horizontalLayoutWidget->height());
}

QAction *MainWindow::getAction(const QString &text)
{
    foreach (QAction *action, m_menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return 0;
}

QMenu *MainWindow::getMenu(QMenu *menu, const QString &text)
{
    if (menu == 0) {
        return 0;
    }
    foreach (QAction *action, menu->actions()) {
        QMenu *m = action->menu();
        if (m == 0) {
            continue;
        }
        if (action->text() == text) {
            return m;
        }
        m = getMenu(action->menu(), text);
        if (m != 0) {
            return m;
        }
    }
    return 0;
}

void MainWindow::on_actionOpenFile_triggered()
{
    static bool first = true;
    QString firstPath;
//    firstPath = QDir::homePath();
//    firstPath = "D:/Musics/mp3/古风";
    firstPath = "D:/Videos";
    QString file = QFileDialog::getOpenFileName(this, "打开媒体文件", first ? firstPath : "", tr("Media Files (*.*)"));
    first = false;
    play(file);
}

void MainWindow::on_playPauseBtn_clicked()
{
    if (m_player.isPlaying()) {
        m_player.pause();
    }
    else {
        m_player.play();
    }
}

void MainWindow::on_stopBtn_clicked()
{
    m_player.unload();
    ui->openGLWidget->clear();
    ui->label->setText("0:00:00/0:00:00");
    ui->openGLWidget->resetWidth();
    ui->openGLWidget->resetHeight();
}

void MainWindow::on_actionResetVideoWidthHeight()
{
    ui->openGLWidget->resetWidth();
    ui->openGLWidget->resetHeight();
}

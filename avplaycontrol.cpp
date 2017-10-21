#include "avplaycontrol.h"
#include "audioplayer_directsound.h"

AVPlayControl::AVPlayControl()
    : m_decoderCore(0)
    , m_enabledAudioStreamIndex(-1)
    , m_enabledVideoStreamIndex(-1)
    , m_audioPlayer(0)
    , m_videoDecoderBuffer(0)
    , m_isPlaying(false)
    , m_position(0.0)
{

}

AVPlayControl::~AVPlayControl()
{
    unload();
}

void AVPlayControl::init()
{
    m_videoShowTimer.setInterval(40);
    connect(&m_videoShowTimer, SIGNAL(timeout()),
            this, SLOT(onVideoShowTimerTimeout()));
}

bool AVPlayControl::load(const QString &file)
{
    if (!QFile::exists(file)) {
        return false;
    }
    if (file == m_file) {
        return true;
    }

    if (isLoaded()) {
        unload();
    }

    m_decoderCore = new AVDecoderCore();
    if (!m_decoderCore->load(file)) {
        unload();
        return false;
    }
    m_decoderCore->showInfo();

    if (!m_decoderCore->hasAudioStream() && !m_decoderCore->hasVideoStream()) {
        unload();
        return false;
    }

    if (m_decoderCore->hasAudioStream()) {
        int index = 0;
        if (!m_decoderCore->enableAudioStream(index)) {
            unload();
            return false;
        }
        m_enabledAudioStreamIndex = index;
        m_audioPlayer = new AudioPlayer_DirectSound(m_decoderCore, m_enabledAudioStreamIndex);
        if (!m_audioPlayer->isAvailable()) {
            unload();
            return false;
        }
        connect(m_audioPlayer, SIGNAL(playbackStateChanged(bool)),
                this, SLOT(onAudioPlayerPlaybackStateChanged(bool)));
        connect(m_audioPlayer, SIGNAL(positionChanged(double)),
                this, SLOT(onAudioPlayerPositionChanged(double)));
    }

    if (m_decoderCore->hasVideoStream()) {
        int index = 0;
        if (!m_decoderCore->enableVideoStream(index)) {
            unload();
            return false;
        }
        m_enabledVideoStreamIndex = index;
        m_videoDecoderBuffer = new VideoDecoderBuffer(m_decoderCore, m_enabledVideoStreamIndex);
        if (!m_videoDecoderBuffer->isAvailable()) {
            unload();
            return false;
        }
        connect(m_videoDecoderBuffer, SIGNAL(buffered()),
                this, SLOT(onVideoDecoderBuffered()));
    }

    setFile(file);
    return true;
}

void AVPlayControl::unload()
{
    if (!isLoaded()) {
        return;
    }

    if (m_audioPlayer != 0) {
//        m_audioPlayer->deleteLater();
        delete m_audioPlayer;
        m_audioPlayer = 0;
    }

    if (m_videoDecoderBuffer != 0) {
//        m_videoDecoderBuffer->deleteLater();
        delete m_videoDecoderBuffer;
        m_videoDecoderBuffer = 0;
    }

    if (m_decoderCore != 0) {
        delete m_decoderCore;
        m_enabledAudioStreamIndex = -1;
        m_enabledVideoStreamIndex = -1;
        m_decoderCore = 0;
    }

    m_videoShowTimer.stop();

    setFile(QString());
    setPlaybackState(false);
    setPosition(0.0);
}

bool AVPlayControl::isLoaded()
{
    return !m_file.isEmpty();
}

QString AVPlayControl::getFile()
{
    return m_file;
}

bool AVPlayControl::isAudioAvailable()
{
    if (!isLoaded()) {
        return false;
    }
    if (m_audioPlayer == 0 || !m_audioPlayer->isAvailable()) {
        return false;
    }
    return true;
}

bool AVPlayControl::hasAudioStream()
{
    if (!isLoaded()) {
        return false;
    }
    return m_decoderCore->hasAudioStream();
}

int AVPlayControl::getAudioStreamCount()
{
    if (!isLoaded()) {
        return 0;
    }
    return m_decoderCore->getAudioStreamCount();
}

QString AVPlayControl::getAudioStreamTitle(int index)
{
    if (!isLoaded()) {
        return QString();
    }
    return m_decoderCore->getAudioStreamTitle(index);
}

int AVPlayControl::getCurrentAudioStreamIndex()
{
    if (!isLoaded()) {
        return -1;
    }
    return m_enabledAudioStreamIndex;
}

void AVPlayControl::changeAudioStream(int index)
{
    if (!isLoaded()) {
        return;
    }
    if (index == m_enabledAudioStreamIndex) {
        return;
    }

    if (!m_decoderCore->enableAudioStream(index)) {
        return;
    }
    AudioPlayerBase *player = new AudioPlayer_DirectSound(m_decoderCore, index);
    if (!player->isAvailable()) {
        delete player;
        m_decoderCore->disableAudioStream(index);
        return;
    }
    connect(player, SIGNAL(playbackStateChanged(bool)),
            this, SLOT(onAudioPlayerPlaybackStateChanged(bool)));
    connect(player, SIGNAL(positionChanged(double)),
            this, SLOT(onAudioPlayerPositionChanged(double)));

    if (isAudioAvailable()) {
        bool isPlaying = m_audioPlayer->isPlaying();
        double position = m_audioPlayer->getPosition();
        delete m_audioPlayer;

        player->seek(position);
        if (isPlaying) {
            player->play();
        }
    }

    m_decoderCore->disableAudioStream(m_enabledAudioStreamIndex);
    m_enabledAudioStreamIndex = index;
    m_audioPlayer = player;
}

void AVPlayControl::changeAudioStream(const QString &title)
{
    if (!isLoaded()) {
        return;
    }
    for (int i = 0; i < m_decoderCore->getAudioStreamCount(); ++i) {
        if (m_decoderCore->getAudioStreamTitle(i) == title) {
            changeAudioStream(i);
            break;
        }
    }
}

bool AVPlayControl::isVideoAvailable()
{
    if (!isLoaded()) {
        return false;
    }
    if (m_videoDecoderBuffer == 0 || !m_videoDecoderBuffer->isAvailable()) {
        return false;
    }
    return true;
}

void AVPlayControl::play()
{
    if (!isLoaded()) {
        return;
    }
    if (isPlaying()) {
        return;
    }

    if (isAudioAvailable()) {
        m_audioPlayer->play();
        return;
    }

    if (isVideoAvailable()) {
        if (m_videoDecoderBuffer->isDecodeEnd()) {
            m_videoDecoderBuffer->resetDecoder();
        }
        m_videoShowTimer.start();
    }

    checkPlaybackState();
}

void AVPlayControl::pause()
{
    if (!isLoaded()) {
        return;
    }
    if (!isPlaying()) {
        return;
    }

    if (isAudioAvailable()) {
        m_audioPlayer->stop();
    }

    if (isVideoAvailable()) {
        m_videoShowTimer.stop();
    }

    checkPlaybackState();
}

bool AVPlayControl::isPlaying()
{
    if (!isLoaded()) {
        return false;
    }
    if (isAudioAvailable()) {
        return m_audioPlayer->isPlaying();
    }
    if (isVideoAvailable()) {
        return m_videoShowTimer.isActive();
    }
    return false;
}

double AVPlayControl::getDuration()
{
    if (!isLoaded()) {
        return 0.0;
    }
    if (isAudioAvailable()) {
        return m_decoderCore->getAudioDuration(m_enabledAudioStreamIndex);
    }
    if (isVideoAvailable()) {
        return m_decoderCore->getVideoDuration(m_enabledVideoStreamIndex);
    }
    return 0.0;
}

double AVPlayControl::getPosition()
{
    if (!isLoaded()) {
        return 0.0;
    }
    return m_position;
}

void AVPlayControl::seek(double pos)
{
    if (pos < 0) {
        return;
    }
    if (!isLoaded()) {
        return;
    }
    if (pos > getDuration()) {
        return;
    }
    double spos;
    if (!getSeekPos(pos, spos)) {
        return;
    }
    if (isAudioAvailable()) {
        m_audioPlayer->seek(spos);
    }
    if (isVideoAvailable()) {
        m_videoDecoderBuffer->seek(spos);
    }
}

bool AVPlayControl::getSeekPos(double t, double &spos)
{
    if (t < 0.0) {
        return false;
    }
    if (!isLoaded()) {
        return false;
    }
    if (isVideoAvailable()) {
        return m_decoderCore->getVideoSeekPos(m_enabledVideoStreamIndex, t, AVDecoderCore::SEEK_POS_LEFT_KEY, spos);
    }
    if (isAudioAvailable()) {
        spos = t;
        return true;
    }
    return false;
}

bool AVPlayControl::isSeeking()
{
    if (!isLoaded()) {
        return false;
    }
    if (isAudioAvailable()) {
        return m_audioPlayer->isSeeking();
    }
    if (isVideoAvailable()) {
        return m_videoDecoderBuffer->isSeeking();
    }
    return false;
}

int AVPlayControl::getVideoWidth()
{
    if (!isLoaded()) {
        return 0;
    }
    if (!isVideoAvailable()) {
        return 0;
    }
    return m_decoderCore->getVideoWidth(m_enabledVideoStreamIndex);
}

int AVPlayControl::getVideoHeight()
{
    if (!isLoaded()) {
        return 0;
    }
    if (!isVideoAvailable()) {
        return 0;
    }
    return m_decoderCore->getVideoHeight(m_enabledVideoStreamIndex);
}

bool AVPlayControl::hasCover()
{
    if (!isLoaded()) {
        return false;
    }
    return m_decoderCore->hasCover();
}

int AVPlayControl::getCoverCount()
{
    if (!isLoaded()) {
        return 0;
    }
    return m_decoderCore->getCoverCount();
}

bool AVPlayControl::getCover(int index, SPAVFrame &frame)
{
    if (!isLoaded()) {
        return false;
    }
    return m_decoderCore->getCover(index, frame);
}

void AVPlayControl::onVideoShowTimerTimeout()
{
    if (!isLoaded()) {
        m_videoShowTimer.stop();
        checkPlaybackState();
        return;
    }

    if (!isVideoAvailable()) {
        m_videoShowTimer.stop();
        checkPlaybackState();
        return;
    }

    while (1) {
        if (!m_videoDecoderBuffer->hasBufferedData()) {
            if (m_videoDecoderBuffer->isDecodeEnd()) {
                m_videoShowTimer.stop();
                checkPlaybackState();
            }
            return;
        }
        VideoDecoderBuffer::VideoData vd = m_videoDecoderBuffer->popBufferedData();
        if (vd.time < 0 || vd.frame.isNull()) {
            continue;
        }
        emit videoFrameUpdated(vd.frame);
        setPosition(vd.time);
    }
}

void AVPlayControl::onAudioPlayerPlaybackStateChanged(bool isPlaying)
{
    if (!isLoaded()) {
        return;
    }
    checkPlaybackState();
}

void AVPlayControl::onAudioPlayerPositionChanged(double position)
{
    if (!isLoaded()) {
        return;
    }
    if (isVideoAvailable()) {
        syncVideo2Audio(position);
    }
    setPosition(position);
}

void AVPlayControl::onVideoDecoderBuffered()
{
    if (!isLoaded()) {
        return;
    }
    if (!isVideoAvailable()) {
        return;
    }

    if (isAudioAvailable()) {
        if (!m_audioPlayer->isPlaying()) {
            emit videoFrameUpdated(m_videoDecoderBuffer->getBufferedData().frame);
        }
        else {
            syncVideo2Audio(m_audioPlayer->getPosition());
        }
    }
}

void AVPlayControl::setFile(const QString &file)
{
    if (file == m_file) {
        return;
    }
    m_file = file;
    emit fileChanged(file);
}

void AVPlayControl::setPlaybackState(bool isPlaying)
{
    if (isPlaying == m_isPlaying) {
        return;
    }
    m_isPlaying = isPlaying;
    emit playbackStateChanged(isPlaying);
}

void AVPlayControl::setPosition(double position)
{
    if (position == m_position) {
        return;
    }
    m_position = position;
    emit positionChanged(position);
}

void AVPlayControl::updateVideo()
{
    if (!isVideoAvailable()) {
        return;
    }
    if (!m_videoDecoderBuffer->hasBufferedData()) {
        return;
    }

    VideoDecoderBuffer::VideoData vd = m_videoDecoderBuffer->popBufferedData();
    if (vd.time < 0 || vd.frame.isNull()) {
        return;
    }
    emit videoFrameUpdated(vd.frame);

    if (!isAudioAvailable()) {
        setPosition(vd.time);
    }
}

void AVPlayControl::syncVideo2Audio(double postion)
{
    if (!isLoaded()) {
        return;
    }
    if (!isVideoAvailable()) {
        return;
    }

    VideoDecoderBuffer::VideoData vd;
    while (1) {
        if (!m_videoDecoderBuffer->hasBufferedData()) {
            return;
        }
        vd = m_videoDecoderBuffer->getBufferedData();
        if (vd.time < 0 || vd.frame.isNull()) {
            m_videoDecoderBuffer->popBufferedData();
            continue;
        }

        double duration = 0.04, frameRate = 0.0;
        if (vd.duration > 0.0) {
            duration =  vd.duration;
        }
        else if ((frameRate = m_decoderCore->getVideoFrameRate(m_enabledVideoStreamIndex)) > 0.0) {
            duration = 1 / frameRate;
        }

        if (vd.time + duration < postion) {
//            qDebug() << __PRETTY_FUNCTION__
//                     << "skip video buffered data"
//                     << "vd time:" << vd.time
//                     << "vd duration:" << vd.duration
//                     << "m_position:" << m_position
//                     << "duration:" << duration
//                     << "audio pos:" << postion;
            m_videoDecoderBuffer->popBufferedData();
            continue;
        }

        if (vd.time <= postion && postion <= vd.time + duration) {
            emit videoFrameUpdated(vd.frame);
        }
        break;
    }
}

void AVPlayControl::checkPlaybackState()
{
    if (!isLoaded()) {
        setPlaybackState(false);
        return;
    }

    if (isAudioAvailable()) {
        if (m_audioPlayer->isPlaying()) {
            setPlaybackState(true);
            return;
        }
    }

    if (isVideoAvailable()) {
        if (m_videoShowTimer.isActive()) {
            setPlaybackState(true);
            return;
        }
    }

    setPlaybackState(false);
}

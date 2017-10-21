#include "audioplayerbase.h"

AudioPlayerBase::AudioPlayerBase(AVDecoderCore *decoder, int audioStreamIndex, QObject *parent)
    : QObject(parent)
    , m_avdecoder(decoder)
    , m_enabledAudioStreamIndex(audioStreamIndex)
    , m_decoderBuffer(decoder, audioStreamIndex)
    , m_isPlaying(false)
    , m_position(0.0)
    , m_isSeeking(false)
{
}

void AudioPlayerBase::play()
{
}

bool AudioPlayerBase::isPlaying()
{
    return m_isPlaying;
}

double AudioPlayerBase::getPosition()
{
    return m_position;
}

void AudioPlayerBase::seek(double pos)
{
}

bool AudioPlayerBase::isSeeking()
{
    return m_isSeeking;
}

bool AudioPlayerBase::isDecoderAvailable()
{
    if (m_avdecoder == 0) {
        return false;
    }
    if (!m_avdecoder->hasAudioStream()) {
        return false;
    }
    if (!m_avdecoder->isAudioStreamEnabled(m_enabledAudioStreamIndex)) {
        return false;
    }
    if (!m_decoderBuffer.isAvailable()) {
        return false;
    }
    return true;
}

void AudioPlayerBase::setPlaybackState(bool isPlaying)
{
    if (isPlaying == m_isPlaying) {
        return;
    }
    m_isPlaying = isPlaying;
    emit playbackStateChanged(isPlaying);
}

void AudioPlayerBase::setPosition(double pos)
{
    if (pos == m_position) {
        return;
    }
    m_position = pos;
//    qDebug() << __PRETTY_FUNCTION__ << "pos:" << pos;
    emit positionChanged(pos);
}

void AudioPlayerBase::setSeekingState(bool isSeeking)
{
    if (isSeeking == m_isSeeking) {
        return;
    }
    m_isSeeking = isSeeking;
//    qDebug() << __PRETTY_FUNCTION__ << "isSeeking:" << isSeeking;
    emit seekingStateChanged(isSeeking);
}

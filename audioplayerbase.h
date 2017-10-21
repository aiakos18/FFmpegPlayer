#ifndef AUDIOPLAYERBASE_H
#define AUDIOPLAYERBASE_H

#include <QObject>
#include <QtCore>

#include "avdecodercore.h"
#include "audiodecoderbuffer.h"

class AudioPlayerBase : public QObject
{
    Q_OBJECT
public:
    AudioPlayerBase(AVDecoderCore *decoder, int audioStreamIndex, QObject *parent = nullptr);

    virtual bool isAvailable() = 0;
    virtual void play() = 0;
    virtual void stop() = 0;
    virtual void seek(double pos) = 0;

    bool isPlaying();
    double getPosition();
    bool isSeeking();

signals:
    void playbackStateChanged(bool isPlaying);
    void positionChanged(double postion);
    void seekingStateChanged(bool isSeeking);

protected:
    bool isDecoderAvailable();
    void setPlaybackState(bool isPlaying);
    void setPosition(double pos);
    void setSeekingState(bool isSeeking);

protected:
    AVDecoderCore * m_avdecoder;
    int m_enabledAudioStreamIndex;
    AudioDecoderBuffer m_decoderBuffer;
    bool m_isPlaying;
    double m_position;
    bool m_isSeeking;
};

#endif // AUDIOPLAYERBASE_H

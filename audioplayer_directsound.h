#ifndef AUDIOPLAYER_DIRECTSOUND_H
#define AUDIOPLAYER_DIRECTSOUND_H

#include <windows.h>
#include <dsound.h>

#include <QtCore>

#include "audioplayerbase.h"
#include "audiodecoderbuffer.h"

class AudioPlayer_DirectSound : public AudioPlayerBase
{
    Q_OBJECT
public:
    explicit AudioPlayer_DirectSound(AVDecoderCore *decoder, int audioStreamIndex, QObject *parent = nullptr);
    virtual ~AudioPlayer_DirectSound();

    bool isAvailable();
    void play();
    void stop();
    void seek(double pos);

protected slots:
    void onDecoderSeekingStateChanged(bool isSeeking);

protected:
    Q_INVOKABLE void outputAudioData(int taskid);
    HWND getWindowHandle();

protected:
    QThread m_thread;
    int m_taskid;
    QMutex m_mtx, m_dataMtx;
    QWaitCondition m_cond;
    AudioDecoderBuffer::AudioData m_ad;
};

#endif // AUDIOPLAYER_DIRECTSOUND_H

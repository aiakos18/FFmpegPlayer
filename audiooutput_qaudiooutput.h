#ifndef AUDIOOUTPUT_QAUDIOOUTPUT_H
#define AUDIOOUTPUT_QAUDIOOUTPUT_H

#include <QAudioOutput>

#include "audiooutputbase.h"
#include "avdecodercore.h"

class AudioOutput_QAudioOutput : public AudioOutputBase
{
    Q_OBJECT
public:
    explicit AudioOutput_QAudioOutput(AVDecoderCore *decoder, QObject *parent = nullptr);
    virtual ~AudioOutput_QAudioOutput();

    virtual bool isAvailable();

    virtual void init();

    virtual void play();
    virtual void pause();
    virtual bool isPlaying();

    virtual double getPosition();
    virtual void seek(double pos);

protected:
    Q_INVOKABLE void outputAudioData(int taskid);

protected:
    QThread m_thread;
    AVDecoderCore *m_decoder;
    QByteArray m_data;

    int m_taskid;
    QMutex m_mtx;
    AVDecoder::AudioData m_ad;
};

#endif // AUDIOOUTPUT_QAUDIOOUTPUT_H

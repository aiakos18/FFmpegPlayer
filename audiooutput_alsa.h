#ifndef AUDIOOUTPUT_ALSA_H
#define AUDIOOUTPUT_ALSA_H

#include "audiooutputbase.h"
#include "avdecoder.h"

class AudioOutput_Alsa : public AudioOutputBase
{
    Q_OBJECT
public:
    explicit AudioOutput_Alsa(QObject *parent = nullptr);
    virtual ~AudioOutput_Alsa();

    virtual void init();

    virtual bool loadFile(const QString &file);
    virtual void unload();
    virtual bool isLoaded();
    virtual QString getFile();

    virtual void play();
    virtual void pause();
    virtual bool isPlaying();

    virtual double getDuration();
    virtual double getPosition();
    virtual void seek(double pos);

protected:
    Q_INVOKABLE void outputAudioData(int taskid);

protected:
    QThread m_thread;
    AVDecoder m_decoder;
    QByteArray m_data;

    int m_taskid;
    QMutex m_mtx;
    AVDecoder::AudioData m_ad;
};

#endif // AUDIOOUTPUT_ALSA_H

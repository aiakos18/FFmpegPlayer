#ifndef AUDIOOUTPUT_SDL2_H
#define AUDIOOUTPUT_SDL2_H

#include <SDL2/SDL.h>

#include "audiooutputbase.h"
#include "avdecoder.h"

class AudioOutput_SDL2 : public AudioOutputBase
{
    Q_OBJECT
public:
    explicit AudioOutput_SDL2(QObject *parent = nullptr);
    virtual ~AudioOutput_SDL2();

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

    static void onSdlCallback(void *userdata, uint8_t * stream, int len);
    void doSdlCallback(uint8_t * stream, int len);

protected:
    QThread m_thread;
    AVDecoder m_decoder;
    QByteArray m_data;

    int m_taskid;
    QMutex m_mtx;
    AVDecoder::AudioData m_ad;
};

#endif // AUDIOOUTPUT_SDL2_H

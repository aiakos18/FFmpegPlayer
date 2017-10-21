#include "audioplayer_directsound.h"
#include "smartmutex.h"

AudioPlayer_DirectSound::AudioPlayer_DirectSound(AVDecoderCore *decoder, int audioStreamIndex, QObject *parent)
    : AudioPlayerBase(decoder, audioStreamIndex, parent)
    , m_taskid(0)
{
    moveToThread(&m_thread);

    if (isAvailable()) {
        m_thread.start();

        connect(&m_decoderBuffer, SIGNAL(seekingStateChanged(bool)),
                this, SLOT(onDecoderSeekingStateChanged(bool)), Qt::DirectConnection);
    }
}

AudioPlayer_DirectSound::~AudioPlayer_DirectSound()
{
    stop();

    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

bool AudioPlayer_DirectSound::isAvailable()
{
    return isDecoderAvailable();
}

void AudioPlayer_DirectSound::play()
{
    if (isPlaying()) {
        return;
    }
    if (!isAvailable()) {
        return;
    }

    if (m_decoderBuffer.isDecodeEnd()) {
        m_decoderBuffer.resetDecoder();
    }
    QMetaObject::invokeMethod(this, "outputAudioData", Q_ARG(int,++m_taskid));
}

void AudioPlayer_DirectSound::stop()
{
    ++m_taskid;
}

void AudioPlayer_DirectSound::seek(double pos)
{
    if (pos < 0.0) {
        return;
    }
    if (!isAvailable()) {
        return;
    }
    SmartMutex mtx(&m_mtx);
//    qDebug() << __PRETTY_FUNCTION__ << "start";
    if (!m_decoderBuffer.seek(pos)) {
        return;
    }
    m_ad.data.clear();
    m_ad.time = 0.0;
    m_ad.duration = 0.0;
    setSeekingState(true);
//    qDebug() << __PRETTY_FUNCTION__ << "end";
}

void AudioPlayer_DirectSound::onDecoderSeekingStateChanged(bool isSeeking)
{
    if (isSeeking) {
        m_mtx.lock();
        setSeekingState(true);
        m_mtx.unlock();
    }
    else {
        m_mtx.lock();
        setSeekingState(false);
        m_mtx.unlock();
    }
}

void AudioPlayer_DirectSound::outputAudioData(int taskid)
{
    qDebug() << __PRETTY_FUNCTION__ << "start";

    if (!isAvailable()) {
        return;
    }

    int sampleRate = m_avdecoder->getAudioOutputSampleRate(m_enabledAudioStreamIndex);
    int sampleSize = m_avdecoder->getAudioOutputSampleSize(m_enabledAudioStreamIndex);
    int channels = m_avdecoder->getAudioOutputChannels(m_enabledAudioStreamIndex);
    int bytesPerSample = m_avdecoder->getAudioOutputBytesPerSample(m_enabledAudioStreamIndex);
    int bytesPerFrame = m_avdecoder->getAudioOutputBytesPerFrame(m_enabledAudioStreamIndex);
    int bytesPerSecond = m_avdecoder->getAudioOutputBytesPerSecond(m_enabledAudioStreamIndex);
    qDebug() << __PRETTY_FUNCTION__ << "sample rate:" << sampleRate;
    qDebug() << __PRETTY_FUNCTION__ << "sample size:" << sampleSize;
    qDebug() << __PRETTY_FUNCTION__ << "channels:" << channels;
    qDebug() << __PRETTY_FUNCTION__ << "bytes per sample:" << bytesPerSample;
    qDebug() << __PRETTY_FUNCTION__ << "bytes per frame:" << bytesPerFrame;
    qDebug() << __PRETTY_FUNCTION__ << "bytes per second:" << bytesPerSecond;

    int audioBufCount = 10;
    double notifyEverytime = 0.01;
    int bufferNotifySize = bytesPerSecond * notifyEverytime;
    qDebug() << __PRETTY_FUNCTION__ << "buffer notify size:" << bufferNotifySize;
    m_decoderBuffer.setBufferMinSize(bufferNotifySize * 4);

    IDirectSound8 *pDS8 = 0;
    IDirectSoundBuffer *pDSBuffer = 0;
    IDirectSoundBuffer8 *pDSBuffer8 = 0;
    IDirectSoundNotify *pDSNotify = 0;
    int offset = 0;
    DSBUFFERDESC dsbd;
    DSBPOSITIONNOTIFY pDSPosNotify[audioBufCount];
    HANDLE event[audioBufCount];

    HRESULT r = DirectSoundCreate8(0, &pDS8, 0);
    if (r != 0) {
        qDebug() << "cannot create direct sound";
        goto END;
    }
    if (!pDS8) {
        qDebug() << "pDS8 null";
        goto END;
    }
    r = pDS8->SetCooperativeLevel(getWindowHandle(), DSSCL_NORMAL);
    if (r != 0) {
        qDebug() << "cannot SetCooperativeLevel";
        goto END;
    }

    pDS8->SetSpeakerConfig(DSSPEAKER_5POINT1);

    memset(&dsbd,0,sizeof(dsbd));
    dsbd.dwSize=sizeof(dsbd);
    dsbd.dwFlags=DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY |DSBCAPS_GETCURRENTPOSITION2;
    dsbd.dwBufferBytes=audioBufCount*bufferNotifySize;
    //WAVE Header
    dsbd.lpwfxFormat=(WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
    /* format type */
    dsbd.lpwfxFormat->wFormatTag=WAVE_FORMAT_PCM;
    /* number of channels (i.e. mono, stereo...) */
    (dsbd.lpwfxFormat)->nChannels=channels;
    /* sample rate */
    (dsbd.lpwfxFormat)->nSamplesPerSec=sampleRate;
    /* for buffer estimation */
    (dsbd.lpwfxFormat)->nAvgBytesPerSec=bytesPerSecond;
    /* block size of data */
    (dsbd.lpwfxFormat)->nBlockAlign=bytesPerFrame;
    /* number of bits per sample of mono data */
    (dsbd.lpwfxFormat)->wBitsPerSample=sampleSize;
    (dsbd.lpwfxFormat)->cbSize=0;

    r = pDS8->CreateSoundBuffer(&dsbd, &pDSBuffer, 0);
    if (r != 0) {
        qDebug() << "cannot CreateSoundBuffer:" << r;
        goto END;
    }

    r = pDSBuffer->QueryInterface(IID_IDirectSoundBuffer, (LPVOID*)&pDSBuffer8);
    if (r != 0) {
        qDebug() << "cannot get IDirectSoundBuffer8" << r;
        goto END;
    }

    r = pDSBuffer8->QueryInterface(IID_IDirectSoundNotify,(LPVOID*)&pDSNotify);
    if (r != 0) {
        qDebug() << "cannot get IDirectSoundNotify:" << r;
        goto END;
    }

    for(int i = 0; i< audioBufCount; i++){
        pDSPosNotify[i].dwOffset = i * bufferNotifySize;
        event[i]=::CreateEvent(NULL, false, false, NULL);
        pDSPosNotify[i].hEventNotify = event[i];
    }
    r = pDSNotify->SetNotificationPositions(audioBufCount, pDSPosNotify);
    if (r != 0) {
        qDebug() << "cannot get SetNotificationPositions:" << r;
        goto END;
    }

    pDSBuffer8->SetCurrentPosition(0);
    pDSBuffer8->Play(0,0,DSBPLAY_LOOPING);
    setPlaybackState(true);

    while (taskid == m_taskid) {
        if (m_ad.data.isEmpty()
                && !m_decoderBuffer.hasBufferedData()
                && m_decoderBuffer.isDecodeEnd()) {
            pDSBuffer8->Stop();
            setPlaybackState(false);
            break;
        }

        SmartMutex mtx(&m_mtx);
//        if (!isPlaying()) {
//            m_cond.wait(&m_mtx);
//            continue;
//        }

        QByteArray data(bufferNotifySize, 0);
        double pos = m_position;
        int r = 0;
        while (r < bufferNotifySize) {
            if (!m_ad.data.isEmpty()) {
                int cp = qMin(m_ad.data.size(), bufferNotifySize - r);
                data.replace(r, cp, m_ad.data);
                r += cp;
                double dur = m_ad.duration * cp / m_ad.data.size();
                m_ad.data.remove(0, cp);
                m_ad.time += dur;
                m_ad.duration -= dur ;
                pos = m_ad.time;
                continue;
            }

            if (!m_decoderBuffer.hasBufferedData()) {
//                qDebug() << __PRETTY_FUNCTION__ << "decoder has no audio data";
                break;
            }
            if (!m_isSeeking) {
//                qDebug() <<  __PRETTY_FUNCTION__ << "popBufferedData";
                m_ad = m_decoderBuffer.popBufferedData();
            }
            else {
//                qDebug() <<  __PRETTY_FUNCTION__ << "is seeking, cannot popBufferedData";
                break;
            }
        }

        VOID *buf1 = 0, *buf2 = 0;
        DWORD buflen1, buflen2;
        pDSBuffer8->Lock(offset, bufferNotifySize, &buf1, &buflen1, &buf2, &buflen2, 0);
        memcpy(buf1, data.data(), buflen1);
        if (buf2) {
            memcpy(buf2, data.data() + buflen1, buflen2);
        }
        offset += buflen1 + buflen2;
        offset %= (bufferNotifySize * audioBufCount);
        pDSBuffer8->Unlock(buf1, buflen1, buf2, buflen2);
        WaitForMultipleObjects(audioBufCount, event, FALSE, notifyEverytime * 1000 * 2);
        setPosition(pos);
    }

END:
    m_dataMtx.lock();
    m_ad.data.clear();
    m_ad.time = 0;
    m_ad.duration = 0;
    m_dataMtx.unlock();

    if (pDSNotify) {
        pDSNotify->Release();
    }

    if (pDSBuffer8) {
        pDSBuffer8->Release();
    }

    if (pDSBuffer) {
        pDSBuffer->Release();
    }

    if (pDS8) {
        pDS8->Release();
    }

    setPlaybackState(false);
    setSeekingState(false);

    qDebug() << __PRETTY_FUNCTION__ << "end";
}

HWND AudioPlayer_DirectSound::getWindowHandle()
{
    DWORD dwPID = 0;
    HWND hwndRet = NULL;
    // 取得第一个窗口句柄
    HWND hwndWindow = ::GetTopWindow(0);
    while (hwndWindow) {
        dwPID = 0;
        // 通过窗口句柄取得进程ID
        DWORD dwTheardID = ::GetWindowThreadProcessId(hwndWindow, &dwPID);
        if (dwTheardID != 0) {
            // 判断和参数传入的进程ID是否相等
            if (dwPID == getpid()) {
                // 进程ID相等，则记录窗口句柄
                hwndRet = hwndWindow;
                break;
            }
        }
        // 取得下一个窗口句柄
        hwndWindow = ::GetNextWindow(hwndWindow, GW_HWNDNEXT);
    }
    // 上面取得的窗口，不一定是最上层的窗口，需要通过GetParent获取最顶层窗口
    HWND hwndWindowParent = NULL;
    // 循环查找父窗口，以便保证返回的句柄是最顶层的窗口句柄
    while (hwndRet != NULL) {
        hwndWindowParent = ::GetParent(hwndRet);
        if (hwndWindowParent == NULL) {
            break;
        }
        hwndRet = hwndWindowParent;
    }
    // 返回窗口句柄
    return hwndRet;
}

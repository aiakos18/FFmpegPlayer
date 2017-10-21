#include "audiooutput_alsa.h"

AudioOutput_Alsa::AudioOutput_Alsa(QObject *parent)
    : AudioOutputBase(parent)
{

}

AudioOutput_Alsa::~AudioOutput_Alsa()
{
    if (isLoaded()) {
        unload();
    }

    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

void AudioOutput_Alsa::init()
{
    m_decoder.init();

    moveToThread(&m_thread);
    m_thread.start();
}

bool AudioOutput_Alsa::loadFile(const QString &file)
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

    if (!m_decoder.load(file, AVDecoder::DECODING_AUDIO)) {
        return false;
    }

    setFile(file);
    return true;
}

void AudioOutput_Alsa::unload()
{
    m_decoder.unload();
    m_taskid++;

    setFile(QString());
    setPlaybackState(false);
    setPosition(0.0);
}

bool AudioOutput_Alsa::isLoaded()
{
    return m_decoder.audio_hasStream();
}

QString AudioOutput_Alsa::getFile()
{
    return AudioOutputBase::getFile();
}

void AudioOutput_Alsa::play()
{
    QMetaObject::invokeMethod(this, "outputAudioData", Q_ARG(int,++m_taskid));
}

void AudioOutput_Alsa::pause()
{

}

bool AudioOutput_Alsa::isPlaying()
{
    return AudioOutputBase::isPlaying();
}

double AudioOutput_Alsa::getDuration()
{
    return m_decoder.audio_getDuration();
}

double AudioOutput_Alsa::getPosition()
{
    return AudioOutputBase::getPosition();
}

void AudioOutput_Alsa::seek(double pos)
{
    m_decoder.audio_seek(pos);
    m_mtx.lock();
    m_data.clear();
    m_ad.data.clear();
    m_ad.time = 0.0;
    m_ad.duration = 0.0;
    m_mtx.unlock();
}

void AudioOutput_Alsa::outputAudioData(int taskid)
{
    int sampleRate = m_decoder.audioSampleRate();
    int sampleSize = m_decoder.audioSampleSize();
    int channels = m_decoder.audioChannels();
    int bytesPerSample = m_decoder.audioBytesPerSample();
    int bytesPerFrame = m_decoder.audioBytesPerFrame();
    int bytesPerSecond = m_decoder.audioBytesPerSecond();
    qDebug("sample rate: %d, sample size: %d, channels: %d, bytes per sample: %d, bytes per frame: %d, bytes per second: %d",
           sampleRate, sampleSize, channels, bytesPerSample, bytesPerFrame, bytesPerSecond);

    AVSampleFormat sampleFormat = m_decoder.audioSampleFormat();
    snd_pcm_format_t pcmFormat;
    switch (sampleFormat) {
    case AV_SAMPLE_FMT_U8:
        pcmFormat = SND_PCM_FORMAT_U8;
        break;
    case AV_SAMPLE_FMT_S16:
        pcmFormat = SND_PCM_FORMAT_S16;
        break;
    case AV_SAMPLE_FMT_S32:
        pcmFormat = SND_PCM_FORMAT_S32;
        break;
    default:
        qDebug("cannot find avail snd sample format");
        return;
    }
    qDebug("snd format: %d", pcmFormat);

    snd_pcm_t *pcm = 0;
    int ret = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, /*SND_PCM_NONBLOCK*/0);
    if (ret < 0) {
        qDebug("snd_pcm_open(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }

    snd_pcm_hw_params_t *hwp;
    ret = snd_pcm_hw_params_malloc(&hwp);
    if (ret < 0) {
        qDebug("snd_pcm_hw_params_malloc(): errno: %d, %s", ret, snd_strerror(ret));
    }
    if ((ret = snd_pcm_hw_params_any(pcm, hwp)) < 0) {
        qDebug("snd_pcm_hw_params_any(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }
    if ((ret = snd_pcm_hw_params_set_access(pcm, hwp, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        qDebug("snd_pcm_hw_params_set_access(): errno: %d, %s, format: SND_PCM_ACCESS_RW_INTERLEAVED", ret, snd_strerror(ret));
        return;
    }

    unsigned int uival = sampleRate;
    if ((ret = snd_pcm_hw_params_set_rate_near(pcm, hwp, &uival, 0)) < 0) {
        qDebug("snd_pcm_hw_params_set_rate(): errno: %d, %s, rate: %d", ret, snd_strerror(ret), uival);
        return;
    }
    qDebug("set rate expect: %d, actual: %d", sampleRate, uival);

    uival = channels;
    if ((ret = snd_pcm_hw_params_set_channels_near(pcm, hwp, &uival)) < 0) {
        qDebug("snd_pcm_hw_params_set_channels(): errno: %d, %s, channels: %d", ret, snd_strerror(ret), channels);
        return;
    }
    qDebug("set channel expect: %d, actual: %d", channels, uival);

    if ((ret = snd_pcm_hw_params_set_format(pcm, hwp, pcmFormat)) < 0) {
        qDebug("snd_pcm_hw_params_set_format(): errno: %d, %s, format: %d", ret, snd_strerror(ret), pcmFormat);
        return;
    }

    uival = 20 * 1000;
    ret = snd_pcm_hw_params_set_buffer_time_near(pcm, hwp, &uival, 0);
    if (ret < 0) {
        qDebug() << "failed to set periods time" << snd_strerror(ret);
        return;
    }
    qDebug("set buffer time expect: %d, actual: %d", 20 * 1000, uival);

    uival = 1;
    ret = snd_pcm_hw_params_set_periods_near(pcm, hwp, &uival, 0);
    if (ret < 0) {
        qDebug() << "failed to set periods" << snd_strerror(ret);
        return;
    }
    qDebug("set periods expect: %d, actual: %d", 1, uival);

    if ((ret = snd_pcm_hw_params(pcm, hwp)) < 0) {
        qDebug("snd_pcm_hw_params(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }

    int dir = 0;
    snd_pcm_uframes_t ulval;
    ret = snd_pcm_hw_params_get_period_size(hwp, &ulval, &dir);
    if (ret < 0) {
        qDebug("failed to get period frames");
        return;
    }
    qDebug("period frames: %ld", ulval);
    snd_pcm_uframes_t periodFrames = ulval;

    ret = snd_pcm_hw_params_get_period_time(hwp, &uival, &dir);
    if (ret < 0) {
        qDebug("failed to get period time");
        return;
    }
    qDebug("period time: %d", uival);

    if ((ret = snd_pcm_hw_params_get_buffer_size(hwp, &ulval)) < 0) {
        qDebug("failed to get buffer frames");
        return;
    }
    qDebug("buffer frames: %ld", ulval);
    snd_pcm_uframes_t bufferFrames = ulval;

    if ((ret = snd_pcm_hw_params_get_buffer_time(hwp, &uival, &dir)) < 0) {
        qDebug("failed to get buffer time");
        return;
    }
    qDebug("buffer time: %d", uival);

    ret = snd_pcm_hw_params_get_periods(hwp, &uival, &dir);
    if (ret < 0) {
        qDebug("failed to get periods");
        return;
    }
    qDebug("periods: %d", uival);


    bool canPause = snd_pcm_hw_params_can_pause(hwp);
    qDebug("device %s pause", canPause ? "can" : "cannot");


    int framesOnceWrite = bufferFrames, bytesOnceWrite = framesOnceWrite * bytesPerFrame;
    qDebug() << "framesOnceWrite:" << framesOnceWrite << "bytesOnceWrite:" << bytesOnceWrite;

    snd_pcm_sw_params_t *swp;
    ret = snd_pcm_sw_params_malloc(&swp);
    if (ret < 0) {
        qDebug("snd_pcm_sw_params_malloc(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }
    ret = snd_pcm_sw_params_current(pcm, swp);
    if (ret < 0) {
        qDebug("snd_pcm_sw_params_current(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }
    ret = snd_pcm_sw_params_set_start_threshold(pcm, swp, 0);
    if (ret < 0) {
        qDebug("snd_pcm_sw_params_set_start_threshold(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }
//    ret = snd_pcm_sw_params_set_avail_min(pcm, swp, framesOnceWrite);
//    if (ret < 0) {
//        qDebug("snd_pcm_sw_params_set_avail_min(): errno: %d, %s", ret, snd_strerror(ret));
//        return;
//    }
    ret = snd_pcm_sw_params(pcm, swp);
    if (ret < 0) {
        qDebug("snd_pcm_sw_params(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }

    qDebug() << "alsa setup success";

    ret = snd_pcm_prepare(pcm);
    if (ret < 0) {
        qDebug("snd_pcm_prepare(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }
    ret = snd_pcm_start(pcm);
    if (ret < 0) {
        qDebug("snd_pcm_start(): errno: %d, %s", ret, snd_strerror(ret));
        return;
    }

    m_decoder.audio_setMinBufferSize(bytesOnceWrite);

    while (taskid == m_taskid) {
        if (!m_decoder.audio_hasBufferedData()) {
            if (m_decoder.decodingState() == AVDecoder::DECODING_END) {
                if (!m_data.isEmpty()) {
                    QByteArray zero(bytesOnceWrite - m_data.size(), 0);
                    m_data.append(zero);
                }
                else {
                    break;
                }
            }
            else {
                QThread::msleep(1);
                continue;
            }
        }

        AVDecoder::AudioData ad = m_decoder.audio_popBufferedData();
        if (m_data.isEmpty()) {
            setPosition(ad.time);
        }
        m_data.append(ad.data);

        while (m_data.size() >= bytesOnceWrite) {
//            int ret = snd_pcm_wait(pcm, 1000 * bytesOnceWrite / bytesPerSec);
//            if (ret == 0) {
////                qDebug() << "pcm wait timeout";
//                continue;
//            }
//            else if (ret != 1) {
//                qDebug() << "pcm wait unknown error";
//                continue;
//            }
//            snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
//            if (avail < 0) {
//                qDebug() << "failed to do snd_pcm_avail_update" << snd_strerror(avail);
////                snd_pcm_recover(pcm, avail, 0);
//                snd_pcm_prepare(pcm);
//                snd_pcm_start(pcm);
//                continue;
//            }
//            else if (avail < framesOnceWrite) {
////                qDebug() << "avail less than framesOnceWrite";
//                continue;
//            }
////            qDebug() << "avail:" << avail;

            snd_pcm_sframes_t actual = snd_pcm_writei(pcm, m_data.data(), framesOnceWrite);
            if (actual < 0) {
                qDebug() << "failed to snd_pcm_writei:" << snd_strerror(actual);
//                snd_pcm_recover(pcm, actual, 0);
                snd_pcm_prepare(pcm);
                snd_pcm_start(pcm);
            }
            else if (actual > 0) {
//                qDebug() << "write frames:" << actual;
                m_data.remove(0, actual * bytesPerFrame);
                setPosition(m_position + 1.0 * bytesOnceWrite / bytesPerSecond);
            }
            else {
                qDebug() << "write 0";
            }
        }
    }

    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
}

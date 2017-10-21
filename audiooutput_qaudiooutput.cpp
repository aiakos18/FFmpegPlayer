#include "audiooutput_qaudiooutput.h"

AudioOutput_QAudioOutput::AudioOutput_QAudioOutput(AVDecoderCore *decoder, QObject *parent)
    : m_decoder(decoder), AudioOutputBase(parent)
{
    moveToThread(&m_thread);
    if (isAvailable()) {
        m_thread.start();
    }
}

AudioOutput_QAudioOutput::~AudioOutput_QAudioOutput()
{
    if (isPlaying()) {

    }
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

bool AudioOutput_QAudioOutput::isAvailable()
{
    if (m_decoder == 0) {
        return false;
    }
    if (!m_decoder->audio_hasStream()) {
        return false;
    }
    return true;
}

void AudioOutput_QAudioOutput::play()
{
    QMetaObject::invokeMethod(this, "outputAudioData", Q_ARG(int,++m_taskid));
}

void AudioOutput_QAudioOutput::pause()
{

}

bool AudioOutput_QAudioOutput::isPlaying()
{
    return AudioOutputBase::isPlaying();
}

double AudioOutput_QAudioOutput::getPosition()
{
    return AudioOutputBase::getPosition();
}

void AudioOutput_QAudioOutput::seek(double pos)
{
    m_decoder.audio_seek(pos);
    m_mtx.lock();
    m_data.clear();
    m_ad.data.clear();
    m_ad.time = 0.0;
    m_ad.duration = 0.0;
    m_mtx.unlock();
}

void AudioOutput_QAudioOutput::outputAudioData(int taskid)
{
    int sampleRate = m_decoder.audio_getSampleRate();
    int sampleSize = m_decoder.audio_getSampleSize();
    int channels = m_decoder.audio_getChannels();
    int bytesPerSample = m_decoder.audio_getBytesPerSample();
    int bytesPerFrame = m_decoder.audio_getBytesPerFrame();
    int bytesPerSecond = m_decoder.audio_getBytesPerSecond();
    qDebug("sample rate: %d, sample size: %d, channels: %d, bytes per sample: %d, bytes per frame: %d, bytes per second: %d",
           sampleRate, sampleSize, channels, bytesPerSample, bytesPerFrame, bytesPerSecond);

    QAudioFormat audioFormat;
    audioFormat.setCodec("audio/pcm");
    audioFormat.setSampleRate(sampleRate);
    audioFormat.setChannelCount(channels);
    audioFormat.setSampleSize(sampleSize);
    switch (m_decoder.audio_getSampleFormat()) {
    case AV_SAMPLE_FMT_U8:
        audioFormat.setSampleType(QAudioFormat::UnSignedInt);
        break;
    case AV_SAMPLE_FMT_S16:
        audioFormat.setSampleType(QAudioFormat::SignedInt);
        break;
    case AV_SAMPLE_FMT_S32:
        audioFormat.setSampleType(QAudioFormat::SignedInt);
        break;
    default:
        qDebug("cannot find avail qt audio sample format");
        return;
    }

    if (!audioFormat.isValid()) {
        qDebug() << "audio output format is not valid";
        return;
    }

    QAudioOutput *audioOutput = new QAudioOutput(audioFormat);
    QIODevice *ioDevice = audioOutput->start();
    m_decoder.audio_setMinBufferSize(audioOutput->periodSize());
    while (taskid == m_taskid) {
        if (!m_decoder.audio_hasBufferedData()) {
            if (m_decoder.decodingState() == AVDecoder::DECODING_END) {
                if (!m_data.isEmpty()) {
                    QByteArray zero(audioOutput->periodSize() - m_data.size(), 0);
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
        m_data.append(ad.data);

        while (m_data.size() >= audioOutput->periodSize()) {
            int expect = audioOutput->periodSize();
            int actual = ioDevice->write(m_data.data(), expect);
//            qDebug() << "expect:" << expect << "actual:" << actual << "avail:" << m_output->bytesFree();
            if (actual > 0) {
                m_data.remove(0, actual);
                double duration = audioFormat.durationForBytes(actual)/1000000.0;
                QThread::msleep(0.9*duration*1000);
                setPosition(m_position + duration);
            }
            else if (actual == 0) {
                QThread::msleep(1);
            }
            else {
                qDebug() << "write audio data to device error";
                audioOutput->reset();
            }
        }
    }

    ioDevice->close();
    ioDevice = 0;
    delete audioOutput;
    audioOutput = 0;
    setPosition(0);
}

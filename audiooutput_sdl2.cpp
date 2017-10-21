#include "audiooutput_sdl2.h"

AudioOutput_SDL2::AudioOutput_SDL2(QObject *parent)
    : AudioOutputBase(parent)
{

}

AudioOutput_SDL2::~AudioOutput_SDL2()
{
    if (isLoaded()) {
        unload();
    }

    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

void AudioOutput_SDL2::init()
{
    m_decoder.init();

    moveToThread(&m_thread);
    m_thread.start();
}

bool AudioOutput_SDL2::loadFile(const QString &file)
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

void AudioOutput_SDL2::unload()
{
    m_decoder.unload();
    m_taskid++;
    SDL_CloseAudio();

    setFile(QString());
    setPlaybackState(false);
    setPosition(0.0);
}

bool AudioOutput_SDL2::isLoaded()
{
    return m_decoder.audio_hasStream();
}

QString AudioOutput_SDL2::getFile()
{
    return AudioOutputBase::getFile();
}

void AudioOutput_SDL2::play()
{
    if (SDL_GetAudioStatus() == SDL_AUDIO_STOPPED) {
        QMetaObject::invokeMethod(this, "outputAudioData_SDL", Q_ARG(int,++m_taskid));
    }
    else {
        SDL_PauseAudio(0);
    }
}

void AudioOutput_SDL2::pause()
{
    SDL_PauseAudio(1);
}

bool AudioOutput_SDL2::isPlaying()
{
    return AudioOutputBase::isPlaying();
}

double AudioOutput_SDL2::getDuration()
{
    return m_decoder.audio_getDuration();
}

double AudioOutput_SDL2::getPosition()
{
    return AudioOutputBase::getPosition();
}

void AudioOutput_SDL2::seek(double pos)
{
    m_decoder.audio_seek(pos);
    m_mtx.lock();
    m_data.clear();
    m_ad.data.clear();
    m_ad.time = 0.0;
    m_ad.duration = 0.0;
    m_mtx.unlock();
}

void AudioOutput_SDL2::outputAudioData(int taskid)
{
    qDebug() << "outputAudioData_SDL in";

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        int r = SDL_Init(SDL_INIT_AUDIO);
        if (r < 0) {
            qDebug() << "failed to init sdl audio" << SDL_GetError();
            return;
        }
    }

    int sampleRate = m_decoder.audioSampleRate();
    int sampleSize = m_decoder.audioSampleSize();
    int channels = m_decoder.audioChannels();
    int bytesPerSample = m_decoder.audioBytesPerSample();
    int bytesPerFrame = m_decoder.audioBytesPerFrame();
    int bytesPerSecond = m_decoder.audioBytesPerSecond();
    qDebug("sample rate: %d, sample size: %d, channels: %d, bytes per sample: %d, bytes per frame: %d, bytes per second: %d",
           sampleRate, sampleSize, channels, bytesPerSample, bytesPerFrame, bytesPerSecond);

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = sampleRate;
    wanted_spec.channels = channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024/8;
//    wanted_spec.size = 1024 * bytesPerSample;
    wanted_spec.callback = onSdlCallback;
    wanted_spec.userdata = this;
    switch (m_decoder.audioSampleFormat()) {
    case AV_SAMPLE_FMT_U8:
        wanted_spec.format = AUDIO_U8;
        break;
    case AV_SAMPLE_FMT_S16:
        wanted_spec.format = AUDIO_S16;
        break;
    case AV_SAMPLE_FMT_S32:
        wanted_spec.format = AUDIO_S32;
        break;
    default:
        qDebug("cannot find avail sdl sample format");
        return;
    }

    SDL_AudioSpec actual;
    if (SDL_OpenAudio(&wanted_spec, &actual) < 0) {
        qDebug("can't open sdl audio, %s", SDL_GetError());
        return;
    }
    qDebug("actual sample rate: %d, sample format: %d, channels: %d, samples: %d, size: %d",
           actual.freq, actual.format, actual.channels, actual.samples, actual.size);

    int bufferSize = actual.size;
    m_decoder.audio_setMinBufferSize(bufferSize * 10);

    SDL_PauseAudio(0);
}

void AudioOutput_SDL2::onSdlCallback(void *userdata, uint8_t *stream, int len)
{
    AudioOutput *_this = (AudioOutput*)userdata;
    _this->doSdlCallback(stream, len);
}

void AudioOutput_SDL2::doSdlCallback(uint8_t *stream, int len)
{
    if (m_ad.data.isEmpty() && !m_decoder.audio_hasBufferedData()
            && (m_decoder.decodingState() == AVDecoder::DECODING_END
                || m_decoder.decodingState() == AVDecoder::DECODING_NOT_START)) {
        SDL_PauseAudio(1);
//        SDL_CloseAudio();
        return;
    }

    SDL_memset(stream, 0, len);
    double pos = m_position;
    int r = 0;
    m_mtx.lock();
    while (r < len) {
        if (!m_ad.data.isEmpty()) {
            int cp = qMin(m_ad.data.size(), len - r);
            SDL_MixAudio(stream + r, (uint8_t*)m_ad.data.data(), cp, SDL_MIX_MAXVOLUME);
            r += cp;
            double dur = m_ad.duration * cp / m_ad.data.size();
            m_ad.data.remove(0, cp);
            m_ad.time += dur;
            m_ad.duration -= dur ;
            pos = m_ad.time;
            continue;
        }

        if (!m_decoder.audio_hasBufferedData()) {
//            qDebug() << "decoder has no audio data";
            break;
        }
        m_ad = m_decoder.audio_popBufferedData();
    }
    m_mtx.unlock();
    setPosition(pos);
}

#include "avdecodercore.h"

static char *iav_err2str(int eid)
{
    static QByteArray s;
    s.resize(128);
    return av_make_error_string(s.data(), 128, eid);
}

static void deleteAVPacket(AVPacket *pkt)
{
    av_packet_unref(pkt);
    delete pkt;
}

static void freeAVFarme(AVFrame *frame)
{
    av_frame_free(&frame);
}

static void freeSwrContext(SwrContext *cxt)
{
    swr_free(&cxt);
}

AVDecoderCore::AVDecoderCore()
    : m_formatContext(0)
{
    qRegisterMetaType<SPAVFrame>("SPAVFrame");
}

AVDecoderCore::~AVDecoderCore()
{
    unload();
}

bool AVDecoderCore::load(const QString &file)
{
    if (!QFile::exists(file)) {
        return false;
    }

    if (file == m_file) {
        return true;
    }

    unload();

    av_register_all();
    m_formatContext = avformat_alloc_context();
    int result = avformat_open_input(&m_formatContext, file.toStdString().c_str(),NULL,NULL);
    if (result < 0){
        qDebug() <<  __PRETTY_FUNCTION__ << "failed to do avformat_open_input";
        unload();
        return false;
    }

    //获取媒体流信息
    result = avformat_find_stream_info(m_formatContext,NULL);
    if (result < 0){
        qDebug() <<  __PRETTY_FUNCTION__ << "failed to do avformat_find_stream_info";
        unload();
        return false;
    }

    for (int i = 0; i < m_formatContext->nb_streams; ++i) {
        AVStream *stream = m_formatContext->streams[i];
        AVCodecParameters *codecPar = stream->codecpar;
        AVMediaType type = codecPar->codec_type;

        if (type == AVMEDIA_TYPE_AUDIO) {
            AudioStreamParty *asp = new AudioStreamParty;
            m_audioStreamParties.append(asp);

            asp->streamParty.streamIndex = i;
            asp->streamParty.streamType = type;

            if (stream->duration > 0) {
                asp->duration = stream->duration * av_q2d(stream->time_base);
            }
            else if (m_formatContext->duration > 0) {
                asp->duration = m_formatContext->duration * av_q2d(AV_TIME_BASE_Q);
            }
            else {
                asp->duration = 0.0;
            }

            asp->bitrate = codecPar->bit_rate;
            asp->frameSize = codecPar->frame_size;

            AVSampleFormat inputSampleFormat = (AVSampleFormat)codecPar->format;
            AVSampleFormat outputSampleFormat = inputSampleFormat;
            if (av_sample_fmt_is_planar(inputSampleFormat)) {
                outputSampleFormat = av_get_packed_sample_fmt(inputSampleFormat);
            }
            if (outputSampleFormat == AV_SAMPLE_FMT_FLT
                    || outputSampleFormat == AV_SAMPLE_FMT_DBL
                    || outputSampleFormat == AV_SAMPLE_FMT_S64) {
                outputSampleFormat = AV_SAMPLE_FMT_S32;
            }

            int inputChannels = codecPar->channels;
            uint64_t inputChannelLayout = codecPar->channel_layout;
            if (inputChannels > 0 && inputChannelLayout == 0) {
                inputChannelLayout = av_get_default_channel_layout(inputChannels);
            }
            else if (inputChannelLayout > 0 && inputChannels == 0) {
                inputChannels = av_get_channel_layout_nb_channels(inputChannelLayout);
            }
            int outputChannels = (inputChannels > 2) ? 2 : inputChannels;
            uint64_t outputChannelLayout = (inputChannels > 2) ? AV_CH_LAYOUT_STEREO : inputChannelLayout;

            asp->inputSampleFormat = inputSampleFormat;
            asp->inputBytesPerSample = av_get_bytes_per_sample(inputSampleFormat);
            asp->inputSampleSize = asp->inputBytesPerSample * 8;
            asp->inputSampleRate = codecPar->sample_rate;
            asp->inputChannels = codecPar->channels;
            asp->inputChannelLayout = codecPar->channel_layout;
            asp->inputBytesPerFrame = asp->inputBytesPerSample * asp->inputChannels;
            asp->inputBytesPerSecond = asp->inputBytesPerFrame * asp->inputSampleRate;

            asp->outputSampleFormat = outputSampleFormat;
            asp->outputBytesPerSample = av_get_bytes_per_sample(outputSampleFormat);
            asp->outputSampleSize = asp->outputBytesPerSample * 8;
            asp->outputSampleRate = codecPar->sample_rate;
            asp->outputChannels = outputChannels;
            asp->outputChannelLayout = outputChannelLayout;
            asp->outputBytesPerFrame = asp->outputBytesPerSample * asp->outputChannels;
            asp->outputBytesPerSecond = asp->outputBytesPerFrame * asp->outputSampleRate;

            if (stream->metadata != 0) {
                AVDictionaryEntry *t = NULL;
                while ((t = av_dict_get(stream->metadata, "", t, AV_DICT_IGNORE_SUFFIX))) {
                    asp->metadata.insert(t->key, t->value);
                }
            }
        }
        else if (type == AVMEDIA_TYPE_VIDEO) {
            if (stream->attached_pic.data == 0) {
                VideoStreamParty *vsp = new VideoStreamParty;
                m_videoStreamParties.append(vsp);

                vsp->streamParty.streamIndex = i;
                vsp->streamParty.streamType = type;
                vsp->streamParty2.streamIndex = i;
                vsp->streamParty2.streamType = type;

                if (stream->duration > 0) {
                    vsp->duration = stream->duration * av_q2d(stream->time_base);
                }
                else if (m_formatContext->duration > 0) {
                    vsp->duration = m_formatContext->duration * av_q2d(AV_TIME_BASE_Q);
                }
                else {
                    vsp->duration = 0.0;
                }

                vsp->bitrate = codecPar->bit_rate;
                vsp->frameRate = av_q2d(av_stream_get_r_frame_rate(stream));
                vsp->width = codecPar->width;
                vsp->height = codecPar->height;
                vsp->format = (AVPixelFormat)codecPar->format;

                if (stream->metadata != 0) {
                    AVDictionaryEntry *t = NULL;
                    while ((t = av_dict_get(stream->metadata, "", t, AV_DICT_IGNORE_SUFFIX))) {
                        vsp->metadata.insert(t->key, t->value);
                    }
                }
            }
            else {
                CoverStreamParty *csp = new CoverStreamParty;
                m_coverStreamParties.append(csp);

                csp->streamParty.streamIndex = i;
                csp->streamParty.streamType = type;

                SPAVFrame cover;
                if (getCover(m_formatContext, i, cover)) {
                    csp->cover = cover;
                }

                if (stream->metadata != 0) {
                    AVDictionaryEntry *t = NULL;
                    while ((t = av_dict_get(stream->metadata, "", t, AV_DICT_IGNORE_SUFFIX))) {
                        csp->metadata.insert(t->key, t->value);
                    }
                }
            }
        }
        else if (type == AVMEDIA_TYPE_SUBTITLE) {
            SubtitleStreamParty *ssp = new SubtitleStreamParty;
            m_subtitleStreamParties.append(ssp);

            ssp->streamParty.streamIndex = i;
            ssp->streamParty.streamType = type;

            if (stream->metadata != 0) {
                AVDictionaryEntry *t = NULL;
                while ((t = av_dict_get(stream->metadata, "", t, AV_DICT_IGNORE_SUFFIX))) {
                    ssp->metadata.insert(t->key, t->value);
                }
            }
        }
    }

    avformat_close_input(&m_formatContext);
    avformat_free_context(m_formatContext);
    m_formatContext = 0;

    m_file = file;
    return true;
}

void AVDecoderCore::unload()
{
    for (int i = 0; i < m_audioStreamParties.count(); ++i) {
        disableAudioStream(i);
        delete m_audioStreamParties[i];
    }
    m_audioStreamParties.clear();

    for (int i = 0; i < m_videoStreamParties.count(); ++i) {
        disableVideoStream(i);
        delete m_videoStreamParties[i];
    }
    m_videoStreamParties.clear();

    for (int i = 0; i < m_coverStreamParties.count(); ++i) {
        delete m_coverStreamParties[i];
    }
    m_coverStreamParties.clear();

    for (int i = 0; i < m_subtitleStreamParties.count(); ++i) {
        delete m_subtitleStreamParties[i];
    }
    m_subtitleStreamParties.clear();

    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        avformat_free_context(m_formatContext);
        m_formatContext = 0;
    }

    m_file.clear();
}

bool AVDecoderCore::isLoaded()
{
    return !m_file.isEmpty();
}

QString AVDecoderCore::getFile()
{
    return m_file;
}

void AVDecoderCore::showInfo()
{
    if (!isLoaded()) {
        return;
    }

    foreach (AudioStreamParty *asp, m_audioStreamParties) {
        qDebug() << "stream" << asp->streamParty.streamIndex << ":"
                 << av_get_media_type_string(asp->streamParty.streamType);

        qDebug() << "audio duration:" << asp->duration;
        qDebug() << "audio bit rate:" << asp->bitrate;
        qDebug() << "audio frame_size:" << asp->frameSize;

        qDebug() << "audio input sample format:" << asp->inputSampleFormat << av_get_sample_fmt_name(asp->inputSampleFormat);
        qDebug() << "audio input sample size:" << asp->inputSampleSize;
        qDebug() << "audio input sample rate:" << asp->inputSampleRate;
        qDebug() << "audio input channels:" << asp->inputChannels;

        char channelLayoutName[32];
        memset(channelLayoutName, 0, 32);
        av_get_channel_layout_string(channelLayoutName, 32, asp->inputChannels, asp->inputChannelLayout);
        qDebug() << "audio input channel_layout:" << asp->inputChannelLayout << channelLayoutName;

        qDebug() << "audio input bytes per sample:" << asp->inputBytesPerSample;
        qDebug() << "audio input bytes per frame:" << asp->inputBytesPerFrame;
        qDebug() << "audio input bytes per second:" << asp->inputBytesPerSecond;

        qDebug() << "audio output sample format:" << asp->outputSampleFormat << av_get_sample_fmt_name(asp->outputSampleFormat);
        qDebug() << "audio output sample size:" << asp->outputSampleSize;
        qDebug() << "audio output sample rate:" << asp->outputSampleRate;
        qDebug() << "audio output channels:" << asp->outputChannels;

        memset(channelLayoutName, 0, 32);
        av_get_channel_layout_string(channelLayoutName, 32, asp->outputChannels, asp->outputChannelLayout);
        qDebug() << "audio output channel_layout:" << asp->outputChannelLayout << channelLayoutName;

        qDebug() << "audio output bytes per sample:" << asp->outputBytesPerSample;
        qDebug() << "audio output bytes per frame:" << asp->outputBytesPerFrame;
        qDebug() << "audio output bytes per second:" << asp->outputBytesPerSecond;

        foreach (QString key, asp->metadata.keys()) {
            qDebug() << key << ":" << asp->metadata[key];
        }
        qDebug("");
    }

    foreach (VideoStreamParty *vsp, m_videoStreamParties) {
        qDebug() << "stream" << vsp->streamParty.streamIndex << ":"
                 << av_get_media_type_string(vsp->streamParty.streamType);

        qDebug() << "video duration:" << vsp->duration;
        qDebug() << "video bit rate:" << vsp->bitrate;
        qDebug() << "video frame rate:" << vsp->frameRate;

        qDebug() << "video width:" << vsp->width;
        qDebug() << "video height:" << vsp->height;
        qDebug() << "video format:" << vsp->format << av_get_pix_fmt_name(vsp->format);

        foreach (QString key, vsp->metadata.keys()) {
            qDebug() << key << ":" << vsp->metadata[key];
        }
        qDebug("");
    }

    foreach (CoverStreamParty *csp, m_coverStreamParties) {
        qDebug() << "stream" << csp->streamParty.streamIndex << ":"
                 << av_get_media_type_string(csp->streamParty.streamType);

        if (!csp->cover.isNull()) {
            qDebug() << "cover width:" << csp->cover->width;
            qDebug() << "cover height:" << csp->cover->height;
            qDebug() << "cover format:" << csp->cover->format << av_get_pix_fmt_name((AVPixelFormat)csp->cover->format);
        }

        foreach (QString key, csp->metadata.keys()) {
            qDebug() << key << ":" << csp->metadata[key];
        }
        qDebug("");
    }

    foreach (SubtitleStreamParty *ssp, m_subtitleStreamParties) {
        qDebug() << "stream" << ssp->streamParty.streamIndex << ":"
                 << av_get_media_type_string(ssp->streamParty.streamType);

        foreach (QString key, ssp->metadata.keys()) {
            qDebug() << key << ":" << ssp->metadata[key];
        }
        qDebug("");
    }
}

bool AVDecoderCore::hasAudioStream()
{
    return !m_audioStreamParties.isEmpty();
}

int AVDecoderCore::getAudioStreamCount()
{
    return m_audioStreamParties.count();
}

QString AVDecoderCore::getAudioStreamTitle(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return QString();
    }
    return m_audioStreamParties[index]->metadata["title"];
}

bool AVDecoderCore::enableAudioStream(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return false;
    }
    if (isAudioStreamEnabled(index)) {
        return true;
    }

    if (m_file.isEmpty()) {
        return false;
    }

    AudioStreamParty *asp = m_audioStreamParties[index];
    if (!initStreamParty(&(asp->streamParty), m_file)) {
        return false;
    }
    return true;
}

void AVDecoderCore::disableAudioStream(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return;
    }
    if (!isAudioStreamEnabled(index)) {
        return;
    }

    AudioStreamParty *asp = m_audioStreamParties[index];
    uninitStreamParty(&(asp->streamParty));
}

bool AVDecoderCore::isAudioStreamEnabled(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return false;
    }
    return (m_audioStreamParties[index]->streamParty.formatContext != 0);
}

bool AVDecoderCore::hasEnabledAudioStream()
{
    for (int i = 0; i < m_audioStreamParties.count(); ++i) {
        if (isAudioStreamEnabled(i)) {
            return true;
        }
    }
    return false;
}

int AVDecoderCore::getEnabledAudioStreamCount()
{
    int count = 0;
    for (int i = 0; i < m_audioStreamParties.count(); ++i) {
        if (isAudioStreamEnabled(i)) {
            count++;
        }
    }
    return count;
}

QList<int> AVDecoderCore::getEnabledAudioIndexes()
{
    QList<int> indexes;
    for (int i = 0; i < m_audioStreamParties.count(); ++i) {
        if (isAudioStreamEnabled(i)) {
            indexes.append(i);
        }
    }
    return indexes;
}

double AVDecoderCore::getAudioDuration(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0.0;
    }
    return m_audioStreamParties[index]->duration;
}

AVSampleFormat AVDecoderCore::getAudioSampleFormat(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return AV_SAMPLE_FMT_NONE;
    }
    return m_audioStreamParties[index]->inputSampleFormat;
}

int AVDecoderCore::getAudioSampleSize(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputSampleSize;
}

int AVDecoderCore::getAudioSampleRate(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputSampleRate;
}

int AVDecoderCore::getAudioChannels(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputChannels;
}

uint64_t AVDecoderCore::getAudioChannelLayout(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputChannelLayout;
}

int AVDecoderCore::getAudioBytesPerSample(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputBytesPerSample;
}

int AVDecoderCore::getAudioBytesPerFrame(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputBytesPerFrame;
}

int AVDecoderCore::getAudioBytesPerSecond(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->inputBytesPerSecond;
}

AVSampleFormat AVDecoderCore::getAudioOutputSampleFormat(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return AV_SAMPLE_FMT_NONE;
    }
    return m_audioStreamParties[index]->outputSampleFormat;
}

int AVDecoderCore::getAudioOutputSampleSize(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputSampleSize;
}

int AVDecoderCore::getAudioOutputSampleRate(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputSampleRate;
}

int AVDecoderCore::getAudioOutputChannels(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputChannels;
}

uint64_t AVDecoderCore::getAudioOutputChannelLayout(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputChannelLayout;
}

int AVDecoderCore::getAudioOutputBytesPerSample(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputBytesPerSample;
}

int AVDecoderCore::getAudioOutputBytesPerFrame(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputBytesPerFrame;
}

int AVDecoderCore::getAudioOutputBytesPerSecond(int index)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return 0;
    }
    return m_audioStreamParties[index]->outputBytesPerSecond;
}

bool AVDecoderCore::seekAudio(int index, double pos)
{
    if (index < 0 || index >= m_audioStreamParties.count()) {
        return false;
    }
    if (!isAudioStreamEnabled(index)) {
        return false;
    }

    if (pos < 0) {
        return false;
    }
    
    double duration = getAudioDuration(index);
    if (pos > duration) {
        return false;
    }

    StreamParty &sp = m_audioStreamParties[index]->streamParty;
    avcodec_flush_buffers(sp.codecContext);
    int averr = av_seek_frame(sp.formatContext,
                              sp.streamIndex,
                              pos / av_q2d(sp.stream->time_base),
                              AVSEEK_FLAG_BACKWARD);
    if (averr < 0) {
        qDebug() <<  __PRETTY_FUNCTION__ << "failed to do av_seek_frame" << averr << iav_err2str(averr) << pos;
        return false;
    }
    return true;
}

int AVDecoderCore::getAudioNextFrame(int index, QByteArray &data, double &pts, double &duration)
{
    int averr = AVERROR_UNKNOWN;
    if (index < 0 || index > m_audioStreamParties.count()) {
        return averr;
    }
    if (!isAudioStreamEnabled(index)) {
        return averr;
    }

    data.clear();
    pts = 0.0;
    duration = 0.0;
    StreamParty &sp = m_audioStreamParties[index]->streamParty;
    while (1) {
        AVPacket *avPacket = new AVPacket;
        QSharedPointer<AVPacket> spAVPacket(avPacket, deleteAVPacket);
        av_init_packet(avPacket);

        if ((averr = av_read_frame(sp.formatContext, avPacket)) != 0) {
            if (averr == AVERROR_EOF) {
                qDebug() <<  __PRETTY_FUNCTION__ << "decode end of file";
            }
            else {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to read frame," << averr << iav_err2str(averr);
            }
            return averr;
        }

        if (avPacket->stream_index != sp.streamIndex) {
            continue;
        }

//        qDebug() <<  __PRETTY_FUNCTION__
//                 << "packet pts:" << av_q2d(m_audioStreamParty->streamParty.stream->time_base) * avPacket->pts
//                 << "dts:" << av_q2d(m_audioStreamParty->streamParty.stream->time_base) * avPacket->dts
//                 << "flags:" << avPacket->flags;

        if ((averr = avcodec_send_packet(sp.codecContext, avPacket)) != 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to send packet," << averr << iav_err2str(averr);
            continue;
        }

        AVFrame *pAVFrame = av_frame_alloc();
        SPAVFrame spAVFrame(pAVFrame, freeAVFarme);
        while (1) {
            if ((averr = avcodec_receive_frame(sp.codecContext, pAVFrame)) != 0) {
                if (averr == AVERROR(EAGAIN)) {
                    break;
                }
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to receive frame," << averr << iav_err2str(averr);
                if (averr == AVERROR_EOF) {
                    return data.size() > 0;
                }
                return averr;
            }

//            qDebug() <<  __PRETTY_FUNCTION__
//                     << "frame  pts:" << av_q2d(m_audioStreamParty->streamParty.stream->time_base) * pAVFrame->pts
//                     << "duration:" << av_q2d(m_audioStreamParty->streamParty.stream->time_base) * pAVFrame->pkt_duration
//                     << "samples:" << pAVFrame->nb_samples;

            if (pAVFrame->channels > 0 && pAVFrame->channel_layout == 0) {
                pAVFrame->channel_layout = av_get_default_channel_layout(pAVFrame->channels);
            }
            else if (pAVFrame->channel_layout > 0 && pAVFrame->channels == 0) {
                pAVFrame->channels = av_get_channel_layout_nb_channels(pAVFrame->channel_layout);
            }

            int outputChannels = (pAVFrame->channels > 2) ? 2 : pAVFrame->channels;
            uint64_t outputChannelLayout = (pAVFrame->channels > 2) ? AV_CH_LAYOUT_STEREO : pAVFrame->channel_layout;

            SwrContext *swr = swr_alloc();
            if (!swr) {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to alloc SwrContext";
                return AVERROR(ENOMEM);
            }
            QSharedPointer<SwrContext> spSwr(swr, freeSwrContext);

            av_opt_set_channel_layout(swr, "in_channel_layout",  pAVFrame->channel_layout, 0);
            av_opt_set_channel_layout(swr, "out_channel_layout", outputChannelLayout,  0);
            av_opt_set_int(swr, "in_sample_rate", pAVFrame->sample_rate, 0);
            av_opt_set_int(swr, "out_sample_rate", pAVFrame->sample_rate, 0);
            av_opt_set_sample_fmt(swr, "in_sample_fmt",  (AVSampleFormat)sp.codecPar->format, 0);
            av_opt_set_sample_fmt(swr, "out_sample_fmt", m_audioStreamParties[index]->outputSampleFormat, 0);

//            SwrContext *swr = swr_alloc_set_opts(NULL,  // we're allocating a new context
//                                pAVFrame->channel_layout,               // out_ch_layout
//                                m_audioOutSampleFmt,                    // out_sample_fmt
//                                pAVFrame->sample_rate,                  // out_sample_rate
//                                pAVFrame->channel_layout,               // in_ch_layout
//                                (AVSampleFormat)pAVFrame->format,       // in_sample_fmt
//                                pAVFrame->sample_rate,                  // in_sample_rate
//                                0,                    // log_offset
//                                NULL);                // log_ctx

            if ((averr = swr_init(swr)) < 0) {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to init swr," << averr << iav_err2str(averr);
                return AVERROR_UNKNOWN;
            }

            AVFrame *pAVFrameOut = av_frame_alloc();
            QSharedPointer<AVFrame> spAVFrameOut(pAVFrameOut, freeAVFarme);

            int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr, pAVFrame->sample_rate) + pAVFrame->nb_samples,
                                                pAVFrame->sample_rate, pAVFrame->sample_rate, AV_ROUND_INF);
            pAVFrameOut->format = m_audioStreamParties[index]->outputSampleFormat;
            pAVFrameOut->nb_samples = dst_nb_samples;
            pAVFrameOut->channel_layout = outputChannelLayout;
            if ((averr = av_frame_get_buffer(pAVFrameOut, 1)) != 0) {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to get frame buffer," << averr << iav_err2str(averr);
                return AVERROR_UNKNOWN;
            }

            int nb = swr_convert(swr, pAVFrameOut->data, dst_nb_samples, (const uint8_t**)pAVFrame->data, pAVFrame->nb_samples);
            int data_size = outputChannels * nb * av_get_bytes_per_sample(m_audioStreamParties[index]->outputSampleFormat);
            data += QByteArray((const char*)pAVFrameOut->data[0], data_size);
            if (pts == 0.0) {
                pts = av_q2d(sp.stream->time_base) * pAVFrame->pts;
            }
            duration += av_q2d(sp.stream->time_base) * pAVFrame->pkt_duration;
        }
        if (data.size() > 0) {
            break;
        }
    }
    return 0;
}

bool AVDecoderCore::hasVideoStream()
{
    return !m_videoStreamParties.isEmpty();
}

int AVDecoderCore::getVideoStreamCount()
{
    return m_videoStreamParties.count();
}

bool AVDecoderCore::enableVideoStream(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return false;
    }
    if (isVideoStreamEnabled(index)) {
        return true;
    }

    if (m_file.isEmpty()) {
        return false;
    }

    VideoStreamParty *vsp = m_videoStreamParties[index];
    if (!initStreamParty(&(vsp->streamParty), m_file)) {
        return false;
    }
    if (!initStreamParty(&(vsp->streamParty2), m_file)) {
        uninitStreamParty(&(vsp->streamParty));
        return false;
    }
    return true;
}

void AVDecoderCore::disableVideoStream(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return;
    }
    if (!isVideoStreamEnabled(index)) {
        return;
    }

    VideoStreamParty *vsp = m_videoStreamParties[index];
    uninitStreamParty(&(vsp->streamParty));
    uninitStreamParty(&(vsp->streamParty2));
}

bool AVDecoderCore::isVideoStreamEnabled(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return false;
    }
    return (m_videoStreamParties[index]->streamParty.formatContext != 0);
}

bool AVDecoderCore::hasEnabledVideoStream()
{
    for (int i = 0; i < m_videoStreamParties.count(); ++i) {
        if (isVideoStreamEnabled(i)) {
            return true;
        }
    }
    return false;
}

int AVDecoderCore::getEnabledVideoStreamCount()
{
    int count = 0;
    for (int i = 0; i < m_videoStreamParties.count(); ++i) {
        if (isVideoStreamEnabled(i)) {
            count++;
        }
    }
    return count;
}

QList<int> AVDecoderCore::getEnabledVideoIndexes()
{
    QList<int> indexes;
    for (int i = 0; i < m_videoStreamParties.count(); ++i) {
        if (isVideoStreamEnabled(i)) {
            indexes.append(i);
        }
    }
    return indexes;
}

double AVDecoderCore::getVideoDuration(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return 0.0;
    }
    return m_videoStreamParties[index]->duration;
}

double AVDecoderCore::getVideoFrameRate(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return 0.0;
    }
    return m_videoStreamParties[index]->frameRate;
}

int AVDecoderCore::getVideoWidth(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return 0;
    }
    return m_videoStreamParties[index]->width;
}

int AVDecoderCore::getVideoHeight(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return 0;
    }
    return m_videoStreamParties[index]->height;
}

AVPixelFormat AVDecoderCore::getVideoPixelFormat(int index)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return AV_PIX_FMT_NONE;
    }
    return m_videoStreamParties[index]->format;
}

bool AVDecoderCore::seekVideo(int index, double pos, AVDecoderCore::SEEK_Type type)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return false;
    }
    if (!isVideoStreamEnabled(index)) {
        return false;
    }
    if (pos < 0) {
        return false;
    }
    double duration = getVideoDuration(index);
    if (pos > duration) {
        return false;
    }

    StreamParty &sp = m_videoStreamParties[index]->streamParty;
    avcodec_flush_buffers(sp.codecContext);
    int averr = AVERROR_UNKNOWN;
    if (type == SEEK_USER_SET) {
        if ((averr = av_seek_frame(sp.formatContext,
                                   sp.streamIndex,
                                   pos / av_q2d(sp.stream->time_base),
                                   AVSEEK_FLAG_BACKWARD)) < 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to do av_seek_frame, pos:" << pos << "flag:" << AVSEEK_FLAG_BACKWARD;
            return false;
        }
        SPAVFrame frame;
        if ((averr = getVideoNextFrame(index, frame, pos)) < 0) {
            if (averr == AVERROR_EOF) {
                qDebug("%s cannot seek to %f, try to seek %f",  __PRETTY_FUNCTION__, pos, pos - 1);
                return seekVideo(pos - 1, type);
            }
            return false;
        }
    }
    else if (type == SEEK_LEFT_KEY) {
        if ((averr = av_seek_frame(sp.formatContext,
                                   sp.streamIndex,
                                   pos / av_q2d(sp.stream->time_base),
                                   AVSEEK_FLAG_BACKWARD)) < 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to do av_seek_frame, pos:" << pos << "flag:" << AVSEEK_FLAG_BACKWARD;
            return false;
        }
        SPAVFrame frame;
        if ((averr = getVideoNextFrame(index, frame)) < 0) {
            if (averr == AVERROR_EOF) {
                qDebug("%s cannot seek to %f, try to seek %f",  __PRETTY_FUNCTION__, pos, pos - 1);
                return seekVideo(pos - 1, type);
            }
            return false;
        }
    }
    else if (type == SEEK_RIGHT_KEY) {
        if ((averr = av_seek_frame(sp.formatContext,
                                   sp.streamIndex,
                                   pos / av_q2d(sp.stream->time_base),
                                   AVSEEK_FLAG_FRAME)) < 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to do av_seek_frame, pos:" << pos << "flag:" << AVSEEK_FLAG_FRAME;
            return false;
        }
        SPAVFrame frame;
        if ((averr = getVideoNextFrame(index, frame)) < 0) {
            if (averr == AVERROR_EOF) {
                qDebug("%s cannot seek to %f, try to seek %f",  __PRETTY_FUNCTION__, pos, pos - 1);
                return seekVideo(pos - 1, type);
            }
            return false;
        }
    }
    return true;
}

bool AVDecoderCore::getVideoSeekPos(int index, double t, AVDecoderCore::SEEK_POS_Type type, double &spos)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return false;
    }
    if (!isVideoStreamEnabled(index)) {
        return false;
    }
    if (t < 0) {
        return false;
    }
    double duration = getVideoDuration(index);
    if (t > duration) {
        return false;
    }

    StreamParty &sp = m_videoStreamParties[index]->streamParty2;
    if (type == SEEK_POS_LEFT_KEY || type == SEEK_POS_RIGHT_KEY) {
        int AVSEEK_FLAG = type == SEEK_POS_LEFT_KEY ? AVSEEK_FLAG_BACKWARD : AVSEEK_FLAG_FRAME;
        avcodec_flush_buffers(sp.codecContext);
        int averr = av_seek_frame(sp.formatContext,
                                  sp.streamIndex,
                                  t / av_q2d(sp.stream->time_base),
                                  AVSEEK_FLAG);
        if (averr < 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to do av_seek_frame" << averr << iav_err2str(averr) << t << type;
            return false;
        }
        while (1) {
            AVPacket *avPacket = new AVPacket;
            QSharedPointer<AVPacket> spAVPacket(avPacket, deleteAVPacket);
            av_init_packet(avPacket);
            averr = av_read_frame(sp.formatContext, avPacket);
            if (averr != 0) {
                if (averr == AVERROR_EOF) {
                    return getVideoSeekPos(index, t - 1, type, spos);
                }
                else {
                    qDebug() <<  __PRETTY_FUNCTION__ << "failed to do av_read_frame()" << averr << iav_err2str(averr) << t << type;
                    return false;
                }
            }
            if (avPacket->stream_index != sp.streamIndex) {
                continue;
            }
            if (avPacket->flags & AV_PKT_FLAG_KEY) {
                spos = av_q2d(sp.stream->time_base) * avPacket->pts;
                return true;
            }
        }
    }
    else if (type == SEEK_POS_NEAREST) {
        double leftPts = t, rightPts = t;
        bool leftOk = getVideoSeekPos(index, t, SEEK_POS_LEFT_KEY, leftPts);
        bool rightOk = getVideoSeekPos(index, t, SEEK_POS_RIGHT_KEY, rightPts);
        if (leftOk) {
            if (rightOk) {
                spos = (qAbs(t - leftPts) < qAbs(t - rightPts)) ? leftPts : rightPts;
                return true;
            }
            else {
                spos = leftPts;
                return true;
            }
        }
        else {
            if (rightOk) {
                spos = rightPts;
                return true;
            }
            else {
                return false;
            }
        }
    }
    return false;
}

bool AVDecoderCore::getVideoKeyFramePosList(int index, QList<double> &posList)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return false;
    }
    if (!isVideoStreamEnabled(index)) {
        return false;
    }

    double duration = getVideoDuration(index);
    double t = 0.0;
    while (t < duration) {
        double pos;
        if (!getVideoSeekPos(index, t, SEEK_POS_LEFT_KEY, pos)) {
            return false;
        }
        posList.append(pos);
        t = pos + 0.1;
    }
    return true;
}

int AVDecoderCore::getVideoNextFrame(int index, SPAVFrame &frame)
{
    return getVideoNextFrame(index, frame, false, 0.0, false);
}

int AVDecoderCore::getVideoNextFrame(int index, SPAVFrame &frame, double pos)
{
    return getVideoNextFrame(index, frame, true, pos, false);
}

int AVDecoderCore::getVideoNextKeyFrame(int index, SPAVFrame &frame)
{
    return getVideoNextFrame(index, frame, false, 0.0, true);
}

double AVDecoderCore::calculateVideoTimestamp(int index, long long t)
{
    if (index < 0 || index >= m_videoStreamParties.count()) {
        return 0.0;
    }
    if (!isVideoStreamEnabled(index)) {
        return 0.0;
    }
    StreamParty &sp = m_videoStreamParties[index]->streamParty;
    return t * av_q2d(sp.stream->time_base);
}

bool AVDecoderCore::hasCover()
{
    return !m_coverStreamParties.isEmpty();
}

int AVDecoderCore::getCoverCount()
{
    return m_coverStreamParties.count();
}

bool AVDecoderCore::getCover(int index, SPAVFrame &frame)
{
    if (index < 0 || index >= m_coverStreamParties.count()) {
        return false;
    }
    frame = m_coverStreamParties[index]->cover;
    return true;
}

bool AVDecoderCore::initStreamParty(AVDecoderCore::StreamParty *sp, const QString &file)
{
    if (sp == 0 || sp->streamIndex < 0) {
        return false;
    }
    if (file.isEmpty()) {
        return false;
    }

    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, file.toStdString().c_str(), NULL, NULL) < 0) {
        avformat_free_context(formatContext);
        return false;
    }
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return false;
    }

    if (sp->streamIndex >= formatContext->nb_streams) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return false;
    }

    AVStream *stream = formatContext->streams[sp->streamIndex];
    AVCodecParameters *codecPar = stream->codecpar;
    AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (codec == 0) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return false;
    }
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (codecContext == 0) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return false;
    }
    if (avcodec_parameters_to_context(codecContext, codecPar) < 0) {
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return false;
    }
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return false;
    }

    sp->formatContext = formatContext;
    sp->stream = stream;
    sp->codecPar = codecPar;
    sp->codecContext = codecContext;
    return true;
}

void AVDecoderCore::uninitStreamParty(AVDecoderCore::StreamParty *sp)
{
    if (sp == 0) {
        return;
    }

    if (sp->codecContext != 0) {
        avcodec_close(sp->codecContext);
        avcodec_free_context(&(sp->codecContext));
    }
    if (sp->formatContext != 0) {
        avformat_close_input(&(sp->formatContext));
        avformat_free_context(sp->formatContext);
    }

    sp->formatContext = 0;
    sp->stream = 0;
    sp->codecPar = 0;
    sp->codecContext = 0;
}

//double AVDecoderCore::getAudioDuration(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0.0;
//    }
//    if (asp->streamParty.stream->duration > 0) {
//        return asp->streamParty.stream->duration * av_q2d(asp->streamParty.stream->time_base);
//    }
//    if (asp->streamParty.formatContext == 0) {
//        return 0.0;
//    }
//    if (asp->streamParty.formatContext->duration > 0) {
//        return asp->streamParty.formatContext->duration * av_q2d(AV_TIME_BASE_Q);
//    }
//    return 0.0;
//}

//int AVDecoderCore::getVideoBitrate(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return asp->streamParty.codecPar->bit_rate;
//}

//AVSampleFormat AVDecoderCore::getAudioSampleFormat(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return AV_SAMPLE_FMT_NONE;
//    }
//    return (AVSampleFormat)asp->streamParty.codecPar->format;
//}

//int AVDecoderCore::getAudioSampleSize(AVDecoderCore::AudioStreamParty *asp)
//{
//    return getAudioBytesPerSample(asp) * 8;
//}

//int AVDecoderCore::getAudioSampleRate(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return asp->streamParty.codecPar->sample_rate;
//}

//int AVDecoderCore::getAudioChannels(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return asp->streamParty.codecPar->channels;
//}

//uint64_t AVDecoderCore::getAudioChannelLayout(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return asp->streamParty.codecPar->channel_layout;
//}

//int AVDecoderCore::getAudioBytesPerSample(AVDecoderCore::AudioStreamParty *asp)
//{
//    return av_get_bytes_per_sample(getAudioSampleFormat(asp));
//}

//int AVDecoderCore::getAudioBytesPerFrame(AVDecoderCore::AudioStreamParty *asp)
//{
//    return getAudioChannels(asp) * getAudioBytesPerSample(asp);
//}

//int AVDecoderCore::getAudioBytesPerSecond(AVDecoderCore::AudioStreamParty *asp)
//{
//    return getAudioBytesPerFrame(asp) * getAudioSampleRate(asp);
//}

//AVSampleFormat AVDecoderCore::getAudioOutputSampleFormat(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return AV_SAMPLE_FMT_NONE;
//    }
//    return asp->outputSampleFormat;
//}

//int AVDecoderCore::getAudioOutputSampleSize(AVDecoderCore::AudioStreamParty *asp)
//{
//    return getAudioOutputBytesPerSample(asp) * 8;
//}

//int AVDecoderCore::getAudioOutputSampleRate(AVDecoderCore::AudioStreamParty *asp)
//{
//    return getAudioSampleRate(asp);
//}

//int AVDecoderCore::getAudioOutputChannels(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return asp->outputChannels;
//}

//uint64_t AVDecoderCore::getAudioOutputChannelLayout(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return asp->outputChannelLayout;
//}

//int AVDecoderCore::getAudioOutputBytesPerSample(AVDecoderCore::AudioStreamParty *asp)
//{
//    return av_get_bytes_per_sample(getAudioOutputSampleFormat(asp));
//}

//int AVDecoderCore::getAudioOutputBytesPerFrame(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return getAudioOutputChannels(asp) * getAudioOutputBytesPerSample(asp);
//}

//int AVDecoderCore::getAudioOutputBytesPerSecond(AVDecoderCore::AudioStreamParty *asp)
//{
//    if (asp == 0) {
//        return 0;
//    }
//    return getAudioOutputBytesPerFrame(asp) * getAudioOutputSampleRate(asp);
//}

//double AVDecoderCore::getVideoDuration(AVDecoderCore::VideoStreamParty *vsp)
//{
//    if (vsp == 0) {
//        return 0.0;
//    }
//    if (vsp->streamParty.stream->duration > 0) {
//        return vsp->streamParty.stream->duration * av_q2d(vsp->streamParty.stream->time_base);
//    }
//    if (m_videoStreamParty->streamParty.formatContext == 0) {
//        return 0.0;
//    }
//    if (m_videoStreamParty->streamParty.formatContext->duration > 0) {
//        return m_videoStreamParty->streamParty.formatContext->duration * av_q2d(AV_TIME_BASE_Q);
//    }
//    return 0.0;
//}

//double AVDecoderCore::getVideoBitrate(VideoStreamParty *vsp)
//{
//    if (vsp == 0) {
//        return 0;
//    }
//    return vsp->streamParty.codecPar->bit_rate;
//}

//double AVDecoderCore::getVideoFrameRate(AVDecoderCore::VideoStreamParty *vsp)
//{
//    if (vsp == 0) {
//        return 0.0;
//    }
//    return av_q2d(av_stream_get_r_frame_rate(vsp->streamParty.stream));
//}

//int AVDecoderCore::getVideoWidth(AVDecoderCore::VideoStreamParty *vsp)
//{
//    if (vsp == 0) {
//        return 0;
//    }
//    return vsp->streamParty.codecPar->width;
//}

//int AVDecoderCore::getVideoHeight(AVDecoderCore::VideoStreamParty *vsp)
//{
//    if (vsp == 0) {
//        return 0;
//    }
//    return vsp->streamParty.codecPar->height;
//}

//AVPixelFormat AVDecoderCore::getVideoPixelFormat(AVDecoderCore::VideoStreamParty *vsp)
//{
//    if (vsp == 0) {
//        return AV_PIX_FMT_NONE;
//    }
//    return (AVPixelFormat)vsp->streamParty.codecPar->format;
//}

bool AVDecoderCore::getCover(AVFormatContext *formatContext, int streamIndex, SPAVFrame &frame)
{
    if (formatContext == 0) {
        return false;
    }
    if (streamIndex < 0 || streamIndex >= formatContext->nb_streams) {
        return false;
    }

    AVStream *stream = formatContext->streams[streamIndex];
    if (stream->attached_pic.data == 0) {
        return false;
    }
    AVCodecParameters *codecPar = stream->codecpar;
    if (codecPar->codec_type != AVMEDIA_TYPE_VIDEO) {
        return false;
    }
    AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (codec == 0) {
        return false;
    }
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (codecContext == 0) {
        return false;
    }
    if (avcodec_parameters_to_context(codecContext, codecPar) < 0) {
        avcodec_free_context(&codecContext);
        return false;
    }
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        avcodec_free_context(&codecContext);
        return false;
    }

    int averr = avcodec_send_packet(codecContext, &stream->attached_pic);
    if (averr == 0) {
        AVFrame *pAVFrame = av_frame_alloc();
        SPAVFrame spAVFrame(pAVFrame, freeAVFarme);
        averr = avcodec_receive_frame(codecContext, pAVFrame);
        if (averr == 0) {
            frame = spAVFrame;
            return true;
        }
        else {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to do avcodec_receive_frame" << averr << iav_err2str(averr);
        }
    }
    else {
        qDebug() <<  __PRETTY_FUNCTION__ << "failed to do avcodec_send_packet" << averr << iav_err2str(averr);
    }

    avcodec_close(codecContext);
    avcodec_free_context(&codecContext);
    return false;
}

int AVDecoderCore::getVideoNextFrame(int index, SPAVFrame &frame, bool specifyPos, double pos, bool specifyKeyFrame)
{
    int averr = AVERROR_UNKNOWN;
    if (index < 0 || index > m_videoStreamParties.count()) {
        return averr;
    }
    if (!isVideoStreamEnabled(index)) {
        return averr;
    }
    if (specifyPos && pos < 0) {
        return averr;
    }
    StreamParty &sp = m_videoStreamParties[index]->streamParty;
    while (1) {
        AVPacket *avPacket = new AVPacket;
        QSharedPointer<AVPacket> spAVPacket(avPacket, deleteAVPacket);
        av_init_packet(avPacket);
        if ((averr = av_read_frame(sp.formatContext, avPacket)) != 0) {
            if (averr == AVERROR_EOF) {
                qDebug() <<  __PRETTY_FUNCTION__ << "decode end of file";
            }
            else {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to read frame" << averr << iav_err2str(averr);
            }
            return averr;
        }

        if (avPacket->stream_index != sp.streamIndex) {
            continue;
        }

//        qDebug() <<  __PRETTY_FUNCTION__
//                 << "packet pts:" << av_q2d(sp.stream->time_base) * avPacket->pts
//                 << "dts:" << av_q2d(sp.stream->time_base) * avPacket->dts
//                 << "flags:" << avPacket->flags;

        if (specifyKeyFrame && !(avPacket->flags & AV_PKT_FLAG_KEY)) {
            continue;
        }

        if ((averr = avcodec_send_packet(sp.codecContext, avPacket)) != 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "failed to send packet," << averr << iav_err2str(averr);
//            return averr;
            continue;
        }

        AVFrame *pAVFrame = av_frame_alloc();
        SPAVFrame spAVFrame(pAVFrame, freeAVFarme);
        if ((averr = avcodec_receive_frame(sp.codecContext, pAVFrame)) != 0) {
            if (averr != AVERROR(EAGAIN)) {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to receive frame," << averr << iav_err2str(averr);
                return averr;
            }
            continue;
        }

//        qDebug() <<  __PRETTY_FUNCTION__
//                  << "frame  pts:" << av_q2d(sp.stream->time_base) * pAVFrame->pts
//                  << "frame  duration:" << av_q2d(sp.stream->time_base) * pAVFrame->pkt_duration
//                  << "key_frame:" << pAVFrame->key_frame;

        if (specifyKeyFrame && pAVFrame->key_frame != 1) {
            continue;
        }
        if (specifyPos && (pAVFrame->pts + pAVFrame->pkt_duration) * av_q2d(sp.stream->time_base) < pos) {
            continue;
        }
        frame = spAVFrame;
        break;

//        AVFrame *pAVFrameOut = av_frame_alloc();
//        pAVFrameOut->format = AV_PIX_FMT_RGB32;
//        pAVFrameOut->width = pAVFrame->width;
//        pAVFrameOut->height = pAVFrame->height;
//        av_frame_get_buffer(pAVFrameOut, 1);

//        SwsContext *pSwsContext = sws_getContext(pAVFrame->width,pAVFrame->height,(AVPixelFormat)pAVFrame->format,
//                                                 pAVFrameOut->width,pAVFrameOut->height,(AVPixelFormat)pAVFrameOut->format,
//                                                 SWS_FAST_BILINEAR,0,0,0);
//        sws_scale(pSwsContext,pAVFrame->data,pAVFrame->linesize,0,
//                  pAVFrame->height,pAVFrameOut->data,pAVFrameOut->linesize);
//        sws_freeContext(pSwsContext);
    }
    return 0;
}

void AVDecoderCore::getSubtitle(AVDecoderCore::SubtitleStreamParty *ssp)
{
    if (ssp == 0) {
        return;
    }

    int averr = 0;
    while (1) {
        AVPacket *avPacket = new AVPacket;
        QSharedPointer<AVPacket> spAVPacket(avPacket, deleteAVPacket);
        av_init_packet(avPacket);
        if ((averr = av_read_frame(ssp->streamParty.formatContext, avPacket)) != 0) {
            if (averr == AVERROR_EOF) {
                qDebug() <<  __PRETTY_FUNCTION__ << "decode end of file";
            }
            else {
                qDebug() <<  __PRETTY_FUNCTION__ << "failed to read frame" << averr << iav_err2str(averr);
            }
            return;
        }

        if (avPacket->stream_index != ssp->streamParty.streamIndex) {
            continue;
        }

        int got_sub_ptr = 0;
        AVSubtitle avSubtitle;
        int r = avcodec_decode_subtitle2(ssp->streamParty.codecContext, &avSubtitle, &got_sub_ptr, avPacket);
        if (r < 0) {
            qDebug() << __PRETTY_FUNCTION__ << "failed to do avcodec_decode_subtitle2" << r << iav_err2str(r);
            if (got_sub_ptr) {
                avsubtitle_free(&avSubtitle);
            }
            return;
        }

        qDebug() << avSubtitle.format
                 << avSubtitle.start_display_time
                 << avSubtitle.end_display_time
                 << avSubtitle.pts
                 << avSubtitle.num_rects
                 << avSubtitle.rects
                 << avSubtitle.rects[0]->type
                 << avSubtitle.rects[0]->ass;

        if (got_sub_ptr) {
            avsubtitle_free(&avSubtitle);
        }
    }
}

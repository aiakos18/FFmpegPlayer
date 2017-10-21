#include "avdecoder.h"
#include "smartmutex.h"

static char *iav_err2str(int eid)
{
    static QByteArray s;
    s.resize(128);
    return av_make_error_string(s.data(), 128, eid);
}

static void clean_qimage_buffer(void*frame)
{
    if (frame) {
        av_frame_free((AVFrame**)(&frame));
    }
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

AVDecoder::AVDecoder(QObject *parent)
    : QObject(parent)
    , m_decodingType(DECODING_NONE)
    , m_decodingState(DECODING_STOPPED)
    , m_formatContext(0)
    , m_videoStream(0)
    , m_audioStream(0)
    , m_videoCodecPar(0)
    , m_audioCodecPar(0)
    , m_videoCodecContext(0)
    , m_audioCodecContext(0)
    , m_videoStreamIndex(-1)
    , m_audioStreamIndex(-1)
    , m_audioOutSampleFmt(AV_SAMPLE_FMT_NONE)
    , m_audioOutputChannels(0)
    , m_audioOutputChannelLayout(0)
    , m_audioMinBufferSize(0)
    , m_videoMinBufferFrameCount(0)
    , m_dataMtx(QMutex::Recursive)
    , m_opeMtx(QMutex::Recursive)
    , m_isSeeking(false)
{

}

AVDecoder::~AVDecoder()
{
    unload();

    requestQuit();

    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

void AVDecoder::init()
{
    qRegisterMetaType<SPAVFrame>("SPAVFrame");

    moveToThread(&m_thread);
    m_thread.start();
    QMetaObject::invokeMethod(this, "handleRequests");

    av_register_all();
}

bool AVDecoder::load(const QString &file, DECODING_Type decodeType)
{
    if (!QFile::exists(file) || decodeType == DECODING_NONE) {
        return false;
    }
    if (file == m_file && decodeType == m_decodingType) {
        return true;
    }

    unload();

    m_formatContext = avformat_alloc_context();
    int result = avformat_open_input(&m_formatContext, file.toStdString().c_str(),NULL,NULL);
    if (result < 0){
        qDebug() << "打开媒体流失败";
        unload();
        return false;
    }

    //获取视频流信息
    result = avformat_find_stream_info(m_formatContext,NULL);
    if (result < 0){
        qDebug() << "获取媒体流信息失败";
        unload();
        return false;
    }
    qDebug("媒体流个数: %d", m_formatContext->nb_streams);

    if (decodeType == DECODING_VIDEO) {
        m_videoStreamIndex = -1;
        for (uint i = 0; i < m_formatContext->nb_streams; i++) {
            AVStream *stream = m_formatContext->streams[i];
            AVMediaType type = stream->codecpar->codec_type;
            if (type == AVMEDIA_TYPE_VIDEO) {
                m_videoStreamIndex = i;
                m_videoStream = stream;
                qDebug("视频流索引: %d", m_videoStreamIndex);
                qDebug("视频时间基: %d/%d", stream->time_base.num, stream->time_base.den);
            }
        }

        if (m_videoStreamIndex == -1) {
            qDebug("没有视频流");
            unload();
            return false;
        }
        m_videoCodecPar = m_videoStream->codecpar;

        //获取视频流解码器
        AVCodec *pAVVideoCodec = avcodec_find_decoder(m_videoCodecPar->codec_id);

        //打开视频流解码器
        m_videoCodecContext = avcodec_alloc_context3(pAVVideoCodec);
//        m_videoCodecContext->thread_count = 2;
        avcodec_parameters_to_context(m_videoCodecContext, m_videoCodecPar);
        result = avcodec_open2(m_videoCodecContext,pAVVideoCodec,NULL);
        if (result<0){
            qDebug()<<"打开视频流解码器失败";
            unload();
            return false;
        }

        qDebug("视频时长: %f, 视频格式: %d, %s, 比特率: %lld, 帧率: %f, 视频宽: %d，视频高: %d",
               video_getDuration(),
               m_videoCodecPar->format,
               av_get_pix_fmt_name((AVPixelFormat)m_videoCodecPar->format),
               m_videoCodecPar->bit_rate,
               video_getFrameRate(),
               m_videoCodecPar->width,
               m_videoCodecPar->height);
    }
    else if (decodeType == DECODING_AUDIO) {
        m_audioStreamIndex = -1;
        for (uint i = 0; i < m_formatContext->nb_streams; i++) {
            AVStream *stream = m_formatContext->streams[i];
            AVMediaType type = stream->codecpar->codec_type;
            if (type == AVMEDIA_TYPE_AUDIO) {
                m_audioStreamIndex = i;
                m_audioStream = stream;
                qDebug("音频流索引: %d", m_audioStreamIndex);
                qDebug("音频时间基: %d/%d", stream->time_base.num, stream->time_base.den);
            }
        }

        if (m_audioStreamIndex == -1){
            qDebug("没有音频流");
            unload();
            return false;
        }
        m_audioCodecPar = m_audioStream->codecpar;
        if (m_audioCodecPar->channels > 0 && m_audioCodecPar->channel_layout == 0) {
            m_audioCodecPar->channel_layout = av_get_default_channel_layout(m_audioCodecPar->channels);
        }
        else if (m_audioCodecPar->channel_layout > 0 && m_audioCodecPar->channels == 0) {
            m_audioCodecPar->channels = av_get_channel_layout_nb_channels(m_audioCodecPar->channel_layout);
        }
        if (m_audioCodecPar->channels > 2) {
            m_audioOutputChannels = 2;
        }

        //获取音频流解码器
        AVCodec *pAVAudioCodec = avcodec_find_decoder(m_audioCodecPar->codec_id);

        //打开音频流解码器
        m_audioCodecContext = avcodec_alloc_context3(pAVAudioCodec);
        avcodec_parameters_to_context(m_audioCodecContext, m_audioCodecPar);
        result = avcodec_open2(m_audioCodecContext,pAVAudioCodec,NULL);
        if (result<0){
            qDebug()<<"打开音频流解码器失败";
            unload();
            return false;
        }

        AVSampleFormat audioInSampleFmt = (AVSampleFormat)m_audioCodecPar->format;
        m_audioOutSampleFmt = audioInSampleFmt;
        if (av_sample_fmt_is_planar(audioInSampleFmt)) {
            m_audioOutSampleFmt = av_get_packed_sample_fmt(audioInSampleFmt);
        }
        if (m_audioOutSampleFmt == AV_SAMPLE_FMT_FLT
                || m_audioOutSampleFmt == AV_SAMPLE_FMT_DBL
                || m_audioOutSampleFmt == AV_SAMPLE_FMT_S64) {
            m_audioOutSampleFmt = AV_SAMPLE_FMT_S32;
        }

        char channelLayoutName[32];
        memset(channelLayoutName, 0, 32);
        av_get_channel_layout_string(channelLayoutName, 32, m_audioCodecPar->channels, m_audioCodecPar->channel_layout);
        qDebug("音频时长: %f, 采样格式: %d, %s, 比特率: %lld, 采样率: %d, 采样位深: %d, 通道数: %d, 通道布局类型: %llu, %s",
               audio_getDuration(),
               (AVSampleFormat)m_audioCodecPar->format,
               av_get_sample_fmt_name((AVSampleFormat)m_audioCodecPar->format),
               m_audioCodecPar->bit_rate,
               m_audioCodecPar->sample_rate,
               audio_getSampleSize(),
               m_audioCodecPar->channels,
               m_audioCodecPar->channel_layout,
               channelLayoutName);
        qDebug("输出音频的采样格式: %s", av_get_sample_fmt_name(m_audioOutSampleFmt));
        qDebug() << "frame_size:" << m_audioCodecPar->frame_size;
    }
    else {
        return false;
    }

    m_audioDatas.clear();
    m_videoDatas.clear();

    m_file = file;
    m_decodingType = decodeType;

    requestDecode();
    setDecodingState(DECODING_STARTED);

    return true;
}

void AVDecoder::unload()
{
    if (!isLoaded()) {
        return;
    }

    SmartMutex mtx(&m_opeMtx);

    if (m_videoCodecContext) {
        avcodec_close(m_videoCodecContext);
        avcodec_free_context(&m_videoCodecContext);
        m_videoCodecContext = 0;
    }
    if (m_audioCodecContext) {
        avcodec_close(m_audioCodecContext);
        avcodec_free_context(&m_audioCodecContext);
        m_audioCodecContext = 0;
    }
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        avformat_free_context(m_formatContext);
        m_formatContext = 0;

        m_videoStream = 0;
        m_videoCodecPar = 0;
        m_videoStreamIndex = -1;

        m_audioStream = 0;
        m_audioCodecPar = 0;
        m_audioStreamIndex = -1;
    }

    m_file.clear();
    m_decodingType = DECODING_NONE;
    setDecodingState(DECODING_STOPPED);

    m_audioOutSampleFmt = AV_SAMPLE_FMT_NONE;
    m_audioOutputChannels = 0;
    m_audioOutputChannelLayout = 0;
    m_videoCover.reset();
    m_audioMinBufferSize = 0;
    m_videoMinBufferFrameCount = 0;
    setSeekingState(false);

    m_dataMtx.lock();
    m_audioDatas.clear();
    m_videoDatas.clear();
    m_dataMtx.unlock();

    clearRequestList();
}

bool AVDecoder::isLoaded()
{
    return (!m_file.isEmpty() && m_decodingType != DECODING_NONE);
}

QString AVDecoder::getFile()
{
    return m_file;
}

AVDecoder::DECODING_Type AVDecoder::decodingType()
{
    return m_decodingType;
}

AVDecoder::DECODING_State AVDecoder::decodingState()
{
    return m_decodingState;
}

bool AVDecoder::audio_hasStream()
{
    return (m_audioStreamIndex >= 0 && m_audioStream);
}

int AVDecoder::audio_getSampleRate()
{
    if (!audio_hasStream()) {
        return 0;
    }
    return m_audioCodecPar->sample_rate;
}

AVSampleFormat AVDecoder::audio_getSampleFormat()
{
    if (!audio_hasStream()) {
        return AV_SAMPLE_FMT_NONE;
    }
    return m_audioOutSampleFmt;
}

int AVDecoder::audio_getSampleSize()
{
    if (!audio_hasStream()) {
        return 0;
    }
    return av_get_bytes_per_sample(m_audioOutSampleFmt) * 8;
}

int AVDecoder::audio_getChannels()
{
    if (!audio_hasStream()) {
        return 0;
    }
    return m_audioCodecPar->channels > 2 ? 2 : m_audioCodecPar->channels;
}

int AVDecoder::audio_getBytesPerSample()
{
    if (!audio_hasStream()) {
        return 0;
    }
    return audio_getSampleSize() / 8;
}

int AVDecoder::audio_getBytesPerFrame()
{
    if (!audio_hasStream()) {
        return 0;
    }
    return audio_getBytesPerSample() * audio_getChannels();
}

int AVDecoder::audio_getBytesPerSecond()
{
    if (!audio_hasStream()) {
        return 0;
    }
    return audio_getBytesPerFrame() * audio_getSampleRate();
}

double AVDecoder::audio_getDuration()
{
    if (!audio_hasStream()) {
        return 0.0;
    }
    if (m_formatContext->duration > 0) {
        return m_formatContext->duration * av_q2d(AV_TIME_BASE_Q);
    }
    if (m_audioStream->duration > 0) {
        return m_audioStream->duration * av_q2d(m_audioStream->time_base);
    }
    return 0.0;
}

void AVDecoder::audio_seek(double time)
{
    removeDecodeRequests();
    requestSeekAudio(time);
}

bool AVDecoder::audio_hasBufferedData()
{
    return !m_audioDatas.isEmpty();
}

bool AVDecoder::audio_isDataBuffered()
{
    SmartMutex dataMtx(&m_dataMtx);
    quint32 size = 0;
    foreach (AudioData ad, m_audioDatas) {
        size += ad.data.size();
    }
    return (m_audioMinBufferSize == 0) ? (size > 0) : (size >= m_audioMinBufferSize);
}

AVDecoder::AudioData AVDecoder::audio_getBufferedData()
{
    SmartMutex dataMtx(&m_dataMtx);
    AudioData ad = { QByteArray(), 0.0, 0.0 };
    if (!m_audioDatas.isEmpty()) {
        ad = m_audioDatas.front();
    }
    return ad;
}

AVDecoder::AudioData AVDecoder::audio_popBufferedData()
{
    SmartMutex dataMtx(&m_dataMtx);
    AudioData ad = { QByteArray(), 0.0, 0.0 };
    if (!m_audioDatas.isEmpty()) {
        ad = m_audioDatas.front();
        m_audioDatas.pop_front();
        if (!audio_isDataBuffered()) {
            requestDecode();
        }
    }
    return ad;
}

void AVDecoder::audio_setMinBufferSize(int size)
{
    if (size < 0) {
        return;
    }
    m_audioMinBufferSize = size;
    if (!audio_isDataBuffered()) {
        requestDecode();
    }
}

bool AVDecoder::video_hasStream()
{
    return (m_videoStreamIndex >= 0 && m_videoStream);
}

int AVDecoder::video_getWidth()
{
    if (!video_hasStream()) {
        return 0;
    }
    return m_videoCodecPar->width;
}

int AVDecoder::video_getHeight()
{
    if (!video_hasStream()) {
        return 0;
    }
    return m_videoCodecPar->height;
}

AVPixelFormat AVDecoder::video_getPixelFormat()
{
    if (!video_hasStream()) {
        return AV_PIX_FMT_NONE;
    }
    return (AVPixelFormat)m_videoCodecPar->format;
}

double AVDecoder::video_getFrameRate()
{
    if (!video_hasStream()) {
        return 0;
    }
    return av_q2d(av_stream_get_r_frame_rate(m_videoStream));
}

double AVDecoder::video_getDuration()
{
    if (!video_hasStream()) {
        return 0.0;
    }
    if (m_formatContext->duration > 0) {
        return m_formatContext->duration * av_q2d(AV_TIME_BASE_Q);
    }
    if (m_videoStream->duration > 0) {
        return m_videoStream->duration * av_q2d(m_videoStream->time_base);
    }
    return 0;
}

void AVDecoder::video_seek(double pos, SEEK_Type type)
{
    if (!video_hasStream()) {
        return;
    }
    removeDecodeRequests();
    requestSeekVideo(pos, type);
}

bool AVDecoder::video_hasBufferedData()
{
    return !m_videoDatas.isEmpty();
}

bool AVDecoder::video_isDataBuffered()
{
    SmartMutex dataMtx(&m_dataMtx);
    return (m_videoMinBufferFrameCount == 0) ? (m_videoDatas.count() > 0) : (m_videoDatas.count() >= m_videoMinBufferFrameCount);
}

AVDecoder::VideoData AVDecoder::video_getBufferedData()
{
    SmartMutex dataMtx(&m_dataMtx);
    VideoData vd;
    if (!m_videoDatas.isEmpty()) {
        vd = m_videoDatas.front();
    }
    return vd;
}

AVDecoder::VideoData AVDecoder::video_popBufferedData()
{
    SmartMutex dataMtx(&m_dataMtx);
    VideoData vd;
    if (!m_videoDatas.isEmpty()) {
        vd = m_videoDatas.front();
        m_videoDatas.pop_front();
        if (!video_isDataBuffered()) {
            requestDecode();
        }
    }
    return vd;
}

void AVDecoder::video_setMinBufferCount(int count)
{
    if (count < 0) {
        return;
    }
    m_videoMinBufferFrameCount = count;
    if (!video_isDataBuffered()) {
        requestDecode();
    }
}

bool AVDecoder::isSeeking()
{
    return m_isSeeking;
}

void AVDecoder::setDecodingState(AVDecoder::DECODING_State state)
{
    if (state == m_decodingState) {
        return;
    }
    m_decodingState = state;
    emit stateChanged(state);
}

void AVDecoder::setSeekingState(bool isSeeking)
{
    if (isSeeking == m_isSeeking) {
        return;
    }
    m_isSeeking = isSeeking;
    qDebug() << "decording type:" << m_decodingType << "seeking state:" << m_isSeeking;
    emit seekingStateChanged(isSeeking);
}

void AVDecoder::handleRequests()
{
    while (1) {
        m_reqMtx.lock();
        if (m_requestList.isEmpty()) {
//            qDebug() << "cond in";
            m_cond.wait(&m_reqMtx);
//            qDebug() << "cond out";
            m_reqMtx.unlock();
            continue;
        }
        Request req = m_requestList.front();
        m_requestList.pop_front();
        m_reqMtx.unlock();
//        qDebug() << "request:" << req.rid << req.p1;

        switch (req.rid) {
        case REQUEST_DECODE:
            doDecode(req.p1);
            break;

        case REQUEST_AUDIO_SEEK:
            doSeekAudio(req.p1.toDouble());
            break;

        case REQUEST_VIDEO_SEEK:
            doSeekVideo(req.p1.toMap()["pos"].toDouble(), (SEEK_Type)req.p1.toMap()["type"].toInt());
            break;

        case REQUEST_QUIT:
            m_reqMtx.lock();
            m_requestList.clear();
            m_reqMtx.unlock();
            return;

        default:
            break;
        }
    }
}

void AVDecoder::pushRequest(const Request &req)
{
    m_reqMtx.lock();
    m_requestList.push_back(req);
    m_cond.wakeAll();
    m_reqMtx.unlock();
}

void AVDecoder::clearRequestList()
{
    m_reqMtx.lock();
    m_requestList.clear();
    m_reqMtx.unlock();
}

void AVDecoder::removeDecodeRequests()
{
    m_reqMtx.lock();
    for (QList<Request>::iterator it = m_requestList.begin(); it != m_requestList.end(); ) {
        if (it->rid == REQUEST_DECODE) {
            it = m_requestList.erase(it);
        }
        else {
            ++it;
        }
    }
    m_reqMtx.unlock();
}

void AVDecoder::requestDecode(QVariant p)
{
    Request req(REQUEST_DECODE, p);
    pushRequest(req);
}

void AVDecoder::requestSeekAudio(double pos)
{
    Request req(REQUEST_AUDIO_SEEK, pos);
    pushRequest(req);
}

void AVDecoder::requestSeekVideo(double pos, SEEK_Type type)
{
    QVariantMap vmap;
    vmap["pos"] = pos;
    vmap["type"] = type;
    Request req(REQUEST_VIDEO_SEEK, vmap);
    pushRequest(req);
}

void AVDecoder::requestQuit()
{
    Request req(REQUEST_QUIT);
    pushRequest(req);
}

void AVDecoder::doDecode(QVariant p)
{
    SmartMutex mtx(&m_opeMtx);

    QVariantMap vmap = p.toMap();
    bool needSeekPos = vmap.keys().contains("pos");
    double pos = vmap["pos"].toDouble();

    if (m_decodingState == DECODING_STOPPED) {
        return;
    }
    int averr;
    if (m_decodingType == DECODING_AUDIO) {
        if (audio_isDataBuffered()) {
            return;
        }

        while (1) {
            if (audio_isDataBuffered()) {
                emit audioDataBuffered();
                return;
            }

            AVPacket *avPacket = new AVPacket;
            QSharedPointer<AVPacket> spAVPacket(avPacket, deleteAVPacket);

            av_init_packet(avPacket);

            if ((averr = av_read_frame(m_formatContext, avPacket)) != 0) {
                if (averr == AVERROR_EOF) {
                    qDebug() << "decode audio: end of file";
                }
                else {
                    qDebug() << "decode audio: failed to read frame," << iav_err2str(averr);
                    if (!m_audioDatas.isEmpty()) {
                        emit audioDataBuffered();
                    }
                }
                setDecodingState(DECODING_END);
                return;
            }
            setDecodingState(DECODING_STARTED);

            if (avPacket->stream_index != m_audioStreamIndex) {
                continue;
            }

            if ((averr = avcodec_send_packet(m_audioCodecContext, avPacket)) != 0) {
                qDebug() << "decode audio: failed to send packet," << iav_err2str(averr) << averr;
                continue;
            }

            AVFrame *pAVFrame = av_frame_alloc();
            if (!pAVFrame) {
                qDebug() << "decode audio: failed to alloc AVFrame";
                return;
            }
            SPAVFrame spAVFrame(pAVFrame, freeAVFarme);
            while (1) {
                if ((averr = avcodec_receive_frame(m_audioCodecContext, pAVFrame)) != 0) {
                    if (averr != AVERROR(EAGAIN)) {
                        qDebug() << "decode audio: failed to receive frame," << iav_err2str(averr) << averr;
                    }
                    break;
                }
//                qDebug("decode audio: pts: %f, duration: %f, samples: %d",
//                       av_q2d(m_audioStream->time_base) * pAVFrame->pts,
//                       av_q2d(m_audioStream->time_base) * pAVFrame->pkt_duration,
//                       pAVFrame->nb_samples);

                if (pAVFrame->channels > 0 && pAVFrame->channel_layout == 0) {
                    pAVFrame->channel_layout = av_get_default_channel_layout(pAVFrame->channels);
                }
                else if (pAVFrame->channel_layout > 0 && pAVFrame->channels == 0) {
                    pAVFrame->channels = av_get_channel_layout_nb_channels(pAVFrame->channel_layout);
                }

                uint64_t outputChannelLayout = pAVFrame->channels > 2 ? AV_CH_LAYOUT_STEREO : pAVFrame->channel_layout;
                int outputChannels = pAVFrame->channels > 2 ? 2 : pAVFrame->channels;

                SwrContext *swr = swr_alloc();
                if (!swr) {
                    qDebug() << "decode audio: failed to alloc SwrContext";
                    return;
                }
                QSharedPointer<SwrContext> spSwr(swr, freeSwrContext);

                av_opt_set_channel_layout(swr, "in_channel_layout",  pAVFrame->channel_layout, 0);
                av_opt_set_channel_layout(swr, "out_channel_layout", outputChannelLayout,  0);
                av_opt_set_int(swr, "in_sample_rate", pAVFrame->sample_rate, 0);
                av_opt_set_int(swr, "out_sample_rate", pAVFrame->sample_rate, 0);
                av_opt_set_sample_fmt(swr, "in_sample_fmt",  (AVSampleFormat)m_audioCodecPar->format, 0);
                av_opt_set_sample_fmt(swr, "out_sample_fmt", m_audioOutSampleFmt, 0);

//                SwrContext *swr = swr_alloc_set_opts(NULL,  // we're allocating a new context
//                                    pAVFrame->channel_layout,               // out_ch_layout
//                                    m_audioOutSampleFmt,                    // out_sample_fmt
//                                    pAVFrame->sample_rate,                  // out_sample_rate
//                                    pAVFrame->channel_layout,               // in_ch_layout
//                                    (AVSampleFormat)pAVFrame->format,       // in_sample_fmt
//                                    pAVFrame->sample_rate,                  // in_sample_rate
//                                    0,                    // log_offset
//                                    NULL);                // log_ctx

                if ((averr = swr_init(swr)) < 0) {
                    qDebug() << "decode audio: failed to init swr," << iav_err2str(averr);
                    return;
                }

                AVFrame *pAVFrameOut = av_frame_alloc();
                if (!pAVFrameOut) {
                    qDebug() << "decode audio: failed to alloc AVFrame";
                    return;
                }
                QSharedPointer<AVFrame> spAVFrameOut(pAVFrameOut, freeAVFarme);

                int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr, pAVFrame->sample_rate) + pAVFrame->nb_samples,
                                                    pAVFrame->sample_rate, pAVFrame->sample_rate, AV_ROUND_INF);
                pAVFrameOut->format = m_audioOutSampleFmt;
                pAVFrameOut->nb_samples = dst_nb_samples;
                pAVFrameOut->channel_layout = outputChannelLayout;
                if ((averr = av_frame_get_buffer(pAVFrameOut, 1)) != 0) {
                    qDebug() << "decode audio: failed to get frame buffer," << iav_err2str(averr);;
                    return;
                }

                int nb = swr_convert(swr, pAVFrameOut->data, dst_nb_samples, (const uint8_t**)pAVFrame->data, pAVFrame->nb_samples);
                int data_size = outputChannels * nb * av_get_bytes_per_sample(m_audioOutSampleFmt);
                QByteArray data((const char*)pAVFrameOut->data[0], data_size);
                AudioData ad = { data, av_q2d(m_audioStream->time_base) * pAVFrame->pts,
                               av_q2d(m_audioStream->time_base) * pAVFrame->pkt_duration};
                m_dataMtx.lock();
                m_audioDatas.push_back(ad);
                m_dataMtx.unlock();
            }
        }
    }
    else if (m_decodingType == DECODING_VIDEO) {
        if (video_isDataBuffered()) {
            return;
        }

        while (1) {
            if (video_isDataBuffered()) {
                emit videoDataBuffered();
                return;
            }

            AVPacket *avPacket = new AVPacket;
            QSharedPointer<AVPacket> spAVPacket(avPacket, deleteAVPacket);
            av_init_packet(avPacket);
            if ((averr = av_read_frame(m_formatContext, avPacket)) != 0) {
                if (averr == AVERROR_EOF) {
                    qDebug() << "decode video: end of file";
                }
                else {
                    qDebug() << "decode video: failed to read frame," << iav_err2str(averr);
                    if (!m_videoDatas.isEmpty()) {
                        emit videoDataBuffered();
                    }
                }
                setDecodingState(DECODING_END);
                av_seek_frame(m_formatContext, m_videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
                return;
            }
            setDecodingState(DECODING_STARTED);

            if (avPacket->stream_index != m_videoStreamIndex) {
                continue;
            }

//            qDebug("decode video: packet pts: %6.2f, dts: %6.2f, flags: %d",
//                   av_q2d(m_videoStream->time_base) * avPacket->pts,
//                   av_q2d(m_videoStream->time_base) * avPacket->dts,
//                   avPacket->flags);

            if ((averr = avcodec_send_packet(m_videoCodecContext, avPacket)) != 0) {
                qDebug() << "decode video: failed to send packet," << iav_err2str(averr) << averr;
                continue;
            }

            AVFrame *pAVFrame = av_frame_alloc();
            if (!pAVFrame) {
                qDebug() << "decode video: failed to alloc AVFrame";
                return;
            }
            SPAVFrame spAVFrame(pAVFrame, freeAVFarme);
            if ((averr = avcodec_receive_frame(m_videoCodecContext, pAVFrame)) != 0) {
                if (averr != AVERROR(EAGAIN)) {
                    qDebug() << "decode video: failed to receive frame," << iav_err2str(averr) << averr;
                }
                continue;
            }

            if (needSeekPos) {
                double pts = av_q2d(m_videoStream->time_base) * pAVFrame->pts;
                if (pts < pos) {
                    continue;
                }
            }

//            qDebug("decode video: frame  pts: %6.2f, key_frame: %d",
//                   av_q2d(m_videoStream->time_base) * pAVFrame->pts, pAVFrame->key_frame);

//            qDebug("decode video: pts time: %f", av_q2d(m_videoStream->time_base) * pAVFrame->pts);
//            qDebug("video frame width: %d, height: %d", pAVFrame->width, pAVFrame->height);

//            AVFrame *pAVFrameOut = av_frame_alloc();
//            pAVFrameOut->format = AV_PIX_FMT_RGB32;
//            pAVFrameOut->width = pAVFrame->width;
//            pAVFrameOut->height = pAVFrame->height;
//            av_frame_get_buffer(pAVFrameOut, 1);

//            SwsContext *pSwsContext = sws_getContext(pAVFrame->width,pAVFrame->height,(AVPixelFormat)pAVFrame->format,
//                                                     pAVFrameOut->width,pAVFrameOut->height,(AVPixelFormat)pAVFrameOut->format,
//                                                     SWS_FAST_BILINEAR,0,0,0);
//            sws_scale(pSwsContext,pAVFrame->data,pAVFrame->linesize,0,
//                      pAVFrame->height,pAVFrameOut->data,pAVFrameOut->linesize);
//            sws_freeContext(pSwsContext);

            VideoData vd = { spAVFrame,
                             av_q2d(m_videoStream->time_base) * pAVFrame->pts,
                             av_q2d(m_videoStream->time_base) * pAVFrame->pkt_duration };
            m_dataMtx.lock();
            m_videoDatas.push_back(vd);
            m_dataMtx.unlock();
        }
    }
}

void AVDecoder::doSeekAudio(double pos)
{
    qDebug() << "doAudioSeek" << pos;
    SmartMutex mtx(&m_opeMtx);

    if (!m_formatContext || m_audioStreamIndex == -1 || !m_audioStream) {
        return;
    }

    m_dataMtx.lock();
    m_audioDatas.clear();
    m_dataMtx.unlock();

    QTime t;
    t.start();

    avcodec_flush_buffers(m_audioCodecContext);
    av_seek_frame(m_formatContext, m_audioStreamIndex, pos / av_q2d(m_audioStream->time_base), AVSEEK_FLAG_BACKWARD);
    qDebug() << "doAudioSeek cost" << t.elapsed() << "ms";

    QVariantMap mp;
    mp["needSeekPos"] = true;
    mp["pos"] = pos;
    requestDecode(mp);
}

void AVDecoder::doSeekVideo(double pos, SEEK_Type type)
{
    if (pos < 0) {
        return;
    }
    if (!video_hasStream()) {
        return;
    }

    m_dataMtx.lock();
    m_videoDatas.clear();
    m_dataMtx.unlock();

    setSeekingState(true);

    qDebug() << "doSeekVideo() pos:" << pos << "seek type:" << type;
    QTime t;
    t.start();

    QVariantMap vmap;
    m_opeMtx.lock();
    avcodec_flush_buffers(m_videoCodecContext);
    if (type == SEEK_POS) {
        av_seek_frame(m_formatContext, m_videoStreamIndex, pos / av_q2d(m_videoStream->time_base), AVSEEK_FLAG_BACKWARD);
        vmap["pos"] = pos;
    }
    else if (type == SEEK_LEFT_KEY) {
        av_seek_frame(m_formatContext, m_videoStreamIndex, pos / av_q2d(m_videoStream->time_base), AVSEEK_FLAG_BACKWARD);
    }
    else if (type == SEEK_RIGHT_KEY) {
        if (pos >= video_getDuration()) {
            av_seek_frame(m_formatContext, m_videoStreamIndex, pos / av_q2d(m_videoStream->time_base), AVSEEK_FLAG_BACKWARD);
        }
        else {
            av_seek_frame(m_formatContext, m_videoStreamIndex, pos / av_q2d(m_videoStream->time_base), AVSEEK_FLAG_FRAME);
        }
    }
    m_opeMtx.unlock();

//    requestDecode(vmap);
    doDecode(vmap);

    qDebug() << "doSeekVideo() cost" << t.elapsed() << "ms";

    setSeekingState(false);
}

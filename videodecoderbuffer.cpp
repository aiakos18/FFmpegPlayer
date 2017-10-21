#include "videodecoderbuffer.h"
#include "smartmutex.h"

VideoDecoderBuffer::VideoDecoderBuffer(AVDecoderCore *decoder, int videoStreamIndex, QObject *parent)
    : QObject(parent)
    , m_decoderCore(decoder)
    , m_enabledVideoStreamIndex(videoStreamIndex)
    , m_isDecodeEnd(false)
    , m_isSeeking(false)
    , m_bufferMinCount(0)
    , m_dataMtx(QMutex::Recursive)
    , m_opeMtx(QMutex::Recursive)
{
    moveToThread(&m_thread);

    if (isAvailable()) {
        m_thread.start();
        QMetaObject::invokeMethod(this, "handleRequests");
        requestDecode();
    }
}

VideoDecoderBuffer::~VideoDecoderBuffer()
{
    if (m_thread.isRunning()) {
        requestQuit();
        m_thread.quit();
        m_thread.wait();
    }
}

bool VideoDecoderBuffer::isAvailable()
{
    if (m_decoderCore == 0) {
        return false;
    }
    if (!m_decoderCore->hasVideoStream()) {
        return false;
    }
    if (!m_decoderCore->isVideoStreamEnabled(m_enabledVideoStreamIndex)) {
        return false;
    }
    return true;
}

bool VideoDecoderBuffer::isDecodeEnd()
{
    return m_isDecodeEnd;
}

void VideoDecoderBuffer::resetDecoder()
{
    if (!isAvailable()) {
        return;
    }

    SmartMutex opeMtx(&m_opeMtx);
    m_decoderCore->seekVideo(m_enabledVideoStreamIndex, 0);
    setDecodeEnd(false);

    m_dataMtx.lock();
    m_bufferedDatas.clear();
    m_dataMtx.unlock();
    requestDecode();
}

bool VideoDecoderBuffer::seek(double pos, AVDecoderCore::SEEK_Type type)
{
    if (!isAvailable()) {
        return false;
    }
    removeDecodeRequests();
    requestSeekVideo(pos, type);
    return true;
}

bool VideoDecoderBuffer::isSeeking()
{
    return m_isSeeking;
}

void VideoDecoderBuffer::setBufferMinCount(int count)
{
    if (count < 0) {
        return;
    }
    m_bufferMinCount = count;
    if (!isBuffered()) {
        requestDecode();
    }
}

int VideoDecoderBuffer::getBufferMinCount()
{
    return m_bufferMinCount;
}

int VideoDecoderBuffer::getBufferCount()
{
    if (!isAvailable()) {
        return 0;
    }

    SmartMutex dataMtx(&m_dataMtx);
    return m_bufferedDatas.count();
}

bool VideoDecoderBuffer::isBuffered()
{
    if (!isAvailable()) {
        return false;
    }
    int count = getBufferCount();
    return (m_bufferMinCount == 0) ? (count > 0) : (count >= m_bufferMinCount);
}

bool VideoDecoderBuffer::hasBufferedData()
{
    if (!isAvailable()) {
        return false;
    }

    SmartMutex dataMtx(&m_dataMtx);
    return !m_bufferedDatas.isEmpty();
}

VideoDecoderBuffer::VideoData VideoDecoderBuffer::getBufferedData()
{
    VideoData vd;
    if (!isAvailable()) {
        return vd;
    }

    SmartMutex dataMtx(&m_dataMtx);
    if (!m_bufferedDatas.isEmpty()) {
        vd = m_bufferedDatas.front();
    }
    return vd;
}

VideoDecoderBuffer::VideoData VideoDecoderBuffer::popBufferedData()
{
    VideoData vd;
    if (!isAvailable()) {
        return vd;
    }

    SmartMutex dataMtx(&m_dataMtx);
    if (!m_bufferedDatas.isEmpty()) {
        vd = m_bufferedDatas.front();
        m_bufferedDatas.pop_front();
        if (!isBuffered()) {
            requestDecode();
        }
    }
    return vd;
}

void VideoDecoderBuffer::setDecodeEnd(bool isDecodeEnd)
{
    if (isDecodeEnd == m_isDecodeEnd) {
        return;
    }
    m_isDecodeEnd = isDecodeEnd;
}

void VideoDecoderBuffer::setSeekingState(bool isSeeking)
{
    if (isSeeking == m_isSeeking) {
        return;
    }
    m_isSeeking = isSeeking;
    emit seekingStateChanged(isSeeking);
}

void VideoDecoderBuffer::handleRequests()
{
    while (1) {
        m_reqMtx.lock();
        if (m_requestList.isEmpty()) {
            m_cond.wait(&m_reqMtx);
            m_reqMtx.unlock();
            continue;
        }
        Request req = m_requestList.front();
        m_requestList.pop_front();
        m_reqMtx.unlock();

        switch (req.rid) {
        case REQUEST_DECODE:
            doDecode(req.p1);
            break;

        case REQUEST_SEEK:
            doSeek(req.p1.toMap()["pos"].toDouble(), (AVDecoderCore::SEEK_Type)req.p1.toMap()["type"].toInt());
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

void VideoDecoderBuffer::pushRequest(const Request &req)
{
    m_reqMtx.lock();
    m_requestList.push_back(req);
    m_cond.wakeAll();
    m_reqMtx.unlock();
}

void VideoDecoderBuffer::clearRequestList()
{
    m_reqMtx.lock();
    m_requestList.clear();
    m_reqMtx.unlock();
}

void VideoDecoderBuffer::removeDecodeRequests()
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

void VideoDecoderBuffer::requestDecode(QVariant p)
{
    Request req(REQUEST_DECODE, p);
    pushRequest(req);
}

void VideoDecoderBuffer::requestSeekVideo(double pos, AVDecoderCore::SEEK_Type type)
{
    QVariantMap vmap;
    vmap["pos"] = pos;
    vmap["type"] = type;
    Request req(REQUEST_SEEK, vmap);
    pushRequest(req);
}

void VideoDecoderBuffer::requestQuit()
{
    Request req(REQUEST_QUIT);
    pushRequest(req);
}

void VideoDecoderBuffer::doDecode(QVariant p)
{
    if (!isAvailable()) {
        return;
    }
    if (isBuffered()) {
        return;
    }
    if (isDecodeEnd()) {
        return;
    }

    SmartMutex opeMtx(&m_opeMtx);
    while (1) {
        if (isBuffered()) {
            emit buffered();
            return;
        }

        SPAVFrame frame;
        int averr = m_decoderCore->getVideoNextFrame(m_enabledVideoStreamIndex, frame);
        if (averr != 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "cannot get next frame";
            setDecodeEnd(true);
            return;
        }

        setDecodeEnd(false);

        VideoData vd;
        vd.frame = frame;
        vd.time = m_decoderCore->calculateVideoTimestamp(m_enabledVideoStreamIndex, frame->pts);
        vd.duration = m_decoderCore->calculateVideoTimestamp(m_enabledVideoStreamIndex, frame->pkt_duration);
        m_dataMtx.lock();
        m_bufferedDatas.push_back(vd);
        m_dataMtx.unlock();
    }
}

void VideoDecoderBuffer::doSeek(double pos, AVDecoderCore::SEEK_Type type)
{
    if (pos < 0) {
        return;
    }
    if (!isAvailable()) {
        return;
    }

    m_dataMtx.lock();
    m_bufferedDatas.clear();
    m_dataMtx.unlock();

    setSeekingState(true);

    qDebug() << __PRETTY_FUNCTION__ << "pos:" << pos << "seek type:" << type;
    QTime t;
    t.start();
    m_decoderCore->seekVideo(m_enabledVideoStreamIndex, pos, type);
    setDecodeEnd(false);
    doDecode(QVariant());
    qDebug() << __PRETTY_FUNCTION__  << "cost" << t.elapsed() << "ms";

    setSeekingState(false);
}

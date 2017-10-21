#include "audiodecoderbuffer.h"
#include "smartmutex.h"

AudioDecoderBuffer::AudioDecoderBuffer(AVDecoderCore *decoder, int audioStreamIndex, QObject *parent)
    : QObject(parent)
    , m_decoderCore(decoder)
    , m_enabledAudioStreamIndex(audioStreamIndex)
    , m_isDecodeEnd(false)
    , m_isSeeking(false)
    , m_bufferMinSize(0)
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

AudioDecoderBuffer::~AudioDecoderBuffer()
{
    if (m_thread.isRunning()) {
        requestQuit();
        m_thread.quit();
        m_thread.wait();
    }
}

bool AudioDecoderBuffer::isAvailable()
{
    if (m_decoderCore == 0) {
        return false;
    }
    if (!m_decoderCore->hasAudioStream()) {
        return false;
    }
    if (!m_decoderCore->isAudioStreamEnabled(m_enabledAudioStreamIndex)) {
        return false;
    }
    return true;
}

bool AudioDecoderBuffer::isDecodeEnd()
{
    return m_isDecodeEnd;
}

void AudioDecoderBuffer::resetDecoder()
{
    if (!isAvailable()) {
        return;
    }

    SmartMutex opeMtx(&m_opeMtx);
    m_decoderCore->seekAudio(m_enabledAudioStreamIndex, 0);
    setDecodeEnd(false);

    m_dataMtx.lock();
    m_bufferedDatas.clear();
    m_dataMtx.unlock();
    requestDecode();
}

bool AudioDecoderBuffer::seek(double time)
{
    if (!isAvailable()) {
        return false;
    }
    removeDecodeRequests();
    requestSeekAudio(time);
    return true;
}

bool AudioDecoderBuffer::isSeeking()
{
    return m_isSeeking;
}

void AudioDecoderBuffer::setBufferMinSize(int size)
{
    if (size < 0) {
        return;
    }
    m_bufferMinSize = size;
    if (!isBuffered()) {
        requestDecode();
    }
}

int AudioDecoderBuffer::getBufferMinSize()
{
    return m_bufferMinSize;
}

int AudioDecoderBuffer::getBufferSize()
{
    if (!isAvailable()) {
        return 0;
    }

    SmartMutex dataMtx(&m_dataMtx);
    quint32 size = 0;
    foreach (AudioData ad, m_bufferedDatas) {
        size += ad.data.size();
    }
    return size;
}

bool AudioDecoderBuffer::isBuffered()
{
    if (!isAvailable()) {
        return false;
    }

    int size = getBufferSize();
    return (m_bufferMinSize == 0) ? (size > 0) : (size >= m_bufferMinSize);
}

bool AudioDecoderBuffer::hasBufferedData()
{
    if (!isAvailable()) {
        return false;
    }

    SmartMutex dataMtx(&m_dataMtx);
    return !m_bufferedDatas.isEmpty();
}

AudioDecoderBuffer::AudioData AudioDecoderBuffer::getBufferedData()
{
    AudioData ad;
    if (!isAvailable()) {
        return ad;
    }

    SmartMutex dataMtx(&m_dataMtx);
    if (!m_bufferedDatas.isEmpty()) {
        ad = m_bufferedDatas.front();
    }
    return ad;
}

AudioDecoderBuffer::AudioData AudioDecoderBuffer::popBufferedData()
{
    AudioData ad;
    if (!isAvailable()) {
        return ad;
    }

    SmartMutex dataMtx(&m_dataMtx);
    if (!m_bufferedDatas.isEmpty()) {
        ad = m_bufferedDatas.front();
        m_bufferedDatas.pop_front();
        if (!isBuffered()) {
            requestDecode();
        }
    }
    return ad;
}

void AudioDecoderBuffer::setDecodeEnd(bool isDecodeEnd)
{
    if (isDecodeEnd == m_isDecodeEnd) {
        return;
    }
    m_isDecodeEnd = isDecodeEnd;
}

void AudioDecoderBuffer::setSeekingState(bool isSeeking)
{
    if (isSeeking == m_isSeeking) {
        return;
    }
    m_isSeeking = isSeeking;
    emit seekingStateChanged(isSeeking);
}

void AudioDecoderBuffer::handleRequests()
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
            doDecode();
            break;

        case REQUEST_SEEK:
            doSeek(req.p1.toDouble());
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

void AudioDecoderBuffer::pushRequest(const Request &req)
{
    m_reqMtx.lock();
    m_requestList.push_back(req);
    m_cond.wakeAll();
    m_reqMtx.unlock();
}

void AudioDecoderBuffer::clearRequestList()
{
    m_reqMtx.lock();
    m_requestList.clear();
    m_reqMtx.unlock();
}

void AudioDecoderBuffer::removeDecodeRequests()
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

void AudioDecoderBuffer::requestDecode(QVariant p)
{
    Request req(REQUEST_DECODE, p);
    pushRequest(req);
}

void AudioDecoderBuffer::requestSeekAudio(double pos)
{
    Request req(REQUEST_SEEK, pos);
    pushRequest(req);
}

void AudioDecoderBuffer::requestQuit()
{
    Request req(REQUEST_QUIT);
    pushRequest(req);
}

void AudioDecoderBuffer::doDecode()
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
            emit audioDataBuffered();
            return;
        }

        QByteArray data;
        double pts, duration;
        int averr = m_decoderCore->getAudioNextFrame(m_enabledAudioStreamIndex, data, pts, duration);
        if (averr != 0) {
            qDebug() <<  __PRETTY_FUNCTION__ << "cannot get next frame";
            setDecodeEnd(true);
            return;
        }

        setDecodeEnd(false);

        AudioData vd;
        vd.data = data;
        vd.time = pts;
        vd.duration = duration;
        m_dataMtx.lock();
        m_bufferedDatas.push_back(vd);
        m_dataMtx.unlock();
    }
}

void AudioDecoderBuffer::doSeek(double pos)
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

    qDebug() <<  __PRETTY_FUNCTION__ << "pos:" << pos;
    QTime t;
    t.start();
    m_decoderCore->seekAudio(m_enabledAudioStreamIndex, pos);
    qDebug() <<  __PRETTY_FUNCTION__ << "cost" << t.elapsed() << "ms";
    t.start();
    setDecodeEnd(false);
    doDecode();
    qDebug() <<  __PRETTY_FUNCTION__ << "cost" << t.elapsed() << "ms";

    setSeekingState(false);
}

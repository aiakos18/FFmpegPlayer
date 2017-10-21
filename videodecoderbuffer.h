#ifndef VIDEODECODERBUFFER_H
#define VIDEODECODERBUFFER_H

#include "avdecodercore.h"

class VideoDecoderBuffer : public QObject
{
    Q_OBJECT
public:
    struct VideoData {
        SPAVFrame frame;
        double time, duration;

        VideoData() : time(0.0), duration(0.0) {}
    };

public:
    explicit VideoDecoderBuffer(AVDecoderCore *decoder, int videoStreamIndex, QObject *parent = 0);
    ~VideoDecoderBuffer();

    bool isAvailable();
    bool isDecodeEnd();
    void resetDecoder();

    bool seek(double pos, AVDecoderCore::SEEK_Type type = AVDecoderCore::SEEK_LEFT_KEY);
    bool isSeeking();

    void setBufferMinCount(int count);
    int getBufferMinCount();
    int getBufferCount();
    bool isBuffered();
    bool hasBufferedData();
    VideoData getBufferedData();
    VideoData popBufferedData();

signals:
    void seekingStateChanged(bool isSeeking);
    void buffered();

protected:
    enum REQUEST_ID {
        REQUEST_DECODE,
        REQUEST_SEEK,
        REQUEST_QUIT,
    };

    struct Request {
        Request(REQUEST_ID _rid, QVariant _p = QVariant()) : rid(_rid), p1(_p) {}
        REQUEST_ID rid;
        QVariant   p1;
    };

    void setDecodeEnd(bool isDecodeEnd);
    void setSeekingState(bool isSeeking);

    Q_INVOKABLE void handleRequests();

    void pushRequest(const Request &req);
    void clearRequestList();
    void removeDecodeRequests();

    void requestDecode(QVariant p = QVariant());
    void requestSeekVideo(double pos, AVDecoderCore::SEEK_Type type);
    void requestQuit();

    void doDecode(QVariant p);
    void doSeek(double pos, AVDecoderCore::SEEK_Type type);

private:
    AVDecoderCore *m_decoderCore;
    int m_enabledVideoStreamIndex;
    bool m_isDecodeEnd;
    bool m_isSeeking;

    QThread m_thread;
    QList<Request> m_requestList;

    QList<VideoData> m_bufferedDatas;
    int m_bufferMinCount;

    QMutex m_reqMtx, m_dataMtx, m_opeMtx;
    QWaitCondition m_cond;
};

#endif // VIDEODECODERBUFFER_H

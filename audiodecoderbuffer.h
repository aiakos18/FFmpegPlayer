#ifndef AUDIODECODERBUFFER_H
#define AUDIODECODERBUFFER_H

#include "avdecodercore.h"

class AudioDecoderBuffer : public QObject
{
    Q_OBJECT
public:
    enum DECODING_State {
        DECODING_STOPPED,
        DECODING_STARTED,
        DECODING_END,
    };

    struct AudioData {
        QByteArray data;
        double time, duration;

        AudioData() : time(0.0), duration(0.0) {}
    };

public:
    explicit AudioDecoderBuffer(AVDecoderCore *decoder, int audioStreamIndex, QObject *parent = 0);

    ~AudioDecoderBuffer();

    bool isAvailable();
    bool isDecodeEnd();
    void resetDecoder();

    bool seek(double time);
    bool isSeeking();

    void setBufferMinSize(int size);
    int getBufferMinSize();
    int getBufferSize();
    bool isBuffered();
    bool hasBufferedData();
    AudioData getBufferedData();
    AudioData popBufferedData();

signals:
    void audioDataBuffered();
    void seekingStateChanged(bool isSeeking);

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
    void requestSeekAudio(double pos);
    void requestQuit();

    void doDecode();
    void doSeek(double pos);

private:
    AVDecoderCore *m_decoderCore;
    int m_enabledAudioStreamIndex;
    bool m_isDecodeEnd;
    bool m_isSeeking;

    QThread m_thread;
    QList<Request> m_requestList;

    QList<AudioData> m_bufferedDatas;
    int m_bufferMinSize;

    QMutex m_reqMtx, m_dataMtx, m_opeMtx;
    QWaitCondition m_cond;
};

#endif // AUDIODECODERBUFFER_H

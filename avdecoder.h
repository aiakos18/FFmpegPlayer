#ifndef AVDECODER_H
#define AVDECODER_H

#include <QtCore>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#include "avdecodercore.h"

#ifndef TYPEDEF_SPAVFRAME
#define TYPEDEF_SPAVFRAME
typedef QSharedPointer<AVFrame> SPAVFrame;
#endif

class AVDecoder : public QObject
{
    Q_OBJECT
public:
    enum DECODING_Type {
        DECODING_NONE = -1,
        DECODING_VIDEO,
        DECODING_AUDIO,
    };

    enum DECODING_State {
        DECODING_STOPPED,
        DECODING_STARTED,
        DECODING_END,
    };

    enum SEEK_Type {
        SEEK_POS,
        SEEK_LEFT_KEY,
        SEEK_RIGHT_KEY,
    };

    struct AudioData {
        QByteArray data;
        double time, duration;
    };

    struct VideoData {
        SPAVFrame frame;
        double time, duration;
    };

public:
    explicit AVDecoder(QObject *parent = 0);

    ~AVDecoder();

    void init();

    bool load(const QString &getFile, DECODING_Type decodingType);
    void unload();
    bool isLoaded();
    QString getFile();
    DECODING_Type decodingType();
    DECODING_State decodingState();


    // audio interfaces
    bool audio_hasStream();

    int audio_getSampleRate();
    AVSampleFormat audio_getSampleFormat();
    int audio_getSampleSize();
    int audio_getChannels();
    int audio_getBytesPerSample();
    int audio_getBytesPerFrame();
    int audio_getBytesPerSecond();

    double audio_getDuration();
    void audio_seek(double time);

    bool audio_hasBufferedData();
    bool audio_isDataBuffered();
    AudioData audio_getBufferedData();
    AudioData audio_popBufferedData();
    void audio_setMinBufferSize(int size);


    // video interfaces
    bool video_hasStream();

    int video_getWidth();
    int video_getHeight();
    AVPixelFormat video_getPixelFormat();
    double video_getFrameRate();

    double video_getDuration();
    void video_seek(double pos, SEEK_Type type = SEEK_LEFT_KEY);

    bool video_hasBufferedData();
    bool video_isDataBuffered();
    VideoData video_getBufferedData();
    VideoData video_popBufferedData();
    void video_setMinBufferCount(int count);

    // common interfaces
    bool isSeeking();

signals:
    void stateChanged(AVDecoder::DECODING_State decodingState);
    void audioDataBuffered();
    void videoDataBuffered();

    void seekingStateChanged(bool isSeeking);

protected:
    enum REQUEST_ID {
        REQUEST_DECODE,
        REQUEST_AUDIO_SEEK,
        REQUEST_VIDEO_SEEK,
        REQUEST_QUIT,
    };

    struct Request {
        Request(REQUEST_ID _rid, QVariant _p = QVariant()) : rid(_rid), p1(_p) {}
        REQUEST_ID rid;
        QVariant   p1;
    };

    void setDecodingState(DECODING_State decodingState);
    void setSeekingState(bool isSeeking);

    Q_INVOKABLE void handleRequests();

    void pushRequest(const Request &req);
    void clearRequestList();
    void removeDecodeRequests();

    void requestDecode(QVariant p = QVariant());
    void requestSeekAudio(double pos);
    void requestSeekVideo(double pos, SEEK_Type type);
    void requestQuit();

    void doDecode(QVariant p);
    void doSeekAudio(double pos);
    void doSeekVideo(double pos, SEEK_Type type);

private:
    QString m_file;
    DECODING_Type m_decodingType;

    AVDecoderCore m_decoderHelper;

    DECODING_State m_decodingState;

    QThread m_thread;
    QList<Request> m_requestList;

    AVFormatContext *m_formatContext;
    AVStream *m_videoStream, *m_audioStream;
    AVCodecParameters *m_videoCodecPar, *m_audioCodecPar;
    AVCodecContext *m_videoCodecContext, *m_audioCodecContext;
    int m_videoStreamIndex, m_audioStreamIndex;
    AVSampleFormat m_audioOutSampleFmt;
    uint m_audioOutputChannels, m_audioOutputChannelLayout;
    SPAVFrame m_videoCover;

    QList<AudioData> m_audioDatas;
    int m_audioMinBufferSize;

    QList<VideoData> m_videoDatas;
    int m_videoMinBufferFrameCount;

    QMutex m_reqMtx, m_dataMtx, m_opeMtx;
    QWaitCondition m_cond;

    bool m_isSeeking;
};

#endif // AVDECODER_H

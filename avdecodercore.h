#ifndef AVDECODERCORE_H
#define AVDECODERCORE_H

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

#include <QtCore>

#ifndef TYPEDEF_SPAVFRAME
#define TYPEDEF_SPAVFRAME
typedef QSharedPointer<AVFrame> SPAVFrame;
#endif

class AVDecoderCore
{
public:
    enum SEEK_POS_Type {
        SEEK_POS_LEFT_KEY,
        SEEK_POS_RIGHT_KEY,
        SEEK_POS_NEAREST,
    };

    enum SEEK_Type {
        SEEK_USER_SET,
        SEEK_LEFT_KEY,
        SEEK_RIGHT_KEY,
    };

public:
    AVDecoderCore();
    ~AVDecoderCore();

    bool load(const QString &file);
    void unload();
    bool isLoaded();
    QString getFile();
    void showInfo();

    // audio interfaces
    bool hasAudioStream();
    int getAudioStreamCount();

    QString getAudioStreamTitle(int index);

    bool enableAudioStream(int index);
    void disableAudioStream(int index);
    bool isAudioStreamEnabled(int index);

    bool hasEnabledAudioStream();
    int getEnabledAudioStreamCount();
    QList<int> getEnabledAudioIndexes();

    double getAudioDuration(int index);

    AVSampleFormat getAudioSampleFormat(int index);
    int getAudioSampleSize(int index);
    int getAudioSampleRate(int index);
    int getAudioChannels(int index);
    uint64_t getAudioChannelLayout(int index);
    int getAudioBytesPerSample(int index);
    int getAudioBytesPerFrame(int index);
    int getAudioBytesPerSecond(int index);

    AVSampleFormat getAudioOutputSampleFormat(int index);
    int getAudioOutputSampleSize(int index);
    int getAudioOutputSampleRate(int index);
    int getAudioOutputChannels(int index);
    uint64_t getAudioOutputChannelLayout(int index);
    int getAudioOutputBytesPerSample(int index);
    int getAudioOutputBytesPerFrame(int index);
    int getAudioOutputBytesPerSecond(int index);

    bool seekAudio(int index, double pos);
    int getAudioNextFrame(int index, QByteArray &data, double &pts, double &duration);


    // video interfaces
    bool hasVideoStream();
    int getVideoStreamCount();

    bool enableVideoStream(int index);
    void disableVideoStream(int index);
    bool isVideoStreamEnabled(int index);

    bool hasEnabledVideoStream();
    int getEnabledVideoStreamCount();
    QList<int> getEnabledVideoIndexes();

    double getVideoDuration(int index);
    double getVideoFrameRate(int index);
    int getVideoWidth(int index);
    int getVideoHeight(int index);
    AVPixelFormat getVideoPixelFormat(int index);

    bool seekVideo(int index, double pos, SEEK_Type type = SEEK_LEFT_KEY);
    bool getVideoSeekPos(int index, double t, SEEK_POS_Type type, double &spos);
    bool getVideoKeyFramePosList(int index, QList<double> &posList);

    int getVideoNextFrame(int index, SPAVFrame &frame);
    int getVideoNextFrame(int index, SPAVFrame &frame, double pos);
    int getVideoNextKeyFrame(int index, SPAVFrame &frame);

    double calculateVideoTimestamp(int index, long long t);


    bool hasCover();
    int getCoverCount();
    bool getCover(int index, SPAVFrame &frame);

protected:
    struct StreamParty {
        int streamIndex;
        AVMediaType streamType;
        AVFormatContext *formatContext;
        AVStream *stream;
        AVCodecParameters *codecPar;
        AVCodecContext *codecContext;

        StreamParty()
            : streamIndex(-1), streamType(AVMEDIA_TYPE_UNKNOWN), formatContext(0), stream(0), codecPar(0), codecContext(0)
        {}
    };

    struct AudioStreamParty {
        StreamParty streamParty;

        double duration;
        int64_t bitrate;
        int frameSize;

        AVSampleFormat inputSampleFormat;
        int inputSampleSize;
        int inputSampleRate;
        int inputChannels;
        uint64_t inputChannelLayout;
        int inputBytesPerSample;
        int inputBytesPerFrame;
        int inputBytesPerSecond;

        AVSampleFormat outputSampleFormat;
        int outputSampleSize;
        int outputSampleRate;
        int outputChannels;
        uint64_t outputChannelLayout;
        int outputBytesPerSample;
        int outputBytesPerFrame;
        int outputBytesPerSecond;

        QMap<QString,QString> metadata;

        AudioStreamParty()
            : duration(0.0), bitrate(0), frameSize(0)
            , inputSampleFormat(AV_SAMPLE_FMT_NONE), inputSampleSize(0), inputSampleRate(0)
            , inputChannels(0), inputChannelLayout(0)
            , inputBytesPerSample(0), inputBytesPerFrame(0), inputBytesPerSecond(0)
            , outputSampleFormat(AV_SAMPLE_FMT_NONE), outputSampleSize(0), outputSampleRate(0)
            , outputChannels(0), outputChannelLayout(0)
            , outputBytesPerSample(0), outputBytesPerFrame(0), outputBytesPerSecond(0)
        {}
    };

    struct VideoStreamParty {
        StreamParty streamParty;
        StreamParty streamParty2;   // for query

        double duration;
        int64_t bitrate;
        double frameRate;
        int width, height;
        AVPixelFormat format;

        QMap<QString,QString> metadata;

        VideoStreamParty()
            : duration(0.0), bitrate(0), frameRate(0.0)
            , width(0), height(0), format(AV_PIX_FMT_NONE)
        {}
    };

    struct CoverStreamParty {
        StreamParty streamParty;

        SPAVFrame cover;
        QMap<QString,QString> metadata;

        CoverStreamParty()
        {}
    };

    struct SubtitleText {
        QString text;
        int fontSize;

        SubtitleText() : fontSize(16)
        {}
    };
    struct SubtitleStreamParty {
        StreamParty streamParty;

        QString title;
        QList<SubtitleText> subtitleTexts;

        QMap<QString,QString> metadata;

        SubtitleStreamParty()
        {}
    };

protected:
    bool initStreamParty(StreamParty *sp, const QString &file);
    void uninitStreamParty(StreamParty *sp);

//    double getAudioDuration(AudioStreamParty *asp);
//    int getVideoBitrate(AudioStreamParty *asp);

//    AVSampleFormat getAudioSampleFormat(AudioStreamParty *asp);
//    int getAudioSampleSize(AudioStreamParty *asp);
//    int getAudioSampleRate(AudioStreamParty *asp);
//    int getAudioChannels(AudioStreamParty *asp);
//    uint64_t getAudioChannelLayout(AudioStreamParty *asp);
//    int getAudioBytesPerSample(AudioStreamParty *asp);
//    int getAudioBytesPerFrame(AudioStreamParty *asp);
//    int getAudioBytesPerSecond(AudioStreamParty *asp);

//    AVSampleFormat getAudioOutputSampleFormat(AudioStreamParty *asp);
//    int getAudioOutputSampleSize(AudioStreamParty *asp);
//    int getAudioOutputSampleRate(AudioStreamParty *asp);
//    int getAudioOutputChannels(AudioStreamParty *asp);
//    uint64_t getAudioOutputChannelLayout(AudioStreamParty *asp);
//    int getAudioOutputBytesPerSample(AudioStreamParty *asp);
//    int getAudioOutputBytesPerFrame(AudioStreamParty *asp);
//    int getAudioOutputBytesPerSecond(AudioStreamParty *asp);

//    double getVideoDuration(VideoStreamParty *vsp);
//    double getVideoBitrate(VideoStreamParty *vsp);
//    double getVideoFrameRate(VideoStreamParty *vsp);
//    int getVideoWidth(VideoStreamParty *vsp);
//    int getVideoHeight(VideoStreamParty *vsp);
//    AVPixelFormat getVideoPixelFormat(VideoStreamParty *vsp);

    int getVideoNextFrame(int index, SPAVFrame &frame, bool specifyPos, double pos, bool specifyKeyFrame);

    void getSubtitle(SubtitleStreamParty *ssp);

    bool getCover(AVFormatContext *formatContext, int streamIndex, SPAVFrame &frame);

protected:
    QString m_file;

    AVFormatContext *m_formatContext;

    QList<AudioStreamParty*> m_audioStreamParties;
    QList<VideoStreamParty*> m_videoStreamParties;
    QList<CoverStreamParty*> m_coverStreamParties;
    QList<SubtitleStreamParty*> m_subtitleStreamParties;

    QThread m_decodeThread;
    int m_taskid;
};

#endif // AVDECODERCORE_H

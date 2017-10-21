#ifndef AVPLAYCONTROL_H
#define AVPLAYCONTROL_H

#include <QtCore>
#include <QtOpenGL>

#include "audioplayerbase.h"
#include "videodecoderbuffer.h"

class AVPlayControl : public QObject
{
    Q_OBJECT
public:
    enum PlaybackState {
        Stopped, Playing, Paused,
    };

public:
    AVPlayControl();
    ~AVPlayControl();

    void init();

    bool load(const QString &file);
    void unload();
    bool isLoaded();
    QString getFile();

    bool isAudioAvailable();
    bool hasAudioStream();
    int getAudioStreamCount();
    QString getAudioStreamTitle(int index);
    int getCurrentAudioStreamIndex();
    void changeAudioStream(int index);
    void changeAudioStream(const QString &title);

    bool isVideoAvailable();

    void play();
    void pause();
    bool isPlaying();

    double getDuration();
    double getPosition();
    void seek(double pos);
    bool getSeekPos(double t, double &spos);
    bool isSeeking();

    int getVideoWidth();
    int getVideoHeight();

    bool hasCover();
    int getCoverCount();
    bool getCover(int index, SPAVFrame &frame);

signals:
    void fileChanged(QString file);
    void playbackStateChanged(bool isPlaying);
    void videoFrameUpdated(SPAVFrame frame);
    void positionChanged(double pos);

protected slots:
    void onVideoShowTimerTimeout();

    void onAudioPlayerPlaybackStateChanged(bool isPlaying);
    void onAudioPlayerPositionChanged(double getPosition);

    void onVideoDecoderBuffered();

protected:
    void setFile(const QString &file);
    void setPlaybackState(bool isPlaying);
    void setPosition(double position);

    void updateVideo();
    void syncVideo2Audio(double postion);

    void checkPlaybackState();

private:
    QString m_file;
    AVDecoderCore *m_decoderCore;
    int m_enabledAudioStreamIndex, m_enabledVideoStreamIndex;
    AudioPlayerBase *m_audioPlayer;
    VideoDecoderBuffer *m_videoDecoderBuffer;
    bool m_isPlaying;
    double m_position;

    QTimer m_videoShowTimer;

};

#endif // AVPLAYCONTROL_H

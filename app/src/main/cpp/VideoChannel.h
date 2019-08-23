//
// Created by Eric on 2019/8/13.
//

#ifndef FFMPEG_VIDEOCHANNEL_H
#define FFMPEG_VIDEOCHANNEL_H


#include "BaseChannel.h"
#include "AudioChannel.h"

class VideoChannel : public BaseChannel {
    typedef void (*RenderFunc) (uint8_t *,int,int,int);

public:
    VideoChannel(int index,AVCodecContext *avCodecContext,int fps,AVRational timebase);

    ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderFunc(RenderFunc func);

    void setAudioChannel(AudioChannel *audioChannel);

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderFunc render_func;
    int fps;
    AudioChannel *audioChannel = 0;
};


#endif //FFMPEG_VIDEOCHANNEL_H

//
// Created by Eric on 2019/8/13.
//

#ifndef FFMPEG_VIDEOCHANNEL_H
#define FFMPEG_VIDEOCHANNEL_H


#include "BaseChannel.h"

class VideoChannel : public BaseChannel {
    typedef void (*RenderFunc) (uint8_t *,int,int,int);

public:
    VideoChannel(int index,AVCodecContext *avCodecContext,int fps);

    ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderFunc(RenderFunc func);

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderFunc render_func;
    int fps;
};


#endif //FFMPEG_VIDEOCHANNEL_H

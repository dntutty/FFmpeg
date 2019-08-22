//
// Created by Eric on 2019/8/13.
//

#ifndef FFMPEG_GHSFFMPEG_H
#define FFMPEG_GHSFFMPEG_H


#include "JavaCallHelper.h"
#include "AudioChannel.h"
#include "VideoChannel.h"
#include <cstring>
#include <pthread.h>
#include "marco.h"
extern "C" {
#include <libavformat/avformat.h>
}


class GHSFFmpeg {
    typedef void (*RenderFunc) (uint8_t *,int,int,int);
public:
    GHSFFmpeg(JavaCallHelper *javaCallHelper,char* dataSource);

    ~GHSFFmpeg();


    void prepare();

    void _prepare();

    void start();

    void _start();

    void setRenderFunc(RenderFunc render_func);

private:
    JavaCallHelper *javaCallHelper = 0;
    AudioChannel *audioChannel = 0;
    VideoChannel *videoChannel = 0;
    char *dataSource;
    pthread_t pid_prepare;
    pthread_t pid_start;
    bool is_playing;
    AVFormatContext *formatContext = 0;
    RenderFunc render_func;
};


#endif //FFMPEG_GHSFFMPEG_H

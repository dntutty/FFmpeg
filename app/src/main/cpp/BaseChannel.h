//
// Created by Eric on 2019/8/13.
//

#ifndef FFMPEG_BASECHANNEL_H
#define FFMPEG_BASECHANNEL_H
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
}

#include "safe_queue.h"
#include "marco.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

/**
 * VideoChannel和AudioChannel的父类
 */
class BaseChannel {
public:                                                                       //初始化列表
    BaseChannel(int index, AVCodecContext *codecContext, AVRational timebase) : index(index),
                                                                                codecContext(codecContext),
                                                                                timebase(timebase) {
        LOGE("CHANNEL构造");
        this->packets.setReleaseFunc(releaseAvPacket);
        this->frames.setReleaseFunc(releaseAvFrame);
    }

    //虚析构函数
    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    static void releaseAvPacket(AVPacket **packet) {
        if (packet) {
            av_packet_free(packet);
            *packet = 0;
        }
    }

    static void releaseAvFrame(AVFrame **frame) {
        if (frame) {
            av_frame_free(frame);
            *frame = 0;
        }
    }

    //纯虚函数（抽象方法）
    virtual void stop() = 0;

    virtual void start() = 0;

    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    int index;
    bool is_playing = 0;
    AVCodecContext *codecContext;//todo 未释放
    AVRational timebase;
    double audio_time;

};


#endif //FFMPEG_BASECHANNEL_H

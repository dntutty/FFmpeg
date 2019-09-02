//
// Created by Eric on 2019/8/13.
//




#include "VideoChannel.h"

/**
 * 丢包
 * @param q
 */
void dropAVPacket(queue<AVPacket *> &q) {
    while (!q.empty()) {
        AVPacket *avPacket = q.front();
        //I 帧、B 帧 、P 帧------>Between,Previous
        //不能丢I帧,AV_PKT_FLAG_KEY: I帧（关键帧）
        if (avPacket->flags != AV_PKT_FLAG_KEY) {
            BaseChannel::releaseAvPacket(&avPacket);
            q.pop();
        } else {
            break;
        }
    }
}

/**
 * 丢包
 * @param q
 */
void dropAVFrame(queue<AVFrame *> &q) {
    if (!q.empty()) {
        AVFrame *avframe = q.front();
        BaseChannel::releaseAvFrame(&avframe);
        q.pop();

    }
}

VideoChannel::VideoChannel(int index, AVCodecContext *codecContext, int fps, AVRational timebase)
        : BaseChannel(index,
                      codecContext, timebase) {
    this->fps = fps;
    packets.setSyncFunc(dropAVPacket);
    frames.setSyncFunc(dropAVFrame);
}

VideoChannel::~VideoChannel() {
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
}

void *task_video_decode(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->video_decode();
    return 0;
}

void *task_video_play(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->video_play();
    return 0;
}

void VideoChannel::start() {
    is_playing = true;
    //设置队列状态为工作状态
    packets.setWork(1);
    frames.setWork(1);
    //解码
    pthread_create(&pid_video_decode, NULL, task_video_decode, this);

    pthread_create(&pid_video_play, NULL, task_video_play, this);
}


/**
 * 真正视频解码
 */
void VideoChannel::video_decode() {
    AVPacket *packet = NULL;
    while (is_playing) {
        int ret = packets.pop(packet);
        if (!is_playing) {
            //如果停止播放了，跳出循环 释放packet
            LOGE("解码停止了");
            break;
        }
        if (!ret) {
            //取数据包失败
            LOGE("解码失败了");
            continue;
        }

        //拿到了视频数据包（编码压缩了的），需要把数据包给解码器进行解码
        ret = avcodec_send_packet(codecContext, packet);
        if (ret) {
            //往解码器发送数据失败
            LOGE("往解码器发送数据失败");
            break;
        }
        releaseAvPacket(&packet);//释放packet,后面不需要了
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            //重来
            continue;
        } else if (ret) {
            //从解码器获取数据失败，跳出循环
            LOGE("从解码器获取数据失败");
            break;
        }
        //ret == 0 数据收发正常，成功获取到了解码后的视频原始数据包 AVFrame,格式是yuv
        //对frame进行处理（渲染播放）直接写？？
        while (is_playing && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        frames.push(frame);

    }//end while
    releaseAvPacket(&packet);
}


void VideoChannel::video_play() {
    AVFrame *frame;
    //要注意对原始数据进行格式转换：yuv > rgba
    //yuv:400x800 > rgba:400x800
//    SwsContext *sws_ctx = sws_alloc_context();
    uint8_t *dst_data[4];
    int dst_linesize[4];
    SwsContext *sws_ctx = sws_getContext(codecContext->width, codecContext->height,
                                         codecContext->pix_fmt, codecContext->width,
                                         codecContext->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL,
                                         NULL, NULL);
    //给dst_data dst_linesize 申请内存空间
    av_image_alloc(dst_data, dst_linesize, codecContext->width, codecContext->height,
                   AV_PIX_FMT_RGBA, 1);
    //根据fps(传入的流的平均帧率来控制每一帧的延时时间
    //sleep : fps --> delay时间
    //1/fps == delay
    double delay_time_per_frame = 1.0 / fps;
    while (is_playing) {
        int ret = frames.pop(frame);
        if (!is_playing) {
            //如果停止了播放，跳出循环，释放packet
            LOGE("停止");
            break;
        }
        if (!ret) {
            //数据获取失败
            LOGE("数据获取失败");
            continue;
        }
        //取到了yuv原始数据，下面要进行格式转换
        sws_scale(sws_ctx, frame->data, frame->linesize, 0, codecContext->height, dst_data,
                  dst_linesize);
        //进行休眠
        //每一帧还有自己的额外延时时间
        double extra_delay = frame->repeat_pict / (2 * fps);
        double real_delay = delay_time_per_frame + extra_delay;
//        av_usleep(real_delay * 1000000);//直接以视频播放规则来播放
        //不能了，需要根据音频的播放时间来判断
        //获取视频的播放时间

        double video_time = frame->best_effort_timestamp * av_q2d(timebase);

        if (audioChannel) {
            double audio_time = audioChannel->audio_time;
            //获取音视频播放的时间差
            double time_diff = video_time - audio_time;
            if (time_diff > 0) {//视频播放的比较快，等音频（sleep)
                //自然播放状态下，time_diff的值不会很大
                //但是，seek后time_diff的值可能很大，导致休眠时间太久
                if (time_diff > 1) {
                    av_usleep(real_delay * 2 * 1000000);
                } else {
                    av_usleep((real_delay + time_diff) * 1000000);

                }
            } else if (time_diff < 0) {//音频播放的较快，追音频（尝试丢包）
                //视频包：packet和frame
                if (fabs(time_diff) > 0.05) {
                    //时间差大于0.05,有明显的延迟感
                    //丢包:要操作队列中的数据：一定要小心
                    packets.sync();
//                    frames.sync();
                    continue;
                }
            }
        } else {
            //没有音频，类似gif
            av_usleep(real_delay * 1000000);
        }



        //dst_data:AV_PIX_FMT_RGBA格式的数据
        //渲染，回调出去>native-lib里
        //渲染一副图像，需要什么信息?
        //宽高！> 图象的尺寸
        //图像的内容!（数据）>图像怎么画
        //需要：1.data;2.linesize;3.width;4,height
        render_func(dst_data[0], dst_linesize[0], codecContext->width, codecContext->height);
        releaseAvFrame(&frame);
    }//end while

    releaseAvFrame(&frame);
    is_playing = 0;
    av_freep(&dst_data[0]);
    sws_freeContext(sws_ctx);
    //MediaCodec
}

void VideoChannel::setRenderFunc(RenderFunc render_func) {
    this->render_func = render_func;
}

void VideoChannel::setAudioChannel(AudioChannel *audioChannel) {
    this->audioChannel = audioChannel;
}


void VideoChannel::stop() {
    is_playing = 0;
    packets.setWork(0);
    frames.setWork(0);
}
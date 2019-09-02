//
// Created by Eric on 2019/8/13.
//

#include "GHSFFmpeg.h"


GHSFFmpeg::GHSFFmpeg(JavaCallHelper *javaCallHelper, char *dataSource) {

    this->javaCallHelper = javaCallHelper;
//    this->dataSource = dataSource;
    //这里的dataSource是从Java传过来的字符串，通过jni接口转成了c++的字符串。
    //在jni方法中被释放掉了，导致dataSource变成悬空指针（指向一块已经释放了的内存）
    //解决办法
    //内存拷贝,strlen获取字符串长度，strcpy:拷贝字符串

    //java: "hello"
    //c 字符串以\0结尾 : "hello\0"
    this->dataSource = new char[strlen(dataSource) + 1];
    strcpy(this->dataSource, dataSource);
}

GHSFFmpeg::~GHSFFmpeg() {
    DELETE(dataSource);
    DELETE(javaCallHelper);
//    if(dataSource) {
//        delete dataSource;
//        dataSource = 0;
//    }
//
//    if(javaCallHelper) {
//        delete javaCallHelper;
//        javaCallHelper = 0;
//
//    }

}


/**
 * 准备线程pid_prepare真正执行的函数
 * @param args
 * @return
 */
void *task_prepare(void *args) {
    //打开输入
    GHSFFmpeg *ghsffmpeg = static_cast<GHSFFmpeg *>(args);
    ghsffmpeg->_prepare();
    return 0;//一定一定一定要返回0！！！
}

/**
 * 播放准备
 * 可能是主线程
 * 解封装流程
 * ffmpeg的doc/samples/
 */
void GHSFFmpeg::prepare() {
    //可以直接进行解码api调用么？
    //线程问题
    //文件io流问题
    //直播：网络
    //不能直接调用

    pthread_create(&pid_prepare, 0, task_prepare, this);
}

void *task_start(void *args) {
    GHSFFmpeg *ghsffmpeg = static_cast<GHSFFmpeg *>(args);
    ghsffmpeg->_start();
    return 0;//一定一定一定要返回0！！！
}

/**
 * 准备完成开始播放
 */
void GHSFFmpeg::start() {
    is_playing = 1;
    if (videoChannel) {
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->start();
    }
    if (audioChannel) {
        audioChannel->start();
    }
    pthread_create(&pid_start, 0, task_start, this);
}

//设置为友元函数
void *task_stop(void *args) {
    GHSFFmpeg *ghsfFmpeg = static_cast<GHSFFmpeg *>(args);
    ghsfFmpeg->is_playing = 0;
    //    pthread_join,这里调用了后会阻塞主线程，可能会引发ANR
    pthread_join(ghsfFmpeg->pid_prepare, 0);//等线程执行完再执行

    if (ghsfFmpeg->formatContext) {
        avformat_close_input(&ghsfFmpeg->formatContext);
        avformat_free_context(ghsfFmpeg->formatContext);
        ghsfFmpeg->formatContext = 0;
    }
    DELETE(ghsfFmpeg->videoChannel);
    DELETE(ghsfFmpeg->audioChannel);
    DELETE(ghsfFmpeg);
    return 0;//一定要返回0！！！
}


void GHSFFmpeg::_prepare() {
    LOGE("native 执行线程");
    formatContext = avformat_alloc_context();
    AVDictionary *options = 0;
    av_dict_set(&options, "timeout", "3000000", 0);//设置超时时间为10秒，单位是微秒
    int ret = avformat_open_input(&formatContext, dataSource, 0, &options);
    av_dict_free(&options);
    if (ret) {
        //失败，回调给用户即：java
        LOGE("打开媒体失败:%s", av_err2str(ret));
//        javaCallHelper jni 回调java方法
        javaCallHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
        //todo 作业：反射通知java
        //1 反射java 属性(成员/静态)
        //2 反射java 方法(成员/静态)
        //3 子线程反射
        return;
    }
    //2.查找媒体中的流信息
    ret = avformat_find_stream_info(formatContext, 0);
    if (ret < 0) {
        //todo 作业
        LOGE("查找媒体中的流信息失败:%s", av_err2str(ret));
        javaCallHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        return;
    }
    LOGE("查找媒体中的流信息成功");


    //这里的i就是后面203行的packet->stream_index
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        //3.获取媒体流（音频或视频）
        AVStream *stream = formatContext->streams[i];
        //4.获取编解码这段流的参数
        AVCodecParameters *codecParameters = stream->codecpar;
        //5.通过参数中的id(编解码的方式），来查找当前流的解码器
        AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            //todo 作业
            LOGE("查找当前流的解码器失败");
            javaCallHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            return;
        }
        LOGE("查找当前流的解码器成功");

        //6 创建解码器上下文
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        //7 设置解码器上下文的参数
        ret = avcodec_parameters_to_context(codecContext, codecParameters);
        if (ret < 0) {
            //todo 作业
            LOGE("设置解码器上下文失败:%s", av_err2str(ret));
            javaCallHelper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMTERS_FAIL);
            return;
        }
        LOGE("设置解码器上下文成功");

        //8 打开解码器
        ret = avcodec_open2(codecContext, codec, 0);
        if (ret) {
            //todo 作业
            LOGE("打开解码器失败:%s", av_err2str(ret));
            javaCallHelper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            return;
        }
        LOGE("打开解码器成功");
        AVRational timebase = stream->time_base;
        //判断流类型（音频还是视频？）
        if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频
            LOGE("打开音频流");
            audioChannel = new AudioChannel(i, codecContext, timebase);
        } else if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            //视频
            LOGE("打开视频流");
            AVRational fram_rate = stream->avg_frame_rate;
            //int fps = fram_rate.num / fram_rate.den;
            int fps = static_cast<int>(av_q2d(fram_rate));

            videoChannel = new VideoChannel(i, codecContext, fps, timebase);
            LOGE("打开视频流2");

            videoChannel->setRenderFunc(render_func);
        }

    }//end for

    if (!audioChannel && !videoChannel) {
        //既没有音频也没有视频
        //todo 作业：反射java
        LOGE("没有音视频");
        javaCallHelper->onError(THREAD_CHILD, FFMPEG_NO_MEDIA);
        return;
    }

    //准备好了,反射通知java
    LOGE("准备完毕");
    if (javaCallHelper) {
        javaCallHelper->onPrePared(THREAD_CHILD);
    }
}

/**
 * 真正执行解码播放
 */
void GHSFFmpeg::_start() {
    while (is_playing) {
        /**
         * 内存泄露点1
         * 控制 视频packets队列
         */
        if (videoChannel && videoChannel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
//        内存泄漏点1（控制音频packets队列）
        if (audioChannel && audioChannel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        AVPacket *packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet);
        if (!ret) {

            //要判断流类型是视频还是音频
            if (videoChannel && packet->stream_index == videoChannel->index) {
                //往视频编码数据包队列中添加数据
                videoChannel->packets.push(packet);
            } else if (audioChannel && packet->stream_index == audioChannel->index) {
                //往音频编码数据包队列中添加数据
                audioChannel->packets.push(packet);
            }
        } else if (ret == AVERROR_EOF) {
            //表示读完了
            //要考虑读完了，是否播放完了的情况
            LOGE("读取音频视频数据结束");
            if (videoChannel->packets.is_empty() && videoChannel->frames.is_empty() &&
                audioChannel->packets.is_empty() && audioChannel->frames.is_empty()) {
//                播放完了
                av_packet_free(&packet);
                break;
            }
        } else {
            //todo 作业
            LOGE("读取音频视频数据包失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_READ_PACKETS_FAIL);
            }
            av_packet_free(&packet);
            break;
        }
    }//end while

    is_playing = 0;
    //停止解码播放(音频和视频)
    videoChannel->stop();
    audioChannel->stop();
}


void GHSFFmpeg::setRenderFunc(GHSFFmpeg::RenderFunc render_func) {
    this->render_func = render_func;
}

/**
 * 停止播放
 */
void GHSFFmpeg::stop() {
    is_playing = 0;
    javaCallHelper = 0;//prepare阻塞中停止了，还是回调给”java准备好了“
//    在主线程中，要保证——prepare方法(子线程中)执行完再释放(主线程)
//    pthread_join,这里调用了后会阻塞主线程，可能会引发ANR
    pthread_join(pid_prepare, 0);//等线程执行完再执行

//    既然在主线程会引发ANR，那么我们到子线程中去释放
    pthread_create(&pid_stop, 0, task_stop, this);//创建stop子线程
//    if(formatContext) {
//        avformat_close_input(&formatContext);
//        avformat_free_context(formatContext);
//        formatContext = 0;
//    }
//    if (videoChannel) {
//        videoChannel->stop();
//    }
//    if (audioChannel) {
//        audioChannel->stop();
//    }

}


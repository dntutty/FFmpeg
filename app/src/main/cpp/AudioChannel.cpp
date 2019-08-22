//
// Created by Eric on 2019/8/13.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int index, AVCodecContext *codecContext) : BaseChannel(index,

                                                                                  codecContext) {
    //缓冲区大小如何定？
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100;
    //2(通道数） * 2(16bit = 2 字节）* 44100（采样率）
    out_buffers_size = out_channels * out_sample_size * out_sample_rate;
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));
    memset(out_buffers, 0, out_buffers_size);
}

AudioChannel::~AudioChannel() {

}

void *task_audio_decode(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_decode();
    return 0;
}

void *task_audio_play(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_play();
    return 0;
}

void AudioChannel::start() {
    is_playing = true;
    //设置队列状态为工作状态
    packets.setWork(1);
    frames.setWork(1);
    //解码
    pthread_create(&pid_audio_decode, NULL, task_audio_decode, this);

    pthread_create(&pid_audio_play, NULL, task_audio_play, this);
}

void AudioChannel::stop() {

}

void AudioChannel::audio_decode() {
    AVPacket *packet = 0;
    while (is_playing) {
        int ret = packets.pop(packet);
        if (!is_playing) {
            //解码停止了，跳出循环，释放packet
            break;
        }
        if (!ret) {
            LOGE("取出音频包失败");
            break;
        }
        ret = avcodec_send_packet(codecContext, packet);
        if (ret == AVERROR_EOF) {
            LOGE("解码器被释放，没有新的数据包可以发送给它");
        } else if (ret) {
            LOGE("发送音频数据包到解码器失败");
            break;
        }
        releaseAvPacket(&packet);
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            LOGE("解码器被释放，没有新的音频frame接收");
            break;
        } else if (ret) {
            LOGE("从解码器接收音频数据包失败");
            break;
        }
        while (is_playing && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        frames.push(frame);//PCM帧数据

    } //end while
    releaseAvPacket(&packet);
}

//4.3 创建回调函数
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(context);
    int pcm_size = audioChannel->getPCM();
    (*bq)->Enqueue(bq, nextBuffer, pcm_size);
}

void AudioChannel::audio_play() {
    /**
     * 1.创建引擎并获取引擎接口
     */
    SLresult result;
    //1.1 创建引擎对象：SLObjectItf engineObject
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //1.2 初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //1.3 获取引擎接口 SLEngineItf engineInterface
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    /**
     * 2.设置混音器
     */
    //2.1 创建混音器： SLObjectItf outputMixObject
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //2.2 初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    /**
     * 3. 创建播放器
     */
    //3.1 配置输入声音信息
    //创建buffer缓冲类型的队列 2个队列
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    //pcm数据格式
    //SL_DATAFORMAT_PCM: 数据格式为pcm格式
    //2: 双声道
    //SL_SAMPLINGRATE_44_1: 采样率为44100
    //SL_PCMSAMPLEFORMAT_FIXED_16: 采样率格式为16bit
    //SL_PCMSAMPLEFROMAT_FIXED_16: 数据大小为16bit
    //SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT:左右声道（双声道）
    //SL_BYTEORDER_LITTLEENDIAN: 小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};

    //数据源将上述配置信息放到这个数据源中
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    //3.2 配置音轨（输出）
    //设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};
    //需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    //3.3 创建播放器
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &audioSrc,
                                                   &audioSnk, 1, ids, req);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //3.4 初始化播放器： SLObjectItf bqPlayerObject
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //3.5 获取播放器接口：SLPlayItf bqPalyerPlay
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    /**
     * 4. 设置播放回调函数
     */
    //(*bqPalyerObject)->是解引用
    //4.1 获取播放器队列接口:SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);

    //4.2 设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq,void *context)
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    /**
     * 5. 设置播放器状态为播放状态
     */
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    /**
     * 6.手动播放回调函数
     */
    bqPlayerCallback(bqPlayerBufferQueue, this);


}

/**
 * 获取pcm数据
 * @return 数据大小
 */
int AudioChannel::getPCM() {
    AVFrame *frame = 0;
    SwrContext *swr_ctx = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                                             out_sample_rate, codecContext->channel_layout,
                                             codecContext->sample_fmt, codecContext->sample_rate, 0,
                                             0);
    //初始化重采样的上下文
    swr_init(swr_ctx);
    while (is_playing) {
        int ret = frames.pop(frame);
        if (!is_playing) {
            //如果停止播放了，跳出循环 释放packet
            break;
        }
        if (!ret) {
            //取数据包失败
            continue;
        }
        //pcm 数据在frame中
        //这里获得的解码后pcm的音频原始数据，有可能与创建的播放器中设置的pcm格式不一样
        //重采样？example
        swr_convert(swr_ctx, &out_buffers, out_buffers_size,
                    const_cast<const uint8_t **>(frame->data), frame->nb_samples);


    }//end while

    return 0;
}



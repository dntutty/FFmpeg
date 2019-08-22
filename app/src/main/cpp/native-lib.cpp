#include <jni.h>
#include <string>
#include "GHSFFmpeg.h"

#include <android/native_window_jni.h>

//extern "C" {
//#include <libavutil/avutil.h>
//#include <libavformat/avformat.h>
//}

JavaVM *javaVM = 0;
JavaCallHelper *javaCallHelper = 0;
GHSFFmpeg *ghsfFmpeg = 0;
ANativeWindow *window = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//静态初始化mutex
//extern "C" JNIEXPORT jstring JNICALL
//Java_com_dntutty_ffmpeg_MainActivity_stringFromJNI(
//        JNIEnv *env,
//        jobject /* this */) {
//    std::string hello = "Hello from C++";
//    return env->NewStringUTF(av_version_info());
//}


jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVM = vm;
    return JNI_VERSION_1_6;
}

void renderFrame(uint8_t *src_data, int src_linesize, int width, int height) {
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(window, &buffer, 0)) {
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }

    //把buffer中的数据进行赋值(修改)
    uint8_t *dst_data = static_cast<uint8_t *>(buffer.bits);
    int dst_linesize = buffer.stride * 4;//ARGB
    //逐行拷贝
    for (int i = 0; i < buffer.height; ++i) {
        memcpy(dst_data + i * dst_linesize, src_data + i * src_linesize, dst_linesize);

    }
    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_dntutty_ffmpeg_GHSPlayer_prepareNative(JNIEnv *env, jobject instance,
                                                jstring dataSource_) {
    const char *dataSource = env->GetStringUTFChars(dataSource_, 0);

    javaCallHelper = new JavaCallHelper(javaVM, env, instance);
    ghsfFmpeg = new GHSFFmpeg(javaCallHelper, const_cast<char *>(dataSource));
    ghsfFmpeg->setRenderFunc(renderFrame);
    ghsfFmpeg->prepare();


    //下面将dataSource释放，造成悬空指针
    env->ReleaseStringUTFChars(dataSource_, dataSource);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_dntutty_ffmpeg_GHSPlayer_startNative(JNIEnv *env, jobject instance) {
    if (ghsfFmpeg) {
        LOGE("native start");
        ghsfFmpeg->start();
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_com_dntutty_ffmpeg_GHSPlayer_setSurfaceNative(JNIEnv *env, jobject instance, jobject surface) {
    pthread_mutex_lock(&mutex);
    //显示释放之前显示的窗口
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }

    //创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env, surface);
    pthread_mutex_unlock(&mutex);
}
//
// Created by Eric on 2019/8/13.
//

#ifndef FFMPEG_JAVACALLHELPER_H
#define FFMPEG_JAVACALLHELPER_H


#include <jni.h>

class JavaCallHelper {
public:
    JavaCallHelper(JavaVM *javaVM_,JNIEnv* env_,jobject instance_);
    ~JavaCallHelper();
    void onError(int thread_mode,int error_code);
    void onPrePared(int thread_mode);
    void onStart();
    void onStop();

private:
    JavaVM *javaVM;
    JNIEnv *env;
    jobject instance;
    jmethodID jmd_prepared;
    jmethodID jmd_error;
};


#endif //FFMPEG_JAVACALLHELPER_H

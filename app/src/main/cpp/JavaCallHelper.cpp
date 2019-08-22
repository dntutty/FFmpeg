//
// Created by Eric on 2019/8/13.
//

#include "JavaCallHelper.h"
#include "marco.h"

JavaCallHelper::JavaCallHelper(JavaVM *javaVM_,JNIEnv *env_,jobject instance_) {
    this->javaVM = javaVM_;
    this->env = env_;
//    this->instance = instance_;//不能直接赋值
    //一旦涉及到jobject跨方法，跨线程，需要创建全局引用
    this->instance = env->NewGlobalRef(instance_);
    jclass clazz = env->GetObjectClass(instance);

    //cd 进入class所在目录 执行：javap -s 全限定名，查看输出的descriptor
    // xx\app\build\intermediates\classes\debug>javap -s com.netease.jnitest.Helper
    this->jmd_prepared = env->GetMethodID(clazz,"onPrepared","()V");
    this->jmd_error = env->GetMethodID(clazz,"onError","(I)V");
}

JavaCallHelper::~JavaCallHelper() {
    this->javaVM = 0;
    env->DeleteGlobalRef(instance);

}

void JavaCallHelper::onPrePared(int thread_mode) {
    if(thread_mode==THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(instance,jmd_prepared);
    } else{
        //子线程
        //当前子线程的JINEnv
        LOGE("准备完成，开始调用java");
        JNIEnv *env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_prepared);
        javaVM->DetachCurrentThread();
    }
}

void JavaCallHelper::onError(int thread_mode,int error_code) {
    if(thread_mode==THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(instance,jmd_error,error_code);
    } else{
        //子线程
        //当前子线程的JINEnv
        JNIEnv *env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_error,error_code);
        javaVM->DetachCurrentThread();
    }
}

void JavaCallHelper::onStart() {

}

void JavaCallHelper::onStop() {

}

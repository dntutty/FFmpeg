//
// Created by Eric on 2019/8/18.
//

#ifndef FFMPEG_SAFE_QUEUE_H
#define FFMPEG_SAFE_QUEUE_H

#include <pthread.h>
#include <queue>
#include "marco.h"

using namespace std;


template<typename T>
class SafeQueue {
    typedef void (*release_func)(T *);

public:
    SafeQueue() {
        pthread_mutex_init(&mutex, NULL);//动态初始化的方式
        pthread_cond_init(&cond, NULL);
    }

    ~SafeQueue() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    /**
     * 入队操作
     * @param t
     */
    void push(T t) {
        //插入操作，队列加锁
        pthread_mutex_lock(&mutex);
        if (work) {
            //工作状态需要push
            q.push(t);
            pthread_cond_signal(&cond);
        } else {
            //非工作状态
            if (func) {
                func(&t);
            }
        }
        //队列解锁
        pthread_mutex_unlock(&mutex);
    }

    /**
     * 出队
     * @param t
     */
    int pop(T &t) {
        int ret = 0;
        pthread_mutex_lock(&mutex);
        while (work && q.empty()) {
            //工作状态，说明确实需要数据（pop),但是队列为空，需要等待
            pthread_cond_wait(&cond, &mutex);
        }
        if(!q.empty()) {
            t = q.front();
            q.pop();
            ret = 1;
        }
        pthread_mutex_unlock(&mutex);
        return ret;
    }

    /**
     * 设置队列的工作状态
     * @param work
     */
    void setWork(int work) {
        pthread_mutex_lock(&mutex);
        this->work = work;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    /**
     * 判断队列是否为空
     * @return
     */
    int is_empty() {
        return q.empty();
    }

    /**
     * 获取队列大小
     * @return
     */
    int size() {
        return q.size();
    }

    /**
     * 清空队列
     * 队列中的元素如何释放?
     * AVPacket
     */
    void clear() {
        pthread_mutex_lock(&mutex);
        //todo
//        if (release) {
//            while (!q.empty()) {
//                T t = q.front();
//                release(&t);
//                q.pop();
//            }
//        }

        //for循环清空队列
        if (func) {
            unsigned int size = q.size();
            for (int i = 0; i < size; i++) {
                T t = q.front();

                func(&t);

                q.pop();
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    void setReleaseFunc(release_func func) {
        this->func = func;
    }

private:
    queue<T> q;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int work = 0;//标记队列是否工作
    release_func func;
};


#endif //FFMPEG_SAFE_QUEUE_H

package com.dntutty.ffmpeg;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class GHSPlayer implements SurfaceHolder.Callback{
    private static final String TAG = "GHSPlayer";
    static {
        System.loadLibrary("native-lib");
    }


    //直播地址或媒体文件路径
    private String dataSource;
    private JavaCallHelper helper;
    private SurfaceHolder holder;

    public void setDataSource(String dataSource) {
        this.dataSource = dataSource;
    }


    /**
     * 播放准备工作
     */
    public void prepare() {
        Log.e(TAG, "prepare: "+"开始调用native");
        prepareNative(dataSource);
    }


    /**
     * 开始播放
     */
    public void start() {
        startNative();
    }



    private native void prepareNative(String dataSource);

    private native void startNative();
    /**
     * 供native反射调用
     * 表示播放器准备好了可以开始播放
     */
    public void onPrepared() {
        if (helper != null) {
            helper.onPrepared();
        }
    }

    public void onError(int errorCode) {
        if (helper != null) {
            helper.onError(errorCode);
        }
    }


    public void setJavaCallHelper(JavaCallHelper helper) {
        this.helper = helper;
    }

    public void setSurfaceView(SurfaceView sv) {
        if (holder != null) {
            holder.removeCallback(this);
        }
        holder = sv.getHolder();
        holder.addCallback(this);
    }

    /**
     * 画布创建回调
     * @param surfaceHolder
     */
    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {

    }

    /**
     * 画布刷新
     * @param surfaceHolder
     * @param i
     * @param i1
     * @param i2
     */
    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {
        setSurfaceNative(surfaceHolder.getSurface());
    }

    private native void setSurfaceNative(Surface surface);


    /**
     * 画布销毁
     * @param surfaceHolder
     */
    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    interface JavaCallHelper {
        void onPrepared();

        void onError(int errorCode);

        void onStart();

        void onStop();
    }

}

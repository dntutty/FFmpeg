package com.dntutty.ffmpeg;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private SurfaceView sv;
    private GHSPlayer player;
    private TextView play;
    public static final int REQUEST_CODE_ASK_CALL_PHONE = 101;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        sv = findViewById(R.id.surfaceview);
        play = findViewById(R.id.tv_play);
        player = new GHSPlayer();
        play.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                player.prepare();
            }
        });
        requestPermission();
        player.setSurfaceView(sv);
        player.setDataSource(new File(Environment.getExternalStorageDirectory() + File.separator + "demo.mp4").getAbsolutePath());
        player.setJavaCallHelper(new GHSPlayer.JavaCallHelper() {
            @Override
            public void onPrepared() {
                Log.e("ffmpeg", "onPrepared: 开始播放");
                //通知播放
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "开始播放", Toast.LENGTH_LONG).show();
                    }
                });
                //播放 调用到native去播放
                player.start();
            }

            @Override
            public void onError(final int errorCode) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "出错了，错误码：" + errorCode, Toast.LENGTH_LONG).show();
                    }
                });
            }

            @Override
            public void onStart() {

            }

            @Override
            public void onStop() {

            }
        });
    }

    private void requestPermission() {
        int checkCallPhonePermission = ContextCompat.checkSelfPermission(this, Manifest.permission.CALL_PHONE);
        if(checkCallPhonePermission != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE}, REQUEST_CODE_ASK_CALL_PHONE);
            return;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
    }


    @Override
    protected void onStop() {
        super.onStop();
        if (player != null) {
            player.stop();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        player.release();
    }
}

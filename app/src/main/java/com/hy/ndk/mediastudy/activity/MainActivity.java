package com.hy.ndk.mediastudy.activity;

import android.Manifest;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.hy.jni.base.Constant;
import com.hy.jni.base.TimeUtils;
import com.hy.ndk.mediastudy.MediaTest;
import com.hy.ndk.mediastudy.R;
import com.tbruyelle.rxpermissions2.Permission;
import com.tbruyelle.rxpermissions2.RxPermissions;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.OnClick;
import io.reactivex.disposables.Disposable;
import io.reactivex.functions.Consumer;

public class MainActivity extends AppCompatActivity {

    @BindView(R.id.main_recorder_tv)
    TextView mRecordTimeTv;
    @BindView(R.id.main_recorder_btn)
    Button mRecorderBtn;

    private MediaTest mMediaTest;
    private Disposable mDisposable;

    private boolean mIsPlayingAssets;
    private boolean mAssetsPlayerCreated;

    private boolean mIsPlayingPcm;
    private boolean mPcmPlayerCreate;

    private boolean mRecorderCreate;
    private boolean mIsPlayingRecord;
    private boolean mIsRecording;
    private boolean mIsAudioPlayerCreate;

    private String mRecordPath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/MediaStudy/"
            + TimeUtils.date2String(new Date()) + ".pcm";
    private long mRecordStartTime;
    private Handler mTimeHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        ButterKnife.bind(this);

        mMediaTest = new MediaTest();
        requestPermission();

        init();
        initAudioEngine();
    }

    @Override
    protected void onPause() {
        super.onPause();

        mIsPlayingAssets = false;
        mMediaTest.playAssets(false);
        mIsPlayingPcm = false;
        mMediaTest.playPCM(false);
    }

    private void init() {
        mTimeHandler = new Handler();
    }

    private void initAudioEngine() {
        mMediaTest.createEngine();
    }

    /**
     * 申请所需权限（不管用户是否同意，app正常运行）
     */
    private void requestPermission() {
        RxPermissions rxPermissions = new RxPermissions(MainActivity.this);
        mDisposable = rxPermissions.requestEach(
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.CAMERA,
                Manifest.permission.RECORD_AUDIO)
                .subscribe(new Consumer<Permission>() {
                    @Override
                    public void accept(Permission permission) {
                        if (permission.shouldShowRequestPermissionRationale) {
                            if (permission.name.equals(Manifest.permission.WRITE_EXTERNAL_STORAGE) ||
                                    permission.name.equals(Manifest.permission.CAMERA)
                                    || permission.name.equals(Manifest.permission.RECORD_AUDIO)) {
                                finish();
                            }
                        }
                    }
                });
    }

    @OnClick({R.id.main_assets_btn, R.id.main_pcm_btn, R.id.main_recorder_btn,
            R.id.main_play_back_btn, R.id.main_record_video_btn})
    public void onViewClicked(View view) {
        switch (view.getId()) {
            case R.id.main_assets_btn:
                if (!mAssetsPlayerCreated) {
                    mAssetsPlayerCreated = mMediaTest.createAssetsAudioPlayer(getAssets(), "background.mp3");
                }

                if (mAssetsPlayerCreated) {
                    mIsPlayingAssets = !mIsPlayingAssets;
                    mMediaTest.playAssets(mIsPlayingAssets);
                }
                break;
            case R.id.main_pcm_btn:
                String path = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Music/test.pcm";
                if (!mPcmPlayerCreate) {
                    mPcmPlayerCreate = mMediaTest.createPcmAudioPlayer(path);
                }
                if (mPcmPlayerCreate) {
                    mIsPlayingPcm = !mIsPlayingPcm;
                    mMediaTest.playPCM(mIsPlayingPcm);
                }
                break;
            case R.id.main_recorder_btn:
                if (!mRecorderCreate) {
                    mRecorderCreate = mMediaTest.createAudioRecorder();
                } else {
                    //录制音频
                    if (!mIsRecording) {
                        mRecordStartTime = TimeUtils.date2Millis(new Date());
                        mTimeHandler.postDelayed(mTimeRunnable, 0);

                        mMediaTest.startRecord(mRecordPath);

                        mIsRecording = true;
                        mRecorderBtn.setText("停止录制");
                    } else {
                        mTimeHandler.removeCallbacks(mTimeRunnable);

                        mMediaTest.stopRecord();

                        mIsRecording = false;
                        mRecorderBtn.setText("开始录制");
                    }
                }
                break;
            case R.id.main_play_back_btn:
                if (mIsAudioPlayerCreate) {
                    mIsAudioPlayerCreate = mMediaTest.createAudioPlayer(mRecordPath);
                } else {
                    mIsPlayingRecord = !mIsPlayingRecord;
                    mMediaTest.playRecord(mIsPlayingRecord);
                }
                break;
            case R.id.main_record_video_btn:
                Bundle recordVideoBundle = new Bundle();
                recordVideoBundle.putString(Constant.TYPE, Constant.TYPE_RECORD_VIDEO);
                Intent recordVideoIntent = new Intent(this, CommonHostActivity.class);
                recordVideoIntent.putExtras(recordVideoBundle);
                startActivity(recordVideoIntent);
                break;
        }
    }

    private Runnable mTimeRunnable = new Runnable() {
        @Override
        public void run() {
            mRecordTimeTv.setText(TimeUtils.millis2String(TimeUtils.date2Millis(new Date()) - mRecordStartTime,
                    new SimpleDateFormat("HH:mm:ss", Locale.getDefault())));
            mTimeHandler.postDelayed(this, 1000);
        }
    };

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mMediaTest.shutdown();
        mMediaTest.destroy();
        if (mDisposable != null) {
            mDisposable.dispose();
        }
    }
}

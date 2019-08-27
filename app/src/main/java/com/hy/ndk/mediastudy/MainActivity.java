package com.hy.ndk.mediastudy;

import android.Manifest;
import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.view.View;

import com.tbruyelle.rxpermissions2.Permission;
import com.tbruyelle.rxpermissions2.RxPermissions;

import butterknife.ButterKnife;
import butterknife.OnClick;
import io.reactivex.disposables.Disposable;
import io.reactivex.functions.Consumer;

public class MainActivity extends AppCompatActivity {

    private MediaTest mMediaTest;
    private Disposable mDisposable;

    private boolean mIsPlayingAssets;
    private boolean mAssetsPlayerCreated;

    private boolean mIsPlayingPcm;
    private boolean mPcmPlayerCreate;

    private boolean mRecorderCreate;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        ButterKnife.bind(this);

        mMediaTest = new MediaTest();
        requestPermission();

        initAudioEngine();
    }

    @Override
    protected void onPause() {
        super.onPause();

        mMediaTest.playClip(0, 0);
        mIsPlayingAssets = false;
        mMediaTest.playAssets(false);
        mIsPlayingPcm = false;
        mMediaTest.playPCM(false);
    }

    private void initAudioEngine() {
        mMediaTest.createEngine();

        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        String nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        int sampleRate = Integer.parseInt(nativeParam);
        nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        int bufSize = Integer.parseInt(nativeParam);
        mMediaTest.createBufferQueueAudioPlayer(sampleRate, bufSize);
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

    @OnClick({R.id.main_hello_btn, R.id.main_android_btn, R.id.main_assets_btn,
            R.id.main_pcm_btn, R.id.main_recorder_btn, R.id.main_play_back_btn})
    public void onViewClicked(View view) {
        switch (view.getId()) {
            case R.id.main_hello_btn:
                mMediaTest.playClip(1, 5);
                break;
            case R.id.main_android_btn:
                mMediaTest.playClip(2, 5);
                break;
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
                    mMediaTest.startRecord();
                }
                break;
            case R.id.main_play_back_btn:
                mMediaTest.playClip(3, 1);
                break;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mMediaTest.destroy();
        if (mDisposable != null) {
            mDisposable.dispose();
        }
    }
}

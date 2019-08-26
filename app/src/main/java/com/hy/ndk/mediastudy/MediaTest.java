package com.hy.ndk.mediastudy;

import android.content.res.AssetManager;

public class MediaTest {
    MediaTest() {
        System.loadLibrary("native-bridge");
        init();
    }


    private native void init();

    public native void destroy();

    private native void createBufferQueueAudioPlayer(int sampleRate, int bufSize);

    private native void playClip(int which, int count);

    private native void createAssetsAudioPlayer(AssetManager assetManager, String fileName);

    private native void playAssets();

    private native void playPCM();

    private native void createAudioRecorder();

    private native void startRecord();

    private native void stopRecord();

    private native void playRecord();

    private native void shutdown();
}

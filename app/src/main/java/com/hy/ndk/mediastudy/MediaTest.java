package com.hy.ndk.mediastudy;

import android.content.res.AssetManager;

public class MediaTest {
    MediaTest() {
        System.loadLibrary("native-bridge");
        init();
    }


    private native void init();

    public native void destroy();

    public native void createEngine();

    public native void createBufferQueueAudioPlayer(int sampleRate, int bufSize);

    public native void playClip(int which, int count);

    public native boolean createAssetsAudioPlayer(AssetManager assetManager, String fileName);

    public native void playAssets(boolean isPlaying);

    public native boolean createPcmAudioPlayer(String fileName);

    public native void playPCM(boolean isPlaying);

    public native boolean createAudioRecorder();

    public native void startRecord();

    public native void stopRecord();

    public native void playRecord();

    public native void shutdown();
}

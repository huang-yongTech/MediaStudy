package com.hy.ndk.mediastudy;

import android.content.res.AssetManager;

public class MediaTest {
    public MediaTest() {
        System.loadLibrary("native-bridge");
        init();
    }

    private native void init();

    public native void destroy();

    public native void createEngine();

    public native boolean createAssetsAudioPlayer(AssetManager assetManager, String fileName);

    public native void playAssets(boolean isPlaying);

    public native boolean createPcmAudioPlayer(String fileName);

    public native void playPCM(boolean isPlaying);

    public native boolean createAudioRecorder();

    public native void startRecord(String filePath);

    public native void stopRecord();

    public native boolean createAudioPlayer(String filePath);

    public native void playRecord(boolean isPlaying);

    public native void shutdown();
}

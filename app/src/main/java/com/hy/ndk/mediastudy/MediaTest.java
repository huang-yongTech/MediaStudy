package com.hy.ndk.mediastudy;

public class MediaTest {
    public MediaTest() {
        System.loadLibrary("native-bridge");
        init();
    }

    private native void init();
    public native void destroy();
}

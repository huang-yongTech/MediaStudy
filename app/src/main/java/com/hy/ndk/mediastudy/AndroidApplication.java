package com.hy.ndk.mediastudy;

import android.app.Application;

import com.hy.jni.base.CrashHandler;

public class AndroidApplication extends Application {

    private static AndroidApplication sInstance;

    @Override
    public void onCreate() {
        super.onCreate();
        sInstance = this;

        //在这里为应用设置异常处理程序，然后我们的程序才能捕获未处理的异常
        setupCrashHandler();

    }

    /**
     * APP异常崩溃处理
     */
    private void setupCrashHandler() {
        CrashHandler crashHandler = CrashHandler.getInstance();
        crashHandler.init(this);
    }


    public static AndroidApplication getInstance() {
        return sInstance;
    }

}

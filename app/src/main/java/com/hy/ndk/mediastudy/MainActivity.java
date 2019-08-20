package com.hy.ndk.mediastudy;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

public class MainActivity extends AppCompatActivity {

    private MediaTest mMediaTest;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mMediaTest = new MediaTest();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mMediaTest.destroy();
    }
}

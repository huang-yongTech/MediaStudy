package com.hy.ndk.mediastudy;

import android.Manifest;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

import com.tbruyelle.rxpermissions2.Permission;
import com.tbruyelle.rxpermissions2.RxPermissions;

import io.reactivex.disposables.Disposable;
import io.reactivex.functions.Consumer;

public class MainActivity extends AppCompatActivity {

    private MediaTest mMediaTest;

    private Disposable mDisposable;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mMediaTest = new MediaTest();
        requestPermission();
    }

    /**
     * 申请所需权限（不管用户是否同意，app正常运行）
     */
    private void requestPermission() {
        RxPermissions rxPermissions = new RxPermissions(MainActivity.this);
        mDisposable = rxPermissions.requestEach(
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.CAMERA)
                .subscribe(new Consumer<Permission>() {
                    @Override
                    public void accept(Permission permission) {
                        if (permission.shouldShowRequestPermissionRationale) {
                            if (permission.name.equals(Manifest.permission.WRITE_EXTERNAL_STORAGE) ||
                                    permission.name.equals(Manifest.permission.CAMERA)) {
                                finish();
                            }
                        }
                    }
                });
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

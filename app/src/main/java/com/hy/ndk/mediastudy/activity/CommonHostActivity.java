package com.hy.ndk.mediastudy.activity;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AppCompatActivity;

import com.hy.jni.base.Constant;
import com.hy.ndk.mediastudy.R;
import com.hy.ndk.mediastudy.fragment.RecordVideoFragment;

/**
 * 通用详情界面fragment托管activity
 */
public class CommonHostActivity extends AppCompatActivity {

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_common_host);

        init();
    }

    /**
     * 初始化UI，子类可重载该方法用于子类的相关初始化
     */
    public void init() {
        Intent intent = getIntent();
        Bundle bundle = intent.getExtras();
        if (bundle != null) {
            String type = bundle.getString(Constant.TYPE);
            if (type != null) {
                switch (type) {
                    case Constant.TYPE_RECORD_VIDEO:
                        changeFragment(R.id.host_fragment_container, new RecordVideoFragment());
                        break;
                    default:

                        break;
                }
            }
        }
    }

    /**
     * 在Activity中对子Fragment进行替换
     *
     * @param containerViewId fragment容器
     * @param fragment        将要加载的fragment
     */
    private void changeFragment(int containerViewId, Fragment fragment) {
        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.replace(containerViewId, fragment);
        transaction.commit();
    }
}

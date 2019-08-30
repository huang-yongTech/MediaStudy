package com.hy.ndk.mediastudy.fragment;

import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.hy.jni.base.Constant;
import com.hy.ndk.mediastudy.R;

/**
 *
 */
public class RecordVideoFragment extends Fragment {
    private String mParam1;
    private String mParam2;

    private GLSurfaceView mGlView;

    public RecordVideoFragment() {
    }

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     */
    public static RecordVideoFragment newInstance(String param1, String param2) {
        RecordVideoFragment fragment = new RecordVideoFragment();
        Bundle args = new Bundle();
        args.putString(Constant.ARG_PARAM1, param1);
        args.putString(Constant.ARG_PARAM2, param2);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            mParam1 = getArguments().getString(Constant.ARG_PARAM1);
            mParam2 = getArguments().getString(Constant.ARG_PARAM2);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_record_video, container, false);

        init();
        return view;
    }

    private void init() {

    }
}

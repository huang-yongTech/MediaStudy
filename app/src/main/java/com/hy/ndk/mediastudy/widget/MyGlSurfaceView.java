package com.hy.ndk.mediastudy.widget;

import android.content.Context;
import android.opengl.GLSurfaceView;

/**
 * 自定义GlSurfaceView
 */
public class MyGlSurfaceView extends GLSurfaceView {

    private final MyGlRenderer renderer;

    public MyGlSurfaceView(Context context) {
        super(context);

        //创建opengl3.0上下文环境
        setEGLContextClientVersion(3);

        renderer = new MyGlRenderer();

        //配置在GlSurfaceView上绘制的渲染器
        setRenderer(renderer);

        //设置当绘制数据改变时重新绘制界面
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }
}

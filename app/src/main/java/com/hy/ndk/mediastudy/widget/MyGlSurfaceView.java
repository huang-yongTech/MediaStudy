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

        // Create an OpenGL ES 2.0 context
        setEGLContextClientVersion(3);

        renderer = new MyGlRenderer();

        // Set the Renderer for drawing on the GLSurfaceView
        setRenderer(renderer);

        // Render the view only when there is a change in the drawing data
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }
}

package com.limelight.vr;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.SurfaceTexture;
import javax.microedition.khronos.egl.EGLConfig;
import android.opengl.GLSurfaceView;
import android.opengl.GLSurfaceView.Renderer;
import android.view.Surface;

import javax.microedition.khronos.opengles.GL10;

public class VrRenderer implements Renderer, SurfaceTexture.OnFrameAvailableListener {
    static {
        System.loadLibrary("vr_renderer");
    }

    public interface SurfaceListener {
        void onSurfaceReady(Surface surface);
    }

    private final GLSurfaceView glSurfaceView;
    private final SurfaceListener surfaceListener;
    private final long nativeHandle;
    private final float[] textureTransform = new float[16];
    private SurfaceTexture surfaceTexture;
    private Surface videoSurface;
    private volatile boolean frameAvailable = false;

    public VrRenderer(Context context, GLSurfaceView glSurfaceView, SurfaceListener surfaceListener) {
        this.glSurfaceView = glSurfaceView;
        this.surfaceListener = surfaceListener;
        this.nativeHandle = nativeCreate(context, context.getAssets());
        for (int i = 0; i < 16; i++) {
            textureTransform[i] = (i % 5 == 0) ? 1f : 0f;
        }
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
        int textureId = nativeOnSurfaceCreated(nativeHandle);
        surfaceTexture = new SurfaceTexture(textureId);
        surfaceTexture.setOnFrameAvailableListener(this);
        videoSurface = new Surface(surfaceTexture);
        if (surfaceListener != null) {
            surfaceListener.onSurfaceReady(videoSurface);
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl10, int width, int height) {
        nativeOnSurfaceChanged(nativeHandle, width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl10) {
        if (surfaceTexture != null && frameAvailable) {
            surfaceTexture.updateTexImage();
            surfaceTexture.getTransformMatrix(textureTransform);
            nativeSetTextureTransform(nativeHandle, textureTransform);
            frameAvailable = false;
        }
        nativeOnDrawFrame(nativeHandle);
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        frameAvailable = true;
        glSurfaceView.requestRender();
    }

    public void onResume() {
        glSurfaceView.onResume();
        nativeOnResume(nativeHandle);
    }

    public void onPause() {
        nativeOnPause(nativeHandle);
        glSurfaceView.onPause();
    }

    public void release() {
        if (videoSurface != null) {
            videoSurface.release();
            videoSurface = null;
        }
        if (surfaceTexture != null) {
            surfaceTexture.setOnFrameAvailableListener(null);
            surfaceTexture.release();
            surfaceTexture = null;
        }
        nativeDestroy(nativeHandle);
    }

    public Surface getVideoSurface() {
        return videoSurface;
    }

    private native long nativeCreate(Context context, AssetManager assetManager);
    private native void nativeDestroy(long handle);
    private native int nativeOnSurfaceCreated(long handle);
    private native void nativeOnSurfaceChanged(long handle, int width, int height);
    private native void nativeSetTextureTransform(long handle, float[] transform);
    private native void nativeOnDrawFrame(long handle);
    private native void nativeOnPause(long handle);
    private native void nativeOnResume(long handle);
}

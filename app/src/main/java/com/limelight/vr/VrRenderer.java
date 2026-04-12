package com.limelight.vr;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.SurfaceTexture;
import javax.microedition.khronos.egl.EGLConfig;
import android.opengl.GLSurfaceView;
import android.opengl.GLSurfaceView.Renderer;
import android.os.SystemClock;
import android.view.Surface;
import android.view.SurfaceHolder;

import com.limelight.LimeLog;

import javax.microedition.khronos.opengles.GL10;

public class VrRenderer implements Renderer, SurfaceTexture.OnFrameAvailableListener {
    static {
        System.loadLibrary("vr_renderer");
    }

    public static final int CURVATURE_MODE_FLAT = 0;
    public static final int CURVATURE_MODE_TV_CINEMA = 1;
    public static final int CURVATURE_MODE_GAMING_SCREEN = 2;

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
        LimeLog.info("VrRenderer.onSurfaceCreated: oldVideoSurface=" + videoSurface + " oldSurfaceTexture=" + surfaceTexture);
        int textureId = nativeOnSurfaceCreated(nativeHandle);
        surfaceTexture = new SurfaceTexture(textureId);
        surfaceTexture.setOnFrameAvailableListener(this);
        videoSurface = new Surface(surfaceTexture);
        LimeLog.info("VrRenderer.onSurfaceCreated: newVideoSurface=" + videoSurface + " isValid=" + videoSurface.isValid());
        if (surfaceListener != null) {
            surfaceListener.onSurfaceReady(videoSurface);
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl10, int width, int height) {
        LimeLog.info("VrRenderer.onSurfaceChanged: width=" + width + " height=" + height);
        nativeOnSurfaceChanged(nativeHandle, width, height);
    }

    private int frameDeliverCount = 0;
    private int frameConsumeCount = 0;
    private long lastFrameAvailableLog = 0;
    private long lastConsumeLog = 0;

    @Override
    public void onDrawFrame(GL10 gl10) {
        if (surfaceTexture != null) {
            try {
                surfaceTexture.updateTexImage();
                frameConsumeCount++;
                surfaceTexture.getTransformMatrix(textureTransform);
                nativeSetTextureTransform(nativeHandle, textureTransform);
                frameAvailable = false;
                long now = SystemClock.elapsedRealtime();
                if (frameConsumeCount % 60 == 0 || now - lastConsumeLog > 5000) {
                    LimeLog.warning("VR render: consumed frame #" + frameConsumeCount + " (delivered=" + frameDeliverCount + ")");
                    lastConsumeLog = now;
                }
            } catch (Exception e) {
            }
        }
        nativeOnDrawFrame(nativeHandle);
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        frameDeliverCount++;
        long now = SystemClock.elapsedRealtime();
        if (frameDeliverCount % 60 == 0 || now - lastFrameAvailableLog > 5000) {
            LimeLog.warning("VR render: frame available #" + frameDeliverCount + " (consumed=" + frameConsumeCount + ")");
            lastFrameAvailableLog = now;
        }
        frameAvailable = true;
        glSurfaceView.requestRender();
    }

    public void onResume() {
        LimeLog.info("VrRenderer.onResume");
        glSurfaceView.onResume();
        nativeOnResume(nativeHandle);
    }

    public void onPause() {
        LimeLog.info("VrRenderer.onPause");
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

    public void setScreenDistance(float distanceMeters) {
        nativeSetScreenDistance(nativeHandle, distanceMeters);
    }

    public void setScreenSize(float sizeMultiplier) {
        nativeSetScreenSize(nativeHandle, sizeMultiplier);
    }

    public void adjustScreenDistance(float deltaMeters) {
        nativeAdjustScreenDistance(nativeHandle, deltaMeters);
    }

    public void adjustScreenSize(float deltaMultiplier) {
        nativeAdjustScreenSize(nativeHandle, deltaMultiplier);
    }

    public void recenterView() {
        nativeRecenterView(nativeHandle);
    }

    public void setCurvatureMode(int mode) {
        nativeSetCurvatureMode(nativeHandle, mode);
    }

    public void setCurvatureAmount(float percent) {
        nativeSetCurvatureAmount(nativeHandle, percent);
    }

    public void setHorizontalCurvature(float percent) {
        nativeSetHorizontalCurvature(nativeHandle, percent);
    }

    public void setVerticalCurvature(float percent) {
        nativeSetVerticalCurvature(nativeHandle, percent);
    }

    public float getCurvatureAmount() {
        return nativeGetCurvatureAmount(nativeHandle);
    }

    public float getHorizontalCurvature() {
        return nativeGetHorizontalCurvature(nativeHandle);
    }

    public float getVerticalCurvature() {
        return nativeGetVerticalCurvature(nativeHandle);
    }

    private native long nativeCreate(Context context, AssetManager assetManager);
    private native void nativeDestroy(long handle);
    private native int nativeOnSurfaceCreated(long handle);
    private native void nativeOnSurfaceChanged(long handle, int width, int height);
    private native void nativeSetTextureTransform(long handle, float[] transform);
    private native void nativeOnDrawFrame(long handle);
    private native void nativeOnPause(long handle);
    private native void nativeOnResume(long handle);
    private native void nativeSetScreenDistance(long handle, float distanceMeters);
    private native void nativeSetScreenSize(long handle, float sizeMultiplier);
    private native void nativeAdjustScreenDistance(long handle, float deltaMeters);
    private native void nativeAdjustScreenSize(long handle, float deltaMultiplier);
    private native void nativeRecenterView(long handle);
    private native void nativeSetCurvatureMode(long handle, int mode);
    private native void nativeSetCurvatureAmount(long handle, float percent);
    private native void nativeSetHorizontalCurvature(long handle, float percent);
    private native void nativeSetVerticalCurvature(long handle, float percent);
    private native float nativeGetCurvatureAmount(long handle);
    private native float nativeGetHorizontalCurvature(long handle);
    private native float nativeGetVerticalCurvature(long handle);
}

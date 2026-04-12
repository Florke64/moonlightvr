package com.limelight.vr;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.SurfaceTexture;
import javax.microedition.khronos.egl.EGLConfig;
import android.opengl.GLSurfaceView;
import android.opengl.GLSurfaceView.Renderer;
import android.os.SystemClock;
import android.view.Surface;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLUtils;
import android.opengl.GLES20;

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
    private int[] skyboxResourceIds;
    private Resources skyboxResources;
    private int skyboxTextureId = 0;

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

        if (skyboxResources != null && skyboxResourceIds != null && skyboxResourceIds.length == 6) {
            loadSkyboxCubemap(skyboxResources, skyboxResourceIds);
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
        if (skyboxTextureId != 0) {
            int[] texture = new int[] { skyboxTextureId };
            GLES20.glDeleteTextures(1, texture, 0);
            skyboxTextureId = 0;
        }
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

    public void adjustScreenPosition(float deltaX, float deltaY) {
        nativeAdjustScreenPosition(nativeHandle, deltaX, deltaY);
    }

    public void adjustScreenRotation(float deltaRadians) {
        nativeAdjustScreenRotation(nativeHandle, deltaRadians);
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
 
    public void setSkyboxEnabled(boolean enabled) {
        nativeSetSkyboxEnabled(nativeHandle, enabled);
    }

    public void loadSkyboxCubemap(Resources resources, int[] resourceIds) {
        if (resourceIds == null || resourceIds.length != 6) {
            LimeLog.warning("loadSkyboxCubemap: expected 6 resource IDs, got " + (resourceIds != null ? resourceIds.length : 0));
            return;
        }

        skyboxResources = resources;
        skyboxResourceIds = resourceIds.clone();

        if (skyboxTextureId != 0) {
            int[] oldTexture = new int[] { skyboxTextureId };
            GLES20.glDeleteTextures(1, oldTexture, 0);
            skyboxTextureId = 0;
        }

        int[] textureIds = new int[1];
        GLES20.glGenTextures(1, textureIds, 0);
        if (textureIds[0] == 0) {
            LimeLog.warning("loadSkyboxCubemap: glGenTextures returned 0");
            nativeSetSkyboxTexture(nativeHandle, 0);
            return;
        }

        GLES20.glBindTexture(GLES20.GL_TEXTURE_CUBE_MAP, textureIds[0]);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_CUBE_MAP, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_CUBE_MAP, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_CUBE_MAP, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_CUBE_MAP, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

        int[] cubeMapTargets = {
            GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
        };

        int[] panoramaToFaceMap = {
            1,
            3,
            4,
            5,
            0,
            2
        };

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        boolean allFacesLoaded = true;

        for (int i = 0; i < 6; i++) {
            int resourceIndex = panoramaToFaceMap[i];
            Bitmap bitmap = null;
            try {
                bitmap = BitmapFactory.decodeResource(resources, resourceIds[resourceIndex], options);
                if (bitmap != null) {
                    GLUtils.texImage2D(cubeMapTargets[i], 0, bitmap, 0);
                    int glError = GLES20.glGetError();
                    if (glError != GLES20.GL_NO_ERROR) {
                        allFacesLoaded = false;
                        LimeLog.warning("loadSkyboxCubemap: GL error 0x" + Integer.toHexString(glError) + " on face " + i + " size=" + bitmap.getWidth() + "x" + bitmap.getHeight());
                    }
                } else {
                    allFacesLoaded = false;
                    LimeLog.warning("loadSkyboxCubemap: decodeResource returned null for face " + i);
                }
            } catch (Exception e) {
                allFacesLoaded = false;
                LimeLog.warning("loadSkyboxCubemap: failed to load face " + i + ": " + e.getMessage());
            } finally {
                if (bitmap != null) {
                    bitmap.recycle();
                }
            }
        }

        GLES20.glBindTexture(GLES20.GL_TEXTURE_CUBE_MAP, 0);

        if (!allFacesLoaded) {
            GLES20.glDeleteTextures(1, textureIds, 0);
            nativeSetSkyboxTexture(nativeHandle, 0);
            LimeLog.warning("loadSkyboxCubemap: cubemap upload failed; disabled skybox texture");
            return;
        }

        nativeSetSkyboxTexture(nativeHandle, textureIds[0]);
        skyboxTextureId = textureIds[0];
        LimeLog.info("loadSkyboxCubemap: loaded cubemap with texture ID " + textureIds[0]);
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

    public float getScreenSize() {
        return nativeGetScreenSize(nativeHandle);
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
    private native void nativeAdjustScreenPosition(long handle, float deltaX, float deltaY);
    private native void nativeAdjustScreenRotation(long handle, float deltaRadians);
    private native void nativeRecenterView(long handle);
    private native void nativeSetCurvatureMode(long handle, int mode);
    private native void nativeSetCurvatureAmount(long handle, float percent);
    private native void nativeSetHorizontalCurvature(long handle, float percent);
    private native void nativeSetVerticalCurvature(long handle, float percent);
    private native void nativeSetSkyboxEnabled(long handle, boolean enabled);
    private native void nativeSetSkyboxTexture(long handle, int textureId);
    private native float nativeGetCurvatureAmount(long handle);
    private native float nativeGetHorizontalCurvature(long handle);
    private native float nativeGetVerticalCurvature(long handle);
    private native float nativeGetScreenSize(long handle);
}

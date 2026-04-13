package com.limelight.vr;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;

import java.util.Collections;
import java.util.List;

public class VrCameraManager {
    private static final String TAG = "VrCameraManager";

    private final Context context;
    private CameraManager cameraManager;
    private CameraDevice cameraDevice;
    private CameraCaptureSession captureSession;
    private Surface cameraSurface;
    private HandlerThread cameraThread;
    private Handler cameraHandler;
    private boolean isStarted = false;

    public VrCameraManager(Context context) {
        this.context = context;
    }

    public boolean isStarted() {
        return isStarted;
    }

    public void startCamera(Surface surface) {
        if (isStarted) {
            Log.w(TAG, "Camera already started");
            return;
        }

        if (context.checkSelfPermission(Manifest.permission.CAMERA)
                != PackageManager.PERMISSION_GRANTED) {
            Log.e(TAG, "Camera permission not granted");
            return;
        }

        cameraSurface = surface;
        cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);

        cameraThread = new HandlerThread("CameraThread");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());

        try {
            String cameraId = getRearCameraId();
            if (cameraId == null) {
                Log.e(TAG, "No rear camera found");
                return;
            }

            cameraManager.openCamera(cameraId, stateCallback, cameraHandler);
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to open camera", e);
        }
    }

    private String getRearCameraId() {
        try {
            for (String cameraId : cameraManager.getCameraIdList()) {
                CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(cameraId);
                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == CameraCharacteristics.LENS_FACING_BACK) {
                    return cameraId;
                }
            }
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to get camera list", e);
        }
        return null;
    }

    private final CameraDevice.StateCallback stateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice camera) {
            cameraDevice = camera;
            createCaptureSession();
        }

        @Override
        public void onDisconnected(CameraDevice camera) {
            camera.close();
            cameraDevice = null;
        }

        @Override
        public void onError(CameraDevice camera, int error) {
            camera.close();
            cameraDevice = null;
            Log.e(TAG, "Camera error: " + error);
        }
    };

    private void createCaptureSession() {
        if (cameraDevice == null || cameraSurface == null) {
            Log.e(TAG, "Cannot create session: device=" + cameraDevice + " surface=" + cameraSurface);
            return;
        }

        try {
            List<Surface> surfaces = Collections.singletonList(cameraSurface);
            Log.i(TAG, "Creating capture session with surface: " + cameraSurface + " isValid=" + cameraSurface.isValid());
            cameraDevice.createCaptureSession(
                    surfaces,
                    new CameraCaptureSession.StateCallback() {
                        @Override
                        public void onConfigured(CameraCaptureSession session) {
                            Log.i(TAG, "Capture session configured successfully");
                            captureSession = session;
                            startPreview();
                        }

                        @Override
                        public void onConfigureFailed(CameraCaptureSession session) {
                            Log.e(TAG, "Camera configuration FAILED");
                        }
                    },
                    cameraHandler
            );
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to create capture session", e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Illegal state creating capture session", e);
        }
    }

    private void startPreview() {
        if (captureSession == null || cameraDevice == null) {
            return;
        }

        try {
            CaptureRequest.Builder previewRequest = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            previewRequest.addTarget(cameraSurface);
            previewRequest.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);

            captureSession.setRepeatingRequest(previewRequest.build(), null, cameraHandler);
            isStarted = true;
            Log.i(TAG, "Camera preview started");
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to start preview", e);
        }
    }

    public void stopCamera() {
        isStarted = false;
        if (captureSession != null) {
            try {
                captureSession.stopRepeating();
            } catch (Exception ignored) {
            }
            captureSession.close();
            captureSession = null;
        }
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        cameraSurface = null;
        if (cameraThread != null) {
            cameraThread.quitSafely();
            cameraThread = null;
        }
        cameraHandler = null;
        Log.i(TAG, "Camera stopped");
    }
}

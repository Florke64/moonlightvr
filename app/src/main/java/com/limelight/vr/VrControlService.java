package com.limelight.vr;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class VrControlService extends Service {
    private static final int NOTIFICATION_ID = 1001;
    private static final String CHANNEL_ID = "moonlight_vr_control";
    private static final String VR_PREF_KEY = "checkbox_enable_vr";

    private final IBinder binder = new LocalBinder();
    private VrControlServer server;
    private ExecutorService executor;
    private boolean isRunning = false;
    private VrRenderer vrRenderer;

    public class LocalBinder extends Binder {
        public VrControlService getService() {
            return VrControlService.this;
        }
    }

    public void setVrRenderer(VrRenderer renderer) {
        this.vrRenderer = renderer;
        if (server != null) {
            server.setVrRenderer(renderer);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && "STOP".equals(intent.getAction())) {
            stopServer();
            stopSelf();
            return START_STICKY;
        }
        
        startForeground(NOTIFICATION_ID, buildNotification());
        startServer();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        stopServer();
        super.onDestroy();
    }

    private void startServer() {
        if (isRunning) return;
        
        executor = Executors.newSingleThreadExecutor();
        executor.execute(() -> {
            try {
                server = new VrControlServer(getApplicationContext());
                if (vrRenderer != null) {
                    server.setVrRenderer(vrRenderer);
                }
                server.start();
                isRunning = true;
            } catch (Exception e) {
                e.printStackTrace();
                stopSelf();
            }
        });
    }

    private void stopServer() {
        if (server != null) {
            server.stop();
            server = null;
        }
        if (executor != null) {
            executor.shutdown();
            executor = null;
        }
        isRunning = false;
    }

    private int getPort() {
        return server != null ? server.getPort() : -1;
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, "VR Control", NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("MoonlightVR Control Server");
            NotificationManager mgr = getSystemService(NotificationManager.class);
            if (mgr != null) mgr.createNotificationChannel(channel);
        }
    }

    private Notification buildNotification() {
        Intent notificationIntent = new Intent(Intent.ACTION_VIEW);
        notificationIntent.setData(android.net.Uri.parse("moonlight://main"));
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        PendingIntent pi = PendingIntent.getActivity(this, 0, notificationIntent, flags);
        
        Intent stopIntent = new Intent(this, VrControlService.class);
        stopIntent.setAction("STOP");
        PendingIntent spi = PendingIntent.getService(this, 0, stopIntent, flags);

        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder = new Notification.Builder(this, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(this);
        }
        
        return builder
            .setContentTitle("MoonlightVR")
            .setContentText("HTTPS Control Server Running")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentIntent(pi)
            .addAction(0, "Stop", spi)
            .build();
    }
}

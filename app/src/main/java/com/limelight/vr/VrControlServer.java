package com.limelight.vr;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.util.Base64;

import com.limelight.BuildConfig;

import fi.iki.elonen.NanoHTTPD;

import org.bouncycastle.asn1.x500.X500Name;
import org.bouncycastle.cert.X509v3CertificateBuilder;
import org.bouncycastle.cert.jcajce.JcaX509CertificateConverter;
import org.bouncycastle.cert.jcajce.JcaX509v3CertificateBuilder;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.operator.ContentSigner;
import org.bouncycastle.operator.jcajce.JcaContentSignerBuilder;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.math.BigInteger;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.Provider;
import java.security.SecureRandom;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;

public class VrControlServer extends NanoHTTPD {
    private static final int DEFAULT_PORT = 8555;
    private static final String VERSION = BuildConfig.VERSION_NAME;
    private static final String KEY_ALIAS = "MoonlightVrServerKey";
    private static final String PREF_NAME = "VrControlPrefs";
    private static final String PREF_KEY_PASS = "keystore_pass";
    private static final String KEYSTORE_FILE = "vr_keystore.p12";
    private static final int ZOOM_ANIMATION_DURATION_MS = 500;

    private final Context context;
    private VrRenderer vrRenderer;
    private Timer quickZoomTimer;
    private float preZoomSize = -1f;
    private Handler animationHandler;
    private Runnable zoomAnimator;
    private float animStartSize;
    private float animTargetSize;
    private long animStartTime;
    private float currentBrightness = 1.0f;

    public VrControlServer(Context context) throws Exception {
        super(DEFAULT_PORT);
        this.context = context.getApplicationContext();
        initSecureServer();
    }

    public void setVrRenderer(VrRenderer renderer) {
        this.vrRenderer = renderer;
    }

    private void initSecureServer() throws Exception {
        KeyStore keyStore = loadOrGenerateKeyStore();

        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        char[] password = prefs.getString(PREF_KEY_PASS, "").toCharArray();

        KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        kmf.init(keyStore, password);

        SSLContext sslContext = SSLContext.getInstance("TLS");
        sslContext.init(kmf.getKeyManagers(), null, null);
        makeSecure(sslContext.getServerSocketFactory(), null);
    }

    private KeyStore loadOrGenerateKeyStore() throws Exception {
        File file = new File(context.getFilesDir(), KEYSTORE_FILE);
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        String passwordStr = prefs.getString(PREF_KEY_PASS, null);

        if (passwordStr == null || !file.exists()) {
            byte[] randomBytes = new byte[16];
            new SecureRandom().nextBytes(randomBytes);
            passwordStr = Base64.encodeToString(randomBytes, Base64.NO_WRAP);
            prefs.edit().putString(PREF_KEY_PASS, passwordStr).apply();

            Provider bcProvider = new BouncyCastleProvider();

            KeyPairGenerator keyGen = KeyPairGenerator.getInstance("RSA");
            keyGen.initialize(2048, new SecureRandom());
            KeyPair keyPair = keyGen.generateKeyPair();

            long now = System.currentTimeMillis();
            Date startDate = new Date(now);
            Calendar calendar = Calendar.getInstance();
            calendar.setTime(startDate);
            calendar.add(Calendar.YEAR, 5);
            Date endDate = calendar.getTime();

            X500Name subject = new X500Name("CN=MoonlightVR Control, O=Limelight, C=US");
            BigInteger serial = BigInteger.valueOf(now);

            X509v3CertificateBuilder certBuilder = new JcaX509v3CertificateBuilder(
                    subject, serial, startDate, endDate, subject, keyPair.getPublic());

            ContentSigner signer = new JcaContentSignerBuilder("SHA256WithRSAEncryption")
                    .setProvider(bcProvider)
                    .build(keyPair.getPrivate());

            X509Certificate cert = new JcaX509CertificateConverter()
                    .setProvider(bcProvider)
                    .getCertificate(certBuilder.build(signer));

            KeyStore ks = KeyStore.getInstance("PKCS12");
            ks.load(null, null);
            ks.setKeyEntry(KEY_ALIAS, keyPair.getPrivate(), passwordStr.toCharArray(), new Certificate[]{cert});

            try (FileOutputStream fos = new FileOutputStream(file)) {
                ks.store(fos, passwordStr.toCharArray());
            }
        }

        KeyStore ks = KeyStore.getInstance("PKCS12");
        try (FileInputStream fis = new FileInputStream(file)) {
            ks.load(fis, passwordStr.toCharArray());
        }
        return ks;
    }

    private static final float ZOOM_STEP = 0.2f;
    private static final float SCREEN_SIZE_STEP = 0.1f;
    private static final float CURVATURE_STEP = 10f;
    private static final float HORIZONTAL_CURVE_STEP = 10f;
    private static final float VERTICAL_CURVE_STEP = 10f;

    @Override
    public Response serve(IHTTPSession session) {
        String uri = session.getUri();
        if (uri.equals("/") || uri.equals("/index.html")) {
            return serveHtml();
        } else if (uri.equals("/recenter")) {
            if (vrRenderer != null) {
                vrRenderer.recenterView();
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/brightness")) {
            if (vrRenderer != null) {
                List<String> q = session.getParameters().get("v");
                if (q != null && !q.isEmpty()) {
                    try {
                        float val = Float.parseFloat(q.get(0));
                        currentBrightness = Math.max(0.0f, Math.min(1.0f, val));
                        vrRenderer.setSkyboxBrightness(currentBrightness);
                    } catch (NumberFormatException e) {}
                }
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"brightness\":" + currentBrightness + "}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/ping")) {
            return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"pong\",\"ts\":" + System.currentTimeMillis() + "}");
        } else if (uri.equals("/gesture") && Method.POST.equals(session.getMethod())) {
            try {
                Map<String, String> files = new java.util.HashMap<>();
                session.parseBody(files);
                String jsonStr = files.get("postData");
                if (jsonStr != null && vrRenderer != null) {
                    org.json.JSONObject obj = new org.json.JSONObject(jsonStr);
                    String type = obj.optString("type", "");

                    boolean isQuickZoom = android.preference.PreferenceManager.getDefaultSharedPreferences(context)
                            .getBoolean("checkbox_quick_zoom", false);

                    if ("pan".equals(type)) {
                        float dx = (float) obj.optDouble("dx", 0.0);
                        float dy = (float) obj.optDouble("dy", 0.0);
                        vrRenderer.adjustScreenPosition(dx, dy);
                    } else if ("pinch".equals(type)) {
                        float dScale = (float) obj.optDouble("dScale", 0.0);
                        if (isQuickZoom && preZoomSize < 0) {
                            preZoomSize = vrRenderer.getScreenSize();
                        }
                        vrRenderer.adjustScreenSize(dScale * -4.0f);
                        if (isQuickZoom && preZoomSize > 0) {
                            resetQuickZoomIdle();
                        }
                    } else if ("rotate".equals(type)) {
                        float rotation = (float) obj.optDouble("rotation", 0.0);
                        vrRenderer.adjustScreenRotation(rotation);
                    } else if ("pinch_rotate".equals(type)) {
                        float dScale = (float) obj.optDouble("dScale", 0.0);
                        float rotation = (float) obj.optDouble("rotation", 0.0);
                        if (isQuickZoom && preZoomSize < 0) {
                            preZoomSize = vrRenderer.getScreenSize();
                        }
                        vrRenderer.adjustScreenSize(dScale * -4.0f);
                        vrRenderer.adjustScreenRotation(rotation);
                        if (isQuickZoom && preZoomSize > 0) {
                            resetQuickZoomIdle();
                        }
                    } else if ("doubletap".equals(type)) {
                        if (isQuickZoom && quickZoomTimer != null) {
                            quickZoomTimer.cancel();
                            quickZoomTimer.purge();
                            preZoomSize = -1f;
                        }
                        resetToPreferenceSettings();
                        vrRenderer.recenterView();
                    } else if ("brightness".equals(type)) {
                        float dy = (float) obj.optDouble("dy", 0.0);
                        currentBrightness = Math.max(0.0f, Math.min(1.0f, currentBrightness + dy));
                        vrRenderer.setSkyboxBrightness(currentBrightness);
                    }
                }
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\"}");
            } catch (Exception e) {
                return newFixedLengthResponse(Response.Status.INTERNAL_ERROR, "application/json", "{\"error\":\"" + e.getMessage() + "\"}");
            }
        } else if (uri.equals("/buttons.html") || uri.equals("/buttons")) {
            return serveButtonsHtml();
        } else if (uri.equals("/zoomin")) {
            if (vrRenderer != null) {
                vrRenderer.adjustScreenDistance(-ZOOM_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"zoomin\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/zoomout")) {
            if (vrRenderer != null) {
                vrRenderer.adjustScreenDistance(ZOOM_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"zoomout\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/screenup")) {
            if (vrRenderer != null) {
                vrRenderer.adjustScreenSize(SCREEN_SIZE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"screenup\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/screendown")) {
            if (vrRenderer != null) {
                vrRenderer.adjustScreenSize(-SCREEN_SIZE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"screendown\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/curvature-flat")) {
            if (vrRenderer != null) {
                vrRenderer.setCurvatureMode(VrRenderer.CURVATURE_MODE_FLAT);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"curvature-flat\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/curvature-tv")) {
            if (vrRenderer != null) {
                vrRenderer.setCurvatureMode(VrRenderer.CURVATURE_MODE_TV_CINEMA);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"curvature-tv\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/curvature-gaming")) {
            if (vrRenderer != null) {
                vrRenderer.setCurvatureMode(VrRenderer.CURVATURE_MODE_GAMING_SCREEN);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"curvature-gaming\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/curvature-increase")) {
            if (vrRenderer != null) {
                vrRenderer.setCurvatureAmount(vrRenderer.getCurvatureAmount() + CURVATURE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"curvature-increase\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/curvature-decrease")) {
            if (vrRenderer != null) {
                vrRenderer.setCurvatureAmount(vrRenderer.getCurvatureAmount() - CURVATURE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"curvature-decrease\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/horizontal-increase")) {
            if (vrRenderer != null) {
                vrRenderer.setHorizontalCurvature(vrRenderer.getHorizontalCurvature() + HORIZONTAL_CURVE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"horizontal-increase\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/horizontal-decrease")) {
            if (vrRenderer != null) {
                vrRenderer.setHorizontalCurvature(vrRenderer.getHorizontalCurvature() - HORIZONTAL_CURVE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"horizontal-decrease\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/vertical-increase")) {
            if (vrRenderer != null) {
                vrRenderer.setVerticalCurvature(vrRenderer.getVerticalCurvature() + VERTICAL_CURVE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"vertical-increase\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        } else if (uri.equals("/vertical-decrease")) {
            if (vrRenderer != null) {
                vrRenderer.setVerticalCurvature(vrRenderer.getVerticalCurvature() - VERTICAL_CURVE_STEP);
                return newFixedLengthResponse(Response.Status.OK, "application/json", "{\"status\":\"ok\",\"action\":\"vertical-decrease\"}");
            }
            return newFixedLengthResponse(Response.Status.SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"no renderer\"}");
        }
        return serveStaticAsset(uri);
    }

    private Response serveHtml() {
        String html = "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" +
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no\">" +
            "<title>VR Control</title>" +
            "<style>" +
            "body, html { margin: 0; padding: 0; width: 100%; height: 100%; background: #222; overflow: hidden; touch-action: none; color: #aa344a; font-family: sans-serif; user-select: none; -webkit-user-select: none; overscroll-behavior: none; }" +
            "#canvas { width: 100%; height: 100%; display: block; }" +
            "#hud { position: absolute; top: 20px; left: 20px; pointer-events: none; opacity: 0.8; font-size: 14px; }" +
            "#menu { position: absolute; top: 20px; right: 20px; pointer-events: auto; }" +
            "#menu a { color: #aa344a; text-decoration: none; font-size: 14px; }" +
            "#status { position: absolute; bottom: 20px; right: 20px; width: 16px; height: 16px; border-radius: 50%; background: #f44; box-shadow: 0 0 8px #f44; }" +
            "</style></head><body>" +
            "<div id=\"hud\"><strong>MoonlightVR Gestures</strong><br>" +
            "1 Finger: Brightness (up/down)<br>2 Fingers: Pinch to Zoom<br>2 Fingers: Rotate<br>Double Tap: Recenter</div>" +
            "<div id=\"menu\"><a href=\"/buttons.html\">Buttons</a></div>" +
            "<div id=\"status\"></div>" +
            "<canvas id=\"canvas\"></canvas>" +
            "<script>" +
            "const cvs = document.getElementById('canvas');" +
            "const ctx = cvs.getContext('2d');" +
            "let width, height;" +
            "function resize() { width = cvs.width = window.innerWidth; height = cvs.height = window.innerHeight; }" +
            "window.addEventListener('resize', resize); resize();" +
            "let touches = {}; let initDist = null; let initRot = null; let lastTap = 0; let lastSend = 0;" +

            "const statusEl = document.getElementById('status');" +
            "const setHealthy = () => { statusEl.style.background = '#4f4'; statusEl.style.boxShadow = '0 0 8px #4f4'; };" +
            "const setUnhealthy = () => { statusEl.style.background = '#f44'; statusEl.style.boxShadow = '0 0 8px #f44'; };" +
            "function sendGesture(payload) {" +
            "  let now = Date.now();" +
            "  if (now - lastSend > 33) {" +
            "    fetch('/gesture', { method: 'POST', body: JSON.stringify(payload) }).then(r => {" +
            "      if(r.ok) setHealthy();" +
            "    }).catch(() => setUnhealthy());" +
            "    lastSend = now;" +
            "  }" +
            "}" +
            "cvs.addEventListener('touchstart', e => {" +
            "  e.preventDefault();" +
            "  for(let t of e.changedTouches) touches[t.identifier] = { x: t.clientX, y: t.clientY };" +
            "  let vals = Object.values(touches);" +
            "  if(vals.length === 2) {" +
            "    let dx = vals[0].x - vals[1].x, dy = vals[0].y - vals[1].y;" +
            "    initDist = Math.hypot(dx, dy); initRot = Math.atan2(dy, dx);" +
            "  }" +
            "  if(vals.length === 1) {" +
            "    let now = Date.now();" +
            "    if(now - lastTap < 300) { sendGesture({type:'doubletap'}); lastTap = 0; } else lastTap = now;" +
            "  }" +
            "});" +
            "cvs.addEventListener('touchmove', e => {" +
            "  e.preventDefault();" +
            "  let active = e.touches;" +
            "  if(active.length === 1) {" +
            "    let t = active[0], old = touches[t.identifier];" +
            "    if(old) {" +
            "      let dy = t.clientY - old.y;" +
            "      if(Math.abs(dy) > 2) sendGesture({ type: 'brightness', dy: -dy * 0.015 });" +
            "    }" +
            "    touches[t.identifier] = { x: t.clientX, y: t.clientY };" +
            "  } else if(active.length === 2) {" +
            "    let t1 = active[0], t2 = active[1];" +
            "    let dx = t1.clientX - t2.clientX, dy = t1.clientY - t2.clientY;" +
            "    let dist = Math.hypot(dx, dy), rot = Math.atan2(dy, dx);" +
            "    if(initDist !== null && initRot !== null) {" +
            "      let dRot = rot - initRot;" +
            "      if(dRot > Math.PI) dRot -= Math.PI*2; if(dRot < -Math.PI) dRot += Math.PI*2;" +
            "      sendGesture({ type: 'pinch_rotate', dScale: (initDist - dist)*0.005, rotation: dRot });" +
            "    }" +
            "    initDist = dist; initRot = rot;" +
            "    touches[t1.identifier] = { x: t1.clientX, y: t1.clientY };" +
            "    touches[t2.identifier] = { x: t2.clientX, y: t2.clientY };" +
            "  }" +
            "  ctx.clearRect(0,0,width,height); ctx.fillStyle = 'rgba(170,52,74,0.4)';" +
            "  for(let i=0; i<active.length; i++) { ctx.beginPath(); ctx.arc(active[i].clientX, active[i].clientY, 40, 0, Math.PI*2); ctx.fill(); }" +
            "});" +
            "cvs.addEventListener('touchend', e => {" +
            "  e.preventDefault(); for(let t of e.changedTouches) delete touches[t.identifier];" +
            "  if(Object.keys(touches).length < 2) { initDist = null; initRot = null; }" +
            "  ctx.clearRect(0,0,width,height);" +
            "});" +
            "setInterval(() => {" +
            "  fetch('/ping').then(r => {" +
            "    setHealthy();" +
            "    if (wakeLock === null) requestWakeLock();" +
            "  }).catch(() => {" +
            "    setUnhealthy();" +
            "    if (wakeLock !== null) { wakeLock.release(); wakeLock = null; }" +
            "  });" +
            "}, 1000);" +

            "let wakeLock = null;" +
            "const releaseWakeLock = () => { if (wakeLock !== null) { wakeLock.release(); wakeLock = null; console.log('Wake Lock released - no activity'); } };" +
            "const requestWakeLock = async () => {" +
            "  if ('wakeLock' in navigator && wakeLock === null) {" +
            "    try {" +
            "      wakeLock = await navigator.wakeLock.request('screen');" +
            "      wakeLock.addEventListener('release', () => { console.log('Wake Lock released'); });" +
            "    } catch (err) { console.error('Wake Lock:', err.message); }" +
            "  }" +
            "};" +
            "requestWakeLock();" +
            "document.addEventListener('visibilitychange', async () => {" +
            "  if (document.visibilityState === 'visible') requestWakeLock();" +
            "});" +
            "setInterval(() => {" +
            "  if (Date.now() - lastGesture > 30000) releaseWakeLock();" +
            "}, 5000);" +
            "</script></body></html>";
        return newFixedLengthResponse(Response.Status.OK, "text/html", html);
    }

    private Response serveButtonsHtml() {
        String html = "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" +
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" +
            "<title>MoonlightVR Control</title>" +
            "<style>" +
            "body{font-family:sans-serif;margin:0;padding:20px;background:#222;color:#eee}" +
            "h1{color:#aa344a}.card{background:#333;padding:20px;border-radius:8px;margin:10px 0}" +
            ".intro{border-left:4px solid #aa344a}" +
            ".status{color:#4f4}.btn{display:block;width:100%;padding:15px;margin:5px 0;" +
            "background:#aa344a;border:none;border-radius:4px;color:#111;font-size:16px;font-weight:bold;cursor:pointer}" +
            ".btn:active{background:#622a35}.row{display:flex;gap:10px}.row .btn{flex:1}" +
            "input[type=range]{width:100%;margin:15px 0}</style></head>" +
            "<body><h1>MoonlightVR</h1>" +
            "<div class=\"card intro\">" +
            "<div class=\"card\"><h3>Version: " + VERSION + "</h3>" +
            "<p class=\"status\">HTTPS Server Running on port 8555</p></div>" +
            "<div class=\"card\"><a href=\"/\" style=\"color:#aa344a;\">&larr; Back to Canvas</a></div>" +
            "<div class=\"card\"><h2>Screen Position</h2>" +
            "<div class=\"row\"><button class=\"btn\" onclick=\"fetch('/zoomin')\">Zoom In</button>" +
            "<button class=\"btn\" onclick=\"fetch('/zoomout')\">Zoom Out</button></div>" +
            "<p>Closer screen = more immersive, farther = less strain</p></div>" +
            "<div class=\"card\"><h2>Screen Size</h2>" +
            "<div class=\"row\"><button class=\"btn\" onclick=\"fetch('/screenup')\">Bigger</button>" +
            "<button class=\"btn\" onclick=\"fetch('/screendown')\">Smaller</button></div>" +
            "<p>Adjust virtual screen size</p></div>" +
            "<div class=\"card\"><h2>Curvature Preset</h2>" +
            "<div class=\"row\"><button class=\"btn\" onclick=\"fetch('/curvature-flat')\">Flat</button>" +
            "<button class=\"btn\" onclick=\"fetch('/curvature-tv')\">Void Sphere</button>" +
            "<button class=\"btn\" onclick=\"fetch('/curvature-gaming')\">Gaming Monitor</button></div>" +
            "<p>Choose screen shape for VR view</p></div>" +
            "<div class=\"card\"><h2>Curvature Intensity</h2>" +
            "<div class=\"row\"><button class=\"btn\" onclick=\"fetch('/curvature-decrease')\">Less</button>" +
            "<button class=\"btn\" onclick=\"fetch('/curvature-increase')\">More</button></div>" +
            "<p>Control how much screen wraps around you</p></div>" +
            "<div class=\"card\"><h2>Horizontal Curve</h2>" +
            "<div class=\"row\"><button class=\"btn\" onclick=\"fetch('/horizontal-decrease')\">Less</button>" +
            "<button class=\"btn\" onclick=\"fetch('/horizontal-increase')\">More</button></div>" +
            "<p>Adjust horizontal bend (Void Sphere mode)</p></div>" +
            "<div class=\"card\"><h2>Vertical Curve</h2>" +
            "<div class=\"row\"><button class=\"btn\" onclick=\"fetch('/vertical-decrease')\">Less</button>" +
            "<button class=\"btn\" onclick=\"fetch('/vertical-increase')\">More</button></div>" +
            "<p>Adjust vertical bend (Void Sphere mode only)</p></div>" +
            "<div class=\"card\"><h2>View</h2>" +
            "<button class=\"btn\" onclick=\"fetch('/recenter')\">Recenter View</button>" +
            "<p>Reset view orientation to center</p></div>" +
            "</body></html>";
        return newFixedLengthResponse(Response.Status.OK, "text/html", html);
    }

    private Response serveStaticAsset(String uri) {
        try {
            InputStream is = getClass().getResourceAsStream("/assets/vr-control" + uri);
            if (is != null) {
                String mime = getMimeType(uri);
                return newChunkedResponse(Response.Status.OK, mime, is);
            }
        } catch (Exception ignored) {}
        return newFixedLengthResponse(Response.Status.NOT_FOUND, "text/plain", "Not Found");
    }

    private String getMimeType(String uri) {
        if (uri.endsWith(".css")) return "text/css";
        if (uri.endsWith(".js")) return "application/javascript";
        if (uri.endsWith(".png")) return "image/png";
        if (uri.endsWith(".jpg") || uri.endsWith(".jpeg")) return "image/jpeg";
        if (uri.endsWith(".svg")) return "image/svg+xml";
        if (uri.endsWith(".json")) return "application/json";
        return "text/plain";
    }

    private void resetQuickZoomIdle() {
        if (quickZoomTimer != null) {
            quickZoomTimer.cancel();
            quickZoomTimer.purge();
        }
        quickZoomTimer = new Timer();
        quickZoomTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                if (preZoomSize > 0 && vrRenderer != null) {
                    float currentSize = vrRenderer.getScreenSize();
                    if (Math.abs(currentSize - preZoomSize) > 0.01f) {
                        animateZoomTo(preZoomSize);
                    }
                    preZoomSize = -1f;
                }
            }
        }, 5000);
    }

    private void animateZoomTo(final float targetSize) {
        if (vrRenderer == null) return;

        if (animationHandler == null) {
            animationHandler = new Handler(Looper.getMainLooper());
        }
        if (zoomAnimator != null) {
            animationHandler.removeCallbacks(zoomAnimator);
        }

        final float startSize = vrRenderer.getScreenSize();
        animStartSize = startSize;
        animTargetSize = targetSize;
        animStartTime = System.currentTimeMillis();

        zoomAnimator = new Runnable() {
            @Override
            public void run() {
                long elapsed = System.currentTimeMillis() - animStartTime;
                float t = Math.min(1.0f, (float) elapsed / ZOOM_ANIMATION_DURATION_MS);
                t = t * t * (3f - 2f * t);

                float newSize = animStartSize + (animTargetSize - animStartSize) * t;
                vrRenderer.setScreenSize(newSize);

                if (elapsed < ZOOM_ANIMATION_DURATION_MS) {
                    animationHandler.postDelayed(this, 16);
                }
            }
        };
        animationHandler.post(zoomAnimator);
    }

    private void resetToPreferenceSettings() {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        
        int vrDistanceRaw = prefs.getInt("seekbar_vr_screen_distance", 20);
        float vrDistance = vrDistanceRaw / 10f;
        
        int vrSizeRaw = prefs.getInt("seekbar_vr_screen_size", 50);
        float vrSize = vrSizeRaw / 50f;
        
        vrRenderer.setScreenDistance(vrDistance);
        vrRenderer.setScreenSize(vrSize);
    }

    public int getPort() {
        return DEFAULT_PORT;
    }
}

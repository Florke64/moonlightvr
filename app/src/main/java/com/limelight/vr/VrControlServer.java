package com.limelight.vr;

import android.content.Context;
import android.content.SharedPreferences;
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

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;

public class VrControlServer extends NanoHTTPD {
    private static final int DEFAULT_PORT = 8555;
    private static final String VERSION = BuildConfig.VERSION_NAME;
    private static final String KEY_ALIAS = "MoonlightVrServerKey";
    private static final String PREF_NAME = "VrControlPrefs";
    private static final String PREF_KEY_PASS = "keystore_pass";
    private static final String KEYSTORE_FILE = "vr_keystore.p12";

    private final Context context;
    private VrRenderer vrRenderer;

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
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" +
            "<title>MoonlightVR Control</title>" +
            "<style>" +
            "body{font-family:sans-serif;margin:0;padding:20px;background:#111;color:#eee}" +
            "h1{color:#00d8ff}.card{background:#222;padding:20px;border-radius:8px;margin:10px 0}" +
            ".intro{border-left:4px solid #00d8ff}" +
            ".status{color:#4f4}.btn{display:block;width:100%;padding:15px;margin:5px 0;" +
            "background:#00d8ff;border:none;border-radius:4px;color:#111;font-size:16px;font-weight:bold;cursor:pointer}" +
            ".btn:active{background:#00b8e6}.row{display:flex;gap:10px}.row .btn{flex:1}" +
            "input[type=range]{width:100%;margin:15px 0}</style></head>" +
            "<body><h1>MoonlightVR</h1>" +
            "<div class=\"card intro\"><h2>About MoonlightVR</h2>" +
            "<p>MoonlightVR extends Moonlight Android with Google Cardboard rendering so your streamed game appears on a stable virtual screen in 3D space with head tracking.</p>" +
            "<p>This integrated HTTPS control panel lets you fine-tune distance, size, and curvature in real time while you play.</p></div>" +
            "<div class=\"card\"><h3>Version: " + VERSION + "</h3>" +
            "<p class=\"status\">HTTPS Server Running on port 8555</p></div>" +
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

    public int getPort() {
        return DEFAULT_PORT;
    }
}

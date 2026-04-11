# AGENTS

## Repository Goal
This fork of the Moonlight Android client adds native support for Google Cardboard VR rendering so streamed Sunshine/Moonlight gameplay appears on a virtual TV screen. The VR screen is anchored in 3D space with proper head tracking and adjustable distance. The intent is to keep the existing flat-screen pipeline intact while layering in the VR experience when the user opts in.

## Key Concepts
- **Base project:** The original `moonlight-android` client with its multi-activity flow, StreamView/SurfaceHolder rendering, decoder pipeline, and JNI bridge (including `moonlight-core`).
- **Extended VR path:** A dedicated `GLSurfaceView` (`app/src/main/java/com/limelight/vr/VrRenderer.java`) that drives a Cardboard-powered native renderer (`app/src/main/jni/vr/`), handles lifecycle callbacks, and surfaces a `Surface` for the decoder. 
- **Head Tracking & Rendering:** Native head tracking computes per-eye View and Projection matrices using the Cardboard SDK. These are combined into an MVP matrix passed to the shader to keep the virtual screen stationary in 3D space. The model matrix explicitly flips the Y-axis scale to correct upside-down texture orientation.
- **VR Preferences:** `PreferenceConfiguration` exposes `checkbox_enable_vr` (to toggle VR mode) and a slider for "VR screen distance" (1.0–4.0m). `Game` reads these preferences, initializing the VR surface when enabled and passing the user's distance preference down to the native renderer.
- **Native build:** Custom NDK module `vr_renderer` compiles Cardboard SDK sources (via `vendor/cardboard`) alongside the existing `moonlight-core` libs, linking against `libc++_shared` and `atomic`.

## Project Structure Notes
- `app/` contains the Android client. VR-specific Java changes sit in `app/src/main/java/com/limelight/vr/`. UI adjustments (like the distance slider) are located in `app/src/main/res/xml/preferences.xml` and `strings.xml`.
- `app/src/main/jni/` houses the JNI build. The `vr/` directory holds the native VR renderer (`vr_renderer.cpp/h`) and JNI bindings (`vr_renderer_jni.cc`). This is where the MVP matrix calculations, screen distance clamping (`SetScreenDistance`), and `UpdateModelMatrix` logic reside.
- `vendor/cardboard/` is a git submodule providing the SDK/proto sources used by the native renderer.

## Implementation Strategy
1. Allow `Game` to switch between StreamView and `GLSurfaceView` VR rendering, tying decoder setup to whichever surface is active.
2. Introduce `VrRenderer` in Java; it owns the `GLSurfaceView.Renderer`, surface lifecycle callbacks, and passes state (like `setScreenDistance`) to the native layer.
3. In JNI, build a `vr_renderer` shared library: include Cardboard SDK sources, handle per-eye viewports, apply the MVP uniform for head tracking, and expose JNI entry points for initialization, frame drawing, and preference updates.
4. Keep the legacy `SurfaceHolder` path untouched so existing deployments continue to stream to the device screen unless `enableVr` is set.

## Notable Divergences from Upstream Moonlight-Android
- Added the Cardboard SDK submodule and a new native VR renderer module.
- **Dependency Packaging:** Cardboard SDK Java classes are explicitly packaged and preserved via ProGuard rules to prevent `ClassNotFoundException` crashes during native JNI invocations.
- Introduced VR preference strings, layout updates, and a SeekBar slider for distance control.
- Modified decoder configuration (`MediaCodecDecoderRenderer`) to accept an explicit `Surface`, enabling rendering into the Cardboard surface instead of the legacy `SurfaceHolder`.
- Updated `ComputerDatabaseManager` resilience (directory creation, migration logging) to improve robustness after clearing app data.

## For New Agents
- **Important:** Do NOT modify or commit changes to git submodules (especially `vendor/cardboard/`). Only operate on the main `moonlight-android` branch code. Submodule updates should only be done manually by the repository owner.
- **Build Requirements:** Building this project requires Java 17 (e.g., for `jlink`). Ensure you build with: `export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 && ./gradlew :app:assembleDebug`.
- Start with `Game` and `VrRenderer` to understand how the Java layer chooses paths and passes parameters to JNI.
- When modifying rendering logic in `vr_renderer.cpp`, remember that the `model_matrix` handles orientation correction (Y-axis flip) and distance scaling, while the MVP matrix handles the Cardboard 3DOF head tracking.
- Keep the Cardboard submodule intact—avoid committing IDE/config artifacts, and run `git submodule update --init --recursive` if dependencies appear missing.
- Any UI strings, preferences, or Android lifecycle tweaks should be done in the `app/src/main` layer so the experience stays cohesive for both VR and non-VR users.

## Integrated HTTPS Server
The app includes an embedded HTTPS server for VR control panel access from LAN devices.

- **Server:** NanoHTTPD-based lightweight HTTP server (via Gradle dependency `org.nanohttpd:nanohttpd:2.3.1`) with TLS support.
- **Port:** 8555 (HTTPS with self-signed certificate).
- **Auto-start:** Server starts automatically when VR mode is enabled (`checkbox_enable_vr` preference).
- **Service:** `VrControlService` runs as a foreground service to keep the server alive.
- **Certificate Architecture (Option A):** To bypass Conscrypt's hardware key padding restrictions, the server uses BouncyCastle (`bcprov-jdk18on` / `bcpkix-jdk18on`) to generate a software RSA KeyPair on first run.
  - Generated KeyPair is wrapped in an X.509 certificate, saved to an app-private `PKCS12` keystore (`vr_keystore.p12`).
  - The keystore is encrypted with a 16-byte secure random password stored in `SharedPreferences`, leaving no hardcoded credentials inside the APK.
- **Static UI:** Version info is sourced from `BuildConfig.VERSION_NAME`.
- **Files:**
  - `app/src/main/java/com/limelight/vr/VrControlServer.java` - HTTP server implementation that builds the runtime keystore and serves the control page.
  - `app/src/main/java/com/limelight/vr/VrControlService.java` - Foreground service managing the NanoHTTPD lifecycle.

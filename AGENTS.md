# AGENTS

## Repository Goal
This fork of the Moonlight Android client (**MoonlightVR**) adds native support for Google Cardboard VR rendering so streamed Sunshine/Moonlight gameplay appears on a virtual TV screen. The VR screen is anchored in 3D space with proper head tracking and adjustable distance. The intent is to keep the existing flat-screen pipeline intact while layering in the VR experience.

## Key Concepts
- **Base project:** The original `moonlight-android` client with its multi-activity flow, StreamView/SurfaceHolder rendering, decoder pipeline, and JNI bridge (including `moonlight-core`).
- **Extended VR path:** A dedicated `GLSurfaceView` (`app/src/main/java/com/limelight/vr/VrRenderer.java`) that drives a Cardboard-powered native renderer (`app/src/main/jni/vr/`), handles lifecycle callbacks, and surfaces a `Surface` for the decoder. 
- **Head Tracking & Rendering:** Native head tracking computes per-eye View and Projection matrices using the Cardboard SDK. These are combined into an MVP matrix passed to the shader to keep the virtual screen stationary in 3D space. The model matrix explicitly flips the Y-axis scale to correct upside-down texture orientation.
- **VR Preferences:** `PreferenceConfiguration` exposes:
  - `checkbox_enable_vr` - Toggle VR mode
  - `seekbar_vr_screen_distance` - Screen distance (1.0–4.0m)
  - `seekbar_vr_screen_size` - Screen size multiplier (0.25x–2.0x, up to 4.0x with quick zoom)
  - `checkbox_quick_zoom` - Enable magnifying glass mode with auto-revert
  - Curvature settings (mode, amount, horizontal/vertical)
  - `checkbox_enable_skybox` - Toggle skybox background
  - Lens scale and per-eye offset are now persisted along with a `checkbox_lock_vr_lenses` toggle that blocks accidental adjustments in VR mode.
  - `checkbox_enable_vr_camera_pip` - Toggle rear camera picture-in-picture view in VR (off by default).
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
- **Dependency Warning:** Never add, never remove, never update dependencies (Gradle plugins, Android libraries, etc.) unless you fully understand the cascading effects. Dependency changes or environment updates can break the build pipeline, or increase project complexity.
- **Build Requirements:** Building this project requires Java 21. Use Java 21 for all builds:
  ```bash
  export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 && ./gradlew :app:assembleDebug
  ```
- **Toolchain Versions:**
  - **AGP:** 8.13.2 (Android Gradle Plugin)
  - **Gradle:** 8.13
  - **Java:** 21 (LTS)
  - **ProGuard/R8:** Uses `proguard-android-optimize.txt` (not the deprecated `proguard-android.txt`)
  - **compileSdk:** 35 (targetSdk: 35, minSdk: 21)
- Start with `Game` and `VrRenderer` to understand how the Java layer chooses paths and passes parameters to JNI.
- When modifying rendering logic in `vr_renderer.cpp`, remember that the `model_matrix` handles orientation correction (Y-axis flip) and distance scaling, while the MVP matrix handles the Cardboard 3DOF head tracking.
- Keep the Cardboard submodule intact—avoid committing IDE/config artifacts, and run `git submodule update --init --recursive` if dependencies appear missing.
- Any UI strings, preferences, or Android lifecycle tweaks should be done in the `app/src/main` layer so the experience stays cohesive for both VR and non-VR users.

## VR Render Pipeline (Execution Order)
1. `Game.onCreate()` reads `PreferenceConfiguration`, toggles `vrMode`, and instantiates `VrRenderer` when VR is enabled.
2. `VrRenderer` calls `nativeCreate()`/`nativeOnSurfaceCreated()` to allocate `VrMoonlightApp`, create an external OES texture, and feed a `SurfaceTexture`/`Surface` back through `VrRenderer.SurfaceListener`.
3. `Game.handleVrSurfaceReady()` binds that `Surface` to `MediaCodecDecoderRenderer`, so the decoder writes directly into the OES texture managed by VR.
4. Each decoder frame triggers `VrRenderer.onFrameAvailable()`, which requests render, then `onDrawFrame()` updates the `SurfaceTexture`, sends the texture transform via `nativeSetTextureTransform()`, and calls `nativeOnDrawFrame()`.
5. `VrMoonlightApp::OnDrawFrame()` updates Cardboard device params/head pose, renders the video into an offscreen FBO per eye, optionally draws the skybox cubemap, and then hands the FBO texture to `CardboardDistortionRenderer_renderEyeToDisplay()`.
6. `CardboardDistortionRenderer` composites the per-eye viewports, applies lens distortion, writes to the screen, and finally `glDisable(GL_DEPTH_TEST)` plus any debug overlays exit (`kLineVertices`).

## Call Graph: Java -> JNI -> Native -> Cardboard
- `Game` (`app/src/main/java/com/limelight/Game.java`) configures VR settings and starts `VrRenderer` + `UiService`.
- `VrCameraManager` (`app/src/main/java/com/limelight/vr/VrCameraManager.java`) manages Camera2 capture lifecycle, opens rear camera, and streams frames to the GL Surface provided by `VrRenderer`.
- `VrRenderer` (`app/src/main/java/com/limelight/vr/VrRenderer.java`) implements `GLSurfaceView.Renderer`, handles lifecycle, exposes `setScreenDistance`, `setCurvature*`, `setSkyboxEnabled`, camera PiP methods (`createCameraSurface`, `setCameraEnabled`, `setCameraTextureTransform`), and uploads cubemap textures before calling native methods (`nativeCreate`, `nativeOnDrawFrame`, `nativeSetSkyboxTexture`, etc.).
- JNI bridge (`app/src/main/jni/vr/vr_renderer_jni.cc`) forwards every GL/VR call to `VrMoonlightApp` (create, destroy, surface events, transforms, preference setters, skybox toggles), and exposes `nativeOnDrawFrame()` for each frame.
- Native renderer (`app/src/main/jni/vr/vr_renderer.cpp/.h`) allocates Cardboard helpers, compiles shaders, builds meshes, updates model matrices, and orchestrates `RenderVideoToTexture()` plus skybox drawing.
- Cardboard SDK sources under `vendor/cardboard/` supply:
  * `CardboardHeadTracker` (pose prediction),
  * `CardboardLensDistortion` (per-eye projection/eye-from-head matrices),
  * `CardboardDistortionRenderer` (final distortion/compositing),
  * `rendering/opengl_es2_distortion_renderer.cc` (GL draw helper shared in `VrMoonlightApp`).

## Per-Frame Data Flow
1. Decoder writes decoded video frames to the `Surface` provided by `VrRenderer`.
2. `SurfaceTexture` attached to the OES texture is updated inside `VrRenderer.onFrameAvailable()`/`onDrawFrame()`.
3. The OES texture is sampled inside `RenderVideoToTexture()` via `video_program_` when rendering into the FBO (`render_texture_`).
4. The FBO stores both color (`render_texture_`) and depth (`depth_renderbuffer_`) attachments; the screen mesh is drawn twice (per eye) into different viewports and captured texture coordinates.
5. After both eyes are rendered (including the optional skybox pass), `CardboardDistortionRenderer_renderEyeToDisplay()` takes the FBO texture, applies lens distortion meshes, and writes the final stereo output to the back buffer.

## Skybox Integration Notes
- **Cubemap upload path:** `VrRenderer.loadSkyboxCubemap()` loads six `panorama_<0-5>` resources (or HDR variants) and uploads them to GL cubemap faces; after successful upload it calls `nativeSetSkyboxTexture()` to hand the texture ID to native code.
- **Face mapping convention:** `panoramaToFaceMap = {1,3,4,5,0,2}` maps [front/right/up/down/back/left] -> OpenGL targets `GL_TEXTURE_CUBE_MAP_{+X,-X,+Y,-Y,+Z,-Z}` to match Minecraft's strip layout.
- **Rendering order + depth:** Skybox renders immediately after the video mesh in `RenderVideoToTexture()`, with `glDepthFunc(GL_LEQUAL)` and `glDepthMask(GL_FALSE)` so the skybox always passes depth but never writes depth, ensuring it stays behind the screen regardless of depth clears.
- **Common failure modes:** cubemap upload failures (e.g., GL errors on face upload), invalid texture IDs passed to native, shader compile/link failures, Surface context recreation without reuploading the cubemap. Logging (via `LimeLog.warning`) flags these conditions.

## Hotspots for Minor/Intermediate Rendering Changes
- **Geometry adjustments:** `UpdateScreenGeometry()` (native) rebuilds curved mesh verts/texcoords when curvature settings change; change grid density or curvature math here.
- **Transforms:** `UpdateModelMatrix()` controls screen distance, size, position (yaw/pitch orbit), and Z-axis rotation; edit for alternate screen orientation or distance scaling.
- **Per-eye math:** `RenderVideoToTexture()` orchestrates per-eye view/projection matrices, draws the screen/skybox FBO pass, and manages depth state.
- **Shaders:** `kVideo*`, `kLine*`, and `kSkybox*` shader strings near the top of `vr_renderer.cpp`; adjust these strings and the associated attribute/uniform lookups when changing shading behavior.
- **Lifecycle/resource safety:** `VrRenderer` handles texture creation/release, context loss, and `surfaceListener` callbacks; extend it if you need to reinitialize additional GL resources during a context reset or add a custom `Skybox` loader.
 - **Lens distortion customization:** `UpdateDeviceParams()` now tracks a mesh dirty flag so `TransformLensMesh()` can scale and clamp horizontal offsets per eye without rebuilding the Cardboard renderer, making zoom/pan gestures feel immediate.

## Camera PiP Feature
The VR mode includes an optional rear camera picture-in-picture (PiP) view that displays the device's rear camera feed on a secondary screen in VR space.

- **Camera API:** Uses Camera2 API to open the first rear-facing camera (`CameraCharacteristics.LENS_FACING_BACK`).
- **Surface Pipeline:** `VrRenderer.createCameraSurface()` creates an OES texture wrapped in a `SurfaceTexture` + `Surface`. This `Surface` is passed to `VrCameraManager` which runs a capture session and streams frames to it.
- **Transform:** Camera frame transform includes a +90° rotation correction applied via `Matrix.multiplyMM()` in `VrRenderer.onDrawFrame()` to orient the feed upright in VR.
- **Rendering:** Native C++ renders the camera texture to a separate PiP quad positioned below the main game screen (`UpdateCameraModelMatrix()`), drawn per-eye with its own MVP matrix in `RenderVideoToTexture()`.
- **Preference:** Controlled by `checkbox_enable_vr_camera_pip` in preferences (default: off). Camera starts only when enabled and user grants runtime camera permission.
- **Files:**
  - `app/src/main/java/com/limelight/vr/VrCameraManager.java` - Camera2 wrapper managing capture lifecycle.
  - Native camera methods in `vr_renderer.cpp` (`SetCameraTexture`, `SetCameraTextureTransform`, `SetCameraEnabled`, `UpdateCameraModelMatrix`).

## Debug Checklist
- **GL shader compile/link logs:** `LoadGLShader()` logs shader compile failures; `VrMoonlightApp::OnSurfaceCreated()` now checks program link status and logs info logs.
- **Texture ID health:** `loadSkyboxCubemap()` verifies `glGenTextures()` returns non-zero before proceeding, deletes textures when uploads fail, and sets `skyboxTextureId` to zero on cleanup.
- **Surface validity:** `MediaCodecDecoderRenderer.configureAndStartDecoder()` reports severe logs if `Surface` is `null` or `!isValid()`; `Game.handleVrSurfaceReady()` avoids starting a connection until a valid surface arrives.
- **Context recreation behavior:** `VrRenderer.onSurfaceCreated()` reuploads the saved cubemap (if present) to ensure skybox textures survive context loss; add similar reloads if you later add new GL assets.

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

## VR WebUI Canvas Interface
The integrated web UI provides a touch-based canvas for VR scene control.

### Touch Gestures
- **Two-finger pinch:** Zoom in/out (adjusts screen size)
- **Two-finger rotate:** Rotate screen around Z-axis
- **Double-tap:** Recenter view and reset to preference settings

### Endpoints
- `GET /` - Canvas UI
- `GET /buttons.html` - Button-based controls (legacy fallback)
- `GET /recenter` - Recenter view
- `POST /gesture` - Handle gesture JSON (`{"type":"pinch"|"rotate"|"pinch_rotate"|"pan"|"doubletap", ...}`)
- `GET /ping` - Health check, returns `{"status":"pong","ts":<timestamp>}`

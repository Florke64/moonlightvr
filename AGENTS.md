# AGENTS

## Repository Goal
This fork of the Moonlight Android client adds native support for Google Cardboard VR rendering so streamed Sunshine/Moonlight gameplay can appear on a virtual TV screen inside a stereoscopic environment. The intent is to keep the existing flat-screen pipeline intact while layering in the VR experience when the user opts in.

## Key Concepts
- **Base project:** The original `moonlight-android` client with its multi-activity flow, StreamView/SurfaceHolder rendering, decoder pipeline, and JNI bridge (including `moonlight-core`).
- **Extended VR path:** A dedicated `GLSurfaceView` (`app/src/main/java/com/limelight/vr/VrRenderer.java`) that drives a Cardboard-powered native renderer (`app/src/main/jni/vr/`), handles lifecycle callbacks, and surfaces a `Surface` for the decoder.
- **Preferences toggle:** `PreferenceConfiguration` now exposes `checkbox_enable_vr` so users can request the VR experience; `Game` reads the toggle and only initializes the VR surface/decoder path when enabled.
- **Native build:** Custom NDK module `vr_renderer` compiles Cardboard SDK sources (via `vendor/cardboard`) alongside the existing `moonlight-core` libs, linking against `libc++_shared` and `atomic` to satisfy the Cardboard runtime.

## Project Structure Notes
- `app/` contains the Android client, with the main UI (`Game`, preferences, layout resources) and Java helpers. Most VR-specific Java changes sit in `app/src/main/java/com/limelight/vr/` and updated UI XML strings/preferences.
- `app/src/main/jni/` houses the JNI build (Application.mk, Android.mk). The new `vr/` directory holds the native VR renderer, utilities, and JNI glue that interfaced with the Cardboard SDK sources imported under `vendor/cardboard/sdk/` and `vendor/cardboard/proto/`.
- `vendor/cardboard/` is a git submodule (same as original upstream Cardboard repo) providing the SDK/proto sources used by the native renderer. Keep the submodule clean—only track upstream files.
## Implementation Strategy
1. Allow `Game` to switch between StreamView and `GLSurfaceView` VR rendering, tying decoder setup to whichever surface is active.
2. Introduce `VrRenderer` in Java; it owns the `GLSurfaceView.Renderer`, surface lifecycle callbacks, and signals when the native Cardboard compositor surface is ready.
3. In JNI, build a `vr_renderer` shared library: include Cardboard SDK sources, configure `std::thread`, `shared_ptr`, and render to an OpenGL surface, exposing JNI entry points for initialization, frame drawing, and release.
4. Keep the legacy `SurfaceHolder` path untouched so existing deployments continue to stream to the device screen unless `enableVr` is set.

## Notable Divergences from Upstream Moonlight-Android
- Added the Cardboard SDK submodule and a new native VR renderer module instead of relying solely on the existing `moonlight-core` renderer surfaces.
- Introduced VR preference strings, layout updates, and lifecycle handling in `Game` to manage both video surfaces and Cardboard state.
- Modified decoder configuration (`MediaCodecDecoderRenderer`) to accept an explicit `Surface`, enabling rendering into the Cardboard surface instead of the legacy `SurfaceHolder`.
- Updated `ComputerDatabaseManager` resilience (directory creation, migration logging) to improve robustness after clearing app data, ensuring the fork doesn’t silently fail when users reset data.

## For New Agents
- Start with `Game` and `VrRenderer` to understand how the Java layer chooses between flat and VR rendering paths.
- Follow the JNI bridge in `app/src/main/jni/vr/` and `moonlight-core/Android.mk` to see how the Cardboard renderer is built and linked.
- Keep the Cardboard submodule intact—avoid committing IDE/config artifacts, and run `git submodule update --init` when necessary.
- Any UI strings, preferences, or Android lifecycle tweaks should be done in the `app/src/main` layer so the experience stays cohesive for both VR and non-VR users.

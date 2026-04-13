#include "vr_renderer.h"

#include <jni.h>

static JavaVM* g_java_vm = nullptr;

static moonlight_vr::VrMoonlightApp* FromHandle(jlong handle) {
  return reinterpret_cast<moonlight_vr::VrMoonlightApp*>(handle);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  g_java_vm = vm;
  return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_limelight_vr_VrRenderer_nativeCreate(JNIEnv* env, jobject obj,
                                              jobject context,
                                              jobject asset_manager) {
  return reinterpret_cast<jlong>(new moonlight_vr::VrMoonlightApp(
      g_java_vm, context, asset_manager));
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeDestroy(JNIEnv*, jobject, jlong handle) {
  delete FromHandle(handle);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnSurfaceCreated(JNIEnv* env, jobject,
                                                       jlong handle) {
  return FromHandle(handle)->OnSurfaceCreated(env);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnSurfaceChanged(JNIEnv*, jobject,
                                                       jlong handle, jint width,
                                                       jint height) {
  FromHandle(handle)->OnSurfaceChanged(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetTextureTransform(JNIEnv* env, jobject,
                                                          jlong handle,
                                                          jfloatArray matrix) {
  if (matrix == nullptr) {
    return;
  }
  jfloat* elements = env->GetFloatArrayElements(matrix, nullptr);
  FromHandle(handle)->SetTextureTransform(elements);
  env->ReleaseFloatArrayElements(matrix, elements, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetScreenDistance(JNIEnv*, jobject,
                                                        jlong handle,
                                                        jfloat distanceMeters) {
  FromHandle(handle)->SetScreenDistance(distanceMeters);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetScreenSize(JNIEnv*, jobject,
                                                     jlong handle,
                                                     jfloat sizeMultiplier) {
  FromHandle(handle)->SetScreenSize(sizeMultiplier);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeAdjustScreenDistance(JNIEnv*, jobject,
                                                        jlong handle,
                                                        jfloat deltaMeters) {
  FromHandle(handle)->AdjustScreenDistance(deltaMeters);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeAdjustScreenSize(JNIEnv*, jobject,
                                                      jlong handle,
                                                      jfloat deltaMultiplier) {
  FromHandle(handle)->AdjustScreenSize(deltaMultiplier);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetCurvatureMode(JNIEnv*, jobject,
                                                      jlong handle,
                                                      jint mode) {
  FromHandle(handle)->SetCurvatureMode(mode);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetCurvatureAmount(JNIEnv*, jobject,
                                                        jlong handle,
                                                        jfloat percent) {
  FromHandle(handle)->SetCurvatureAmount(percent);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetHorizontalCurvature(JNIEnv*, jobject,
                                                            jlong handle,
                                                            jfloat percent) {
  FromHandle(handle)->SetHorizontalCurvature(percent);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetVerticalCurvature(JNIEnv*, jobject,
                                                           jlong handle,
                                                           jfloat percent) {
  FromHandle(handle)->SetVerticalCurvature(percent);
}
 
extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetSkyboxEnabled(JNIEnv*, jobject,
                                                        jlong handle,
                                                        jboolean enabled) {
  FromHandle(handle)->SetSkyboxEnabled(enabled);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetSkyboxTexture(JNIEnv*, jobject,
                                                         jlong handle,
                                                         jint textureId) {
  FromHandle(handle)->SetSkyboxTexture(textureId);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetSkyboxBrightness(JNIEnv*, jobject,
                                                           jlong handle,
                                                           jfloat brightness) {
  FromHandle(handle)->SetSkyboxBrightness(brightness);
}
 
extern "C" JNIEXPORT jfloat JNICALL
Java_com_limelight_vr_VrRenderer_nativeGetCurvatureAmount(JNIEnv*, jobject,
                                                        jlong handle) {
  return FromHandle(handle)->GetCurvatureAmount();
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_limelight_vr_VrRenderer_nativeGetHorizontalCurvature(JNIEnv*, jobject,
                                                            jlong handle) {
  return FromHandle(handle)->GetHorizontalCurvature();
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_limelight_vr_VrRenderer_nativeGetVerticalCurvature(JNIEnv*, jobject,
                                                          jlong handle) {
  return FromHandle(handle)->GetVerticalCurvature();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnDrawFrame(JNIEnv*, jobject, jlong handle) {
  FromHandle(handle)->OnDrawFrame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnPause(JNIEnv*, jobject, jlong handle) {
  FromHandle(handle)->OnPause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnResume(JNIEnv* env, jobject, jlong handle) {
  FromHandle(handle)->OnResume();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeRecenterView(JNIEnv*, jobject, jlong handle) {
  FromHandle(handle)->RecenterView();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeAdjustScreenPosition(JNIEnv*, jobject,
                                                            jlong handle,
                                                            jfloat deltaX,
                                                            jfloat deltaY) {
  FromHandle(handle)->AdjustScreenPosition(deltaX, deltaY);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeAdjustScreenRotation(JNIEnv*, jobject,
                                                            jlong handle,
                                                            jfloat deltaRadians) {
  FromHandle(handle)->AdjustScreenRotation(deltaRadians);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeSetLensScale(JNIEnv*, jobject,
                                                     jlong handle,
                                                     jfloat scale) {
  FromHandle(handle)->SetLensScale(scale);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeAdjustLeftLensOffset(JNIEnv*, jobject,
                                                             jlong handle,
                                                             jfloat deltaX) {
  FromHandle(handle)->AdjustLeftLensOffset(deltaX);
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeAdjustRightLensOffset(JNIEnv*, jobject,
                                                              jlong handle,
                                                              jfloat deltaX) {
  FromHandle(handle)->AdjustRightLensOffset(deltaX);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_limelight_vr_VrRenderer_nativeGetScreenSize(JNIEnv*, jobject, jlong handle) {
  return FromHandle(handle)->GetScreenSize();
}

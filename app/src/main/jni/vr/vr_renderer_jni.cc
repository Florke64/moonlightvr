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
Java_com_limelight_vr_VrRenderer_nativeOnDrawFrame(JNIEnv*, jobject, jlong handle) {
  FromHandle(handle)->OnDrawFrame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnPause(JNIEnv*, jobject, jlong handle) {
  FromHandle(handle)->OnPause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_limelight_vr_VrRenderer_nativeOnResume(JNIEnv*, jobject, jlong handle) {
  FromHandle(handle)->OnResume();
}

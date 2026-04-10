#ifndef VR_RENDERER_H_
#define VR_RENDERER_H_

#include <array>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include "cardboard.h"
#include "util.h"

namespace moonlight_vr {

class VrMoonlightApp {
 public:
  VrMoonlightApp(JavaVM* vm, jobject activity, jobject asset_manager);
  ~VrMoonlightApp();

  jint OnSurfaceCreated(JNIEnv* env);
  void OnSurfaceChanged(int width, int height);
  void SetTextureTransform(const float* transform);
  void OnDrawFrame();
  void OnPause();
  void OnResume();

 private:
  void ResetRenderTarget();
  void UpdateRenderTarget();
  bool UpdateDeviceParams();
  void RenderVideoToTexture(const std::array<float, 3>& head_position,
                            const std::array<float, 4>& head_orientation);
  void DestroyCardboardResources();

  JavaVM* java_vm_;
  jobject activity_;
  jobject java_asset_mgr_;
  AAssetManager* asset_mgr_;

  CardboardHeadTracker* head_tracker_;
  CardboardLensDistortion* lens_distortion_;
  CardboardDistortionRenderer* distortion_renderer_;

  CardboardEyeTextureDescription left_eye_texture_description_;
  CardboardEyeTextureDescription right_eye_texture_description_;

  bool screen_params_changed_;
  bool device_params_changed_;
  int screen_width_;
  int screen_height_;

  GLuint video_program_;
  GLuint video_texture_;
  GLuint render_texture_;
  GLuint framebuffer_;
  GLuint depth_renderbuffer_;
  GLint position_attrib_;
  GLint texcoord_attrib_;
  GLint texture_transform_uniform_;
  GLint sampler_uniform_;
  GLint mvp_uniform_;

  float texture_transform_[16];
  Matrix4x4 model_matrix_;
};

}  // namespace moonlight_vr

#endif  // VR_RENDERER_H_

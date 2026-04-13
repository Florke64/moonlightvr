#ifndef VR_RENDERER_H_
#define VR_RENDERER_H_

#include <array>
#include <vector>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include "cardboard.h"
#include "util.h"

namespace moonlight_vr {

class VrMoonlightApp {
 public:
  static constexpr int kCurvatureModeFlat = 0;
  static constexpr int kCurvatureModeTvCinema = 1;
  static constexpr int kCurvatureModeGamingScreen = 2;
  VrMoonlightApp(JavaVM* vm, jobject activity, jobject asset_manager);
  ~VrMoonlightApp();

  jint OnSurfaceCreated(JNIEnv* env);
  void OnSurfaceChanged(int width, int height);
  void SetTextureTransform(const float* transform);
  void OnDrawFrame();
  void OnPause();
  void OnResume();
  void SetScreenDistance(float meters);
  void SetScreenSize(float sizeMultiplier);
  void AdjustScreenDistance(float deltaMeters);
  void AdjustScreenSize(float deltaMultiplier);
  void AdjustScreenPosition(float deltaX, float deltaY);
  void AdjustScreenRotation(float deltaRadians);
  void SetCurvatureMode(int mode);
  void SetCurvatureAmount(float percent);
  void SetHorizontalCurvature(float percent);
  void SetVerticalCurvature(float percent);
  void SetSkyboxEnabled(bool enabled);
  void SetSkyboxTexture(GLuint textureId);
  void SetSkyboxBrightness(float brightness);
  void SetLensScale(float scale);
  void AdjustLeftLensOffset(float deltaX);
  void AdjustRightLensOffset(float deltaX);
  float GetCurvatureAmount() const;
  float GetHorizontalCurvature() const;
  float GetVerticalCurvature() const;
  float GetScreenSize() const;
  void SetCurrentFramePose(const std::array<float, 4>& orientation);
  void RecenterView();

 private:
  void ResetRenderTarget();
  void UpdateRenderTarget();
  bool UpdateDeviceParams();
  void RenderVideoToTexture(const std::array<float, 3>& head_position,
                            const std::array<float, 4>& head_orientation);
  void DestroyCardboardResources();
  void UpdateModelMatrix();
  void UpdateScreenGeometry();
  void TransformLensMesh(std::vector<float>& vertices, float scale,
                         float offset_x, bool is_left_eye);

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
  GLuint line_program_;
  GLuint video_texture_;
  GLuint render_texture_;
  GLuint framebuffer_;
  GLuint depth_renderbuffer_;
  GLint position_attrib_;
  GLint texcoord_attrib_;
  GLint texture_transform_uniform_;
  GLint sampler_uniform_;
  GLint mvp_uniform_;
  GLint line_pos_attrib_;
  GLint line_mvp_uniform_;
  GLuint skybox_program_;
  GLint skybox_pos_attrib_;
  GLint skybox_vp_uniform_;
  GLint skybox_sampler_uniform_;
  GLint skybox_brightness_uniform_;
  GLuint skybox_texture_;

  float texture_transform_[16];
  Matrix4x4 model_matrix_;
  float screen_distance_meters_;
  float screen_size_multiplier_;
  int curvature_mode_;
  float curvature_amount_percent_;
  float horizontal_curvature_percent_;
  float vertical_curvature_percent_;
  std::vector<GLfloat> screen_vertices_;
  std::vector<GLfloat> screen_texcoords_;
  bool using_curved_geometry_;
  int curved_rows_;
  int curved_vertices_per_strip_;
  std::array<float, 4> last_rendered_orientation_;
  bool has_render_pose_;
  bool skybox_enabled_ = true;
  float skybox_brightness_ = 1.0f;
  std::array<float, 4> recenter_offset_;
  bool has_recenter_offset_;
  std::array<float, 4> current_recenter_offset_;
  bool has_current_recenter_offset_;
  int64_t last_recenter_update_nanos_;
  bool geometry_dirty_;
  float screen_yaw_radians_ = 0.0f;
  float screen_pitch_radians_ = 0.0f;
  float screen_rotation_radians_ = 0.0f;
  float lens_scale_ = 1.0f;
  float left_lens_offset_x_ = 0.0f;
  float right_lens_offset_x_ = 0.0f;
  bool lens_mesh_dirty_ = true;
};

}  // namespace moonlight_vr

#endif  // VR_RENDERER_H_

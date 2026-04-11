#include "vr_renderer.h"

#include <jni.h>
#include <android/asset_manager_jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/log.h>

#include "cardboard.h"
#include "qr_code.h"

namespace moonlight_vr {

namespace {

constexpr float kZNear = 0.1f;
constexpr float kZFar = 100.0f;
constexpr int kVelocityFilterCutoffFrequency = 6;
constexpr int64_t kPredictionTimeWithoutVsyncNanos = 50000000LL;
constexpr float kScreenWidthMeters = 2.2f;
constexpr float kScreenAspectRatio = 16.0f / 9.0f;
constexpr float kMinScreenDistanceMeters = 1.0f;
constexpr float kMaxScreenDistanceMeters = 4.0f;
constexpr float kDefaultScreenDistanceMeters = 2.0f;

constexpr int kEyeCount = 2;

const GLfloat kQuadVertices[] = {
    -1.f, -1.f, 0.f,
     1.f, -1.f, 0.f,
    -1.f,  1.f, 0.f,
     1.f,  1.f, 0.f,
};

const GLfloat kQuadTexCoords[] = {
     0.f, 1.f,
     1.f, 1.f,
     0.f, 0.f,
     1.f, 0.f,
};

const char kVideoVertexShader[] =
    "attribute vec4 a_Position;"              "\n"
    "attribute vec2 a_TexCoord;"             "\n"
    "uniform mat4 u_TextureTransform;"       "\n"
    "uniform mat4 u_MVP;"                    "\n"
    "varying vec2 v_TexCoord;"               "\n"
    "void main() {"                           "\n"
    "  v_TexCoord = (u_TextureTransform * vec4(a_TexCoord, 0.0, 1.0)).xy;" "\n"
    "  gl_Position = u_MVP * a_Position;"     "\n"
    "}";

const char kVideoFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require" "\n"
    "precision mediump float;"                 "\n"
    "uniform samplerExternalOES u_Texture;"    "\n"
    "varying vec2 v_TexCoord;"                 "\n"
    "void main() {"                            "\n"
    "  gl_FragColor = texture2D(u_Texture, v_TexCoord);" "\n"
    "}";

const char kLineVertexShader[] =
    "attribute vec4 a_Position;"              "\n"
    "uniform mat4 u_MVP;"                    "\n"
    "void main() {"                           "\n"
    "  gl_Position = u_MVP * a_Position;"     "\n"
    "}";

const char kLineFragmentShader[] =
    "precision mediump float;"                "\n"
    "void main() {"                           "\n"
    "  gl_FragColor = vec4(0.627, 0.627, 0.627, 1.0);" "\n"
    "}";

const GLfloat kLineVertices[] = {
    0.f, -1.f, 0.f,
    0.f,  1.f, 0.f,
};

}  // namespace

VrMoonlightApp::VrMoonlightApp(JavaVM* vm, jobject activity, jobject asset_manager)
    : java_vm_(vm),
      activity_(nullptr),
      java_asset_mgr_(nullptr),
      asset_mgr_(nullptr),
      head_tracker_(nullptr),
      lens_distortion_(nullptr),
      distortion_renderer_(nullptr),
      screen_params_changed_(false),
      device_params_changed_(true),
      screen_width_(0),
      screen_height_(0),
      video_program_(0),
      line_program_(0),
      video_texture_(0),
      render_texture_(0),
      framebuffer_(0),
      depth_renderbuffer_(0),
      position_attrib_(-1),
      texcoord_attrib_(-1),
      texture_transform_uniform_(-1),
      sampler_uniform_(-1),
      mvp_uniform_(-1),
      line_pos_attrib_(-1),
      line_mvp_uniform_(-1),
      model_matrix_(),
      screen_distance_meters_(kDefaultScreenDistanceMeters),
      screen_size_multiplier_(1.0f) {
  std::fill(std::begin(texture_transform_), std::end(texture_transform_), 0.f);
  texture_transform_[0] = texture_transform_[5] = texture_transform_[10] =
      texture_transform_[15] = 1.f;

  JNIEnv* env = nullptr;
  int result = java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (result != JNI_OK) {
    // If the environment doesn't exist, attach the current thread
    result = java_vm_->AttachCurrentThread(&env, nullptr);
    if (result != JNI_OK) {
      // Log error if attaching fails
      return;
    }
  }

  if (env != nullptr) {
    activity_ = env->NewGlobalRef(activity);
    java_asset_mgr_ = env->NewGlobalRef(asset_manager);
    asset_mgr_ = AAssetManager_fromJava(env, asset_manager);
  }

  if (activity_ && java_asset_mgr_) {
    Cardboard_initializeAndroid(java_vm_, activity_);
    head_tracker_ = CardboardHeadTracker_create();
    CardboardHeadTracker_setLowPassFilter(head_tracker_, kVelocityFilterCutoffFrequency);
  } else {
    // Log error: could not initialize Cardboard resources
    head_tracker_ = nullptr;
  }

  UpdateModelMatrix();
}

VrMoonlightApp::~VrMoonlightApp() {
  DestroyCardboardResources();
  if (render_texture_) {
    glDeleteTextures(1, &render_texture_);
  }
  if (video_texture_) {
    glDeleteTextures(1, &video_texture_);
  }
  if (line_program_) {
    glDeleteProgram(line_program_);
  }
  if (framebuffer_) {
    glDeleteFramebuffers(1, &framebuffer_);
  }
  if (depth_renderbuffer_) {
    glDeleteRenderbuffers(1, &depth_renderbuffer_);
  }
  if (head_tracker_) {
    CardboardHeadTracker_destroy(head_tracker_);
  }
  JNIEnv* env = nullptr;
  if (java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
    if (activity_) {
      env->DeleteGlobalRef(activity_);
    }
    if (java_asset_mgr_) {
      env->DeleteGlobalRef(java_asset_mgr_);
    }
  }
}

jint VrMoonlightApp::OnSurfaceCreated(JNIEnv* env) {
  if (video_program_ == 0) {
    GLuint vertex_shader = LoadGLShader(GL_VERTEX_SHADER, kVideoVertexShader);
    GLuint fragment_shader = LoadGLShader(GL_FRAGMENT_SHADER, kVideoFragmentShader);
    video_program_ = glCreateProgram();
    glAttachShader(video_program_, vertex_shader);
    glAttachShader(video_program_, fragment_shader);
    glLinkProgram(video_program_);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    position_attrib_ = glGetAttribLocation(video_program_, "a_Position");
    texcoord_attrib_ = glGetAttribLocation(video_program_, "a_TexCoord");
    texture_transform_uniform_ = glGetUniformLocation(video_program_,
                                                      "u_TextureTransform");
    mvp_uniform_ = glGetUniformLocation(video_program_, "u_MVP");
    sampler_uniform_ = glGetUniformLocation(video_program_, "u_Texture");
  }

  if (line_program_ == 0) {
    GLuint line_vs = LoadGLShader(GL_VERTEX_SHADER, kLineVertexShader);
    GLuint line_fs = LoadGLShader(GL_FRAGMENT_SHADER, kLineFragmentShader);
    line_program_ = glCreateProgram();
    glAttachShader(line_program_, line_vs);
    glAttachShader(line_program_, line_fs);
    glLinkProgram(line_program_);
    glDeleteShader(line_vs);
    glDeleteShader(line_fs);
    line_pos_attrib_ = glGetAttribLocation(line_program_, "a_Position");
    line_mvp_uniform_ = glGetUniformLocation(line_program_, "u_MVP");
  }

  if (video_texture_ == 0) {
    glGenTextures(1, &video_texture_);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, video_texture_);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  }

  ResetRenderTarget();

  return static_cast<jint>(video_texture_);
}

void VrMoonlightApp::OnSurfaceChanged(int width, int height) {
  screen_width_ = width;
  screen_height_ = height;
  screen_params_changed_ = true;
  UpdateRenderTarget();
}

void VrMoonlightApp::SetTextureTransform(const float* transform) {
  if (transform != nullptr) {
    std::copy(transform, transform + 16, texture_transform_);
  }
}

void VrMoonlightApp::OnDrawFrame() {
  if (screen_width_ == 0 || screen_height_ == 0) {
    return;
  }
  if (!UpdateDeviceParams()) {
    return;
  }
  int64_t timestamp = GetBootTimeNano() + kPredictionTimeWithoutVsyncNanos;
  float position[3];
  float orientation[4];
  CardboardHeadTracker_getPose(head_tracker_, timestamp, kLandscapeLeft,
                               position, orientation);

  const std::array<float, 3> head_position = {position[0], position[1],
                                              position[2]};
  const std::array<float, 4> head_orientation = {orientation[0], orientation[1],
                                                 orientation[2], orientation[3]};

  RenderVideoToTexture(head_position, head_orientation);

  CardboardDistortionRenderer_renderEyeToDisplay(
      distortion_renderer_, 0, 0, 0, screen_width_, screen_height_,
      &left_eye_texture_description_, &right_eye_texture_description_);

  glViewport(0, 0, screen_width_, screen_height_);
  glDisable(GL_DEPTH_TEST);
  Matrix4x4 identity;
  for (int i = 0; i < 4; ++i) {
    identity.m[i][i] = 1.f;
  }
  auto identity_gl = identity.ToGlArray();
  glUseProgram(line_program_);
  glUniformMatrix4fv(line_mvp_uniform_, 1, GL_FALSE, identity_gl.data());
  glEnableVertexAttribArray(line_pos_attrib_);
  glVertexAttribPointer(line_pos_attrib_, 3, GL_FLOAT, GL_FALSE, 0, kLineVertices);
  glDrawArrays(GL_LINES, 0, 2);
  glDisableVertexAttribArray(line_pos_attrib_);
}

void VrMoonlightApp::OnPause() {
  CardboardHeadTracker_pause(head_tracker_);
}

void VrMoonlightApp::OnResume() {
  CardboardHeadTracker_resume(head_tracker_);
  device_params_changed_ = true;
}

void VrMoonlightApp::ResetRenderTarget() {
  if (render_texture_) {
    glDeleteTextures(1, &render_texture_);
    render_texture_ = 0;
  }
  if (framebuffer_) {
    glDeleteFramebuffers(1, &framebuffer_);
    framebuffer_ = 0;
  }
  if (depth_renderbuffer_) {
    glDeleteRenderbuffers(1, &depth_renderbuffer_);
    depth_renderbuffer_ = 0;
  }
}

void VrMoonlightApp::UpdateRenderTarget() {
  ResetRenderTarget();
  if (screen_width_ == 0 || screen_height_ == 0) {
    return;
  }

  glGenTextures(1, &render_texture_);
  glBindTexture(GL_TEXTURE_2D, render_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width_, screen_height_, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenRenderbuffers(1, &depth_renderbuffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, screen_width_,
                        screen_height_);

  glGenFramebuffers(1, &framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         render_texture_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth_renderbuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  left_eye_texture_description_.texture = render_texture_;
  left_eye_texture_description_.left_u = 0.f;
  left_eye_texture_description_.right_u = 0.5f;
  left_eye_texture_description_.top_v = 1.f;
  left_eye_texture_description_.bottom_v = 0.f;

  right_eye_texture_description_ = left_eye_texture_description_;
  right_eye_texture_description_.left_u = 0.5f;
  right_eye_texture_description_.right_u = 1.f;
}

bool VrMoonlightApp::UpdateDeviceParams() {
  if (!screen_params_changed_ && !device_params_changed_) {
    return true;
  }

  uint8_t* params = nullptr;
  int size = 0;
  CardboardQrCode_getSavedDeviceParams(&params, &size);
  bool free_params = false;
  if (size == 0) {
    CardboardQrCode_getCardboardV1DeviceParams(&params, &size);
  } else {
    free_params = true;
  }

  if (screen_width_ == 0 || screen_height_ == 0 || size == 0) {
    if (free_params && params != nullptr) {
      CardboardQrCode_destroy(params);
    }
    return false;
  }

  if (lens_distortion_ != nullptr) {
    CardboardLensDistortion_destroy(lens_distortion_);
    lens_distortion_ = nullptr;
  }
  lens_distortion_ = CardboardLensDistortion_create(params, size, screen_width_,
                                                   screen_height_);
  if (free_params && params != nullptr) {
    CardboardQrCode_destroy(params);
  }
  if (lens_distortion_ == nullptr) {
    return false;
  }

  if (distortion_renderer_ != nullptr) {
    CardboardDistortionRenderer_destroy(distortion_renderer_);
    distortion_renderer_ = nullptr;
  }
  CardboardOpenGlEsDistortionRendererConfig config{kGlTexture2D};
  distortion_renderer_ = CardboardOpenGlEs2DistortionRenderer_create(&config);

  CardboardMesh left_mesh;
  CardboardMesh right_mesh;
  CardboardLensDistortion_getDistortionMesh(lens_distortion_, kLeft, &left_mesh);
  CardboardLensDistortion_getDistortionMesh(lens_distortion_, kRight, &right_mesh);
  CardboardDistortionRenderer_setMesh(distortion_renderer_, &left_mesh, kLeft);
  CardboardDistortionRenderer_setMesh(distortion_renderer_, &right_mesh, kRight);

  screen_params_changed_ = false;
  device_params_changed_ = false;
  return true;
}

void VrMoonlightApp::RenderVideoToTexture(
    const std::array<float, 3>& head_position,
    const std::array<float, 4>& head_orientation) {
  if (framebuffer_ == 0 || render_texture_ == 0 || lens_distortion_ == nullptr) {
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glViewport(0, 0, screen_width_, screen_height_);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);

  glUseProgram(video_program_);
  glEnableVertexAttribArray(position_attrib_);
  glVertexAttribPointer(position_attrib_, 3, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat) * 3, kQuadVertices);
  glEnableVertexAttribArray(texcoord_attrib_);
  glVertexAttribPointer(texcoord_attrib_, 2, GL_FLOAT, GL_FALSE, 0,
                        kQuadTexCoords);
  glUniformMatrix4fv(texture_transform_uniform_, 1, GL_FALSE, texture_transform_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, video_texture_);
  glUniform1i(sampler_uniform_, 0);

  Quatf orientation(head_orientation[0], head_orientation[1],
                    head_orientation[2], head_orientation[3]);
  Matrix4x4 head_view = GetViewMatrixFromPose(head_position, orientation);

  const int left_eye_width = screen_width_ / kEyeCount;
  const int right_eye_width = screen_width_ - left_eye_width;
  const std::array<int, kEyeCount> viewport_x = {0, left_eye_width};
  const std::array<int, kEyeCount> viewport_width = {left_eye_width,
                                                   right_eye_width};
  const std::array<CardboardEye, kEyeCount> eyes = {kLeft, kRight};

  for (int eye_index = 0; eye_index < kEyeCount; ++eye_index) {
    float eye_from_head_raw[16];
    CardboardLensDistortion_getEyeFromHeadMatrix(
        lens_distortion_, eyes[eye_index], eye_from_head_raw);
    Matrix4x4 eye_from_head = GetMatrixFromGlArray(eye_from_head_raw);

    float projection_raw[16];
    CardboardLensDistortion_getProjectionMatrix(
        lens_distortion_, eyes[eye_index], kZNear, kZFar, projection_raw);
    Matrix4x4 projection = GetMatrixFromGlArray(projection_raw);

    Matrix4x4 view = eye_from_head * head_view;
    Matrix4x4 mvp = projection * view * model_matrix_;
    std::array<float, 16> mvp_gl = mvp.ToGlArray();

    glViewport(viewport_x[eye_index], 0, viewport_width[eye_index], screen_height_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUniformMatrix4fv(mvp_uniform_, 1, GL_FALSE, mvp_gl.data());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  glDisableVertexAttribArray(position_attrib_);
  glDisableVertexAttribArray(texcoord_attrib_);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VrMoonlightApp::DestroyCardboardResources() {
  if (distortion_renderer_ != nullptr) {
    CardboardDistortionRenderer_destroy(distortion_renderer_);
    distortion_renderer_ = nullptr;
  }
  if (lens_distortion_ != nullptr) {
    CardboardLensDistortion_destroy(lens_distortion_);
    lens_distortion_ = nullptr;
  }
}

void VrMoonlightApp::SetScreenDistance(float meters) {
  screen_distance_meters_ = meters;
  if (screen_distance_meters_ < kMinScreenDistanceMeters) {
    screen_distance_meters_ = kMinScreenDistanceMeters;
  } else if (screen_distance_meters_ > kMaxScreenDistanceMeters) {
    screen_distance_meters_ = kMaxScreenDistanceMeters;
  }
  UpdateModelMatrix();
}

void VrMoonlightApp::SetScreenSize(float sizeMultiplier) {
  screen_size_multiplier_ = sizeMultiplier;
  if (screen_size_multiplier_ < 0.25f) {
    screen_size_multiplier_ = 0.25f;
  } else if (screen_size_multiplier_ > 2.0f) {
    screen_size_multiplier_ = 2.0f;
  }
  UpdateModelMatrix();
}

void VrMoonlightApp::UpdateModelMatrix() {
  const float half_width = kScreenWidthMeters * 0.5f * screen_size_multiplier_;
  const float half_height = half_width / kScreenAspectRatio;
  const std::array<float, 3> scale = {half_width, -half_height, 1.0f};
  const std::array<float, 3> translation = {0.0f, 0.0f, -screen_distance_meters_};
  model_matrix_ = GetTranslationMatrix(translation) * GetScaleMatrix(scale);
}

}  // namespace moonlight_vr

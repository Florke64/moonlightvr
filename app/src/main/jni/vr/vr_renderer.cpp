#include "vr_renderer.h"

#include <jni.h>
#include <android/asset_manager_jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>
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
constexpr int64_t kDisplayPosePredictionNanos = 16000000LL;
constexpr float kScreenWidthMeters = 2.2f;
constexpr float kScreenAspectRatio = 16.0f / 9.0f;
constexpr float kMinScreenDistanceMeters = 1.0f;
constexpr float kMaxScreenDistanceMeters = 4.0f;
constexpr float kDefaultScreenDistanceMeters = 2.0f;

constexpr int kEyeCount = 2;

constexpr float kMaxHorizontalCurvatureAngle = 3.14159265358979323846f;
constexpr float kMaxVerticalCurvatureAngle = 1.57079632679489661923f;
constexpr int kCurvedGridCols = 64;
constexpr int kCurvedGridRowsTv = 32;
constexpr int kCurvedGridRowsGaming = 2;
constexpr float kMinCurvatureAngle = 0.0001f;

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

const char kSkyboxVertexShader[] =
    "attribute vec4 a_Position;"           "\n"
    "uniform mat4 u_VP;"                  "\n"
    "varying vec3 v_TexCoord;"             "\n"
    "void main() {"                        "\n"
    "  v_TexCoord = a_Position.xyz;"       "\n"
    "  vec4 pos = u_VP * vec4(a_Position.xyz, 1.0);" "\n"
    "  gl_Position = pos.xyww;"           "\n"
    "}";

const char kSkyboxFragmentShader[] =
    "precision mediump float;"                               "\n"
    "varying vec3 v_TexCoord;"                               "\n"
    "uniform samplerCube u_SkyboxTexture;"                   "\n"
    "void main() {"                                          "\n"
    "  vec3 dir = normalize(v_TexCoord);"                    "\n"
    "  gl_FragColor = textureCube(u_SkyboxTexture, dir);"    "\n"
    "}";

const GLfloat kSkyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
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
       skybox_program_(0),
       skybox_pos_attrib_(-1),
       skybox_vp_uniform_(-1),
       skybox_sampler_uniform_(-1),
       skybox_texture_(0),
       model_matrix_(),
      screen_distance_meters_(kDefaultScreenDistanceMeters),
      screen_size_multiplier_(1.0f),
      curvature_mode_(VrMoonlightApp::kCurvatureModeFlat),
      curvature_amount_percent_(50.f),
      horizontal_curvature_percent_(50.f),
      vertical_curvature_percent_(50.f),
      using_curved_geometry_(false),
      curved_rows_(0),
      curved_vertices_per_strip_(0),
      last_rendered_orientation_{0.f, 0.f, 0.f, 1.f},
      has_render_pose_(false),
      recenter_offset_{0.f, 0.f, 0.f, 1.f},
      has_recenter_offset_(false),
      current_recenter_offset_{0.f, 0.f, 0.f, 1.f},
      has_current_recenter_offset_(false),
      last_recenter_update_nanos_(0),
      geometry_dirty_(true) {
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
  UpdateScreenGeometry();
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
  if (skybox_program_) {
    glDeleteProgram(skybox_program_);
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

  if (skybox_program_ == 0) {
    GLuint skybox_vs = LoadGLShader(GL_VERTEX_SHADER, kSkyboxVertexShader);
    GLuint skybox_fs = LoadGLShader(GL_FRAGMENT_SHADER, kSkyboxFragmentShader);
    if (skybox_vs != 0 && skybox_fs != 0) {
      skybox_program_ = glCreateProgram();
      glAttachShader(skybox_program_, skybox_vs);
      glAttachShader(skybox_program_, skybox_fs);
      glLinkProgram(skybox_program_);
      GLint linked = GL_FALSE;
      glGetProgramiv(skybox_program_, GL_LINK_STATUS, &linked);
      if (linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(skybox_program_, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
          std::string info(length, '\0');
          glGetProgramInfoLog(skybox_program_, length, nullptr, info.data());
          VR_LOGE("Skybox program link failed: %s", info.c_str());
        }
        glDeleteProgram(skybox_program_);
        skybox_program_ = 0;
      }
      glDeleteShader(skybox_vs);
      glDeleteShader(skybox_fs);
    }

    if (skybox_program_ != 0) {
      skybox_pos_attrib_ = glGetAttribLocation(skybox_program_, "a_Position");
      skybox_vp_uniform_ = glGetUniformLocation(skybox_program_, "u_VP");
      skybox_sampler_uniform_ = glGetUniformLocation(skybox_program_, "u_SkyboxTexture");
    }
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
  if (geometry_dirty_) {
    UpdateScreenGeometry();
    geometry_dirty_ = false;
  }

  if (screen_width_ == 0 || screen_height_ == 0) {
    return;
  }
  if (!UpdateDeviceParams()) {
    return;
  }
  int64_t render_timestamp = GetBootTimeNano() + kPredictionTimeWithoutVsyncNanos;
  float render_position[3];
  float render_orientation[4];
  CardboardHeadTracker_getPose(head_tracker_, render_timestamp, kLandscapeLeft,
                               render_position, render_orientation);

  const std::array<float, 3> head_position = {render_position[0],
                                              render_position[1],
                                              render_position[2]};
  std::array<float, 4> head_orientation = {render_orientation[0],
                                           render_orientation[1],
                                           render_orientation[2],
                                           render_orientation[3]};

  Quatf inv_recenter(0.f, 0.f, 0.f, 1.f);
  if (has_recenter_offset_) {
    const int64_t now_nanos = GetBootTimeNano();
    if (!has_current_recenter_offset_) {
      current_recenter_offset_ = {0.f, 0.f, 0.f, 1.f};
      has_current_recenter_offset_ = true;
    }

    float alpha = 0.05f;
    if (last_recenter_update_nanos_ != 0) {
      const float dt_seconds =
          static_cast<float>(now_nanos - last_recenter_update_nanos_) /
          1000000000.0f;
      constexpr float kRecenterTimeConstantSeconds = 1.0f;
      alpha = 1.0f - std::exp(-dt_seconds / kRecenterTimeConstantSeconds);
      if (alpha < 0.0f) {
        alpha = 0.0f;
      } else if (alpha > 1.0f) {
        alpha = 1.0f;
      }
    }
    last_recenter_update_nanos_ = now_nanos;

    Quatf current_q(current_recenter_offset_[0], current_recenter_offset_[1],
                    current_recenter_offset_[2], current_recenter_offset_[3]);
    Quatf target_q(recenter_offset_[0], recenter_offset_[1],
                   recenter_offset_[2], recenter_offset_[3]);
    Quatf slerped = SlerpQuaternions(current_q, target_q, alpha);
    current_recenter_offset_ = {slerped.x, slerped.y, slerped.z, slerped.w};
    inv_recenter = Quatf(-slerped.x, -slerped.y, -slerped.z, slerped.w);

    Quatf render_q(head_orientation[0], head_orientation[1], head_orientation[2],
                   head_orientation[3]);
    Quatf corrected_render = render_q * inv_recenter;
    head_orientation = {corrected_render.x, corrected_render.y,
                        corrected_render.z, corrected_render.w};
  }

  SetCurrentFramePose(head_orientation);
  RenderVideoToTexture(head_position, head_orientation);

  int64_t display_timestamp = GetBootTimeNano() + kDisplayPosePredictionNanos;
  float display_position[3];
  float display_orientation[4];
  CardboardHeadTracker_getPose(head_tracker_, display_timestamp, kLandscapeLeft,
                               display_position, display_orientation);
  std::array<float, 4> latest_orientation = {display_orientation[0],
                                             display_orientation[1],
                                             display_orientation[2],
                                             display_orientation[3]};
  if (has_recenter_offset_) {
    Quatf latest_q(latest_orientation[0], latest_orientation[1],
                   latest_orientation[2], latest_orientation[3]);
    Quatf corrected_latest = latest_q * inv_recenter;
    latest_orientation = {corrected_latest.x, corrected_latest.y,
                          corrected_latest.z, corrected_latest.w};
  }

  float u_offset = 0.f;
  float v_offset = 0.f;
  if (has_render_pose_) {
    CalculateUvOffset(last_rendered_orientation_, latest_orientation,
                      u_offset, v_offset);
  }

  left_eye_texture_description_.left_u = 0.f + u_offset;
  left_eye_texture_description_.right_u = 0.5f + u_offset;
  left_eye_texture_description_.top_v = 1.f + v_offset;
  left_eye_texture_description_.bottom_v = 0.f + v_offset;

  right_eye_texture_description_.left_u = 0.5f + u_offset;
  right_eye_texture_description_.right_u = 1.f + u_offset;
  right_eye_texture_description_.top_v = 1.f + v_offset;
  right_eye_texture_description_.bottom_v = 0.f + v_offset;

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
  last_recenter_update_nanos_ = 0;
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
  const bool use_curved_geometry = using_curved_geometry_ &&
      !screen_vertices_.empty() && !screen_texcoords_.empty();
  glEnableVertexAttribArray(position_attrib_);
  if (use_curved_geometry) {
    glVertexAttribPointer(position_attrib_, 3, GL_FLOAT, GL_FALSE, 0,
                           screen_vertices_.data());
  } else {
    glVertexAttribPointer(position_attrib_, 3, GL_FLOAT, GL_FALSE,
                           sizeof(GLfloat) * 3, kQuadVertices);
  }
  glEnableVertexAttribArray(texcoord_attrib_);
  if (use_curved_geometry) {
    glVertexAttribPointer(texcoord_attrib_, 2, GL_FLOAT, GL_FALSE, 0,
                          screen_texcoords_.data());
  } else {
    glVertexAttribPointer(texcoord_attrib_, 2, GL_FLOAT, GL_FALSE, 0,
                          kQuadTexCoords);
  }
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
    glUniformMatrix4fv(mvp_uniform_, 1, GL_FALSE, mvp_gl.data());
    if (use_curved_geometry) {
      for (int row = 0; row < curved_rows_; ++row) {
        glDrawArrays(GL_TRIANGLE_STRIP,
                     row * curved_vertices_per_strip_,
                     curved_vertices_per_strip_);
      }
    } else {
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
  }

  glDisableVertexAttribArray(position_attrib_);
  glDisableVertexAttribArray(texcoord_attrib_);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

  if (skybox_enabled_ && skybox_program_ != 0 && skybox_texture_ != 0) {
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glUseProgram(skybox_program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_);
    glUniform1i(skybox_sampler_uniform_, 0);
    glEnableVertexAttribArray(skybox_pos_attrib_);
    glVertexAttribPointer(skybox_pos_attrib_, 3, GL_FLOAT, GL_FALSE, 0,
                          kSkyboxVertices);

    for (int eye_index = 0; eye_index < kEyeCount; ++eye_index) {
      float eye_from_head_raw[16];
      CardboardLensDistortion_getEyeFromHeadMatrix(
          lens_distortion_, eyes[eye_index], eye_from_head_raw);
      Matrix4x4 eye_from_head = GetMatrixFromGlArray(eye_from_head_raw);

      float projection_raw[16];
      CardboardLensDistortion_getProjectionMatrix(
          lens_distortion_, eyes[eye_index], kZNear, kZFar, projection_raw);
      Matrix4x4 projection = GetMatrixFromGlArray(projection_raw);

      Matrix4x4 view_no_trans = eye_from_head * head_view;
      view_no_trans.m[0][3] = 0.0f;
      view_no_trans.m[1][3] = 0.0f;
      view_no_trans.m[2][3] = 0.0f;

      Matrix4x4 skybox_vp = projection * view_no_trans;
      std::array<float, 16> skybox_vp_gl = skybox_vp.ToGlArray();

      glViewport(viewport_x[eye_index], 0, viewport_width[eye_index],
                 screen_height_);
      glUniformMatrix4fv(skybox_vp_uniform_, 1, GL_FALSE, skybox_vp_gl.data());
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glDisableVertexAttribArray(skybox_pos_attrib_);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
  }

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

void VrMoonlightApp::SetCurrentFramePose(const std::array<float, 4>& orientation) {
  last_rendered_orientation_ = orientation;
  has_render_pose_ = true;
}

void VrMoonlightApp::RecenterView() {
  screen_yaw_radians_ = 0.0f;
  screen_pitch_radians_ = 0.0f;
  screen_rotation_radians_ = 0.0f;
  UpdateModelMatrix();

  if (head_tracker_ == nullptr) {
    return;
  }
  int64_t timestamp = GetBootTimeNano() + kPredictionTimeWithoutVsyncNanos;
  float position[3];
  float orientation[4];
  CardboardHeadTracker_getPose(head_tracker_, timestamp, kLandscapeLeft,
                               position, orientation);
  recenter_offset_ = {orientation[0], orientation[1], orientation[2],
                      orientation[3]};
  has_recenter_offset_ = true;
  if (!has_current_recenter_offset_) {
    current_recenter_offset_ = {0.f, 0.f, 0.f, 1.f};
    has_current_recenter_offset_ = true;
  }
  last_recenter_update_nanos_ = 0;
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
  } else if (screen_size_multiplier_ > 4.0f) {
    screen_size_multiplier_ = 4.0f;
  }
  UpdateModelMatrix();
  geometry_dirty_ = true;
}

void VrMoonlightApp::AdjustScreenDistance(float deltaMeters) {
  screen_distance_meters_ += deltaMeters;
  if (screen_distance_meters_ < kMinScreenDistanceMeters) {
    screen_distance_meters_ = kMinScreenDistanceMeters;
  } else if (screen_distance_meters_ > kMaxScreenDistanceMeters) {
    screen_distance_meters_ = kMaxScreenDistanceMeters;
  }
  UpdateModelMatrix();
}

void VrMoonlightApp::AdjustScreenSize(float deltaMultiplier) {
  screen_size_multiplier_ += deltaMultiplier;
  if (screen_size_multiplier_ < 0.25f) {
    screen_size_multiplier_ = 0.25f;
  } else if (screen_size_multiplier_ > 4.0f) {
    screen_size_multiplier_ = 4.0f;
  }
  UpdateModelMatrix();
  geometry_dirty_ = true;
}

void VrMoonlightApp::AdjustScreenPosition(float deltaX, float deltaY) {
  const float kYawSensitivity = 3.0f;
  const float kPitchSensitivity = 3.0f;

  screen_yaw_radians_ += deltaX * kYawSensitivity;
  screen_pitch_radians_ += deltaY * kPitchSensitivity;

  screen_pitch_radians_ = std::clamp(screen_pitch_radians_, -1.047f, 1.047f);
  screen_yaw_radians_ = std::clamp(screen_yaw_radians_, -3.14159f, 3.14159f);
  UpdateModelMatrix();
}

void VrMoonlightApp::AdjustScreenRotation(float deltaRadians) {
  screen_rotation_radians_ += deltaRadians;
  screen_rotation_radians_ = std::clamp(screen_rotation_radians_, -1.5708f, 1.5708f);
  UpdateModelMatrix();
}

void VrMoonlightApp::SetCurvatureMode(int mode) {
  if (mode < kCurvatureModeFlat || mode > kCurvatureModeGamingScreen) {
    return;
  }
  curvature_mode_ = mode;
  geometry_dirty_ = true;
}

void VrMoonlightApp::SetCurvatureAmount(float percent) {
  curvature_amount_percent_ = std::clamp(percent, 0.f, 100.f);
  geometry_dirty_ = true;
}

void VrMoonlightApp::SetHorizontalCurvature(float percent) {
  horizontal_curvature_percent_ = std::clamp(percent, 0.f, 100.f);
  geometry_dirty_ = true;
}

void VrMoonlightApp::SetVerticalCurvature(float percent) {
  vertical_curvature_percent_ = std::clamp(percent, 0.f, 100.f);
  geometry_dirty_ = true;
}

float VrMoonlightApp::GetCurvatureAmount() const {
  return curvature_amount_percent_;
}

float VrMoonlightApp::GetHorizontalCurvature() const {
  return horizontal_curvature_percent_;
}

float VrMoonlightApp::GetVerticalCurvature() const {
  return vertical_curvature_percent_;
}

float VrMoonlightApp::GetScreenSize() const {
  return screen_size_multiplier_;
}

void VrMoonlightApp::SetSkyboxEnabled(bool enabled) {
  skybox_enabled_ = enabled;
}

void VrMoonlightApp::SetSkyboxTexture(GLuint textureId) {
  skybox_texture_ = textureId;
}
 
void VrMoonlightApp::UpdateModelMatrix() {
  const float half_width = kScreenWidthMeters * 0.5f * screen_size_multiplier_;
  const float half_height = half_width / kScreenAspectRatio;
  const std::array<float, 3> scale = {half_width, -half_height, 1.0f};
  const std::array<float, 3> translation = {0.0f, 0.0f, -screen_distance_meters_};

  Matrix4x4 local_rot;
  for (int i = 0; i < 16; ++i) local_rot.m[i/4][i%4] = (i%5 == 0) ? 1.0f : 0.0f;
  float cos_r = std::cos(screen_rotation_radians_);
  float sin_r = std::sin(screen_rotation_radians_);
  local_rot.m[0][0] = cos_r;  local_rot.m[0][1] = -sin_r;
  local_rot.m[1][0] = sin_r;  local_rot.m[1][1] = cos_r;

  Matrix4x4 orbit_rot;
  for (int i = 0; i < 16; ++i) orbit_rot.m[i/4][i%4] = (i%5 == 0) ? 1.0f : 0.0f;
  float cos_y = std::cos(screen_yaw_radians_);
  float sin_y = std::sin(screen_yaw_radians_);
  float cos_x = std::cos(screen_pitch_radians_);
  float sin_x = std::sin(screen_pitch_radians_);

  orbit_rot.m[0][0] = cos_y;
  orbit_rot.m[0][1] = sin_y * sin_x;
  orbit_rot.m[0][2] = sin_y * cos_x;
  orbit_rot.m[1][0] = 0.0f;
  orbit_rot.m[1][1] = cos_x;
  orbit_rot.m[1][2] = -sin_x;
  orbit_rot.m[2][0] = -sin_y;
  orbit_rot.m[2][1] = cos_y * sin_x;
  orbit_rot.m[2][2] = cos_y * cos_x;

  model_matrix_ = orbit_rot * GetTranslationMatrix(translation) * local_rot * GetScaleMatrix(scale);
}

void VrMoonlightApp::UpdateScreenGeometry() {
  const float half_width = kScreenWidthMeters * 0.5f * screen_size_multiplier_;
  const float half_height = half_width / kScreenAspectRatio;

  float global_ratio = curvature_amount_percent_ / 100.0f;
  float h_ratio = global_ratio * (horizontal_curvature_percent_ / 100.0f);
  float v_ratio = global_ratio * (vertical_curvature_percent_ / 100.0f);

  if (curvature_mode_ == kCurvatureModeGamingScreen) {
    v_ratio = 0.0f;
  }

  if (curvature_mode_ == kCurvatureModeFlat || (h_ratio <= 0.001f && v_ratio <= 0.001f)) {
    using_curved_geometry_ = false;
    curved_rows_ = 0;
    curved_vertices_per_strip_ = 0;
    screen_vertices_.clear();
    screen_texcoords_.clear();
    return;
  }

  curved_rows_ = (curvature_mode_ == kCurvatureModeTvCinema) ? kCurvedGridRowsTv : kCurvedGridRowsGaming;
  curved_vertices_per_strip_ = (kCurvedGridCols + 1) * 2;
  const float rows_float = static_cast<float>(curved_rows_);

  const float max_theta = h_ratio * kMaxHorizontalCurvatureAngle;
  const float max_phi = v_ratio * kMaxVerticalCurvatureAngle;

  screen_vertices_.clear();
  screen_texcoords_.clear();
  screen_vertices_.reserve(curved_rows_ * curved_vertices_per_strip_ * 3);
  screen_texcoords_.reserve(curved_rows_ * curved_vertices_per_strip_ * 2);

  for (int row = 0; row < curved_rows_; ++row) {
    for (int col = 0; col <= kCurvedGridCols; ++col) {
      const float u = static_cast<float>(col) / static_cast<float>(kCurvedGridCols);
      float flat_x = (u - 0.5f) * 2.0f * half_width;

      for (int v_idx = 0; v_idx < 2; ++v_idx) {
        const float v_norm = (static_cast<float>(row + v_idx) / rows_float);
        float flat_y = (v_norm - 0.5f) * 2.0f * half_height;

        float phys_x = flat_x;
        float phys_y = flat_y;
        float phys_z = 0.0f;

        if (curvature_mode_ == kCurvatureModeGamingScreen) {
          if (max_theta > 0.001f) {
            float radius = (2.0f * half_width) / max_theta;
            float theta = flat_x / radius;
            phys_x = radius * std::sin(theta);
            phys_z = radius * (1.0f - std::cos(theta));
          }
        } else if (curvature_mode_ == kCurvatureModeTvCinema) {
          float r_h = (max_theta > 0.001f) ? ((2.0f * half_width) / max_theta) : 10000.0f;
          float r_v = (max_phi > 0.001f) ? ((2.0f * half_height) / max_phi) : 10000.0f;

          float theta = (max_theta > 0.001f) ? (flat_x / r_h) : 0.0f;
          float phi = (max_phi > 0.001f) ? (flat_y / r_v) : 0.0f;

          phys_x = r_h * std::sin(theta) * std::cos(phi);
          phys_y = r_v * std::sin(phi);

          float z_h = r_h * (1.0f - std::cos(theta));
          float z_v = r_v * (1.0f - std::cos(phi));
          phys_z = z_h * std::cos(phi) + z_v;
        }

        float final_x = phys_x / half_width;
        float final_y = phys_y / half_height;
        float final_z = phys_z;

        screen_vertices_.push_back(final_x);
        screen_vertices_.push_back(final_y);
        screen_vertices_.push_back(final_z);
        screen_texcoords_.push_back(u);
        screen_texcoords_.push_back(1.0f - v_norm);
      }
    }
  }
  using_curved_geometry_ = true;
}

}  // namespace moonlight_vr

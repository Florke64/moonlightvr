#ifndef MOONLIGHT_VR_UTIL_H_
#define MOONLIGHT_VR_UTIL_H_

#include <array>
#include <cstdint>

#include <GLES2/gl2.h>
#include <android/log.h>

#define VR_LOG_TAG "VrMoonlightApp"
#define VR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, VR_LOG_TAG, __VA_ARGS__)

namespace moonlight_vr {

struct Matrix4x4 {
  float m[4][4];
  Matrix4x4();
  Matrix4x4 operator*(const Matrix4x4& other) const;
  std::array<float, 4> operator*(const std::array<float, 4>& vec) const;
  std::array<float, 16> ToGlArray() const;
};

struct Quatf {
  float x;
  float y;
  float z;
  float w;
  Quatf(float x_, float y_, float z_, float w_);
  Matrix4x4 ToMatrix() const;
};

Matrix4x4 GetMatrixFromGlArray(const float* gl_array);
Matrix4x4 GetTranslationMatrix(const std::array<float, 3>& translation);
Matrix4x4 GetScaleMatrix(const std::array<float, 3>& scale);
Matrix4x4 GetViewMatrixFromPose(const std::array<float, 3>& translation,
                                const Quatf& orientation);
float AngleBetweenVectors(const std::array<float, 4>& vec1,
                          const std::array<float, 4>& vec2);
int64_t GetBootTimeNano();
GLuint LoadGLShader(GLenum type, const char* shader_source);
void CheckGlError(const char* file, int line, const char* label);
#define CHECK_GL_ERROR(label) \
  moonlight_vr::CheckGlError(__FILE__, __LINE__, label)

}  // namespace moonlight_vr

#endif  // MOONLIGHT_VR_UTIL_H_

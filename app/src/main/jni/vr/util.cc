#include <string>
#include "util.h"

#include <cmath>
#include <ctime>
#include <cstring>

namespace moonlight_vr {

namespace {

float VectorNorm(const std::array<float, 4>& vec) {
  return std::sqrt(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
}

float VectorDotProduct(const std::array<float, 4>& v1,
                       const std::array<float, 4>& v2) {
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

constexpr float kDegreesToUvX = 0.5f / 90.0f;
constexpr float kDegreesToUvY = 1.0f / 90.0f;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;

Quatf MultiplyQuaternions(const Quatf& lhs, const Quatf& rhs) {
  return Quatf(
      lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z);
}

Quatf InverseQuaternion(const Quatf& quat) {
  return Quatf(-quat.x, -quat.y, -quat.z, quat.w);
}

}  // namespace

Matrix4x4::Matrix4x4() {
  std::memset(m, 0, sizeof(m));
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const {
  Matrix4x4 result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0.0f;
      for (int k = 0; k < 4; ++k) {
        result.m[i][j] += this->m[i][k] * other.m[k][j];
      }
    }
  }
  return result;
}

std::array<float, 4> Matrix4x4::operator*(const std::array<float, 4>& vec) const {
  std::array<float, 4> result{};
  for (int i = 0; i < 4; ++i) {
    result[i] = 0;
    for (int k = 0; k < 4; ++k) {
      result[i] += this->m[i][k] * vec[k];
    }
  }
  return result;
}

std::array<float, 16> Matrix4x4::ToGlArray() const {
  std::array<float, 16> result;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      result[col * 4 + row] = m[row][col];
    }
  }
  return result;
}

Quatf::Quatf(float x_, float y_, float z_, float w_)
    : x(x_), y(y_), z(z_), w(w_) {}

Quatf Quatf::operator*(const Quatf& other) const {
  return Quatf(
      w * other.x + x * other.w + y * other.z - z * other.y,
      w * other.y - x * other.z + y * other.w + z * other.x,
      w * other.z + x * other.y - y * other.x + z * other.w,
      w * other.w - x * other.x - y * other.y - z * other.z);
}

Matrix4x4 Quatf::ToMatrix() const {
  Matrix4x4 out;
  const float xx = 2.0f * x * x;
  const float yy = 2.0f * y * y;
  const float zz = 2.0f * z * z;
  const float xy = 2.0f * x * y;
  const float xz = 2.0f * x * z;
  const float yz = 2.0f * y * z;
  const float xw = 2.0f * x * w;
  const float yw = 2.0f * y * w;
  const float zw = 2.0f * z * w;

  out.m[0][0] = 1 - yy - zz;
  out.m[0][1] = xy + zw;
  out.m[0][2] = xz - yw;
  out.m[0][3] = 0.0f;
  out.m[1][0] = xy - zw;
  out.m[1][1] = 1 - xx - zz;
  out.m[1][2] = yz + xw;
  out.m[1][3] = 0.0f;
  out.m[2][0] = xz + yw;
  out.m[2][1] = yz - xw;
  out.m[2][2] = 1 - xx - yy;
  out.m[2][3] = 0.0f;
  out.m[3][0] = 0.0f;
  out.m[3][1] = 0.0f;
  out.m[3][2] = 0.0f;
  out.m[3][3] = 1.0f;
  return out;
}

Matrix4x4 GetMatrixFromGlArray(const float* gl_array) {
  Matrix4x4 matrix;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      matrix.m[row][col] = gl_array[col * 4 + row];
    }
  }
  return matrix;
}

Matrix4x4 GetTranslationMatrix(const std::array<float, 3>& translation) {
  Matrix4x4 matrix;
  for (int i = 0; i < 4; ++i) {
    matrix.m[i][i] = 1.0f;
  }
  matrix.m[0][3] = translation[0];
  matrix.m[1][3] = translation[1];
  matrix.m[2][3] = translation[2];
  return matrix;
}

Matrix4x4 GetScaleMatrix(const std::array<float, 3>& scale) {
  Matrix4x4 matrix;
  for (int i = 0; i < 4; ++i) {
    matrix.m[i][i] = 1.0f;
  }
  matrix.m[0][0] = scale[0];
  matrix.m[1][1] = scale[1];
  matrix.m[2][2] = scale[2];
  return matrix;
}

Matrix4x4 GetViewMatrixFromPose(const std::array<float, 3>& translation,
                                const Quatf& orientation) {
  Matrix4x4 rotation = orientation.ToMatrix();
  Matrix4x4 view;
  for (int i = 0; i < 4; ++i) {
    view.m[i][i] = 1.0f;
  }

  // Transpose rotation component to invert orientation.
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      view.m[row][col] = rotation.m[col][row];
    }
  }

  // Compute translated camera position.
  view.m[0][3] = -(view.m[0][0] * translation[0] +
                  view.m[0][1] * translation[1] +
                  view.m[0][2] * translation[2]);
  view.m[1][3] = -(view.m[1][0] * translation[0] +
                  view.m[1][1] * translation[1] +
                  view.m[1][2] * translation[2]);
  view.m[2][3] = -(view.m[2][0] * translation[0] +
                  view.m[2][1] * translation[1] +
                  view.m[2][2] * translation[2]);

  return view;
}

float AngleBetweenVectors(const std::array<float, 4>& vec1,
                          const std::array<float, 4>& vec2) {
  float dot = VectorDotProduct(vec1, vec2);
  float norm = VectorNorm(vec1) * VectorNorm(vec2);
  if (norm == 0.0f) {
    return 0.0f;
  }
  float safe_value = dot / norm;
  if (safe_value < -1.0f) {
    safe_value = -1.0f;
  } else if (safe_value > 1.0f) {
    safe_value = 1.0f;
  }
  return std::acos(safe_value);
}

int64_t GetBootTimeNano() {
  timespec time_spec = {};
  clock_gettime(CLOCK_BOOTTIME, &time_spec);
  return static_cast<int64_t>(time_spec.tv_sec) * 1000000000LL + time_spec.tv_nsec;
}

GLuint LoadGLShader(GLenum type, const char* shader_source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &shader_source, nullptr);
  glCompileShader(shader);
  GLint result;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
      std::string info(length, '\0');
      glGetShaderInfoLog(shader, length, nullptr, info.data());
      VR_LOGE("Shader compilation failed: %s", info.c_str());
    }
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void CheckGlError(const char* file, int line, const char* label) {
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    VR_LOGE("GL error in %s:%d [%s]: 0x%04x", file, line, label, error);
  }
}

void CalculateUvOffset(const std::array<float, 4>& q_render,
                       const std::array<float, 4>& q_latest,
                       float& out_u_offset,
                       float& out_v_offset) {
  Quatf latest(q_latest[0], q_latest[1], q_latest[2], q_latest[3]);
  Quatf render(q_render[0], q_render[1], q_render[2], q_render[3]);
  Quatf delta = MultiplyQuaternions(latest, InverseQuaternion(render));

  float yaw_rad = std::atan2(
      2.0f * (delta.y * delta.w - delta.x * delta.z),
      1.0f - 2.0f * (delta.y * delta.y + delta.z * delta.z));
  float sin_pitch = 2.0f * (delta.x * delta.w + delta.y * delta.z);
  if (sin_pitch > 1.0f) {
    sin_pitch = 1.0f;
  } else if (sin_pitch < -1.0f) {
    sin_pitch = -1.0f;
  }
  float pitch_rad = std::asin(sin_pitch);

  float yaw_deg = yaw_rad * kRadiansToDegrees;
  float pitch_deg = pitch_rad * kRadiansToDegrees;

  out_u_offset = yaw_deg * kDegreesToUvX;
  out_v_offset = pitch_deg * kDegreesToUvY;
}

Quatf SlerpQuaternions(const Quatf& a, const Quatf& b, float t) {
  float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

  Quatf b_adj = b;
  if (dot < 0.0f) {
    b_adj.x = -b.x;
    b_adj.y = -b.y;
    b_adj.z = -b.z;
    b_adj.w = -b.w;
    dot = -dot;
  }

  if (dot > 0.9995f) {
    Quatf lerped(
        a.x + t * (b_adj.x - a.x),
        a.y + t * (b_adj.y - a.y),
        a.z + t * (b_adj.z - a.z),
        a.w + t * (b_adj.w - a.w));
    const float norm = std::sqrt(lerped.x * lerped.x + lerped.y * lerped.y +
                                 lerped.z * lerped.z + lerped.w * lerped.w);
    if (norm > 0.0f) {
      lerped.x /= norm;
      lerped.y /= norm;
      lerped.z /= norm;
      lerped.w /= norm;
    }
    return lerped;
  }

  float theta_0 = std::acos(dot);
  float theta = theta_0 * t;
  float sin_theta = std::sin(theta);
  float s0 = std::cos(theta) - dot * sin_theta / std::sin(theta_0);
  float s1 = sin_theta / std::sin(theta_0);

  return Quatf(
      s0 * a.x + s1 * b_adj.x,
      s0 * a.y + s1 * b_adj.y,
      s0 * a.z + s1 * b_adj.z,
      s0 * a.w + s1 * b_adj.w);
}

}  // namespace moonlight_vr

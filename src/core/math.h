// Small math library following Source conventions:
// Z up, X forward, Y left; angles in degrees as (pitch, yaw, roll); pitch > 0 looks down.
// Mat3x4 is row-major (same layout as matrix3x4_t in Source files).
// Mat4 is column-major, for the GPU.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ose {

constexpr float kPi = 3.14159265358979323846f;
inline float degToRad(float d) { return d * (kPi / 180.0f); }
inline float radToDeg(float r) { return r * (180.0f / kPi); }
template <class T> T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Vec2 {
    float x = 0, y = 0;
    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
};
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline float lengthSq(const Vec3& v) { return dot(v, v); }
inline Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 1e-12f ? v / l : Vec3{0, 0, 0};
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline Vec3 vmin(const Vec3& a, const Vec3& b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
inline Vec3 vmax(const Vec3& a, const Vec3& b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    Vec3 xyz() const { return {x, y, z}; }
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
};

// Euler angles in degrees, Source order.
struct QAngle {
    float pitch = 0, yaw = 0, roll = 0;
};

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
};

inline Quat operator*(const Quat& a, const Quat& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline float dot(const Quat& a, const Quat& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline Quat normalize(const Quat& q) {
    float l = std::sqrt(dot(q, q));
    if (l < 1e-12f) return {};
    return {q.x / l, q.y / l, q.z / l, q.w / l};
}
// Shortest-path normalised lerp; good enough for animation blending.
Quat nlerp(const Quat& a, const Quat& b, float t);
Quat slerp(const Quat& a, const Quat& b, float t);
// Radian Euler (x = roll, y = pitch, z = yaw) as stored in MDL bones.
Quat quatFromRadianEuler(const Vec3& e);
Quat quatFromAngles(const QAngle& a);
Vec3 rotate(const Quat& q, const Vec3& v);

// Row-major 3x4 affine transform: rows are (x, y, z) output components, column 3 = translation.
struct Mat3x4 {
    float m[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    static Mat3x4 identity() { return {}; }
    Vec3 column(int c) const { return {m[0][c], m[1][c], m[2][c]}; }
    void setColumn(int c, const Vec3& v) { m[0][c] = v.x; m[1][c] = v.y; m[2][c] = v.z; }
    Vec3 origin() const { return column(3); }
};
Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b);
Vec3 transformPoint(const Mat3x4& m, const Vec3& p);
Vec3 transformDir(const Mat3x4& m, const Vec3& d);
Mat3x4 invertAffine(const Mat3x4& m);  // assumes orthonormal rotation (no scale)
Mat3x4 invertGeneral(const Mat3x4& m);
Mat3x4 matrixFromQuat(const Quat& q, const Vec3& origin);
Quat quatFromMatrix(const Mat3x4& m);
Mat3x4 matrixFromAngles(const QAngle& a, const Vec3& origin = {});
QAngle anglesFromMatrix(const Mat3x4& m);
void angleVectors(const QAngle& a, Vec3* forward, Vec3* right, Vec3* up);
QAngle vectorAngles(const Vec3& forward);

// Column-major 4x4 for the GPU.
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
    static Mat4 identity() { return {}; }
};
Mat4 operator*(const Mat4& a, const Mat4& b);
Vec4 operator*(const Mat4& a, const Vec4& v);
Mat4 toMat4(const Mat3x4& m);
Mat4 perspective(float fovyRadians, float aspect, float zNear, float zFar);
// Off-centre perspective from the tangents of the four half angles (left and down are
// negative for a view looking between them), as head-mounted displays give them per eye.
Mat4 perspectiveTangents(float tanLeft, float tanRight, float tanUp, float tanDown, float zNear, float zFar);
Mat4 orthographic(float l, float r, float b, float t, float n, float f);
// View matrix for a Source camera (origin + angles): maps Source world space to GL eye space
// (-Z forward, +X right, +Y up).
Mat4 viewMatrix(const Vec3& origin, const QAngle& angles);
Mat4 inverse(const Mat4& m);

struct AABB {
    Vec3 mins{1e30f, 1e30f, 1e30f};
    Vec3 maxs{-1e30f, -1e30f, -1e30f};
    void add(const Vec3& p) { mins = vmin(mins, p); maxs = vmax(maxs, p); }
    bool valid() const { return mins.x <= maxs.x; }
    Vec3 center() const { return (mins + maxs) * 0.5f; }
    Vec3 extents() const { return (maxs - mins) * 0.5f; }
};

struct Plane {
    Vec3 normal;
    float dist = 0;
    float distanceTo(const Vec3& p) const { return dot(normal, p) - dist; }
};

// Six planes extracted from a view-projection matrix; normals point inwards.
struct Frustum {
    Plane planes[6];
    static Frustum fromMatrix(const Mat4& viewProj);
    bool cullBox(const Vec3& mins, const Vec3& maxs) const;  // true = completely outside
    bool cullSphere(const Vec3& c, float r) const;
};

float halfToFloat(uint16_t h);
uint16_t floatToHalf(float f);
// Shared-exponent RGB (GL_RGB9_E5): three 9-bit mantissas and a 5-bit exponent, 4 bytes.
uint32_t packRgb9e5(float r, float g, float b);
Vec3 unpackRgb9e5(uint32_t v);

}  // namespace ose

#include "core/math.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ose {

Quat nlerp(const Quat& a, const Quat& b, float t) {
    float d = dot(a, b);
    float s = d < 0 ? -1.0f : 1.0f;
    Quat r{a.x + (b.x * s - a.x) * t, a.y + (b.y * s - a.y) * t, a.z + (b.z * s - a.z) * t,
           a.w + (b.w * s - a.w) * t};
    return normalize(r);
}

Quat slerp(const Quat& a, const Quat& bIn, float t) {
    Quat b = bIn;
    float cosom = dot(a, b);
    if (cosom < 0) {
        cosom = -cosom;
        b = {-b.x, -b.y, -b.z, -b.w};
    }
    float sclp, sclq;
    if (1.0f - cosom > 1e-6f) {
        float omega = std::acos(cosom);
        float sinom = std::sin(omega);
        sclp = std::sin((1.0f - t) * omega) / sinom;
        sclq = std::sin(t * omega) / sinom;
    } else {
        sclp = 1.0f - t;
        sclq = t;
    }
    return normalize(Quat{sclp * a.x + sclq * b.x, sclp * a.y + sclq * b.y, sclp * a.z + sclq * b.z,
                          sclp * a.w + sclq * b.w});
}

// q = Rz(e.z) * Ry(e.y) * Rx(e.x)
Quat quatFromRadianEuler(const Vec3& e) {
    float sx = std::sin(e.x * 0.5f), cx = std::cos(e.x * 0.5f);
    float sy = std::sin(e.y * 0.5f), cy = std::cos(e.y * 0.5f);
    float sz = std::sin(e.z * 0.5f), cz = std::cos(e.z * 0.5f);
    return {cz * cy * sx - sz * sy * cx, cz * sy * cx + sz * cy * sx, sz * cy * cx - cz * sy * sx,
            cz * cy * cx + sz * sy * sx};
}

Quat quatFromAngles(const QAngle& a) {
    return quatFromRadianEuler({degToRad(a.roll), degToRad(a.pitch), degToRad(a.yaw)});
}

Vec3 rotate(const Quat& q, const Vec3& v) {
    Vec3 u{q.x, q.y, q.z};
    Vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b) {
    Mat3x4 r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
        }
        r.m[i][3] += a.m[i][3];
    }
    return r;
}

Vec3 transformPoint(const Mat3x4& m, const Vec3& p) {
    return {m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
            m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
            m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3]};
}

Vec3 transformDir(const Mat3x4& m, const Vec3& p) {
    return {m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z,
            m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z,
            m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z};
}

Mat3x4 invertAffine(const Mat3x4& m) {
    Mat3x4 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r.m[i][j] = m.m[j][i];
    Vec3 t = m.origin();
    for (int i = 0; i < 3; ++i) r.m[i][3] = -(r.m[i][0] * t.x + r.m[i][1] * t.y + r.m[i][2] * t.z);
    return r;
}

Mat3x4 invertGeneral(const Mat3x4& m) {
    const float a = m.m[0][0], b = m.m[0][1], c = m.m[0][2];
    const float d = m.m[1][0], e = m.m[1][1], f = m.m[1][2];
    const float g = m.m[2][0], h = m.m[2][1], i = m.m[2][2];
    float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    float det = a * A + b * B + c * C;
    if (std::fabs(det) < 1e-20f) return Mat3x4::identity();
    float inv = 1.0f / det;
    Mat3x4 r;
    r.m[0][0] = A * inv;
    r.m[0][1] = -(b * i - c * h) * inv;
    r.m[0][2] = (b * f - c * e) * inv;
    r.m[1][0] = B * inv;
    r.m[1][1] = (a * i - c * g) * inv;
    r.m[1][2] = -(a * f - c * d) * inv;
    r.m[2][0] = C * inv;
    r.m[2][1] = -(a * h - b * g) * inv;
    r.m[2][2] = (a * e - b * d) * inv;
    Vec3 t = m.origin();
    for (int k = 0; k < 3; ++k) r.m[k][3] = -(r.m[k][0] * t.x + r.m[k][1] * t.y + r.m[k][2] * t.z);
    return r;
}

Mat3x4 matrixFromQuat(const Quat& q, const Vec3& o) {
    Mat3x4 r;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    r.m[0][0] = 1 - 2 * (yy + zz); r.m[0][1] = 2 * (xy - wz);     r.m[0][2] = 2 * (xz + wy);
    r.m[1][0] = 2 * (xy + wz);     r.m[1][1] = 1 - 2 * (xx + zz); r.m[1][2] = 2 * (yz - wx);
    r.m[2][0] = 2 * (xz - wy);     r.m[2][1] = 2 * (yz + wx);     r.m[2][2] = 1 - 2 * (xx + yy);
    r.m[0][3] = o.x; r.m[1][3] = o.y; r.m[2][3] = o.z;
    return r;
}

Quat quatFromMatrix(const Mat3x4& m) {
    Quat q;
    float tr = m.m[0][0] + m.m[1][1] + m.m[2][2];
    if (tr > 0) {
        float s = std::sqrt(tr + 1.0f) * 2;
        q.w = 0.25f * s;
        q.x = (m.m[2][1] - m.m[1][2]) / s;
        q.y = (m.m[0][2] - m.m[2][0]) / s;
        q.z = (m.m[1][0] - m.m[0][1]) / s;
    } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2;
        q.w = (m.m[2][1] - m.m[1][2]) / s;
        q.x = 0.25f * s;
        q.y = (m.m[0][1] + m.m[1][0]) / s;
        q.z = (m.m[0][2] + m.m[2][0]) / s;
    } else if (m.m[1][1] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2;
        q.w = (m.m[0][2] - m.m[2][0]) / s;
        q.x = (m.m[0][1] + m.m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[1][2] + m.m[2][1]) / s;
    } else {
        float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2;
        q.w = (m.m[1][0] - m.m[0][1]) / s;
        q.x = (m.m[0][2] + m.m[2][0]) / s;
        q.y = (m.m[1][2] + m.m[2][1]) / s;
        q.z = 0.25f * s;
    }
    return normalize(q);
}

// Columns: forward, left, up. R = Rz(yaw) * Ry(pitch) * Rx(roll).
Mat3x4 matrixFromAngles(const QAngle& a, const Vec3& origin) {
    float sp = std::sin(degToRad(a.pitch)), cp = std::cos(degToRad(a.pitch));
    float sy = std::sin(degToRad(a.yaw)), cy = std::cos(degToRad(a.yaw));
    float sr = std::sin(degToRad(a.roll)), cr = std::cos(degToRad(a.roll));
    Mat3x4 m;
    m.m[0][0] = cp * cy; m.m[1][0] = cp * sy; m.m[2][0] = -sp;
    m.m[0][1] = sr * sp * cy - cr * sy; m.m[1][1] = sr * sp * sy + cr * cy; m.m[2][1] = sr * cp;
    m.m[0][2] = cr * sp * cy + sr * sy; m.m[1][2] = cr * sp * sy - sr * cy; m.m[2][2] = cr * cp;
    m.m[0][3] = origin.x; m.m[1][3] = origin.y; m.m[2][3] = origin.z;
    return m;
}

QAngle anglesFromMatrix(const Mat3x4& m) {
    Vec3 fwd = m.column(0), left = m.column(1), up = m.column(2);
    float xy = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y);
    QAngle a;
    if (xy > 0.001f) {
        a.yaw = radToDeg(std::atan2(fwd.y, fwd.x));
        a.pitch = radToDeg(std::atan2(-fwd.z, xy));
        a.roll = radToDeg(std::atan2(left.z, up.z));
    } else {
        a.yaw = radToDeg(std::atan2(-left.x, left.y));
        a.pitch = radToDeg(std::atan2(-fwd.z, xy));
        a.roll = 0;
    }
    return a;
}

void angleVectors(const QAngle& a, Vec3* forward, Vec3* right, Vec3* up) {
    Mat3x4 m = matrixFromAngles(a);
    if (forward) *forward = m.column(0);
    if (right) *right = -m.column(1);
    if (up) *up = m.column(2);
}

QAngle vectorAngles(const Vec3& f) {
    QAngle a;
    if (f.x == 0 && f.y == 0) {
        a.pitch = f.z > 0 ? 270.0f : 90.0f;
        return a;
    }
    a.yaw = radToDeg(std::atan2(f.y, f.x));
    if (a.yaw < 0) a.yaw += 360;
    a.pitch = radToDeg(std::atan2(-f.z, std::sqrt(f.x * f.x + f.y * f.y)));
    if (a.pitch < 0) a.pitch += 360;
    return a;
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.at(rr, k) * b.at(k, c);
            r.at(rr, c) = s;
        }
    return r;
}

Vec4 operator*(const Mat4& a, const Vec4& v) {
    Vec4 r;
    for (int i = 0; i < 4; ++i) r[i] = a.at(i, 0) * v.x + a.at(i, 1) * v.y + a.at(i, 2) * v.z + a.at(i, 3) * v.w;
    return r;
}

Mat4 toMat4(const Mat3x4& m) {
    Mat4 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j) r.at(i, j) = m.m[i][j];
    return r;
}

Mat4 perspective(float fovy, float aspect, float n, float f) {
    Mat4 r;
    std::memset(r.m, 0, sizeof(r.m));
    float t = 1.0f / std::tan(fovy * 0.5f);
    r.at(0, 0) = t / aspect;
    r.at(1, 1) = t;
    r.at(2, 2) = (f + n) / (n - f);
    r.at(2, 3) = 2 * f * n / (n - f);
    r.at(3, 2) = -1;
    return r;
}

Mat4 perspectiveTangents(float tl, float tr, float tu, float td, float n, float f) {
    Mat4 r;
    std::memset(r.m, 0, sizeof(r.m));
    r.at(0, 0) = 2 / (tr - tl);
    r.at(0, 2) = (tr + tl) / (tr - tl);
    r.at(1, 1) = 2 / (tu - td);
    r.at(1, 2) = (tu + td) / (tu - td);
    r.at(2, 2) = (f + n) / (n - f);
    r.at(2, 3) = 2 * f * n / (n - f);
    r.at(3, 2) = -1;
    return r;
}

Mat4 orthographic(float l, float r, float b, float t, float n, float f) {
    Mat4 m;
    m.at(0, 0) = 2 / (r - l);
    m.at(1, 1) = 2 / (t - b);
    m.at(2, 2) = -2 / (f - n);
    m.at(0, 3) = -(r + l) / (r - l);
    m.at(1, 3) = -(t + b) / (t - b);
    m.at(2, 3) = -(f + n) / (f - n);
    return m;
}

Mat4 viewMatrix(const Vec3& o, const QAngle& angles) {
    Vec3 f, r, u;
    angleVectors(angles, &f, &r, &u);
    Mat4 m;
    for (int c = 0; c < 3; ++c) {
        m.at(0, c) = r[c];
        m.at(1, c) = u[c];
        m.at(2, c) = -f[c];
    }
    m.at(0, 3) = -dot(r, o);
    m.at(1, 3) = -dot(u, o);
    m.at(2, 3) = dot(f, o);
    return m;
}

Mat4 inverse(const Mat4& mat) {
    const float* m = mat.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat4 r;
    if (std::fabs(det) < 1e-30f) return r;
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * det;
    return r;
}

Frustum Frustum::fromMatrix(const Mat4& vp) {
    Frustum f;
    auto row = [&](int r) { return Vec4{vp.at(r, 0), vp.at(r, 1), vp.at(r, 2), vp.at(r, 3)}; };
    Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    Vec4 eq[6] = {
        {r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w},
        {r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w},
        {r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w},
        {r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w},
        {r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w},
        {r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w},
    };
    for (int i = 0; i < 6; ++i) {
        Vec3 n{eq[i].x, eq[i].y, eq[i].z};
        float l = length(n);
        if (l < 1e-12f) l = 1;
        f.planes[i].normal = n / l;
        f.planes[i].dist = -eq[i].w / l;
    }
    return f;
}

bool Frustum::cullBox(const Vec3& mins, const Vec3& maxs) const {
    for (const auto& p : planes) {
        Vec3 v{p.normal.x >= 0 ? maxs.x : mins.x, p.normal.y >= 0 ? maxs.y : mins.y,
               p.normal.z >= 0 ? maxs.z : mins.z};
        if (p.distanceTo(v) < 0) return true;
    }
    return false;
}

bool Frustum::cullSphere(const Vec3& c, float r) const {
    for (const auto& p : planes)
        if (p.distanceTo(c) < -r) return true;
    return false;
}

float halfToFloat(uint16_t h) {
    uint32_t sign = (h >> 15) & 1, exp = (h >> 10) & 0x1f, mant = h & 0x3ff;
    uint32_t bits;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign << 31;
        } else {
            exp = 127 - 15 + 1;
            while (!(mant & 0x400)) { mant <<= 1; --exp; }
            mant &= 0x3ff;
            bits = (sign << 31) | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        bits = (sign << 31) | 0x7f800000 | (mant << 13);
    } else {
        bits = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
    }
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

uint16_t floatToHalf(float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    uint32_t sign = (x >> 16) & 0x8000;
    int32_t exp = int32_t((x >> 23) & 0xff) - 127 + 15;
    uint32_t mant = x & 0x7fffff;
    if (((x >> 23) & 0xff) == 0xff) return uint16_t(sign | 0x7c00 | (mant ? 0x200 : 0));
    if (exp <= 0) {
        if (exp < -10) return uint16_t(sign);
        mant |= 0x800000;
        uint32_t shift = uint32_t(14 - exp);
        uint32_t half = mant >> shift;
        if ((mant >> (shift - 1)) & 1) half += 1;  // round
        return uint16_t(sign | half);
    }
    if (exp >= 31) return uint16_t(sign | 0x7c00);
    uint32_t half = sign | (uint32_t(exp) << 10) | (mant >> 13);
    if (mant & 0x1000) half += 1;  // round to nearest
    return uint16_t(half);
}

}  // namespace ose

namespace ose {

// As in the EXT_texture_shared_exponent specification (N = 9 mantissa bits, bias 15).
uint32_t packRgb9e5(float r, float g, float b) {
    constexpr int N = 9, B = 15, Emax = 31;
    const float maxValue = float((1 << N) - 1) / float(1 << N) * std::ldexp(1.0f, Emax - B);
    auto clampc = [&](float c) { return std::isfinite(c) ? std::clamp(c, 0.0f, maxValue) : 0.0f; };
    r = clampc(r), g = clampc(g), b = clampc(b);
    float maxc = std::max({r, g, b});
    int e = std::max(-B - 1, int(std::floor(std::log2(std::max(maxc, 1e-30f))))) + 1 + B;
    float scale = std::ldexp(1.0f, e - B - N);
    if (int(std::floor(maxc / scale + 0.5f)) == (1 << N)) {
        ++e;
        scale *= 2;
    }
    auto m = [&](float c) { return uint32_t(std::min(int(std::floor(c / scale + 0.5f)), (1 << N) - 1)); };
    return m(r) | (m(g) << 9) | (m(b) << 18) | (uint32_t(e) << 27);
}

Vec3 unpackRgb9e5(uint32_t v) {
    float scale = std::ldexp(1.0f, int(v >> 27) - 15 - 9);
    return {float(v & 511) * scale, float((v >> 9) & 511) * scale, float((v >> 18) & 511) * scale};
}

}  // namespace ose

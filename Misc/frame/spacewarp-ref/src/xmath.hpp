// Minimal double-precision math for spacewarp-ref. Column-major 4x4 matrices
// (m[col * 4 + row]), right-handed OpenXR conventions (+Y up, -Z forward).
#pragma once
#include <cmath>

namespace xm
{
struct Vec3
{
    double x{0}, y{0}, z{0};
};
struct Vec4
{
    double x{0}, y{0}, z{0}, w{0};
};
struct Quat
{
    double x{0}, y{0}, z{0}, w{1};
};
struct Mat4
{
    double m[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }

inline Mat4 identity() { return Mat4{}; }

inline Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for(int c = 0; c < 4; ++c)
        for(int row = 0; row < 4; ++row)
        {
            double s = 0;
            for(int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

inline Vec4 mul(const Mat4& a, Vec4 v)
{
    const double in[4] = {v.x, v.y, v.z, v.w};
    double out[4];
    for(int row = 0; row < 4; ++row)
        out[row] = a.m[0 * 4 + row] * in[0] + a.m[1 * 4 + row] * in[1] + a.m[2 * 4 + row] * in[2] + a.m[3 * 4 + row] * in[3];
    return {out[0], out[1], out[2], out[3]};
}

inline Vec3 transformPoint(const Mat4& a, Vec3 p)
{
    const Vec4 r = mul(a, Vec4{p.x, p.y, p.z, 1});
    return {r.x, r.y, r.z};
}

inline Vec3 transformDir(const Mat4& a, Vec3 d)
{
    const Vec4 r = mul(a, Vec4{d.x, d.y, d.z, 0});
    return {r.x, r.y, r.z};
}

inline Mat4 rotation(Quat q)
{
    const double n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if(n > 0)
    {
        q.x /= n;
        q.y /= n;
        q.z /= n;
        q.w /= n;
    }
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    Mat4 r;
    r.m[0] = 1 - 2 * (yy + zz);
    r.m[1] = 2 * (xy + wz);
    r.m[2] = 2 * (xz - wy);
    r.m[4] = 2 * (xy - wz);
    r.m[5] = 1 - 2 * (xx + zz);
    r.m[6] = 2 * (yz + wx);
    r.m[8] = 2 * (xz + wy);
    r.m[9] = 2 * (yz - wx);
    r.m[10] = 1 - 2 * (xx + yy);
    return r;
}

inline Mat4 translation(Vec3 t)
{
    Mat4 r;
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

inline Mat4 scaling(Vec3 s)
{
    Mat4 r;
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

// Rotation about +Y by angle radians (right-handed: +angle turns -Z towards -X).
inline Mat4 rotationY(double angle)
{
    Mat4 r;
    const double c = std::cos(angle), s = std::sin(angle);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

inline Mat4 pose(Quat q, Vec3 p) { return mul(translation(p), rotation(q)); }

// Inverse of a rigid transform (rotation + translation).
inline Mat4 rigidInverse(const Mat4& a)
{
    Mat4 r;
    for(int c = 0; c < 3; ++c)
        for(int row = 0; row < 3; ++row) r.m[c * 4 + row] = a.m[row * 4 + c];
    const Vec3 t{a.m[12], a.m[13], a.m[14]};
    for(int row = 0; row < 3; ++row)
        r.m[12 + row] = -(r.m[0 * 4 + row] * t.x + r.m[1 * 4 + row] * t.y + r.m[2 * 4 + row] * t.z);
    r.m[3] = r.m[7] = r.m[11] = 0;
    r.m[15] = 1;
    return r;
}

// Rotation part only (translation removed).
inline Mat4 rotationPart(const Mat4& a)
{
    Mat4 r = a;
    r.m[12] = r.m[13] = r.m[14] = 0;
    r.m[3] = r.m[7] = r.m[11] = 0;
    r.m[15] = 1;
    return r;
}

// General 4x4 inverse (cofactor expansion). Returns false when singular.
inline bool inverse(const Mat4& a, Mat4& out)
{
    const double* m = a.m;
    double inv[16];
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
    const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if(det == 0 || !std::isfinite(det)) return false;
    for(int i = 0; i < 16; ++i) out.m[i] = inv[i] / det;
    return true;
}

// Vulkan clip conventions (as xr_linear.h GRAPHICS_VULKAN): NDC +Y down,
// depth 0..1. Standard depth: near -> 0, far -> 1. Reversed: near -> 1, far -> 0.
inline Mat4 projectionFov(double angleLeft, double angleRight, double angleUp, double angleDown,
    double nearZ, double farZ, bool reversed)
{
    const double tanLeft = std::tan(angleLeft), tanRight = std::tan(angleRight);
    const double tanDown = std::tan(angleDown), tanUp = std::tan(angleUp);
    const double tanWidth = tanRight - tanLeft;
    const double tanHeight = tanDown - tanUp; // Vulkan: flips Y
    Mat4 r;
    for(double& v : r.m) v = 0;
    r.m[0] = 2 / tanWidth;
    r.m[8] = (tanRight + tanLeft) / tanWidth;
    r.m[5] = 2 / tanHeight;
    r.m[9] = (tanUp + tanDown) / tanHeight;
    if(!reversed)
    {
        r.m[10] = -farZ / (farZ - nearZ);
        r.m[14] = -(farZ * nearZ) / (farZ - nearZ);
    }
    else
    {
        r.m[10] = nearZ / (farZ - nearZ);
        r.m[14] = (farZ * nearZ) / (farZ - nearZ);
    }
    r.m[11] = -1;
    return r;
}

inline void toFloat(const Mat4& a, float out[16])
{
    for(int i = 0; i < 16; ++i) out[i] = static_cast<float>(a.m[i]);
}

inline Vec3 ndc(const Mat4& viewProjModel, Vec3 p)
{
    const Vec4 c = mul(viewProjModel, Vec4{p.x, p.y, p.z, 1});
    return {c.x / c.w, c.y / c.w, c.z / c.w};
}
} // namespace xm

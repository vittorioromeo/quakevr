#include "Zancle/Base/Memcpy.hpp"
#pragma once

// vr_simd.hpp -- four float lanes, one interface over SSE2 (x86-64: Windows, Linux), NEON (AArch64: the Quest, the
// Steam Frame, ARM Linux) and plain C++ (anything else; and the reference the other two are tested against).
//
// The kernels that were SSE2 intrinsics (vr_ao.cpp's rays, vr_audio.cpp's anti-alias filter, vr_audio_io.cpp's
// interpolation taps) are written once against `simd::Native` and so build on ARM (QUEST_PLAN.md, C2). Every backend
// does the same IEEE float operation per lane in the same order, so the three give the same bits for the same inputs:
// no fused multiply-add (none is emitted from these), comparisons with SSE's NaN rules (cmpNotLt(a, b) is "not a < b",
// true for a NaN), and hsum's fixed order ((v0 + v2) + (v1 + v3)). vr_simd_test (vr_simd_test.cpp) checks the native
// backend against `simd::Scalar` lane by lane, bit for bit.
//
// A backend is a struct of static functions (not a namespace) so a kernel can be a template over it: the test runs
// the same kernel on Native and on Scalar. Define QVR_SIMD_SCALAR to make Scalar the native one (a portability check).
//
//   using V = simd::Native;
//   V::F4 a = V::set1(1.f), b = V::loadu(p);
//   V::storeu(out, V::add(V::mul(a, b), V::zero()));


#if !defined(QVR_SIMD_SCALAR) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define QVR_SIMD_SSE2 1
#include <emmintrin.h>
#elif !defined(QVR_SIMD_SCALAR) && (defined(__aarch64__) || defined(_M_ARM64)) // (AArch64 only: 32-bit ARM's NEON has no
                                                                                  // vdivq_f32 and flushes denormals: Scalar)
#define QVR_SIMD_NEON 1
#include <arm_neon.h>
#endif

namespace simd {

// The plain C++ backend: four floats in a struct, masks as their bit patterns (all ones or all zeros per lane).
struct Scalar
{
    struct F4
    {
        float v[4];
    };

    static constexpr const char* name = "scalar";

    static unsigned bits(float f)
    {
        unsigned u;
        ZA_MEMCPY(&u, &f, sizeof(u));
        return u;
    }
    static float fromBits(unsigned u)
    {
        float f;
        ZA_MEMCPY(&f, &u, sizeof(f));
        return f;
    }
    static float mask(bool on) { return fromBits(on ? 0xffffffffu : 0u); }

    static F4 zero() { return F4{{0.f, 0.f, 0.f, 0.f}}; }
    static F4 set1(float x) { return F4{{x, x, x, x}}; }
    static F4 load(const float* p) { return F4{{p[0], p[1], p[2], p[3]}}; } // (aligned to 16 bytes)
    static F4 loadu(const float* p) { return F4{{p[0], p[1], p[2], p[3]}}; }
    static void store(float* p, F4 a) { ZA_MEMCPY(p, a.v, sizeof(a.v)); }
    static void storeu(float* p, F4 a) { ZA_MEMCPY(p, a.v, sizeof(a.v)); }
    static F4 add(F4 a, F4 b) { return F4{{a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2], a.v[3] + b.v[3]}}; }
    static F4 sub(F4 a, F4 b) { return F4{{a.v[0] - b.v[0], a.v[1] - b.v[1], a.v[2] - b.v[2], a.v[3] - b.v[3]}}; }
    static F4 mul(F4 a, F4 b) { return F4{{a.v[0] * b.v[0], a.v[1] * b.v[1], a.v[2] * b.v[2], a.v[3] * b.v[3]}}; }
    static F4 div(F4 a, F4 b) { return F4{{a.v[0] / b.v[0], a.v[1] / b.v[1], a.v[2] / b.v[2], a.v[3] / b.v[3]}}; }
    static F4 bitAnd(F4 a, F4 b)
    {
        F4 r;
        for(int i = 0; i < 4; i++)
        {
            r.v[i] = fromBits(bits(a.v[i]) & bits(b.v[i]));
        }
        return r;
    }
    static F4 bitOr(F4 a, F4 b)
    {
        F4 r;
        for(int i = 0; i < 4; i++)
        {
            r.v[i] = fromBits(bits(a.v[i]) | bits(b.v[i]));
        }
        return r;
    }
    // ~a & b (SSE's andnot)
    static F4 bitAndNot(F4 a, F4 b)
    {
        F4 r;
        for(int i = 0; i < 4; i++)
        {
            r.v[i] = fromBits(~bits(a.v[i]) & bits(b.v[i]));
        }
        return r;
    }
    // The sign bit cleared (NaNs keep their payload).
    static F4 abs(F4 a)
    {
        F4 r;
        for(int i = 0; i < 4; i++)
        {
            r.v[i] = fromBits(bits(a.v[i]) & 0x7fffffffu);
        }
        return r;
    }
    static F4 cmpLt(F4 a, F4 b) { return F4{{mask(a.v[0] < b.v[0]), mask(a.v[1] < b.v[1]), mask(a.v[2] < b.v[2]), mask(a.v[3] < b.v[3])}}; }
    static F4 cmpGt(F4 a, F4 b) { return F4{{mask(a.v[0] > b.v[0]), mask(a.v[1] > b.v[1]), mask(a.v[2] > b.v[2]), mask(a.v[3] > b.v[3])}}; }
    static F4 cmpNotLt(F4 a, F4 b) { return F4{{mask(!(a.v[0] < b.v[0])), mask(!(a.v[1] < b.v[1])), mask(!(a.v[2] < b.v[2])), mask(!(a.v[3] < b.v[3]))}}; }
    static F4 cmpNotGt(F4 a, F4 b) { return F4{{mask(!(a.v[0] > b.v[0])), mask(!(a.v[1] > b.v[1])), mask(!(a.v[2] > b.v[2])), mask(!(a.v[3] > b.v[3]))}}; }
    // Eight 16-bit integers (unaligned) as two vectors of floats: s[0..3], s[4..7].
    static void widenS16(const short* s, F4& lo, F4& hi)
    {
        lo = F4{{static_cast<float>(s[0]), static_cast<float>(s[1]), static_cast<float>(s[2]), static_cast<float>(s[3])}};
        hi = F4{{static_cast<float>(s[4]), static_cast<float>(s[5]), static_cast<float>(s[6]), static_cast<float>(s[7])}};
    }
    // The four lanes' sum, in a fixed order: (v0 + v2) + (v1 + v3).
    static float hsum(F4 a) { return (a.v[0] + a.v[2]) + (a.v[1] + a.v[3]); }
};

#if defined(QVR_SIMD_SSE2)
struct Sse2
{
    using F4 = __m128;

    static constexpr const char* name = "sse2";

    static F4 zero() { return _mm_setzero_ps(); }
    static F4 set1(float x) { return _mm_set1_ps(x); }
    static F4 load(const float* p) { return _mm_load_ps(p); }
    static F4 loadu(const float* p) { return _mm_loadu_ps(p); }
    static void store(float* p, F4 a) { _mm_store_ps(p, a); }
    static void storeu(float* p, F4 a) { _mm_storeu_ps(p, a); }
    static F4 add(F4 a, F4 b) { return _mm_add_ps(a, b); }
    static F4 sub(F4 a, F4 b) { return _mm_sub_ps(a, b); }
    static F4 mul(F4 a, F4 b) { return _mm_mul_ps(a, b); }
    static F4 div(F4 a, F4 b) { return _mm_div_ps(a, b); }
    static F4 bitAnd(F4 a, F4 b) { return _mm_and_ps(a, b); }
    static F4 bitOr(F4 a, F4 b) { return _mm_or_ps(a, b); }
    static F4 bitAndNot(F4 a, F4 b) { return _mm_andnot_ps(a, b); }
    static F4 abs(F4 a) { return _mm_and_ps(a, _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff))); }
    static F4 cmpLt(F4 a, F4 b) { return _mm_cmplt_ps(a, b); }
    static F4 cmpGt(F4 a, F4 b) { return _mm_cmpgt_ps(a, b); }
    static F4 cmpNotLt(F4 a, F4 b) { return _mm_cmpnlt_ps(a, b); }
    static F4 cmpNotGt(F4 a, F4 b) { return _mm_cmpngt_ps(a, b); }
    static void widenS16(const short* s, F4& lo, F4& hi)
    {
        const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s));
        lo = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(v, v), 16));
        hi = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(v, v), 16));
    }
    static float hsum(F4 a)
    {
        const __m128 s = _mm_add_ps(a, _mm_movehl_ps(a, a)); // v0 + v2, v1 + v3
        return _mm_cvtss_f32(_mm_add_ss(s, _mm_shuffle_ps(s, s, 1)));
    }
};
using Native = Sse2;
#elif defined(QVR_SIMD_NEON)
struct Neon
{
    using F4 = float32x4_t;

    static constexpr const char* name = "neon";

    static F4 u2f(uint32x4_t u) { return vreinterpretq_f32_u32(u); }
    static uint32x4_t f2u(F4 f) { return vreinterpretq_u32_f32(f); }

    static F4 zero() { return vdupq_n_f32(0.f); }
    static F4 set1(float x) { return vdupq_n_f32(x); }
    static F4 load(const float* p) { return vld1q_f32(p); }
    static F4 loadu(const float* p) { return vld1q_f32(p); }
    static void store(float* p, F4 a) { vst1q_f32(p, a); }
    static void storeu(float* p, F4 a) { vst1q_f32(p, a); }
    static F4 add(F4 a, F4 b) { return vaddq_f32(a, b); }
    static F4 sub(F4 a, F4 b) { return vsubq_f32(a, b); }
    static F4 mul(F4 a, F4 b) { return vmulq_f32(a, b); }
    static F4 div(F4 a, F4 b) { return vdivq_f32(a, b); } // (AArch64: IEEE division, as divps)
    static F4 bitAnd(F4 a, F4 b) { return u2f(vandq_u32(f2u(a), f2u(b))); }
    static F4 bitOr(F4 a, F4 b) { return u2f(vorrq_u32(f2u(a), f2u(b))); }
    static F4 bitAndNot(F4 a, F4 b) { return u2f(vbicq_u32(f2u(b), f2u(a))); } // b & ~a
    static F4 abs(F4 a) { return u2f(vandq_u32(f2u(a), vdupq_n_u32(0x7fffffffu))); }
    static F4 cmpLt(F4 a, F4 b) { return u2f(vcltq_f32(a, b)); }
    static F4 cmpGt(F4 a, F4 b) { return u2f(vcgtq_f32(a, b)); }
    static F4 cmpNotLt(F4 a, F4 b) { return u2f(vmvnq_u32(vcltq_f32(a, b))); }
    static F4 cmpNotGt(F4 a, F4 b) { return u2f(vmvnq_u32(vcgtq_f32(a, b))); }
    static void widenS16(const short* s, F4& lo, F4& hi)
    {
        const int16x8_t v = vld1q_s16(s);
        lo = vcvtq_f32_s32(vmovl_s16(vget_low_s16(v)));
        hi = vcvtq_f32_s32(vmovl_s16(vget_high_s16(v)));
    }
    static float hsum(F4 a)
    {
        const float32x2_t s = vadd_f32(vget_low_f32(a), vget_high_f32(a)); // v0 + v2, v1 + v3
        return vget_lane_f32(s, 0) + vget_lane_f32(s, 1);
    }
};
using Native = Neon;
#else
using Native = Scalar;
#endif

} // namespace simd

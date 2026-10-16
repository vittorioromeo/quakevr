#pragma once

// The engine's small vector types, for its vector kernels (the AO bake's rays, vr_ao.cpp; the audio mixer's filters,
// vr_audio.cpp): four floats (f32x4), eight (f32x8), eight 16-bit integers (i16x8) and four 32-bit ones (i32x4), with
// exactly the operations those kernels use. Three implementations of one interface:
//
// - x86 (namespace x86): SSE2 (x64's baseline) for the four-lane types, AVX for f32x8: the same intrinsics the kernels
//   called before this header. f32x8 needs AVX: only in a function marked QVR_TARGET_AVX, called after a CPU check
//   (vr_ao.cpp's cpuHasAvx).
// - ARM64 (namespace neon): NEON; f32x8 is two float32x4.
// - Anything else, or QVR_SIMD_FORCE_SCALAR (namespace scalar): one float at a time, each operation the same IEEE one.
//
// Every operation gives the same bits on the three (vr_simd_selftest checks it: vr_simd.cpp), so the kernels' results
// do not depend on the platform: the arithmetic is IEEE single precision rounded to nearest (division too: SSE's divps,
// AArch64's fdiv and C's / are all correctly rounded), a comparison is a lane of all ones or all zeros, and "not less"
// and "not greater" are true when either side is a NaN (SSE's cmpnlt/cmpngt, unordered). Only a NaN result's payload
// may differ (which input's NaN comes out): the kernels never read one. A multiply and an add stay two roundings only if
// the compiler fuses nothing: vr.cmake builds the kernels' files with -ffp-contract=off (gcc fuses across intrinsics).
//
// qvr::simd::f32x4 and the rest are the native ones (x86, neon or scalar); qvr::simd::scalar's are always there.

#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IntTypes.hpp"

#if !defined(QVR_SIMD_FORCE_SCALAR) && (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))
#define QVR_SIMD_X86 1
#include <immintrin.h>
#elif !defined(QVR_SIMD_FORCE_SCALAR) && (defined(__aarch64__) || defined(_M_ARM64))
#define QVR_SIMD_NEON 1
#include <arm_neon.h>
#endif

// A function using f32x8 on x86: compiled for AVX (gcc and clang; MSVC needs nothing). Empty elsewhere.
#if defined(QVR_SIMD_X86) && (defined(__clang__) || defined(__GNUC__))
#define QVR_TARGET_AVX __attribute__((target("avx")))
#else
#define QVR_TARGET_AVX
#endif

// Each operation inlined even in Debug, as the intrinsics it wraps are.
#if defined(__clang__) || defined(__GNUC__)
#define QVR_SIMD_INLINE [[gnu::always_inline]] inline
#define QVR_SIMD_INLINE_AVX [[gnu::always_inline]] inline QVR_TARGET_AVX
#else
#define QVR_SIMD_INLINE __forceinline
#define QVR_SIMD_INLINE_AVX __forceinline
#endif

namespace qvr::simd
{

////////////////////////////////////////////////////////////
// Scalar: the reference, and the fallback.
namespace scalar
{

struct f32x4
{
    float l[4];

    QVR_SIMD_INLINE static f32x4 zero() { return {{0.f, 0.f, 0.f, 0.f}}; }
    QVR_SIMD_INLINE static f32x4 splat(float x) { return {{x, x, x, x}}; }
    QVR_SIMD_INLINE static f32x4 splatBits(za::U32 bits) { return splat(ZA_BIT_CAST(float, bits)); }
    QVR_SIMD_INLINE static f32x4 load(const float* p) { return {{p[0], p[1], p[2], p[3]}}; }
    QVR_SIMD_INLINE static f32x4 loadu(const float* p) { return load(p); }
    QVR_SIMD_INLINE void store(float* p) const
    {
        for(int i = 0; i < 4; i++)
        {
            p[i] = l[i];
        }
    }
    QVR_SIMD_INLINE void storeu(float* p) const { store(p); }
};

namespace detail
{
QVR_SIMD_INLINE float mask(bool b) { return ZA_BIT_CAST(float, b ? 0xffffffffu : 0u); }
QVR_SIMD_INLINE za::U32 bits(float x) { return ZA_BIT_CAST(za::U32, x); }
QVR_SIMD_INLINE float fromBits(za::U32 x) { return ZA_BIT_CAST(float, x); }
} // namespace detail

#define QVR_SIMD_SCALAR_LANEWISE(name, expr) \
    QVR_SIMD_INLINE f32x4 name(f32x4 a, f32x4 b) \
    { \
        f32x4 r; \
        for(int i = 0; i < 4; i++) \
        { \
            const float x = a.l[i], y = b.l[i]; \
            r.l[i] = (expr); \
        } \
        return r; \
    }
QVR_SIMD_SCALAR_LANEWISE(operator+, x + y)
QVR_SIMD_SCALAR_LANEWISE(operator-, x - y)
QVR_SIMD_SCALAR_LANEWISE(operator*, x* y)
QVR_SIMD_SCALAR_LANEWISE(operator/, x / y)
QVR_SIMD_SCALAR_LANEWISE(operator&, detail::fromBits(detail::bits(x) & detail::bits(y)))
QVR_SIMD_SCALAR_LANEWISE(cmpLess, detail::mask(x < y))
QVR_SIMD_SCALAR_LANEWISE(cmpGreater, detail::mask(x > y))
QVR_SIMD_SCALAR_LANEWISE(cmpNotLess, detail::mask(!(x < y)))
QVR_SIMD_SCALAR_LANEWISE(cmpNotGreater, detail::mask(!(x > y)))
#undef QVR_SIMD_SCALAR_LANEWISE

// Each lane a's where the mask's bit is set, else b's (bit by bit: or(and(m, a), andnot(m, b))).
QVR_SIMD_INLINE f32x4 select(f32x4 mask, f32x4 a, f32x4 b)
{
    f32x4 r;
    for(int i = 0; i < 4; i++)
    {
        const za::U32 m = detail::bits(mask.l[i]);
        r.l[i] = detail::fromBits((m & detail::bits(a.l[i])) | (~m & detail::bits(b.l[i])));
    }
    return r;
}

// The four lanes' sum, in SSE's order (movehl, add, shuffle, add_ss): (l0 + l2) + (l1 + l3).
QVR_SIMD_INLINE float sum(f32x4 a)
{
    const float even = a.l[0] + a.l[2], odd = a.l[1] + a.l[3];
    return even + odd;
}

// Eight floats as two f32x4 (also NEON's).
template <typename F4>
struct F32x8Of
{
    F4 lo, hi;

    QVR_SIMD_INLINE static F32x8Of zero() { return {F4::zero(), F4::zero()}; }
    QVR_SIMD_INLINE static F32x8Of splat(float x) { return {F4::splat(x), F4::splat(x)}; }
    QVR_SIMD_INLINE static F32x8Of splatBits(za::U32 bits) { return {F4::splatBits(bits), F4::splatBits(bits)}; }
    QVR_SIMD_INLINE static F32x8Of load(const float* p) { return {F4::load(p), F4::load(p + 4)}; }
    QVR_SIMD_INLINE void store(float* p) const
    {
        lo.store(p);
        hi.store(p + 4);
    }
    // clang-format off
    QVR_SIMD_INLINE friend F32x8Of operator+(F32x8Of a, F32x8Of b) { return {a.lo + b.lo, a.hi + b.hi}; }
    QVR_SIMD_INLINE friend F32x8Of operator-(F32x8Of a, F32x8Of b) { return {a.lo - b.lo, a.hi - b.hi}; }
    QVR_SIMD_INLINE friend F32x8Of operator*(F32x8Of a, F32x8Of b) { return {a.lo * b.lo, a.hi * b.hi}; }
    QVR_SIMD_INLINE friend F32x8Of operator/(F32x8Of a, F32x8Of b) { return {a.lo / b.lo, a.hi / b.hi}; }
    QVR_SIMD_INLINE friend F32x8Of operator&(F32x8Of a, F32x8Of b) { return {a.lo & b.lo, a.hi & b.hi}; }
    QVR_SIMD_INLINE friend F32x8Of cmpLess(F32x8Of a, F32x8Of b) { return {cmpLess(a.lo, b.lo), cmpLess(a.hi, b.hi)}; }
    QVR_SIMD_INLINE friend F32x8Of cmpGreater(F32x8Of a, F32x8Of b) { return {cmpGreater(a.lo, b.lo), cmpGreater(a.hi, b.hi)}; }
    QVR_SIMD_INLINE friend F32x8Of cmpNotLess(F32x8Of a, F32x8Of b) { return {cmpNotLess(a.lo, b.lo), cmpNotLess(a.hi, b.hi)}; }
    QVR_SIMD_INLINE friend F32x8Of cmpNotGreater(F32x8Of a, F32x8Of b) { return {cmpNotGreater(a.lo, b.lo), cmpNotGreater(a.hi, b.hi)}; }
    QVR_SIMD_INLINE friend F32x8Of select(F32x8Of m, F32x8Of a, F32x8Of b) { return {select(m.lo, a.lo, b.lo), select(m.hi, a.hi, b.hi)}; }
    // clang-format on
};

using f32x8 = F32x8Of<f32x4>;

struct i32x4
{
    za::I32 l[4];

    QVR_SIMD_INLINE static i32x4 loadu(const za::I32* p) { return {{p[0], p[1], p[2], p[3]}}; }
    QVR_SIMD_INLINE void storeu(za::I32* p) const
    {
        for(int i = 0; i < 4; i++)
        {
            p[i] = l[i];
        }
    }
};

// Each lane converted to the nearest float (ties to even: above 2^24 not every integer is a float).
QVR_SIMD_INLINE f32x4 toF32(i32x4 a)
{
    f32x4 r;
    for(int i = 0; i < 4; i++)
    {
        r.l[i] = static_cast<float>(a.l[i]);
    }
    return r;
}

struct i16x8
{
    za::I16 l[8];

    QVR_SIMD_INLINE static i16x8 loadu(const za::I16* p)
    {
        i16x8 r;
        for(int i = 0; i < 8; i++)
        {
            r.l[i] = p[i];
        }
        return r;
    }
};

// Lanes 0-3 (widenLow) or 4-7 (widenHigh), sign-extended to 32 bits.
QVR_SIMD_INLINE i32x4 widenLow(i16x8 a) { return {{a.l[0], a.l[1], a.l[2], a.l[3]}}; }
QVR_SIMD_INLINE i32x4 widenHigh(i16x8 a) { return {{a.l[4], a.l[5], a.l[6], a.l[7]}}; }

} // namespace scalar

#if defined(QVR_SIMD_X86)
////////////////////////////////////////////////////////////
// x86: SSE2 and AVX.
namespace x86
{

struct f32x4
{
    __m128 v;

    QVR_SIMD_INLINE static f32x4 zero() { return {_mm_setzero_ps()}; }
    QVR_SIMD_INLINE static f32x4 splat(float x) { return {_mm_set1_ps(x)}; }
    QVR_SIMD_INLINE static f32x4 splatBits(za::U32 bits) { return {_mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(bits)))}; }
    QVR_SIMD_INLINE static f32x4 load(const float* p) { return {_mm_load_ps(p)}; } // (16-byte aligned)
    QVR_SIMD_INLINE static f32x4 loadu(const float* p) { return {_mm_loadu_ps(p)}; }
    QVR_SIMD_INLINE void store(float* p) const { _mm_store_ps(p, v); } // (16-byte aligned)
    QVR_SIMD_INLINE void storeu(float* p) const { _mm_storeu_ps(p, v); }
};

// clang-format off
QVR_SIMD_INLINE f32x4 operator+(f32x4 a, f32x4 b) { return {_mm_add_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator-(f32x4 a, f32x4 b) { return {_mm_sub_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator*(f32x4 a, f32x4 b) { return {_mm_mul_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator/(f32x4 a, f32x4 b) { return {_mm_div_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator&(f32x4 a, f32x4 b) { return {_mm_and_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 cmpLess(f32x4 a, f32x4 b) { return {_mm_cmplt_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 cmpGreater(f32x4 a, f32x4 b) { return {_mm_cmpgt_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 cmpNotLess(f32x4 a, f32x4 b) { return {_mm_cmpnlt_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 cmpNotGreater(f32x4 a, f32x4 b) { return {_mm_cmpngt_ps(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 select(f32x4 m, f32x4 a, f32x4 b) { return {_mm_or_ps(_mm_and_ps(m.v, a.v), _mm_andnot_ps(m.v, b.v))}; }
// clang-format on

QVR_SIMD_INLINE float sum(f32x4 a)
{
    __m128 s = _mm_add_ps(a.v, _mm_movehl_ps(a.v, a.v));
    s = _mm_add_ss(s, _mm_shuffle_ps(s, s, 1));
    return _mm_cvtss_f32(s);
}

// AVX: only in QVR_TARGET_AVX functions.
struct f32x8
{
    __m256 v;

    QVR_SIMD_INLINE_AVX static f32x8 zero() { return {_mm256_setzero_ps()}; }
    QVR_SIMD_INLINE_AVX static f32x8 splat(float x) { return {_mm256_set1_ps(x)}; }
    QVR_SIMD_INLINE_AVX static f32x8 splatBits(za::U32 bits) { return {_mm256_castsi256_ps(_mm256_set1_epi32(static_cast<int>(bits)))}; }
    QVR_SIMD_INLINE_AVX static f32x8 load(const float* p) { return {_mm256_load_ps(p)}; } // (32-byte aligned)
    QVR_SIMD_INLINE_AVX void store(float* p) const { _mm256_store_ps(p, v); } // (32-byte aligned)
};

// clang-format off
QVR_SIMD_INLINE_AVX f32x8 operator+(f32x8 a, f32x8 b) { return {_mm256_add_ps(a.v, b.v)}; }
QVR_SIMD_INLINE_AVX f32x8 operator-(f32x8 a, f32x8 b) { return {_mm256_sub_ps(a.v, b.v)}; }
QVR_SIMD_INLINE_AVX f32x8 operator*(f32x8 a, f32x8 b) { return {_mm256_mul_ps(a.v, b.v)}; }
QVR_SIMD_INLINE_AVX f32x8 operator/(f32x8 a, f32x8 b) { return {_mm256_div_ps(a.v, b.v)}; }
QVR_SIMD_INLINE_AVX f32x8 operator&(f32x8 a, f32x8 b) { return {_mm256_and_ps(a.v, b.v)}; }
QVR_SIMD_INLINE_AVX f32x8 cmpLess(f32x8 a, f32x8 b) { return {_mm256_cmp_ps(a.v, b.v, _CMP_LT_OQ)}; }
QVR_SIMD_INLINE_AVX f32x8 cmpGreater(f32x8 a, f32x8 b) { return {_mm256_cmp_ps(a.v, b.v, _CMP_GT_OQ)}; }
QVR_SIMD_INLINE_AVX f32x8 cmpNotLess(f32x8 a, f32x8 b) { return {_mm256_cmp_ps(a.v, b.v, _CMP_NLT_UQ)}; }
QVR_SIMD_INLINE_AVX f32x8 cmpNotGreater(f32x8 a, f32x8 b) { return {_mm256_cmp_ps(a.v, b.v, _CMP_NGT_UQ)}; }
// (blendv reads each mask lane's top bit: the same as select's bits for a comparison's all-ones or all-zeros lanes)
QVR_SIMD_INLINE_AVX f32x8 select(f32x8 m, f32x8 a, f32x8 b) { return {_mm256_blendv_ps(b.v, a.v, m.v)}; }
// clang-format on

struct i32x4
{
    __m128i v;

    QVR_SIMD_INLINE static i32x4 loadu(const za::I32* p) { return {_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))}; }
    QVR_SIMD_INLINE void storeu(za::I32* p) const { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
};

QVR_SIMD_INLINE f32x4 toF32(i32x4 a) { return {_mm_cvtepi32_ps(a.v)}; }

struct i16x8
{
    __m128i v;

    QVR_SIMD_INLINE static i16x8 loadu(const za::I16* p) { return {_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))}; }
};

// (each 16-bit lane twice, the pair shifted down 16 arithmetically: the sign extended)
QVR_SIMD_INLINE i32x4 widenLow(i16x8 a) { return {_mm_srai_epi32(_mm_unpacklo_epi16(a.v, a.v), 16)}; }
QVR_SIMD_INLINE i32x4 widenHigh(i16x8 a) { return {_mm_srai_epi32(_mm_unpackhi_epi16(a.v, a.v), 16)}; }

} // namespace x86

namespace native = x86;
inline constexpr const char* nativeName = "SSE2/AVX";
inline constexpr bool f32x8NeedsAvx = true;

#elif defined(QVR_SIMD_NEON)
////////////////////////////////////////////////////////////
// ARM64: NEON.
namespace neon
{

struct f32x4
{
    float32x4_t v;

    QVR_SIMD_INLINE static f32x4 zero() { return {vdupq_n_f32(0.f)}; }
    QVR_SIMD_INLINE static f32x4 splat(float x) { return {vdupq_n_f32(x)}; }
    QVR_SIMD_INLINE static f32x4 splatBits(za::U32 bits) { return {vreinterpretq_f32_u32(vdupq_n_u32(bits))}; }
    QVR_SIMD_INLINE static f32x4 load(const float* p) { return {vld1q_f32(p)}; }
    QVR_SIMD_INLINE static f32x4 loadu(const float* p) { return {vld1q_f32(p)}; }
    QVR_SIMD_INLINE void store(float* p) const { vst1q_f32(p, v); }
    QVR_SIMD_INLINE void storeu(float* p) const { vst1q_f32(p, v); }
};

namespace detail
{
QVR_SIMD_INLINE uint32x4_t u(f32x4 a) { return vreinterpretq_u32_f32(a.v); }
QVR_SIMD_INLINE f32x4 f(uint32x4_t a) { return {vreinterpretq_f32_u32(a)}; }
} // namespace detail

// clang-format off
QVR_SIMD_INLINE f32x4 operator+(f32x4 a, f32x4 b) { return {vaddq_f32(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator-(f32x4 a, f32x4 b) { return {vsubq_f32(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator*(f32x4 a, f32x4 b) { return {vmulq_f32(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator/(f32x4 a, f32x4 b) { return {vdivq_f32(a.v, b.v)}; }
QVR_SIMD_INLINE f32x4 operator&(f32x4 a, f32x4 b) { return detail::f(vandq_u32(detail::u(a), detail::u(b))); }
QVR_SIMD_INLINE f32x4 cmpLess(f32x4 a, f32x4 b) { return detail::f(vcltq_f32(a.v, b.v)); }
QVR_SIMD_INLINE f32x4 cmpGreater(f32x4 a, f32x4 b) { return detail::f(vcgtq_f32(a.v, b.v)); }
// (NEON's ordered "less" negated: true for a NaN, as SSE's unordered cmpnlt)
QVR_SIMD_INLINE f32x4 cmpNotLess(f32x4 a, f32x4 b) { return detail::f(vmvnq_u32(vcltq_f32(a.v, b.v))); }
QVR_SIMD_INLINE f32x4 cmpNotGreater(f32x4 a, f32x4 b) { return detail::f(vmvnq_u32(vcgtq_f32(a.v, b.v))); }
QVR_SIMD_INLINE f32x4 select(f32x4 m, f32x4 a, f32x4 b) { return {vbslq_f32(detail::u(m), a.v, b.v)}; }
// clang-format on

QVR_SIMD_INLINE float sum(f32x4 a)
{
    const float32x2_t s = vadd_f32(vget_low_f32(a.v), vget_high_f32(a.v)); // (l0 + l2, l1 + l3)
    return vget_lane_f32(s, 0) + vget_lane_f32(s, 1);
}

using f32x8 = scalar::F32x8Of<f32x4>;

struct i32x4
{
    int32x4_t v;

    QVR_SIMD_INLINE static i32x4 loadu(const za::I32* p) { return {vld1q_s32(p)}; }
    QVR_SIMD_INLINE void storeu(za::I32* p) const { vst1q_s32(p, v); }
};

QVR_SIMD_INLINE f32x4 toF32(i32x4 a) { return {vcvtq_f32_s32(a.v)}; } // (to nearest, ties to even: as cvtdq2ps)

struct i16x8
{
    int16x8_t v;

    QVR_SIMD_INLINE static i16x8 loadu(const za::I16* p) { return {vld1q_s16(p)}; }
};

QVR_SIMD_INLINE i32x4 widenLow(i16x8 a) { return {vmovl_s16(vget_low_s16(a.v))}; }
QVR_SIMD_INLINE i32x4 widenHigh(i16x8 a) { return {vmovl_s16(vget_high_s16(a.v))}; }

} // namespace neon

namespace native = neon;
inline constexpr const char* nativeName = "NEON";
inline constexpr bool f32x8NeedsAvx = false;

#else

namespace native = scalar;
inline constexpr const char* nativeName = "scalar";
inline constexpr bool f32x8NeedsAvx = false;

#endif

using native::f32x4;
using native::f32x8;
using native::i16x8;
using native::i32x4;

// vr_simd_selftest (vr_simd.cpp): every operation through the native types and the scalar ones, on edge inputs (zeros,
// denormals, infinities, NaNs, the integer limits) and random bits; returns how many results differ (0: the same bits;
// a NaN result counts as the same as any NaN). f32x8 only with `withF32x8` (x86: the CPU has AVX). One line to `print`.
int selfTest(bool withF32x8, void (*print)(const char* line));

} // namespace qvr::simd

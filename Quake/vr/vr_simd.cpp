// vr_simd_selftest: vr_simd.hpp's native types (SSE2/AVX on x86, NEON on ARM64) against its scalar ones, operation by
// operation, on edge inputs and random bits. The kernels' results are the same on every platform only if these agree.
// (Engine-free: the command is registered in vr_ao.cpp; this file compiles alone for any target.)

#include "vr_simd.hpp"

#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IntTypes.hpp"

#include <stdio.h>

namespace qvr::simd
{
namespace
{

// The edge values, as bits: each with every other (and itself) in the float tests.
constexpr za::U32 edgeBits[] = {
    0x00000000, 0x80000000, // +-0
    0x3f800000, 0xbf800000, // +-1
    0x00000001, 0x80000001, // the smallest denormals
    0x007fffff, 0x807fffff, // the largest denormals
    0x00800000, 0x80800000, // the smallest normals
    0x7f7fffff, 0xff7fffff, // the largest finite
    0x7f800000, 0xff800000, // +-infinity
    0x7fc00000, 0xffc00000, // quiet NaNs
    0x7f800001, 0x7fc12345, // a signalling NaN, a NaN with a payload
    0x3089705f, // 1e-9 (the AO bake's det threshold)
    0x3f000000, 0x3eaaaaab, // 0.5, 1/3
    0x34000000, 0x3f800001, // 2^-23, 1 + 2^-23
    0x60ad78ec, 0xc0200000, // 1e20, -2.5
};
constexpr int edgeCount = static_cast<int>(sizeof(edgeBits) / sizeof(edgeBits[0]));

// The integer edges for the conversion: the limits, odd values past 2^24 (rounded: ties to even), their negatives.
constexpr za::I32 intEdges[] = {0, 1, -1, 32767, -32768, 16777216, 16777217, 16777219, -16777217, -16777219, 33554435,
    2147483647, -2147483647 - 1, 2147483583, 2147483584, -2147483583, 123456789, -987654321};
constexpr int intEdgeCount = static_cast<int>(sizeof(intEdges) / sizeof(intEdges[0]));

constexpr int randomPairs = 1 << 16;

// A value through a volatile, so the compiler cannot fold the tests at compile time (the hardware is what is tested).
template <typename T>
T opaque(T x)
{
    volatile T v = x;
    return v;
}

// A 64-bit LCG (Knuth's MMIX constants): the same bits on every run and platform.
struct Lcg
{
    za::U64 state = 0x853c49e6748fea9bull;

    za::U32 next()
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<za::U32>(state >> 32);
    }
};

bool isNan(za::U32 bits) { return (bits & 0x7fffffffu) > 0x7f800000u; }

// The same result: the same bits, or (`nanAny`) both NaNs, whatever their payloads.
bool same(float a, float b, bool nanAny)
{
    const za::U32 x = ZA_BIT_CAST(za::U32, a), y = ZA_BIT_CAST(za::U32, b);
    return x == y || (nanAny && isNan(x) && isNan(y));
}

struct Tally
{
    int checks = 0;
    int differ = 0;
    za::U32 firstA = 0, firstB = 0; // (the first differing case's inputs)
    int firstOp = -1;

    void check(bool ok, int op, za::U32 a, za::U32 b)
    {
        checks++;
        if(!ok && differ++ == 0)
        {
            firstOp = op;
            firstA = a;
            firstB = b;
        }
    }
};

// The f32x4/f32x8 operations on lanes a and b, one row of results per operation. Rows 0-3, 10 and 13 are arithmetic (a
// NaN's payload may differ), the rest bits. Row 13: f32x4's sum() of a (lane 0) and of a + b (lane 1), through loadu.
// (A macro, not a template: clang inlines f32x8's AVX operations only into a function compiled for AVX.)
constexpr int floatOps = 14;
constexpr bool opNanAny[floatOps] = {
    true, true, true, true, false, false, false, false, false, false, true, false, false, true};

#define QVR_SIMD_FLOAT_ROWS(V) \
    const V x = V::load(a), y = V::load(b); \
    (x + y).store(out[0]); \
    (x - y).store(out[1]); \
    (x * y).store(out[2]); \
    (x / y).store(out[3]); \
    (x & y).store(out[4]); \
    cmpLess(x, y).store(out[5]); \
    cmpGreater(x, y).store(out[6]); \
    cmpNotLess(x, y).store(out[7]); \
    cmpNotGreater(x, y).store(out[8]); \
    select(cmpLess(x, y), x, y).store(out[9]); \
    V::splat(a[0]).store(out[10]); \
    (V::splatBits(ZA_BIT_CAST(za::U32, b[0])) & V::splatBits(0x7fffffffu)).store(out[11]); \
    V::zero().store(out[12]); \
    V::zero().store(out[13])

template <typename V>
void f32x4Rows(const float* a, const float* b, float (*out)[4])
{
    QVR_SIMD_FLOAT_ROWS(V);
    out[13][0] = sum(V::loadu(a));
    out[13][1] = sum(V::loadu(a) + V::loadu(b));
}

template <typename V>
QVR_TARGET_AVX void f32x8Rows(const float* a, const float* b, float (*out)[8])
{
    QVR_SIMD_FLOAT_ROWS(V);
}

#undef QVR_SIMD_FLOAT_ROWS

// Lanes `count` wide through both paths, the results compared.
template <int N>
void compareRows(Tally& t, const float* a, const float* b, const float (*native)[N], const float (*reference)[N])
{
    for(int op = 0; op < floatOps; op++)
    {
        for(int i = 0; i < N; i++)
        {
            t.check(same(native[op][i], reference[op][i], opNanAny[op]), op, ZA_BIT_CAST(za::U32, a[i]),
                ZA_BIT_CAST(za::U32, b[i]));
        }
    }
}

// Every edge pair, then random pairs, `N` lanes at a time.
template <int N, typename Run>
void floatCases(Tally& t, Run run)
{
    Lcg lcg;
    const int total = edgeCount * edgeCount + randomPairs;
    for(int base = 0; base < total; base += N)
    {
        alignas(32) float a[N], b[N];
        for(int i = 0; i < N; i++)
        {
            const int k = (base + i) % total;
            za::U32 x, y;
            if(k < edgeCount * edgeCount)
            {
                x = opaque(edgeBits[k / edgeCount]);
                y = opaque(edgeBits[k % edgeCount]);
            }
            else
            {
                x = lcg.next();
                y = (k & 3) == 0 ? x ^ (lcg.next() & 0x80000003u) : lcg.next(); // (some pairs near or equal)
            }
            a[i] = ZA_BIT_CAST(float, x);
            b[i] = ZA_BIT_CAST(float, y);
        }
        alignas(32) float native[floatOps][N], reference[floatOps][N];
        run(a, b, native, reference);
        compareRows<N>(t, a, b, native, reference);
    }
}

void intCases(Tally& wide, Tally& conv)
{
    // Every 16-bit value, eight at a time: widened (sign-extended) and converted.
    for(int base = -32768; base < 32768; base += 8)
    {
        za::I16 v[8];
        for(int i = 0; i < 8; i++)
        {
            v[i] = static_cast<za::I16>(opaque(base + i));
        }
        za::I32 n[2][4], r[2][4];
        float nf[2][4], rf[2][4];
        const native::i16x8 nv = native::i16x8::loadu(v);
        const scalar::i16x8 rv = scalar::i16x8::loadu(v);
        widenLow(nv).storeu(n[0]);
        widenHigh(nv).storeu(n[1]);
        widenLow(rv).storeu(r[0]);
        widenHigh(rv).storeu(r[1]);
        toF32(widenLow(nv)).storeu(nf[0]);
        toF32(widenHigh(nv)).storeu(nf[1]);
        toF32(widenLow(rv)).storeu(rf[0]);
        toF32(widenHigh(rv)).storeu(rf[1]);
        for(int h = 0; h < 2; h++)
        {
            for(int i = 0; i < 4; i++)
            {
                const za::U32 in = static_cast<za::U32>(v[h * 4 + i]);
                wide.check(n[h][i] == r[h][i] && n[h][i] == v[h * 4 + i], 0, in, 0);
                wide.check(same(nf[h][i], rf[h][i], false), 1, in, 0);
            }
        }
    }
    // 32-bit integers to floats: the edges, then random bits.
    Lcg lcg;
    for(int base = 0; base < intEdgeCount + randomPairs; base += 4)
    {
        za::I32 v[4];
        for(int i = 0; i < 4; i++)
        {
            const int k = base + i;
            v[i] = k < intEdgeCount ? opaque(intEdges[k])
                                    : static_cast<za::I32>(lcg.next());
        }
        float nf[4], rf[4];
        toF32(native::i32x4::loadu(v)).storeu(nf);
        toF32(scalar::i32x4::loadu(v)).storeu(rf);
        za::I32 back[4];
        native::i32x4::loadu(v).storeu(back);
        for(int i = 0; i < 4; i++)
        {
            conv.check(same(nf[i], rf[i], false) && back[i] == v[i], 0, static_cast<za::U32>(v[i]), 0);
        }
    }
}

int appendTally(char* line, int at, int size, const char* name, const Tally& t)
{
    if(at < 0 || at >= size)
    {
        return at;
    }
    if(t.differ == 0)
    {
        return at + snprintf(line + at, static_cast<size_t>(size - at), "; %s %d results, the same", name, t.checks);
    }
    return at + snprintf(line + at, static_cast<size_t>(size - at), "; %s %d results, %d DIFFER (first: operation %d, %08x %08x)",
                    name, t.checks, t.differ, t.firstOp, t.firstA, t.firstB);
}

} // namespace

int selfTest(bool withF32x8, void (*print)(const char* line))
{
    Tally t4, t8, wide, conv;
    floatCases<4>(t4, [](const float* a, const float* b, float (*n)[4], float (*r)[4]) {
        f32x4Rows<native::f32x4>(a, b, n);
        f32x4Rows<scalar::f32x4>(a, b, r);
    });
    if(withF32x8)
    {
        floatCases<8>(t8, [](const float* a, const float* b, float (*n)[8], float (*r)[8]) {
            f32x8Rows<native::f32x8>(a, b, n);
            f32x8Rows<scalar::f32x8>(a, b, r);
        });
    }
    intCases(wide, conv);
    char line[512];
    int at = snprintf(line, sizeof(line), "vr_simd_selftest: %s against scalar", nativeName);
    at = appendTally(line, at, static_cast<int>(sizeof(line)), "f32x4", t4);
    if(withF32x8)
    {
        at = appendTally(line, at, static_cast<int>(sizeof(line)), "f32x8", t8);
    }
    else
    {
        at += snprintf(line + at, sizeof(line) - static_cast<size_t>(at), "; f32x8 not run (no AVX)");
    }
    at = appendTally(line, at, static_cast<int>(sizeof(line)), "i16x8 widen", wide);
    appendTally(line, at, static_cast<int>(sizeof(line)), "i32x4 to float", conv);
    print(line);
    return t4.differ + t8.differ + wide.differ + conv.differ;
}

} // namespace qvr::simd

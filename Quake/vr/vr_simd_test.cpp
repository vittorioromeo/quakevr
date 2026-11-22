// vr_simd_test.cpp -- "vr_simd_test": vr_simd.hpp's native backend (SSE2 on x64, NEON on ARM) against its plain C++
// one, bit for bit: every operation on fixed and random lanes (zeros of both signs, denormals, infinities, NaNs, the
// edges of the comparisons), then the three kernels that use it in the shapes they have there (vr_ao.cpp's ray test,
// vr_audio.cpp's anti-alias filter, vr_audio_io.cpp's interpolation taps). Ends with "vr_simd_test: N passed, M failed"
// (QUEST_PLAN.md, C2: the SIMD layer the Quest build needs, checked on the PC first).

#include "vr_simd.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_testkit.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"


namespace
{

struct Rng
{
    unsigned s = 0x9e3779b9u;
    unsigned next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uniform(float lo, float hi) { return lo + (hi - lo) * static_cast<float>(next() >> 8) * (1.f / 16777216.f); }
};

constexpr int lanesPerCase = 4;
constexpr int randomCases = 4096;
constexpr unsigned specials[] = {
    0x00000000u, 0x80000000u, // +0, -0
    0x00000001u, 0x807fffffu, // denormals
    0x3f800000u, 0xbf800000u, // 1, -1
    0x7f800000u, 0xff800000u, // infinities
    0x7fc00000u, 0xffc00001u, // NaNs
    0x3089705fu, 0x7f7fffffu, // 1e-9 (the rays' det threshold), the largest float
};
constexpr int specialCount = static_cast<int>(sizeof(specials) / sizeof(specials[0]));

float fromBits(unsigned u)
{
    float f;
    ZA_MEMCPY(&f, &u, sizeof(f));
    return f;
}

// One lane set: special values for the first cases, then random ones (some from the specials, mostly ordinary).
void fill(Rng& rng, int at, float* out)
{
    for(int i = 0; i < lanesPerCase; i++)
    {
        if(at < specialCount * specialCount)
        {
            out[i] = fromBits(specials[(i & 1) ? at / specialCount : at % specialCount]);
        }
        else if((rng.next() & 15) == 0)
        {
            out[i] = fromBits(specials[rng.next() % specialCount]);
        }
        else
        {
            out[i] = rng.uniform(-4.f, 4.f);
        }
    }
}

template<class V>
void applyOp(int op, const float* a, const float* b, float* out)
{
    const typename V::F4 x = V::loadu(a), y = V::loadu(b);
    typename V::F4 r;
    switch(op)
    {
        case 0: r = V::add(x, y); break;
        case 1: r = V::sub(x, y); break;
        case 2: r = V::mul(x, y); break;
        case 3: r = V::div(x, y); break;
        case 4: r = V::bitAnd(x, y); break;
        case 5: r = V::bitOr(x, y); break;
        case 6: r = V::bitAndNot(x, y); break;
        case 7: r = V::abs(x); break;
        case 8: r = V::cmpLt(x, y); break;
        case 9: r = V::cmpGt(x, y); break;
        case 10: r = V::cmpNotLt(x, y); break;
        case 11: r = V::cmpNotGt(x, y); break;
        case 12: r = V::set1(a[2]); break;
        default: r = V::zero(); break;
    }
    V::storeu(out, r);
    if(op == 13)
    {
        out[0] = V::hsum(x);
    }
}
constexpr const char* opNames[] = {"add", "sub", "mul", "div", "and", "or", "andnot", "abs", "cmplt", "cmpgt",
    "cmpnotlt", "cmpnotgt", "set1", "hsum"};
constexpr int opCount = static_cast<int>(sizeof(opNames) / sizeof(opNames[0]));

// vr_ao.cpp's raysX4, on made-up triangles (its operations, the same order).
template<class V>
void rayKernel(const float* tri, int ntri, const float* dx, const float* dy, const float* dz, float* best)
{
    using F4 = typename V::F4;
    const F4 vtmin = V::set1(0.01f), vzero = V::zero(), vone = V::set1(1.f), vtiny = V::set1(1e-9f);
    const F4 ddx = V::loadu(dx), ddy = V::loadu(dy), ddz = V::loadu(dz);
    F4 vbest = V::set1(64.f);
    for(int c = 0; c < ntri; c++)
    {
        const float* t = tri + c * 13;
        const F4 e1x = V::set1(t[0]), e1y = V::set1(t[1]), e1z = V::set1(t[2]);
        const F4 e2x = V::set1(t[3]), e2y = V::set1(t[4]), e2z = V::set1(t[5]);
        const F4 pvx = V::sub(V::mul(ddy, e2z), V::mul(e2y, ddz));
        const F4 pvy = V::sub(V::mul(ddz, e2x), V::mul(e2z, ddx));
        const F4 pvz = V::sub(V::mul(ddx, e2y), V::mul(e2x, ddy));
        const F4 det = V::add(V::add(V::mul(e1x, pvx), V::mul(e1y, pvy)), V::mul(e1z, pvz));
        F4 ok = V::cmpNotLt(V::abs(det), vtiny);
        const F4 inv = V::div(vone, det);
        const F4 u = V::mul(V::add(V::add(V::mul(V::set1(t[6]), pvx), V::mul(V::set1(t[7]), pvy)), V::mul(V::set1(t[8]), pvz)), inv);
        ok = V::bitAnd(ok, V::bitAnd(V::cmpNotLt(u, vzero), V::cmpNotGt(u, vone)));
        const F4 v = V::mul(V::add(V::add(V::mul(ddx, V::set1(t[9])), V::mul(ddy, V::set1(t[10]))), V::mul(ddz, V::set1(t[11]))), inv);
        ok = V::bitAnd(ok, V::bitAnd(V::cmpNotLt(v, vzero), V::cmpNotGt(V::add(u, v), vone)));
        const F4 dist = V::mul(V::set1(t[12]), inv);
        ok = V::bitAnd(ok, V::bitAnd(V::cmpGt(dist, vtmin), V::cmpLt(dist, vbest)));
        vbest = V::bitOr(V::bitAnd(ok, dist), V::bitAndNot(ok, vbest));
    }
    V::storeu(best, vbest);
}

// vr_audio.cpp's AntiAlias::applyOne four outputs at a time (eight sums of four lanes, then the tail taps).
template<class V>
void firKernel(const float* in, const float* c, int taps, float* out)
{
    typename V::F4 s[8];
    for(int j = 0; j < 8; j++)
    {
        s[j] = V::zero();
    }
    int k = 0;
    for(; k + 8 <= taps; k += 8)
    {
        for(int j = 0; j < 8; j++)
        {
            s[j] = V::add(s[j], V::mul(V::set1(c[k + j]), V::loadu(in + k + j)));
        }
    }
    typename V::F4 y = V::add(V::add(V::add(s[0], s[1]), V::add(s[2], s[3])), V::add(V::add(s[4], s[5]), V::add(s[6], s[7])));
    for(; k < taps; k++)
    {
        y = V::add(y, V::mul(V::set1(c[k]), V::loadu(in + k)));
    }
    V::storeu(out, y);
}

// vr_audio_io.cpp's interpolation: 16-bit samples times a row of taps, eight at a time, one sum.
template<class V>
float tapsKernel(const short* s, const float* row, int taps)
{
    typename V::F4 acc0 = V::zero(), acc1 = V::zero();
    for(int j = 0; j < taps; j += 8)
    {
        typename V::F4 lo, hi;
        V::widenS16(s + j, lo, hi);
        acc0 = V::add(acc0, V::mul(V::loadu(row + j), lo));
        acc1 = V::add(acc1, V::mul(V::loadu(row + j + 4), hi));
    }
    return V::hsum(V::add(acc0, acc1));
}

// The same bits, lane by lane; two NaNs count as the same (which input's payload an add or a multiply of two NaNs
// keeps is the compiler's operand order and the CPU's rule, not IEEE's: x86 and ARM differ, and the kernels only ever
// compare a NaN, which every backend does the same way). Masks and the bit operations are compared exactly.
bool sameBits(const float* a, const float* b, int n)
{
    for(int i = 0; i < n; i++)
    {
        if(ZA_MEMCMP(a + i, b + i, sizeof(float)) != 0 && !(a[i] != a[i] && b[i] != b[i]))
        {
            return false;
        }
    }
    return true;
}

void test_f()
{
    qvr::testkit::Tally t;
    Rng rng;
    for(int op = 0; op < opCount; op++)
    {
        for(int at = 0; at < randomCases; at++)
        {
            float a[4], b[4], rn[4] = {}, rs[4] = {};
            fill(rng, at, a);
            fill(rng, at + 7, b);
            applyOp<simd::Native>(op, a, b, rn);
            applyOp<simd::Scalar>(op, a, b, rs);
            t.check(sameBits(rn, rs, op == 13 ? 1 : 4), "vr_simd_test", opNames[op]);
        }
    }

    // The kernels: random rays against random triangles (a few near-degenerate), filters and taps over random signals.
    constexpr int ntri = 24;
    constexpr int taps = 31; // (an odd count: the filter's tail loop runs too)
    constexpr int interpTaps = 16;
    for(int at = 0; at < 512; at++)
    {
        float tri[ntri * 13], dx[4], dy[4], dz[4], bn[4], bs[4];
        for(float& f : tri)
        {
            f = rng.uniform(-2.f, 2.f);
        }
        if(at & 1)
        {
            tri[0] = tri[1] = tri[2] = 0.f; // a degenerate triangle: det 0
        }
        for(int i = 0; i < 4; i++)
        {
            dx[i] = rng.uniform(-1.f, 1.f);
            dy[i] = rng.uniform(-1.f, 1.f);
            dz[i] = rng.uniform(-1.f, 1.f);
        }
        rayKernel<simd::Native>(tri, ntri, dx, dy, dz, bn);
        rayKernel<simd::Scalar>(tri, ntri, dx, dy, dz, bs);
        t.check(sameBits(bn, bs, 4), "vr_simd_test", "ray kernel");

        float in[taps + 4], c[taps], fn[4], fs[4];
        for(float& f : in)
        {
            f = rng.uniform(-1.f, 1.f);
        }
        for(float& f : c)
        {
            f = rng.uniform(-0.5f, 0.5f);
        }
        firKernel<simd::Native>(in, c, taps, fn);
        firKernel<simd::Scalar>(in, c, taps, fs);
        t.check(sameBits(fn, fs, 4), "vr_simd_test", "filter kernel");

        short smp[interpTaps];
        float row[interpTaps];
        for(int i = 0; i < interpTaps; i++)
        {
            smp[i] = static_cast<short>(static_cast<int>(rng.next() & 0xffff) - 32768);
            row[i] = rng.uniform(-1.f, 1.f);
        }
        const float tn = tapsKernel<simd::Native>(smp, row, interpTaps);
        const float ts = tapsKernel<simd::Scalar>(smp, row, interpTaps);
        t.check(sameBits(&tn, &ts, 1), "vr_simd_test", "taps kernel");
    }
    Con_Printf("vr_simd_test: native %s against scalar\n", simd::Native::name);
    t.verdict("vr_simd_test");
}

} // namespace

extern "C" void VR_SimdTestInit(void)
{
    Cmd_AddCommand("vr_simd_test", test_f);
}

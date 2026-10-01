// vr_zancle.cpp -- Zancle's assertion failures (ZA_ASSERT) as a Quake error; and vr_zancle_math_test, Zancle's math
// against the standard library's.
//
// Zancle (external/zancle/README.md) turns its asserts on where NDEBUG is not defined (ZA_DEBUG, Config.hpp): the
// engine's Debug builds, in any file that includes a Zancle header, and the library's own sources when they are built
// as Debug (QVR_ZANCLE_DEBUG). A failure, in either, calls the handler installed here (za::setAssertHandler, from
// VR_InstallCrashHandler at startup): it breaks into the debugger if one is attached, then reports the assert and quits
// (Sys_Error; in a test run, the crash report with its stack). With the library built without its asserts (Release
// settings) it has no assert function for the engine's Debug files to call: this file defines it, the same report.

#include "vr_engine.hpp"

#include "vr_zancle.hpp"

#include "Zancle/Algorithm/NthElement.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/IsInf.hpp"
#include "Zancle/Base/IsNan.hpp"
#include "Zancle/Base/Limits.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cbrt.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Exp2.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Hypot.hpp"
#include "Zancle/Math/Llround.hpp"
#include "Zancle/Math/Log2.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Trunc.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "Zancle/Vocabulary/Pair.hpp"

#include <algorithm> // vr_zancle_math_test's reference (std::min, max, clamp, stable_sort, nth_element; std::pair)
#include <cmath>     // vr_zancle_math_test's reference (the std:: functions the migration replaced), nothing else
#include <cstdlib>   // (std::abs's integer overloads, the same)

extern "C" void VR_FatalReport(const char* what); // vr_crash.cpp (test runs: the report, then the process ends)

namespace
{

[[noreturn]] void reportAssert(const char* code, const char* file, const int line)
{
    // A test run (QVR_NO_ERROR_DIALOG): the crash report, with the stack that failed it (vr_crash.cpp).
    if(getenv("QVR_NO_ERROR_DIALOG"))
    {
        char what[1024];
        q_snprintf(what, sizeof(what), "Zancle assertion failed: ZA_ASSERT(%s) at %s:%d", code, file, line);
        VR_FatalReport(what);
    }
    Sys_Error("Zancle assertion failed: ZA_ASSERT(%s) at %s:%d", code, file, line);
}

} // namespace

// Called first thing (VR_InstallCrashHandler), before any other thread: Zancle's handler is not synchronised.
extern "C" void VR_InstallZancleAssertHandler()
{
    za::setAssertHandler(&reportAssert);
}

#if defined(ZA_DEBUG) && !defined(QVR_ZANCLE_DEBUG)

// The library built without its asserts (Release settings: QVR_ZANCLE_DEBUG off) defines no assert function (its
// Assert.cpp has it only with ZA_DEBUG): the engine's Debug files call this one.
namespace za::priv
{

void assertFailure(const char* code, const char* file, const int line)
{
    reportAssert(code, file, line);
}

} // namespace za::priv

#endif

namespace qza
{

namespace
{

// The same value to the last bit (any two NaNs alike; -0 and +0 told apart).
[[nodiscard]] bool same(const float a, const float b) noexcept
{
    return (ZA_ISNAN(a) && ZA_ISNAN(b)) || ZA_BIT_CAST(za::U32, a) == ZA_BIT_CAST(za::U32, b);
}

[[nodiscard]] bool same(const double a, const double b) noexcept
{
    return (ZA_ISNAN(a) && ZA_ISNAN(b)) || ZA_BIT_CAST(za::U64, a) == ZA_BIT_CAST(za::U64, b);
}

} // namespace

// vr_zancle_math_test: the Zancle (and qza) math the Zancle migration put in place of the standard library's, against
// the standard library's on edge values (signed zeros, halves, the wrap angles, huge, tiny, infinite, NaN): every result
// the same to the last bit. Also counts where za::truncatedRemainder (Zancle's old za::remainder, before fad225a4)
// differs from std::remainder (the reason the angle wraps use za::remainder, the IEEE one).
void mathTest_f()
{
    constexpr float inf = ZA_FLOAT_INFINITY, nanF = ZA_FLOAT_NAN;
    const float vals[] = {0.f, -0.f, 0.5f, -0.5f, 1.5f, -1.5f, 2.5f, -2.5f, 0.49999997f, 1.f, -1.f, 3.14159265f, 179.f,
        -179.f, 179.99f, 180.f, -180.f, 180.01f, 181.f, -181.f, 270.f, -270.f, 359.f, -359.f, 360.f, -360.f, 361.f, 540.f,
        -540.f, 719.f, 720.f, -720.f, 1e-40f, -1e-40f, 1e-7f, 123456.78f, -98765.43f, 8388607.5f, 1e9f, -1e9f, 3e38f,
        -3e38f, inf, -inf, nanF};
    const float divisors[] = {360.f, 6.28318531f, 1.f, 0.5f, 7.f, -360.f};
    int checks = 0, fails = 0, zaRemainderDiffers = 0, zaRemainderChecks = 0;
    const auto check = [&](const bool ok, const char* what, const double a, const double b) {
        ++checks;
        if(!ok && ++fails <= 12)
        {
            Con_Printf("vr_zancle_math_test: FAIL %s (%.9g, %.9g)\n", what, a, b);
        }
    };
    for(const float a : vals)
    {
        const double d = a;
        const bool finite = ZA_ISFINITE(a);
        check(same(za::floor(a), std::floor(a)) && same(za::floor(d), std::floor(d)), "floor", a, 0);
        check(same(za::ceil(a), std::ceil(a)) && same(za::ceil(d), std::ceil(d)), "ceil", a, 0);
        check(same(za::round(a), std::round(a)) && same(za::round(d), std::round(d)), "round", a, 0);
        check(same(za::trunc(a), std::trunc(a)) && same(za::trunc(d), std::trunc(d)), "trunc", a, 0);
        check(same(za::cbrt(a), std::cbrt(a)) && same(za::cbrt(d), std::cbrt(d)), "cbrt", a, 0);
        check(same(za::log2(a), std::log2(a)) && same(za::log2(d), std::log2(d)), "log2", a, 0);
        check(same(za::exp2(a), std::exp2(a)) && same(za::exp2(d), std::exp2(d)), "exp2", a, 0);
        check(same(za::abs(a), std::abs(a)) && same(za::abs(d), std::abs(d)), "abs", a, 0);
        check(ZA_ISFINITE(a) == std::isfinite(a) && ZA_ISNAN(a) == std::isnan(a) && ZA_ISINF(a) == std::isinf(a), "isfinite/isnan/isinf", a, 0);
        if(finite && za::fabs(a) < 1e9f) // (lround and llround out of their range: unspecified)
        {
            check(za::lround(a) == std::lround(a) && za::lround(d) == std::lround(d), "lround", a, 0);
            check(za::llround(a) == std::llround(a) && za::llround(d) == std::llround(d), "llround", a, 0);
            const int i = static_cast<int>(a);
            check(za::abs(i) == std::abs(i), "abs (int)", a, 0);
        }
        for(const float b : vals)
        {
            check(same(za::atan2(a, b), std::atan2(a, b)), "atan2", a, b);
            check(same(za::copysign(a, b), std::copysign(a, b)), "copysign", a, b);
            check(same(za::hypot(a, b), std::hypot(a, b)), "hypot", a, b);
            check(same(za::fmin(a, b), std::fmin(a, b)) && same(za::fmax(a, b), std::fmax(a, b)), "fmin/fmax", a, b);
            check(same(za::min(a, b), std::min(a, b)) && same(za::max(a, b), std::max(a, b)), "min/max", a, b);
            // Of 3 or 4: std::min and std::max folded from the left (qza::minOf/maxOf's, which these replaced); against
            // std's list overloads too where there is no NaN (no order then: MSVC's vectorised ones give other results).
            const float m3 = std::min(std::min(a, b), -b), M3 = std::max(std::max(a, b), -b);
            const float m4 = std::min(std::min(std::min(b, a), a), b), M4 = std::max(std::max(std::max(b, a), a), b);
            check(same(za::min(a, b, -b), m3) && same(za::max(a, b, -b), M3) && same(za::min(b, a, a, b), m4) &&
                      same(za::max(b, a, a, b), M4) &&
                      (ZA_ISNAN(a) || ZA_ISNAN(b) ||
                          (same(m3, std::min({a, b, -b})) && same(M3, std::max({a, b, -b})) &&
                              same(m4, std::min({b, a, a, b})) && same(M4, std::max({b, a, a, b})))),
                "min/max (3, 4)", a, b);
            if(!ZA_ISNAN(a) && !ZA_ISNAN(b))
            {
                const float lo = za::min(a, b), hi = za::max(a, b);
                for(const float v : vals)
                {
                    check(same(za::clamp(v, lo, hi), std::clamp(v, lo, hi)), "clamp", v, a);
                }
            }
        }
        for(const float b : divisors)
        {
            check(same(za::fmod(a, b), std::fmod(a, b)), "fmod", a, b);
            check(same(za::remainder(a, b), std::remainder(a, b)) && same(za::remainder(d, double{b}), std::remainder(d, double{b})),
                "remainder", a, b);
            if(finite && b > 0.f && za::fabs(a / b) < 2e9f) // (za::truncatedRemainder: b > 0, a quotient that fits an int)
            {
                ++zaRemainderChecks;
                zaRemainderDiffers += same(za::truncatedRemainder(a, b), std::remainder(a, b)) ? 0 : 1;
            }
        }
    }
    // The angle wrap: every whole and half degree from -1080 to 1080 lands in [-180, 180].
    for(int k = -2160; k <= 2160; k++)
    {
        const float a = static_cast<float>(k) * 0.5f, r = za::remainder(a, 360.f);
        check(r >= -180.f && r <= 180.f && same(r, std::remainder(a, 360.f)), "the angle wrap", a, 360.0);
    }
    // The algorithms that replaced std's: stableSort is std::stable_sort to the element (keys with many ties, each
    // tagged with its place); nthElement puts std::nth_element's value at the nth place, nothing after it less and
    // nothing before it greater (which of equal keys goes where is its own). Fixed seed: the same arrays every run.
    za::FastNonCryptoRng rng{20261001u};
    for(int round = 0; round < 200; round++)
    {
        const int n = rng.getI(1, 300), range = rng.getI(1, 40);
        za::Vector<za::Pair<int, int>> mine;
        za::Vector<std::pair<int, int>> ref;
        for(int i = 0; i < n; i++)
        {
            const int key = rng.getI(0, range);
            mine.pushBack(za::Pair<int, int>{key, i});
            ref.pushBack(std::pair<int, int>{key, i});
        }
        za::stableSort(mine.begin(), mine.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::stable_sort(ref.begin(), ref.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        bool sameOrder = true;
        for(int i = 0; i < n; i++)
        {
            sameOrder = sameOrder && mine[i].first == ref[i].first && mine[i].second == ref[i].second;
        }
        check(sameOrder, "stableSort", n, range);
        za::Vector<int> keys, refKeys;
        for(int i = 0; i < n; i++)
        {
            keys.pushBack(rng.getI(0, range));
            refKeys.pushBack(keys.back());
        }
        const int nth = rng.getI(0, n - 1);
        za::nthElement(keys.begin(), keys.begin() + nth, keys.end(), [](int a, int b) { return a < b; });
        std::nth_element(refKeys.begin(), refKeys.begin() + nth, refKeys.end());
        bool partitioned = keys[nth] == refKeys[nth];
        for(int i = 0; i < n; i++)
        {
            partitioned = partitioned && (i < nth ? keys[i] <= keys[nth] : keys[i] >= keys[nth]);
        }
        check(partitioned, "nthElement", n, nth);
    }
    Con_Printf("vr_zancle_math_test: %d checks, %d failed%s; za::truncatedRemainder differs from std::remainder in %d of %d\n",
        checks, fails, fails ? " (FAIL)" : " (ok)", zaRemainderDiffers, zaRemainderChecks);
}

} // namespace qza

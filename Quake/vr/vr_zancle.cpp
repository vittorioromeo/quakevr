// vr_zancle.cpp -- Zancle's assertion failures (ZA_ASSERT) in the engine's own code, as a Quake error; and the stand-ins
// of vr_zancle.hpp that keep a standard header out of the headers (qza::nowNs).
//
// Zancle (external/zancle/README.md) turns its asserts on where NDEBUG is not defined (ZA_DEBUG, Config.hpp): the
// engine's Debug builds, in any file that includes a Zancle header. Its own sources are built without them (NDEBUG) in
// every configuration, so its Assert.cpp defines no handler for those files to call: this is it. A failure breaks into
// the debugger if one is attached, then reports the assert and quits (Sys_Error).

#include "vr_engine.hpp"

#include "vr_zancle.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/IsInf.hpp"
#include "Zancle/Base/IsNan.hpp"
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

#include <chrono> // ZANCLE-TODO: a nanosecond steady clock (qza::nowNs)
#include <algorithm> // vr_zancle_math_test's reference (std::min, std::max, std::clamp), nothing else
#include <cmath>     // vr_zancle_math_test's reference (the std:: functions the migration replaced), nothing else
#include <cstdlib>   // (std::abs's integer overloads, the same)

#ifdef ZA_DEBUG

extern "C" void VR_FatalReport(const char* what); // vr_crash.cpp (test runs: the report, then the process ends)

namespace za::priv
{

void assertFailure(const char* code, const char* file, const int line)
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

} // namespace za::priv

#endif

namespace qza
{

za::I64 nowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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
// the same to the last bit. Also counts where za::remainder (a truncated remainder) differs from std::remainder (the
// reason the angle wraps use qza::remainder).
void mathTest_f()
{
    constexpr float inf = __builtin_inff();
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
        check(same(qza::abs(a), std::abs(a)) && same(qza::abs(d), std::abs(d)), "abs", a, 0);
        check(ZA_ISFINITE(a) == std::isfinite(a) && ZA_ISNAN(a) == std::isnan(a) && ZA_ISINF(a) == std::isinf(a), "isfinite/isnan/isinf", a, 0);
        if(finite && za::fabs(a) < 1e9f) // (lround and llround out of their range: unspecified)
        {
            check(za::lround(a) == std::lround(a) && za::lround(d) == std::lround(d), "lround", a, 0);
            check(za::llround(a) == std::llround(a) && za::llround(d) == std::llround(d), "llround", a, 0);
            const int i = static_cast<int>(a);
            check(qza::abs(i) == std::abs(i), "abs (int)", a, 0);
        }
        for(const float b : vals)
        {
            check(same(za::atan2(a, b), std::atan2(a, b)), "atan2", a, b);
            check(same(za::copysign(a, b), std::copysign(a, b)), "copysign", a, b);
            check(same(za::hypot(a, b), std::hypot(a, b)), "hypot", a, b);
            check(same(za::fmin(a, b), std::fmin(a, b)) && same(za::fmax(a, b), std::fmax(a, b)), "fmin/fmax", a, b);
            check(same(za::min(a, b), std::min(a, b)) && same(za::max(a, b), std::max(a, b)), "min/max", a, b);
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
            check(same(qza::remainder(a, b), std::remainder(a, b)) && same(qza::remainder(d, double{b}), std::remainder(d, double{b})),
                "qza::remainder", a, b);
            if(finite && b > 0.f && za::fabs(a / b) < 2e9f) // (za::remainder: b > 0, a quotient that fits an int)
            {
                ++zaRemainderChecks;
                zaRemainderDiffers += same(za::remainder(a, b), std::remainder(a, b)) ? 0 : 1;
            }
        }
    }
    // The angle wrap: every whole and half degree from -1080 to 1080 lands in [-180, 180].
    for(int k = -2160; k <= 2160; k++)
    {
        const float a = static_cast<float>(k) * 0.5f, r = qza::remainder(a, 360.f);
        check(r >= -180.f && r <= 180.f && same(r, std::remainder(a, 360.f)), "the angle wrap", a, 360.0);
    }
    Con_Printf("vr_zancle_math_test: %d checks, %d failed%s; za::remainder (truncated) differs from std::remainder in %d of %d\n",
        checks, fails, fails ? " (FAIL)" : " (ok)", zaRemainderDiffers, zaRemainderChecks);
}

} // namespace qza

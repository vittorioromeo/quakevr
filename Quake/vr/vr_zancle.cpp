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

#include "Zancle/Base/Assert.hpp"

extern "C" void VR_FatalReport(const char* what); // vr_crash.cpp (test runs: the report, then the process ends)

namespace
{

[[noreturn]] void reportAssert(const char* code, const char* file, const int line)
{
    // A Quake error: its crash report has the stack that failed it (vr_crash.cpp; off the main thread too). Before the
    // engine is up (a static initialiser's: no host to shut down) the report alone, and the process ends.
    if(!host_parms)
    {
        char what[1024];
        q_snprintf(what, sizeof(what), "Zancle assertion failed: ZA_ASSERT(%s) at %s:%d", code, file, line);
        VR_FatalReport(what);
    }
    Sys_Error("Zancle assertion failed: ZA_ASSERT(%s) at %s:%d", code, file, line);
}

} // namespace

// vr_crash_test assert (vr_main.cpp): a failed assert's report in any build (Release has no ZA_ASSERT to fail).
extern "C" void VR_ReportAssert(const char* code, const char* file, const int line)
{
    reportAssert(code, file, line);
}

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

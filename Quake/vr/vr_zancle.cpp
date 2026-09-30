// vr_zancle.cpp -- Zancle's assertion failures (ZA_ASSERT) in the engine's own code, as a Quake error.
//
// Zancle (external/zancle/README.md) turns its asserts on where NDEBUG is not defined (ZA_DEBUG, Config.hpp): the
// engine's Debug builds, in any file that includes a Zancle header. Its own sources are built without them (NDEBUG) in
// every configuration, so its Assert.cpp defines no handler for those files to call: this is it. A failure breaks into
// the debugger if one is attached, then reports the assert and quits (Sys_Error).

#include "vr_engine.hpp"

#include "Zancle/Base/Assert.hpp"

#ifdef ZA_DEBUG

namespace za::priv
{

void assertFailure(const char* code, const char* file, const int line)
{
    Sys_Error("Zancle assertion failed: ZA_ASSERT(%s) at %s:%d", code, file, line);
}

} // namespace za::priv

#endif

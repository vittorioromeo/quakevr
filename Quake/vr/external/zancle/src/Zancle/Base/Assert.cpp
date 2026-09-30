// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"

#include "Zancle/Config.hpp" // IWYU pragma: keep

// Quake VR (local change): not with QVR_ZANCLE_DEBUG (the library built with its asserts, in the engine's Debug): the
// engine's handler (Quake/vr/vr_zancle.cpp) is the one, for its files and the library's.
#if defined(ZA_DEBUG) && !defined(QVR_ZANCLE_DEBUG)

    #include "Zancle/Base/Abort.hpp"
    #include "Zancle/Base/StackTrace.hpp"

    #include <cstdio>


namespace za::priv
{
////////////////////////////////////////////////////////////
void assertFailure(const char* code, const char* file, const int line)
{
    // Flush pending regular output first, as `abort()` does not flush stdio buffers
    std::fflush(stdout);

    // NOLINTNEXTLINE(modernize-use-std-print)
    std::fprintf(stderr, "\n[[ZANCLE ASSERTION FAILURE]]\n- %s:%d\n- ZA_ASSERT(%s);\n", file, line, code);

    printStackTrace();
    za::abort();
}

} // namespace za::priv

#endif

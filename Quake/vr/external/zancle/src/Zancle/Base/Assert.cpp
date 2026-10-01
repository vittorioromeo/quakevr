// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"

#include "Zancle/Config.hpp" // IWYU pragma: keep

#ifdef ZA_DEBUG

    #include "Zancle/Base/Abort.hpp"
    #include "Zancle/Base/StackTrace.hpp"

    #include <cstdio>

#endif


namespace
{
////////////////////////////////////////////////////////////
constinit za::AssertHandler assertHandler = nullptr; // `nullptr`: default handling

} // namespace


namespace za
{
////////////////////////////////////////////////////////////
AssertHandler setAssertHandler(const AssertHandler handler) noexcept
{
    const AssertHandler previous = assertHandler;
    assertHandler                = handler;
    return previous;
}

} // namespace za


#ifdef ZA_DEBUG

namespace za::priv
{
////////////////////////////////////////////////////////////
void assertFailure(const char* code, const char* file, const int line)
{
    // A failed assertion within the handler gets the default handling, instead of recursing
    thread_local bool inHandler = false;

    if (const AssertHandler handler = assertHandler; handler != nullptr && !inHandler)
    {
        inHandler = true;
        handler(code, file, line); // normally does not return
        inHandler = false;
    }

    // Flush pending regular output first, as `abort()` does not flush stdio buffers
    std::fflush(stdout);

    // NOLINTNEXTLINE(modernize-use-std-print)
    std::fprintf(stderr, "\n[[ZANCLE ASSERTION FAILURE]]\n- %s:%d\n- ZA_ASSERT(%s);\n", file, line, code);

    printStackTrace();
    za::abort();
}

} // namespace za::priv

#endif

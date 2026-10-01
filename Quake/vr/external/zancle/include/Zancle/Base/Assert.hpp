#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Function called when an assertion fails: the failed condition's code, its file, and its line
///
////////////////////////////////////////////////////////////
using AssertHandler = void (*)(const char* code, const char* file, int line);


////////////////////////////////////////////////////////////
/// \brief Install `handler` to be called when a `ZA_ASSERT` fails, in user code or in the library itself
///
/// Useful to route failures into an application's own error reporting
/// (e.g. a crash dialog or a log). A handler normally does not return
/// (e.g. it reports and exits); if it does, the default handling follows:
/// printing the failure and a stack trace to `stderr`, then aborting. A
/// failed assertion within the handler itself also gets the default handling.
///
/// The handler is not synchronized: install it at startup, before other
/// threads may fail an assertion. Assertions only exist in debug builds
/// (`ZA_DEBUG`); elsewhere, installing a handler has no effect.
///
/// \param handler Handler to install, or `nullptr` for the default handling
///
/// \return The previously installed handler (`nullptr` for the default handling)
///
////////////////////////////////////////////////////////////
ZA_SYSTEM_API AssertHandler setAssertHandler(AssertHandler handler) noexcept;

} // namespace za


#ifdef ZA_DEBUG

namespace za::priv
{
////////////////////////////////////////////////////////////
[[noreturn, gnu::cold, gnu::noinline]] ZA_SYSTEM_API void assertFailure(const char* code, const char* file, int line);

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_ASSERT(...)                                                   \
        do                                                                   \
        {                                                                    \
            if (!static_cast<bool>(__VA_ARGS__)) [[unlikely]]                \
                ::za::priv::assertFailure(#__VA_ARGS__, __FILE__, __LINE__); \
                                                                             \
        } while (false)

#else

    ////////////////////////////////////////////////////////////
    #define ZA_ASSERT(...)

#endif

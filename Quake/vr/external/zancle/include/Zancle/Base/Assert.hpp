#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep


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

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
/// \brief Immediately terminate the current process abnormally
///
/// This function calls `std::abort()`. It is used internally
/// by Zancle for irrecoverable errors, such as failed assertions
/// in debug mode.
///
////////////////////////////////////////////////////////////
[[noreturn, gnu::cold, gnu::noinline]] ZA_SYSTEM_API void abort() noexcept;

} // namespace za

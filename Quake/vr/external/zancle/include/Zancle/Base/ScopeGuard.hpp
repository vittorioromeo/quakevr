#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/TokenPaste.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief RAII helper that runs an arbitrary callable on scope exit
///
/// Inherits from the callable so that no extra storage is required for
/// stateless lambdas (empty base optimization). The destructor invokes
/// the captured callable, making this a building block for ad-hoc
/// cleanup code without requiring a custom RAII type.
///
/// Use the `ZA_SCOPE_GUARD` macro for the canonical
/// `[&] { ... }` pattern.
///
////////////////////////////////////////////////////////////
template <typename F>
struct [[nodiscard]] ScopeGuard : F
{
    ////////////////////////////////////////////////////////////
    /// \brief Destructor, invokes the wrapped callable
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] ~ScopeGuard()
    {
        static_cast<F&>(*this)();
    }
};


////////////////////////////////////////////////////////////
/// \brief Deduction guide allowing `ScopeGuard{lambda}` syntax (required by clang-cl)
///
////////////////////////////////////////////////////////////
template <typename F>
ScopeGuard(F) -> ScopeGuard<F>; // Needed by clang-cl

} // namespace za


////////////////////////////////////////////////////////////
/// \brief Convenience macro that declares a uniquely-named scope guard from a lambda body
///
/// Usage: `ZA_SCOPE_GUARD({ cleanup(); });`
///
/// The lambda is marked `always_inline` so that its body is inlined into
/// the guard's destructor even in unoptimized builds, without recursively
/// inlining everything it calls (as `gnu::flatten` would on GCC).
///
////////////////////////////////////////////////////////////
#define ZA_SCOPE_GUARD(...)                                      \
    const ::za::ScopeGuard ZA_TOKEN_PASTE(_scopeGuard, __LINE__) \
    {                                                            \
        [&] [[gnu::always_inline]] __VA_ARGS__                   \
    }

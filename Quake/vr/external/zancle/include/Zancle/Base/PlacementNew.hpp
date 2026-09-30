#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \brief Custom placement-new overload that does not require including `<new>`
///
/// The extra `int` parameter disambiguates this overload from the
/// standard placement-new.
///
/// Deliberately not `noexcept`: a new-expression calling a non-throwing
/// allocation function must null-check the returned pointer before
/// constructing the object ([expr.new]), and only the standard placement
/// form is exempt. That check was emitted at every call site, even at
/// `-O2`. `gnu::returns_nonnull` additionally removes it on Clang.
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::const, gnu::returns_nonnull]] inline void* operator new(decltype(sizeof(int)), int, void* ptr)
{
    return ptr;
}


////////////////////////////////////////////////////////////
/// \brief Matching no-op placement-delete (required by the language for completeness)
///
////////////////////////////////////////////////////////////
[[gnu::always_inline]] inline void operator delete(void*, int, void*) noexcept
{
}


////////////////////////////////////////////////////////////
/// \brief Convenience macro: `ZA_PLACEMENT_NEW(addr) T(args...)`
///
/// Equivalent to `::new (addr) T(args...)` but uses the placement-new
/// overload above to avoid `<new>`.
///
////////////////////////////////////////////////////////////
#define ZA_PLACEMENT_NEW(...) ::new (int{}, __VA_ARGS__)

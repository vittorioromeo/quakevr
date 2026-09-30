#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \brief Opt a class into trivial relocatability when the given predicate is true.
///
/// Must be placed in a `public` section of the class body.
///
/// The opt-in is intentionally *not* inherited: a class deriving from
/// an opted-in class must opt in again, as it may add members (e.g.
/// self-pointers) that do not survive a `memcpy`. This is enforced via
/// the declared-only `zaPrivTriviallyRelocatableSelf` member function,
/// whose `decltype(this)` return type names the class that opted in.
///
////////////////////////////////////////////////////////////
#define ZA_ENABLE_TRIVIAL_RELOCATION_IF(...)    \
    enum : bool                                 \
    {                                           \
        enableTrivialRelocation = (__VA_ARGS__) \
    };                                          \
                                                \
    auto zaPrivTriviallyRelocatableSelf() const noexcept -> decltype(this)


////////////////////////////////////////////////////////////
/// \brief Opt a class into trivial relocatability unconditionally
///
/// Shorthand for `ZA_ENABLE_TRIVIAL_RELOCATION_IF(true)`. Place inside
/// the class body:
///
/// \code
/// class Foo
/// {
/// public:
///     ZA_ENABLE_TRIVIAL_RELOCATION;
/// };
/// \endcode
///
////////////////////////////////////////////////////////////
#define ZA_ENABLE_TRIVIAL_RELOCATION ZA_ENABLE_TRIVIAL_RELOCATION_IF(true)

#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/FwdStdAlignedNewDelete.hpp" // IWYU pragma: export


////////////////////////////////////////////////////////////
#ifdef __CLANGD__
    #include <new> // IWYU pragma: export
#endif


////////////////////////////////////////////////////////////
// The declarations below must name the very types and object of the
// standard library, so that replacement allocation functions declared
// with them replace the library's, and so that `<new>` can still be
// included before or after this header. Every major implementation
// declares them directly in `namespace std` (never in an inline or
// versioned namespace, as they are part of the language support ABI):
//
// - MSVC's STL (`vcruntime_new.h`, also used by clang-cl and by libc++
//   on the vcruntime ABI): `struct nothrow_t`, `extern nothrow_t const
//   nothrow`, `enum class align_val_t : size_t` in `namespace std`.
// - libstdc++ (`<new>`): the same, inside `extern "C++" { namespace std`.
// - libc++ (`<new>`, `<__new/...>`): the same, in a `namespace std`
//   "purposefully not using versioning namespace".
//
////////////////////////////////////////////////////////////
namespace std // NOLINT(cert-dcl58-cpp)
{
////////////////////////////////////////////////////////////
struct nothrow_t;               // NOLINT(readability-identifier-naming)
extern const nothrow_t nothrow; // NOLINT(readability-identifier-naming, readability-redundant-declaration)

} // namespace std


////////////////////////////////////////////////////////////
// The replaceable nothrow allocation functions (the others are
// implicitly declared, or in `FwdStdAlignedNewDelete.hpp`)
////////////////////////////////////////////////////////////
// NOLINTBEGIN(readability-redundant-declaration)
void* operator new(decltype(sizeof(0)), const std::nothrow_t&) noexcept;
void* operator new[](decltype(sizeof(0)), const std::nothrow_t&) noexcept;
void* operator new(decltype(sizeof(0)), std::align_val_t, const std::nothrow_t&) noexcept;
void* operator new[](decltype(sizeof(0)), std::align_val_t, const std::nothrow_t&) noexcept;

void operator delete(void*, const std::nothrow_t&) noexcept;
void operator delete[](void*, const std::nothrow_t&) noexcept;
void operator delete(void*, std::align_val_t, const std::nothrow_t&) noexcept;
void operator delete[](void*, std::align_val_t, const std::nothrow_t&) noexcept;
// NOLINTEND(readability-redundant-declaration)


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Alias for `std::align_val_t`
///
/// Being the same type, an `operator new` or `operator delete` declared
/// with it is still the standard's aligned form (or replaces it).
///
////////////////////////////////////////////////////////////
using AlignValT = std::align_val_t;


////////////////////////////////////////////////////////////
/// \brief Alias for `std::nothrow_t`
///
/// Being the same type, an `operator new` or `operator delete` declared
/// with it is still the standard's nothrow form (or replaces it).
///
////////////////////////////////////////////////////////////
using NothrowT = std::nothrow_t;

} // namespace za


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Forward declarations of `<new>`'s `std::nothrow_t`, `std::nothrow`
///        and `std::align_val_t`, and of the nothrow allocation functions
///
/// Lets code call or replace the nothrow and aligned forms of
/// `operator new` and `operator delete` (e.g. an allocation counter's
/// replacement functions) without including the heavy `<new>` header.
/// `std::nothrow_t` is incomplete here: it can be named, passed by
/// reference, and `std::nothrow` passed, as the allocation functions do.
/// For placement new, see `Zancle/Base/PlacementNew.hpp`.
///
////////////////////////////////////////////////////////////

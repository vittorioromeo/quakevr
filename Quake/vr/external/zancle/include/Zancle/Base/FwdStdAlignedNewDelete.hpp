#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#ifdef __CLANGD__
    #include <new> // IWYU pragma: export
#endif


////////////////////////////////////////////////////////////
namespace std
{
////////////////////////////////////////////////////////////
enum class align_val_t : decltype(sizeof(0));

} // namespace std


////////////////////////////////////////////////////////////
void* operator new(decltype(sizeof(0)), std::align_val_t); // NOLINT(readability-redundant-declaration)
void  operator delete(void*, std::align_val_t) noexcept;   // NOLINT(readability-redundant-declaration)
void operator delete(void*, decltype(sizeof(0)), std::align_val_t) noexcept; // NOLINT(readability-redundant-declaration)

// Not implicitly declared by every compiler configuration (e.g. Clang without sized deallocation enabled)
void operator delete(void*, decltype(sizeof(0))) noexcept; // NOLINT(readability-redundant-declaration)


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Forward declarations of `std::align_val_t` and aligned `new`/`delete`
///
/// Allows code to use the aligned-allocation overloads of `operator new`
/// and `operator delete` without pulling in the heavy `<new>` header.
///
////////////////////////////////////////////////////////////

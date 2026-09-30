#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Returns an rvalue reference to a hypothetical `T`
///        without requiring `T` to be constructible
///
/// Unlike `std::declval`, `declVal<void>()` is ill-formed: supporting it
/// would require either a builtin trait in the return type (rejected by
/// GCC in mangled signatures) or an extra overload, which would slow
/// down overload resolution for every other use.
///
////////////////////////////////////////////////////////////
template <typename T>
T&& declVal() noexcept;

} // namespace za

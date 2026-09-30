#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief `std::ptrdiff_t` equivalent obtained without including `<cstddef>`
///
////////////////////////////////////////////////////////////
using PtrDiffT = decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));

} // namespace za

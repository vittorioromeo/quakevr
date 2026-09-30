#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief `std::size_t` equivalent obtained without including `<cstddef>`
///
////////////////////////////////////////////////////////////
using SizeT = decltype(sizeof(int));

} // namespace za

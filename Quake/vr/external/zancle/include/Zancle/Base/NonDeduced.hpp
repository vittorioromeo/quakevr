#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Intentionally suppress function template argument deduction
///
////////////////////////////////////////////////////////////
template <typename T>
struct NonDeduced
{
    using type = T;
};

} // namespace za

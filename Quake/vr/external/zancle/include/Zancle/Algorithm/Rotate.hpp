#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Swap.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Rotate `[first, last)` so that `*middle` becomes the new first element
///
/// \return Iterator to the new position of the element originally at `first`
///         (i.e. `first + (last - middle)`)
///
////////////////////////////////////////////////////////////
template <typename ForwardIt>
constexpr ForwardIt rotate(ForwardIt first, ForwardIt middle, const ForwardIt last)
{
    if (first == middle)
        return last;

    if (middle == last)
        return first;

    ForwardIt read = middle;

    do
    {
        iterSwap(first++, read++);

        if (first == middle)
            middle = read;
    } while (read != last);

    ForwardIt ret = first;

    for (read = middle; read != last;)
    {
        iterSwap(first++, read++);

        if (first == middle)
            middle = read;
        else if (read == last)
            read = middle;
    }

    return ret;
}

} // namespace za

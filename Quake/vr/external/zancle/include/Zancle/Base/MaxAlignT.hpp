#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief `std::max_align_t` replacement that does not require `<cstddef>`
///
/// Has the strictest fundamental alignment of any scalar type, suitable
/// as the alignment for raw storage that may hold any standard-layout
/// object (e.g. inside `InPlacePImpl`).
///
////////////////////////////////////////////////////////////
struct MaxAlignT
{
    long long   a [[gnu::aligned(alignof(long long))]];
    long double b [[gnu::aligned(alignof(long double))]];
#if defined(__i386__)
    __float128 c [[gnu::aligned(alignof(__float128))]];
#endif
};

} // namespace za

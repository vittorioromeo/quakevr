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
// Quake VR (local change): only where the target has __float128 (32-bit x86 Windows, clang-cl's i686-pc-windows-msvc,
// has not; Quake/vr/external/zancle/README.md).
#if defined(__i386__) && defined(__SIZEOF_FLOAT128__)
    __float128 c [[gnu::aligned(alignof(__float128))]];
#endif
};

} // namespace za

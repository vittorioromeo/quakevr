#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \brief `std::launder` replacement, prefers `__builtin_launder` to avoid `<new>`
///
/// `std::launder` tells the optimizer that a pointer obtained by some
/// other means actually points to a freshly constructed object. It is
/// required after placement-new into reused storage to make pointer
/// dereferences well-defined.
///
////////////////////////////////////////////////////////////
#if __has_builtin(__builtin_launder)

    #define ZA_LAUNDER __builtin_launder

#else

    #include <new>

    #define ZA_LAUNDER ::std::launder

#endif


////////////////////////////////////////////////////////////
/// \brief Combined `reinterpret_cast` + `launder` for typed buffer access
///
/// Convenience macro: `ZA_LAUNDER_CAST(T*, buffer)` is the
/// laundered equivalent of `reinterpret_cast<T*>(buffer)`.
///
////////////////////////////////////////////////////////////
#define ZA_LAUNDER_CAST(type, buffer) ZA_LAUNDER(reinterpret_cast<type>(buffer))

#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \brief Apply Clang's `[[clang::trivial_abi]]` attribute when supported
///
/// `trivial_abi` lets a non-trivial type be passed in registers (and
/// memcpy'd in calling conventions) by ignoring its non-trivial
/// special members at the ABI boundary.
///
////////////////////////////////////////////////////////////
#ifdef __clang__

    #define ZA_TRIVIAL_ABI [[clang::trivial_abi]]

#else

    #define ZA_TRIVIAL_ABI

#endif

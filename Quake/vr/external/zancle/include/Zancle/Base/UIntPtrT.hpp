#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
#ifdef __UINTPTR_TYPE__
using UIntPtrT = __UINTPTR_TYPE__;
#else
    #error "Could not determine a uintptr equivalent type (GCC/Clang)."
#endif

} // namespace za

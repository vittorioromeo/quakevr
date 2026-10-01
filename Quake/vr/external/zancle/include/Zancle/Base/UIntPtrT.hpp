#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
#if defined(__UINTPTR_TYPE__) // GCC, Clang
using UIntPtrT = __UINTPTR_TYPE__;
#elif defined(_WIN64) // MSVC
using UIntPtrT = unsigned long long;
#elif defined(_WIN32) // MSVC
using UIntPtrT = unsigned int;
#else
    #error "Could not determine a uintptr equivalent type"
#endif

} // namespace za

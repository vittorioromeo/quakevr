#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
#ifdef __INT64_TYPE__ // GCC and Clang (including clang-cl and Emscripten)

////////////////////////////////////////////////////////////
// 8 bits integer types
using I8 = __INT8_TYPE__;
using U8 = __UINT8_TYPE__;


////////////////////////////////////////////////////////////
// 16 bits integer types
using I16 = __INT16_TYPE__;
using U16 = __UINT16_TYPE__;


////////////////////////////////////////////////////////////
// 32 bits integer types
using I32 = __INT32_TYPE__;
using U32 = __UINT32_TYPE__;


////////////////////////////////////////////////////////////
// 64 bits integer types
using I64 = __INT64_TYPE__;
using U64 = __UINT64_TYPE__;


#else


// Assume standard Windows data models (ILP32, LLP64) where:
// char = 1, short = 2, int = 4, long = 4, long long = 8

////////////////////////////////////////////////////////////
// 8 bits integer types
using I8 = signed char;
using U8 = unsigned char;


////////////////////////////////////////////////////////////
// 16 bits integer types
using I16 = short;
using U16 = unsigned short;


////////////////////////////////////////////////////////////
// 32 bits integer types
using I32 = int;
using U32 = unsigned int;


////////////////////////////////////////////////////////////
// 64 bits integer types
using I64 = long long;
using U64 = unsigned long long;


#endif

} // namespace za


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Fixed-width integer aliases (`I8`/`U8` ... `I64`/`U64`)
///
/// `<cstdint>` equivalents that do not require including the header.
/// On GCC and Clang the types are the compiler-predefined `__INTn_TYPE__`
/// and `__UINTn_TYPE__`, i.e. exactly the same types as `std::intN_t` and
/// `std::uintN_t` (e.g. `U64` is `unsigned long` on LP64 Linux, but
/// `unsigned long long` on Windows, macOS, and Emscripten). On other
/// compilers (e.g. MSVC) the standard ILP32/LLP64 data model is assumed.
///
////////////////////////////////////////////////////////////

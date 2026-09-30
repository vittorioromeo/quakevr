#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Portable lifetime annotations for dangling detection
///
/// These attributes only enable compiler diagnostics (e.g. Clang's
/// `-Wdangling` and `-Wdangling-gsl`) and never affect code generation.
/// They expand to nothing on compilers that do not support them.
///
/// - `ZA_LIFETIMEBOUND`: the result of a function refers to the annotated
///   parameter (or, placed after the function's qualifiers, to `*this`).
///   Use it on owning types' members returning pointers/references into
///   the object, e.g. `const char* c_str() const ZA_LIFETIMEBOUND`.
///
/// - `ZA_GSL_OWNER(T)` / `ZA_GSL_POINTER(T)`: mark a class as owning
///   (e.g. `Vector`, `String`) or referring to (e.g. `Span`, `StringView`)
///   objects of type `T`. Constructing a pointer type from a temporary owner
///   is diagnosed, whereas constructing it from a temporary pointer type
///   (e.g. `Span<int>` to `Span<const int>`) is not -- unlike annotating a
///   generic converting constructor with `ZA_LIFETIMEBOUND`.
///
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
#if __has_cpp_attribute(clang::lifetimebound)

    #define ZA_LIFETIMEBOUND [[clang::lifetimebound]]

#else

    #define ZA_LIFETIMEBOUND

#endif


////////////////////////////////////////////////////////////
#if __has_cpp_attribute(gsl::Owner) && __has_cpp_attribute(gsl::Pointer)

    #define ZA_GSL_OWNER(...)   [[gsl::Owner(__VA_ARGS__)]]
    #define ZA_GSL_POINTER(...) [[gsl::Pointer(__VA_ARGS__)]]

#else

    #define ZA_GSL_OWNER(...)
    #define ZA_GSL_POINTER(...)

#endif

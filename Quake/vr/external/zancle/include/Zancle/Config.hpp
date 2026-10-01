#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Zancle version
////////////////////////////////////////////////////////////
#define ZA_VERSION_MAJOR      3
#define ZA_VERSION_MINOR      1
#define ZA_VERSION_PATCH      0
#define ZA_VERSION_IS_RELEASE false


////////////////////////////////////////////////////////////
// Identify the operating system
// see https://sourceforge.net/p/predef/wiki/Home/
////////////////////////////////////////////////////////////
#if defined(_WIN32)

    // Windows
    #define ZA_SYSTEM_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

#elif defined(__APPLE__) && defined(__MACH__)

    // Apple platform, see which one it is
    #include "TargetConditionals.h"

    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define ZA_SYSTEM_IOS

    #elif TARGET_OS_MAC
        #define ZA_SYSTEM_MACOS

    #else

        // Unsupported Apple system
        #error This Apple operating system is not supported by Zancle library

    #endif

#elif defined(__unix__)

    // UNIX system, see which one it is
    #if defined(__ANDROID__)
        #define ZA_SYSTEM_ANDROID

    #elif defined(__linux__)
        #define ZA_SYSTEM_LINUX
        #define ZA_SYSTEM_LINUX_OR_BSD

    #elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        #define ZA_SYSTEM_FREEBSD
        #define ZA_SYSTEM_LINUX_OR_BSD

    #elif defined(__OpenBSD__)
        #define ZA_SYSTEM_OPENBSD
        #define ZA_SYSTEM_LINUX_OR_BSD

    #elif defined(__NetBSD__)
        #define ZA_SYSTEM_NETBSD
        #define ZA_SYSTEM_LINUX_OR_BSD

    #elif defined(__EMSCRIPTEN__)
        #define ZA_SYSTEM_EMSCRIPTEN

    #else

        // Unsupported UNIX system
        #error This UNIX operating system is not supported by Zancle library

    #endif

#else

    // Unsupported system
    #error This operating system is not supported by Zancle library

#endif


////////////////////////////////////////////////////////////
// Ensure minimum C++ language standard version is met
////////////////////////////////////////////////////////////
// MSVC reports the standard via `_MSVC_LANG` (`__cplusplus` needs `/Zc:__cplusplus`); clang-cl
// defines `_MSVC_LANG` too, but with an older value, and reports the standard via `__cplusplus`
// clang-format off
#if defined(_MSC_VER) && !defined(__clang__)
    #define ZA_PRIV_CPLUSPLUS _MSVC_LANG
#else
    #define ZA_PRIV_CPLUSPLUS __cplusplus
#endif

#if ZA_PRIV_CPLUSPLUS < 202302L
    #error "Enable C++23 or newer for your compiler (e.g. -std=c++23 for GCC/Clang or /std:c++23 for MSVC)"
#endif

#undef ZA_PRIV_CPLUSPLUS
// clang-format on


////////////////////////////////////////////////////////////
// Portable debug macro
////////////////////////////////////////////////////////////
#if !defined(NDEBUG)

    #define ZA_DEBUG

#endif


////////////////////////////////////////////////////////////
// Helpers to create portable import / export macros for each module
////////////////////////////////////////////////////////////
#if !defined(ZA_STATIC)

    #if defined(ZA_SYSTEM_WINDOWS)

        // Windows compilers need specific (and different) keywords for export and import
        #define ZA_API_EXPORT __declspec(dllexport)
        #define ZA_API_IMPORT __declspec(dllimport)

        // For Visual C++ compilers, we also need to turn off this annoying C4251 & C4275 warning
        #ifdef _MSC_VER

            #pragma warning(disable : 4251) // Using standard library types in our own exported types is okay
            #pragma warning(disable : 4275) // Exporting types derived from the standard library is okay

        #endif

    #else // Linux, FreeBSD, macOS

        // GNU attribute syntax rather than `[[gnu::visibility]]`: like `__declspec`, it may appear among
        // the decl-specifiers (e.g. `static ZA_SYSTEM_API void f();`), where GCC rejects standard attributes
        #define ZA_API_EXPORT __attribute__((visibility("default")))
        #define ZA_API_IMPORT __attribute__((visibility("default")))

    #endif

#else

    // Static build doesn't need import/export macros
    #define ZA_API_EXPORT
    #define ZA_API_IMPORT

#endif


////////////////////////////////////////////////////////////
// Portable import / export macro for the aggregate `zancle-system` library
// (i.e. all utility modules: Base, Trait, Math, Diagnostic, Vocabulary,
// Container, Algorithm, String, Geometry, Chrono, Concurrency, Fmt, IO,
// Scn, Err, Lifetime, Mixin). Defined here in Config.hpp so callers don't
// need to pull in a per-module Export.hpp.
////////////////////////////////////////////////////////////
#if defined(ZA_SYSTEM_EXPORTS)
    #define ZA_SYSTEM_API ZA_API_EXPORT
#else
    #define ZA_SYSTEM_API ZA_API_IMPORT
#endif


////////////////////////////////////////////////////////////
// ODR Violation Detection Mechanism
////////////////////////////////////////////////////////////
extern "C"
{
#ifdef ZA_ENABLE_LIFETIME_TRACKING
    extern ZA_SYSTEM_API void zancleInternalAbiCheckLifetimeTrackingEnabled();
#else
    extern ZA_SYSTEM_API void zancleInternalAbiCheckLifetimeTrackingDisabled();
#endif
}


namespace za::priv
{
////////////////////////////////////////////////////////////
#ifdef ZA_ENABLE_LIFETIME_TRACKING
inline void (*const abiCheckFuncPtr)() = &zancleInternalAbiCheckLifetimeTrackingEnabled;
#else
inline void (*const abiCheckFuncPtr)() = &zancleInternalAbiCheckLifetimeTrackingDisabled;
#endif

////////////////////////////////////////////////////////////
struct [[maybe_unused]] ABICheckTrigger
{
    ABICheckTrigger()
    {
        [[maybe_unused]] volatile auto* dummy = reinterpret_cast<void*>(abiCheckFuncPtr);
    }
};

////////////////////////////////////////////////////////////
[[maybe_unused]] inline volatile za::priv::ABICheckTrigger abiCheckTrigger;

} // namespace za::priv

# Zancle (vendored: the modules Quake VR uses)

Vittorio Romeo's C++23 library (a fork of SFML grown into a general toolkit). zlib licence for the SFML-derived parts
and the author's own (`license.md`); moodycamel's queue is under the simplified BSD licence (the header of
`extlibs/moodycamel/concurrentqueue.h`), its semaphore under zlib (`lightweightsemaphore.h`).

- Upstream: https://github.com/vittorioromeo/zancle, branch `rebrand_to_zancle`
- Commit: `6b8c610639544ac69dcc071c8e64458adee8588b` (2026-09-28, "Fix anchor point mixin setters, docs, and add tests")

The Quake VR code is written on Zancle instead of the C++ standard library (`docs/vr-port/ROUND21.md`, "Zancle
migration"; `docs/vr-port/CODE_STYLE.md`, "Zancle, not the standard library"). Vendored: the headers the VR code
includes and every header they reach, each reached header's source file, and moodycamel's three headers (upstream's
`extlibs/headers/moodycamel/`, here `extlibs/moodycamel/`): 159 files, about 1.2 MB.

- `include/Zancle/`: `Algorithm` (Sort, Find, AnyOf, Count, Copy, Erase, Remove, Rotate, Unique, MaxElement),
  `Base` (the builtin macros: Memcpy, Strcmp, BitCast, IsNan..., Assert, Macros, Swap, IntTypes, SizeT...),
  `Chrono` (Clock, Time), `Concurrency` (Atomic, AtomicMutex, LockGuard, Thread, ThreadPool), `Container` (Vector,
  SmallVector, InPlaceVector, Array, Bitset, AnkerlUnorderedDense), `Math` (the math wrappers, MinMax, Clamp, the
  float limits), `String` (String, StringView, ToString, ToChars, FromChars), `Trait`, `Vocabulary` (Optional, Span,
  UniquePtr, FunctionRef, FixedFunction, InPlacePImpl), `Fmt/FmtAppendMixinFwd.hpp`, `Config.hpp`.
- `src/Zancle/`: `Base/{Abort,Assert}.cpp` (and `StackTrace.hpp`), `Chrono/Clock.cpp`, `Concurrency/*.cpp`,
  `String/String.cpp`, `Vocabulary/Optional.cpp`, `Lifetime/LifetimeTrackingABICheck.cpp` (the symbol `Config.hpp`'s ABI
  check references).

The list is made by following the includes (a header's `#include`s, and its source file's if it has one), from the
Zancle headers that `Quake/vr`'s files include; nothing else is kept. The whole engine is built as C++23 (its C files
as C) by GCC or Clang, on Windows by clang-cl (Zancle is written against the GCC/Clang builtins: `__atomic_*`,
`__has_builtin`), with Zancle's `include` on the include path and `ZA_STATIC` defined.

Build (every build file compiles the same set):

- Zancle's own sources: a static library, C++23, `ZA_STATIC`: `Windows/VisualStudio/zancle.vcxproj` (the ClangCL
  toolset, as the engine; the same runtime library as ironwail per configuration; ThinLTO in Release, as the engine),
  which `quakevr.props` references; CMake's `qvr_zancle`; the Makefiles' `ZANCLE_CXXFLAGS`. Links `synchronization.lib`
  (WaitOnAddress).
  - Release: optimised, `NDEBUG` (Zancle's asserts off).
  - Debug, by the switch `QVR_ZANCLE_DEBUG` (MSBuild property, CMake option, Makefile variable; `docs/BUILDING.md`):
    on (the default while the migration settles), built as the engine's Debug (unoptimised, Zancle's asserts on, the
    define `QVR_ZANCLE_DEBUG`); off, optimised with its asserts off, as in Release.
- In the engine's own files Zancle's headers follow the engine's settings: its asserts follow the engine's `NDEBUG`
  (on in Debug). A failure calls `Quake/vr/vr_zancle.cpp`'s handler (a Quake error; in a test run, the crash report
  with its stack), for the engine's files and, with `QVR_ZANCLE_DEBUG`, the library's.
- CMake (`Quake/vr/vr.cmake`): on Windows configure with `-T ClangCL` (plain MSVC stops with a message). The Makefiles:
  `-pthread` or `-lsynchronization`; the engine's C++ files are `-std=c++23 -DZA_STATIC` with Zancle's `include`.

Local changes (marked `Quake VR (local change)` in the file):

- `include/Zancle/Config.hpp`: the C++23 check reads `__cplusplus` under clang-cl, which sets `_MSVC_LANG` to 202004L in
  C++23 mode (as MSVC's `/std:c++latest` did) and so was rejected. Proposed upstream (ROUND21.md, "Zancle proposals").
- `include/Zancle/Base/MaxAlignT.hpp`: its `__float128` member on 32-bit x86 only where the target has the type
  (`__SIZEOF_FLOAT128__`): clang-cl's 32-bit Windows target (ironwail's Win32 configurations) has not, and stopped there.
- `include/Zancle/Base/InitializerList.hpp`: the real `<initializer_list>` under `_MSC_VER` too. Zancle's own
  `std::initializer_list` is a pointer and a size; MSVC's STL's is two pointers, and a program that also uses MSVC's STL
  got both layouts in different files (an ODR violation: a caller built with one layout passed a pointer as the size of
  an instantiation built with the other; heap corruption in Release, a 2 TB allocation in a Debug static initialiser).
- `src/Zancle/Base/Assert.cpp`: no handler with `QVR_ZANCLE_DEBUG` (the engine's `vr_zancle.cpp` is the one).

To update: copy the same files from upstream over these, keep the local changes unless upstream took them, and change
the commit above. `python Misc/quakevr/zancle_vendor.py <upstream checkout>` adds what the VR code's includes reach and
is missing (and with named headers, those); `--list-unused` lists what nothing reaches any more (to delete).

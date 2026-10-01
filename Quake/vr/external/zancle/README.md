# Zancle (vendored: the modules Quake VR uses)

Vittorio Romeo's C++23 library (a fork of SFML grown into a general toolkit). zlib licence for the SFML-derived parts
and the author's own (`license.md`); moodycamel's queue is under the simplified BSD licence (the header of
`extlibs/moodycamel/concurrentqueue.h`), its semaphore under zlib (`lightweightsemaphore.h`).

- Upstream: https://github.com/vittorioromeo/zancle, branch `rebrand_to_zancle`
- Commit: `534219bfe992fb655b63e835556ca69a189440fa` (2026-10-01, "Add nthElement, stableSort, upperBound, Pair, and
  container ordering"); no local changes: the files are upstream's.

The Quake VR code is written on Zancle instead of the C++ standard library (`docs/vr-port/ROUND21.md`, "Zancle
migration"; `docs/vr-port/CODE_STYLE.md`, "Zancle, not the standard library"). Vendored: the headers the VR code
includes and every header they reach, each reached header's source file, and moodycamel's three headers (upstream's
`extlibs/headers/moodycamel/`, here `extlibs/moodycamel/`): 186 files, about 1.5 MB.

- `include/Zancle/`: `Algorithm` (Sort, StableSort, NthElement, Find, AnyOf, Count, Copy, Erase, Remove, Rotate,
  Unique, MaxElement, Fill, Iota, Replace, LowerBound, UpperBound, StablePartition), `Base` (the builtin macros:
  Memcpy, Strcmp, BitCast, IsNan, IsInf, IsFinite..., Limits, Assert, Macros, Swap, IntTypes, SizeT, ReverseIterator...),
  `Chrono` (Clock, with nanosecond readings, and Time), `Concurrency` (Atomic, AtomicMutex, LockGuard, Thread,
  ThreadPool), `Container` (Vector, SmallVector, InPlaceVector, Array, Bitset, AnkerlUnorderedDense), `Math` (the math
  wrappers, with Abs, Hypot, Cbrt, Log2, Exp2, Llround, Copysign, Trunc; MinMax, Clamp), `Random` (FastNonCryptoRng and
  its xoroshiro128++ generator), `String` (String, StringView, ToString, ToChars, FromChars), `Trait`, `Vocabulary`
  (Optional, Span, UniquePtr, Pair, FunctionRef, FixedFunction, InPlacePImpl), `Fmt/FmtAppendMixinFwd.hpp`,
  `Config.hpp`.
- `src/Zancle/`: `Base/{Abort,Assert,StackTrace}.cpp`, `Chrono/Clock.cpp`, `Concurrency/*.cpp`,
  `String/String.cpp`, `Vocabulary/Optional.cpp`, `Lifetime/LifetimeTrackingABICheck.cpp` (the symbol `Config.hpp`'s ABI
  check references).

The list is made by following the includes (a header's `#include`s, and its source file's if it has one, a private
`src/` header's too), from the
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
    on (the default while the migration settles), built as the engine's Debug (unoptimised, Zancle's asserts on; the
    engine gets the define `QVR_ZANCLE_DEBUG`); off, optimised with its asserts off, as in Release.
- In the engine's own files Zancle's headers follow the engine's settings: its asserts follow the engine's `NDEBUG`
  (on in Debug). A failure calls `Quake/vr/vr_zancle.cpp`'s handler (a Quake error; in a test run, the crash report
  with its stack), for the engine's files and, with `QVR_ZANCLE_DEBUG`, the library's: installed at startup with
  `za::setAssertHandler` (`VR_InstallCrashHandler`). Without `QVR_ZANCLE_DEBUG` the library has no assert function
  (`Assert.cpp` defines it with `ZA_DEBUG` only) and `vr_zancle.cpp` defines it for the engine's Debug files.
  `vr_debug_crash assert` / `zassert` (Debug > Crashes) fail one, in the engine's code or the library's.
- CMake (`Quake/vr/vr.cmake`): on Windows configure with `-T ClangCL` (plain MSVC stops with a message). The Makefiles:
  `-pthread` or `-lsynchronization`; the engine's C++ files are `-std=c++23 -DZA_STATIC` with Zancle's `include`.

Local changes: none since 534219bf. The last ones went upstream at `fad225a4`: `MaxAlignT.hpp`'s `__float128` guard
(`__SIZEOF_FLOAT128__`, for 32-bit clang-cl), `InitializerList.hpp`'s real `<initializer_list>` under `_MSC_VER` (two
layouts of one type had corrupted memory: ROUND21.md, "Zancle update to 4ed9c3cc"), and the assert hook
(`za::setAssertHandler`, in place of QVR's edit to `Assert.cpp`). `Config.hpp`'s clang-cl C++23 check went upstream at
4ed9c3cc.

To update: `python Misc/quakevr/zancle_vendor.py --update <upstream checkout>` copies every vendored file upstream
changed and adds what the VR code's includes now reach (without `--update`, only what is missing; with named headers,
those); `--list-unused` lists what nothing reaches any more (to delete). Then change the commit above.

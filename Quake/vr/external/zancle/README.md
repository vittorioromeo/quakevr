# Zancle (vendored: the concurrency module)

Vittorio Romeo's C++23 library (a fork of SFML grown into a general toolkit). zlib licence for the SFML-derived parts
and the author's own (`license.md`); moodycamel's queue is under the simplified BSD licence (the header of
`extlibs/moodycamel/concurrentqueue.h`), its semaphore under zlib (`lightweightsemaphore.h`).

- Upstream: https://github.com/vittorioromeo/zancle, branch `rebrand_to_zancle`
- Commit: `6b8c610639544ac69dcc071c8e64458adee8588b` (2026-09-28, "Fix anchor point mixin setters, docs, and add tests")

Only the concurrency module and what it includes are vendored (68 files, about 440 KB, 150 KB of which is moodycamel's
queue): `include/Zancle/Concurrency/*` and the headers they reach (`Base`, `Trait`, `Vocabulary/FixedFunction`,
`Vocabulary/InPlacePImpl`, `Container/Vector`, `Chrono/Time`, `Math/MinMaxMacros`, `Config.hpp`), the sources
`src/Zancle/Concurrency/*.cpp`, `src/Zancle/Base/{Abort,Assert}.cpp` and
`src/Zancle/Lifetime/LifetimeTrackingABICheck.cpp` (the symbol `Config.hpp`'s ABI check references), and moodycamel's
three headers (upstream's `extlibs/headers/moodycamel/`, here `extlibs/moodycamel/`). The list was made by following
the includes from the concurrency module (the files `#include`d and each header's source file).

Quake VR uses it through `Quake/vr/vr_jobs.hpp` (the game's thread pool, `docs/vr-port/ROUND21.md`, "The game's thread
pool"). The whole engine is built as C++23 (its C files as C) by GCC or Clang, on Windows by clang-cl (Zancle is
written against the GCC/Clang builtins: `__atomic_*`, `__has_builtin`), so any Quake VR file may include Zancle's
headers (`Quake/vr/external/zancle/include` is on the engine's include path, `ZA_STATIC` defined).

Build (every build file compiles the same set):

- Zancle's own sources: a static library, C++23, `NDEBUG` (Zancle's asserts off) and `ZA_STATIC`, optimised (`/O2`,
  `-O2`) in every configuration, Debug too: `Windows/VisualStudio/zancle.vcxproj` (the ClangCL toolset, as the engine;
  the same runtime library as ironwail per configuration; ThinLTO in Release, as the engine), which `quakevr.props`
  references. Links `synchronization.lib` (WaitOnAddress).
- In the engine's own files Zancle's asserts follow the engine's `NDEBUG`: on in Debug, where a failure calls
  `Quake/vr/vr_zancle.cpp`'s handler (a Quake error, after a break into an attached debugger), as the library's
  `Assert.cpp` defines none without `ZA_DEBUG`.
- CMake (`Quake/vr/vr.cmake`): the `qvr_zancle` static library; on Windows configure with `-T ClangCL` (plain MSVC stops
  with a message). The Makefiles: `ZANCLE_CXXFLAGS` (`-std=c++23 -O2 -DNDEBUG -DZA_STATIC`), `-pthread` or
  `-lsynchronization`; the engine's C++ files are `-std=c++23 -DZA_STATIC` with Zancle's `include`.

Local changes (marked `Quake VR (local change)` in the file):

- `include/Zancle/Config.hpp`: the C++23 check reads `__cplusplus` under clang-cl, which sets `_MSVC_LANG` to 202004L in
  C++23 mode (as MSVC's `/std:c++latest` did) and so was rejected. Proposed upstream (ROUND21.md, "Zancle proposals").
- `include/Zancle/Base/MaxAlignT.hpp`: its `__float128` member on 32-bit x86 only where the target has the type
  (`__SIZEOF_FLOAT128__`): clang-cl's 32-bit Windows target (ironwail's Win32 configurations) has not, and stopped there.

To update: copy the same files from upstream over these (following the includes again if the module's dependencies
changed), keep the local change unless upstream took it, and change the commit above.

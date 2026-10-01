# Zancle: issues and proposals from the Quake VR migration

For Vittorio Romeo, Zancle's author. This recaps every Zancle bug, portability gap, missing feature and API proposal
found while moving Quake VR's VR code (`Quake/vr`) to Zancle. Each item gives what it is, where it was found, the
evidence, the proposed fix and its status. Sources: `docs/vr-port/ROUND21.md` (cited by section),
`C:\OHWorkspace\qvr-kit\ZANCLE_MSVC.md` (cited as "MSVC audit, Rn"), `docs/vr-port/CODE_STYLE.md`,
`Quake/vr/vr_zancle.hpp`, `Quake/vr/external/zancle/README.md` and `Misc/quakevr/zancle_vendor.py`.

Status keys: **fixed upstream**, **worked around in QVR** (a local change or a `qza` shim), **open** (nothing done
upstream).

## Summary

- **Vendored commit:** `534219bfe992fb655b63e835556ca69a189440fa` (branch `rebrand_to_zancle`, 2026-10-01): 186
  files, about 1.5 MB (`external/zancle/README.md`; before it, `4ed9c3cc`, 178 files). It was first vendored at `6b8c6106` (2026-09-28), the concurrency module
  only (ROUND21, "Zancle, vendored"). `zancle_vendor.py` copies the include closure of what `Quake/vr` uses. It keeps
  files that are already there unless run with `--update`, which takes every file upstream changed.
- **Local changes: none since 534219bf.** The three kept at 4ed9c3cc went upstream at `fad225a4`: `Base/InitializerList.hpp`
  (B1), `Base/MaxAlignT.hpp` (B6) and `src/Zancle/Base/Assert.cpp` (A10, now `za::setAssertHandler`). `Config.hpp` went
  at 4ed9c3cc (B7).
- **Upstream since 4ed9c3cc** (ROUND21, "Zancle update to 534219bf"): `fad225a4` (B1, B2, B3, B5, B6, A10), `8bb74612`
  (A2, A3, A6; M1-M3, M5, M6, M8, M11, M15, parts of A4 and A5), `00019de8` (docs: N3, N4, M9), `dbd1cfa4` (M4),
  `8c3ac3b2` (A8), `534219bf` (M7, M10, M13, M14). Open: B4, M9 (a node-stable map), M12, A1, A9, P2-P13 but P8.
- **Scale** (ROUND21, "Zancle migration"): `std::` uses in `Quake/vr` went from 7454 to 60, and standard headers from
  708 to 16. After the follow-ups, 48 uses and 8 headers remain, and 19 `ZANCLE-TODO`s (ROUND21, "Zancle
  follow-ups"). The work took 18 commits over 169 files, about +9.5k/-8.4k lines, plus `vr_zancle.hpp` (394 lines)
  and `vr_files` (568 lines). Behaviour stayed identical: the melee eval matches byte for byte, the grasp sweep bit
  for bit, and the probes, menu dump and sound test match.
- **Verdict** (ROUND21, "Was the Zancle migration worth it?"):
  - Worth it for development: Debug CPU per frame is -25% (3.38 -> 2.53 ms), and rebuilding one file is about a
    fifth faster. Zancle's asserts are now on in Debug.
  - Neutral for players: Release CPU per frame is within noise, and the Release exe is 14% smaller.
  - Costs: churn, an unfamiliar API, 4 bugs introduced by the migration (all fixed before the merge), and a
    dependency to maintain.
  - Zancle bugs found: 1 real bug (B1) and 2 traps (B3, A1). A second real bug (B2) turned up later, in "Zancle
    update to 4ed9c3cc".

## 1. Bugs

**B1. `std::initializer_list` ODR clash with MSVC's STL** (`Base/InitializerList.hpp`)
- **What:**
  - Zancle defines its own `std::initializer_list` (a pointer and a size) when `<initializer_list>` was not
    included. MSVC's STL defines it as two pointers.
  - A file that includes Zancle first and a file that includes the STL first get two layouts of the same
    `std::initializer_list<T>`. The linker keeps one copy of each inline user, such as a container's braced-list
    constructor.
- **Where found:** ROUND21, "Zancle migration" ("Two bugs found on the way"). Symptoms: a 2 TB allocation in a Debug
  static initialiser, and heap corruption in Release.
- **Upstream's fix is not correct:**
  - Upstream `13db45220` / `4ed9c3cc` keeps its own type, defines the STL's guard `_INITIALIZER_LIST_`, and adds a
    `(first, last)` constructor for cl.
  - The guard only decides which definition a file gets: the one whose header came first. Both layouts still exist
    in one program.
  - Standalone test (ROUND21, "The initializer_list fix upstream: not correct", scratchpad `iltest/`): with
    upstream's header, clang-cl `/O2` saw a size of 612011736004, cl `/O2` saw 70620542364, and both compilers
    crashed at `/Od` (0xC0000409).
  - `sizeof` (16) and `alignof` (8) agree in every case, so a size check cannot catch it.
- **Proposed fix:**
  - Preferred: under `_MSC_VER`, include the real `<initializer_list>`. MSVC's header is small (`<yvals_core.h>` and
    `<cstddef>`). This is QVR's local change, and it passes on clang-cl and cl at `/O2` and `/Od`.
  - Alternative: keep Zancle's own type but lay it out exactly as MSVC's (two pointers, with `begin`/`end`/`size`
    derived from them, plus the `(first, last)` constructor). This also passes every case.
  - Also: under cl, the header's `[[gnu::always_inline]]` and `[[gnu::pure]]` warn C5030 (see P6).
- **Status:** **fixed upstream** (`fad225a4`): the real header under `_MSC_VER` (cl and clang-cl), the preferred fix
  above. QVR's local change dropped. The standalone test again (two files, one including Zancle's header first, one
  MSVC's STL first; clang-cl x64): upstream's header passes at `/O2` and `/Od`, 4ed9c3cc's still crashes
  (0xC0000409); the engine in Release and Debug (`QVR_ZANCLE_DEBUG`) runs the maps, the eval and the self-test clean.

**B2. `za::remainder` is not `std::remainder`** (Math)
- **What:**
  - Zancle's version truncates the quotient, giving fmod's result, and divides through an `int`, so it is wrong
    past 2^31 quotients. `std::remainder` rounds the quotient to the nearest (IEEE).
  - The migration's angle wraps `za::remainder(x, 360.f)` returned -360..360 instead of -180..180. Affected: the
    avatar's feet turn, snap turn, motion playback yaw, review lerp, panel follow, profiler HUD, view angles and the
    weight's twist.
- **Where found:** ROUND21, "Zancle update to 4ed9c3cc" (and noted earlier in the round, just before "Box3D on the
  pool").
- **Evidence:** `vr_zancle_math_test` ran 102625 checks against std; `za::remainder` differs in 89 of its 198 cases,
  and every other migrated math function matched to the last bit.
- **Proposed fix:** either rename it (`truncatedRemainder`) or make it the IEEE one (`__builtin_remainder*`), and
  drop the `int` division.
- **Status:** **fixed upstream** (`fad225a4`): `za::remainder` is IEEE (the builtin), the old one is
  `za::truncatedRemainder`. QVR's 13 angle wraps (all on `qza::remainder`, the IEEE one, since 4ed9c3cc) use
  `za::remainder`: the same results (the self-test: every half degree in -1080..1080, and the edge values, bit for bit
  as `std::remainder`); `qza::remainder` gone. No call wanted truncation: `vr_torso.cpp`'s `wrapYaw` rounds the
  quotient half away from zero (as written before the migration), kept.

**B3. `vectorEraseIf` accepts an ankerl map and corrupts it** (Algorithm / Container)
- **What:** the codemod turned `std::erase_if(map, ...)` into `za::vectorEraseIf`. That compiles on
  `ankerl::unordered_dense::map`, because the map has `begin`/`end`/`erase`, but it moves the values without their
  buckets.
- **Where found:** ROUND21, "Zancle migration" (decals' gib trails and bullet holes). Zancle's Debug asserts caught
  it ("index past the end", 2 runs out of 22).
- **Proposed fix:** constrain `vectorEraseIf` to vectors, or give `za::` a single `eraseIf` that dispatches to the
  map's own `erase_if`.
- **Status:** **fixed upstream** (`fad225a4`): `vectorEraseIf` requires `data()` (contiguous). QVR's 10 calls are all
  on `za::Vector`s; the maps use ankerl's `erase_if`.

**B4. `ThreadPool::parallelFor` waits by running unrelated tasks** (Concurrency)
- **What:**
  - While a helper it posted has not started, the caller runs whatever is pending (`tryRunPendingTask`). A
    frame-critical caller can pick up a long, unrelated task, such as a model's occlusion bake (tens of ms), and
    miss the frame.
  - The caller also waits for every helper, even after all chunks are taken.
- **Where found:** ROUND21, "Zancle proposals (for upstream)" #5 (thread-pool round).
- **Proposed fix:**
  - Helpers hold a small refcounted, poolable control block instead of the caller's frame.
  - When the chunks run out, the caller closes a gate: an atomic holding the count of helpers inside plus a closed
    bit. It then waits only for the helpers already inside.
  - A helper that starts late sees the gate closed and just releases the block.
  - Nested calls stay safe, and nothing allocates once warm.
  - Reference implementation: `qvr::jobs::Pool::parallelFor` (`Quake/vr/vr_jobs.cpp`, about 80 lines, built on
    `postCopies`).
- **Status:** worked around in QVR (its own `parallelFor`). Open upstream (`fad225a4` made it `noexcept` only).

**B5. An exception out of `parallelFor`'s body is undefined behaviour** (Concurrency)
- **What:**
  - Thrown on the calling thread, it unwinds the frame the helpers still read (`nextChunk`, `helpersRemaining`,
    `f`).
  - Thrown on a worker, it calls `std::terminate`, because the worker loop calls `task()` bare.
- **Where found:** ROUND21, "Zancle proposals (for upstream)" #6.
- **Proposed fix:**
  - Catch per chunk, keep the lowest chunk's exception (deterministic), and rethrow after the join.
  - Document that `post`ed tasks must not throw, or catch and report in the worker loop.
  - At minimum, mark the pool's entry points `noexcept`, as QVR did.
- **Status:** **fixed upstream** (`fad225a4`): `parallelFor`, `tryRunPendingTask` and the worker loop are `noexcept`,
  tasks documented as not allowed to throw. Nothing to change in QVR (built without exceptions).

**B6. `MaxAlignT.hpp`'s `__float128` member breaks 32-bit clang-cl** (Base)
- **What:** the member is used on 32-bit x86 without checking that the target has the type. Ironwail's Win32
  configurations stopped there, with MSVC too.
- **Where found:** ROUND21, "clang-cl: the whole engine, C++23 (trial)"; `external/zancle/README.md`.
- **Proposed fix:** guard the member with `__SIZEOF_FLOAT128__`, as the local change does.
- **Status:** **fixed upstream** (`fad225a4`); QVR's local change dropped. (Not rebuilt for Win32 here: x64 only.)

**B7. `Config.hpp`'s C++23 check rejects clang-cl**
- **What:** clang-cl sets `_MSVC_LANG` = 202004 in C++23 mode (with `__cplusplus` = 202400), so `_MSVC_LANG <
  202302L` fires.
- **Where found:** ROUND21, "Zancle proposals (for upstream)" #1; confirmed in MSVC audit, "Build and CI changes".
- **Fix:** read `__cplusplus` whenever `__clang__` is defined.
- **Status:** fixed upstream (taken by 4ed9c3cc; QVR's local change dropped).

## 2. Portability (cl.exe and clang-cl)

QVR builds Zancle with **clang-cl only**. Plain cl cannot build it: CMake stops with a message, and the whole engine
is built by clang-cl (ROUND21, "Zancle, vendored" and "clang-cl: the whole engine").

The full cl audit is `ZANCLE_MSVC.md`. It was made against upstream `20a39d952` (2026-09-30), which is **before**
`c16b19e` (`ZA_HAS_BUILTIN`), `18fddfb` (`Base/Limits.hpp`) and `13db45220` ("Improve compatibility with MSVC and
clang-cl"), all ancestors of the vendored 4ed9c3cc and 534219bf. Rechecked against `13db45220` (2026-10-01, by reading the code):
P1, P3, P5, the `ZA_CPU_RELAX` part of P7 and the `Path` part of P11 are fixed upstream; QVR already has them (its
vendored files are upstream's 534219bf byte for byte).

Audit baseline: 141 of 432 headers and 130 of 150 `.cpp` files fail. There are 6,768 diagnostics, and 197 fatal
C1012 errors hide most of the rest. After all the fixes below, only 3 headers and 4 `.cpp` files still fail, all
expected or blocked on dependencies.

Status: P1, P3, P5 **fixed upstream** (`13db45220`); P7 and P11 **partly fixed upstream** (`13db45220`); the rest
**open** as of the audit (`20a39d952`). P8 is moot since `fad225a4` (the real `<initializer_list>` under `_MSC_VER`).
None of it matters to QVR, which builds with clang-cl.

- **P1. `__has_builtin` used unguarded in `#if`** (MSVC audit R1; ROUND21 #2). **Fixed upstream** (`c16b19e`, in
  `13db45220`): `Base/HasBuiltin.hpp`'s `ZA_HAS_BUILTIN` (`__has_builtin` where defined; on cl a table:
  `is_constant_evaluated`, `launder`, `strlen`, `memcmp`, `offsetof`, `__assume`; no `bit_cast`), used in 81 files;
  `git grep __has_builtin` outside `HasBuiltin.hpp`: 0 hits (at 4ed9c3cc too); `ZA_PRIV_HAS_MATH_BUILTIN` and
  `SourceLocation.hpp` go through it (SourceLocation's `#error` on cl remains: P11).
  - Scope: 79 files. On cl it gives C4067 and silently takes the fallback.
  - It is fatal C1012 inside `ZA_PRIV_HAS_MATH_BUILTIN`, which blocks every `Math/*.hpp`.
  - `SourceLocation.hpp` hits its `#error`.
  - Hidden effect: `ZA_STRLEN` falls back to the non-constexpr `std::strlen`, so `consteval FmtString` fails in 34
    `.cpp` files.
  - Fix: one `ZA_HAS_BUILTIN(x)` macro with a per-builtin table for cl, using the audit's list of builtins to enable
    and to leave off. Do not enable `__builtin_bit_cast` (a cl parser bug) or `__make_integer_seq` on cl.
- **P2. `Atomic.hpp` and `AtomicWait.cpp` are written on GCC `__atomic_*` builtins and `__ATOMIC_*` constants** (R2;
  ROUND21 #2)
  - Fix: give `MemoryOrder` Zancle's own values, plus a cl backend. Two options:
    - `std::atomic_ref`: verified to compile all of `include/` and `src/`.
    - `_Interlocked*` and `__iso_volatile_*` intrinsics: inline even at `/Od`. The audit has the intrinsic table,
      ARM64 variants included.
- **P3. GCC predefined macros** (R3; ROUND21 #2). **Fixed upstream** (`13db45220`): `UIntPtrT.hpp` has `_WIN64` /
  `_WIN32` branches; `Base/Limits.hpp` (`18fddfb`) defines `ZA_INT_MAX`, `ZA_FLOAT_MAX`... with `__INT_MAX__`... only
  under `#if defined(...)`; `FromChars.hpp` has no `__FLT_*`/`__DBL_*` left; `RectPacker.cpp` uses `ZA_INT_MAX`. The
  only other predefined macro, `IntTypes.hpp`'s `__INT64_TYPE__`, is behind `#ifdef`.
  - `Base/UIntPtrT.hpp` (`__UINTPTR_TYPE__`, else `#error`): add `_WIN64`/`_WIN32` branches.
  - `String/FromChars.hpp` (`__FLT_MAX__`...): use `ZA_FLOAT_MAX`...
  - `RectPacker.cpp` (`__INT_MAX__`).
- **P4. `std::reference_converts_from_temporary_v` is missing from MSVC STL 14.44** (R4)
  - Affects `Trait/ReferenceConvertsFromTemporary.hpp` and `IsInvocableR.hpp`.
  - Fix: the portable fallback macro in R4. It has a known false positive for an lvalue class with a conversion to
    `T&`; document it.
- **P5. `std::fabs` is not constexpr on MSVC** (R5). **Fixed upstream** (`13db45220`): `Math/Fabs.hpp` falls back to
  `za::priv::fabsViaSignBit` (`ZA_BIT_CAST`, `& 0x7FFFFFFF` / `& 0x7FFF...FF`: exact for -0, infinities, NaN) when
  `__builtin_fabs` is missing (the `long double` one still `std::fabsl`).
  - Constexpr callers fail with C3615: `SinCosLookup`, `Transform`, `DrawableBatchUtils`.
  - Fix: a bit-mask `fabs` via `bit_cast`, which is correct for -0.0 and NaN.
- **P6. `gnu::` attributes and `[[assume]]`** (R6; ROUND21 #4; ROUND21, "The initializer_list fix upstream")
  - About 2,580 C5030 warnings plus 185 C5222 warnings. They fail `/WX` and leak into user builds.
  - Includes `[[gnu::cold, gnu::noinline]]` on `za::abort` and `assertFailure`, and the attributes in
    `InitializerList.hpp`.
  - Fix: attribute-list-element macros (`ZA_ALWAYS_INLINE` -> `msvc::forceinline`, `ZA_FLATTEN`, `ZA_NOINLINE`,
    `ZA_COLD`/`ZA_PURE`/`ZA_CONST` empty on cl, `ZA_ASSUME` -> `__assume`). cl accepts an empty element.
- **P7. Builtin fallbacks that compile but lose behaviour or speed on cl** (R7; ROUND21 #3). **Partly fixed upstream**
  (`13db45220`: the first bullet).
  - `ZA_CPU_RELAX()` is empty on cl, so `parallelFor` spins without a pause. Fix: `_mm_pause()` on x86/x64,
    `__yield()` on ARM64. **Fixed** (`Base/CpuRelax.hpp`: `_MSC_VER` with `_M_X64`/`_M_IX86` -> `_mm_pause()`,
    `_M_ARM64`/`_M_ARM` -> `__yield()`).
  - `Prefetch` is empty (still at 4ed9c3cc: `Base/Prefetch.hpp` has only `__builtin_prefetch` or `((void)0)`).
    Fix: `_mm_prefetch` / `__prefetch`.
  - `Clzll`/`Ctzll`/`Popcountll`/`Bswap64` are calls even at `/Ob1`. Fix: intrinsics behind
    `ZA_IS_CONSTANT_EVALUATED()`.
  - `IsNan`/`IsInf`/`Signbit` are CRT calls. Fix: bit tests.
  - `mem*` functions: use `/Oi`.
- **P8. `InitializerList.hpp` lacks cl's `(first, last)` constructor** (R8)
  - Symptom: C2665 at braced lists in 8 `.cpp` files.
  - The audit's verified fix is exactly what upstream later added. **On its own it does not fix B1.** Adopt B1's fix
    instead, which also covers cl. **Moot** since `fad225a4`: under `_MSC_VER` the real header is used.
- **P9. `Optional`** (R9)
  - (a) The explicitly defaulted SMFs on `union Buffer` fail with C2280 for move-only types, blocking 43 TUs. Fix:
    delete those declarations.
  - (b) cl does not elide copies through `?:` (`Font.cpp`). Fix: an immediately-invoked lambda.
- **P10. Explicit instantiation rules cl enforces** (R10)
  - `FmtNumeric`: the `noexcept` must match.
  - `FmtString`: move the requires-clause to the template head.
  - `TextBase`: the deducing-`this` instantiations must say `this`.
  - `Glsl.inl`: the constrained constructor specialization.
- **P11. Missing features on cl** (R11). **Partly fixed upstream** (`13db45220`: the `Path` bullet).
  - `SourceLocation.hpp` needs an MSVC branch (`__builtin_FILE/LINE/COLUMN/FUNCTION`).
  - `RflNames.hpp` hits ICE C1001 on cl: make it an explicit opt-out there.
  - `StackTrace.cpp` uses libbacktrace and `cxxabi`: add a DbgHelp backend.
  - `Path.cpp`'s `file_clock::to_sys` does not exist in MSVC's STL: use `clock_cast`. **Fixed** (`IO/Unity/Path.cpp`:
    `clock_cast<system_clock>` under `_MSVC_STL_VERSION`, so clang-cl too).
- **P12. Pragmas and `/W4` warnings** (R12)
  - Unguarded `#pragma GCC/clang diagnostic` (C4068 ×103), some of them in public headers.
  - C4127 ×415 from `ZA_ASSERT(false && ...)`.
  - C5063 in `SinCosLookup`.
  - Assorted C4172, C4702, C4146, C4661 and C4324.
  - Third-party headers: mark them SYSTEM.
- **P13. Build and CI** (MSVC audit, "Build and CI changes")
  - INTERFACE flags for cl: `/Zc:preprocessor /Zc:__cplusplus /Zc:inline /Zc:throwingNew`, with `/permissive-` made
    INTERFACE.
  - Debug as `/Od /Ob1 /Oi`.
  - Guard `ZA_ENABLE_SANITIZERS` for cl.
  - Skip libbacktrace on cl.
  - Add `vrdev_msvc`, `vrdev_msvc_rel` and `vrdev_clang_cl` presets.
  - Add a `windows-2022` CI job and a header self-containment target (one TU per header, `/W4 /WX`).
  - If cl is not going to be supported, say "clang-cl on Windows" in the readme (ROUND21 #2).

## 3. Missing features

These were the `qza` shims in `Quake/vr/vr_zancle.hpp` (each marked `ZANCLE-TODO`) and the `std::` uses that remained.
At 534219bf only `qza::stableAt` (M9), `qza::sortedByKey` and the `std::map`s (M12) remain.
Sources: ROUND21, "What Zancle lacks" and the second "Zancle proposals" list #9-11; "Zancle update"; "Zancle
follow-ups".

| # | Missing | QVR today | Proposed API | Status |
|---|---|---|---|---|
| M1 | IEEE remainder | `za::remainder` | see B2 | fixed upstream (`fad225a4`) |
| M2 | `abs` for integers (std's overloads, same promotions) | `za::abs` (111 calls) | `za::abs(T)`: integer and float | fixed upstream (`8bb74612`) |
| M3 | quiet NaN constant | `ZA_FLOAT_NAN` (and `ZA_FLOAT_INFINITY`) | `ZA_FLOAT_NAN` in `Base/Limits.hpp` | fixed upstream (`8bb74612`) |
| M4 | a nanosecond steady clock (`za::Clock` counts microseconds) | `za::Clock::nowNanoseconds()`, `za::nanosecondsToSeconds` / `ToMilliseconds` / `ToMicroseconds`; `std::chrono` gone | a ns clock, plus "elapsed since" in s/ms/us/ns | fixed upstream (`dbd1cfa4`) |
| M5 | min/max of a list | variadic `za::min` / `za::max` (28 calls; a left fold, as `qza::minOf`/`maxOf` were) | variadic or list overloads of `za::min`/`max` | fixed upstream (`8bb74612`) |
| M6 | `String(count, char)` | `za::String(count, c)` | that constructor plus `append(count, char)` | fixed upstream (`8bb74612`) |
| M7 | ordering operators on `Array`/`Vector` | `<` (`vr_ledges.cpp`) | `operator<`/`<=>` | fixed upstream (`534219bf`) |
| M8 | `Span::size_bytes` | `span.sizeBytes()` | `sizeBytes()` member | fixed upstream (`8bb74612`) |
| M9 | a map whose values stay put | `qza::stableAt` (`UniquePtr` values in an ankerl map) | a node-stable map, or documented dense-map semantics | documented upstream (`00019de8`); shim kept |
| M10 | `pair` | `za::Pair` / `za::makePair` | `za::Pair` | fixed upstream (`534219bf`) |
| M11 | reverse iterators | `za::reversed` (5 loops), `za::rbegin`/`rend` (1, it erases) | `rbegin`/`rend` on contiguous containers | fixed upstream (`8bb74612`) |
| M12 | an ordered (flat) map | `qza::sortedByKey`; 3 `std::map`s kept in `vr_motion_review.cpp` (sorted writes, pointers kept) | a sorted flat map, plus one with stable nodes | shim + `std::` |
| M13 | a selection algorithm | `za::nthElement` (`vr_audio.cpp`, `vr_hitmodel.cpp`: ties split otherwise, ROUND21) | `za::nthElement` | fixed upstream (`534219bf`) |
| M14 | a stable sort for large ranges (`insertionSort` is O(n²)) | `za::stableSort` (8 calls; `insertionSort` kept for the climb's tolerant comparator) | `za::stableSort` | fixed upstream (`534219bf`) |
| M15 | range overload of `fill` (22 call sites fill a whole array) | `za::fill(a, v)` | `za::fill(range, v)` | fixed upstream (`8bb74612`) |
| M16 | a future/async on `ThreadPool` (`post` has no result and no way to wait) | `qvr::jobs::async` -> `Future<T>`: the first to claim a job runs it, so `get()` runs an unstarted job inline (no deadlock); an unfinished `Future` waits in its destructor; refcount is an intrusive `JobPtr` | `ThreadPool::async` / `Future<T>` (ROUND21 #7) | worked around (vr_jobs) |
| M17 | a re-postable task with no allocation | `jobs::Task` (ROUND21, "Box3D on the pool") | (not proposed in the sources; same family as M16) | worked around |
| M18 | a unique lock | removed; locked part scoped with `LockGuard`, or `Optional<LockGuard>` + `reset()` (CODE_STYLE) | `za::UniqueLock` (#11) | gone |
| M19 | shared ownership | removed: one owner by design (CODE_STYLE) | listed in #11 | gone |
| M20 | a wide string | (not used now) | listed in #11 | open |

These were already provided upstream since 4ed9c3cc and in use, with their shims deleted: `hypot`, `cbrt`, `log2`,
`exp2`, `llround`, `copysign`, `trunc`, `ZA_ISFINITE`, `Limits.hpp`, `fill`, `iota`, `replace`, `lowerBound`,
`stablePartition` and `FastNonCryptoRng`.

## 4. API and ergonomics proposals

The numbering below is from ROUND21's second proposal list, "Zancle proposals... the API, as met in the migration".

- **A1. `Optional`** (#3)
  - The constructor from `T` is explicit, so `return value;` needs `makeOptional`.
  - There is no `operator=(T)`, so `opt.emplace(v)` is needed. std's are implicit.
  - The verdict calls this a trap.
- **A2. `UniquePtr`'s default constructor is explicit** (#4)
  - A struct with a `UniquePtr` member can't be `T{}`, or be made by `makeUnique<T>()`, without `{nullptr}` on the
    member.
  - **Fixed upstream** (`8bb74612`): the four `{nullptr}` members are plain now.
- **A3. `za::Thread(f)` with a plain function does not compile** (#5)
  - `F` is deduced as a function type. Fix: decay it.
  - **Fixed upstream** (`8bb74612`): `vr_gpustats.cpp`'s `za::Thread(run)` (was a lambda calling it).
- **A4. `String` and `StringView`** (#6)
  - `String` has no `append(count, char)`, `compare(pos, n, ...)` or member `swap`.
  - `StringView` has no `front()`/`back()`.
  - Document that `substrByPosLen` returns a view and that `String s = view;` is explicit.
  - **Partly fixed upstream** (`8bb74612`): the members above. The documentation point is open.
- **A5. `Vector`, `Span` and `Array`** (#7)
  - `Vector` has no `assign(count, value)` and no `insert(pos, first, last)` or `insert(pos, {list})`.
  - `Span` has no `subspan`.
  - `Array` has no `fill`, `front` or `back`.
  - Reverse iterators and ordering operators are covered in M7 and M11.
  - **Fixed upstream** (`8bb74612`). QVR uses none of these members yet.
- **A6. `getArraySize` is `consteval`** (#8)
  - It can't be used on an array reached through a reference parameter.
  - **Fixed upstream** (`8bb74612`, `constexpr`). With M15's array `fill`, QVR's two such places need neither.
- **A7. Math wrappers take exactly `float`, `double` or `long double`** (#9)
  - Keep this: it made every implicit promotion visible.
- **A8. `FastNonCryptoRng.hpp` pulls in `Geometry/Priv/Vec2Base.hpp` for `getVec2f`** ("Zancle update", proposal 4)
  - Move that into a separate header, so that non-geometry users don't include it.
  - **Fixed upstream** (`8c3ac3b2`): its `Vec2` members are templates; Random no longer reaches `Geometry`
    (`Geometry/Priv/Vec2Base.hpp` and `Math/ClampMacro.hpp` are vendored but unused now, to delete).
- **A9. `ThreadPool` niceties** (thread-pool round #8)
  - Name the worker threads ("za worker N": `SetThreadDescription` / `pthread_setname_np`).
  - Add a cap parameter to `getHardwareWorkerCountExcludingCallingThread`. A 32-thread CPU gets 31 workers, which
    the game never fills; QVR caps at 31 and makes the count settable.
- **A10. Assert handler hook** (inferred from QVR's local change to `src/Zancle/Base/Assert.cpp`)
  - QVR has to edit `Assert.cpp` so that its own handler (`vr_zancle.cpp`) receives the library's assert failures.
  - The sources record only the local change, not a proposal. A user-settable handler would remove it.
  - **Fixed upstream** (`fad225a4`, `za::setAssertHandler`). QVR installs its handler at startup; with the library
    built without asserts (`QVR_ZANCLE_DEBUG` off) the engine defines the assert function itself, which the library
    then lacks. Tested with `vr_debug_crash assert` / `zassert` in both Debug variants.

## 5. Performance and debug notes

- **N1. The Debug speed-up may not all be Zancle's.**
  - Debug frame time fell 25%, and e1m1's ledge build went from 1308 to 16.5 ms.
  - MSVC's checked iterators (`_ITERATOR_DEBUG_LEVEL`) may account for much of that. It was never measured, so the
    gain should not all be credited to Zancle ("Was it worth it?").
- **N2. Release: no change.**
  - Release frame time is within noise (0.440 vs 0.448 ms). The exe went from 9.94 to 8.52 MB.
  - Rebuilding one `.cpp` is about a fifth faster, and the compiler back end's time went from 57 to 31 s. Full
    rebuilds barely change, because the engine's own headers dominate them ("Zancle migration", "Results").
- **N3. `quickSort` orders equal elements differently from MSVC's `std::sort`.**
  - Zancle's insertion-sort threshold is 16, MSVC's is 32. Both sorts are unstable, so this is legal, but it is a
    porting hazard worth documenting.
  - QVR audited every sort ("Zancle migration", "Sorts").
  - **Documented upstream** (`00019de8`). Rechecked at 534219bf: no `quickSort` call relies on the order of equal
    elements (the ones that did were on a stable sort since the migration).
- **N4. Dense-map values move on growth or erase, unlike `std::unordered_map`'s nodes.**
  - This caused one migration crash: dangling pointers into the hit-mesh map.
  - Worth stating prominently next to `AnkerlUnorderedDense` (see M9).
  - **Documented upstream** (`00019de8`). Rechecked at 534219bf: one more reference held into a map, `vr_ambient.cpp`'s
    across an eviction's `erase_if` (fixed; ROUND21, "Zancle update to 534219bf").
- **N5. `za::min`/`max`/`clamp` and the math wrappers match std bit for bit**, NaN included, apart from
  `remainder` (102625 checks, "Zancle update"). At 534219bf `za::remainder` is the IEEE one: the self-test's 105050
  checks (with the variadic `min`/`max`, `abs`, `stableSort` and `nthElement`) all pass.
- **N6. Zancle's asserts are useful.**
  - With `QVR_ZANCLE_DEBUG`, Zancle's asserts are on in Debug. They caught B3, and the B1 crash surfaced in Debug.
    No assert fired inside Zancle's own logic.
- **N7. Zancle builds and runs without exceptions.**
  - `Optional`, ankerl and moodycamel switch to aborting (`__cpp_exceptions` / `_CPPUNWIND`), and this worked as
    expected ("Zancle follow-ups").
- **N8. `InPlaceVector` and `SmallVector` paid off.**
  - They removed the per-frame allocations in the motion sample and the rope chain. Melee-replay allocations per
    frame went from 8.24 to 6.92 ("Zancle follow-ups").
- **N9. Pool cost model.**
  - A chunk costs one atomic add. A `parallelFor`'s real cost is waking the workers, 10-30 µs ("The game's thread
    pool").
  - Results are deterministic across worker counts when users reduce per-slot in a fixed order: grasp sweeps and
    Box3D hashes were identical with 1-32 workers.
- **N10. On cl, plain `/Od` inlines nothing**, not even `__forceinline`.
  - `/Od /Ob1 /Oi` is cl's equivalent of fast-debug. The builtin fallbacks become real calls on cl (MSVC audit R6,
    R7).

## 6. Prioritized action list

Done upstream by 534219bf (were items 1-3, 5, 7, the ergonomics but A1, and the dense-map and sort docs): B1, B2, B3,
B5, B6, A10 (`fad225a4`); the cheap gaps, A2, A3, A6 (`8bb74612`); N3, N4 (`00019de8`); M4 (`dbd1cfa4`); A8
(`8c3ac3b2`); M7, M10, M13, M14 (`534219bf`). QVR's vendored tree has no local changes left. Still open:

1. **Fix B4 in `ThreadPool::parallelFor`.** Never run unrelated tasks; a gated helper control block. Then add
   `async`/`Future` (M16) and thread names plus a worker cap (A9). QVR's `vr_jobs.cpp` is a reference implementation.
2. **The maps:** an ordered flat map and a node-stable map (M12, M9). The last `std::` in QVR's code outside the
   self-test is `vr_motion_review.cpp`'s `std::map` (3 of them).
3. **Ergonomics:** the `Optional` implicit constructor and `operator=` (A1); document the view from `substrByPosLen`
   and the explicit conversion from a view to `String` (A4) and the exact-type math wrappers (A7).
4. **Decide on cl support.** Either work through the rest of `ZANCLE_MSVC.md`'s order (P1, P3, P5, P8, CpuRelax and
   `Path` are done; next P2's `Atomic` backend and P4), re-auditing against current upstream first, or document
   "clang-cl on Windows" only.

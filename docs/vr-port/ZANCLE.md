# Zancle instead of std:: (the mapping)

His rule (standing; restated 2026-11-22): our C++ (Quake/vr, other Quake/*.cpp/.hpp we own; never
`Quake/vr/external/**` or other third-party code, and C files stay C) uses Zancle
(`Quake/vr/external/zancle/include/Zancle`) wherever it has an equivalent, and includes only the headers it really
uses: Zancle's headers are fine-grained (faster to compile), its code is faster in Debug and its stack traces are
shorter. CODE_STYLE.md, "Zancle, not the standard library", has the style; this page is the lookup table, and
`qvr_checks std_uses` (Tools/qvr_checks/std_uses.cpp, its allowlist `Misc/quakevr/std_uses_allowlist.txt`) keeps
`std::` and the heavy standard headers from creeping back. The checker itself (Tools/qvr_checks, the build's source
checks) is held to the same rule and scanned with its own allowlist rows (`Tools/qvr_checks/<file>`).

Behaviour stays identical when converting: watch the float/double overloads (std's mixed-type calls promote, Zancle's
math templates take exactly `float`, `double` or `long double`: cast where std promoted), integer `abs`, the types of
`min`/`max`, `round` against `lround`, NaN, sort stability and atomic memory orders.

## The table

Every header below is `Zancle/<path>`; `vr_pch.hpp` already brings a few (Sort, IntTypes, SizeT, Swap, Array, the
vectors, Atan2, Clamp, MinMax, String, StringView, FunctionRef, UniquePtr, Ankerl), but a file still includes what it
uses (the no-PCH build checks it: `Misc/quakevr/check_nopch.sh`).

### Math (`<cmath>`, `<cstdlib>`'s abs)

| std | Zancle | header | note |
|---|---|---|---|
| `std::sin(x)` .. on a `float` / `double` | `za::sin(x)` (template) or `ZA_MATH_SINF(x)` / `ZA_MATH_SIN(x)` | `Math/Sin.hpp` | same for `cos tan asin acos atan atan2 sqrt cbrt exp exp2 log log2 log10 pow hypot floor ceil trunc round lround llround fmod fmin fmax copysign` (`Math/<Name>.hpp`). The template takes exactly float/double/long double: `std::sqrt(int)` -> `ZA_MATH_SQRT(i)` (converts to double); a mixed `std::pow(float, int)` -> `za::pow(f, static_cast<float>(i))` only if std's result was float (it was double: `ZA_MATH_POW`) |
| `std::fabs` | `za::fabs` / `ZA_MATH_FABSF` | `Math/Fabs.hpp` | |
| `std::abs` (float or signed int) | `za::abs` | `Math/Abs.hpp` | std's overloads: int for small integers, float for float |
| `std::remainder` | `za::remainder` / `ZA_MATH_REMAINDERF` | `Math/Remainder.hpp` | IEEE remainder (the angle wrap); `za::truncatedRemainder` is fmod's (inexact past large quotients: not a drop-in) |
| `std::min` / `std::max` (2 values, or 3+ of one type) | `za::min` / `za::max` | `Math/MinMax.hpp` | `std::min({a, b, c})` -> `za::min(a, b, c)`; `std::min<float>(a, b)` -> `za::min<float>(a, b)` |
| `std::clamp` | `za::clamp` | `Math/Clamp.hpp` | |
| `std::isnan` / `isinf` / `isfinite` / `signbit` | `ZA_ISNAN` / `ZA_ISINF` / `ZA_ISFINITE` / `ZA_SIGNBIT` | `Base/IsNan.hpp` .. | |
| `std::numeric_limits<float>::max()` .. | `ZA_FLOAT_MAX`, `ZA_FLOAT_EPSILON`, `ZA_FLOAT_INFINITY`, `ZA_FLOAT_NAN`, `ZA_INT_MAX` .. | `Base/Limits.hpp` | |
| `M_PI`-style constants | `za::pi`, `za::halfPi`, `za::tau` (float) | `Math/Constants.hpp` | |

### C strings and memory (`<cstring>`)

| std | Zancle | header |
|---|---|---|
| `std::strcmp` / `strncmp` / `strlen` / `strstr` | `ZA_STRCMP` / `ZA_STRNCMP` / `ZA_STRLEN` / `ZA_STRSTR` | `Base/Strcmp.hpp` .. |
| `std::memcpy` / `memset` / `memcmp` / `memmove` / `memchr` | `ZA_MEMCPY` / `ZA_MEMSET` / `ZA_MEMCMP` / `ZA_MEMMOVE` / `ZA_MEMCHR` | `Base/Memcpy.hpp` .. |
| `std::strchr` | none: the C `strchr` (unqualified; quakedef.h's `<string.h>`) | |

### Types and traits

| std | Zancle | header |
|---|---|---|
| `std::size_t` / `std::ptrdiff_t` / `std::uintptr_t` | `za::SizeT` / `za::PtrDiffT` / `za::UIntPtrT` | `Base/SizeT.hpp`, `Base/PtrDiffT.hpp`, `Base/UIntPtrT.hpp` |
| `std::uint32_t`, `int64_t` .. | `za::U32`, `za::I64` .. (`I8`..`U64`) | `Base/IntTypes.hpp` |
| `std::is_trivially_copyable_v<T>` .. (any trait) | `ZA_IS_TRIVIALLY_COPYABLE(T)` .. (`za::isTriviallyCopyable<T>` where a template is needed) | `Trait/<Name>.hpp` |
| `std::decay_t`, `remove_cvref_t`, `underlying_type_t`, `conditional_t`, `declval` | `ZA_DECAY`, `ZA_REMOVE_CVREF`, `ZA_UNDERLYING_TYPE`, `za::Conditional`, `za::declVal` | `Trait/...` |
| `std::initializer_list<T>` | `za::InitializerList<T>` (an alias: the compiler needs std's name) | `Base/InitializerList.hpp` |
| `std::source_location` | `za::SourceLocation` (`current()`, `fileName()`, `line()`; by value: one pointer) | `Base/SourceLocation.hpp` |
| `<new>`'s `std::align_val_t`, `std::nothrow_t`, `std::nothrow` | `za::AlignValT`, `za::NothrowT` (aliases of the standard's own types, forward-declared), `std::nothrow` | `Base/FwdStdNew.hpp` |
| placement `new (p) T(...)` | `ZA_PLACEMENT_NEW(p) T(...)` | `Base/PlacementNew.hpp` |
| `std::move` / `std::forward` | `ZA_MOVE` / `ZA_FORWARD` | `Base/Macros.hpp` |
| `std::swap` / `std::exchange` | `za::genericSwap` / `za::exchange` | `Base/Swap.hpp`, `Base/Exchange.hpp` |
| `std::size(array)` | `za::getArraySize(array)` (a container: `.size()`) | `Base/GetArraySize.hpp` |
| `std::bit_cast` | `ZA_BIT_CAST` | `Base/BitCast.hpp` |

### Containers and vocabulary

| std | Zancle | header | note |
|---|---|---|---|
| `std::array<T, N>` | `za::Array<T, N>` | `Container/Array.hpp` | aggregate, compares with `<` |
| `std::vector` | `za::Vector`, `za::SmallVector<T, N>`, `za::InPlaceVector<T, N>` | `Container/...` | CODE_STYLE.md says which |
| `std::bitset` | `za::Bitset<N>` | `Container/Bitset.hpp` | |
| `std::unordered_map` / `set` | `ankerl::unordered_dense::map` / `set` | `Container/AnkerlUnorderedDense.hpp` | values move on growth: `qza::stableAt` (vr_zancle.hpp) where pointers are kept |
| `std::map` | ankerl map + `qza::sortedByKey(map)` where the order was used; small and not looked up often: a `za::Vector`/`SmallVector`/`InPlaceVector` of pairs | `vr_zancle.hpp` | an ankerl map's values move when it changes: pointers into it must be set again after (vr_motion_review's scan) |
| `std::deque` (for stable addresses) | `za::ChunkedVector` | `Container/ChunkedVector.hpp` | blocks of 64 KiB, never moved |
| `std::string` / `string_view` | `za::String` / `za::StringView` | `String/...` | `za::toString` for integers |
| `std::span` | `za::Span` | `Vocabulary/Span.hpp` | |
| `std::optional` / `pair` / `unique_ptr` / `function` | `za::Optional` / `za::Pair` / `za::UniquePtr` / `za::FunctionRef`, `za::FixedFunction` | `Vocabulary/...` | |
| `std::rbegin` / reversed range-for | `za::rbegin` / `za::reversed(c)` | `Base/ReverseIterator.hpp` | |

### Algorithms (`<algorithm>`, `<numeric>`)

| std | Zancle | header | note |
|---|---|---|---|
| `std::sort` | `za::quickSort` | `Algorithm/Sort.hpp` | unstable like std's, but equal elements can land in another order than MSVC's `std::sort` left them: where ties are possible and the order shows, `za::stableSort` or a key that orders them all |
| `std::stable_sort` | `za::stableSort` | `Algorithm/StableSort.hpp` | stable, allocation-free |
| `std::nth_element` | `za::nthElement` | `Algorithm/NthElement.hpp` | |
| `std::partial_sort(f, m, l)` | `za::nthElement(f, m, l)` then `za::quickSort(f, m)` | both | the same first m in the same order (ties aside) |
| `std::lower_bound` / `upper_bound` | `za::lowerBound` / `za::upperBound` | `Algorithm/LowerBound.hpp` .. | |
| `std::find` / `find_if` / `any_of` | `za::find` / `za::findIf` / `za::anyOf` | `Algorithm/Find.hpp`, `AnyOf.hpp` | |
| `std::count` / `count_if` | `za::count` / `za::countIf` | `Algorithm/Count.hpp` | |
| `std::max_element` / `min_element` | `za::maxElement` / `za::maxElement` with the reversed comparison | `Algorithm/MaxElement.hpp` | both give the first of equals |
| `std::fill` / `copy` / `iota` / `replace` / `rotate` / `unique` / `remove_if` | `za::fill` / `za::copy` / `za::iota` / `za::replace` / `za::rotate` / `za::unique` / `za::removeIf` | `Algorithm/...` | |
| `std::erase_if(vector, p)` | `za::vectorEraseIf(vector, p)` | `Algorithm/Erase.hpp` | a dense map's: ankerl's `erase_if` (ADL) |
| `std::stable_partition` | `za::stablePartition` | `Algorithm/StablePartition.hpp` | |

### Threads and time

| std | Zancle | header | note |
|---|---|---|---|
| `std::atomic<T>` | `za::Atomic<T>` | `Concurrency/Atomic.hpp` | no operators: `std::atomic`'s `x++`, `x = v`, `T(x)` are seq_cst: `fetchAddSeqCst(1)`, `storeSeqCst(v)`, `loadSeqCst()`; explicit orders map 1:1 (`load(std::memory_order_relaxed)` -> `loadRelaxed()`) |
| `std::atomic_thread_fence(o)` | `za::atomicThreadFence<za::MemoryOrder::O>()` | `Concurrency/Atomic.hpp` | |
| `std::atomic_flag` / `std::mutex` + `lock_guard` | `za::AtomicMutex` + `za::LockGuard` | `Concurrency/AtomicMutex.hpp`, `LockGuard.hpp` | a spin-then-wait lock |
| `std::thread` | `za::Thread`, `za::ThisThread` | `Concurrency/Thread.hpp` | |
| `std::async` / futures | `jobs::` (vr_jobs.hpp, on `za::ThreadPool`) | | |
| `std::chrono::steady_clock` | `za::Clock` (`now()`, `nowNanoseconds()`), `za::Time` | `Chrono/Clock.hpp`, `Chrono/Time.hpp` | microsecond `Time`; nanoseconds through `za::nanosecondsTo*` |
| `std::chrono::system_clock` (epoch time) | `za::Clock::nowUnixNanoseconds()` | `Chrono/Clock.hpp` | since 1970 UTC, not monotonic: timestamps compared across processes |
| `size_t` (`std::size_t`) | `za::SizeT` | `Base/SizeT.hpp` | in our C++ everywhere (C files and the C headers they share keep `size_t`: the same type) |

## No Zancle equivalent (kept; the allowlist names each)

- **C stdio/stdlib/time** (`snprintf`, `sscanf`, `atof`, `atoi`, `va_list`, `time`): called unqualified (the C
  functions, quakedef.h's headers), or the engine's `q_snprintf`/`Q_atof` where the file already uses them; never
  `std::`-qualified. Not in the check (the C headers `<cstdio>`, `<cstdarg>`, `<cstdlib>`, `<ctime>`, `<cctype>` are
  allowed).
- **`std::filesystem`** (`vr_files.hpp`/`.cpp`, `vr_motion_review.cpp`): Zancle has no paths or directory walking;
  `qvr::files` wraps it in one place (CODE_STYLE.md, "Files").
- (Zancle against the standard library, once `vr_zancle_math_test`, is Zancle's own unit test:
  `test/Base/MathAgainstStd.test.cpp`.)
- **Tools/qvr_checks** (the build's source checks): `<regex>` (Zancle has no regex: the patterns ported from the
  Python checks; matched over `const char*` ranges of `za::String`), `std::filesystem` and its `std::error_code`
  (paths and directory walking), `<format>` (Zancle's formatter isn't vendored: `common.hpp`'s `fmt`/`Out::line` wrap
  `std::format_to` into a `za::String`, with `std::formatter`s for `za::String`/`za::StringView`) and `std::exception`
  (`main.cpp`: a check's throw reported as its failure). Files are read with the C stdio (`_wfopen`), processes and
  wide strings through the Windows API and `za::Vector<wchar_t>`.
- **Third-party code and its interfaces** (Tracy, OpenXR, imgui, lodepng, Box3D, Opus): untouched.
- **Comments** that name a std facility (what the code replaced, or mimics): the check ignores comments.

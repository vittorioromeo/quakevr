# Code conventions (Quake/vr)

How the VR module keeps state, and what it builds on. The rest of the style (formatting, naming, comments) is the
surrounding code's: read a neighbouring file and write like it.

## Zancle, not the standard library

The VR code is written on Zancle (`Quake/vr/external/zancle`, its README; ROUND21.md, "Zancle migration"), not on the
C++ standard library: Zancle's types compile faster and run faster in Debug. Use:

- `za::Vector` (a list whose size has no small bound), `za::SmallVector<T, N>` (usually small, may grow),
  `za::InPlaceVector<T, N>` (never more than N: a hard bound, aborts past it), `za::Array<T, N>`, `za::Bitset<N>`;
  `ankerl::unordered_dense::map` / `set` for hashing. A dense map's values **move** when it grows or loses an entry:
  where a pointer or a reference into one is kept past the next insertion (a cache that hands out pointers), hold the
  values in `za::UniquePtr` (`qza::stableAt`). Its order is the insertion order (not sorted, not std's hash order):
  where the old `std::map`'s order was printed or summed, loop over `qza::sortedByKey(map)`.
- `za::String`, `za::StringView` (`substrByPosLen` returns a view: `za::String{...}` to keep it), `za::toString` for an
  integer (a float's text through `va`/`snprintf`, whose formats the code already uses), `za::Optional`, `za::Span`,
  `za::UniquePtr` / `za::makeUnique`, `za::FunctionRef` (a callback for the call's length), `za::FixedFunction`.
- `za::min` / `za::max` / `za::clamp`, `za::sin` and the rest of `Zancle/Math` (exactly `float`, `double` or `long
  double`: a mixed call that std promoted needs its cast), `ZA_MEMCPY` / `ZA_STRCMP` and the other builtin macros,
  `ZA_MOVE`, `ZA_FORWARD`, `ZA_ASSERT` (on in Debug: its handler is `vr_zancle.cpp`'s).
- `za::quickSort` (unstable: equal elements may come out in another order than `std::sort` left them; where that
  matters, a key that orders them all or `za::insertionSort`, stable), `za::find`, `za::anyOf`, `za::count`, ...
- `za::Atomic`, `za::AtomicMutex` with `za::LockGuard` (`qza::UniqueLock` to unlock early), `za::Thread`,
  `za::ThisThread`; `za::Clock` / `za::Time` (microseconds; `qza::nowNs` for nanoseconds).
- Files: `qvr::files` (`vr_files.hpp`: whole files read and written, directories listed, std::filesystem's path parts),
  through the engine's `Sys_*` calls (UTF-8 paths).
- What Zancle lacks: `vr_zancle.hpp` (namespace `qza`: `abs`, `hypot` and the other missing math, `fill`, `iota`,
  `lowerBound`, `stablePartition`, `Pair`, `minOf` / `maxOf`, `rbegin` / `rend`, ...), each a proposal for Zancle. Add
  a missing piece there (marked `ZANCLE-TODO`) rather than reach for `std::`. `std::` stays only where Zancle has no
  such thing and a stand-in would not do (random engines whose sequences matter, exception transport, an ordered map
  with stable nodes, `std::shared_ptr` across threads, `std::nth_element` where its partition must stay as it was):
  each such place says `// ZANCLE-TODO: <what's missing>` (the list: ROUND21.md, "Zancle migration").

## Scratch buffers and caches

**No function-local `static` (or `thread_local`) variables in `Quake/vr`, of any type, except `static constexpr`.** Global
state is marked as such, at file scope, where it can be seen: not hidden in a function. A local static is also paid for on
every call: one that is built at run time (`static const T x = f();`, `static const za::Vector<...> list = {...}`) or has a
destructor gets a thread-safe guard (MSVC/clang-cl: a `$TSS` guard variable checked on each call, `_Init_thread_header`
on the first), and a mutable one is state nobody sees. `static constexpr` is the exception because `constexpr` proves the
compiler initialises it at compile time and nothing changes it: no guard, no state. `static const` is not enough, even
when its value happens to be constant (`static const int n = 3;` has no guard; `static const int n = f();` does, and the
two read alike): spell a constant `static constexpr` (`static constexpr const char* names[] = {...}`).

A container kept only to avoid allocating it again next time (a scratch buffer), or results kept for a key (a cache), is
the main case. Hidden in a function it is never given back (a one-off peak stays for the session), never counted, never
emptied when what it was made from goes away, and nothing says which thread may touch it. It goes into its system's
state instead:

1. **Scratch** (its contents mean nothing from one call to the next): a member of the file's scratch struct, registered
   with `mem::Scratch` (`vr_mem.hpp`). The function takes a reference and clears it itself; the capacity stays from call
   to call (no allocation once warm), and the whole set is given back at every map change.

   ```cpp
   // The hold search's buffers (the server's frame: the main thread).
   struct ClimbScratch
   {
       za::Vector<int> nearby;      // the ledges of one model near the hand (gather)
       za::Vector<Candidate> found; // (findHold)
       auto members() { return mem::list(nearby, found); }
   };
   mem::Scratch<ClimbScratch> scratch{"climb"};

   void gather(...)
   {
       za::Vector<int>& nearby = scratch.nearby;
       nearby.clear();
   ```

   - One member per use: two functions that may be live at once (one calls the other) never share a buffer.
   - `members()` lists every member (a `static_assert` checks their sizes add up to the struct's). A member of a type of
     your own that holds heap memory gets a `heldBytes(const T&)` next to it, found by ADL, so that it is counted
     (`gadget`'s `Lines`, `grasp::Shape`).
   - A function that returns text by pointer (`const char* label()`, the menus' info lines) keeps it in a scratch set
     too ("readouts"): valid until the same function's next call, which is how the menu uses it (drawn at once). Text a
     cached structure points into lives as long as that structure, in the same owner (`vr_menu.cpp`'s `PageTexts`).
2. **Cache** (results kept for a key): `mem::Cache` with the events that make it stale: `mem::MapChange` (anything
   keyed by a pointer into the hunk: textures, edicts, the world), `mem::GameDirChange` (models' slots, game files),
   `mem::ModelReload` (`vr_model_reload`, `vr_hand_reload`), or `mem::Never` (only counted; its owner empties it). A key
   that can change otherwise (a setting) is part of the key and checked on use. Explicit resets that already exist
   (`resetServerWorld`, `VR_OnClientClearState`, `VR_OnGameDirChanged`'s list) stay; `mem::on(event)` is called from
   `VR_OnClearMemory`, `VR_OnGameDirChanged` and the two reload commands.
3. **State that is not a buffer** (what a frame built once and every view draws: `flashlight.cpp`'s `CordDraw`,
   `rope.cpp`'s `RopeDraw`; what a function remembers from one call to the next: a "once a frame" frame number, the last
   print's time, a "registered" flag, a debug trace's open `FILE*`, an easing's last value): a named variable or struct
   at file scope in the anonymous namespace, next to the function (or the system's other state), with a comment saying
   who uses it and from which thread (`avatar.cpp`'s `ArmEase`, `view.cpp`'s `OncePerFrame` and `GrappleDebug`,
   `main.cpp`'s `UnpacedSwap`). A member of the class when the function is a member (`MockBackend::shake`). The
   function may bind a reference to it under the old name (`int& lastFrame = oncePerFrame.quadArcs;`). A debug
   command's working buffer is a plain local; a fixed array that is all written before it is read is a plain local too
   (`coil.cpp`'s forces), unless it is too big for the stack (then file scope: `throw.cpp`'s `bothHistory`).
4. **Constants**: `static constexpr` when the type allows (`bodycal`'s `splitList`, the name tables), else a
   namespace-scope `const` built before `main` (read by any thread, no guard): `ao`'s `rayDirs`, `ambient`'s `rayTable`,
   `avatar`'s `bindPose`, `unstick`'s `spotOffsets`, `motion`'s `categoryList`, `handrig`'s `referenceRig`. Its
   initialiser may use only constexpr data and pure functions: another file's globals may not be made yet (the static
   initialisation order). An empty object returned by reference (`none`) is a namespace-scope `const` too. Text handed
   out by pointer goes into the file's readouts set (`menu`'s `MenuReadouts`) or a file-scope `char` array (valid until
   the function's next call). An engine cvar is read through its `extern "C" cvar_t` (not a `Cvar_FindVar` cached in a
   static).

`vr_memstats` prints the registered sets' bytes (totals and the largest), `vr_limits` the totals.

`Misc/quakevr/check_statics.py` (run by the kit's `build.sh`, which fails on it) rejects every function-local `static`
or `thread_local` in `Quake/vr` that is not `static constexpr`, of any type, in any function or lambda body (an
`if(static bool once = false; !once)` too); class members and namespace-scope variables are not function-local. It
reads the source's scopes (comments, strings and raw strings skipped), so it needs no compiler. A line that must stay
(a worker thread's own buffer) ends with `// statics-ok: <why>`; none does today. `--legacy` applies the old rule
(`static std::`/`za::`/`ankerl::` only) for comparison.

## Threads

Work for other threads goes to the game's thread pool (`vr_jobs.hpp`, Zancle's): `jobs::parallelFor` to share a loop
out (the calling thread takes part and returns when every chunk ran; each chunk writes only its own items, reduced in a
fixed order after, so the results never depend on the thread count), `jobs::async` for a task whose result comes later
(a `jobs::Future`; waiting runs it on the waiting thread if no worker has started it). No thread (`za::Thread`) or async
call of a system's own: a dedicated thread only for a loop that blocks or sleeps for its whole life (`gpustats`'s sampler),
which would hold a worker. A task never waits for anything but its own sub-tasks; `vr_jobs_parallel 0` runs every
`parallelFor` on its caller alone (the reference to compare with).

A task owns what it touches: its job's copy of the data (`ao`'s `PoseJob`), locals, or a `thread_local` when the same
helper also runs on the main thread (`decals`'s atlas RNG). Registered sets are the main thread's alone (`mem::on` and
the reports walk them there), and so are the console, the profiler's scopes and the engine's globals: nothing a task
runs prints or profiles. Results come back through the owner's queue under its mutex (`ao`'s `BakeQueue`,
`imgprefetch`'s items) or a `jobs::Future` (`motion`'s saves, `decals`'s atlas). Box3D's step may run on the pool
(`vr_box3d_threads`; ROUND21.md, "Box3D on the pool"): its callbacks (`shouldCollide`, `preSolve` and what they call)
then run on workers, with the stepping thread's QuakeC VM lent (`qcvm` and `pr_global_struct` are thread-local). They
only read the entities and the world's state, count with an atomic, and never print (`vr_debug_box3d 2` steps on one
thread).

## GL objects

GL names are plain ids (`gfx::Texture`, `gfx::Target`: no destructor), made and deleted by their system's functions
(its reset, `VR_Shutdown`), never by a static's destructor after the context is gone. `vid_restart` keeps the GL
context (only the engine's framebuffers are made again), so the VR module's GL objects live on.

## Allocations a frame

The profiler counts the main thread's C++ allocations (`vr_profile_report`: "allocations", avg/max a frame; the systems
CSV's column): `vr_alloccount.cpp` replaces `operator new` with a counting one. A system's frame should allocate nothing
once warm; its first frames after a map load grow its scratch sets again.

## QuakeC

fteqcc's operator priorities are QC's, not C's: parenthesise every `&&`/`||` on the right of an assignment,
`x = (a && b);`, since `x = a && b` (also `+=` and `self.f = a && b`) is `(x = a) && b`. Also `a && b ? c : d` is
`a && (b ? c : d)`, `a || b && c` is `(a || b) && c`, `!a == b` is `!(a == b)` and `a & b == c` is `(a & b) == c`.
`Misc/quakevr/check_qc_precedence.py` (run by `QC/build.sh`, `QC/build.bat` and the kit's `build.sh`) fails on any
expression that compiles differently under C's priorities; `Misc/quakevr/qcrepro/repro.qc` shows each case.

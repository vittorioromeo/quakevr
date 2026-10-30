# Code conventions (Quake/vr)

How the VR module keeps state, and what it builds on. The rest of the style (formatting, naming, comments) is the
surrounding code's: read a neighbouring file and write like it.

## Includes and the build's speed

- Every file includes what it uses. The build forces the precompiled header `vr_pch.hpp` (glm, the common Zancle
  headers, `vr_engine.hpp`) into each `Quake/vr` .cpp, so a missing include still compiles there:
  `Misc/quakevr/check_nopch.sh` builds without it (incrementally after its first run) and fails on one. Only headers
  that rarely change go into `vr_pch.hpp`: a change to it rebuilds every VR file.
- `vr_engine.hpp` has only SDL's core headers (quakedef.h with `QVR_ENGINE_SDL_CORE`), and quakedef.h no longer pulls
  in windows.h: a file that calls SDL includes `SDL.h`, one that uses the Windows API includes `<windows.h>` (or
  declares the few functions it calls, as `vr_ao.cpp` does).
- A shared header doesn't include a heavy header (windows.h, phonon.h, OpenXR, box3d) for one inline function: the
  function goes into the .cpp. A large file splits into parts with an `_internal.hpp` (`vr_view_internal.hpp`,
  `vr_menu_internal.hpp`: the old anonymous namespace a named `*_detail` one).

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
  `za::UniquePtr` / `za::makeUnique`, `za::FunctionRef` (a callback for the call's length), `za::FixedFunction`,
  `za::Pair` / `za::makePair` (where the two have no better names than a struct's), `za::reversed(c)` (a range-for
  backwards; `za::rbegin` / `za::rend`). `Array` and the vectors compare with `<` (element by element).
- `za::min` / `za::max` (also of 3 or more, all of one type) / `za::clamp`, `za::abs` (std's overloads), `za::sin` and
  the rest of `Zancle/Math` (exactly `float`, `double` or `long double`: a mixed call that std promoted needs its cast);
  `za::remainder` is the IEEE one (std's: an angle wrap to -180..180), `za::truncatedRemainder` fmod's (fast, inexact
  past large quotients). `ZA_FLOAT_NAN` / `ZA_FLOAT_INFINITY` (`Base/Limits.hpp`), `ZA_MEMCPY` / `ZA_STRCMP` and the
  other builtin macros, `ZA_MOVE`, `ZA_FORWARD`, `ZA_ASSERT` (on in Debug: its handler is `vr_zancle.cpp`'s, installed
  with `za::setAssertHandler`).
- `za::quickSort` (unstable: equal elements may come out in another order than `std::sort` left them; where that
  matters, a key that orders them all or `za::stableSort`, stable and allocation-free; `za::insertionSort` is stable
  too, for a few elements or a comparator that is not a strict weak order), `za::nthElement`, `za::fill` (a range, an
  array or two iterators), `za::find`, `za::anyOf`, `za::count`, `za::lowerBound` / `za::upperBound`, ...
  `za::vectorEraseIf` takes contiguous containers only; a dense map's is `erase_if(map, ...)` (ankerl's, by ADL).
- `za::Atomic`, `za::AtomicMutex` with `za::LockGuard` (to unlock early: the locked part in a scope of its own, or
  `za::Optional<za::LockGuard<...>>` and `reset()`), `za::Thread`,
  `za::ThisThread`; `za::Clock` / `za::Time` (microseconds); `za::Clock::nowNanoseconds()` and
  `za::nanosecondsToSeconds` / `ToMilliseconds` / `ToMicroseconds` for nanoseconds.
- Files: `qvr::files` (`vr_files.hpp`: whole files read and written, directories listed, std::filesystem's path parts),
  through the engine's `Sys_*` calls (UTF-8 paths).
- What Zancle lacks: `vr_zancle.hpp` (namespace `qza`: `stableAt` and `sortedByKey`, for the maps; still lacking at
  Zancle 2f8a1ca5), each a proposal for Zancle. Add a missing piece there (marked `ZANCLE-TODO`) rather than reach for `std::`. `std::` stays only where
  Zancle has no such thing and a stand-in would not do (an ordered map with stable nodes, `std::map` in
  `vr_motion_review.cpp`): each such place says `// ZANCLE-TODO: <what's missing>` (the list: ROUND21.md, "Zancle
  migration", "Zancle follow-ups" and the Zancle updates). The vendored Zancle has no local changes: update it with
  `Misc/quakevr/zancle_vendor.py` (`external/zancle/README.md`).
- **No exceptions.** The engine's C++ and Zancle are built without them (clang-cl without `/EH`, `-fno-exceptions`):
  no `try`, `catch` or `throw`. A failure that can't go on ends the game with the crash report (`abort()`, or
  `Sys_Error` from the engine's side); the thread pool's tasks and loops are `noexcept`. No shared ownership either
  (`std::shared_ptr`): one owner, and others hold a pointer the owner provably outlives (or a copy).

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

Work for other threads goes to the game's thread pool (`vr_jobs.hpp`, Zancle's): `jobs::parallelFor` (on `za::parallelFor`) to share a
loop out (the calling thread takes part and returns when every chunk ran; each chunk writes only its own items, reduced in a
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
CSV's column): `vr_alloccount.cpp` replaces `operator new` with a counting one, including aligned forms. Engine-owned
C calls use `VR_HeapMalloc/Calloc/Realloc/Free` (`vr_alloccount.h`); Box3D uses aligned allocator callbacks and image
libraries use their allocator hooks. Do not use allocation-name macros across system/vendor headers. The wrappers
preserve CRT pointer ownership and failure semantics; C++ allocation uses raw C heap storage (malloc/free) to avoid double counting.
The C heap is mimalloc (`vr_crtheap.c`, `external/mimalloc/README.md`): every malloc, free, `_aligned_*`, strdup and so
new/delete in the executable; the DLLs (SDL2, libcurl, OpenXR, Steam Audio) keep the C runtime's. Never hand a DLL a
block to free, nor free one of theirs with `free` (theirs go back with `SDL_free`, `curl_free`, ...); `vr_heap` counts
the C runtime's own blocks freed here (`_fullpath(NULL)` and the like: handled, but each one a call to look at).
Counters are per thread; the frame profiler reads only the main thread. Its additional columns are `cpp_deletes`,
`c_heap_requests`, `c_heap_frees` and `heap_requested_kib` (request bytes floored to KiB per frame, not live heap usage).
C request counts include attempts; null frees are ignored. `vr_alloc_sites 300 50` records heap event kinds, exact
requested bytes, caller sites, and the peak allocation-request frame. Add a third argument of `1` to retain and print
that individual frame's stack counters (`peak_site`) as well as window totals. Windows stack resolution requires the build's
PDB. Tracing is opt-in and changes timing; do not benchmark CPU cost with it enabled. DLL-private heaps and zone/hunk
suballocations are outside these counters (the zone/hunk's CRT backing allocation is covered).

A system's frame should allocate nothing once warm; its first frames after a map load grow its scratch sets again.
Use retained scratch storage for sequential users and fixed inline storage for small bounded data. Do not reuse one
scratch vector across nested/reentrant calls or workers. `vr_alloc_test` checks wrapper semantics and counter deltas.

## QuakeC

fteqcc's operator priorities are QC's, not C's: parenthesise every `&&`/`||` on the right of an assignment,
`x = (a && b);`, since `x = a && b` (also `+=` and `self.f = a && b`) is `(x = a) && b`. Also `a && b ? c : d` is
`a && (b ? c : d)`, `a || b && c` is `(a || b) && c`, `!a == b` is `!(a == b)` and `a & b == c` is `(a & b) == c`.
`Misc/quakevr/check_qc_precedence.py` (run by `QC/build.sh`, `QC/build.bat` and the kit's `build.sh`) fails on any
expression that compiles differently under C's priorities; the repro that showed each case, `Misc/quakevr/qcrepro/repro.qc`, is in git history (commit `2f6ffb4ba`).

Test code stays out of releases: a QC file of tests (`*_test.qc`, listed in `progs.src`) is wrapped whole in
`#ifdef QVR_TESTS` ... `#endif` (the macro is defined in `QC/vr_macros.qc` unless fteqcc is given `-DQVR_NO_TESTS`, which
the release packaging does), and every call or declaration of it from game code is wrapped the same way. A new test file:
wrap it, and compile once with `-DQVR_NO_TESTS` (no errors allowed; the compile writes `quakevr/progs.dat`: build again
for the development one).

## Canonical helpers

One implementation each; new code uses these, a local copy is a review finding. Other clusters add their own lists below.

### Geometry and math (`Quake/vr/vr_geom.hpp`, namespace `qvr::geom`)

- `geom::vec(const float*)` and `geom::store(vec3, float*)`: Quake `vec3_t` to/from `glm::vec3` (a file may write
  `using geom::vec;` to keep the short name). Not yet merged: the `setVec(float*, vec3)` spellings (climb, foegrab, portals).
- `geom::pi`, `geom::deg` (float pi; degrees to radians), `geom::up` (+z).
- `geom::safeNormalize(v, fallback = up, eps = 1e-5f)`: pass the site's eps when it differs.
- `geom::wrap180(a)` (round form, no loop: the old `while` wraps hung on inf/NaN), `geom::yawDelta(to, from)`,
  `geom::yawOf(dir)`.
- `geom::smooth01(t)`, `geom::smoothstep(e0, e1, x, minWidth)`, `geom::approachExp(now, target, dt, tau)`.
- `geom::closestOnSegment(p, a, b, &t, eps)`, `geom::segmentDistance(p, a, b, &t, eps)`, `geom::closestOnTriangle`.
- `geom::quakeFromTracking(v)`: tracking space to Quake axes.
- Angles to axes and back: `held::axesFromAngles` / `held::anglesFromAxes` (vr_held.hpp); the rest of the angle helpers
  are in `hands::`.
- A matrix from a float[16]: `glm::make_mat4` (no local `toMat4`). Angle between two vectors: `still::degreesBetween`.

### Engine access (refactor A2)

Use these; don't write a local copy (a file's own wrapper is fine only when it bakes in that site's own rule or
constants, and then its body calls the canonical one).

| need | helper | where |
|---|---|---|
| the server's VM current for a scope | `progs::ServerVm vm;` (RAII) | vr_progs.hpp |
| the local player | `progs::localPlayer()` (null when not hosting / client 0 not in game) | vr_progs.hpp |
| the first player slot (edict 1) | `progs::playerEdict()` (null only when not hosting; client 0 may not be in yet) | vr_progs.hpp |
| a server edict by number | `progs::edictAt(num)` (null: out of range, free, not hosting) | vr_progs.hpp |
| edict loops, nearest one | `progs::forEachEdict(first, fn)`, `progs::nearestBy(first, limit, distance, &best)` | vr_progs.hpp |
| calling QC | `progs::callQc(fn, a, b, c)`, `qcFloat`, `qcText(dst, size, fn, a)`, `qcVec(fn, a, fallback)`; by name with `progs::findFunction` (or bind it in `Bindings`) | vr_progs.hpp |
| builtin vector parms / returns | `progs::vecParm(OFS_PARMn)`, `returnVec(v)`, `setVecParm(ofs, v)`, `returnedVec()` | vr_progs.hpp |
| entity fields | `progs::fields().name` (add the field to vr_fields.inc; no `ED_FindFieldOffset("...")` for a fixed name) | vr_fields.inc |
| a hand's fields | `progs::handFields(hand).pos / rot / held / muzzle / shotRot / loadPortPos / loadPortAxis / loadPortFace / throwVel / avel` (`hand` nonzero: the main hand) | vr_progs.hpp |
| point contents, liquids | `worldtrace::contents(p)` (server), `clientContents(p)`, `liquidContents(p, client)`, `isLiquid(c, currents)`; each site keeps its own rule of what is liquid | vr_trace.hpp |
| a server line trace | `worldtrace::serverLine(a, b)` (passes the world) / `serverLine(a, b, pass, type)` | vr_trace.hpp |
| a controller buzz | `haptics::pulse(hand, seconds, amplitude, hz = 160)` (vr_disablehaptics and the hand's bounds inside) | vr_backend.hpp |
| a cvar set for a scope | `cvars::Override o{cvar, value};` (RAII) | vr_cvars.hpp |
| cvars kept and put back later | `cvars::Snapshot` (`keep(cvar)`, `restore()`) | vr_cvars.hpp |
| every cvar | `cvars::forEach(fn)` | vr_cvars.hpp |
| a VR cvar's value | `vr_x.value` directly, never `Cvar_VariableValue("vr_x")` (by name only for names typed or made at run time) | vr_cvars.hpp |
| a client entity's number | `client::entityNumber(e, limit)` (-1: null, the world, not one of cl_entities) | vr_client.hpp |

The player's edict: never `EDICT_NUM(1)` or `svs.clients[0].edict`. `localPlayer()` when the code needs client 0
in the game (its guard: `sv.active && svs.clients[0].active`): HUD, motion, climbing, traces from the hands.
`playerEdict()` when the server running is enough (its guard: `sv.active`; svs.maxclients is always at least 1):
console commands, tests and diagnostics, server-frame code, QC calls with the player as `self`. Single player only:
`svs.maxclients == 1 ? progs::playerEdict() : nullptr`. Neither depends on the current VM. Sites guarded by something
else (a test's state, `svs.maxclients` alone, nothing: the caller's) were left as they were (ROUND23, "player edict").

### Small libraries (refactor A3)

Do not write a local copy (a second `rnd`, `lower`, `trim`, `startsWith`, `appendf`, `nearestNormal`, LE reader, quantile, UI click or verdict line).

| need | helper | where |
|---|---|---|
| a float in [lo, hi] from a file's own `za::FastNonCryptoRng` (lo > hi allowed) | `vrand::range(rng, lo, hi)` | `vr_random.hpp` |
| the small seeded streams (xorshift32, the 1664525 LCG) as `[0,1)` floats / ranges | `vrand::xorshift32/xorshift01/xorshiftRange`, `vrand::lcg01/lcgRange` | `vr_random.hpp` |
| `startsWith` / `endsWith` on C strings, `lower`, `trim` (space, tab, CR, LF), `appendf(za::String&, fmt, ...)` (any length) | `strings::startsWith/endsWith/lower/trim/appendf` | `vr_strings.hpp` |
| a quantile of sorted samples; the rank rule is a parameter (`Floor`, `Nearest`, `Count`) | `vrstats::quantileSorted`, `rankIndex` | `vr_stats.hpp` |
| little-endian int/float from model bytes (unchecked); a bounds-checked reader (0 past the end) | `bytes::le32/lef32`, `bytes::View` | `vr_bytes.hpp` |
| the MDL normal index nearest to a direction | `mdlbuild::nearestNormal` | `vr_mdlbuild.hpp` |
| a verdict line `<name>: N passed, M failed`; a pass/fail counter | `testkit::verdict`, `testkit::Tally` | `vr_testkit.hpp` |
| a UI click / cue sound by kind (the path table) | `menuui::uiSound(UiSound::Move/Select/Adjust/Deny/Talk/Pickup)` | `vr_menuui.hpp` |
| linking a GL program (info log, label, shader cleanup); a bound-FBO completeness check | `gfx::linkProgram`, `gfx::boundFramebufferComplete` | `vr_glutil.hpp` |

### Kits (refactor B2)

| need | helper | where |
|---|---|---|
| an offscreen texture + framebuffer made for a size (colour or depth, completeness checked, labelled) | `gfx::RenderTarget` (`ensure(w, h, RenderTargetFormat{...}, name)`, `destroy()`) | `vr_glutil.hpp` |
| an in-world tool panel's push buttons: a button's middle, a hand pressing it, the button under a hand | `toolpanel::Frame`, `Grid`, `buttonCentre`, `pressedBy`, `buttonUnder` | `vr_toolpanel.hpp` |
| a bounded undo stack (oldest dropped); a tool's feedback line (panel + console) | `toolpanel::pushUndo(stack, max, item)`, `toolpanel::say(line, until, tool, text, seconds)` | `vr_toolpanel.hpp` |
| a panel image's 2D text and boxes (Quake's font, gold/white, shrunk to fit) | `toolpanel::Rect`, `fill`, `text`, `textFit` | `vr_toolpanel.hpp` |
| a client one-shot sound at a place / on the local player | `audio::playAt(sfx, at, volume, attenuation, entity = 0)`, `audio::playLocal(sfx, volume)` | `vr_audio.hpp` |
| per-slot cvars (`vr_<x>_<key>_NN`): storage, names, a cvar's key | `SlotTable<Key, Slots, Keys>` (`at`, `define`, `keyOf`, iteration) | `vr_slottable.hpp` |
| QC functions the engine calls every frame | a `progs::Bindings` member resolved once per map (`bindings().arenaCards.CardSerial`), never `findFunction("...")` per frame | `vr_progs.hpp` |
| work done at each map load, in order | a row of `prewarmSteps` (vr_main.cpp `VR_NewMap`) | `vr_main.cpp` |
| a new view entity (hands, weapons, body parts) | a member of `Entities` listed in `Entities::members` at its place in the draw order (`zip(a, b)` for arrays visited per index); `forEachEntity` walks the list, a static_assert fails on a member left out | `vr_view_internal.hpp` |
| client state reset when the client's state is cleared (a map, a loaded game) | a call in `VR_OnClientClearState`; per-map data rebuilt: compare `worldGeneration()` (with the world pointer) | `vr_client.cpp`, `vr_main.hpp` |

### QC (refactor A4)

Use these instead of a local copy (refactor 2026-10-29, cluster A4).

- `QC/vr_util.qc`, end of file: `VR_Player()` (the first player: never spell `find(world, classname, "player")`),
  `VR_NextPlayer(p)`, `VR_PlayerAlive()` (the first living one), `clamp(v, lo, hi)`, `VR_Wrap180(a)` (-180 .. 180,
  floor form; the base packs' one-step `if(d > 180) d -= 360` wraps are a different rule and stay), `VR_Cross(a, b)`,
  `VR_Lerp(a, b, t)`, `VR_Smooth(x)` (clamped smoothstep), `VR_PointSegmentDistance(p, a, b)`,
  `VR_SegmentDistance(a0, a1, b0, b1)` (closest points in `vr_seg_s`, `vr_seg_t`) and `VR_SegmentDistanceEps` (the
  same with a site's own thresholds).
- `QC/vr_testutil.qc` (tests only): `VR_Test_Begin("tag")`, `VR_Test_Check(ok, what)` (`<tag>: PASS|FAIL <what>`),
  `VR_Test_Verdict()` (`<tag>: <p> passed, <f> failed`), one tally (`vr_test_pass`, `vr_test_fail`). The armed grunts'
  harness (`VR_AGT_Check/Print/Spawn/Remove/Hunt`, vr_armedgrunt_test.qc: `agtest: <what> PASS|FAIL` by bprint) is
  the other shared one; a new test uses one of the two, never a third copy.
- Cvars: read with a handle, `cvar_hget(cvarh_<name>)` (`QC/vr_cvars.qc`, compiled right after defs.qc so every file
  sees it); `cvar("...")` only in tests and one-off code. A vr_cvars.inc cvar's handle is generated: use
  `cvarh_<name>` and run `python Misc/quakevr/gen_vr_cvars_qc.py` (it writes `QC/vr_cvars_gen.qc`, a handle for every
  one the QC uses and no other: each costs a global; `--check` fails a stale file). Only the engine's own cvars and the
  ones vr_cvars.inc makes through its macros are hand-written in vr_cvars.qc (a global and a `VR_CVAR_HMAKE` line).
  Handles are made again at every load (their numbers are the session's).
- A per-hand field of a pair: `VR_HAND_FIELD(e, xHand, loadportpos, offloadportpos)` (vr_util.qc), not a hand-written
  `xHand == cVR_MainHand ? e.loadportpos : e.offloadportpos`.
- Tables, not if-chains (refactor B1): a weapon's numbers, model, name, ammo and pickup classname are its row of
  `VR_WEAPONS` (`QC/vr_weaponutil.qc`, one `vr_weapon_row` a weapon id, in id order: a new weapon is a new row; the
  `WeaponIdTo*` functions read their column); the arena perks' name, text, card model and colour are `VR_ARK_PERKS`
  (vr_arena_perks.qc); ClientObituary's words are `VR_OBIT_MONSTERS` / `VR_OBIT_TRAPS` / `VR_OBIT_WEAPONS` (client.qc);
  the waves' monster kinds (arena and horde) `VR_WAVE_KINDS` (vr_arena.qc); the horde's squads and wave picks
  `MGH_SQUADS` / `MGH_WAVES` (vr_mg_horde.qc); AD's weapon upgrades `AD_UPGRADES` (vr_ad_bridge.qc).
  A struct array with an initializer (`MG_poses`, these) is the form; a value a setting or a function decides stays a
  line of code before the table read. Each table costs a global per value: QC globals are near the id format's 64k.
- Builtins: a `= #0` prototype is bound by name; `Misc/quakevr/check_builtins.py` (run by the kit's build.sh) fails the
  build when one names no engine builtin, or a `vrBuiltins` entry has no prototype.

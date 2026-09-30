# Code conventions (Quake/vr)

How the VR module keeps state. The rest of the style (formatting, naming, comments) is the surrounding code's: read a
neighbouring file and write like it.

## Scratch buffers and caches

A function that needs a container only to avoid allocating it again next time (a scratch buffer), or that keeps results
for a key (a cache), does **not** declare it `static` (or `thread_local`) inside the function. Hidden there it is never
given back (a one-off peak stays for the session), never counted, never emptied when what it was made from goes away,
and nothing says which thread may touch it. It goes into its system's state instead:

1. **Scratch** (its contents mean nothing from one call to the next): a member of the file's scratch struct, registered
   with `mem::Scratch` (`vr_mem.hpp`). The function takes a reference and clears it itself; the capacity stays from call
   to call (no allocation once warm), and the whole set is given back at every map change.

   ```cpp
   // The hold search's buffers (the server's frame: the main thread).
   struct ClimbScratch
   {
       std::vector<int> nearby;      // the ledges of one model near the hand (gather)
       std::vector<Candidate> found; // (findHold)
       auto members() { return std::tie(nearby, found); }
   };
   mem::Scratch<ClimbScratch> scratch{"climb"};

   void gather(...)
   {
       std::vector<int>& nearby = scratch.nearby;
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
3. **Per-frame state that is not a buffer** (what a frame built once and every view draws: `flashlight.cpp`'s
   `CordDraw`, `rope.cpp`'s `RopeDraw`; a debug command's totals): a named struct at file scope in the anonymous
   namespace, next to the system's other state. A debug command's working buffer is a plain local.
4. **Constants** built once (`static const std::array` of fixed data, a `std::regex`): `constexpr` when the type allows
   (`bodycal`'s `splitList`), else a namespace-scope `const` (made before `main`, read by any thread: `ao`'s `rayDirs`)
   or a function-local `static const` (its initialisation is thread-safe). Never mutable.

`vr_memstats` prints the registered sets' bytes (totals and the largest), `vr_limits` the totals.

`Misc/quakevr/check_statics.py` (run by the kit's `build.sh`, which fails on it) rejects a mutable function-local
`static std::...` or an indented `thread_local` in `Quake/vr`; a worker's own buffer that must stay ends with
`// statics-ok: <why>`.

## Threads

A worker thread owns what it touches: its job's copy of the data (`ao`'s `PoseJob`), locals, or a `thread_local` when
the same helper also runs on the main thread (`decals`'s atlas RNG). Registered sets are the main thread's alone
(`mem::on` and the reports walk them there). Results come back through the owner's queue under its mutex (`ao`'s
`BakeQueue`, `imgprefetch`'s items, `motion`'s futures). Box3D steps with one worker (the caller's), so its callbacks
run on the main thread.

## GL objects

GL names are plain ids (`gfx::Texture`, `gfx::Target`: no destructor), made and deleted by their system's functions
(its reset, `VR_Shutdown`), never by a static's destructor after the context is gone. `vid_restart` keeps the GL
context (only the engine's framebuffers are made again), so the VR module's GL objects live on.

## Allocations a frame

The profiler counts the main thread's C++ allocations (`vr_profile_report`: "allocations", avg/max a frame; the systems
CSV's column): `vr_alloccount.cpp` replaces `operator new` with a counting one. A system's frame should allocate nothing
once warm; its first frames after a map load grow its scratch sets again.

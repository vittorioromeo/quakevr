# QuakeC to C++ (qc2cpp, 2026-11-28)

His plan (2026-11-28): our QuakeC only runs in our engine and our engine only runs our QuakeC, so translate all the
QuakeC to C++ once, bake it into the engine and carry on in C++. This job builds the tools and proves them: **QuakeC
stays the source** until the splitscreen work is merged into next; then the switch (section 5).

What exists now:

- **The generator:** FTEQCC's C++ backend, `-cppout <dir>` (`Tools/fteqcc/qcc_cpp.c`, hooked by `cpp.patch`; the
  repo's `fteqcc64.exe` is rebuilt with it by `build_fteqcc.sh`, its progs.dat byte-identical with and without the
  flag). It writes the progs it just compiled: one `.cpp` per QuakeC file, `qc_funcs.h`, `qc_table.cpp`.
- **The build option:** `/p:QvrQcCpp=1` (or `QVR_QC_CPP=1` in the environment, e.g. `QVR_QC_CPP=1 bash build.sh
  <tree>`): the Visual Studio build generates into `build/qccpp` (a build output, never committed) and compiles it in
  (`QVR_QC_CPP`). Without it, nothing changes (vr_qccpp.cpp is a stub that never runs C++).
- **The switch:** `vr_qc_cpp 0` (default) the interpreter, `1` the C++ for every server QuakeC call;
  `vr_qc_cpp_info` says whether the built-in C++ matches the loaded progs (Debug > Profiling).
- **The check:** `Misc/quakevr/qc_cpp_compare.py <tree>`: seeded scenarios run with 0 and with 1, their
  `vr_bench_statehash` and `profile` totals compared.

## 1. Does anything load a progs other than ours? (the check, part a)

The engine loads one server progs, `progs.dat`, the first the search path finds (`sv_main.c` PR_LoadProgs), and never
checks it is ours (only the format and header CRC; `vr_progs.cpp` checkGameCode warns when the field `handpos` is
missing and plays on with VR gameplay off).

- **AD, Copper, Dwell, Quoth, the mission packs: compiled into our progs** (progs.src: 140 `ad/`, 61 `copper/`, 6
  `dwell/` units, the `hip_*`, `rogue_*`, `honey_*` files, vr_quoth.qc), spawn functions remapped by prefix (`ad_`,
  `cu_`, `dw_`; vr_rules.cpp). Their folders are blocked from supplying a progs.dat or csprogs.dat (VR_SkipSearchPath,
  skipInFolder in vr_ad/vr_dwell/vr_quoth); map packages with a progs are refused at install and MP download.
- **Another progs can still win:** `-game <mod>` (it goes above quakevr), the `game` command, or a quakevr folder with
  no progs.dat (then rogue/hipnotic/id1's vanilla one). Today that "works" with VR gameplay off.
- **CSQC:** Ironwail's simple CSQC still tries csprogs.dat then progs.dat at every signon (CL_LoadCSProgs, host.c),
  kept only if `CSQC_DrawHud` exists: our QC has none, so for us nothing loads; a mod's csprogs.dat above quakevr would.
  No menu QC loader exists.
- **Demos, MP clients:** clients never run server progs (the server is authoritative); demos run only the CSQC try.

**Verdict: nothing we ship or support loads another progs.** With only a baked-in progs, these stop working or need a
replacement: (1) running another mod's progs (`-game`, `game`, the vanilla fallback: drop it, or keep the interpreter
for foreign progs); (2) a mod's CSQC HUD (Ironwail feature; drop or keep the client VM); (3) saves resolve fields,
globals and function values by name: a name table is needed (the generator emits the function and field tables);
the save header's progs CRC needs a replacement (a build hash); (4) spawn by classname and map function keys, the
`ad_`/`cu_`/`dw_` remap, `callspawn`/`functionname`: a name -> function registry (the table); (5) bytecode tools:
`vr_qcexec_bench`/`vr_qcexec_run`, `traceon`, `profile`, `vr_qcprofile`, `vr_blowline_dump`, the `VR_TestImpulse`
check; (6) ~132 `findFunction` call sites, 395 `QVR_FIELD` bindings (vr_fields.inc) and ~40 global bindings become
direct C++ references.

## 2. Semantics the C++ must keep (the check, part b)

The generated C++ keeps every one of these by construction: it runs on the VM's own memory (`qcvm->globals`, the
edicts) and each statement is the interpreter's handler (`Quake/vr/qccpp/qccpp_rt.h`). The list is what the later
readable rewrite (section 5) must keep by hand.

- **Floats:** all arithmetic in float, each statement's operations in the VM's order; `OP_MUL_V` is
  `(a0*b0 + a1*b1) + a2*b2`; no FMA contraction (the generated files build with `-ffp-contract=off`; x64's baseline
  has no FMA anyway; Android sets it globally). `OP_STATE`'s `time + 0.1` is computed in double, as the VM does.
- **Bits, not values:** `if`/`ifnot` test the 32 bits (`-0.0` is true; a string is true when its offset is not 0);
  stores and loads copy bits. `OP_BITAND`/`OP_BITOR` convert with C's `(int)` (out of range: x64's 0x80000000; the
  generator never folds them at compile time). `OP_NE_S` stores strcmp's value, not 0/1. `OP_AND`/`OP_OR` are float
  truth (no short circuit on the operands, they are already computed).
- **Vectors:** three floats in consecutive globals or fields, element by element (an operand may be the result).
- **Entities:** byte offsets from `qcvm->edicts` (`PROG_TO_EDICT`); world is 0; `!ent` is "is world"; fields are int
  offsets into `edict_t.v`. `OP_ADDRESS` on world while the server is active is the "assignment to world entity" error.
- **Strings:** `string_t` offsets (progs strings, engine strings, tempstrings: PR_GetString resolves them); compare by
  contents (`==`, `!=`, `!s`: null or empty). Tempstrings and zone strings are the builtins' business (unchanged).
- **Calls:** parameters in `OFS_PARM0..7` (3 slots each), copied into the callee's locals at entry; return value in
  `OFS_RETURN` (`OP_DONE` copies globals 0-2 over 1-3, in order); locals saved on the locals stack at entry and put
  back at exit (recursion, and other functions overlapping the same slots: `-Olo`); locals are not zeroed (a "private
  locals" function keeps its values from the last call); `argc` set per call; builtins through `qcvm->builtins[]`
  with the extension check, Tracy zone and profiler timing; a NULL function value is the "NULL function" error.
- **Function values:** `think`/`touch`/`use`/`th_*` fields and function globals hold progs function indexes; saves
  store them by name (`ED_ParseEpair` looks the name up); spawn functions by classname (`ED_LoadFromFile`).
- **Globals the engine sets:** `self`, `other`, `time`, `world`, `msg_entity`, parm1-16... (globalvars_t at fixed
  offsets), plus ~40 found by name (vr_progs.cpp). Saved globals: DEF_SAVEGLOBAL defs, by name.
- **Limits and errors:** the runaway limit (0x1000000 statements per outermost or nested run), stack depth
  (MAX_STACK_DEPTH), locals stack size; PR_RunError prints `xstatement` and the stack (`xstatement` set at calls,
  returns and erroring statements, as the VM).
- **QVR hooks:** the edict index's field watch at `OP_ADDRESS` and watched stores at `OP_STOREP`
  (VR_EdictIndex_Address/Stored: findradius results depend on them), per-function `profile` counts (the `profile`
  command, `instr_total` and SV_RunThink's think costs).
- **Tests:** the QC tests run in QuakeC through impulses (`VR_TestImpulse`, ~100 `*_test.qc`); the engine only checks
  the function exists. In C++ they run as any other QuakeC.
- **Not kept in C++ mode:** `traceon` (statement tracing; use `vr_qc_cpp 0`), `vr_qcexec_bench`/`vr_qcexec_run`
  (they force the interpreter).

## 3. The generator (Tools/fteqcc/qcc_cpp.c)

- **From the compiler, not the bytecode:** it runs at the end of the compile (QCC_WriteData), over FTEQCC's own
  statements with their symbols, after every optimisation (exactly the statements progs.dat gets). Names come from the
  symbols: globals as `g::time`, locals as `l_dist` (constants in the function), fields as `fld::health`, calls as
  direct `qc::monster_use()` calls, builtins by number with their name in a comment; temps stay numbered. Each line of
  QuakeC is a comment above its C++ (the source files are read again for that).
- **Control flow** is labels and gotos (each jump target `s<statement>:`), counted per basic block for the runaway
  limit and `profile` (blocks end at jumps, calls and returns, so the counts are the VM's exactly).
- **Gating:** `qc_table.cpp` carries an FNV-1a hash of the statements and every function's first statement, and the
  function and field tables (names, offsets). At each progs load (`VR_QCCpp_ProgsLoaded`) the engine checks them on
  the first call; a mismatch (a QuakeC edit without regenerating, a `-game` mod's progs) prints why and keeps the
  interpreter.
- **Immediates** are inlined as float literals only when the literal has exactly the same bits; strings, vectors and
  everything else are read from the globals as the VM does (with the string or vector in a comment).
- **Size:** our progs (822,904 statements, 24,655 functions) gives 811 files, 58 MB, in ~7 s with the QC compile.

## 4. Proof and speed

- **Build:** `QVR_QC_CPP=1 bash <kit>/build.sh <tree>`. The generator writes `qc_sources.txt` (the files of this
  compile; the build compiles those, never a stale `.cpp` in the folder) and rewrites only changed files, with
  stable names (`qc_<qcfile>.cpp`), so a QuakeC edit recompiles its own files and `qc_table.cpp`. A whole first
  build adds ~2-3 min (817 files, 58 MB, compiled once each).
- **Compare** (`qc_cpp_compare.py <tree>`, 2026-11-28): e1m1, combat_48, hip1m1 (69 monsters woken, skill 2),
  arena_wave6, bots_dm4 (3 bots), saveload: every QuakeC field's hash over every edict at 3-4 checkpoints 300 frames
  apart, and the `profile` total (up to 122 M statements), equal between `vr_qc_cpp 0` and `1`. Left out: fields fed
  from the real clock (`vr_posetime`, the hands' motion history `mh_*`), which differ between two interpreter runs
  too (`--control`). **Open:** saveload in C++ matched 1 run of 3; the other two differed in one field only,
  `stl_light` after the load (the stealth light, read from the client's own measurement, `clientlight`, which the
  client takes every 0.1 s of `cl.time`), with the same profile total; the interpreter against itself matched 3 of 3.
  Suspect: the client's reconnect after the load lands on another frame relative to the server (timing), not a
  QuakeC difference; to check: print `clientlight` per frame after the load in both modes.
- **Whole suites in C++:** `QVR_QC_CPP_RUN=1` in the environment starts the game with `vr_qc_cpp 1` (every test
  launcher passes its environment on), e.g. `QVR_QC_CPP_RUN=1 python <kit>/scratch/refactor/run_baseline.py ...`.
- **Speed** (PC, Release without LTO, low-impact runs beside his game, so rough): `vr_qcexec_time` (QuakeC's time a
  frame, builtins included) interleaved 0/1 in one run: hip1m1's fight 2.00 -> 1.76 ms a frame (x1.13), combat_48
  2.13 -> 1.86 ms (x1.15), against the interpreter before origin/next's combined instructions. Builtins (traces,
  findradius, the VR code) are much of that time and are the same code either way (not measured apart). The readable
  pass (section 5: locals in registers, no per-block counting) is where the larger gain is.

## 5. The switch (after splitscreen merges)

1. **Freeze:** announce a QC freeze; merge every QC branch; regenerate with `-cppout`; run qc_cpp_compare and the
   full suites with `vr_qc_cpp 1` against the VM baseline.
2. **Readable pass (generator, still mechanical, re-provable):** recover `if`/`else`/`while` from the gotos (the
   jumps are structured: FTEQCC emits them from QuakeC's own statements), turn the temps into C++ locals and the
   locals that are written before read into C++ locals (the private-locals functions keep globals), parameters into
   C++ parameters (`OFS_PARM*` only at builtin and VM boundaries), real `bool` for comparisons feeding an `if`,
   `int` where QuakeC uses whole-number floats only as flags/counters (only where the compare harness stays 0),
   vectors as a `vec3` by const ref, fields as `ent->v.health`-style accessors through a generated edict struct.
   Each step: regenerate, qc_cpp_compare at 0 differences, then the next.
3. **Commit the generated C++** as the new source (`Quake/game/`, one file per old .qc), delete QC/ from the build
   (keep it in history), progs.dat no longer shipped: the engine loads its own built-in "progs" (the function, field
   and global tables from the generator, the globals' initial values as data) so saves, MP and the engine's name
   lookups keep working unchanged.
4. **Saves:** keep the name-based format (fields, globals and function values by name: the tables keep the names);
   the header's progs CRC becomes the game module's build hash; old saves load as they do today.
5. **Tests:** the QC tests (`*_test.qc`, impulses, QuakeC verdict lines) become C++ functions in the same game
   module, still started by the same impulses and console commands and printing the same verdicts, so the scripts
   and `QC/test_numbers.txt` carry over; later, a C++ test registry.
6. **Fast iteration:** the game code as a module with hot reload: a DLL on PC (`vr_game_reload`: the game state lives
   in the engine's memory, the globals and edicts, so a reload swaps code only), a `.so` on Android (dlopen; or a
   static link for release). The interpreter can stay for a release or two for `-game` mods (section 1), then go.
7. **Cleanup passes after:** hand ports of hot paths (agpose-style), dead code from the mods we embed, the builtins
   called as plain C++ functions, the edict index hooks folded into field setters.

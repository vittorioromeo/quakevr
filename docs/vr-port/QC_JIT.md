# QuakeC JIT or AOT for Quake VR (research, qcjit, 2026-11-28)

Question: would compiling QuakeC to native code (a JIT at load time, or AOT: progs.dat translated to C at build time)
pay on the Quest and PC, and is there existing work or a library to build on? Research only: no engine change.
Companions: QC_VM.md (the interpreter loop, vr_qcexec, computed goto since 2026-10), QUEST_OPTIMISATIONS.md ("hip1m1's
fights: QuakeC"), ROUND26 "questopt2" / "qcnative".

**Answer, short:** no JIT. The interpreted part of QuakeC is about half of QuakeC's time (the rest is builtins, already
native), and most of that half is not instruction dispatch but calls (locals saved and restored), builtin marshalling
and memory traffic on 17 KB edicts, which a JIT or AOT does not remove. Best realistic saving: **Quest ~0.4-0.7 ms,
PC ~0.15-0.3 ms a frame** in the 69-monster hip1m1 fight, for weeks of work (a JIT: two backends) and new failure modes.
If anything, a **profile-guided AOT of the ~100-200 hottest functions of our own progs** (progs to C++ at build time,
CRC-gated, the interpreter as fallback, a compare mode) is the one option worth a 2-3 day spike, and only after the
interpreter work (qcvm) and the hand ports (agpose, agstruck: 0.56 -> 0.10 ms for the pose alone) run out.

## 1. What QuakeC costs now (measured)

hip1m1, skill 2, god, `vr_test_monsters 4` then `2` (69 hunting: 40 armed humanoids, 29 worn packs), 300 frames
settled, then 720 frames measured. PC, this tree's Release (no LTO), mock headset, fixed frames (`-QuitOn`, no waits on
the clock). Scripts: worktree `scratch/qcjit_*` (not committed).

| | PC | Quest 3 | source |
|---|---|---|---|
| QC statements a frame | **96,700** (69.6 M in 720 frames, `profile`) | same scene, same progs | this run |
| QuakeC a frame, no profiler | 1.12 ms | 2.83 ms | ROUND26 qcnative (perf recorder) |
| QuakeC a frame, `vr_qcprofile 1` | 1.81 ms | 3.9-5.7 ms | this run; questopt2/qcnative |
| builtins' self time (top 15) | 0.99 ms of the 1.81 | ~half | this run; questopt2 |
| interpreted part (QuakeC minus builtins), no profiler | **~0.5-0.6 ms** | **~1.4 ms** | derived |
| per statement, averaged (calls included) | ~5-6 ns | ~14 ns (taking the PC's count) | derived |

- Our progs: 24,910 QuakeC functions, 822,324 statements, 86,251 globals (345 KB), 4,340 entity fields (**17 KB an
  edict**), 102 builtins; vanilla opcodes only (66, up to OP_BITOR) in FTE's 32-bit statement format.
- The hot set is small: the 25 costliest functions run 55% of the statements (`VR_AG_PropThink` 4.5 M, `VR_AGP_Lod`
  4.3 M, `VR_AG_Class` 2.9 M, `VR_WP_Tug` 2.7 M, `VR_WP_Think` 2.6 M...); 24,885 others share the rest.
- QC_VM.md measured the dispatch-bound cost at ~2.2 ns a statement before computed goto, 1.1-1.5 after, in tight
  arithmetic loops. The fight's ~5-6 ns a statement is therefore mostly **not dispatch**: entering and leaving
  functions (PR_EnterFunction saves every local of the callee to the locals stack and copies parameters; QuakeC
  functions here are short and called often), the builtin call path (checks, profiling
  hooks, `xstatement`), and loads and stores to globals and 17 KB edicts spread over memory (cache misses).
- A data point from elsewhere: QcNat.hs (2023), a progs-to-C transpiler wired into DarkPlaces for Xonotic, "sort of
  worked" and its author found "the performance was the same" as the interpreter. Same lesson: in real game code the
  interpreter is not the bottleneck.

## 2. Existing work

| Project | What | Targets | License | State | Use to us |
|---|---|---|---|---|---|
| FTEQW qclib `pr_x86.c` ("QCJIT") | Load-time QuakeC JIT ("JIT ... load time, not execution time"); bails to the interpreter on any unsupported opcode | **x86 32-bit only** (x87, 32-bit addresses) | FTE is GPLv2+ (the file has no header) | `#ifdef QCJIT`, off; "optimisations: none at the moment"; FIXMEs (unbounded entity numbers) | Design notes only (how calls, builtins, STATE, returns were wired); no x86-64, no arm64 |
| Quake3e `vm_x86.c`, `vm_aarch64.c`, `vm_armv7l.c` | Load-time JIT for Q3's QVM bytecode (int stack machine, not QuakeC) | x86, x86-64, **aarch64**, armv7 | GPLv2+ (compatible) | Maintained (copyright to 2026) | The best reference for a production arm64 JIT in a Quake engine: mmap RW, mprotect RX (W^X), `__clear_cache`, register caching, literal pool, ~1,300 lines. Different bytecode: not reusable as is |
| ioquake3 `vm_x86.c`, `vm_armv7l.c`, `vm_powerpc.c` | The same lineage, older | x86/x86-64, armv7, ppc | GPLv2+ | Maintained, no aarch64 JIT | As above |
| ftrvxmtrx/progsdat | progs.dat to C decompiler (Haskell), meant for "necroquake" to run game logic natively | C output | (LICENSE file, kind not shown) | Archived 2020, 15 commits | Proof of concept only |
| QcNat.hs (Xonotic forum, 2023) | progs to C, one C function per QC function, replacing DarkPlaces' `prvm_executeprogram.h` | C output | snippet | Experimental; variadic PARMn bug | "Performance was the same" (see 1) |
| DarkPlaces, QuakeSpasm(-Spiked), vkQuake, Ironwail | Interpreters (switch or computed goto) | all | GPLv2+ | Maintained | Nothing native; our vr_qcexec is at least as fast |
| qfusion/Warsow, Quake 2 / Q3 lineage | Moved game code to C/C++ DLLs or AngelScript instead of making QuakeC fast | | | | The industry's answer: native game code (what our hand ports do function by function) |

No maintained QuakeC JIT targets x86-64 or arm64; no maintained QC-to-C tool exists.

## 3. Libraries for a JIT (if ever)

| Library | License (vs our GPLv2-or-later) | x86-64 / arm64 | Size, level | Notes |
|---|---|---|---|---|
| sljit | BSD-2: fine | yes / yes (+arm32, riscv, ppc, s390x, loongarch, mips) | small, low-level portable assembler (virtual registers, no RA) | Used by PCRE2's JIT; the best fit for a template JIT over QC's 66 three-address ops |
| AsmJit | zlib: fine | yes / yes | ~500 KB, C++ (Assembler, Builder, Compiler with RA), no STL/exceptions | `JitAllocator` does dual-mapped pages for W^X systems; two backends to write (one per ISA) |
| DynASM (LuaJIT) | MIT: fine | yes / yes | tiny runtime; build-time preprocessor needs Lua | Per-ISA hand assembly; what LuaJIT's interpreter uses |
| MIR (vnmakarov) | MIT: fine | yes / yes (Linux, macOS; **Windows not listed**) | ~23 k lines, ~557 KB; IR with RA and optimisations; c2m C compiler on top | Code within ~10% of gcc -O2 on its benchmarks, compiles ~100x faster than gcc; could compile transpiled C at load time (mods' progs) |
| libtcc (TinyCC, mob branch) | LGPL-2.1: fine | yes / yes (arm64-gen.c) | small C compiler in a library | Compile transpiled C in memory; -O0-like code; original author inactive, community-maintained |
| Copy-and-patch (technique) | none | per target, stencils from clang at build time | no runtime dependency | Xu & Kjolstad 2021: ~10x over interpretation, compiles 100x faster than LLVM -O0; CPython 3.13's JIT (PEP 744). Would reuse the AOT's per-opcode C as stencils |
| GNU lightning / Lightening | LGPLv3+: forces the combined work to GPLv3 (allowed by "or later", but a relicensing decision) | yes / yes | small | Not worth the licence question |
| libgccjit | GPLv3+ | (gcc targets) | whole GCC backend | GPLv3 and huge; no MSVC/NDK story. No |
| LLVM ORC | Apache-2.0 WITH LLVM-exception: GPLv3-compatible only | yes / yes | tens of MB, slow compiles | Too big for an APK; the same licence issue. No |
| Cranelift | Apache-2.0 WITH LLVM-exception | yes / yes | Rust crate, no stable C API | Adds a Rust toolchain and the licence issue. No |
| xbyak / xbyak_aarch64 | BSD-3 / Apache-2.0 | x86 only / arm64 only | header-only | Split backends; Apache-2.0 half not GPLv2-compatible |

### Can we JIT at all? (platforms)

- **Quest (Horizon OS, Android-based):** yes technically. AOSP's sepolicy grants every app `allow appdomain
  self:process execmem;` ("WebView and other application-specific JIT compilers"), so an app may map anonymous
  memory executable (Quake3e's RW then RX `mprotect` flow, `__clear_cache` for the icache). Android 10+'s W^X rule
  forbids exec()-ing files from app data, not JIT memory. Meta's app policies (updated 2026-03) say nothing about
  JIT, interpreters or dynamic code; the code would come from the APK's own progs, not a download. Residual risks:
  a future Horizon OS hardening (execmem removal, BTI enforcement for generated code) and store review surprises.
- **Windows:** `VirtualAlloc` RW then `VirtualProtect` RX; no ACG for ordinary desktop apps. Fine.
- **Linux / Flatpak:** anonymous `mmap` + `mprotect(PROT_EXEC)` works inside the Flatpak sandbox (browsers JIT there).
- **AOT needs none of this:** the code is compiled by the toolchains we already use (clang-cl, NDK clang).

## 4. AOT: our progs to C++ at build time

We build and ship our own progs.dat, so the hot functions can be translated by a build step (a Python script reading
progs.dat, `Misc/quakevr/qc2c.py`) into C++ compiled into the engine; any other progs (mods: AD, Copper...) keeps the
interpreter.

- **Same memory, so saves and MP are untouched.** The generated code reads and writes the same `qcvm->globals` and
  edict field arrays as the interpreter (each opcode as the interpreter's own handler: the same float steps); nothing
  about globals, fields or saves changes. Saves store fields and globals by name, never code.
- **Gating.** Each generated function carries the hash of its statements (and the progs CRC); at load the engine binds
  only functions whose statements match, so a QC edit without regeneration falls back to the interpreter, never runs
  stale native code. func_t stays an index; an AOT table maps index to native function.
- **Exactness.** `-ffp-contract=off` is already on everywhere (Android/android.cmake, quakevr.props), so no FMA
  contraction; QC's float compares, NaN behaviour, `ftoi` and divisions match C's when written as the handlers are.
  Prove it the way agpose did: a cvar `vr_qc_aot` 0/1/2, 2 running both from the same state and comparing every global
  and the touched edicts (the agpose/agstruck compare harness), plus the seeded-fight state hash and `profile` counts.
- **What must be kept:** the runaway limit (count per basic block, check at back edges and calls), `traceon` (on
  trace, finish the call in the interpreter: the AOT function hands over at the next statement), `xstatement` before
  builtins and errors (PR_RunError's stack trace names the right statement), OP_ADDRESS's world check and the field
  watch (vr_qcexec's hooks), the `profile` and `vr_qcprofile` accounting (credited per call), PR_EnterFunction /
  PR_LeaveFunction semantics (locals saved, recursion) though the copies can be unrolled with known sizes.
- **Size.** All 822 k statements would be tens of MB of C++ and minutes of NDK compile; the 25 hottest functions are 55%
  of the statements, ~200 would cover most of a fight. Profile-guided list (from `profile` over the test fights),
  regenerated by build.sh.
- **Debugging.** QuakeC errors keep their messages and stack (above); stepping in generated C++ is possible but ugly;
  `vr_qc_aot 0` is the first answer to any QuakeC bug report.
- **Compared with hand ports** (agpose, agstruck, agparry, propthink): a hand port also changes the algorithm (quiet
  frames skipped, scans shared) and usually saves more per function (the pose: 0.56 -> 0.10 ms on PC); AOT is mechanical,
  keeps QuakeC the source of truth, and helps every hot function a little.

## 5. Gain, effort, risk

Upper bound: the whole interpreted part gone (impossible: memory traffic and calls remain): Quest ~1.4 ms, PC
~0.55 ms. Realistic, from the split in section 1 (dispatch ~1.2 ns of the ~5-6 ns a statement on PC; calls and memory
the rest):

| Option | Quest saving (hip1m1 fight) | PC saving | Effort | Risks |
|---|---|---|---|---|
| Interpreter (qcvm tonight; computed goto and the per-op counters were done in QC_VM.md's vr_qcexec): pre-decoded statements, superinstructions, a cheaper PR_EnterFunction/LeaveFunction | ~0.2-0.4 ms | ~0.05-0.15 ms | days | low; helps mods too |
| Hand ports of the remaining hot props (`VR_AG_PropThink`'s scans, `VR_AGP_Lod`, `VR_AG_Class`, `VR_WP_Tug/Think`), the agpose pattern | ~0.3-1 ms (per item) | ~0.1-0.3 ms | days per item | low with compare mode 2; QuakeC and C++ to keep in step |
| **AOT, hot functions of our progs** | ~0.4-0.7 ms | ~0.15-0.3 ms | spike 2-3 days; production 1-2 weeks | stale code (hash-gated), build step on two toolchains, exactness (compare mode), profiler/trace semantics |
| JIT (sljit or AsmJit, all progs incl. mods) | ~0.3-0.6 ms (a template JIT keeps the same calls and memory traffic) | ~0.1-0.25 ms | 3-6 weeks (two ISAs), long tail of bugs | executable memory policy, crashes in generated code hard to debug, security surface, Horizon OS hardening |

For scale: the Quest's CPU in his hip1m1 fights is 24-41 ms a frame (13.9 at 72 Hz); 0.5 ms is 1-2% of it. The
bigger Quest CPU items stay the renderer's (alias set-up ~4 ms, view entities ~2 ms, sound ~1.5 ms;
QUEST_OPTIMISATIONS.md).

## 6. Recommendation (smallest first)

1. **Let qcvm finish the interpreter work** (no format change; mods gain too). Then re-measure the interpreted part
   (QuakeC minus builtins, perf recorder, hip1m1 fight, Quest) as the baseline.
2. **Keep porting the hottest props by hand** while one function or subsystem dominates (the current list in
   ROUND26 "questopt2"); it beats any compiler because it also changes what runs.
3. **Spike the AOT only if 1-2 leave > ~1 ms of interpreted QuakeC on the Quest:** `qc2c.py` translating the 25
   hottest functions, each opcode as vr_qcexec's handler; non-AOT calls and builtins through the existing paths; a
   `vr_qc_aot` 0/1/2 cvar (Debug > Profiling); measure on the hip1m1 fight on PC and Quest. Go on to production
   (hash gating, ~200 functions, build.sh and Android cmake steps, the trace/runaway/profile semantics) only for
   >= 0.5 ms on the Quest.
4. **No JIT** unless a mod's progs (not ours) becomes the Quest's bottleneck; then sljit (BSD-2, both ISAs) or
   copy-and-patch with the AOT's per-opcode C as stencils, following Quake3e's executable-memory flow.

## Sources

- FTEQW qclib, pr_x86.c (QCJIT, x86-32): https://github.com/fte-team/fteqw/blob/master/engine/qclib/pr_x86.c
- Quake3e JITs (vm_aarch64.c, vm_x86.c, vm_armv7l.c): https://github.com/ec-/Quake3e/tree/master/code/qcommon
- ioquake3 QVM JIT: https://github.com/ioquake/ioq3/tree/main/code/qcommon ; QVM background:
  https://fabiensanglard.net/quake3/qvm.php
- progs.dat to C decompiler (archived): https://github.com/ftrvxmtrx/progsdat
- QcNat.hs, progs to C in DarkPlaces ("performance was the same"): https://forums.xonotic.org/showthread.php?tid=9801
- DarkPlaces interpreter: https://xonotic.org/doxygen/darkplaces/prvm__execprogram_8h.html
- AOSP sepolicy, appdomain execmem: https://android.googlesource.com/platform/system/sepolicy/+/refs/heads/main/private/app.te
- Meta Horizon app policies: https://developers.meta.com/horizon/policy/app-policies
- sljit: https://github.com/zherczeg/sljit ; AsmJit: https://asmjit.com/ ; MIR: https://github.com/vnmakarov/mir ;
  DynASM: https://luajit.org/dynasm.html ; TinyCC: https://bellard.org/tcc/ ; GNU lightning:
  https://www.gnu.org/software/lightning/ ; LLVM ORC: https://llvm.org/docs/ORCv2.html ; Cranelift: https://cranelift.dev/
- Copy-and-patch compilation (Xu, Kjolstad): https://arxiv.org/abs/2011.13127 ; CPython's JIT, PEP 744:
  https://peps.python.org/pep-0744/

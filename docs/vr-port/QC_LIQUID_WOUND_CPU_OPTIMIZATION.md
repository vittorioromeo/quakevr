# Liquid and wound frame scans: CPU optimization

Measured 2026-10-14 against fetched origin/next **a9946d4914c662e0038d0ac5f5b711c760d27176**.
Moving only the pure interval/eligibility predicates into the existing live native flag iterator saves
about **12–14 microseconds per frame of coarse AD QC VM time** in this mock workload.
No settings, intervals, quality, damage, callbacks or RNG calls change.

## Change and equivalence

`findflagsdue(start, flagsField, mask, deadlineField, now, requiredField)` uses the existing `nextFlagged`
iterator and returns the first live candidate satisfying `deadline <= now && required != 0.f`.
`now` is the QC float passed by the caller, rather than the engine's double clock. Equality/unordered
comparisons and signed-zero/NaN eligibility follow the original compiled QC predicates. Invalid field
offsets raise the usual QC runtime error.

Each call resumes after the previous edict and reads current fields. No candidate snapshot, deadline
cache, scheduling bucket or lifecycle state is added. A due callback can change flags/fields, remove
edicts or spawn later edicts before the next call. The original cvar early exit, callback order,
interval writes, corpse behavior and model/takedamage gates remain in effect. Existing findflags and
arbitrary mod QC remain unchanged; the new builtin is bound by name for QC declaring it.

Files: QC/builtins.qc, QC/vr_liquids.qc, QC/vr_wounds.qc, Quake/vr/vr_builtins.cpp.
No new cvars/defaults or persistent entity fields.

## Separating scan/gate and due work

A separate baseline diagnostic extracts the unchanged due wound body into a private named QC function;
the liquid due body already has one. Its extra dispatch is instrumentation, so this single diagnostic
is not an optimization comparison. In a 120-frame AD window:

| Frame function | Scan/gate plus small frame overhead (ms/frame) | Due body (ms/frame) | Due calls |
|---|---:|---:|---:|
| Liquids | 0.022291 | 0.006636 | 576 |
| Wounds | 0.018185 | 0.002447 | 262 |

Subtracting due-body inclusive zones from enclosing frame zones includes iterator/builtin work in
scan/gate time. Repeated candidate checks dominate the due work here. The optimization retains the
native scan and removes repeated QC dispatch/bytecode around skipped candidates.

## Matched measurements

Normal Release and Release-Tracy were built from baseline and final sources. Their executable and
progs SHA-256s are distinct and retained. Three interleaved baseline/final pairs use those exact saved
outputs, with per-launch hash manifests; no incremental-build timestamp assumptions enter the comparison.

Hidden headless mock, flatscreen renderer, exclusive runs, 250 FPS cap, fixed frames/host_framerate 0.004,
all three seeds 7, vr_profile 0. Coarse captures leave QC/memory/GPU detail at 0; separate 120-frame
windows enable only QC detail after load/warmup. Every recorded setting and benchmark count matches
within each pair. AD steady has 206 live monsters; action starts with 214 and ends with 212, identically.

Mean **ms/frame**, with range of three run means; saving in **microseconds/frame**:

| Scope | Baseline | Final | Saving |
|---|---:|---:|---:|
| Detailed liquids frame | 0.029868 (0.029527–0.030537) | 0.015257 (0.013551–0.017240) | 14.61 |
| Detailed wounds frame | 0.021974 (0.021275–0.023216) | 0.007129 (0.006744–0.007404) | 14.85 |
| Coarse main-thread QC VM, steady | 0.247920 (0.244337–0.253618) | 0.233991 (0.231482–0.236135) | 13.93 |
| Coarse main-thread QC VM, action | 0.337846 (0.332121–0.341092) | 0.325837 (0.323930–0.328163) | 12.01 |

Detailed liquids+wounds time drops from 0.051842 to 0.022386 ms/frame: **29.46 µs (56.8%)**.
All coarse QC VM pairs also improve. Due calls remain **576 liquid / 262 wound level** per window.
Combined findflags/findflagsdue dispatches fall from 47,308 to 1,706 per 120 frames: fewer VM/builtin
transitions, with the same eligible entities, sampling intervals and gameplay work.

Whole-server/frame distributions overlap; unrelated GPU/callback timings drift between sessions.
Detailed QC instrumentation costs time, so its larger saving is not a normal-build FPS prediction.
The coarse QC VM saving is the narrower practical result. A single vanilla e1m1 pair preserves counts;
it supports coverage rather than a repeated vanilla performance claim. All 27 used decoded traces are
clean (zero nesting/drop/force-close errors). The unused initial empty-map before_qc.* trace is excluded.

## Exactness and validation

Private QC instrumentation compares every optimized answer with the original QC findflags loop and
predicates in the same live world. Cases cover both masks, equal/future/past deadlines, NaN/±infinity,
zero/negative/NaN eligibility, flag mutations, remove/spawn/reuse and map reload. The final edge fixture
gives a NaN deadline nonzero eligibility and includes NaN/infinite/signed-zero now and required values.
It generates signed zero at runtime as -1/Inf, reports -0 and passes **2,989 oracle comparisons**.

Paired gameplay scripts compare ordered due-callback inputs, woundevent parameters and burn/drown logs,
plus every field hash and complete edict-state hash at **30 checkpoints**:

- e1m1 slime, e1m7 lava/death, spawning, damage/debug settings on/off, vanilla/native AD transitions,
  save/load in both campaigns: **7,814 ordered records**, 23 checkpoints, seven lifecycle fixtures, all equal.
- Four confirmed fully submerged actors (water level 3): normal drowning, swimmer immunity, clean dummy
  with gore 0→1, modelindex 0, damage off/on, save/load: **256 records**, seven checkpoints, all equal.
  Normal/modelindex-zero actors drown for 4 then 6 damage; swimmer/dummy immunity is retained.

Both instrumented variants declare the same builtin/functions to avoid function-index-only entity-hash
differences. No instrumentation, fixture globals, test cvars or logging ships. Final source snapshots and
normal/Tracy/progs hashes match the restored tested outputs. Normal QC/FGD/statics/precedence/engine build,
Release-Tracy build and final normal e1m1 smoke pass; shipped QC has zero warnings.

## Evidence and VR follow-up

Private scripts, source snapshots, executable/progs variants, build logs, state/event comparisons,
benchmark JSONs, captures and decoded results are backed up at **C:/OHWorkspace/qvr-kit/scratch/qcscanopt**.
Primary results: paired_timing_comparison.json, paired_validation.json, paired_manifest.json,
equivalence.json, water_equivalence.json, edge_fixture.game.log, restored_final_hashes.json.
The capture, diagnostic, oracle/instrumentation, lifecycle/water and decoding/analysis scripts retain
the experiment workflow. No id assets are committed.

Ordinary real-headset combat in water/slime/lava, wet corpses and clean/gory training dummies remain
useful follow-up at the author's resolution. No visual or gameplay difference is expected. Keep private
evidence; three quakevr/qcscan*_private.sav test saves may be deleted by the author later.

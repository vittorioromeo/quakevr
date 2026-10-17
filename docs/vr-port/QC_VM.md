# The QuakeC VM's loop (vr_qcexec)

Worktree `qcvm`, 2026-10-15. BACKLOG.md, "QuakeC VM overhead, short of a JIT" and "Faster QuakeC interpreter: research
notes". Files: `Quake/vr/vr_qcexec.c` (the hook, the cvar, the bench), `Quake/vr/vr_qcexec_loop.inc` (the loop, built
twice: computed goto and switch); `pr_exec.c` includes the first (it uses that file's statics) and calls it first in
`PR_ExecuteProgramRun`.

## 0. How much of the frame is QuakeC

Exclusive runs, the mock headset, fixed 90 Hz game frames run uncapped (frame times are the CPU's work, not a headset's
pacing), 900 measured frames each, original loop. `quakec` is vr_profile's scope round every outermost call into the
VM: the interpreter's own work plus every builtin it calls (traces, movetogoal, findradius...). The interpreter's own
share comes from a vr_qcprofile run (QuakeC total minus every builtin's self time; vr_qcprofile itself adds ~0.1 ms).

| scene | frame ms | quakec ms (share) | QC statements a frame | interpreter alone ms (share) |
|---|---|---|---|---|
| ai_crowd_64 (64 monsters, models not drawn) | 3.42 | 0.306 (9.0%) | 12179 | ~0.24 (~7%) |
| combat_48 (48 monsters fighting, gore) | 5.54 | 0.307 (5.5%) | 14662 | ~0.19 (~3.5%) |
| e1m1, 24 spawned monsters fighting | 4.84 | 0.210 (4.3%) | 10656 | ~0.20 (~4%) |
| AD (ad_crucial), 24 spawned monsters | 7.89 | 0.568 (7.2%) | 18805 | ~0.26 (~3.3%) |

Against a 90 Hz headset's 11.1 ms budget the interpreter alone is 1.7-2.3% of the frame, QuakeC with its builtins
2-5%. Both are under the ~10% the plan set for going further, so steps 3-4 (pre-decoded statements,
superinstructions) were not done: the gain left (at most another ~0.1 ms a frame) is smaller than the risk of a second
statement format kept in step with saves, errors and the profilers. The costliest QuakeC is in a few functions
(`forcegrabbable_think_impl` 5-18% of the instructions, VR_Forcegrab_FindTarget, VR_AI_MoveToGoal); moving those to
native code (or the backlog's native filters/lists) is the better next step if QuakeC ever matters.

## 1-2. The new loop

`vr_qcexec` (archived): 1, the default, the new loop with computed-goto dispatch; 2, the same loop with a switch (to
compare, and what a compiler without computed goto gets); 0, the original loop in pr_exec.c, untouched. Debug >
Profiling: "QuakeC Loop" (the cvar) and "QuakeC Loop Bench".

What changed, against the original's per-statement work (`++profile`, a runaway compare, an `if (qcvm->trace)` load,
`qcvm->globals` reloaded for each operand):

- **Locals.** The VM, its globals, edicts and statements in locals; each handler reads its operand indices once.
  Reloaded after every builtin (one may switch VMs or change the trace flag).
- **The statement count is implicit.** count = (st - statements) + off; off changes only where control jumps (a taken
  IF/IFNOT/GOTO, a call, a return), so straight-line code counts nothing. The profile totals (`profile`, vr_profile's
  "quakec by its own instructions", calls into `xfunction->profile`) are added at calls and returns exactly as before.
- **Runaway checked where off changes** (taken jumps, calls, returns) and before anything that leaves the VM
  (builtins, the edict index's hooks, errors). Any statement that crossed the limit is then in the current straight
  run, so the error names the same statement the original's per-statement check did (count = 0x1000001), with the same
  stack trace, and nothing outside the VM ran after it.
- **Tracing** (traceon) is a second dispatch table whose every entry checks the runaway, prints the statement and
  jumps to the real handler (the switch build: one flag in a register). Set at entry and after each builtin, the only
  places the flag can change.
- **Computed goto**: each handler ends in its own indirect jump (`goto *table[op]`, with a bounds check that sends a bad
  opcode to the original's "Bad opcode" error); the switch build shares one.
- Unchanged: builtins' calls (the same code: extension check, Tracy zone, vr_qcprofile and vr_profile_fine timing),
  PR_EnterFunction/PR_LeaveFunction, the QVR hooks (OP_ADDRESS's world check and field watch, OP_STOREP's watched
  store), `xstatement` set at the same points, PR_PrintStatement for traces and errors.

### Checks (all three loops)

- Seeded fights (`sv_random_seed 7`, fixed frames, 24 spawned monsters, 600 frames, a weapon change halfway):
  `vr_bench_statehash` at frames 300 and 600 and every function's `profile` count equal for loops 0, 0 again, 1 and
  2: e1m1 (1127 functions, 12,001,300 statements), AD's ad_crucial (1106, 17,784,242), vrfiringrange (1026,
  11,261,177).
- `vr_qcexec_run`: the same return bits and counts; `vr_qcexec_run trace` (tracing from the first fabs) prints the
  same 149-line trace; a runaway (`ad_mathlib_pow 2 <inf>`, crossing the limit mid-loop on a DIV; `ad_mathlib_fast_pow
  2 <inf>`, on the loop's GOTO) prints the same statement, stack and message.
- `vr_qcexec_bench`: equal results and counts on every function tried.

### Speed

Microbenchmark (`vr_qcexec_bench`, exclusive, best of 7 rounds alternated; AD's math library: arithmetic loops with a
few fabs/rint builtins):

| function (args) | statements a call | original ns | goto ns | switch ns |
|---|---|---|---|---|
| ad_mathlib_pow (1.0001, 2500.5) | 28 | 80.0 | 66.8 (1.20x) | 66.1 (1.21x) |
| ad_mathlib_pow (1.5, 7.25) | 155 | 340.5 | 228.0 (1.49x) | 276.7 (1.23x) |
| ad_mathlib_sqrt (123456.7) | 119 | 260.8 | 169.2 (1.54x) | 213.2 (1.22x) |
| ad_mathlib_fast_pow (1.001, 300) | 87 | 164.2 | 96.9 (1.69x) | 126.0 (1.30x) |

About 2.2 ns a statement before, 1.1-1.5 after in loops; short calls gain less (entering and leaving a function, with
its locals copied, is the same code as before).

Whole frame (exclusive, 900 frames each, vr_profile's `quakec` scope, builtins included; the median of 5 runs
alternated between the loops, e1m1 one run each):

| scene | original ms | goto ms |
|---|---|---|
| ai_crowd_64 | 0.315 | 0.275 (-13%) |
| AD ad_crucial fight | 0.455 | 0.411 (-10%) |
| combat_48 | 0.280 | 0.260 (-7%) |
| e1m1 fight | 0.203 | 0.187 (-8%) |

The switch build lands near the goto one in game (0.18-0.41 ms). The saving, 0.02-0.045 ms a frame, is under 1% of
the frame, as step 0 predicted; frame totals move less than their run-to-run noise (single runs see 20-150 ms hitches
from the spawns, which dominate the averages).

## Not done (steps 3-4) and what would pay more

- Pre-decoded statements (a handler pointer and operand pointers per statement, built at progs load, the original
  kept for errors and the profilers) and superinstructions (field load + IF/IFNOT, ADDRESS + STOREP): perhaps another
  1.3-1.5x on the interpreter, i.e. ~0.03 ms a frame. Not worth a second statement format.
- Native versions of the few hot QuakeC functions (forcegrabbable_think_impl runs 5-18% of all statements on these
  scenes; done below) or the backlog's native filters (`findfilter`, `forall`) cut statements rather than their cost.

## Native prop think (worktree `qcvm`, branch `agent/fgnative`, 2026-10-15)

(2026-10-16: the native is the only one. `forcegrabbable_think_qc`, `vr_prop_think_native` and `vr_propthink_check`
below were removed (ROUND22.md, "A/B switches removed"); `forcegrabbable_think_impl` is `propthink(particles);`.)

`forcegrabbable_think_impl` (QC/weapons.qc: water entry and exit, slowing in water, the dust puff, settling a thrown
weapon) runs for every thrown or dropped weapon, pickup-as-object and bag, 50 times a second each. Its work is now the
`propthink` builtin (`Quake/vr/vr_propthink.cpp`), the way liquids and wounds moved their gates (d05011ec3): the
QuakeC stays as `forcegrabbable_think_qc`, and `forcegrabbable_think_impl` is `if(!propthink(particles))
forcegrabbable_think_qc();`. `vr_prop_think_native` (archived, 1; Debug > Profiling "Native Prop Think") 0 runs the
QuakeC.

- **Same steps:** every field read and written in QuakeC's float steps (its constants as floats: `0.85f`, `0.15f`,
  `0.3f`; `(int)` casts for its bit ops), random() drawn only where the QuakeC draws it (`PR_RandomNumber`, PF_random's
  body: the server's stream, the probes' numbers), the same particle2 message (`progs::sendParticle2`), the model's
  test as `!= string_null` (not empty), NaN angles straightened as the QuakeC does (its dprint too). The edict index
  is told of `.flags` (FL_INWATER is an indexed bit) as the QuakeC's STOREP tells it (`edictindex::fieldStored`).
- **Declines** (returns FALSE, nothing touched, the QuakeC runs): the cvar 0, a progs without one of the fields, self
  the world (the QuakeC's own error), or the angle the settling would bring round beyond 65536 or infinite (the
  QuakeC's loop: long, or forever to the runaway error). Decided before anything is written: those inputs aren't
  changed by the water part.
- **`vr_propthink_check [states]`** (Debug > Profiling "Prop Think Check"): on each thing thinking
  forcegrabbable_think, forcegrabbable_item_think or wpnthrow_think, as it is and in `states` (200) made-up states
  (waterlevel/type, ammocounter, rigid or not, velocities round the 10 u/s edge and NaN, flags, lastwatertime round
  0.15 s, throwhit, throwstabilize and its axis, angles on every range edge, far out and NaN, no model, particles on
  and off), runs the QuakeC and the native from the same state and compares the whole edict's fields, the next
  number of the server's stream and the broadcast bytes; then puts everything back.

### Checks

- `vr_propthink_check 300`: e1m1 27 things, 8127 states; vrfiringrange 86, 25886; AD ad_crucial 157, 47257: all
  identical (233/706/1266 declined: the far-out angles). A first run that gave the QuakeC a different particles
  value than the native reported every resulting random/particle difference (the check sees them).
- Seeded fights (scratch `ptfight.py`: sv_random_seed 7, 8 weapons and items spawned round the player, 24 monsters,
  impulse 232 flings thrice, a weapon change, 600 frames; `vr_bench_statehash` at 4 points): cvar 0, 0 again, 1, 1
  again all equal on e1m1, vrfiringrange and ad_crucial.
- Force grab, catch and throw tests with the cvar 0 and 1: see ROUND22.md, "Native prop think".

### Numbers

QuakeC statements in the seeded 600-frame fights (whole run):

| scene | cvar 0 (QuakeC) | cvar 1 (native) | change | forcegrabbable_think_qc's own (cvar 0) |
|---|---|---|---|---|
| e1m1 | 12,568,598 | 11,859,870 | -5.6% | 643,624 |
| vrfiringrange | 13,152,321 | 11,534,341 | -12.3% | 1,481,480 |
| AD ad_crucial | 20,224,393 | 17,208,098 | -14.9% | 2,792,855 |

(cvar 0 also pays the wrapper's ~6 statements a call; before this change the totals were those less the wrapper's:
about 0.10/0.23/0.45 M less.) With the native the wrapper is 5 statements a think.

Exclusive timing (scratch `pttime.py`: the same fights with props, 900 measured frames, 3 reps alternated, medians):

| scene | QC statements a frame 0 / 1 | quakec ms 0 / 1 | frame ms 0 / 1 |
|---|---|---|---|
| e1m1 | 22,857 / 21,695 (-5%) | 0.203 / 0.209 | 6.45 / 6.07 |
| vrfiringrange | 24,260 / 21,477 (-11%) | 0.197 / 0.198 | 4.57 / 4.77 |
| AD ad_crucial | 33,975 / 28,055 (-17%) | 0.422 / 0.436 | 4.82 / 5.97 |

The statements go down by 1.2-5.9 thousand a frame; at ~1.1-1.5 ns a statement that is ~2-8 µs a frame, under the
runs' noise (quakec's reps spread 0.03-0.05 ms, frame totals more: other agents' runs, spawn hitches). No frame-time
claim: QuakeC was already ~0.2-0.4 ms of a frame.

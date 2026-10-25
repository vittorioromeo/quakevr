# Flinch retrospective: hits as brief active ragdolls (2026-10-18, removed 2026-10-19)

The physical flinch system (`vr_flinch` 0/1/2) was built over eight branches in about one day and then removed by the
author's decision. This is the record, so the idea can be revisited without repeating the dead ends.

Marking: **[V]** verified in the repo or the round notes (ROUND22.md, FLINCH.md, git history on `origin/next`);
**[I]** inferred or reasoned afterwards; **[L]** from the literature or memory, numbers unverified.

Where the original material lives (all on `origin/next` before the removal commit; recover with `git show`):

- `docs/vr-port/FLINCH.md` (design, 186 lines) and the ROUND22.md sections "Flinch: hits as brief active ragdolls",
  "Flinch polish", "Flinch: the odd-pose shuffle", "Flinch: balance from the legs, `vr_flinch 2`", "`vr_flinch 2` on
  stairs, more steps", "`vr_flinch 2`, one change at a time", "`vr_flinch 2` across classes".
- Code: `QC/vr_flinch.qc`, `Quake/vr/vr_ragactive.cpp` (`beginFlinch`, `staggerFrame`, `legsBalance`, `turnUpper`,
  `floorUnder`, `flinchDraw`, `drive`), `Quake/vr/vr_box3d_knockdown.cpp` (`ragdollFlinch`, budget, hand-back,
  `vr_flinch_status`), `QC/combat.qc` (`vr_fl_prepain`), `QC/vr_ad_bridge.qc` (`VR_AD_Hit`).
- Commits: `d060895c5` (flinch 1), `691bbaa15`, `8fd0f4301`, `b0ea6bb2e` (mode 2), `006725c13`, `06678876d`,
  `1d03e4f03` and the reverts. `git log --all -i --grep flinch` lists them all (36 commits).

## 1. Goal

The author wanted Euphoria-like reactions [V, his words as relayed]: an enemy that

- bends where it is hit (the struck part and the spine react),
- backpedals or sidesteps to keep its balance, steps driven by where its weight is relative to its feet,
- topples if the hit is too strong,
- falls naturally on stairs and slopes (not as a canned animation).

After the hit it should blend back into its normal Quake animation and AI. No global spring, no hand of God.

## 2. Timeline of attempts

All headless numbers are from `vrtesthall` (flat floor, a 22-degree slope, 16x16 45-degree stairs), a grunt unless
stated, hit along +y, spot and stand phase jittered, 8 runs per case. [V] for every table in this section; they are
copied from the round notes. The suite scripts were never committed (section 5).

### 2.1 `vr_flinch 1` (branch agent/flinch, `d060895c5`, `c617e59e3`)

Design [V]:

- `T_DamageDeal` (and, after their damage, melee/throw via `VR_HitStagger_*`, and AD's `ad_T_Damage` bridge) computes
  an impulse (shotgun 7 N s per pellet, super shotgun 12, nails 1.2 per damage, melee 3 per damage, thrown props their
  momentum, blasts 3 per damage). The engine returns the change of speed it gives the whole body.
- Under `vr_flinch_min` (0.3 m/s) the old small hit reaction plays. Above it the monster becomes a ragdoll made from the
  pose it is drawn in (the knockdown's path), `ragdollflinch` begins the flinch, QC polls `ragdollstaggered`.
- The balance controller was the shove's stagger: a pelvis pinned by a kinematic anchor and carried along the hit,
  muscles holding the weight up, `turnWhole` turning the body upright, planted feet, wheeling arms.
- Capture step only while the capture point (middle + level speed * sqrt(h/g)) leaves the feet's support.
- Balance = 1 - 0.5 * change of speed / Topple Speed (2.0 m/s, x0.65 on uneven floor). At or under the edge it recovers,
  past it a step or two then falls, at 1.5x it goes down at once.
- Recovery test every frame from 0.25 s: centre of mass over the feet's discs and strip, level speed < 0.35 m/s,
  pelvis and chest within 22 degrees of the stand frame, firm ground, no step under way, held for 0.08 s.
- Hand-back: best-fitting stand/run/struck-near frame (`fitFrame`), origin and yaw follow the body, blended 0.15-0.3 s
  scaled by the pose gap. Budget 6 at once, range 1536, fallback to the small reaction.

Measured [V]:

| case | result |
|---|---|
| grunt, shotgun (0.37-0.52 m/s), 30-damage blow (0.8-1.1 m/s) | recovers in 0.32 s, 0 steps, flat/slope/stairs |
| point-blank super shotgun 2.08 m/s, 60-damage blast 2.25 m/s | flat: falls after steps; slope/stairs: down at once |
| knight, death knight, ogre | recover from sg, ssg, blow (ogre 0.83 m/s in 1.4 s, 4 steps) |
| enforcer | shuffles 8-9 steps, settles ~2.1 s |
| crowd: 10 grunts, one blast each | 6 flinch (budget), 4 refused, all recovered in 0.34 s |
| cost | switch-in 0.13-0.24 ms; drive 0.0063 ms per flinching grunt per frame; server physics 0.14 -> 0.21 ms mean |
| hand-back gap (mean vertex distance) | 0.3-0.4 u shotgun flat, 3.3 blow, 5.4 slope, 16 stairs |
| eval.sh --full | 168/177 with mode 1 and mode 0, same 5 known diffs (melee unaffected) |

**VR verdict [V, author]:** "freeze in place, feet glued, body wobbles". A shotgun to the chest froze it; a medium
blow wobbled the whole ragdoll on stuck legs; an uppercut floated it up and then sprang it back (a hidden spring).
Strong hits toppling immediately was liked.

**Why it failed [V, diagnosed in flinch4]:** three hidden "hand of God" mechanisms:

1. The pelvis pin: a kinematic anchor on the stand pose's pelvis, 3D, carried a fixed short way (2 units for a
   shotgun, 5 for a blow): a spring of up to 2 body weights at 5 Hz, height included. That is the float-and-spring-back.
2. The support: a 0.6-0.95 g lift on every part, gravity all but gone.
3. `turnWhole`: every part's velocity turned about the pelvis.

With the pin carrying the pelvis the capture point never left the feet, so no step was ever taken (0 steps in every
case). The "balance" was an animation-like blend dressed up as physics.

### 2.2 Polish: enforcer, stairs hand-back, AD, real weapons (agent/flinch2, `691bbaa15`)

Fixes [V]:

- The enforcer shuffled 8-9 steps because its stand frame leans over its rifle (centre of mass ~3.7 u ahead of its
  ankles) and its short legs (0.59 m) shrank its support to 4.5 u (grunt 5.8). Fix: stance-centred support
  (`Stagger::supportOffset`), Foot Support scales up only with leg length, 4 corrective steps. Result 0.32-0.52 s.
- Pain frames removed from the hand-back candidates; arms' wheel fades by 0.12-0.32 s.
- Stairs hand-back gap was ~16 u, nearly all vertical (any 32-unit box rests on the stair above while the body stands
  with a foot on the one below; the same 16 u every walking monster floats at there). Blend now scaled by the
  vertical gap (up to 2x = 0.44 s) and tries both the plain and the lifted stand spot.
- AD's monsters take damage in `ad_T_Damage`: `VR_AD_Hit` now calls `VR_Flinch_Hit`. Death knight, guard, soldiers,
  enforcer variants, fury, sergeant, pyro, fumigator all flinch and recover in 0.32-0.62 s. The crossbow knight cannot
  ragdoll (`canRagdoll` 0).
- Recovery's ground test fell back to Quake's own trace (AD's start map floor was invisible to the physics ray).
- Real weapons via the mock: impulses matched the design formula (shotgun 42 N s at 64 u = 0.52 m/s, falloff
  0.95 at 192, 0.68 at 512; nails 20 N s each, two in a row flinch).

Still rough: a blow soon after a knockdown/get-up (odd pose) shuffled 8-11 steps and toppled at 2.0-2.3 s, ~1 run in 2.

### 2.3 The odd-pose shuffle (agent/flinch3, `8fd0f4301`)

Three causes, all about the pose it was struck in [V]:

1. Steps kept the struck stance. Struck in an attack frame the grunt's middle stood 9 u off its feet (stand frame:
   2.4), so every step left it as far off its support: 11 steps, toppled 2.31 s. Fix: stance from the stand frame
   (`beginFlinch`).
2. A blow's body was made in its pain frame (the enforcer's `paind1` bends the chest 35 degrees and it stayed so).
   Fix: `T_Damage` keeps the pre-pain frame in `vr_fl_prepain`; blows/throws build the ragdoll in it.
3. A bent torso stayed bent: a still clip's joint motor holds any bend up to 2x its weight's torque. Fix: torso and
   head motors turn toward the stand frame as tone returns (4 of the way a second, at most 3 rad/s; `drive`).

| | before | after |
|---|---|---|
| grunt | 8/12 at 0.32 s; 4 toppled at 2.31 s after 11 steps | 12/12 at 0.32-0.38 s, 0-1 steps |
| enforcer | 4 at 0.32 s, 6 at 0.95-1.40 s, 2 toppled | 8 at 0.32-0.45 s, 3 at ~0.85 s, 1 at 1.25 s |
| enforcer 2nd hit (ssg) | 3/12 at 2.0 s | all 0.45-0.76 s |

### 2.4 `vr_flinch 2`: balance from the legs (agent/flinch4, `b0ea6bb2e`, +394 lines in vr_ragactive.cpp)

Directly answers the VR verdict. Design [V]:

- No pin, no lift. The hit's whole impulse goes into the body (jolt at the struck point, rest on the parts above the
  legs) times `vr_flinch_legs_push` (2.5, so a shotgun backs it a step; balance/topple use the real impulse).
- Weight borne only through feet on the floor: a push up the legs holding the pelvis at its stand height over the feet
  (3 Hz, critically damped, up to 1.6 weights, never a pull). Struck up, it falls back at once.
- Centre of pressure kept within the bearing feet's soles (0.6 Foot Support round each, and the strip): within them
  the ankles pull the capture point back; past them they cannot (inverted-pendulum acceleration on the upper body).
- A capture step once the capture point is 6 cm past that reach: the swing foot lands where the capture point will be
  at landing, 0.1 leg beyond, half a stance aside, within 0.75 leg of the hip, never across the other foot; carried
  along an arc with the hip's reaction kept so momentum is unchanged.
- Bearing legs follow their planted foot; the hips turn the pelvis upright (`turnUpper`, 8 rad/s per rad, max 4,
  momentum-keeping).
- Modes 0 and 1 unchanged (everything gated on `Stagger::legs`). `vr_flinch` default stayed 1.

Measured, recovered/8 [V]:

| case | mode 1 | mode 2 |
|---|---|---|
| flat, shotgun 64/192 | 8, 0 steps, 0.32 s | 8, 1 step (cos 1.00, 15 u), 0.37 s |
| flat, 30-damage blow | 8, 0 steps | 8, 1 step (21 u), 0.50 s |
| flat, uppercut | 8, 0 steps, rise 0.9 | 8, 1 step, rise 4.8 (falls back) |
| slope, shotgun / blow / uppercut | 8 / 8 / 8 | 8 / 8 / 8 (1 step) |
| stairs, shotgun | 8 | **0** (4 steps against the hit, falls) |
| stairs, blow / uppercut | 8 / 8 | **2 / 0** |

First real steps, but only ever one, and the stairs broke. The author's wish for "a few backpedal steps" was not met.

### 2.5 Stairs, more steps (agent/flinch5, `006725c13`, reverted in `ee809c223`)

A large (+498/-116 lines) rewrite attempting to fix the stairs and lengthen backpedals at once [V]: common rest gap,
hanging feet put down first, friction-like foot grip, landings traced from the pelvis and kept off stair edges within
0.45 leg up / 0.6 down of the other foot, a swing lifting clear of the step, the level floor push scaled and capped at
0.8 g, capture steps cut to max(0.45 leg, 0.72 of full) and re-planned over the first 60% of a step, closing steps for
split stances, leading foot first for side hits.

Result: worse everywhere. Flat fell from 8/8 to 0-2/8 with ~11 steps (stepping in place, centre of mass rising up to
15 u); stairs 0-2/8. Switching each new part off with a bitmask did not bring flat back. Lesson: too many coupled
changes in one go; nothing could be bisected. Kept: `vr_flinch_test_class` (test monster selector) and the stairs
diagnosis (section 3).

### 2.6 One change at a time (agent/flinch6, commits `e326dcda7`, `018e0aac1`, `55a879fc8`, WIPs `06678876d`, `1d03e4f03`)

Method [V]: a fast suite (6 cases x 8 runs, ~40 s), one change at a time, kept only if flat stays 8/8.

| step | flat sg64 / blow / upper | slope blow | stairs sg64 | stairs blow | kept |
|---|---|---|---|---|---|
| mode 1 | 8 8 8 | 8 | 8 | 8 | |
| mode 2 baseline | 8 8 8 (1 step) | 8 | 0 | 2 | |
| 1 `floorUnder` for bearing feet | 8 8 8 | 8 | 0 | 2 | no (knight stairs 6/8 -> 4/8) |
| 2 swing kept 0.08 leg over floor | 8 8 8 | 8 | 0 | 2 | yes |
| 3 hanging foot put down first (6 variants) | 8 8 8 | 8 | 0-1 | 0 | no |
| 4 first steps cut short, re-planned (4 variants) | 8 / 0-2 / 0-8 | 2-8 | 0 | 1-6 | no |

Other classes at the end of flinch6 (mode 2 / mode 1): enforcer 0 0 8 0 0 0 / 8 8 8 8 8 6; knight 8 8 8 8 6 6 / all 8;
ogre sg and uppercut do not flinch in either mode. A stronger knowledge result: a step cut short lands short of the
capture point, and a second step then never catches it: "bimodal", a run takes 1 step and recovers or runs away
(7-8 steps, 200 units, falls) on flat. On stairs cutting steps helped (blow 2 -> 6) but cost flat.

### 2.7 Across classes (agent/flinch7, `1f479d462`, `f1a210073`, `0684c600c`, `cff666e30`)

The only productive round. Recovered/8 over flat sg64, blow, uppercut, slope blow, stairs sg64, stairs blow, stairs sg192 [V]:

| step | grunt | enforcer | knight | death knight | kept |
|---|---|---|---|---|---|
| mode 1 | 8 8 8 8 8 8 8 | 8 8 8 8 8 6 8 | all 8 | all 8 | |
| mode 2 baseline | 8 8 8 8 0 2 0 | 0 0 8 0 0 0 0 | 8 8 8 8 6 6 6 | 8 8 8 8 0 0 0 | |
| 1 jolt cap | same | 8 8 8 6 0 0 0 | same | same | yes |
| 2 hanging foot bears (to a leg) | same | 8 8 8 7 4 5 4 | all 8 | 8 8 8 8 8 1 8 | yes |
| 3 planted foot's floor traced once | all 8 | 8 8 8 5 6 4 6 | all 8 | 8 8 8 8 8 3 8 | yes |
| 4 step only past both feet on floor | all 8 | 8 8 8 4 6 6 6 | all 8 | 8 8 8 8 8 1 8 | no |
| 5 ground test from the pelvis | all 8 | 8 8 8 6 6 6 6 | all 8 | same | no (no effect) |
| 6 landing drawn back onto floor | all 8 | 8 8 8 8 6 8 6 | all 8 | 8 8 8 8 8 7 8 | yes |
| 7 landing height from the pelvis | all 8 (1 at 7) | 8 8 8 7 6 7 6 | all 8 | 8 8 8 8 8 6 8 | no |

End state: mode 2 equals mode 1 on flat and slope for every class measured; the enforcer's light stairs hit 6/8 vs
8/8; death knight stairs blow 7/8; enforcer on stairs from a spot 12 u left of centre 2/8 in both shotgun cases.
Default not switched. **No VR verdict on mode 2 in this table; the author's separate VR judgement of mode 2 was:
"still looks like freezing, no big steps, some melee hits send ragdolls flying and gliding across the floor; worse
than what we had before"** [V, as relayed]. The flying/gliding is consistent with the 2.5x push plus a floor with no
friction model beyond the feet (section 3, inferred [I]).

## 3. Root causes found, in order

Each of these cost at least one full branch to find. [V] unless marked.

1. **Pelvis anchor spring** (mode 1): a 2-weight 5 Hz spring on the pelvis. Made the body float and spring back; made
   steps impossible. Removed in mode 2.
2. **0.6-0.95 g lift**: gravity nearly cancelled, so there was no weight to balance. Removed in mode 2.
3. **`turnWhole`**: rotated every part's velocity about the pelvis; "wobble on glued feet". Replaced by `turnUpper`
   that conserves momentum.
4. **Jolt on a light pelvis**: the local share of the impulse landed on the nearest part. An enforcer's pelvis (6 kg
   under a 57 kg chest) took 7 m/s versus 0.8 for the rest, dragging both legs (bearing foot slid 21 u in 0.15 s),
   jackknifing it. Fix: cap the struck part's jolt at 1.5x the upper body's change of speed (`flinchJoltMost`), rule
   by mass not class. The enforcer's "against the hit" steps were catching this, not its lean.
5. **Stance wider than a stair tread**: a grunt put on 16x16 stairs stands with its box on the step above, one foot 4.5 u
   over its step and the other 20 u over the step below; its legs (~20 u) cannot reach 15.5 u lower. With no pin it
   starts unsupported.
6. **Floor traces too short**: `floorAt` traced from 0.3 m above a foot, missing a floor 20 u down (read as air) and
   starting inside the step above for a foot by a riser (no hit). Fix: trace from the pelvis (`floorUnder`) -- but
   used for the bearing feet it made the knight worse, because the old miss made the hanging foot "stand" and hold
   it. Used only for the swing clearance. The real fix for bearing was tracing once (cause 8) and allowing a hanging
   foot to bear up to a leg over its floor.
7. **Swing through risers**: a step's straight way with a 0.12 leg arc goes through a riser when stepping up; the foot's
   carry then pushed the body sideways at up to 7 m/s through the contact. Fix: keep the swing 0.08 leg over the floor
   along its way.
8. **Floor flicker**: a planted foot's floor re-traced every frame read the step below as it shifted by a unit and
   stopped bearing. Fix: trace once as it stood or landed (stairs grunt 0/2/0 -> 8/8/8).
9. **Landing off the stairs' open side**: a death knight sidestep landed over air, stood, but never passed the recovery
   ground rays, toppling at 2 s. Fix: landing drawn back toward the hip until floor within a leg (stairs blow 1/8 ->
   7/8).
10. **Runaway second step**: a first step cut short lands short of a still-running capture point; the next also lands
    short; 7-8 steps, 200 units, then a fall. Not fixed (the WIP was reverted).
11. **Pain-frame ragdoll build**: ragdoll built in the pain's first frame (enforcer `paind1` chest 35 degrees).
    Fixed with `vr_fl_prepain`.
12. **Stance from the struck pose**: steps reproduced the struck (attack-frame) stance; fixed by taking the stance from
    the stand frame.
13. **Floor ground test on AD maps**: physics ray missed AD floor; fallback to Quake trace.
14. **Flying and gliding ragdolls in mode 2** [I]: a push scaled 2.5x on parts above the legs, with legs whose bearing
    was lost (feet over air, stair edges) and no friction/grip model for sliding feet, matches "sent flying and
    gliding". Not diagnosed in the repo; no round measured it.

## 4. What worked

[V]:

- The *recovery and hand-back* machinery: a ragdoll built from the displayed pose in 0.15-0.24 ms (rigs cached per
  class), a recovery test, and a fit-the-best-frame blend back to animation (gap 0.3-0.4 u for a shotgun on flat).
  Crowd of 10 stayed cheap (0.006 ms per flinching enemy per frame).
- The impact model: the impulse table, falloff and attack-frame guard matched real weapons through the mock.
- Budget, cooldown, safeguards, AD's damage bridge, per-class safety (zombies left out).
- Strong hits toppling immediately into the knockdown (liked in VR).
- In mode 2: genuine capture-point stepping on flat ground and slopes for grunt, knight, death knight (1 step along
  the hit, 8/8), and after the cap/trace fixes flat recovery for all four classes.
- Method lessons: one change at a time against a fast suite; keep rejected WIPs in history as reverted commits.
- Generic fixes (jolt cap by mass, hanging-foot bearing) rather than per-class hacks, as the author asked.

What did **not** work: more than one capture step (runaway), anything on 16x16 stairs (never reached the default
bar), the look in VR (reads as freezing) in both modes.

## 5. Test infrastructure and how to recreate it

In the repo on `origin/next` before removal [V]:

- `vr_flinch_test <n>` (QC, `VR_Flinch_Test`): 1-9 shotgun, super shotgun, nail, super nail, blow, head blow, blast,
  throw, crowd; 10 uppercut (35 damage to the chin, mostly up); 20 a grunt at `vr_flinch_test_x/_y/_z`; 21 ten grunts
  ahead. Parameters `vr_flinch_test_yaw`, `_dist`, `_pellets`, `_health`, `_class` (0 grunt, 1 enforcer, 2 knight,
  3 death knight, 4 ogre, 5 zombie). Debug > Tests > Flinch rows ("Test Monster" selects the class).
- `vr_flinch_debug 1` overlay (hit arrow, centre of mass with floor line, foot discs, coloured by state) and
  log lines; `2` refusals, `3` every hit and why a class cannot flinch (and the struck frame and stance), `4` a
  `legs:` line per frame and a `step:` line per step, `5` every part's place/speed/mass for the first 0.06 s; every
  flinch prints a `result` line (steps, mean cosine along the hit, way stepped, centre-of-mass rise).
- `vr_flinch_status [reset]`: counts, switch-in cost, recovery time, hand-back gap, per-enemy balance.
- Test room `vrtesthall` (made by `Misc/quakevr/make_vrtesthall_map.py`): flat floor, a 22-degree slope (x -240..-80,
  y 460..620), stairs 16 high x 16 deep (x 96..288, y 544..640).

Not committed (lived in the `fastship` worktree's `scratch/`, git-ignored; may still be on disk) [V]:

- `fl_terrain.sh`, `fl_classes.sh`, `fl_crowd.sh` (flinch 1); `fl3_seq.sh` etc. (odd-pose runs); `fl4_suite.py`,
  `fl4_strip.sh` (side-view frame strips every 7 frames, `fl4_strip_<case>_<terrain>[_old].png`); `fl5_suite.py`
  (`FL5_CLASS`, `FL5_ONLY`); `fl6_suite.py <label> <mode>` (`FL6_CLASS`, `FL6_ONLY`, summaries in `fl6_table.txt`);
  `fl7_suite.py <label> <mode>` (`FL7_CLASS=all`, `FL7_CASES`).

To recreate [I]:

1. For each (class, terrain, weapon, mode) run `vr_flinch_test 20` to place a monster at a jittered spot and stand
   phase, then the hit test; read the `result` line of `vr_flinch_debug 4`; count recovered/8 and mean steps; use
   `run.sh -Script ... -Filter "flinch:"`.
2. Cases: flat sg64, flat blow, flat uppercut, slope blow, stairs sg64, stairs blow, stairs sg192 (nails do not
   flinch anything). Classes: grunt, enforcer, knight, death knight; ogre/zombie as controls.
3. Gate: flat 8/8 for every class, stairs light hit 8/8, stairs strong hit may fall.
4. `eval.sh --full` with `vr_flinch 0` (168/177, same 5 known diffs) to confirm melee unaffected.
5. Frame strips: screenshot every 7 frames from a side view (`vr_flinch_test_yaw`) and compose one image with `-Out`.

## 6. Ideas not tried [I unless noted]

1. **Offline tuning (CMA-ES)** [L, recommended in the research]: expose the controller gains and optimize ~10-20 of
   them against the test suite as a score (recovered fraction, steps, travelled distance, hand-back gap, topple on
   strong hits). The suite exists in concept (section 5) and runs in ~40 s; headless runs would need to be batched or
   a ragdoll-only harness built. The gains the last worker would have exposed per class: `legsCop`, `legsStepPast`,
   `legsBears`, `legsLow`, `legsHertz`, `legsPushMost`, `legsDrawn`, `legsUpright`/`Most`, `legsStance`,
   `legsStepSpeed`, `legsOvershoot`, `legsStride`, `legsSwingMost`/`Grip`/`Clear`, `legsHang`, `flinchJoltMost`,
   `vr_flinch_legs_push`, `staggerStepTime`/`Quickest`, and a neutral-capture-point bias.
2. **Destabilized state** (Blade & Sorcery style [L]): do not simulate stepping; scale the per-part spring strengths of
   an animation-driven pose down for a moment so it sags and bends where hit, then back up. Cheap and robust on stairs.
3. **Step budget**: allow N planned steps up front (N-step capturability [L]) computed from the hit before starting,
   and plan the landing from the capture point at *landing time* so the second step does not run away; the flinch6
   runaway is exactly the failure of re-planning from a short first step.
4. **Hip strategy**: the ankle/hip/step hierarchy [L]; only the ankle (centre-of-pressure) and step levels exist. A
   hip/torso lunge would absorb mid-size hits without stepping.
5. **Neutral capture point** from the stand pose, to bias stances (the enforcer's rifle lean) properly instead of
   `supportOffset` patches.
6. **Hybrid animation-driven**: keep the animation playing as the target pose (pose matching with PD springs, the
   Euphoria/B&S pattern [L]) instead of free-falling ragdoll parts held by hand-written supports; hit reactions then
   look like the animation, only physically displaced. The sagging "freeze" look comes from a ragdoll with no
   authored intent.
7. **Stairs as a special case**: a stair-aware stance (both feet on one step) at the switch-in, or flinch disabled/
   instant topple on stairs and slopes steeper than the box can hold.
8. **A friction/grip model for sliding feet** (mode 2's gliding, [I]); a grounded-foot velocity clamp.
9. **Sidesteps** (flinch6 step 5) and leading-foot-first for side hits: never started.
10. Measure with the author's VR loop earlier: both modes were judged "freeze" only in VR; the headless suites
    optimized recovery counts, a proxy that did not track the look.

## 7. Literature and prior art (summary; numbers unverified) [L]

- **Euphoria (NaturalMotion)**: hand-authored behaviour modules writing target poses and per-limb stiffness; stepping
  decided from centre-of-mass state; not a learned controller.
- **SIMBICON** foot placement: `d + c_d*d + c_v*v`.
- **Capture point** (Pratt et al.): `xi = x + v/omega`; N-step capturability; re-plan each step from the current xi
  with the landing-time xi; ankle / hip / step strategy hierarchy; the neutral capture point from the stand pose for
  biased stances.
- **Coros et al., generalized biped walking**: virtual forces; **Wang/Fleet/Hertzmann**: offline CMA-ES of ~10-20
  controller gains against a test suite, recommended over raw torque trajectories or RL.
- **VR games**: Blade & Sorcery SDK (verified): animated target pose + per-part PD springs, a "Destabilized" state that
  scales springs, ground-conditioned multipliers, capsule locomotion, no physical stepping. Half-Life Alyx:
  temporary ragdoll and get-up. Boneworks/PuppetMaster: pin and muscle weights (unverified). Shipped games almost never
  do physical stepping.

## 8. Recommendation for a future attempt

1. Do not restart from "ragdoll plus supports". Start from the animation-driven, spring-weakened pose (section 6 items 2
   and 6): it matches how shipped VR games get their look and sidesteps the stair and stepping problems.
2. If stepping is still wanted, plan steps in a batch from the hit (step budget, landing-time capture point), expose
   the gains, and tune them with CMA-ES against the suite; judge candidates by frame strips and the author's VR eye, not
   only recovery counts.
3. Keep the pieces that worked and are independent of balance: the impact table and falloff, the budget/cooldown
   safeguards, the pre-pain frame, the recovery test and the fit-the-best-frame blend-back.
4. Always one change at a time against the fast suite; never bundle (flinch5 lost a day to it).
5. Treat stairs as out of scope for the first version.

## 9. The replacement

Vittorio chose to remove the physical flinch completely. In its place: **situational active-ragdoll triggers**, i.e. an
enemy becomes an active ragdoll only in specific, clearly-readable situations (the existing knockdown/stagger paths:
shoves, strong hits that topple, leg sweeps, etc.), with the small hit reactions (`vr_hit_react`, 2026-10-11) and
Quake's pains for ordinary hits. Strong-hit toppling, the one part of the flinch the author liked, lives on in the
knockdown. [I for the exact scope of the triggers; the removal itself is the coordinator's statement.]

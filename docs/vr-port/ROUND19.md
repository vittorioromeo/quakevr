# Round 19: performance review

A deep review of the hot code, each change measured before and after (mock headset, 2048² eyes, paused frames,
in-run A/B switches) and kept only if faster or simpler at the same image; plus the Release build flags, the holsters
on the body, a taller VR menu with split pages, and user documentation.

| Area | Result |
|---|---|
| World and liquid shaders | world pass 40–62% less GPU time indoors (e1m1 corridor 1.34 to 0.57 ms, both eyes; with 3 shadowed lights 2.13 to 0.81): cheaper soft-shadow filters, moving-caster maps read first, empty cube faces skipped, a depth pre-pass |
| Model, post-processing, particle shaders | particles and decals ~0.26 ms less in fights (culled per eye, transparent fragments skipped, soft-particle depth copy only when needed); depth-only shadow casters; model light loop |
| CPU: physics, lighting, view | model ambient ~10x, sleeping rigid bodies 3–5x, shadow casters 40–65% cheaper; whole CPU frame ~25% lower |
| CPU: particles, decals, gore | particle quad building 20–40% cheaper; the rest measured no gain and was reverted |
| QuakeC | 33–43% fewer statements per server frame (~13 µs) |
| Build flags | /Ob3 /Gw /GS- added, /GL + /LTCG kept, /fp:precise and SSE2 kept (no measured gain from /fp:fast or AVX2) |

Also in this round:
- **GPU columns in the memory log** (`vr_gpustats.cpp`): the GPU's clocks, temperature, power, use, encoder use and
  slowdown reasons (NVML, loaded from nvml.dll), and each program's share of the GPU's 3D/copy/encode engines (the
  "GPU Engine" performance counters), sampled on a thread once a second and averaged per row: whose GPU work grows
  when a session slows (SteamVR's compositor, Virtual Desktop's streamer, throttling).
- **A world generation** (`qvr::worldGeneration`, bumped by `R_NewMap`): per-map data (ambient cache, model lights,
  torch lights, the env cube, the flashlight, shadow slots, detail cache, the liquid volume) was keyed on the world
  model alone, which Ironwail reuses when the same map name is loaded from another file (Relit Maps switched on
  re-release data): a crash. Now rebuilt on every map load.
- **Relighting re-release maps:** the worldspawn's own light settings (`_bounce`, `_dirtmode`...) are left out of what
  `light` sees, so they get our look.
- **User documentation:** README.md, docs/INSTALL.md, FEATURES.md, SETTINGS.md, BUILDING.md, RELIGHTING.md; the
  package excludes the relit id maps and private folders and carries the relight scripts; `r_wateralpha 0.6` by
  default (see-through only on water-vised maps).
- **Research:** the 2021 re-release works (docs/INSTALL.md "Only the 2021 re-release?"); FrikBot X 0.10.3 is the
  latest and works in deathmatch: kept.

## QuakeC

**How it runs.** The server runs QC at 72 Hz whatever the display rate (`host_netinterval` is 1/72 for
`host_maxfps` above 72 and for 0, the headset-paced case: `Max_Fps_f`, `host.c`), so a 120 Hz headset runs the per-player
QC (PlayerPreThink/PostThink, the hands' melee, bash, deflect, corpse strikes, force grab, carrying) and every entity's
think 72 times a second. The VM (`pr_exec.c`) is fast: a timed loop measured 0.77 ns a statement, and a QC call and
return about 8.4 ns more (as much as 11 statements). A builtin call costs its C work plus the call.

**Measured.** With a temporary `qcprof` console command (in `vr_builtins.cpp`, removed again): each function's own
statement count (the engine's `profile` counters, all of them rather than `profile`'s top 10) and each builtin's calls
and time (the builtin table wrapped), per 72 Hz frame; the scenes at `host_framerate 0.013889`, so game time is frames.
Scenes: vrfiringrange with a sword held and the hands swaying a few millimetres (a `vr_mock_play` file: tracking is never
still, and a still mock hand skips most of the melee tracking), then empty hands; the dummy bash and shove test
(`sw18/dummy.txt`); e1m1 with ten grunts (impulse 244) fighting a god-mode player, then rockets (gibs), then the
aftermath (corpses, gibs). Statements per server frame:

| Scene | Before | After |
|---|---|---|
| vrfiringrange, sword held | 10424 | 5957 (-43%) |
| vrfiringrange, hands empty | 9729 | 6288 (-35%) |
| dummy bash/shove test | 8729 | 5780 (-34%) |
| e1m1, ten grunts fighting | 7116 | 4223 (-41%) |
| e1m1, rockets and gibs | 8307 | 5538 (-33%) |
| e1m1, aftermath | 7535 | 4662 (-38%) |

`cvar_hget` calls went from 120 a frame to 40. The server frame (`vr_profile`, vrfiringrange with the sword, 20 s):
`SV_Physics` (thinks and QC, less the rigid bodies) about 0.079 to 0.065 ms, the whole server frame about 0.113 to
0.100 ms. QC is a small part of the frame either way (the CPU's whole frame is about 0.4 ms): this saves some 13 µs a
server frame, about 1 ms of CPU a second.

**Where it went, before.** Items and dropped weapons (a third to a half of it in vrfiringrange: every ammo box, health
box and weapon lying about thinks at 50 Hz: `forcegrabbable_think_impl`, `wpnthrow_impl_stabilize`); the melee
tracking's striking points (`ArrayGet*blow_cp`/`blow_pp` and `ArraySet*vr_pt*`: fteqcc turns an array indexed by a
variable into accessor functions that binary-search the index, six calls a point a frame); the holsters' hover
(`UpdateHolsterHover`, `VRHolsterToIndex`, `isHolsterHotspot`, `ArrayGet*holsterhover`: some 30 calls a frame for six
flags); `VR_ItemUtil_*` (a 43-way if chain, twice a frame for the antigravity belt and once for every monster's sight
check); and the per-hand getters, each two calls deep (`VRGetHandPos` into `VRImpl_VectorGetter` with the name string),
the bit tests three or four (`VR_HandGrabUtil_IsHandGrabbing` > `VRIsEntHandGrabbingBit` > `VRGetOffHandGrabbingBit`
> `VRHasBit`).

**What changed** (QC only; behaviour and defaults unchanged):

- **Inline branchless getters** (`vr_util.qc`, `vr_handgrabutil.qc`). fteqcc inlines functions marked `inline` whose
  body has no branch, so the per-hand selects are arithmetic: `VR_IMPL_SELECT(hand, off, main)` is
  `main * (hand != cVR_OffHand) + off * (hand == cVR_OffHand)` (both are finite: `a * 1 + b * 0` is exactly `a`; any
  hand but the off hand's 0 is the main hand, as the old if/else had it). The macro getters (`VRGetHandPos`,
  `VRGetHandRot`, `VRGetMuzzlePos`, `VRGetFireButtonPressed`, ...), `VRGetEnt*`, `VRIsHandEmpty`, `VRGetWeapon`,
  `VRGetOtherHand`, `VRHandsTracked`, `VRHasBit`, the `VRGet*Bit` and `VRIsEntHand*Bit` bit tests, the grab state's
  `VR_HandGrabUtil_IsHandGrabbing`/`IsHandPrevGrabbing`/`IsHandReloadFlicking`, `isHolsterHotspot` (a range) and
  `VRIsHandCarryingByForegrip` (both moved to `vr_util.qc`) inline as a few statements, no call. `vr_util.qc` and
  `vr_handgrabutil.qc` now come first in `progs.src`: fteqcc inlines only what it has compiled already (a later
  definition is called, with an "inconsistent context" warning), and an inline body must use only inline functions
  defined before it, or it is not inlined either. The "invalid hand id" prints (`VRImpl_*`) are gone with the calls.
- **No array accessors in the per-frame melee** (`vr_juice.qc`): `VR_Blow_Update` copies the striking points with
  constant indices (`VR_BLOW_KEEP(0..5)`, a macro), and `VR_Blow_Point` is a macro, so the indices are plain fields
  and globals. `VR_Deflect` keeps the newest batting line's time in `bat_newest` (it was `bat_t[bat_i]` every frame)
  and reads the batting speed's cvars only in a stroke.
- **Holster hover** (`weapons.qc`): `UpdateHolsterHover` is twelve plain assignments (constant indices), and
  `VRHolsterToIndex` arithmetic (`hotspot - 3 - (hotspot > QVR_HS_HAND_SWITCH)`).
- **Items at rest**: `wpnthrow_stabilize` returns at once for a settled weapon (`throwstabilize` 0 and the angle
  already in range, where the stabilizing left it as it was); `vr_weapondrop_particles`, the force-grabbable return
  time and `vr_grab_gibs` are read once a frame (`VR_CVars_Frame` in `vr_cvars.qc`, from `StartFrame`) instead of once
  an item think or once a gib in the force grab's search.
- **Item ids** (`vr_itemutil.qc`): the id-to-bit and id-to-category chains are two tables; the two per-frame
  callers (the antigravity belt in PlayerPreThink, the invisibility ring in the monsters' `FindTarget`) test the bit
  directly.
- **Fewer repeated calls**: `VR_Blow_ToPlaySpace` keeps the yaw's cosine and sine until the player turns;
  `VR_Blow_Track` asks `VRMetersToUnits` once for the wrist and the tip; `VR_Blow_IsBlow` and `VR_Deflect` take the
  hand's blow entity instead of looking it up again; `VR_Forcegrab_IsEligible` rejects what has no model (triggers)
  and what is no kind of grabbable first, and the target search skips model-less entities without a call;
  `VR_Corpse_StrikeFrame` tests the hand's speed before the two-handed and foregrip tests.

**Verified.** QC `Done. 0 warnings`. Behaviour: the melee (`melee_final.txt`), knight and parry (`sw18/knight2.txt`),
two-handed and hand-off (`handoff/all.txt`) and sword grip/bash (`sw18/sword.txt`) scripts at `host_maxfps 72;
host_framerate 0.013889` (one server frame a host frame), before and after: each run's gameplay lines (hits and their
speeds, bashes, parries, grips, positions) match a run of the old progs exactly. The old progs alone are not
repeatable run to run in two places (a punch's acceleration, from the mock hand's velocity, and where the knight
walks in the sword script), and the new runs fall on one of the old runs' outcomes there.

**Rejected.** Thinking less often for items at rest (the pickup sparkles and the water checks count thinks: a
change of behaviour); one `findradius` for both empty hands' force grab search (it saves one call only while both
hands are empty); caching the force grab target between frames (a frame late); engine builtins for the melee math
(the VM is not where the time goes: the rest is a few microseconds a frame).

## CPU: physics, lighting, view

**Measured.** `vr_profile 1`, the mock backend, `vr_graphics_preset 4` (ultra: 8 shadowed dynamic lights, 4 map
lights), `host_maxfps 120` (the mock ran at about 64 fps), each scene's CPU scopes averaged over about 1000 frames
after a warm-up: **e1m1**, a grunt spawned ahead and 16 rockets fired at it (the hands and body under the start's map
lights); **vrfiringrange**, 24 backpacks dropped and settled, then idle, then 40 more dropped one by one; **e1m2**,
swimming strokes (`vr_mock_play`) at 600 450 -150; **e4m3**, a rocket launcher in the hand, the hands circling (the
weapon's reflections). Finer scopes were added to find the time (`shadow select`, `dlight casters`, `shadow alias
draw`, `vr hand touches`, `vr swim`, `water mesh pick` stay; the per-entity ones used for the rigid bodies and the touch
links were taken out again, as were the temporary ones on the model hooks). The other agents' work landed during
the same builds (QC, particles, the shadow casters' depth-only alias path), so the frame totals below move for their
reasons too; the scopes listed are this section's own. CPU ms per frame:

| Scope | e1m1 | vrfiringrange idle | vrfiringrange dropping | e1m2 | e4m3 |
|---|---|---|---|---|---|
| model ambient | 0.063 > 0.006 | 0.150 > 0.007 | 0.190 > 0.015 | 0.035 > 0.003 | 0.070 > 0.013 |
| rigid bodies (and keeping items in the world) | 0.051 > 0.009 | 0.052 > 0.011 | 0.087 > 0.036 | 0.084 > 0.024 | 0.176 > 0.069 |
| shadow maps (map lights' moving casters) | 0.152 > 0.075 | - | - | - | 0.211 > 0.126 |
| ... of it, drawing the alias casters | 0.139 > 0.067 | - | - | - | 0.190 > 0.110 |
| alias (the eyes' models, self) | 0.022 > 0.018 | 0.075 > 0.067 | 0.088 > 0.079 | 0.050 > 0.043 | 0.021 > 0.018 |
| view entities (self) | 0.020 > 0.014 (that run at 109 fps) | 0.021 > 0.017 | 0.019 > 0.016 | 0.020 > 0.016 | 0.020 > 0.017 |
| whole frame (everyone's changes) | 0.612 > 0.471 | 0.755 > 0.571 | 0.873 > 0.654 | 0.571 > 0.466 | 0.860 > 0.623 |

With the other agent's depth-only alias path on top, the shadow maps are 0.046 (e1m1) and 0.071 (e4m3). Nothing here
changes the GPU's work.

**What changed** (behaviour and defaults unchanged):

- **Model ambient** (`vr_ambient.cpp`). The cube's 26 rays were traced again every second for every model even where
  it had not moved: the rays only hit the world, which does not move, so the probe would be the same, and its light is
  read at the current light styles anyway. A model due by time alone at the same sample point keeps its probe (only a
  moved model is traced again, as before). Each probe's shading (26 lightmap reads and some 100 `pow`s) is kept with
  the light styles' values and the two contrast settings it was worked out at, and only worked out again when one of
  them changes (a flicker steps ten times a second): easing mixes the two kept cubes. The sample point's search out
  of walls (`openPoint`, up to five BSP point lookups) is kept while the model's middle and its anchor stay put. One
  map lookup a draw instead of two.
- **Rigid bodies asleep** (`vr_rigid.cpp`). Every item is a rigid body (`VR_Carry_Setup`), and every one asleep asked
  every frame whether it still rests on something (up to 8 corner traces) and how deep it is in water (point contents
  and a bisection), then Quake's water transition (point contents). Against the world alone the answers cannot
  change while the body keeps its origin, angles and box (the world's BSP and liquids are static; anything else can
  only add support), so they are kept per entity (`RestMemo`: only a corner resting on the world counts, one resting
  on a door or another body is asked again every frame), and a body asleep where it was returns before its axes and
  box are worked out. The water transition is skipped while the origin and `watertype`/`waterlevel` are as the last
  check left them (it would find the same contents and change nothing).
- **Keeping items in the world** (`vr_rigid.cpp`, `keepInWorld`). An item resting where it was last found free no
  longer traces its box every frame: buried or not, nothing would change for it.
- **Shadow casters** (`vr_lighting.cpp`). Each shadow face drew every caster near the light, and the alias renderer
  set each model up (its frame, matrix, light) before culling it against the face. The casters outside a face's frustum
  are now left out before the call (the same `R_CullModelForEntity` test on the same planes; skeletal, IK-posed
  bodies, which the renderer never culls, are always passed on), so the drawn set is the same. A map light's moving
  casters were collected twice a frame (to know whether it has any, then to draw them): the first collection is kept.
  The brush casters' model matrices are worked out once a light, not once a face.
- **View entities** (`vr_view.cpp`). The view's models (the fingers, the gadget, the body, the pauldrons, the weapon
  buttons: some 20 a frame) were found by name through every model known (`Mod_FindName`'s string compares): the model
  found for the same name last time is used while it is still that one and loaded (the check `Mod_LoadModel` itself
  makes); `setupHolsters` was left alone. `view::find` (behind `VR_IsViewEntity`, asked for every alias model drawn,
  shadow faces too) takes the entity from its address instead of walking the 30 view entities.
- **The profiler's own cost** (`vr_profile.cpp`). Every scope's begin looks its name up among the always-on phases,
  `vr_profile 0` too (some 150 a frame): a small direct-mapped table by the name's address now answers before the
  hash map. With `vr_profile 1`, a scope's node is found by the name's address before comparing strings.

**Verified.** Temporary checks (an environment variable, removed): every model's ambient cube recomputed from
scratch each frame against the kept one, 250 000 checks, none differed; every body asleep on the kept answers asked
the traces again, 262 000 checks, none differed. Screenshots of the four scenes (the hands' shadows under e1m1's map
lights, the backpacks at rest, e4m3 with the weapon's reflections). The backpack buoyancy test (`buoy.txt`: a
backpack dropped into the firing range's water, every frame's state printed) floats and settles as before: it comes to
rest bobbing between -11.7 and -12.0 at 0 to 1 u/s, as the last round's run did (where it lands differs from run to
run: the drop's velocity is random).

**Rejected.**
- **Culling the reflections' cube** (`vr_envmap.cpp`): the surfaces of the PVS of the cube's place as merged index
  ranges (`glMultiDrawElements`). The cube's GPU time stayed 0.017 ms (at 64 x 64 texels the whole world is cheap to
  draw; the clears and the mipmaps are the cost) and the CPU paid 0.012 ms a frame for the lists and the driver's
  multi-draw. Per-face frustum culling would cost more CPU still.
- **The liquid mesh's per-view face pick** (`vr_water.cpp`): testing the frustum before the PVS (leaving out the leaf
  walk of faces off screen) changed nothing measurable. It costs about 3 microseconds a view; the 0.027 ms seen in the
  firing range's first interval was the mesh being built at the map's load.
- **Dense per-entity arrays for the ambient cache and the casters' candidate lists (SoA)**: with the lookups down to
  one a draw and the casters' collection at 2 to 7 microseconds a frame, not worth the eviction bookkeeping.
- **Skipping the lighting hooks in the shadow faces from this side**: it would stop the ambient cube's per-frame
  easing for models seen only by a shadow face (a behaviour change); the depth-only alias path made by the shaders'
  agent covers the cost instead.
- **The memory log** (`vr_main.cpp`): its per-frame part is a few counters every 8th frame, and the GL object scan is
  already throttled; nothing to gain.

**Where the CPU still goes** in these scenes: `SV_Physics`'s own time (the QC, 0.07 to 0.11 ms), the particles
(another agent's), and the shadow faces' alias draws (one flush of GL state per model and face: an instanced
depth-only pass over all faces of a light would be the next step, in `r_alias.c`).

## Build flags

**Before.** Release | x64 already had `/O2 /Oi`, `/GL` with `/LTCG:incremental`, `/Gy`, `/OPT:REF /OPT:ICF`, `/Zi`
and `/MD`, and by default `/GS` (stack cookies), `/fp:precise`, no `/arch` (SSE2) and no CFG.

**Now** (`Windows/VisualStudio/ironwail.vcxproj`, Release | x64 and Release | Win32; Debug unchanged):

| Flag | Before | Now |
|---|---|---|
| Optimisation | `/O2 /Oi` | `/O2 /Oi /Ot /GF` (the last two were implied, now explicit) |
| Inlining | `/Ob2` (implied by `/O2`) | `/Ob3` |
| Link-time optimisation | `/GL`, `/LTCG:incremental` (implied) | the same, `/LTCG:incremental` explicit |
| Unused and duplicate code and data | `/Gy`, `/OPT:REF /OPT:ICF` | plus `/Gw` (globals in their own sections too) |
| Security checks | `/GS` on, SDL and CFG off | `/GS-`, `/sdl-`, CFG off |
| Floating point | `/fp:precise` | `/fp:precise`, explicit |
| Instruction set | SSE2 | SSE2 |
| Debug info | `/Zi`, `/DEBUG` | the same (a `.pdb` for crash dumps; it does not change the code) |

**Floating point.** `/fp:fast` stays off. The engine's C relies on exact IEEE behaviour: `IS_NAN` checks
(`sv_phys.c`, `pr_cmds.c`), the trace epsilons in the movement and collision code, and demo playback. The VR
module alone (a per-file `/fp:fast` for `Quake/vr/*.cpp` in `quakevr.props`) would be safe today: it has no
`isnan`/`isfinite`, no `x != x` and no negated comparisons used as NaN guards (only two `copysign` calls, which
`/fp:fast` keeps). But it measured no gain (below), and every NaN check added later would silently stop working, so
it is not worth it.

**AVX2.** Every PC that runs a PC VR headset has it (Haswell, 2013, and later), but `/arch:AVX2` also measured no
gain, so the build keeps the x64 baseline (SSE2) and runs everywhere.

**Measured** (the same source, four builds, runs interleaved, 960x540 window):

| | Before | New flags | + `/fp:fast` in `vr/` | + `/arch:AVX2` |
|---|---|---|---|---|
| e1m1, VR mock, standing: CPU busy ms/frame | 0.32, 0.32 | 0.33 | 0.29, 0.31 | 0.32, 0.34 |
| e1m1, VR mock, firing rockets | 0.37, 0.37 | 0.36, 0.38 | 0.34, 0.37 | 0.38, 0.38 |
| `timedemo` demo1/2/3 (`vr_enabled 0`), frames | 1834/1642/917 | same | same | same |
| `ironwail.exe` size | 2.75 MB | 3.09 MB | 3.07 MB | 3.10 MB |

(`vr_profile`'s "CPU busy", two runs each; one run of each of the before and new-flags builds, 0.82 and 0.66, fell
on a moment the machine was busy and is left out.) The frame is 0.3 to 0.4 ms of CPU, so the flags change it by no
more than the run-to-run noise. The `timedemo` frame rate sits at about 358 fps in every build: the window's
presentation paces it, not the CPU, so it only shows that the demos play the same frames. The deterministic
gameplay scripts (`handoff/all.txt`, the melee script, at `host_maxfps 72; host_framerate 0.013889`) print the same
lines before and after. The only difference is one punch's acceleration, which differs from run to run with the
old build too (see QuakeC above). Loading e1m1, e1m3, e2m3, e4m3, hip1m1, r1m1, vrfiringrange and vrstart shows no
errors.

**Build times** (32 threads): a full rebuild takes 11 s before and 13 s now. After editing one VR `.cpp` or one
engine `.c` it takes 3 s either way (incremental LTCG), and 0 to 1 s with nothing changed. The Debug build is
unchanged and still builds.

**Not done:** profile-guided optimisation (PGO) needs an instrumented build and a training run for every release,
for a frame that is not CPU-bound. `/Qpar` (automatic threading of loops) finds nothing to do here. The CMake and
Makefile builds keep their own flags.

## Shaders: models, post-processing, particles

The alias (model) shaders, the post-process and bloom chain, the mirror, Quake VR's triangles shader (particles,
decals, lines, text, screens), sprites, the heat haze, the weapons' environment cube, and the CPU code that feeds
them. With the author's settings (`quakevr/ironwail.cfg`, `vr_defaults.cfg`: normal maps, bumps on models, parallax on
models 0.125, directional ambient, rim light, weapon reflections, specular AA, per-pixel dynamic lights with 12
shadowed lights and 16-tap filtering, tone mapping to RGBA16F, grade, dither, bloom 0.1, soft particles, no MSAA).

**How it was measured.** The mock headset at `vr_render_scale 2` (two 2048² eyes), `host_framerate 0.01` and paused
(the same frame every run), `vr_profile`, GPU milliseconds per frame for both eyes. Runs of the same build differed by
up to 30% (the desktop's own programs share the GPU), so every change was measured **in one run**: temporary cvars
switched between the old and the new shader or code path (compiled side by side), in alternating 200-frame
intervals, three or four each, and the medians compared; then the switches were removed. Images: `vr_eyeshot` of both
eyes for each variant of the same paused frame, 8-bit (`qbase2`) and QRP (`qbase3`), diffed per pixel against a
second shot of the same variant (the ammo screen's CRT flicker and the sky's scroll run on real time: 0.02% of the
pixels by more than 4 levels in every pair, all on them).

**Where the time goes** (before; e1m1 with a grunt at 2 m and the shotgun held / vrfiringrange by the dummy / e1m1
after two rockets): the eyes 1.17 / 1.05 / 1.36 ms, of which models (`alias`) 0.068 / 0.105 / 0.040, `vr particles`
0.080 / 0.100 / 0.146 (the scene's distances 0.064 / 0.056 / 0.028 of it), decals - / - / 0.092, bloom 0.088 / 0.12
/ 0.088, post-process 0.106 / 0.11 / 0.112, mirror 0.011, env cube 0.016. The headset's eyes have 2.77 times the
pixels. What each part costs, per pixel in the common case:

- **Models** (per pixel): 4 screen derivatives; parallax (depth 0.125 is active on held weapons, hands and close
  monsters): a height read, then 4 to 8 steps and a secant, 6 reads with the surface's gradients (23% of the pass);
  the skin, the normal map (the bumps, Toksvig from its mip) and the fullbright map; 6 more derivatives (specular AA);
  the ambient cube (4 reads of the instance), the rim (4 more); the reflection for metal texels of held weapons (one
  cube read); the light-cluster read and, per dynamic light reaching the pixel, its data, falloff, cone, shadow
  (1 to 16 compare taps), angle and sheen (37% of the pass by the hands, which the ammo screen's and the gadget's
  small lights always reach); fog. Per vertex: two poses (three with the zero-frame blend), the world matrix, the
  shading direction. Measured by switching each feature off: parallax 0.013 of 0.056, dynamic lights 0.021, rim 0.003,
  reflections 0.004, ambient and bumps about 0.001 each.
- **Post-process** (one pass, into the eye's image): the float scene (8 bytes), four bilinear taps of the bloom, the
  tone curve, the headset's gamma, the 3D grade and two hashes for the dither; the bloom's taps are 29% of it and the
  grade 24% (both switched off in turn), the rest reading and writing the image.
- **Bloom:** the bright pass reads the whole float scene at a quarter of the size (with RGB10_A2 instead of RGBA16F it
  took 0.072 instead of 0.088), then seven small passes (three halvings, the mean, three doublings), R11G11B10F.
- **Particles and decals:** one texture read (the soft ones also the scene's distance) and a blend into RGBA16F for
  every pixel of every quad: overdraw is the cost.

**Kept** (each with the same image: no pixel differs by more than the noise above, 8-bit and QRP):

| Change | Where | Before | After |
|---|---|---|---|
| Quake VR's triangles shader: one program per shade (`#define MODE`) instead of a branch on a uniform, and in blended passes that write no depth a fragment that adds nothing (all four zero) is discarded, so the blend unit neither reads nor writes it (the empty parts of particles' and decals' quads) and the soft ones read no distance there | `vr_gfx_gl.cpp` | e1m1 two rockets: vr particles 0.353, decals 0.104 | 0.328, decals 0.073 (the programs alone: no change to the decals) |
| Particles culled to each eye's frustum (`R_CullBox` on a box as big as the quad can reach) before their quads are made, and the scene's distances made only when a soft particle or sprite is drawn | `vr_particles.cpp` (`buildQuads`, `VR_DrawSceneTranslucent`) | the same: vr particles 0.328, CPU "particle verts" 0.154 ms a frame (both eyes); QRP, both changes: 0.482, CPU 0.424; e1m1's grunt view (particles alive, none in sight; across builds): 0.080 | 0.289, 0.119; QRP 0.307, 0.370; 0.002 (no distances pass) |
| Shadow maps' casters drawn depth only: no light, ambient cube, rim or reflections set up for each face, and a program with no fragment shader (holey skins keep theirs for the alpha test) | `r_alias.c` (`R_DrawAliasModelsDepth`, `ALIAS_DEPTH`), `gl_shaders.c` (`alias_depth`), `vr_lighting.cpp` | three grunts, three shadowed lights: CPU "shadow alias draw" 0.054, GPU shadow maps 0.033 | 0.033 (the eyes' pass sets their light up first instead: +0.005), 0.030 |
| The models' dynamic lights: the light's distance and direction worked out once for its falloff, cone, shadow offset, angle and sheen (they each normalized it again), the direction to the eye once for all lights | `gl_shaders.h` (`ModelDynamicLights`) | alias with lights on models: 0.100, 0.091, 0.057, QRP 0.069 | 0.095, 0.087, 0.055, 0.065 (-4 to -6%; no change without lights) |
| The weapons' environment cube makes the mipmaps of the face just drawn (a `glTextureView` of it) instead of the whole cube's | `vr_envmap.cpp` | hands moving (a face each frame): env cube 0.025 | 0.006 |

**Rejected** (measured the same way; the code stays as it was):

- `pow` with constant exponents in the rim light, Schlick's fresnel and the force-grab glow written as multiplies: no
  difference (the compiler already does it).
- Skipping the Bayer banding noise in the model and sprite shaders when `ScreenDither` is 0 (the eyes with
  `vr_dither`): 0.074 against 0.074, 0.130 against 0.133.
- Skipping the post-process's `pow` for `vr_gamma 1`: 0.110 against 0.110, 0.149 against 0.152 (bound by reading the
  image, not by arithmetic).
- Parallax reading four steps at a time (independent reads instead of a read per iteration): 7% slower for models.
- Trimming particles' quads to their texture's visible area: only the disc and the gun smoke leave much of their
  cell empty, a trimmed quad must keep a margin for the mipmaps (the far ones' halos), and the discard above already
  spares the blending of empty texels. Octagonal quads for round puffs triple the vertices.
- A smaller scene format: R11G11B10F (`vr_tonemap 2`) bands (round 17); the bloom chain already is R11G11B10F.
- Fewer bloom passes: the chain is sequential by nature (each level from the one before) and the mean of the view
  needs the smallest level; its seven small passes are mostly their fixed cost.
- Packing the model instance (224 bytes: 16-byte aligned, the same layout as the shader's `InstanceData`, one upload per
  batch) tighter, e.g. the ambient cube in half floats: every model pixel would unpack it, for no measurable gain.
- The mirror to the window (bloom, curve and grade per window pixel, then the window's own post-process): it is what the
  window shows; at the author's 3440x1440 it is about 0.1 ms.

**For the headset:** the per-pixel savings scale with the pixels (2.77 times): about 0.26 ms a frame on two rockets'
particles and decals, and about 0.2 ms whenever particles exist but none is in view; the CPU and env cube savings
do not. Things to watch in the headset: particles, smoke and blood in fights (the culling is by each eye's frustum:
none should vanish at the edge of the view); decals; shadows of monsters and your body from map lights and muzzle
flashes; reflections on the held weapons while moving them.

## Holsters on the body

Voice note vrfiringrange_2026-09-26_14-35-15 ("very straight and plain and flat ... a little bit of a curve that
adapts to the body"), and round 18's finding: with the body drawn, the hip and upper holsters sit on the front of the
body (round 15) but were still turned as they were for no body (plates side-on, facing sideways), so they stood out
of the belly and chest edgewise and the player looking down saw the plate's body-side face.

**The plates lie on the body.** `body::holsterPositions` can now also return, for each holster on the drawn body, a
`HolsterPlate` (vr_body.hpp): the way the body's surface faces there (`out`), its up along the surface, and how far
the holster's position stands out of it. The torso's rings (make_vrbody.py) are ellipses round the spine, so the
surface's normal is the ellipse's at the holster: the belly's (0.115 m deep, 0.17 m to the side) at the hips, which
faces 25-35° outward from straight ahead there (more with the torso further back, vr_body_torso_back); the chest's (0.13 and 0.19 times the build's torso scale) at the
upper holsters, which face about 34° outward and are tilted back 20° (they sit above the chest's widest, by the
collarbones, where it slopes back towards the neck). It is worked out on the standing body and then carried by the
Follower like the positions, so the plates lean and crouch with the pelvis and chest. `setupHolsters`
(vr_view.cpp) turns legholster.mdl to match: its +y (the plate's concave side) points into the body, +z (the belt
loop) up along it, +x along the body towards the middle (the left holsters are drawn mirrored, as before, so they
match the right). The model is moved along the normal so that the plate's back rests on the surface (the position
itself stays where it was, as the hands' reach target; the move is at most 4 units). No model change was needed:
the plate's own curve (round 18) now wraps round the body.

**The holstered weapon hangs in the loops**, at the model's origin, muzzle down along the plate, its top towards
the body's middle and its grip out towards the hand; tipped out 10° at the hips and 25° on the chest (as a chest rig
carries a gun).

**Without a body** (vr_body_mode 0, or vr_body_anchors 0, which keeps the old positions) and for the shoulder slots
(never drawn) nothing changed: the old angles, turned with the body's yaw.

**The body preview carries them.** `vr_body_debug 2` and `3` (the body in front of you, facing you or turned to show
its left side) now draw the holsters and the holstered weapons on the preview too, as the flashlight already was. It
is the only way to see them from outside in the mock.

**Checked** with the mock (e1m1, vr_body_mode 3, vr_body_torso_back 0.07, looks of 60-80° down and 50° down turned
35° to each side, and the previews). With `vr_leg_holster_model_scale 1.2` the plates are big enough to judge: the
upper ones cover the chest's front corners with their loops level and belt loops up, the hip ones stand upright on
the front of the hips. With the body off the shots match round 18's. To tune in the headset: the chest tilt (20°)
and the weapons' tip (10°, 25°) are constants in `plateOnTheBody` (vr_body.cpp) and `holsterOnBody` (vr_view.cpp).
The hip holsters sit at the top of the thighs, below the belt (the positions are round 15's); with a long weapon
there (the axe) the weapon hides most of the plate from above.

## Shaders: world and liquids

The world (brush) vertex and fragment shaders, the liquid shaders (`LIQUID_SWELL`, `LIQUID_FUNCTIONS`, the water
program) and the snippets they use (`SHADOW_FUNCTIONS`, `PARALLAX_FUNCTIONS`, `DETAIL_FUNCTIONS`), with their feeders
(`r_world.c`, `vr_lighting.cpp`). The author's settings (`ironwail.cfg`, `vr_defaults.cfg`): normal maps 1.5, bumps
in map light 1.2 with deluxemaps, parallax 2.25 units, detail textures, specular 0.2 with specular AA, light contrast
2.3, 12 shadowed dynamic lights and 4 map lights at 1024, `vr_shadow_filter 3` (the softest), caustics, waves,
refraction, glints, foam, geometric waves 12.

**How it was measured.** The mock headset at `vr_render_scale 2` (two 2048² eyes), `host_framerate 0.0111` and
paused (the same frame every run), `vr_profile`, GPU ms per frame for **both eyes** (`world+brush`, and `water`: the
opaque and translucent liquid passes). Two things made runs of the same build differ by 20-30% at first: the mock was
paced by `host_maxfps`, and in a background window Windows' 15.6 ms timer turned 250 fps into 64 fps now and then, so
the GPU idled and clocked down (a lighter frame then *measures* slower); and other programs on the GPU. With
`host_maxfps 1000; vid_vsync 0` the GPU is always busy and repeated runs of one build agree within 2-4%; a run is
thrown away when its frame period or its untouched scopes show another program on the GPU (the SteamVR session and
other agents' runs). Each change was built in a private copy of the tree (so the shared tree never had a half-done
shader) and run against the build before it in the same locked session. Images: `vr_eyeshot` of both eyes of the same
paused frame, diffed per pixel against the build before, QRP (`qbase3`) and 8-bit (`qbase2`). Scenes: e1m1's start
corridor (looking along it; and down at the feet, where the body's map-light shadow falls), a riveted wall 36 units
away, the corridor with three `vr_light_test` lights (shadowed), e1m2's pool, start's lava pit, the firing range in
daylight (and over its water).

**Where the time went** (before; per world pixel in the common case): 10 screen derivatives; parallax within 512
units: 2 to 18 anisotropic reads of the height, in a dependent loop; the diffuse, normal map, fullbright, 1 or 2 detail
reads, 1 to 3 lightmap reads, the deluxemap, and (in maps with water) a 3D read for the caustics; `pow` for the
contrast, `atan`, `sin` and `cos` for the deluxemap's lean, `pow` for its sheen, `log2` for the light cluster and the
specular AA, `exp2` for the fog. Then the lights of the pixel's cluster: **every map light reaching the pixel** (the
author's 4 reach most of a room) read two shadow maps (the world's and the moving things'), 16 bilinear compare taps
each (`vr_shadow_filter 3`), 32 reads a light, and every shadowed dynamic light 16 more. Switching features off one at
a time (e1m1's corridor, 1.60 ms): the shadow filter to 1 tap -0.75 ms, parallax off -0.21, normal maps off -0.12,
detail off -0.06, specular AA off -0.06, the rest within the noise. The shadows were half of the world's cost.

**Kept** (world+brush GPU ms, both 2048² eyes; each row against the build before it, in the same session where marked
(s), else across sessions, ±4%):

| Change | Where | e1m1 corridor | wall | 3 lights | e1m2 pool | lava | range water |
|---|---|---|---|---|---|---|---|
| (before) | | 1.379 | 1.060 | 2.558 | 1.744 | 1.200 | 0.107 |
| The 9- and 16-tap kernels read as 4 and 9 bilinear taps: they are separable, each axis a box of 3 or 4 texels with a bilinear ramp at each end (weights 1-g, 1, 1, 1, g), so pairs of texels are read with one tap placed by their weights (Castaño's PCF); the same weights (the hardware's 8-bit filter weights aside) | `ShadowFilter` | 0.774 | 0.580 | 1.135 | 0.902 | 0.676 | 0.110 |
| A map light's moving things' map is read first, and the world's only where something moving blocks the light (the result is the same: nothing moving in the way means no shadow to add); the face and texel worked out once for both maps; the face picked without branches or constant-array lookups (`ShadowFace`) | `MapLightShadow` | 0.651 | 0.484 | 0.962 | 0.762 | 0.603 | 0.107 |
| The deluxemap's 50° lean clamp without `atan`/`sin`/`cos` (the tangents compared, the direction normalized); the Quake falloff's plane and the force-grab rim use the facing normal already made instead of taking the derivatives and normalizing again | `LuxLight`, the light loop | 0.665 | 0.479 | 0.964 | 0.764 | 0.597 | 0.107 |
| Liquids: the swell worked out once for the waves and the foam; the scene's distance at the pixel read once for the foam and the refraction; the refraction's second projection as `c0 + ViewProj * d`; fresnel's cube as multiplies | `LiquidWaves`, `LiquidFoam`, `LiquidRefract` | 0.655 | 0.477 | 0.973 | 0.762 (water 0.138 > 0.132) | 0.589 | 0.107 (water 0.180 > 0.175) |
| (s) A map light's faces with no moving caster drawn in them are skipped outright: `vr_lighting.cpp` tests each face's pyramid against the casters' bounding spheres (the player's and skeletal models twice their bounds, 96 units at least; brush casters count for every face) and hands the mask to the shader in the light's `shadow.w` (1 + the mask: still nonzero, so the alias shader and `r_alias.c` still tell map lights apart); a light with no face at all is not sent | `viewHasCasters`, `MapSlot::faceMask`, `VR_PushMapLights`, `MapLightShadow` | 0.674 > 0.629 | 0.479 > 0.478 | 0.949 > 0.904 | 0.753 > 0.752 | 0.587 > 0.581 | 0.107 |
| (s) A depth pre-pass of the opaque world and brush models: the same draw calls, depth only (`glprogs.world_depth`: the world vertex shader with no fragment shader), then the shading pass, whose `gl_Position` is `invariant` so it passes the same depths: the costly shading runs once a pixel instead of for every surface drawn over later | `R_DrawBrushModels_Real`, `R_AddBModelPassCalls`, `gl_shaders.c`, `glquake.h` | 0.641 > 0.571 | 0.478 > 0.499 | 0.940 > 0.807 | 0.770 > 0.665 | 0.585 > 0.536 | 0.109 > 0.121 |

**All together** (two runs of each build, averaged; world+brush / water / the eyes' whole GPU time):

| Scene | Before | After |
|---|---|---|
| e1m1 corridor | 1.341 / 0.038 / 1.851 | 0.567 / 0.044 / 1.068 (world -58%) |
| e1m1, looking down at the body | 1.391 / 0.046 / 2.032 | 0.583 / 0.034 / 1.197 (-58%) |
| riveted wall, 36 units | 0.995 / 0.034 / 1.480 | 0.496 / 0.043 / 0.988 (-50%) |
| e1m1 with three shadowed lights | 2.134 / 0.037 / 2.675 | 0.807 / 0.034 / 1.332 (-62%) |
| e1m2's pool | 1.569 / 0.138 / 2.229 | 0.665 / 0.131 / 1.317 (-58%) |
| start's lava | 1.046 / 0.084 / 1.730 | 0.525 / 0.092 / 1.216 (-50%) |
| firing range, daylight | 0.434 / 0.050 / 1.024 | 0.260 / 0.056 / 0.865 (-40%) |
| firing range, over the water | 0.110 / 0.186 / 0.855 | 0.125 / 0.177 / 0.862 (+0.015 world: the pre-pass where the world is cheap) |
| 8-bit textures (`qbase2`), the first five | 1.255, 0.938, 2.165, 1.470, 0.913 | 0.444, 0.422, 0.710, 0.516, 0.412 |

At the Quest 3's two 3292 x 3524 eyes (2.77 times the pixels) the corridor's world pass goes from about 3.7 to about
1.6 ms a frame, and with lights and shadows about, from 5.9 to 2.2 ms (of an 8.3 ms frame at 120 Hz). The CPU cost of
the world's draw calls stays about 0.005 ms an eye (twice the calls for the pre-pass).

**Images** (the build before vs after, 2048² eye shots, QRP and 8-bit): the shadow filter and the map-light changes
change nothing visible (at most 11 levels at a handful of penumbra pixels, the filter weights' 8-bit rounding);
LuxLight, the liquids and the face mask likewise (the liquids at most 1 level). The only visible-in-a-diff change is
the pre-pass's `invariant gl_Position`: the vertices' positions are worked out without the compiler's contractions,
some ULPs off, so a handful of edge pixels flip between the two surfaces meeting there (start's lava pit: 26 pixels over
4 levels, as sparkle dots along the lava's edge, which were there before in other pixels) and the parallax on the
riveted wall moves by a fraction of a texel (mean 0.17 levels, at most 21 on 28 pixels). The pre-pass itself is exact:
the same build with and without it gives the same image. The body's shadow from map lights (`vr_shadow_self 2` against
0, looking down) changes the same pixels, by the same amounts, with the face mask as without (93130 pixels, at most 28
levels; and on the shared tree with the other agents' work, mask against all faces forced: identical images).

**Rejected:**
- **Parallax's height reads as `textureLod`** (one isotropic read at the anisotropic filter's level instead of
  `textureGrad`'s): -5% on e1m1, +8% on the lava, and the relief's silhouettes changed by up to 160 levels on 10 000
  pixels.
- **Fewer parallax steps where the texture is minified** (1.5 steps a texel of the mip read, not of the top level):
  with QRP's 512² textures the step count is limited by `vr_parallax_steps` (16) long before, within the 512 units
  parallax reaches, so it would change nothing; the 8-bit textures already take 4 to 6.
- **Compile-time variants** (e.g. the world program without the parallax code for `vr_parallax 0`): with parallax off,
  the variant measured 0.460 / 0.665 / 0.516 / 0.422 against the uniform branch's 0.443 / 0.658 / 0.512 / 0.407. The
  feature switches are uniform (the same for the whole draw), so the branches cost nothing measurable and the program
  count stays as it was.
- **Parallax four steps at a time** (the models' agent's experiment in the shared `ParallaxUV`): slower for models;
  not pursued for the world.
- **Skipping a dynamic light's shadow on surfaces facing away from it** (no angle term and no sheen there), the Bayer
  banding noise skipped when `ScreenDither` is 0 (the eyes with `vr_dither`), `pow(rim, 2)` as a multiply: no
  measurable change (the compiler already does the last), so the code stays as it was.
- **An early out of the shadow filter when its corner taps agree** (The Lab's): it misses a shadow thinner than the
  kernel between them, a change of the look.
- **Caching the detail texture's settings in `texture_t`** instead of a hash lookup per draw call: the world's draw
  calls cost 0.005 ms of CPU an eye.
- **Liquids further**: each of waves, foam, refraction, glints, fresnel and the swells costs 0.01 to 0.03 ms of the
  0.13 to 0.17 ms a liquid-filled view takes (switched off one at a time); nothing stands out.

**Notes for the other shaders.** `SHADOW_FUNCTIONS` is shared with the alias shader: models lit by shadowed lights get
the 9-tap filter too. `ShadowFace` returns the face in an `out` parameter (the map-light mask). The world vertex
shader's `gl_Position` is `invariant`; `glprogs.world_depth` must stay built from the same `world_vertex_shader`.

**Check in the headset:** shadows of monsters and of your body from map lights (the lamps' shadows of moving things)
as you and they move between a lamp's sides (the face mask: a missing shadow in one direction from a lamp would be it);
dynamic lights' soft shadows (`vr_shadow_filter` 2 and 3); walls, doors, lifts and the item boxes (the depth
pre-pass: no flicker or holes where surfaces meet; fences and liquids are not in it); the water, lava and slime as
before.

## CPU: particles, decals, gore, lights

Quake VR's particles (`vr_particles.cpp`: the pool, the presets, trails, splashes, the quads), decals (`vr_decals.cpp`:
placing, clipping onto the world, the frame's vertices), gore (`vr_gore.cpp`), casings (`vr_shells.cpp`), body blood,
the emissive lights (`vr_emissive.cpp`: projectiles, lava nails, beams, torches), text and lines.

**How it was measured.** The mock backend, `vr_profile 1`, the scopes' CPU ms per frame for both eyes. Scenes:
**e1m1 rockets**: `vr_decal_max 2048` (the menu's help asks 1024 or more for the gore; the author's config has 512), a grunt spawned
ahead (impulse 244) and rocketed point blank (impulse 160), 30 times, then the aftermath: 3000-5000 particles alive
during the fight, 2048 decals (26 000 settled vertices, up to 14 000 changing each frame), about 100 entities; **e1m2
splashes**: under the moat's surface looking up, `vr_particle_test 14 40` (a body's splash) every 12 frames, 60 times:
about 1400 particles, 150 of them rings and foam lying on the waves; **e1m3 torches**: the start, 3000 frames. Runs of
the same build on this shared machine differed by up to 2x (the frame went from 4 to 15 ms with the other agents'
games), so a change was kept only if it showed **in one run**: a temporary cvar switched between the old and the new
code every 154 frames (two rocket cycles), eight intervals each, the medians compared; separate runs (a baseline built
from a copy of the worktree with only these changes taken out, alternating base/new/base/new) served as a check. A
microbenchmark (the particle code copied into a standalone program, 3000 frames of explosions, blood and trails,
4065 particles alive on average) explained the in-game numbers.

**Where the time goes** (e1m1 rockets, a quiet run, the current code of everyone else): particle simulation 0.019,
particle quads (`particle verts`, both eyes, after the round's frustum culling) 0.138, their upload and draw 0.029,
decals' vertices 0.069 and their draw calls 0.045, gore 0.002, gibs' blood (`gib trail`, 30 gibs) 0.004, all the
entities' relinking with the projectile lights and bullet holes 0.007, torch lights 0.002 (e1m3: 0.003-0.004),
casings 0.001. In e1m2's splashes the particle quads are 0.47-0.55 ms, 0.33 ms of it the rings and foam lying on the
waves (each corner of their grids of up to 12 x 12 pieces asks `water::surfaceRise` for the surface's height: four
hash-map lookups). Particles and decals are the only costs here worth a look; the rest is a few microseconds.

**Kept** (the same output: the same floats in the same vertices):

| Change | Where | Before | After |
|---|---|---|---|
| The particles' quads: the vertex arrays (`quads`, `lyingVertices`) grow and are never cleared, with a count of this view's, so each eye no longer constructs 6 default vertices per particle (and 6 per piece lying on a liquid) only to overwrite them; and each particle keeps the cosine and sine of its angle (`cs`, `csAngle`), worked out again only when the angle changed: most particles never turn (twice a frame each before, once ever now), the spinning ones once a frame instead of once per eye | `vr_particles.cpp` (`buildQuads`, `lieOnLiquid`, `VR_DrawSceneTranslucent`) | e1m1 rockets, one run alternating: particle verts median 0.259 (pairs 0.26, 0.31, 0.37, 0.35 ... 0.059, 0.055, 0.057) | 0.206 (0.19, 0.21, 0.22, 0.26 ... 0.040, 0.038, 0.038): -20 to -40%; the upload +0.005. Separate quiet runs: 0.138 against 0.140 (no difference when the CPU has bandwidth to spare). Microbenchmark, both eyes, no culling: 0.29 to 0.19 (the construction 0.07 of it, the sines 0.03) |

Also new (for the next round's profiling): the scopes `particle sim`, `particle verts` (the quads alone) and
`particle upload` under `vr particles`, `decal verts` under `decals`, `gib trail` under `relink`, `shells` under `view
entities`; `vr_decal_count` also prints the decals' vertices (settled, and changing each frame).

**Rejected** (measured; the code stays as it was):

- **The particles as a structure of arrays** (19 arrays, each type's behaviour turned into rates when it is made, one
  branch-free loop for all, the few with a palette ramp or on a liquid in short loops of their own, the dead squeezed
  out in order): the simulation went from 0.016 to 0.046 ms (microbenchmark; in the game 0.045 against 0.046): the
  ordered squeeze moves every array past the first dead particle, and particles die in the order they were made, so
  nearly all of them every frame. Removing the dead by swapping the last into their place (cheap) would change the
  order they are drawn in, and with it the premultiplied "over" blend of overlapping smoke and blood: a puff would
  jump in front of or behind another as others die (flicker). The array of structs with its switch was already about
  4 ns a particle. Making each particle's colour, turned axes and softness once a frame for both eyes (a second
  pass) cost more (0.072) than it saved in the eyes' loop (0.02).
- **A faster sine and cosine** (a polynomial, 1e-7 off): within the noise once the angles are kept (above).
- **A faster random generator** for spawning (PCG instead of `mt19937`): 1.8 against 2.2 ns a number.
- **Decals: the settled marks kept rather than rebuilt** (appended once as each settles instead of rebuilding all of
  them whenever a mark came, went or settled, which in a fight is every frame; a mark gone left as triangles with no
  area until a quarter of them were; kept in a GPU buffer of their own with only the changed range sent, instead of
  uploaded again for each eye), with the drops counted as they come and go instead of each time a drop is added, and
  no allocation per mark in the clipping. Two alternating pairs of runs: decal verts 0.064/0.066 against 0.070/0.074,
  the draw calls 0.015 against 0.014, GPU no better. What each frame costs is the changing marks (splatters darken for
  8 s, pools spread for 12-20 s: 8000-14 000 vertices made again each frame in a fight), which this does not touch;
  rebuilding the settled ones is a straight copy, and the upload is small. Taken out again, with the `gfx::Mesh`
  (retained vertex buffer) it had added to `vr_gfx`.
- **Torch lights: the flame statics found once a map** instead of by name every frame: 3-4 µs before and after.
- **Projectile lights, the lava nails' glow boost, bullet holes: model flags instead of name compares**: the compares
  fail at the seventh character, and the whole relink of about 100 entities (with the gibs' blood) is 0.007 ms; a
  cache keyed by model would have to be forgotten when `Mod_ResetAll` reuses the slots.
- **Gore traces**: already at most 24 lines of blood traced a frame, and a falling drop is traced once when it starts
  to fall (not each frame); the gore's whole frame is 0.002-0.005 ms. Casings (at most 64, one trace a frame each in
  the air, a floor check every 0.3-0.6 s at rest) 0.001 ms.

**Left for their owners** (bigger, and outside these files):

- **Splash rings and foam on the waves**: `water::surfaceRise` looks each height up in `unordered_map`s (the pins, and
  the frame's cache), four per corner of each piece's grid (up to 13 x 13 corners, 150 pieces a view in the splash scene: 0.33 ms of CPU a frame). A dense grid of
  the frame's heights over the liquids in view (an array indexed by the pin's cell) would make each lookup a load.
- **The particles' vertices**: 6 of `gfx::Vertex` (40 bytes) per particle per eye, then copied again into the
  frame's upload buffer. Drawing them instanced (one 48-byte record per particle, the quad made in the vertex shader)
  would cut the CPU's quads and upload by about five times and the bandwidth with them; so would writing straight
  into the mapped upload buffer (an engine function, `GL_Upload`, copies from a caller's array).
- **The changing decals**: spreading and darkening worked out in the shader from each vertex's birth time would leave
  nothing for the CPU to rebuild each frame (and would make the retained buffer above worth having).

**Check in the headset:** particles and splashes look as before (the same vertices); in a big fight, `vr_profile 2`'s
`vr particles` and `decals` rows.

## VR menu

Request: a taller menu in the headset (more options without scrolling), and the largest Advanced VR Options pages
split into separate pages.

**Taller menu** (`vr_menu_height`, new, default 1.35; Advanced > Menu > *Menu Height*, 1 to 2). With the VR menu
style the menu canvas used to fit 320 x 200 menu pixels (times the row spacing) into the panel. It now fits
320 x (200 x `vr_menu_height`, rounded to whole rows): 272 at the default. Quake's 320 x 200 stays in the middle of
the panel (at eye level), and the extra rows go above and below it. A menu pixel is still `vr_menu_scale` units in
the world, so the text is the same size as before; only the panel is taller.

- With the shipped settings (distance 69, scale 0.2, spacing 1.8), the panel is 272 x 1.8 x 0.2 = 98 units high,
  about **±35°** from straight ahead (it was 72 units, ±27.6°).
- The VR pages lay out in the whole height: the title at its top, the list, and four lines of help at its bottom.
  That's **24 rows** on a page with help (was 15) and **28** without (was 19). On the desktop, and in VR with the
  style off, it is Quake's 200 (15 and 19 rows) as before.
- Ironwail's own lists (options pages, key bindings, maps, mods) lay out from the canvas's bounds (`M_UpdateBounds`),
  so they get the taller panel with no change. The menus drawn from pictures (main, single player...) stay in the
  middle.
- The laser, the mouse, right-stick scrolling and the scrollbar go through the same layout (`layout()` in
  vr_menu.cpp, `menuui::menuHeight()`), so rows under the laser are the rows drawn.
- **Back to game** stays at the panel's top, but no further left than just left of Quake's plaque (x -110 menu
  pixels). The canvas is wider in menu pixels now (a 16:9 window), so the panel's corner would have been at about
  51° to the left. It is now at about 32°.
- Cost: the canvas is the desktop window's size, so a taller menu gets fewer texels per menu pixel (a 1080p window:
  2.2 instead of 3; the mock's 960 x 540: 1.1). If the text looks soft in the headset, a bigger desktop window
  helps, or a lower `vr_menu_height`.

**Pages** (`pages[]` in vr_menu.cpp, the builders there and in vr_menu_pages.inc). The Advanced VR Options list is
now grouped by topic (Game, Body and Movement, Weapons, HUD and Menus, Graphics) instead of "the port's own" and
"Quake VR's". Page titles can differ from their short labels in the list (for example, "Liquids" opens "Graphics -
Liquids"). The Graphics page also links to its sub-pages. Rows (items + headers), before and after:

| Before | Rows | After |
|---|---|---|
| Graphics | 108 | Graphics 20 (preset, image, links, performance), Lights 16, Shadows 16, Surfaces 18, Liquids 24, Post-processing 12, Models and Effects 21 |
| Body | 45 | Body 18, Arms and Pauldrons 17, Flashlight 11 |
| Gameplay | 45 | Gameplay 27 (damage, knockback, swords, feel, voice notes), Parry, Bash and Headbutt 19 (with batting projectiles) |
| Throwing and Physics | 33 | Throwing and Physics 18, Carrying and Gibs 16 |
| Wrist Gadget | 32 | Wrist Gadget 10, Screens 16 (the gadget's screen, the weapons' ammo screens and map boards from Immersion), Colours 15 |
| Immersion | 29 | Immersion 21 (with headers); its force grab items went to Force Grab (16 items) |

The rest are unchanged: Play 8, Melee 13, Gore 19, Force Grab 14 (now 16), Player Calibration 2, Locomotion 19,
Swimming 25, Hand/Gun Calibration 10, Weapon Offsets 41, Aiming 14, Hotspots 25, Status Bar 9, Crosshair 6, Menu 7
(now 8, with Menu Height), Particles 3, Transparency 5. The VR Settings main page (50) is as it was.

- No setting was lost. Items were moved with their text, range and help unchanged (a script did the Graphics and
  Immersion split). A script comparing the cvars that menu items reference (HEAD against the worktree) finds the
  same 428, plus `vr_menu_height`. The one consolidation: Immersion's *Force Grab*, *Force Grab Particles* and
  *Force Grab Haptics* were duplicates of the Force Grab page's. They are now only there (with Immersion's help
  texts), next to Immersion's *Outline* and *Effects*.
- Help texts that pointed at the old places were updated: the preset now "sets the settings on all the Graphics
  pages", Auto blob shadows refer to "the real shadows (above)", and hues say "on the Colours page".
- Page indices are positional (`menu_vr <n>`). Links find their page by its builder (`pageIndex`, which replaces
  `weaponOffsetsPage`). "Reopen where left" stores the index for the session only. **`menu_vr list`** prints the
  numbers: 0 VR Settings, 1 Advanced, 2 Play, 3 Gameplay, 4 Parry, 5 Melee, 6 Gore, 7 Throwing, 8 Carrying, 9 Force
  Grab, 10 Body, 11 Arms, 12 Flashlight, 13 Player Calibration, 14 Locomotion, 15 Swimming, 16 Immersion, 17
  Hand/Gun, 18 Weapon Offsets, 19 Aiming, 20 Hotspots, 21 Wrist Gadget, 22 Screens, 23 Colours, 24 Status Bar, 25
  Crosshair, 26 Menu, 27 Graphics, 28-33 its sub-pages, 34 Particles, 35 Transparency.
- docs/SETTINGS.md lists the new pages. Other docs still name the old places (FEATURES.md "Wrist Gadget > Colours",
  INSTALL.md "Graphics > Lights and Shadows > Preset", TESTING.md "Gameplay > Feel", "Graphics > Performance").

**Tested with the mock** (start, the shipped menu settings, a 960 x 540 window):

- Advanced, Graphics, Liquids (24 rows, which fit exactly), Gameplay, VR Settings, and Ironwail's Options, drawn in
  the taller layout.
- Looking 25° up and 25° down: the panel's top and bottom are at about ±34-35°.
- The laser on row 18 of Liquids (*Ripples*, below the old 15 rows) highlighted it, and the trigger switched
  `vr_water_ripples` from 1 to 0. On row 3 (*Water Glints*, in the old title area) it highlighted that row.
- The laser on Back to game lit it, and the trigger closed the menu. `togglemenu` reopened Liquids with the same
  row selected.
- The right stick scrolled Advanced (39 rows) to its end.
- Live preview: two shots of Liquids 100 frames apart differ (the game runs).
- With `vr_enabled 0`: Liquids, Graphics and Options in the classic 320 x 200 layout (15 rows).

**Check in the headset:**

- Is ±35° comfortable? Try *Menu Height* between 1.2 and 1.5 (a higher value helps most on Gameplay, Swimming and
  Weapon Offsets). If the top rows are a strain, a lower *Menu Distance* makes the text bigger, not the panel
  smaller.
- Is the text sharp enough? See "Cost" above.
- Click rows at the very top and bottom of a long page, drag a slider there, and drag the scrollbar.
- Back to game: it moved nearer the menu. Hover it, click it, then reopen the menu.
- Go through the Advanced list's groups: are the pages where you'd look for them?

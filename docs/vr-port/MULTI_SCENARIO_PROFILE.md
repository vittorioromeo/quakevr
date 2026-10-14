# Stacked profiling across gameplay scenarios (2026-10-14)

Measured worker `profilemulti`, base `e3cebc32c`, optimized Release-Tracy, depth 8.
The sole engine change adds the existing whole `3D` GPU scope to detail 0's allowlist.
No gameplay, graphics or cvar defaults changed. This supplies whole-render timing in both
flatscreen and native mock runs without enabling the many detailed GPU scopes.

## Findings to investigate next

* **AD server work grows beyond the gun-creation bottleneck.** With 214→212 live monsters,
  main-thread `QC VM` costs 0.326 ms/frame exclusive versus e1m1's 0.146; `Server physics`
  has another 0.283 exclusive versus 0.058. Its inclusive mean/p95/p99/max is
  0.973/1.305/1.458/2.137 ms. The capture contains 33,429 VM entries in 600 frames
  versus 6,667 in e1m1 AI. `PR_ExecuteProgram` (`pr_exec.c:605`) has decoded callers
  through `VR_AfterPlayerPostThink` (`vr_physics.cpp:1832`), `SV_Physics_Client`
  (`sv_phys.c:1081`) and `SV_Physics` (`sv_phys.c:1348`). This demonstrates the path,
  not that all VM time belongs to that caller. QC detail 0 cannot name the hot QC functions;
  a short function-detail capture is the next useful discriminator.
* **AD shadow-caster collection is a separate CPU cost.** `dlight casters` is
  0.086 ms/frame exclusive versus 0.015 in e1m1 AI, with 2,353 entries/600 frames.
  Per-entry p95/p99/max is 0.042/0.051/0.170 ms. Its decoded path reaches
  `VR_RenderShadowMaps` (`vr_lighting.cpp:1894,1941`), where the scope covers
  `collectWorld`, `collectBrushes` and `collectAliases`. Their individual shares remain
  unmeasured. The inclusive shadow-map CPU scope averages 0.187 ms, while the nested
  GPU shadow scope averages 0.054 ms; these are different datasets and must not be added.
* **Settled-looking wound poses can remain an active physics workload.** In the native
  default-drive capture after 16 simulated seconds, six wound-pose corpses still have
  66/88 parts awake; only 22 parts sleep. `Solve Constraints` mean/p99 is 0.316/0.371 ms
  per step. For an explicit existing `vr_ragdoll_active 0` reference, all 88 parts sleep,
  and that falls to 0.014/0.036 ms. This is a workload distinction, not a recommendation
  to turn the feature off. The remaining sync scope is 0.110 ms/frame exclusive even
  when all rag parts sleep; pose/sync scanning is worth separating before attributing
  it to the solver. Native solver source is `box3d/src/solver.c:1893`, reached from
  `b3World_Step` (`physics_world.c:1145`) through the engine physics path.
* **One held corpse wakes precisely its 11 parts.** A separate native mock capture
  proves the off hand holds `thigh_r` at both window ends (1.7→1.5 units from the hand),
  with 77 other rag parts asleep. Solver mean/p99 becomes 0.103/0.145 ms per step.
  This reference uses drive-disabled corpses spawned dead and a nearer first spawn;
  it is not an otherwise-identical before/after comparison with the active-death scene.
* **Cold gun creation remains a tail cause, including during AD steady play.** The
  weapon window has two decomposition calls, 34.969 and 40.477 ms inclusive;
  voxelisation alone takes 19.534 and 19.539 ms. Decoded callers are
  `voxelise` (`vr_convex.cpp:49`) → `decompose` (`579,616`) → `cutGun`
  (`vr_box3d.cpp:2060`) → `gunPieces` (`2133`). AD steady contains one 26.262 ms
  decomposition with 10.392 ms voxelisation inside a 27.473 ms sync event.
  Warm weapon cycling has no decomposition events and sync max 0.198 ms. This extends
  the known creation finding to an actual AD scene; it does not assign every stall to it.

## Bounded measurements

All values are milliseconds. CPU busy and its percentiles come from the engine's exact
benchmark window. GPU values are individual whole-3D timestamp durations selected by
their CPU emission boundaries; none is a sum of nested eyes/shadows. Each listed GPU
window has one 3D event per recorded frame. Scope percentiles above are per event;
exclusive costs are accumulated duration minus immediate children, divided by frames.

| Phase / frames | CPU busy mean | p95 | p99 | max | GPU 3D mean | p99 |
|---|---:|---:|---:|---:|---:|---:|
| e1m1 steady / 600 | 1.087 | 1.257 | 1.554 | 2.535 | 0.300 | 0.308 |
| e1m1 +8 live grunts / 600 | 0.836 | 1.147 | 1.376 | 7.837 | 0.351 | 0.390 |
| Cold weapon creation / 120 | 1.920 | 1.515 | 37.159 | 42.501 | 0.321 | 0.329 |
| Warm weapon cycling / 240 | 1.459 | 1.709 | 1.779 | 3.590 | 0.330 | 0.345 |
| Native active deaths/contact / 160 | 2.689 | 3.085 | 3.859 | 8.069 | 0.938 | 1.383 |
| Native default-drive late rest / 600 | 1.832 | 1.952 | 2.056 | 2.262 | 0.842 | 1.316 |
| Native drive-disabled sleep / 600 | 1.506 | 1.652 | 1.776 | 1.852 | 0.923 | 1.388 |
| Native verified held corpse / 300 | 1.659 | 1.846 | 2.218 | 3.049 | 0.934 | 1.405 |
| AD steady depth 8 / 600 | 2.054 | 2.297 | 2.880 | 29.990 | 0.511 | 0.731 |
| AD +8 grunts depth 8 / 600 | 1.966 | 2.338 | 2.549 | 3.484 | 0.574 | 0.843 |
| AD steady depth 0 / 600 | 1.846 | 2.079 | 2.603 | 27.340 | 0.513 | 0.767 |
| AD +8 grunts depth 0 / 600 | 1.804 | 2.131 | 2.475 | 40.039 | 0.573 | 0.803 |

The matched AD pair uses identical settings/scripts/counts and ends at state hash
`d65a8a5097b35f67`. Depth 8 adds 11.3% mean CPU busy in steady and 9.0% with AI versus
connected depth 0. This is one sequential pair. The depth-0 AI tail is worse despite
lower mean time; no universal overhead or tail-regression claim is justified.

## State, execution and accounting limits

RTX 4090; private qbase assets; 4x MSAA; `vr_profile 0`; Tracy QC/memory/GPU detail 0;
fixed game frames with `host_framerate 0.004`; seed 7 for server/ragdolls/particles;
`vr_gun_pieces_cache 1`. Captures run sequentially through exclusive kit slots, without
concurrent builds/decoding. All finish exit 0, connected, zero dropped/force-closed zones.

e1m1, weapons and AD use `-flatscreenmode`, a 960×540 window and 250 FPS cap, with
zero eye resolution. Native rag tests use the mock backend with 1024×1024 eyes and
fast mode, so throughput, camera, lights and render costs cannot be compared across modes.
These tests do not measure Quest resolution, OpenXR compositor/Virtual Desktop latency,
real controller interaction or real-headset frame-budget headroom.

* e1m1 steady: 23 live monsters, 128 bodies, awake 18→16. AI: 31 live,
  136 bodies, awake 26→25.
* Cold weapons: 128 bodies throughout. Warm: 128→140 bodies and 0→67 contacts.
  The script cycles a mixed weapon set; these are creation/cycling workloads with
  differing accumulated dropped weapons, not isolated per-weapon cache benchmarks.
* Native active: eight deaths, 136→236 bodies, awake 21→120, contacts 0→197.
  Disabled-drive sleep: 236 bodies, 12 awake other bodies, 0 contacts; all 88 rag parts sleep.
  Held-specific: eight corpses, 237 bodies, 25 awake at both printed held snapshots.
* AD integrated native campaign on `ad_crucial`: steady 206 live monsters, 611 bodies,
  awake 66→58; AI 214→212 live, 619→643 bodies, awake 66→89, contacts 0→27.
  Ordinary gameplay remains enabled, so the AI window includes two deaths/body changes.

Main-thread accumulated exclusive zone duration/frame is 1.096/0.874 ms for e1m1,
1.930/1.467 for weapons, and 2.062/2.001 for AD depth 8. Worker fully-contained
exclusive elapsed duration/frame is separately 0.649/0, 1.918/0 and 0.632/0 respectively.
For native active/rest/sleep/held it is 1.024/0.003/0/0.035. Long generic worker jobs
include a 388.898 ms e1m1 event and a 360.531 ms AD event, yet main-frame busy tails
remain much shorter. Their decoded source is `Job::claimAndRun` (`vr_jobs.cpp:56`)
through `Pool::post` (`108`); the generic scope does not establish the underlying task.
These are wall durations including possible waits/preemption, not sampled CPU utilisation.
Do not add them to main busy time or infer the frame critical path from worker totals.

The analysis excludes incomplete/phase-crossing zones rather than clipping child trees.
Frame boundaries are the next N `commands` starts after the annotated benchmark begin.
There can be a one-frame difference between this scope-selection window and engine
readback-window GPU counts; the engine itself retains only 594/600 warmed GPU samples.
LTO/inlining can expose folded symbol aliases and unknown source files despite named
function resolution. A stack at scope entry demonstrates its callers, not all leaf
work performed inside it. Unscoped time remains exclusive to its enclosing scope.

## Capture proof and retained evidence

The private reader is built against matching Tracy 0.14.1 source with pre-staged
Capstone 6.0.0-Alpha10, zstd 1.5.7 and PPQSort 1.0.6. No downloads/installations occurred.
Every CPU zone in each depth-8 capture has a nonzero stack ID. Counts below include
loading/warmup as well as measured phases; the phase analysis is restricted to the windows above.

| Capture | CPU zones with stacks | Unique stack payloads | Resolved function frames |
|---|---:|---:|---:|
| e1_d8 | 482,281 | 1,032 | 15,607 |
| weapons_d8 | 247,892 | 961 | 13,969 |
| rags_mock_d8 (default-drive rest) | 4,909,198 | 997 | 14,261 |
| rags_valid_d8 (active/sleep) | 1,451,375 | 1,001 | 14,331 |
| rags_held_d8 | 984,258 | 1,011 | 14,478 |
| ad_d8 | 1,273,515 | 1,567 | 23,941 |

Each has zero unresolved function names, missing decoded frames, statistical samples
and context switches. Depth 0 has 1,335,878 CPU zones and zero stacks/payloads/samples/
context switches, confirming the actual variant difference. GPU 3D exists in both variants.
Normal Release/QC/FGD/statics/precedence build and normal e1m1 smoke passed before
these captures; optimized depth-8 and depth-0 builds passed. No gameplay/physics changes
were made, so this task did not rerun melee or broad regressions.

All raw captures, matching depth-8 exe/PDB, scripts, reader source/build logs, per-capture
phase JSON, full console logs, TSV zones/stacks and analysis JSON are backed up outside
the worker at `C:/OHWorkspace/qvr-kit/scratch/profilemulti`. `details.txt` records extra
scope means/tails and resolved callers. The initial `rags_d8` and attempted-held phases
in `rags_mock_d8`/`rags_valid_d8` are retained diagnostics: their labels do not prove
sleep/holding; only the states explicitly listed above are accepted.

Author VR follow-up: profile ordinary AD combat and a corpse held close to the body
at actual headset resolution; capture a short QC-function-detail AD window if server
time is still dominant. Nothing requires deletion; bulky TSVs and private reader build
outputs are optional cleanup after preserving the raw captures and report.

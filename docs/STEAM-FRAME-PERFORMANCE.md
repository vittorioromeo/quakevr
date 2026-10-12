# Steam Frame performance measurements

Measurements were taken on Steam Frame using the released native ARM64 engine
and its OpenGL renderer on Mesa Zink/Turnip. The engine rendered 1728x1728 eye
images in stereo; tracking was mocked, `vr_mock_fast` was off, and GPU timestamp
queries confirmed both eyes were drawn. These are renderer timings from the
device, not OpenXR compositor latency or a physical headset FPS test.

Each row summarizes two 720-frame runs of the same `combat_48` fixture, paced at
72 Hz after 180 warm-up frames. Settings were run in opposite order to limit
warm-up and order bias. Rows through scale 0.25 use the released engine binary;
the .18 row uses an experimental candidate that lowers the engine's minimum
render scale. All use classic preset 0 and no MSAA or mirror.

| Profile | Render scale | Retro edge softness | GPU eye mean | Frame median | Frame p99 |
|---|---:|---|---:|---:|---:|
| Release baseline | 0.45 | Shipped | 30.138 ms | 31.942 ms | 46.310 ms |
| Crisp edges | 0.45 | Off in all 18 categories | 28.262 ms | 30.158 ms | 42.685 ms |
| Crisp performance preview | 0.30 | Off in all 18 categories | 15.537 ms | 16.349 ms | 23.487 ms |
| Crisp performance preview | 0.26 | Off in all 18 categories | 13.507 ms | 14.179 ms | 20.650 ms |
| Crisp performance preview | 0.25 | Off in all 18 categories | 12.559 ms | 13.888 ms | 20.765 ms |
| Ultra performance preview | 0.18 | Shipped | 9.915 ms | 13.888 ms | 18.758 ms |

The .18 candidate measured 9.982 and 9.847 ms GPU-eye means; both GPU p99
values were below the 13.89 ms GPU budget (13.691 and 13.668 ms). With crisp
edges, means were 9.202 and 9.301 ms and GPU p99 was 12.914 and 13.085 ms.
Frame p99 remained 18.486–19.030 ms, so this still does not establish stable
72 Hz. The .18 render is approximately 311x311 pixels per eye, upscaled to
1728x1728, and is visibly blurrier than .25. The candidate lowers the minimum
allowed `vr_render_scale` from .25 to .18; lower requests are not enabled.
This is an optional, severe quality tradeoff, not a default change.

At scale 0.25 the fixture's frame median reaches the 13.89 ms 72 Hz budget,
but its p99 remains above budget. This result does not prove sustained 72 fps
in campaign play. At 1728-pixel eye-image size, render scale 0.25 produces a
432x432 render per eye before upscaling. That setting visibly reduces detail;
disabling retro edge softness also changes the mod's appearance. The optional
profile is separate from the default launcher and is not a lossless optimization.

With retro textures disabled at scale 0.45, combat GPU eye time fell from
30.264 to 23.569 ms. This removes the mod's defining texture effect, so it is
only a diagnostic control. Retro lighting disabled by itself saved about 3.4%.
Half-resolution retro particles showed no repeatable combat gain. Render scale
had the largest measured effect.

At scale 0.25, keeping the shipped retro edge softness measured 13.411 ms GPU
eyes, versus 12.531 ms with softness disabled (about 7% lower). Frame medians
were 13.939 and 13.889 ms; p99 was 21.257 and 20.296 ms, respectively. Paired
1728x1728 captures show the softened-edge profile preserves softer retro
boundaries while both settings retain the lower-resolution detail loss. Neither
profile demonstrates stable 72 Hz in this combat fixture because p99 exceeds
13.89 ms.

At the normal 0.45 scale, disabling edge softness only for world materials
reduced combat GPU-eye mean from 30.188 to 29.440 ms (2.5%) across two
opposite-order runs; frame median averaged 31.304 versus 31.094 ms and p99
44.789 versus 46.267 ms. Disabling softness for brush materials alone showed
no gain: 30.328 ms versus 30.188 ms. The world-only setting changes the edge
appearance on world textures while leaving model/weapon softness unchanged.
It is a modest optional tradeoff and still does not meet the 72 Hz combat
budget. It does not change the default or the existing preview launcher.

The tested shader fast path for retro-light curves had no gain and was reverted.
An exact-color palette lookup candidate also failed to improve performance: the
corrected version rendered pixel-identical eyes but averaged 30.437 ms GPU-eye
time versus 29.972 ms for release, with a slower frame median. It was reverted.
An earlier version bound new samplers over scene textures and omitted geometry;
its deceptively low timings were invalid and excluded.
An optional switch to disable GPU timestamp queries also failed to improve the
frame time. Dynamic-light and blob-shadow controls showed no useful gain.

The separate performance-preview launcher passed a native mock startup and
reported the expected 0.25 render scale and edge-softness cvars. Its separate
install does not overwrite the normal game or QuakeQuest. Physical validation of
OpenXR focus, head/controller tracking, and core weapon/physics interactions is
still required before distributing the preview as a new release. The original
v1.0.0-frame.1 release remains the known-good build.

## Frame CPU phase and Box3D worker tests

To locate the frame-time tails, the benchmark JSON now records per-frame CPU
phase distributions under `cpu_phase_stats_ms` (average, median, p95, p99 and
maximum). On Frame at render scale 0.25, the same hidden `combat_48` mock run
produced frame p99s of 19.37 and 21.11 ms. `screen` p99 was 16.20 and 16.05 ms;
its nested `swap` phase was 14.88 and 14.68 ms. These phases overlap (`swap` is
inside the screen refresh), so they must not be summed. GPU-eye p99 was about
18.7 ms. The mock window/present and renderer therefore contribute to these
tails; they are not OpenXR compositor or physical-headset frame-pacing results.

Box3D worker settings were compared in the same fixture, each variant for two
720-frame runs in opposite order. Turning physics threads off raised
`SV_Physics` p99 from 4.79–5.17 ms to 5.91–6.35 ms; frame p99 averaged 20.61 ms
off and 20.30 ms on. With threads enabled, worker caps 2, 4 and 8 gave average
frame p99s of 21.52, 20.24 and 20.60 ms. Their `SV_Physics` p99 averages were
5.51, 4.60 and 4.48 ms. Four workers remain the best measured total-frame
balance; eight marginally reduces the physics phase but did not improve frame
p99. This confirms the existing default of four for this fixture, rather than
justifying a code or default change.

The activation thresholds 100, 150, 200 and 300 awake bodies were also tested
twice each. Frame p99 averages were 21.25, 20.11, 19.73 and 21.35 ms, while
`SV_Physics` p99 averages were 5.21, 5.15, 5.59 and 6.88 ms. Run-to-run tail
variation and the tradeoff between physics and whole-frame p99 do not support a
reliable new threshold; the existing 150-body default is unchanged. Raw JSON
and logs are retained in the private `performance-results/frame-diagnostics-20261012`
directory. All runs used mock tracking and actual Frame GPU/CPU hardware, not an
OpenXR headset session. The release executable and regular installation were
not modified.

These tests found a repeatable CPU-physics benefit from the already-enabled
four-worker default. A later particle-density A/B/B/A probe also found a useful
GPU-tail reduction for the optional profile below. Neither hidden-window test
establishes physical OpenXR frame pacing; wearer validation remains required.

## CPU sampling profile

A corrected on-device `perf` capture attached after fixture setup and ended
when its 720-frame JSON was written. The exact `screenshot` and `vr_eyeshot 1`
commands were removed from its isolated config; its run log contains no eye
image writes. It collected 1,496 user-cycle samples with no lost samples across
the Frame's A720, A520 and X4 clusters. On A720, notable symbols included
`PR_ExecuteProgramRun` (3.12%), `SV_WriteEntitiesToClient` (2.97%),
`qvr::decals::buildWorld` (2.04%) and `qvr::particles::buildInstances` (1.09%).
On the lower-power A520 cluster, `qvr::particles::buildInstances` accounted for
12.37% of samples and Quake's `S_LowpassFilter` 9.51%; this is a lead for targeted
measurement, not proof that either dominates frame time. The sample counts are
small and spread across cores, and several Zink/Freedreno and kernel symbols
remain unresolved. Raw perf data, report and matching frame JSON are retained
privately in
`performance-results/frame-diagnostics-20261012/cpu-hotspots-no-eyeshot`.
The earlier profile in `cpu-hotspots-clean-window` is superseded because its
fixture still enabled `vr_eyeshot 1`; PNG compression contaminated the capture.
Both profiles use mock tracking and do not prove compositor timing.

## Audio filter quality probe

Because the A520 sample profile also named Quake's `S_LowpassFilter`, I ran the
same 720-frame fixture at `snd_filterquality` 5 and 1 in A-B-B-A order (two runs
per value). The main-thread `sound` phase averaged 0.091 and 0.084 ms
respectively, but frame p99 ranged 20.06–22.82 ms at quality 5 and 20.26–20.87
ms at quality 1. GPU p99 remained 18.55–18.73 ms and 18.49–18.64 ms. This small
two-run probe does not establish a repeatable end-to-end gain, and changing the
filter changes sound quality, so the shipped value stays unchanged. The
separate `vr_snd_bench` attempt used SDL's dummy audio driver and recorded 0.0 s
of mixed sound; its stage timings are invalid and excluded. Raw JSON and logs
remain private in `performance-results/frame-diagnostics-20261012/`.

## Reduced particle-density preview

The separate 0.25 crisp performance profile now also sets
`vr_particle_mult 0.5`. In an A-B-B-A `combat_48` mock-tracking sweep on the
Frame (two 720-frame runs per value, same 0.25 render scale, fixed seeds and
graphics settings), the 1.0 baseline averaged 12.314 ms GPU-eye time and
18.723 ms GPU-eye p99; 0.5 averaged 11.457 ms and 16.477 ms p99. The per-frame
`vr particles` CPU phase fell from 0.302 to 0.118 ms mean and 2.247 to 0.733 ms
p99. Average live VR particles fell from about 5,185 to 1,954. The
`vr_particle_mult 0.75` control had only a small GPU p99 change (18.059 ms
average), so 0.5 is the only tested value with a clear GPU-tail reduction in
this fixture.

Both settings ended with 38 living monsters and 372 physics bodies, and the
eye-image captures retained the same camera pose and weapon placement. A fixed
1728x1728 left-eye capture differed by 5.94 mean absolute RGB levels (0–255),
concentrated in particle effects. This is a visible reduction in particle
density, so it is confined to the optional performance-preview profile. These
measurements use mock tracking and a hidden X11 window; they do not prove
headset image quality, OpenXR frame pacing, or 72 Hz campaign performance. The
default installation and its cvars remain unchanged.

I repeated the particle-density A-B-B-A on SteamVR/OpenXR 2.18.2 with fresh
stereo projection layers on the Frame, using the same released engine, 0.25
render scale, 1728x1728 eye images, 72 Hz pacing and a deterministic
`particles_dense` fixture (720 measured frames after 180 warm-up). The runtime
reported no wearer presence, so I enabled `vr_xr_unfocused 0` and
`vr_xr_unfocused_pause 0` in the scratch profile; head and hand actions remained
invalid. Every run submitted both eyes with `held=0`. Scene counts other than
the intended particle-density change matched across all four runs: 247 edicts,
one living monster, 134 Box3D bodies, nine contacts, 181 ending decals and 17
dynamic lights.

| `vr_particle_mult` | GPU-eye mean | GPU-eye p99 | VR-particle GPU pass | VR-particle CPU phase | Average live VR particles |
|---:|---:|---:|---:|---:|---:|
| 1.0 | 13.687 ms | 15.815 ms | 4.852 ms | 0.858 ms | 15,546 |
| 0.5 | 11.706 ms | 13.948 ms | 2.809 ms | 0.348 ms | 6,496 |

At 0.5, GPU-eye mean fell 14.5%, GPU-eye p99 11.8%, and the VR-particle CPU
phase 59.4%. Frame p99 fell from 17.492 to 15.938 ms. This confirms that the
preview's particle setting reduces rendering work through the actual OpenXR
path, with a matching 58% reduction in average live particles. The reduced
effect density is visible and remains an optional quality tradeoff. Because the
runtime had no wearer tracking and this is a synthetic effects fixture, it
does not establish controller/head tracking, campaign performance, or a 72 Hz
wearer experience. Raw JSON, OpenXR diagnostics and logs are retained privately
in `performance-results/openxr-particles-abba-frame-20261012`.

## Decal grid sizing on Frame

A 720-frame `decals_1024_stream` baseline ran on the physical Frame with the
released engine (SHA-256 `386e4ba6...`), mock tracking, hidden X11, 1728-pixel
eye targets, render scale 0.25 and 72 Hz pacing. The fixture kept 1,024 decals
throughout. It completed all frames with 11.619 ms average GPU-eye time and
12.247 ms p99; the mock tracking and hidden desktop mean this is not compositor
or wearer evidence.

The call-tree profile counted `decals on the world` 0.24 times per measured
frame and averaged 0.043 ms of CPU per frame across that scope. Its main
subscopes averaged 0.002 ms for grid sizing, 0.008 ms for counting, 0.007 ms
for the prefix pass, 0.006 ms for filling, and 0.014 ms for buffer upload.
These measurements put a ceiling on the likely value of sizing a smaller table
from occupied buckets: even eliminating sizing and the prefix pass entirely
would save under 0.01 ms per frame in this fixture, before accounting for any
extra counting or rehashing. A smaller table also increases the risk that its
64-marks-per-bucket cap drops visible decals. I did not make that trade-off.
The raw Frame JSON, profile and log are retained privately in
`performance-results/decal-grid-frame-20261012`.

## Particle instance CPU work

The A520 cycle profile identified `qvr::particles::buildInstances` as a CPU
hotspot. I tested a native ARM64 candidate that read and clamped
`vr_soft_particles_scale` once per instance-build instead of repeating that
invariant lookup for every particle. The per-particle fade and pull arithmetic
was unchanged. The candidate was built and run on Steam Frame separately; the
released binary and normal installation were not changed.

An A-B-B-A comparison used the same `particles_dense` fixture, render scale
0.25, particle multiplier 1, 1728-pixel mock eye images, 72 Hz pacing, fixed
seeds and about 15,546 live VR particles. A scoped profile reported
`particle verts` CPU averages of 0.448 and 0.431 ms for the release build, and
0.413 and 0.434 ms for the candidate: 0.440 ms versus 0.424 ms averaged, but
the individual pairs crossed and the difference is within run-to-run variation.
A separate unprofiled pass measured 0.985 ms versus 0.938 ms in the enclosing
`vr particles` phase.

The dense fixture's unprofiled frame averages were 28.324 ms baseline and
28.332 ms candidate; GPU-eye averages were 27.662 and 27.634 ms, and frame p99
was 34.196 and 34.323 ms. These results do not establish a repeatable benefit
at either the scoped or whole-frame level, so the source change was reverted.
A later eye-image equivalence attempt did not finish its capture; pixel
identity was not verified. The optional profiles, defaults and release
launcher remain unchanged. This mock testing does not validate physical
OpenXR tracking, compositor timing or wearer experience.

## Fresh OpenXR render-scale A-B-B-A on Frame

To measure real OpenXR submissions without a wearer, I used a separate scratch
install of the released engine (SHA-256 `386e4ba6...`) and set
`vr_xr_unfocused 0` plus `vr_xr_unfocused_pause 0` only in its temporary test
profile. SteamVR still reported no user presence, so the head and hand actions
were invalid; the overrides only allowed the renderer to submit fresh frames.
The OpenXR diagnostics confirmed `projection=1 stereo_views=2 quad=0 held=0`
during each 720-frame run. The normal installation and its settings were not
changed.

The stable `idle_e1m1` fixture ran on the Frame's Adreno 750 with
Zink/Turnip through the SteamVR/OpenXR 2.18.2 runtime at 72 Hz with 1728x1728
eye images, 180 warm-up frames and 720 measured frames. The A-B-B-A order
compared render scale 0.45, 0.25, 0.25 and 0.45. Every run had 207 edicts, 23
living monsters, 130 Box3D bodies and the same VR-particle start/max/end counts
(335/367/314; average 321.1). The average awake-body count varied slightly
between 12.2 and 12.3.

| Render scale | GPU-eye mean | GPU-eye p99 | Frame p99 |
|---:|---:|---:|---:|
| 0.45 | 9.085 ms | 10.798 ms | 13.909 ms |
| 0.25 | 4.143 ms | 5.180 ms | 13.898 ms |

This stable idle-scene comparison cut GPU-eye mean by 54% and p99 by 52%, while
both frame distributions remained paced at about 13.89 ms. It confirms that the
existing reduced-scale profile lowers rendering work through the actual
OpenXR/GLX/Turnip path. It does not establish a combat or campaign gain: the
idle scene is underloaded, and 0.25 renders 432x432 pixels per eye before
upscaling rather than the 778x778 pixels at 0.45.

A separate `combat_48` OpenXR attempt was not accepted as an A-B comparison even
after setting `sv_random_seed 7`: starting monster, physics-body and live-
particle counts diverged substantially between repetitions. The raw idle runs
are retained privately in
`performance-results/openxr-idle-abba-frame-20261012/openxr-idle-abba`, and the
combat attempts in
`performance-results/openxr-idle-abba-frame-20261012/openxr-combat-abba-seeded`;
the non-deterministic combat timings are excluded from the reported gain. No
engine code, defaults or launcher changed in this probe.

## OpenXR check of the 0.18 render-scale candidate

I also ran an A-B-B-A `particles_dense` fixture at 0.25 on the released engine
(SHA-256 `386e4ba6...`) and 0.18 on the experimental render-scale-floor build
(SHA-256 `48ce8499...`). The candidate changes the minimum accepted render
scale from 0.25 to 0.18; its preview profile keeps retro edge softness enabled.
Both profiles used `vr_particle_mult 0.5`, the same 1728x1728 OpenXR eye images,
72 Hz pacing, fixed seeds, 180 warm-up frames and 720 measured frames on the
Frame's Adreno 750 with Zink/Turnip and SteamVR/OpenXR 2.18.2.

| Build / render scale | GPU-eye mean | GPU-eye p99 | Frame p99 | VR-particle GPU pass |
|---|---:|---:|---:|---:|
| Released engine / 0.25 | 11.591 ms | 13.068 ms | 15.857 ms | 2.830 ms |
| Floor candidate / 0.18 | 8.104 ms | 9.677 ms | 15.443 ms | 1.801 ms |

The candidate reduced GPU-eye mean by 30.1% and p99 by 25.9% in this fixture.
All four 720-frame runs submitted fresh two-eye projection layers (`held=0`)
and produced 713 GPU-eye samples each. Edicts, living monsters, Box3D body and
contact counts, decals, dynamic lights and live-particle counts matched exactly
between the two repeats of each setting. Runtime timing-log samples reported
72 rendered frames and zero display-period misses during their sampled seconds,
but the engine's frame p99 remained above 13.89 ms; these samples do not
establish sustained wearer performance. The user-presence sensor reported
absent, and head and hand action states remained invalid; the unfocused-render
overrides were confined to the scratch config. Scale 0.18 renders about
311x311 pixels per eye before upscaling, versus 432x432 at 0.25, so the gain
has a severe image quality cost. This verifies the experimental build's OpenXR
startup and GPU behavior; it does not validate wearer acceptance or tracking.
After the timing windows, `vr_eyeshot 3` saved separate 1728x1728 left- and
right-eye images at both scales. All four contain the scene, and each left/right
pair is pixel-distinct; inspection shows the 0.18 image's scene and weapon
details are visibly softer. These renderer captures verify stereo eye content,
not how sharp or comfortable it looks in the headset. The images are retained
privately in `performance-results/openxr-ultra-eye-captures-frame-20261012`;
raw JSON, runtime diagnostics and logs are in
`performance-results/openxr-ultra-particles-abba-frame-20261012`.

## OpenXR particle multiplier A-B-B-A for the Ultra profile

To tune the optional 0.18 Ultra profile, I compared `vr_particle_mult 1` and
`0.5` on the same candidate binary, map, render scale, retro-softness settings,
and `particles_dense` fixture. Runs used 1728x1728 eye images, 72 Hz pacing,
fixed seeds, 180 warm-up frames and 720 measured frames, in order 1, 0.5, 0.5,
1. Every run submitted fresh two-eye projection layers (`held=0`) and yielded
713 or 714 GPU samples. The Frame reported no wearer presence; head and hand
actions were invalid, so this isolates rendering and simulation costs rather
than validating headset interaction.

| Particle multiplier | GPU-eye mean | GPU-eye p99 | Frame p99 | VR-particle GPU phase | VR-particle CPU phase | Average live VR particles |
|---:|---:|---:|---:|---:|---:|---:|
| 1.0 | 9.403 ms | 10.848 ms | 17.718 ms | 3.046 ms | 0.893 ms | 15,546 |
| 0.5 | 8.190 ms | 9.809 ms | 16.694 ms | 1.822 ms | 0.395 ms | 6,496 |

At half density, GPU-eye mean was 12.9% lower and p99 9.6% lower; the VR-
particle CPU phase was 55.8% lower. Average live particles fell by 58.2%, and
the paired eye captures visibly show fewer effects. The frame p99 remained
above the 13.89 ms 72 Hz budget for both settings, so this does not demonstrate
sustained 72 fps. Based on this measured tradeoff, the separate Ultra preview
sets `vr_particle_mult 0.5`; the regular install and the .25 preview are
unchanged. Wearer acceptance and representative combat performance still need
validation. Raw runs and left/right eye captures are retained privately in
`performance-results/openxr-ultra-particlemult-abba-frame-20261012`.

I also checked whether `vr_particle_mult 0.75` is a better quality/performance
compromise for the .18 Ultra profile, using the same candidate binary, fixture,
eye resolution, render scale and A-B-B-A controls (`0.5, 0.75, 0.75, 0.5`).
All four 720-frame runs submitted fresh stereo projection layers and yielded
713 GPU samples. Average edicts, living monsters, Box3D bodies and contacts,
decals and dynamic lights matched across the settings.

| Particle multiplier | GPU-eye mean | GPU-eye p99 | Frame p99 | VR-particle CPU phase | VR-particle GPU phase | Average live particles |
|---:|---:|---:|---:|---:|---:|---:|
| 0.5 | 8.252 ms | 9.818 ms | 15.520 ms | 0.357 ms | 1.866 ms | 6,496 |
| 0.75 | 8.740 ms | 10.292 ms | 17.615 ms | 0.530 ms | 2.371 ms | 9,621 |

The 0.75 setting retained 48% more live particles, at the cost of 5.9% higher
GPU-eye mean, 4.8% higher GPU-eye p99, 48.5% higher VR-particle CPU time and
about 2.10 ms higher frame p99. Both frame-p99 means exceeded the 13.89 ms
72 Hz budget. The optional Ultra profile therefore stays at 0.5, where it
offers the lower GPU/CPU cost; this still needs a wearer to judge whether the
particle loss is acceptable. This no-wearer run used invalid head and hand
actions and establishes no tracking or campaign performance. Raw runs and
paired renderer captures are retained privately in
`performance-results/openxr-ultra-particlemult-05-vs-075-abba-frame-20261012`.

## Installed Ultra-profile launch smoke test on Steam Frame

After copying the profile, I launched the isolated `.18` installation on the
Frame through its native OpenGL/GLX binary and SteamVR/OpenXR 2.18.2, with
`-noconfigwrite` and the no-wearer render overrides. The in-game console read
`vr_render_scale 0.18` and `vr_particle_mult 0.5`; the OpenXR log reported
1728x1728 eye swapchains, a stage-space projection layer with two views, and
the enabled `XR_VALVE_frame_controller_interaction` extension. While the
process was alive, SteamVR reported the scene key
`system.generated.openxr.quake vr.ironwail`, state `Running`, and matching
scene/compositor-focus PIDs. This confirms the installed Ultra launch path
starts as an OpenXR scene app and loads the intended profile values.

A second diagnostic launch added `-qvr-frame-diagnostics` and loaded E1M1.
After the startup panel's initial quad, the installed build repeatedly logged
`FRAME_LAYERS projection=1 stereo_views=2 quad=0 total=1 held=0`, confirming
fresh stereo projection submission. It selected the Frame controller profile
`/interaction_profiles/valve/frame_controller_valve`, but both hand pose
actions were inactive and invalid. Together with disconnected controllers,
this confirms the runtime/profile path is recognized without proving usable
controller tracking or input.

There was no wearer: the user-presence sensor reported absent, the HMD pose was
invalid, and both controllers were disconnected. The session log showed
unpaced periods around 6.2 ms (about 160 frames/s) under the unfocused-render
override. This is not representative compositor pacing or a 72 Hz result and
is excluded from the performance comparisons. The launch used
`-noconfigwrite`; no game config was created or changed. Console/OpenXR logs are
retained privately in
`performance-results/ultra-profile-launch-smoke-frame-20261012`.

## Depth-prepass removal diagnostic

To check whether the opaque world/brush depth prepass was wasting work at the
Ultra profile's 0.18 scale, I built a separate ARM64 candidate that disabled
only the `BP_SOLID` depth-only pass. I ran an A-B-B-A comparison on the Frame:
the existing build, the diagnostic candidate twice, then the existing build
again. All runs used the same `particles_dense`/`vrfiringrange` fixture,
1728x1728 OpenXR eye targets, fixed 72 Hz pacing, render scale 0.18,
`vr_particle_mult 0.5`, 180 warm-up frames and 720 measured frames. Counts for
edicts, monsters, physics bodies, live particles, decals and dynamic lights
matched across all four runs.

| Build | GPU-eye mean | GPU-eye p99 | Frame p99 | World + brush GPU phase |
|---|---:|---:|---:|---:|
| Existing depth prepass (runs 1 and 4) | 8.186 ms | 9.782 ms | 16.375 ms | 2.744 ms |
| Depth prepass disabled (runs 2 and 3) | 17.097 ms | 19.256 ms | 21.645 ms | 8.773 ms |

The no-prepass candidate was about 109% slower in GPU-eye mean and 220% slower
in the world/brush phase, despite reducing draw calls from about 452 to 356.
The order-reversed A-B-B-A controls reproduced the same result, so disabling
this pass is not a performance improvement on this Frame workload. Every run
produced fresh OpenXR projection layers with two stereo views and `held=0`
(15–18 diagnostic samples per run); the device had no wearer, so this tests
rendering only. Renderer eye captures show the same scene geometry, but are not
pixel-identical across runs because of animated effects, capture timing and
slightly different eye poses. Visual inspection found no obvious geometry or
depth-order fault, but these captures do not establish exact visual
equivalence.

The candidate existed only in a separate build directory. The Frame source was
restored and its `r_world.c` hash matched the original; the baseline binary and
normal installation were not changed. The baseline and candidate binary SHA-256
hashes were respectively `48ce8499ba5d14ef6e36b48e0412ae512b1fa8b6db5e631750f56985c49d25db`
and `6df4e864f452ea1c9bca1fa8c8077590dfe95548c77cb01ad03746a258f22af5`;
the restored source hash is `f76147469656f30416f23d509a8b5055f4d8c07a0111282162cb84b5724f1c13`.
Raw JSON, OpenXR diagnostics, logs and eye captures are retained privately in
`performance-results/depth-prepass-abba-20261012`. Since baseline GPU p99 was
below the 13.89 ms 72 Hz budget while frame p99 remained above it, the next
no-wearer diagnosis should correlate frame-time tails with CPU phases, XR
wait/submit and runtime missed-period counters before another GPU code change.

## Frame-tail CPU profile and hidden-window swap diagnostic

I captured per-frame hitch breakdowns on the Frame with `vr_profile_csv 1` and
`vr_profile_hitch 1.0`, restricted to the same 720-frame `particles_dense`
window. The build, render scale 0.18, particle multiplier 0.5, map, eye size,
seeds and fixture matched the preceding baseline. The benchmark recorded
GPU-eye mean 8.13 ms (p99 9.54 ms), frame p99 16.11 ms and CPU-busy p99
12.13 ms. The detailed profiler adds collection overhead, so I used it to
locate phases, not as a replacement for the unprofiled A-B-B-A timings.

The hitch CSV recorded 34 frames whose CPU host time exceeded the 13.89 ms
display budget. Thirty-three regular hitches had hidden desktop-window swap of
4.59 ms mean and 12.33 ms maximum; `xrWaitFrame` stayed below 0.31 ms and
swapchain waits below 0.08 ms. `xrEndFrame` averaged 0.65 ms and peaked at
5.84 ms. Box3D and per-eye view setup also appeared among the larger CPU
scopes. One separate 254.39 ms outlier was dominated by `render_other_ms`
(241.176 ms; eye scopes 136.7/104.3 ms), while swap was 0.765 ms and XR
wait/acquire/end were 0.188/0.080/0.327 ms. Its cause is unknown; this was a
no-wearer capture with profiling enabled, so retain it as an observed maximum
but do not treat it as representative or evidence of a swap/pacing fault.
These phase values come from individual slow frames. The breakdown's CPU
scopes add up to the reported period, but time charged to a wait may reflect
upstream GPU or queue work. It does not prove that window swap alone caused
each late frame. Raw profile and hitch CSVs are retained privately in
`performance-results/frame-tail-hitches-20261012`.

A fresh on-device engine regression pass ran against the release executable at
scale 0.25 and the experimental 0.18 executable using mock tracking. The hull
cache check passed on both. A 200-body, 300-step physics test produced the same
deterministic hash (`92faaba3aff7d472`) at 1, 2, 3, 4, 6, and 8 workers for
both binaries. This validates deterministic engine behavior, not controller
delivery, compositor pacing, or wearer performance. The logs are retained on
the Frame in `performance-results/decal-grid-frame-20261012/core-regression-20261012`.

I also repeated the no-wearer OpenXR probe in the scratch install and held it
open for 2,400 scripted frames. `frame-vr-state.py` observed the native engine
as `EVRSceneApplicationState_Running`, with its PID matching compositor scene
focus and key `system.generated.openxr.quake vr.ironwail`. The engine logged 33
fresh `projection=1 stereo_views=2 quad=0 held=0` layer records, plus one
startup quad. SteamVR selected the Frame controller interaction profile for
both hands, while both pose actions were inactive and invalid because the
controllers were disconnected and the HMD pose was invalid. This confirms
OpenXR scene registration and stereo-layer submission on the Frame; it cannot
confirm head tracking, controller input, or wearer comfort. The probe config,
live runtime-state snapshot, engine log and OpenXR diagnostics are retained on
the Frame in `performance-results/openxr-unworn-live-20261012`.

To test whether hidden-window swapping itself could be removed, I made a
temporary build that skipped `SDL_GL_SwapWindow` only while OpenXR was active
and `vr_mirror 0`. An unprofiled A-B-B-A compared the baseline binary
(`48ce8499...`) with that diagnostic (`b6a04f40...`) on the same fixture. All
four runs had 720 measured frames, 713–714 GPU samples, matching scene counts,
and fresh two-eye OpenXR layers. The results below average the two runs of each
variant.

| Variant | GPU-eye mean | GPU-eye p99 | Frame mean | Frame p99 | CPU `swap` | CPU `xr submit` |
|---|---:|---:|---:|---:|---:|---:|
| Baseline | 8.138 ms | 9.575 ms | 13.927 ms | 15.461 ms | 3.597 ms | 0.695 ms |
| Skip hidden desktop swap | 11.439 ms | 13.019 ms | 14.428 ms | 17.135 ms | 0.346 ms | 6.961 ms |

Skipping the desktop swap reduced that scope by 3.25 ms on average, but the
wait moved into `xrEndFrame`/`xr submit`; GPU-eye mean rose 40.6% and frame p99
rose 10.8%. The repeated B runs agreed, so this candidate is rejected and the
swap remains in the code. Both builds still produced stereo renderer captures
of the same scene. No one was wearing the Frame: SteamVR classified the
session as timed out and its submitted presents as dropped, so compositor
presentation and wearer pacing remain unverified. The candidate was isolated;
`gl_vidsdl.c` was restored to the worktree hash and the baseline binary and
normal installation were unchanged. Captures and logs are retained privately
in `performance-results/window-swap-abba-20261012`.

The measurements narrow the frame-tail work to pacing/back-pressure around
window swap, XR submit, and CPU view setup/Box3D. Another GPU-only edit is not
the next step. A useful follow-up should change one of those CPU/pacing paths
while preserving GL command submission, then repeat the no-wearer A-B-B-A and
reserve wearer validation for headset/compositor behavior.

## GL flush versus window swap diagnostic

I tested an isolated diagnostic to replace the hidden desktop-window swap with
`glFlush` while OpenXR was active and mirroring was disabled. I built it from a
separate source copy (`gl_vidsdl.c` SHA-256
`b2e8c3fbcb80aadf4948f27c3f98d14eb00b45826525a054cb43b14590cdfe5a`; candidate
binary SHA-256
`ecc78b39535911d32bd07955712133d8b34f14be2523adc98410468e2359eeef`) and ran it
only in the scratch benchmark clone. The unchanged baseline control completed
720 frames with 713 GPU samples and 15
fresh stereo OpenXR layer submissions (GPU-eye mean 8.126 ms, p99 9.577 ms;
frame p99 16.437 ms).

The first candidate run did not complete: the engine exited with SIGSEGV
(`-11`), and Mesa Zink reported `DEVICE LOST` / `VK_ERROR_DEVICE_LOST`. The
Frame kernel log at 16:01:54 UTC independently records an Adreno GPU lockup,
names the scratch `ironwail` process as the offending task, and reports a
successful GPU recovery. Because the candidate run failed, it has no valid
performance result and the planned A-B-B-A was aborted. The failure does not
identify whether the application or driver fault came first, but is sufficient
to reject this presentation-path change. Do not retry it or sweep periodic
swaps without root-cause evidence.

After kernel recovery, I reran the unchanged baseline in the same scratch
clone. It completed all 720 frames with 713 GPU samples and 14 fresh stereo
OpenXR layers (GPU-eye mean 7.984 ms, p99 9.406 ms; frame p99 16.516 ms), with
no second GPU reset. The Frame source still has its original swap path
(`gl_vidsdl.c` SHA-256
`1ff14a19573c19f0662b26ae8b1738f198c7bb90f8338250374bb314293cbdac`), and the
baseline executable hash remains
`48ce8499ba5d14ef6e36b48e0412ae512b1fa8b6db5e631750f56985c49d25db`; the
installed game was not involved. Failure and recovery logs, JSON timings and
OpenXR traces are retained privately in
`performance-results/glflush-abba-20261012`.

These runs confirm engine-side rendering resumed after GPU recovery. A
subsequent read-only status check, when no benchmark was running, showed no
wearer: the HMD pose was invalid, both controllers were disconnected, and
SteamVR had no active scene application. The runtime had timed out or dropped
the benchmark's earlier submissions. The runs therefore say nothing about
compositor pacing or worn 72 Hz behavior.

## ThinLTO CPU-only diagnostic

I built a separate ARM64 candidate from the unchanged source with Clang 19,
Release `-O3 -flto=thin`, and `ld.lld` (candidate SHA-256
`3825020074a6492d78c25b469d0b1359f58d2a7e415b71c50b3a90a068f35762`). I
compared it with the installed-source baseline build (`48ce8499...`) in an
A-B-B-A sequence on the Frame, using the mock headset's `vr_mock_fast 2`
headless path. Each run loaded `vrfiringrange`, spawned 500 mixed physics
props, settled them, then measured 899 fixed simulation frames. All four runs
started with the same 634 Box3D bodies and matching entity, awake-body,
contact, and particle counts. GPU and OpenXR phases were zero by design, so
this isolates engine CPU work and does not measure rendering or headset
pacing.

| Build | Frame CPU mean | CPU-busy mean | Frame p99 | Server phase mean |
|---|---:|---:|---:|---:|
| Baseline (runs 1 and 4) | 1.202 ms | 1.173 ms | 4.465 ms | 0.638 ms |
| ThinLTO (runs 2 and 3) | 1.301 ms | 1.269 ms | 5.612 ms | 0.668 ms |

The ThinLTO candidate was about 8% slower in mean frame CPU and CPU-busy time
in this short mock stress test. Its p99 was also higher, though individual
tail frames varied. There is no measured gain to justify carrying this build
change, so the candidate is rejected and remains separate from the installed
game and published source. This is a CPU-only diagnostic, not a conclusive
claim that ThinLTO regresses normal VR play. Raw logs and JSON are retained
privately in `performance-results/thinlto-headless-abba-20261012`.

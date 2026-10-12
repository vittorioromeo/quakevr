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

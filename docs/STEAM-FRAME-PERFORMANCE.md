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

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

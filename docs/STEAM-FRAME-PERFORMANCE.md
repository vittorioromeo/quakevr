# Steam Frame performance measurements

Measurements were taken on Steam Frame using the released native ARM64 engine
and its OpenGL renderer on Mesa Zink/Turnip. The engine rendered 1728x1728 eye
images in stereo; tracking was mocked, `vr_mock_fast` was off, and GPU timestamp
queries confirmed both eyes were drawn. These are renderer timings from the
device, not OpenXR compositor latency or a physical headset FPS test.

Each row summarizes two 720-frame runs of the same `combat_48` fixture, paced at
72 Hz after 180 warm-up frames. Settings were run in opposite order to limit
warm-up and order bias. The profiles through scale 0.25 use the released engine
binary; the final row is a separate experimental candidate. All use classic
preset 0 and no MSAA or mirror.

| Profile | Render scale | Retro edge softness | GPU eye mean | Frame median | Frame p99 |
|---|---:|---|---:|---:|---:|
| Release baseline | 0.45 | Shipped | 30.138 ms | 31.942 ms | 46.310 ms |
| Crisp edges | 0.45 | Off in all 18 categories | 28.262 ms | 30.158 ms | 42.685 ms |
| Crisp performance preview | 0.30 | Off in all 18 categories | 15.537 ms | 16.349 ms | 23.487 ms |
| Crisp performance preview | 0.26 | Off in all 18 categories | 13.507 ms | 14.179 ms | 20.650 ms |
| Crisp performance preview | 0.25 | Off in all 18 categories | 12.559 ms | 13.888 ms | 20.765 ms |
| Direct palette-color LUT candidate | 0.45 | Shipped | 7.923 ms | 13.888 ms | 17.368 ms |

The direct palette-color LUT candidate is an uncommitted experimental engine
build (`d2428f0e15d307170eb4c5148f66bb7ac2abbd54f17b489c89d212a00fc7bdfc`).
It keeps the released 0.45 settings and precomputes the palette's RGB output
beside its existing 128³ index table, removing the dependent palette-buffer
read from retro fragment shaders. Two 720-frame combat runs measured 7.965 and
7.881 ms GPU-eye means, versus 30.216 and 29.918 ms for the release binary in
opposite-order runs. That is a 73.7% reduction in mean GPU-eye time for this
fixture. Frame medians were 13.888 ms in both candidate runs; p99 was 17.715
and 17.020 ms, compared with 46.340 and 45.250 ms on release. This remains a
mock-tracking renderer benchmark, not physical OpenXR frame pacing.

Both eyes in a `combat_48` scene capture were pixel-identical between release
and candidate at 1728x1728. The candidate adds an 8 MiB RGBA8UI 3D lookup table.
The shader compiled and ran on the Frame's Zink/Turnip driver. Real-headset
OpenXR, controller, weapon and physics regression checks are still required;
the candidate has not been installed into the preview shortcut or released.

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

The tested shader fast path for retro-light curves had no gain and was reverted.
An optional switch to disable GPU timestamp queries also failed to improve the
frame time. Dynamic-light and blob-shadow controls showed no useful gain.

The separate performance-preview launcher passed a native mock startup and
reported the expected 0.25 render scale and edge-softness cvars. Its separate
install does not overwrite the normal game or QuakeQuest. Physical validation of
OpenXR focus, head/controller tracking, and core weapon/physics interactions is
still required before distributing the preview as a new release. The original
v1.0.0-frame.1 release remains the known-good build.

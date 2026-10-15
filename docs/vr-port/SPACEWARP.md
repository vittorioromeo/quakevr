# Steam Frame application frame synthesis

An experimental opt-in `XR_EXT_frame_synthesis` path exists on `frame-native`.
It can submit per-eye depth and estimated dynamic **2D image motion** through the
Quake OpenGL renderer. SteamVR accepted frames whose app-side log says
`synthesis=1`, but synthesized compositor output and wearer image quality have
not been verified. It is disabled by default. It does not claim exact per-vertex
velocities, improved campaign image quality, a performance gain, or guaranteed
half-rate pacing.

**Current update (2026-10-14):** a distinct, explicit GL-staging-to-Vulkan
copy path now passes paired motion/depth pixel tests on Steam Frame. Quake's
OpenXR backend enables EXT, exports fresh per-eye motion/depth, and attaches
metadata. A same-build replay A-B-B-A measured the new batched export at about
2 ms average-frame overhead versus color-only, with app frame times still far
above 72 Hz. This establishes the submission path and bounds its cost; runtime
generated-frame cadence and wearer-visible quality remain unverified. See the
[batched export and replay report](../../../performance-results/spacewarp-batched-exports-20261014/RESULTS.md).
The reviewed follow-up fixed a deferred sRGB-state rejection and added batch
timing plus a read-only SteamVR compositor collector. A fresh 450-frame
headless A-B-B-A replay passed its 24-death workload and repeatedly submitted
fresh two-eye EXT data; the app measured 32.08 ms/frame OFF and 35.03 ms ON.
SteamVR had no focused scene or valid HMD pose, so these results still do not
show compositor-generated frames. See the [reviewed build and replay record](../../../performance-results/spacewarp-batch-state-review-20261014/RESULTS.md)
and the [compositor timing collector guide](../../Misc/frame/FRAME_COMPOSITOR_TIMINGS.md).
SteamVR also logged `Failed to signal gpu fence` once at every application
connection. A color-only native-OpenGL OpenXR control reproduced it with
SpaceWarp and Vulkan interop both off, so it is not isolated to the synthesis
bridge; its cause with a tracked headset remains to be checked.

**Deferred-wait experiment:** the default-off `vr_xr_vulkan_defer_wait 1`
now releases successfully submitted Vulkan exports before a CPU fence wait,
then waits before staging reuse. This is permitted by the Vulkan OpenXR
contract. With synthesis enabled throughout, two 450-frame A-B-B-A replays
measured idle mean frame time 25.94 to 22.40 ms and gore mean 33.45 to 27.38 ms.
Gore tail times improved, but idle p95 did not, and neither scene reliably
met a 27.78 ms budget. These unworn results show promising scheduling savings,
not compositor frame generation. The option remains off pending tracked-headset
validation. See [design, synchronization contract and test procedure](FRAME_SYNTHESIS_DEFERRED_GPU_WAIT.md)
and the [raw timing report](../../../performance-results/spacewarp-batch-state-review-20261014/RESULTS.md).

**450-frame gore ABBA (2026-10-14):** with the relaxed-interval hint held at 0
in all four OFF/ON/ON/OFF legs, the scripted workload validated identically
(25 kill events, 24 target deaths and 25 popped heads per leg). The ON legs
each logged 16 fresh stereo motion/depth submissions. Compared with OFF, ON
increased average GPU-eye time by 15.8% and average CPU-busy time by 28.1%;
frame p99 improved 5.2%, while GPU-eye p99 rose 14.2%. This confirms app-side
submission but shows measurable synthesis overhead in the combat workload.
SteamVR had no tracked HMD pose, so compositor use and wearer benefit remain
unverified. See the [full run and raw evidence](../../../performance-results/spacewarp-gore-appside-abba-20261014c/RESULTS.md).

A separate Vulkan OpenXR probe on the Frame runtime enumerated and created
signed-float motion and depth swapchains. A standalone Vulkan test now renders
a moving rectangle with analytically computed per-eye FP16 vectors and matching
depth, reads back GPU pixels to verify them, and submits 120 frames with
`XrFrameSynthesisInfoEXT`. This verifies the controlled Vulkan data/submission
path, not SteamVR's use of the vectors for generated display frames. Quake's
current renderer is still OpenGL. See the
[Vulkan probe record](../../../performance-results/spacewarp-vulkan-format-probe-20261013/RESULTS.md)
and the [reusable engine integration notes](FRAME_SYNTHESIS_ENGINE_GUIDE.md).

## Implemented path

Start the backend with `vr_xr_frame_synthesis 1` to request EXT. The choice is
latched at backend start. `vr_xr_frame_synthesis_scale` defaults to 0.25 of the
runtime's recommended synthesis width/height; `vr_xr_frame_synthesis_relaxed 1`
adds the optional relaxed-interval hint. A running backend created without EXT
cannot acquire the extension just by changing the cvar.

The backend chains a synthesis config to each view, enumerates the GL session's
formats, and creates distinct motion/depth swapchains for each eye. It supports
advertised signed floating-point formats and an experimental `GL_SRGB8_ALPHA8`
motion encoding using EXT scale/offset. The sRGB option is controlled by
`vr_xr_frame_synthesis_srgb_motion` and is off by default. A byte-level GPU
round-trip fixture passed, and SteamVR created the motion/depth swapchains and
accepted projection submissions with synthesis metadata. This validates the
app-to-runtime API path, not the compositor's interpretation or generated
frames. The experimental encoding is quantized and has a finite motion range;
out-of-range vectors must never be silently clamped. Depth uses an advertised
depth-component format. Swapchains use attachment and sampling usage, one sample
and one array layer.

`vr_frame_synthesis.cpp` captures actual scene/composite depth before the other
eye reuses the scene targets. It selects the source using `GL_NeedsSceneEffects`
and the real scaled viewport. MSAA resolves the nearest covered sample (maximum
for reversed Z, minimum otherwise). Each eye owns its depth, camera seed and
separate current/previous final-color pyramids.

The producer computes camera reprojection from depth and current/previous view
matrices, with sky direction handling. A three-level, two-iteration
Lucas-Kanade image-motion pass refines that seed from actual final-color images,
including independently moving objects. It stores current-to-previous UV flow
internally and exports `currentNDC-previousNDC` signed RG vectors. This is an
estimated optical-flow path, not camera-only or fabricated zero-vector support.

Depth and vectors are written into acquired runtime images, detached from GL
framebuffers and released before submission. Each projection view receives its
own `XrFrameSynthesisInfoEXT` with the 2D flag, correct subimages, identity signed
float decoding, depth range and metric clip distances. Reversed Z associates
depth 0 with the far clip and depth 1 with the actual eye near clip. Application
mapping removes tracked eye movement from the rendered world transform; its
incremental surface transform is expressed in current application coordinates.
Composition structs live through `xrEndFrame`; cached held views never retain
pointers to those temporary chains.

The math fixture now exercises both eyes across independent head movement and a
one-metre app-applied move, verifying that tracking cancels and both eyes derive
the same metric app-space delta. The [Khronos contract](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html)
requires an identity delta when the app did not transform its projection-layer
space and defines the position relative to the current app space.

## Gating and fallback

Ordinary color submission remains the default. An unavailable extension, failed
optional instance/config negotiation, unsuitable formats, allocation failure,
image acquire/wait/release failure, incomplete GL target or shader/GL error
prevents synthesis submission. Optional instance negotiation retries without EXT.
A failed synthesis wait is not followed by an illegal image release; synthesis
is disabled until backend restart and the owned swapchain is destroyed at stop.

Both eyes must be ready for the same frame. Color history and matrices commit
only after successful fresh stereo submission. Empty/held/unfocused frames,
failed submission, map changes, render-size changes, display-time discontinuity,
teleports, substantial snap turns and calibration/scale changes reset or warm up
history. A warm-up frame submits color only. Underwater/screen water warps and
portal views currently opt out because their color rays do not match ordinary
scene depth. Turning the cvar off stops synthesis submission immediately.

One scene-content mismatch remains: Quake captures depth after `V_RenderView`,
then adds the HUD/menu panel, laser lines and text overlays to the final eye
color in `stereo::drawUi`. That pass is deliberately drawn over the image with
depth testing and depth writes disabled. Optical flow therefore sees the final
overlay pixels, while the paired depth still describes the world behind them.
The renderer contract calls out this UI/depth issue for other engines too. A
correct fix needs an overlay motion/depth policy (or a separately composed
OpenXR layer); simply copying world depth under the HUD is not valid. It remains
an explicit headset image-quality gate before treating this path as ship-ready.

Two additional safety gates now cover renderer edge cases. `r_norefresh` skips
depth capture because `R_RenderView` leaves the old depth target untouched in
that mode; stereo history then warms up again after normal rendering resumes.
Before submission, the producer compares both eyes' app-space delta poses. If a
camera blend makes the eye mappings non-rigid (the death-view fractional-yaw
case found in review), it rejects synthesis for both eyes and submits color
only. A CPU fixture accepts equivalent stereo deltas and rejects a 18.7 mm
left/right divergence. These gates prevent stale or contradictory metadata;
they do not replace wearer testing.

`FRAME_LAYERS` now reports `synthesis=0/1`. Startup logs distinguish the request,
extension enablement, and usable motion/depth swapchain formats.

### One-frame input capture

Set the non-archived `vr_xr_frame_synthesis_capture 1` cvar while synthesis is
running. On the next valid history frame, the diagnostic writes one stereo pair
under `profile/frame_synthesis_capture/`: previous/current color inputs, scene
depth, camera seed, estimated flow, and the encoded motion/depth images passed
to the backend, with matrices, clip distances, app-space delta, formats and
dimensions in JSON sidecars. The capture uses synchronous GL readback and marks
itself `performance_valid=false`; never use that frame for timing comparisons.
It only captures producer-side inputs and outputs. It does not prove SteamVR
received or used the images. The metadata explicitly records the current issue:
the submitted color contains the UI while its depth comes from the scene before
the UI was drawn.

The seed target is cleared to finite zero motion before the reprojection pass so
uncovered pixels have defined fallback values. Without this clear, two test
captures contained NaNs in the half-float seed image; the post-clear capture
validator confirmed finite values in every channel for both eyes. Raw float
files are declared as little-endian IEEE754 binary32. The successful opt-in
Frame capture is documented in
[`spacewarp-input-capture-final-20261014`](../../../performance-results/spacewarp-input-capture-final-20261014/RESULTS.md).
The guarded wearer precheck with that rebuilt binary is in
[`spacewarp-wearer-precheck-final-20261014`](../../../performance-results/spacewarp-wearer-precheck-final-20261014/RESULTS.md); it stopped before changing settings because the HMD had no valid pose.

## Actual Frame evidence and unresolved runtime gate

The earlier session identifies SteamVR/OpenXR 2.18.2 and advertises both FB
revision 2 and EXT revision 1. Its OpenGL session lists `GL_SRGB8`,
`GL_SRGB8_ALPHA8`, and depth-component 16/24/32F formats, but no signed linear
motion format. A separate Vulkan probe did enumerate/create signed FP16 motion
and D32 depth swapchains. The Vulkan frame-loop fixture has since submitted
120 frames with those formats and verified known per-eye signed motion, depth,
and color through GPU readback. This proves the controlled data path, but not
runtime use of the fields for frame generation.

An instance/system-only probe enabled EXT with OpenGL and queried 1728x1728
color and synthesis recommendations for both eyes. The GL runtime did not
advertise a signed float motion format. A later opt-in sRGB run created both
eyes' motion/depth swapchains and submitted fresh frames with
`XrFrameSynthesisInfoEXT` (`synthesis=1` in the application's `FRAME_LAYERS`
log). This demonstrates that the app's GL encoding, swapchain lifecycle and
projection-layer chain reached `xrEndFrame` without a logged failure. It does
not prove SteamVR actually synthesized or displayed intermediate frames.

The controlled sRGB8 fixture decoded a known 64-pixel test motion with about
0.004 pixel X and 0.001 pixel Y error. The larger risk was runtime performance:
the first range-safety implementation reduced every eye's flow image and
called synchronous `glReadPixels` before allowing submission. A focused profile
measured 9.48 ms left-eye and 8.03 ms right-eye CPU wait in those calls. The
flow solver itself measured under 0.05 ms CPU and 0.6--0.7 ms GPU per eye.
See the [profile capture](../../../performance-results/spacewarp-gore-detail-20261014/RESULTS.md).

The readback gate is now removed. The export shader checks each vector before
encoding it. A finite, representable optical-flow vector is preserved; an
invalid or out-of-range vector uses camera reprojection when that vector is
finite and in range, otherwise it uses neutral zero motion. It never clamps a
bad vector to the sRGB8 endpoint. Alpha is zero on fallback pixels when the
runtime image format has alpha; EXT ignores that channel, so this is only a
diagnostic marker. The production GLSL fixture covers valid flow, out-of-range
flow, NaN/Inf, safe camera fallback, and neutral fallback on Frame's Zink/Turnip.

A two-iteration A/B/A/B frozen-world render capture measured median 13.8904
ms/frame with synthesis on and 13.8903 ms off; median CPU busy was 3.579 versus
2.206 ms, and median GPU eye time 8.585 versus 7.359 ms. The named gore replay
did not execute: `vr_xr_unfocused_pause 1` held game time at 1.300 seconds, all
25 monsters remained alive, and particle/decal/kill-event counts stayed zero.
These timings compare the render path and producer overhead only, not combat
performance. The app stayed at the scripted 72 Hz cadence with an estimated
1.226 ms extra GPU-eye time. The A/B run logged synthesis metadata on 8 of 11
periodic `FRAME_LAYERS` entries while enabled, and zero when disabled.
See the [two-iteration A/B report](../../../performance-results/spacewarp-flow-2iter-abba-20261014/RESULTS.md)
and the [detailed scope capture](../../../performance-results/spacewarp-flow-2iter-detail-20261014/spacewarp-flow-2iter-detail-20261014.json).

The freeze was corrected for the subsequent replay: `vr_xr_unfocused_pause 0`
is forced after persistent settings, and the run temporarily stages the rebuilt
QuakeC bytecode that emits kill markers. Four 450-frame ON/OFF/ON/OFF captures
now pass workload assertions: all record 25 deaths in the scripted window and
the monster count falls 25 to 1. The median enabled-minus-disabled deltas were
about +1.44 ms frame time, +1.39 ms CPU busy time and +0.78 ms GPU eye time.
These are two short samples per setting with no HMD present, so they are app-side
cost estimates and not proof of headset benefit. The [validated replay report](../../../performance-results/spacewarp-gore-real-abba-20261014/RESULTS.md)
contains the raw runs and limits; the [detailed death-event profile](../../../performance-results/spacewarp-gore-real-detail-20261014/RESULTS.md)
finds the largest CPU spike in QuakeC/physics with additional decal and particle
preparation cost.

This remains app-to-runtime submission evidence. HMD presence was absent and
`FRAME_DIAG` had no valid tracked head pose. SteamVR's in-headset motion/depth
textures, generated intermediate frames, and wearer image quality are still
unverified. See the [validated gore replay report](../../../performance-results/spacewarp-gore-real-abba-20261014/RESULTS.md),
[validated gore replay detail](../../../performance-results/spacewarp-gore-real-detail-20261014/RESULTS.md),
[earlier frozen-world A/B report](../../../performance-results/spacewarp-gore-safe-abba-20261014/RESULTS.md),
[detailed safe-path profile](../../../performance-results/spacewarp-gore-safe-detail-20261014/RESULTS.md),
[profile diagnosis](../../../performance-results/spacewarp-gore-detail-20261014/RESULTS.md),
[finite-guard capture](../../../performance-results/spacewarp-flow-finite-guard-20261014/RESULTS.md),
and [first scripted replay](../../../performance-results/spacewarp-gore-replay-20261014/RESULTS.md).
Reusable cross-engine notes start at the [porting index](FRAME_SYNTHESIS_PORTING_INDEX.md),
followed by the [engine integration guide](FRAME_SYNTHESIS_ENGINE_GUIDE.md),
[engine adapter template](FRAME_SYNTHESIS_ENGINE_ADAPTER_TEMPLATE.md),
[EXT/FB comparison](FRAME_SYNTHESIS_EXT_FB_COMPARISON.md), and
[porting validation matrix](FRAME_SYNTHESIS_PORTING_VALIDATION_MATRIX.md). The
[SteamVR headset validation procedure](FRAME_SYNTHESIS_HEADSET_VALIDATION.md)
lists the exact texture-display and performance-recording signals still needed.
The relaxed-interval request bit is now reflected in `FRAME_LAYERS` and was
verified on/off at the application boundary; its runtime effect remains
unverified. See the [flag diagnostic](../../../performance-results/spacewarp-relaxed-diag-20261014/RESULTS.md).

The matching SteamVR compositor log for the 02:25 Quake run contained six
`device does not support depth stencil format` / `Failed to create new shared
image: format=43 dimensions=432x432` messages. Although those messages appeared
within the millisecond-resolution window of the app's motion-chain logs, a
follow-up isolated matrix now narrows the cause: a GL color swapchain and the
experimental sRGB motion swapchain (usage `0x21`) each enumerate three images
without these errors. A GL depth swapchain produces exactly three such errors
per three-image chain. This reproduces with D32F (usage `0x22`), D32F attachment
only (`0x2`), D24 (usage `0x22`), and D16 (usage `0x22`); the same D32F error
occurs when `XR_EXT_frame_synthesis` is not enabled. OpenXR still returns
successfully from `xrCreateSwapchain` and image enumeration, so API success
does not establish SteamVR's internal image sharing succeeded.

The native Vulkan control created and enumerated both-eye FP16 motion and D32F
depth swapchains (three images each) with no matching compositor error. Vulkan
reports runtime formats 43 (sRGB RGBA), 97 (FP16 RGBA), and 126 (D32F). The
string `format=43` in the compositor error therefore identifies its failed
internal shared-image format, not the application depth format. The strongest
current diagnosis is a SteamVR OpenGL depth-swapchain sharing failure, even
though the error text reports an sRGB internal image. That points to the GL
depth/interoperability path as the blocker; changing D16/D24/D32 or dropping
the sampled usage did not fix it. The Vulkan probe is only create/enumerate
evidence and does not prove compositor use or frame synthesis. Full commands
and fresh log deltas are in the [isolated probe report](SPACEWARP_GL_OPENXR_SWAPCHAIN_PROBE_2026-10-14.md).

The log's cumulative `883 presents, 0 dropped, 319 reprojected` from the
original Quake run does not attribute reprojection to EXT frame synthesis.
`FRAME_DIAG head=0` also needs careful reading: this logger gates `head` on the
headset-presence sensor, while raw `xrLocateViews` flags can still report valid
orientation/position while unworn. Neither those flags nor SteamVR's aggregate
reprojected count proves EXT-generated frames.

The earlier Zink investigation rejected **direct external-image aliasing**:
Zink created a different Vulkan image from the exported image and did not show
the ownership transitions needed for that alias route. That finding still
rules out treating the GL and Vulkan handles as the same image. It does not
rule out the separate staging-copy route now used by gameplay. The staging path
has since passed a paired motion/depth pixel fixture and a gameplay replay on
Steam Frame; its remaining cost and runtime consumption are recorded in the
[batched export report](../../../performance-results/spacewarp-batched-exports-20261014/RESULTS.md).
See the [Vulkan bridge roadmap](SPACEWARP_VULKAN_ROADMAP.md),
[cross-API interop investigation](FRAME_SYNTHESIS_CROSS_API_INTEROP.md),
[interop capability result](../../../performance-results/spacewarp-gl-vk-capabilities-20261014/RESULTS.md),
and [round-trip trace](../../../performance-results/gl-vulkan-interop-roundtrip-20261014/RESULTS.txt).

This was a runtime-gate test, not a wearer performance capture. SteamVR showed
the Dashboard, the HMD pose was invalid, and both controllers were disconnected.
The armed motion recorder saved an idle take with 4,344 held frames and no valid
wearer interaction; it is retained only as diagnostic evidence, not as a replay
fixture. The profile samples from this unfocused session are not representative
and must not be used for performance conclusions. Logs are in
`performance-results/spacewarp-live-test-20261013/`.

The installed native OpenXR 1.1.40 headers lack EXT declarations, so
`vr_openxr_synthesis_compat.hpp` provides the published revision-1 ABI when absent.
The repository's Windows headers already declare EXT. FB is not used; the current
Khronos registry deprecates it in favor of EXT.

## Completed verification

Native ARM64 Release `ironwail` compiled and linked with Clang 19.1.7 and `-j2`.
The test binary was launched separately from the installed game binary, with
the original user configuration restored after the session. Earlier profiler
changes were preserved; parallel kill-marker changes were not copied or built
into this test binary and still require an integration build.

CPU-only tests passed with `-O2 -Wall -Wextra -Werror`:

- Byte-equivalent eye projection arithmetic across 256 cases, conventional and
  reversed near/far depth, asymmetric stereo and cross-eye contamination.
- Static reconstruction, motion sign/encoding, submitted-display-time history,
  cancellation of tracked motion from application mapping, and metric transforms.
- The actual composition builder, initialized rects/decoding/depth/pose metadata
  and published 64-bit ABI against the native 1.1.40 headers.

A standalone **hidden desktop GL fixture**, with no Quake startup or OpenXR
instance/session, compiled and executed the actual production GLSL on Frame's
Zink/Turnip Adreno 750 renderer. It passed conventional/reversed depth capture,
4x MSAA nearest-sample resolve, sky/camera reprojection and real depth/vector
export. Independently moving textured foregrounds against stationary backgrounds
had mean flow errors of 0.0038 pixels for the left +4-pixel case, 0.0066 pixels
for the right -3-pixel case, and 0.0067 pixels for an independent +3-pixel Y
case in the controlled 64x64 fixture. Stationary-background error was zero.
These are synthetic correctness checks, not campaign benchmarks.

The complete ARM64 Release `ironwail` target was rebuilt from the current source
with CMake and Clang 19.1.7 after the stereo-delta and `r_norefresh` gates were
added. The portable Zancle decay trait is selected on Clang 19 and GCC 15 because
those compilers report `__decay` as available but reject its use in this function
signature. The rebuilt executable has not yet been launched in an HMD session.

**Controlled app-side motion replay (2026-10-14):** the headset runner now uses
`spacewarp_motion_fixture` by default. It spawns and flings one crate in
`vrcalibration`, then logs its final position. The smoke runner validates frame
count, displacement, absence of monsters/particles/decals, and repeated fresh
stereo submissions as separate app checks; HMD/controller tracking remains an
independent wearer gate. The integrated Frame run passed the app checks (105.3
units of displacement; eight successful synthesis submissions) while correctly
reporting no tracked HMD or controller. Its app timing and raw logs are in
[`spacewarp-motion-fixture-validation-20261014`](../../../performance-results/spacewarp-motion-fixture-validation-20261014/RESULTS.md).
The wrapper's collector now stops with the smoke process rather than collecting
for a fixed interval after scene exit. A current guarded precheck still exits
before touching game settings when HMD tracking is invalid.

An app-only OFF/ON/ON/OFF profile on this fixture found the synthesis-enabled
path added 15.6% to mean GPU-eye time and 31.0% to mean CPU-busy time, while
frame p99 was nearly unchanged. OFF retained Vulkan color-image transport, so
the delta reflects additional synthesis motion/depth work on top of the shared
interop path. These unworn measurements do not show runtime-generated frames
or a wearer benefit. See
[`spacewarp-motion-abba-20261014`](../../../performance-results/spacewarp-motion-abba-20261014/RESULTS.md).

**Recorded gore replay (2026-10-14):** a deterministic demo captures the
`gore_headpop_24` workload (25 head pops / 24 monster deaths) and runs the same
450-frame replay through OFF/ON/ON/OFF profiling. The repeated ON legs added
15.8% mean GPU-eye time and 28.1% CPU-busy time; this confirms a meaningful
producer cost under the death-effects workload. The replay was unworn with no
valid HMD pose, so it does not demonstrate runtime frame generation or visible
benefit. The [demo, four raw legs and comparison](../../../performance-results/spacewarp-gore-demo-abba-20261014/RESULTS.md)
are retained; `QVR_RECORD_DEMO` in the smoke runner records a fixed scenario and
`QVR_DEMO` replays it without requiring hand-recorded player input.

**Post-reboot wearer precheck (2026-10-14):** SteamVR recorded Quake VR at
72 Hz for 347 frames, with depth and motion vectors on 345 frames. The app
logged eight fresh stereo synthesis submissions and nine valid HMD-pose
samples. The wearer saw the motion-vector debug view; it changed but looked
mostly constant, and the scripted crate appeared to pass through walls. The
crate fixture is not a collision or visual-correctness test. SteamVR's
"Resume Game" dashboard remained visible, OpenXR never reached `FOCUSED`, and
neither controller action produced a valid pose sample. This is runtime
buffer-receipt evidence only, not a focused wearer or generated-frame pass.
The fixture profile averaged 22.6 ms/frame (p99 33.5 ms) against 72 Hz; it is
not a normal-game performance result. Raw evidence is in
[`spacewarp-wearer-precheck-post-reboot-20261014`](../../../performance-results/spacewarp-wearer-precheck-post-reboot-20261014/spacewarp-20261014-151453-on-precheck_1/).
The headset validator and wearer runner now require an OpenXR `FOCUSED` sample
and valid left/right controller poses for a headset validation pass.

**Focused wearer OFF/ON/ON/OFF (2026-10-15):** four 720-frame crate-fixture
legs ran at 72 Hz with the dashboard dismissed and tracked controllers. SteamVR
recorded depth and motion vectors on 884/886 frames in B1 and 885/887 in B2;
both ON legs passed the stricter focus/controller validator. Averaged over the
two ON legs versus the two OFF legs, the app's mean frame time rose 4.3%, CPU
busy time 29.8%, GPU-eye time 17.4%, and GPU 3D time 24.9%. Mean frame time was
22.12 ms, above the 13.89 ms 72 Hz budget. B2's compositor timing collector
detected a scene-focus change while Quake was alive, so that leg's compositor
timings are excluded; its app profile and SteamVR CSV remain attributed and
valid. The final OFF leg completed as a continuation after the wrapper stopped
at B2. The OpenVR collector did not prove EXT-generated frames, and
`reprojectedFrames` is not specific to frame synthesis. See the [full report and
raw legs](../../../performance-results/spacewarp-wearer-abba-user-ready-20261015/RESULTS.md).

From a native checkout, build the standalone tests with:

```sh
clang++ -std=c++23 -O2 -Wall -Wextra -Werror -I Quake/vr \
  -I Quake/vr/external Misc/quakevr/tests/vr_synthesis_math_test.cpp \
  -o build/vr_synthesis_math_test
build/vr_synthesis_math_test
clang++ -std=c++23 -O2 -Wall -Wextra -Werror -I Quake/vr \
  Misc/quakevr/tests/vr_synthesis_submission_test.cpp \
  -o build/vr_synthesis_submission_test
build/vr_synthesis_submission_test
clang++ -std=c++23 -O2 -Wall -Wextra -Werror -I Quake/vr \
  -I Quake/vr/external Misc/quakevr/tests/vr_synthesis_gl_test.cpp \
  -lSDL2 -lGL -o build/vr_synthesis_gl_test
DISPLAY=:0 SDL_VIDEODRIVER=x11 build/vr_synthesis_gl_test
```

Private raw evidence remains outside the source repository in
`performance-results/spacewarp-feasibility-20261013/`.

## Correctness limits and wearer validation

Optical flow estimates correspondence from image gradients. Textureless regions,
lighting changes, disocclusion, rapid motion, translucent compositing, view-dependent
reflections and overlay pixels can produce uncertain or incorrect correspondence.
The depth is actual opaque scene depth; it does not invent separate depth for
all blended particles/UI. Heat/bullet-time distortion can also mismatch those
rays. Exact geometry motion passes and explicit effect/overlay treatment remain
the higher-quality follow-up. The 2D flag is intentional and may reduce runtime
synthesis quality compared with correct 3D geometry vectors.

A future authorized wearer test must first confirm EXT-enabled GL formats and
successful vector/depth creation. Inspect both eyes with Valve's depth/vector
texture display, verify artificial locomotion and pose-delta direction, and check
close weapons, animated enemies, transparency/effects, UI, disocclusion,
teleports and focus recovery. Compare the same saved workload off/on and record
application GPU time/FPS, compositor time, submission counts and image quality.
The added depth/copy/flow passes have no measured full-game GPU budget yet.
Synthesis may improve displayed motion below refresh; it does not reduce world
rendering cost and can add enough work to outweigh its benefit. Runtime pacing
and synthesis are not guaranteed by the relaxed-interval hint.

## Primary references

- Khronos: [EXT specification](https://raw.githubusercontent.com/KhronosGroup/OpenXR-Docs/main/specification/sources/chapters/extensions/ext/ext_frame_synthesis.adoc),
  [composition structure](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html),
  [view recommendations](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisConfigViewEXT.html),
  [registry/deprecation metadata](https://raw.githubusercontent.com/KhronosGroup/OpenXR-Docs/main/specification/registry/xr.xml).
- Valve: [Steam Frame depth/motion-vector API and assessment](https://partner.steamgames.com/doc/steamhardware/steamframe/compat/perf_criteria),
  [debugging and recorded counts](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging).
- Reusable notes for other renderers: [OpenXR frame synthesis engine guide](FRAME_SYNTHESIS_ENGINE_GUIDE.md)
  and [engine porting playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md),
  [renderer data contract](FRAME_SYNTHESIS_RENDERER_DATA_CONTRACT.md), and
  [Unreal Engine 5.7 source reference](UNREAL_FRAME_SYNTHESIS_REFERENCE.md).
- Recommended Vulkan proof and renderer-port stages:
  [Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md).


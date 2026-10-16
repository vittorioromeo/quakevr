# Frame synthesis porting validation matrix

Use these gates for a new engine, renderer backend or headset runtime. A passing
row proves only that stage. Keep the test output with the engine revision,
runtime version, graphics API, headset and display mode.

| Gate | Test | Evidence to save | Pass condition |
|---|---|---|---|
| Runtime capability | Enumerate extension support before instance creation. | Runtime name/version, extension list and requested/enabled lists. | EXT is enabled only when advertised and requested; ordinary OpenXR startup still works without it. |
| Per-view recommendation | Chain the synthesis view structure into view enumeration. | Recommended motion dimensions per view. | All views have valid dimensions; stereo sizes/layers are handled explicitly. |
| Binding-specific formats | Enumerate formats after the graphics-bound session exists. | Graphics API, exact format IDs and image usage. | Chosen motion/depth formats are advertised and usable for the renderer's actual writes and reads. |
| Swapchain lifecycle | Repeated acquire/wait/write/release and resize/recreate. | Per-image indices, dimensions, array layers, API validation output. | No stale image, mismatched index assumption, validation error or leaked acquired image. |
| Synthetic vectors | Render a stationary background and translating opaque shape. | GPU readback or numeric comparison for both eyes and X/Y signs. | Known current-minus-previous displacement is reconstructed within a declared tolerance; static region is zero. |
| Camera versus object motion | Test head/camera-only motion and object-only motion separately. | Vector/depth images and expected values. | No double-counted head motion; moving objects and stationary-world camera flow agree with the selected contract. |
| Depth convention | Run conventional and reversed-Z projections and MSAA. | Captured depth image, min/max/near/far metadata, resolve rule. | Runtime interpretation places near/far geometry correctly; metadata nearZ and farZ are distinct metric distances. |
| Stereo metadata | Submit asymmetric left/right test patterns. | Per-view structures and subimage rectangles at `xrEndFrame`. | Each projection view references its own fresh correct images and structures remain alive through the call. |
| Affine format decode | Round-trip signed test values through the exact GPU target and runtime encoding. | Zero, small positive/negative, large in-range and out-of-range samples. | Scale/offset, transfer function and quantization are known; invalid motion is rejected/fallbacked rather than clamped. |
| Temporal reset | Trigger teleport, skipped frame, resize, focus recovery and tracking loss. | History-generation/reset logs and next-frame output. | No stale vectors are submitted with new color; color-only warm-up works. |
| Runtime receipt | Enable the runtime's motion/depth texture diagnostics. | Both-eye screenshots, accepted formats, per-frame buffer counters. | Runtime shows changing coherent motion and depth while known scene/camera motion occurs. |
| Synthesis effect | Compare compositor/displayed-frame diagnostics on/off under a workload that misses native refresh. | Submitted/displayed counts, synthesized/reprojected counters and wearer observations. | Evidence distinguishes frame-synthesis output from ordinary compositor reprojection. |
| Performance | Repeat the same deterministic scenario on/off in alternating order. | CPU/GPU frame times, compositor timings, render resolution, refresh and event counters. | The workload and event are proven active in both modes; added producer cost and output benefit are measured separately. |

## Deterministic scene fixture

Use a minimal scene before testing a real game: a high-contrast stationary
background, a foreground rectangle with known translations in X and Y, a second
moving object with a different depth, and asymmetric eye colors. Move only the
camera, only each object, then both together. Include a textureless patch, a
newly revealed edge, and one translucent element to expose known failure modes.
Check positive and negative motion, each eye independently, both depth
conventions, projection jitter on/off, and target-origin conversion. Save the
rendered color, source depth, source velocity and runtime-target images.

Avoid relying on only a `xrEndFrame` return code. That confirms submission was
accepted at the API boundary; it does not establish that the runtime used the
motion/depth data, generated an intermediate frame, or displayed a correct
result.

## Workload integrity for performance tests

Treat the test scenario name as a request, not proof that the scenario ran.
Before accepting a game profile, include assertions for:

- Game/simulation time advanced through the measurement window.
- The action occurred (for example, a kill-event counter increased).
- State changed as expected (the target count fell; particle/gib/decal counters
  rose if the workload requires them).
- The action happened inside the measured window, rather than only before or
  after it.
- Enabled and disabled runs exercised equivalent events in alternating order.

This repository's first `gore_headpop_24` captures exposed why these assertions
matter: `vr_xr_unfocused_pause 1` held the simulation, so the requested combat
did not occur. The runner was corrected to set the cvar after persistent
settings and assert the events. The later four 450-frame runs each recorded 25
deaths and a monster-count change from 25 to 1. Their short A/B/A/B comparison
estimated the app-side producer cost, but SteamVR had no present wearer, so
they still cannot establish headset performance or whether the runtime made
intermediate frames. Keep both workload validity and wearer/runtime evidence
as separate report fields. See the [corrected replay results](../../../performance-results/spacewarp-gore-real-abba-20261014/RESULTS.md).

## Reporting language

Keep these outcomes separate in reports:

- **Advertised**: runtime lists the extension.
- **Enabled**: instance was created with it.
- **Allocated**: compatible runtime-owned images were created.
- **Produced**: renderer wrote fresh matching motion/depth for every view.
- **Submitted**: metadata was chained into `xrEndFrame`.
- **Received**: runtime diagnostic confirms motion/depth buffers arrived.
- **Synthesized**: runtime diagnostic or controlled display test confirms
  intermediate frame generation.
- **Validated in headset**: wearer verified image quality and input/pacing on
  the target device.

Do not report a later state from evidence that only proves an earlier one.

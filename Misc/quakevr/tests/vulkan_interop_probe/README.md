# OpenXR-device GL motion/depth transport probe

This optional Linux diagnostic is independent of the game and the existing
frame-loop fixture. It uses a hidden GL context and, only outside `--discover`,
an OpenXR Vulkan2 session. It never submits projection layers or synthesis
metadata. Run XR modes only during an explicitly coordinated idle session.
External `timeout` bounds driver or runtime waits that an app cannot cancel.

Build on Steam Frame without building or installing the game:

```sh
cmake -S Misc/quakevr/tests/vulkan_interop_probe -B /tmp/qvr-formats-build \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/qvr-formats-build -j2
```

The initial isolated build used `/tmp/qvr-gl-formats-probe-build`; substitute
that directory in the commands below when using the staged diagnostic.

## Discover without OpenXR

The installed `VK_LAYER_LUNARG_api_dump` also captures Zink's Vulkan calls.
These two commands allocate ordinary GL texture storage; they do not create
an XR instance/session or issue application pixel writes/readbacks:

```sh
DISPLAY=:1 VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump timeout 20s \
  /tmp/qvr-formats-build/qvr_openxr_gl_formats_probe --format motion --discover \
  > /tmp/qvr-motion-discover.txt 2>&1
DISPLAY=:1 VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump timeout 20s \
  /tmp/qvr-formats-build/qvr_openxr_gl_formats_probe --format depth --discover \
  > /tmp/qvr-depth-discover.txt 2>&1
```

Inspect `vkCreateImage` between `DISCOVER_IMAGE_BEGIN` and
`DISCOVER_IMAGE_END`. Record the complete image definition, including any
view-format list, not just its format and allocation size. The program disables
SDL XRandR mode management in this process because its hidden diagnostic never
needs a display-mode change. The initial pre-hint discoveries emitted an SDL
`Could not restore CRTC` warning on exit; this was not an OpenXR error.
The updated motion discovery completed with exit 0 and no CRTC warning on
Frame; its capture is `/tmp/qvr-motion-discover-noxrandr-20261014.txt`.

Parent-reviewed ordinary-image captures on 2026-10-14 produced these **storage
candidates**, not verified imported-image definitions:

| GL storage | Vulkan format | Flags | Usage | View list | Ordinary image requirement |
|---|---:|---:|---:|---|---:|
| `GL_RGBA16F` | 97, `R16G16B16A16_SFLOAT` | `0x40000` | `0x80097` | none | 36,864 bytes |
| `GL_DEPTH_COMPONENT32F` | 126, `D32_SFLOAT` | `0x41000` | `0x80027` | none | 32,768 bytes |

The depth flags include `SAMPLE_LOCATIONS_COMPATIBLE_DEPTH`; the probe enables
`VK_EXT_sample_locations` conditionally for that candidate. Ordinary Zink's
suballocation offset is not an import offset: the probe uses a queried dedicated
allocation and offset zero, without padding or copying an ordinary heap offset.

## Verify actual imported definitions before GPU access

These commands **create an XR session**, but stop after GL memory import and
framebuffer completeness, before beginning frames or accessing image pixels:

```sh
DISPLAY=:1 VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump timeout 30s \
  /tmp/qvr-formats-build/qvr_openxr_gl_formats_probe --format motion \
  --flags 0x40000 --usage 0x80097 --views none --storage-only \
  > /tmp/qvr-motion-storage.txt 2>&1
DISPLAY=:1 VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump timeout 30s \
  /tmp/qvr-formats-build/qvr_openxr_gl_formats_probe --format depth \
  --flags 0x41000 --usage 0x80027 --views none --storage-only \
  > /tmp/qvr-depth-storage.txt 2>&1
```

Compare the exporter's `EXPORT_IMAGE_BEGIN` call against the Zink call between
`ZINK_IMPORT_IMAGE_BEGIN/END`. Check format, flags, usage, extent, mips, layers,
samples, tiling, sharing mode, initial layout, view-format list, dedicated import
metadata, allocation size and memory type. If they differ, adjust explicit
arguments to the **captured imported definition**, rerun storage-only, and
review again. A successful GL import/FBO alone does not prove a legal alias.
The GPU flag is a manual evidence gate; the probe cannot inspect a layer's dump
and prove that gate automatically. Never apply the old RGBA8 view list `[37,57]`
to these formats.

## Pixel transport after the definition gate

Use the reviewed parameters and replace `--storage-only` with
`--verified-image-definition`, retaining an outer timeout and API dump. For
example, **only if motion's imported definition matches the candidate**:

```sh
DISPLAY=:1 VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump timeout 45s \
  /tmp/qvr-formats-build/qvr_openxr_gl_formats_probe --format motion \
  --flags 0x40000 --usage 0x80097 --views none --verified-image-definition \
  > /tmp/qvr-motion-pixels.txt 2>&1
```

Depth runs independently with its reviewed parameters. The existing D32
transfer probe is not currently a valid success path: a later API-dump audit
found a captured XR D32 destination with no transfer usage, and the dump did
not include the matching `xrCreateSwapchain` request or a source/binary hash.
Do not run it with `--verified-image-definition` as evidence of a Vulkan-legal
write until the exact request and actual image definition are captured in one
hashed run. The separate [rasterized D32 attachment-write fixture](../raster_depth_probe/README.md)
passed on Frame on 2026-10-14 using legally sampled runtime-image readback and
same-run usage evidence. It does not validate the historical D32 transfer run.
A motion run checks all 4,096 pixels in each of three reuse cycles:

- GL writes asymmetric quadrants, including signed X/Y values for RGBA16F and
  distinct 0/0.125/0.5/1 depths for D32. Payloads change between cycles.
- Vulkan checks raw shared-image rows, then copies the same format into an
  acquired/waited runtime swapchain with transfer-source/destination usage.
- The XR copy reverses rows explicitly: destination row `y` contains GL row
  `63-y`. This produces Vulkan/XR top-row ordering. Motion payloads are already
  expressed with Vulkan NDC signs; the spatial flip does not negate Y again.
- Same-format per-row copies avoid any optional depth-blit capability or
  float/color conversion. A production bridge should flip once in its export
  shader instead of recording 64 row regions.
- Persistent coherent readback storage remains mapped through all CPU reads.
  Vulkan fences protect command-buffer reuse and readback. Binary external
  semaphores order GL/Vulkan access; Vulkan brackets GL access with EXTERNAL
  ownership transfers and GENERAL layout at the boundary.
- XR images return to their attachment layout on the session queue before
  release; each begun XR frame ends once with zero layers.

## Transferable engine guidance and limits

Separate native renderer outputs from runtime-owned images. Create application
export targets in a format supported by both APIs, prove exact image identity
and synchronization, then transfer or convert into the runtime's images.
Keep color, motion and depth from the same eye and submitted-frame history.
Spatial origin conversion and motion-component sign conversion are separate
operations; test them with an asymmetric pattern rather than a uniform pixel.

The 2026-10-14 Steam Frame run passed the FP16 motion gates: exact
imported-image definition, signed/asymmetric values, row-origin conversion,
three resource-reuse cycles, and pixel copies into OpenXR-owned swapchain
images all passed with zero mismatches. The earlier D32 run matched numeric
pixels, but its captured destination omitted transfer bits and its source is
not linked to the trace; that is diagnostic evidence, not a validated D32
transfer path. RGBA8 color had passed the equivalent XR color-copy slice
earlier. Full output and API dumps are recorded in the
[dated capture](../../../../../performance-results/spacewarp-gl-formats-probe-20261014/)
and [test report](../../../../docs/vr-port/SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md).

These results must not be inferred from compilation, discovery, storage
creation, or a synthesis metadata submission count. No game output, compositor
consumption, generated-frame cadence, wearer image quality or performance is
verified by this diagnostic. The constants matching Zink's image definitions
are driver-specific evidence, not a portable default for other engines.

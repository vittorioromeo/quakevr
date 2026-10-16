# GL/Zink motion and depth transport probe — 2026-10-14

## Result

On the tested Steam Frame image (Mesa 26.3.0-devel `git-abc426bb62`, Zink over
Turnip on Adreno 750), the isolated probe produced matching pixel values for
RGBA16F motion and D32 depth in three reuse cycles. A later audit found that
the captured XR D32 image was created without transfer usage, even though the
probe copied into it. The Vulkan API dump also does not identify the exact
probe source/binary used for that capture. Therefore, the old D32 transfer
result is useful pixel-path evidence but is **not a validated Vulkan-legal
transport result**. The motion image's captured usage includes transfer bits
and remains a valid transfer-path result for that captured image definition.
The new D32 attachment fallback separately passed a hashed Frame GPU test with
same-run usage verification and legal sampled readback; see the
[rasterized D32 report](../../../performance-results/spacewarp-depth-raster-20261014/RESULTS.md).

| GL target | Vulkan/OpenXR format | Flags | Usage | Import definition | Pixel result |
|---|---|---:|---:|---|---|
| `GL_RGBA16F` | `R16G16B16A16_SFLOAT` (97) | `0x40000` | `0x80097` | Exact exporter/Zink match; no view-format list | 12,288 pixels checked on shared image and 12,288 on XR image; zero mismatches, including signed X/Y values |
| `GL_DEPTH_COMPONENT32F` | `D32_SFLOAT` (126) | `0x41000` | `0x80027` | Exact staging exporter/Zink match | Old XR transfer path unproven; separate attachment fallback passed 12,288-pixel Frame GPU check |

Each cycle checked a 64×64 image (4,096 pixels). On the OpenXR copy the test
explicitly reverses rows, so XR/Vulkan row `y` receives GL row `63-y`. The
asymmetric quadrants verified that image-origin conversion is correct. Motion
values already used Vulkan NDC signs; the spatial row flip did not negate the
motion Y component a second time.

This historical probe uses a Vulkan OpenXR session created through
`XR_KHR_vulkan_enable2`, but it does not enable `XR_EXT_frame_synthesis`, submit
projection layers or attach synthesis metadata. Its XR frames end with zero
layers. It proves image transport and pixel correctness only. It does not prove
that SteamVR consumes motion/depth metadata, synthesizes or displays an
intermediate frame, or that Quake's live renderer can supply matching data.
The D32 transfer observation must not be used as a template for other engines;
inspect actual runtime image usage and use a verified attachment-write route
when transfer writes are unavailable. The separate attachment test also ends
with zero layers and no synthesis metadata; it proves the isolated depth
transport path only. See the [depth interop note](FRAME_SYNTHESIS_VULKAN_DEPTH_INTEROP.md).

## Reproduction and raw evidence

Build and run instructions are in the isolated
[probe README](../../Misc/quakevr/tests/vulkan_interop_probe/README.md). The
source is
[`vr_openxr_gl_formats_probe.cpp`](../../Misc/quakevr/tests/vr_openxr_gl_formats_probe.cpp).
Captured API dumps and complete stdout are in
[`performance-results/spacewarp-gl-formats-probe-20261014`](../../../performance-results/spacewarp-gl-formats-probe-20261014/).

The exact flags and usage are properties of the tested Zink/Turnip build, not
portable constants. Another engine/device must capture its own imported-image
definition and repeat the asymmetric pixel and reuse checks. See the
[cross-API integration notes](FRAME_SYNTHESIS_CROSS_API_INTEROP.md) for the
generic proof sequence.

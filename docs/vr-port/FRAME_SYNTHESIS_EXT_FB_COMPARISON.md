# `XR_EXT_frame_synthesis` and `XR_FB_space_warp`

For new engine work, implement `XR_EXT_frame_synthesis` first where the runtime
supports it. Khronos currently marks `XR_FB_space_warp` deprecated by EXT.
Some deployed runtimes may still expose only FB, so an engine with broad device
coverage can keep a separate compatibility backend. Do not treat either
extension string as proof that the runtime accepts useful motion/depth formats
or actually synthesizes frames.

## Shared renderer inputs

Both APIs attach per-view metadata to each `XrCompositionLayerProjectionView`
and refer to runtime-owned motion-vector and depth subimages. Both expect
current-minus-previous NDC displacement, metric depth clip distances, and an
application-space delta pose for artificial locomotion. Both recommend signed
16-bit float motion vectors. Both require color, depth and motion to correspond
to the same eye and frame interval.

That common renderer output can be shared internally. The OpenXR metadata,
format decoding, capability negotiation and runtime-specific submission must
remain separate.

## Differences that affect code

| Concern | `XR_EXT_frame_synthesis` | `XR_FB_space_warp` |
|---|---|---|
| Status in current registry | Replacement extension; revision 1. | Deprecated by EXT; revision 2 remains available on some runtimes. |
| Per-view structure | `XrFrameSynthesisInfoEXT` | `XrCompositionLayerSpaceWarpInfoFB` |
| Motion scale and offset | Explicit `motionVectorScale` and `motionVectorOffset` fields define affine decoding. | No corresponding scale/offset fields. Use the FB vector image contract and its supported format directly. |
| 2D vectors | Explicit `XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT`; uses RG and affine xy decode. This may reduce synthesis quality. | No equivalent 2D mode flag; uses the FB structure's RGB NDC motion-vector contract. |
| Relaxed interval request | EXT flag requests a relaxed interval as a hint. Runtime still controls cadence. | FB uses its own frame-skip flag/semantics; do not map this mechanically to the EXT hint. |
| Recommended vector size | Per-view `XrFrameSynthesisConfigViewEXT` chained during view-configuration enumeration. | `XrSystemSpaceWarpPropertiesFB` queried through system properties. |
| Extension-specific structures | EXT names/types and decoder fields. | FB names/types and flag values. |

Consult the exact extension revision in the runtime and headers used to build the
engine. In particular, do not copy an EXT relaxed-interval request into the FB
flag field or assume the recommendation query structures are interchangeable.

## Recommended backend design

Share an engine-side `MotionDepthFrame` representation containing per-view
motion/depth resources, image dimensions, frame token, convention metadata and
history generation. Then use two small submission backends:

```text
renderer output
  ├── EXT backend: EXT negotiation, config-view query, affine decode metadata,
  │                 optional 2D and relaxed-interval flags
  └── FB backend:  FB negotiation, system-property query, FB flags and structure
```

Each backend owns its composition-chain builder and capability report. If both
extensions are available, prefer EXT unless a tested runtime compatibility
policy explicitly chooses FB. Keep a projection-only fallback when neither
backend is usable.

## Primary Khronos references

- [EXT structure](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html)
- [EXT flags, including 2D motion and relaxed interval](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoFlagBitsEXT.html)
- [FB structure](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrCompositionLayerSpaceWarpInfoFB.html)
- [FB recommended motion/depth dimensions](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSystemSpaceWarpPropertiesFB.html)
- [FB deprecation state](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_FB_space_warp.html)
- [OpenXR registry and specifications](https://registry.khronos.org/OpenXR/)

The Khronos pages are the API authority. Engine samples such as
[Unreal's OpenXR implementation](UNREAL_FRAME_SYNTHESIS_REFERENCE.md) and
[Godot's extension](https://github.com/godotengine/godot/blob/master/modules/openxr/extensions/openxr_frame_synthesis_extension.cpp)
are implementation references whose assumptions can change between engine
versions.

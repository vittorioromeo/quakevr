# Godot OpenXR frame synthesis source reference

Source audit of Godot's current `master` implementation of
`XR_EXT_frame_synthesis`, reviewed 2026-10-14. This describes an engine
implementation, not Steam Frame runtime validation. The link tracks a moving
branch; check the exact Godot tag/commit being ported before relying on any
detail.

## Architecture

Godot makes frame synthesis an optional OpenXR extension setting. Once the
extension is enabled, the OpenXR extension wrapper registers a projection-view
hook. It chains `XrFrameSynthesisConfigViewEXT` during view configuration
enumeration, records the runtime's per-view recommended motion-target size, and
creates motion/depth swapchains when the main OpenXR swapchains are created or
resized. This is a useful lifecycle pattern: synthesis resources follow session
and main-swapchain lifetime rather than being hidden in game scripts.

Before drawing, the wrapper acquires synthesis targets and hands the selected
images to the rendering server as its velocity and velocity-depth textures. The
mobile renderer writes into them. After viewport drawing, the wrapper releases
the acquired targets. While building projection views, it chains one
`XrFrameSynthesisInfoEXT` per eye. If a frame is disabled or deliberately
skipped, it clears the renderer's synthesis texture handles and omits synthesis
metadata for that frame.

The source also exposes runtime controls to enable synthesis, request a relaxed
frame interval and skip the next frame. Renderer-thread state changes are
queued onto Godot's rendering thread. This is a good separation to preserve in
other engines: game-facing controls request behavior, the render thread owns
images, and the OpenXR submission hook owns the composition chain.

## Renderer and target choices in the audited source

The audited file selects formats by rendering driver/method: OpenGL uses
`GL_RGBA16F` motion and `GL_DEPTH24_STENCIL8`; Vulkan mobile uses
`VK_FORMAT_R16G16B16A16_SFLOAT` and `VK_FORMAT_D24_UNORM_S8_UINT`. It does not
enable this path for other Vulkan rendering methods. The targets are array
swapchains with two view layers, and the source assumes both eyes share the
first view's recommended motion width and height.

The composition metadata uses affine scale/offset fields, has an optional Y
flip setting, defaults to 2D flag off, and uses reversed-Z near/far values from
the render state. It computes the application-space delta from the previous
and current world-origin transform once before updating the stored previous
transform, then copies that delta to each eye. These are useful source patterns
to inspect; each value still needs validation against the renderer and target
runtime.

## Porting lessons

- Keep the extension wrapper at the OpenXR/renderer boundary, not in gameplay
  code.
- Recreate targets after the main swapchains change size and free them with the
  OpenXR session.
- Pass OpenXR-owned images into the renderer explicitly; do not assume the
  renderer's ordinary internal velocity target is the right image or format.
- Set and clear renderer texture handles on every path, including disabled,
  skipped, allocation-failed and session-restarted cases.
- Preserve separate metadata per eye and keep the OpenXR structs alive until
  `xrEndFrame`.
- Do not copy the current implementation's two-eye size assumption, hard-coded
  format mapping or reversed-Z defaults without checking the exact target
  runtime. Query format support for the selected graphics binding and use
  per-view dimensions or prove the runtime returns identical dimensions.
- Expose an image-origin diagnostic. Godot includes an optional Y-flip because
  implementations have differed; resolve the target convention with a known
  positive/negative vertical translation, not by trial and error in a game
  scene.
- Audit callback ordering when upgrading engine versions. Godot issue
  [#118902](https://github.com/godotengine/godot/issues/118902) documents a
  4.7 development regression where pre-render ran before swapchain setup and
  frame-synthesis state was not ready. Lifecycle callbacks can change between
  engine releases.

## Source links

- [Godot OpenXR frame synthesis extension](https://github.com/godotengine/godot/blob/master/modules/openxr/extensions/openxr_frame_synthesis_extension.cpp)
- [Godot OpenXR extension wrapper interface](https://github.com/godotengine/godot/tree/master/modules/openxr/extensions)
- [Godot issue #118902: callback order regression](https://github.com/godotengine/godot/issues/118902)
- [OpenXR adapter template](FRAME_SYNTHESIS_ENGINE_ADAPTER_TEMPLATE.md)
- [Validation matrix](FRAME_SYNTHESIS_PORTING_VALIDATION_MATRIX.md)
- [Unreal source reference](UNREAL_FRAME_SYNTHESIS_REFERENCE.md)

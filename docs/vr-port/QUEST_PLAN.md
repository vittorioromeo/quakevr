# Standalone Quest (3/3S, then 2): research, audit, decision, phases

Living plan for Quake VR running natively on the Quest (Android, AArch64, Adreno), started 2026-11-14 (phase C1,
branch `agent/quest`, never shipped by the coordinator). Markers: **[V]** verified (in our tree, or a source read),
**[I]** inferred or from memory (check before relying on it), **[?]** open, to measure on the device.

His goal: Quake VR standalone on Quest 3/3S (Quest 2 if not too hard), everything working and tested, Quest-specific
graphics defaults, perf and latency mitigations; OpenGL ES shared with the PC if it costs the PC nothing, else a
separate ES pipeline, or Vulkan if ES is a problem.

**Summary.** The renderer is much closer to OpenGL ES 3.2 than its "GL 4.3 core" requirement suggests: compute, SSBOs
and image load/store are core ES 3.1; the GL 4.4/4.5 extras (bindless, multi-bind, clip control, layered casters)
already have fallbacks behind `VR_GLSafeOff` (safe mode). The audit finds 14 groups of desktop-only calls, most of
them a handful of lines. Recommendation: **one renderer with an OpenGL ES 3.2 profile** (option A below), validated
on the PC with `-glescompat` before any Android build; the real work is the tiler's performance budget (fewer
full-screen passes, multiview into the swapchain, foveation), not API coverage. Vulkan stays the long-term path
(VULKAN_MIGRATION.md) and the fallback if the Quest's GLES driver or foveation limits bite.

## 1. Research: how the standalone Quake-family ports did it

| Port | Engine base | Graphics | XR API | Notes |
|---|---|---|---|---|
| QuakeQuest (Team Beef, DrBeef) | DarkPlaces [V: README] | DarkPlaces' GLES 2/3 renderer [I] | VrApi first, OpenXR now (Quest 1/2, Pico 4) [V: README] | Data copied by the player to `/sdcard/QuakeQuest/id1` [I]; sideloaded (SideQuest) [V]. Quake-PCVR (GameOrDie007) ported it back to PC OpenXR [V]. |
| Quake2Quest | Yamagi Quake II [I] | GL1-era renderer on GLES through a translation layer [?] | VrApi, later OpenXR [I] | |
| Lambda1VR (Half-Life) | Xash3D FWGS [I] | gl4es-style GL1 to GLES translation [I] | VrApi/OpenXR [I] | |
| QuestZDoom | LZDoom / GZDoom 3.x [I] | GZDoom's GLES renderer [I] | VrApi, then OpenXR [I] | |
| JKXR (Jedi Knight) | OpenJK | GLES | OpenXR; Android NDK + the headset's OpenXR loader [V: search result] | Team Beef's shared OpenXR glue ("TBXR") [I] |
| vkQuake | QuakeSpasm | Vulkan | none | Sascha Willems' Android port (2016, gamepad, no sound) [V]; no maintained Android/Quest build found [V: search]. |
| Quakespasm-VR (ObeseCatLord) | vkQuake 2.0 | Vulkan multiview, fragment shading rate | OpenXR (PC) | VULKAN_MIGRATION.md section 4; PC only as far as we know [?]. |

What they share [I unless marked]:
- **Toolchain:** Android NDK (clang, AArch64 `arm64-v8a` only: the Quest is 64-bit only), Gradle with the Android
  Gradle Plugin for the APK, a `NativeActivity`/`android_native_app_glue` or SDL2's Java shim, `ndk-build` or CMake.
- **OpenXR on Android:** the Khronos loader for Android (`libopenxr_loader.so`, Maven `org.khronos.openxr:
  openxr_loader_for_android`) or Meta's; `XR_KHR_android_create_instance` (JavaVM + activity), `XR_KHR_loader_init`,
  `XR_KHR_opengl_es_enable` (EGL display/config/context binding), swapchains as GLES textures (texture arrays for
  multiview). Meta extensions used by these ports and worth having: `XR_FB_display_refresh_rate` (72/80/90/120 Hz),
  `XR_FB_foveation` + `XR_FB_foveation_configuration` + `XR_FB_swapchain_update_state` (fixed foveated rendering on a
  GLES swapchain), `XR_EXT_performance_settings` (CPU/GPU levels), `XR_FB_color_space`, `XR_KHR_composition_layer_*`
  (menus as quad layers), `XR_FB_space_warp` (Application SpaceWarp: half-rate rendering, needs motion vectors) [?
  GLES support].
- **Rendering budget:** 72 Hz default (13.9 ms), 90 and 120 Hz optional; the old engines render per eye (two passes)
  and get away with it because Quake is light; Meta's guidance is multiview (one pass, both eyes in a tile) [V: Meta
  doc] and fixed foveation.
- **Files:** the player copies his own paks to shared storage (`/sdcard/<Game>/id1/...`): the ports never ship id
  data. Newer Android needs "all files access" (`MANAGE_EXTERNAL_STORAGE`, granted in the headset's settings) or the
  app's own `/sdcard/Android/data/<package>/files` [I].
- **Distribution:** sideloaded APKs (SideQuest, `adb install`), developer mode on the headset; not the Horizon Store.
- **Performance tricks:** fixed foveation (high on 72 Hz), lower eye-buffer scale, no or 2x/4x MSAA resolved on tile,
  CPU/GPU clock levels, sound on its own thread, fewer dynamic lights; DarkPlaces' realtime world lights stay off.

Meta's own position [V: developers.meta.com, "os-vulkan-opengl" and "po-advanced-gpu-pipelines"]:
- "OpenGL ES is now considered a legacy graphics API"; still supported, but new VR features are implemented in Vulkan
  only. Vulkan is the recommended API. OpenXR is the only supported XR API for new apps.
- Tiled GPU: the XR2 Gen 2 (Quest 3/3S) has about 2 MB of tile memory (XR2: 1 MB). Every extra render pass stores to
  RAM (a fixed cost per texture size); full-screen passes are the expensive thing; clears help rather than hurt;
  never store MSAA subsamples (resolve on tile, invalidate depth/MSAA); 2x MSAA is nearly free, 4x is not.
- Foveation is per tile. **With GLES the main pass is foveated only when it renders into the foveated swapchain
  texture itself**; an intermediate target is not (Vulkan can bind the density map to any pass).

Hardware [I]: Quest 3/3S: Snapdragon XR2 Gen 2, Adreno 740, 8 GB shared RAM, 72/90/120 Hz, panel 2064 x 2208 per
eye (3S: 1832 x 1920), default eye buffer about 1680 x 1760; Quest 2: XR2, Adreno 650, 6 GB, 1832 x 1920 panel,
about half the Quest 3's GPU [V: UploadVR headline]. Apps get a few GB of that RAM [?].

## 2. Audit of our code (file:line, `origin/next` 41f17bcdb)

`python Misc/quakevr/gles_audit.py Quake` reproduces the renderer table (a regex count per feature; its uint64 row
matches C++ and is noise).

### 2.1 Renderer: OpenGL 4.3 core vs OpenGL ES 3.2

The engine asks for GL 4.3 core (`gl_vidsdl.c:51`); every GL entry point past 1.1 goes through Ironwail's
function table (`glquake.h:132`, `QGL_CORE_FUNCTIONS`), so a GLES profile changes one loader.

Already ES-ready or behind a switch [V]:
- **Core in ES 3.1/3.2:** compute (`DispatchCompute`, 11 lines: `r_world.c:101` culling, `gl_rlight.c:223` light
  clusters, `vr_temporal.cpp`, `vr_upscale.cpp`), SSBOs (107 lines, 14 files), image load/store (`gl_rlight.c:160`),
  `DrawElementsIndirect`, sampler objects, `TexStorage*`, `BlendFunci`, `MinSampleShading`, `CopyImageSubData`, KHR
  debug groups, float render targets (R11F_G11F_B10F, RGBA16F: colour-renderable in ES 3.2 [I]).
- **Optional with fallbacks** (`gl_vidsdl.c:1193-1229`, `VR_GLSafeOff`): persistent mapped buffers (ES:
  `EXT_buffer_storage` [I: Adreno has it]), bindless textures (with `gl_DrawID`, `GL_ARB_shader_draw_parameters`,
  `gl_shaders.h:504,557`; no ES form; the bound path `r_world.c:449` draws with a `DrawElementsIndirect` loop, ES 3.1),
  multi-bind (loops of binds), clip control (reversed Z: `EXT_clip_control` [?] on Adreno), layered shadow casters
  (`gl_ViewportIndex` in a VS: no ES form; the non-layered path exists).
- **Multiview:** `vr_stereo_single.cpp:122-146` (`GL_OVR_multiview2`, `glFramebufferTextureMultiviewOVR`) and the
  `QVR_MULTIVIEW` twins of 59 programs (`gl_shaders.c:183`): the Quest's natural path. It uses `glTextureView`
  (`vr_stereo_single.cpp:146`) and `glTexStorage3DMultisample` (`:141`) on the PC; on the Quest it renders into the
  swapchain's array texture directly (`OVR_multiview_multisampled_render_to_texture` [I]).

Not gated yet (the 14 rows `vr_gles_report` prints) [V counts]:

| Desktop-only use | Where (first line, count) | ES form |
|---|---|---|
| `#version 430` literals | 52 lines in 17 files: `gl_shaders.c:171` (engine prefix), `vr_gfx_gl.cpp:53` (12), `vr_bloom.cpp:56` (5), `vr_temporal.cpp:72` (4), `vr_upscale.cpp:49` (4), `vr_glutil.cpp:63`, `vr_visor.cpp:418`... | one prefix function: `#version 320 es`, `precision highp float; precision highp int;` and sampler precisions |
| `MultiDrawElementsIndirect` | `r_world.c:445` (bindless only), `vr_lighting.cpp:703` | `EXT_multi_draw_indirect` [?], else the loop |
| `glGetTexImage` | `gl_texmgr.c:541`, `vr_envmap.cpp:245`, `vr_pbr.cpp:352`, `vr_lighting.cpp:1732` | FBO + `glReadPixels` |
| `glGetBufferSubData` | `vr_gfx_gl.cpp:1515`, `vr_temporal.cpp:503`, `vr_water.cpp:1019` | `glMapBufferRange(READ)` |
| `glClearTexImage` | `vr_temporal.cpp:504` | clear through an FBO |
| `glTextureView` | `vr_envmap.cpp:53`, `vr_stereo_single.cpp:146` | render to array layers directly |
| `GL_TEXTURE_LOD_BIAS` | `gl_texmgr.c:169`, `vr_gfx_gl.cpp:44` | bias argument in the shader |
| `GL_DEPTH_CLAMP` | `r_alias.c:53` (view model) | `EXT_depth_clamp` [?] or the view model's depth range |
| `gl_ClipDistance` | `gl_shaders.h:659`, `r_world.c:694`, `r_alias.c:776`, `vr_lighting.cpp:102` | `EXT_clip_cull_distance` [?] or discard |
| `noperspective` | `gl_shaders.h:641`, `vr_glsl_alias.h:378` | `NV_shader_noperspective_interpolation` [?] or multiply by w |
| `textureQueryLevels` | `vr_gfx_gl.cpp:191` | a uniform |
| timer queries (64-bit, `GL_TIMESTAMP`) | `vr_profile.cpp:342`, `vr_tracy.cpp:178`, `vr_lighting.cpp:1807`, `gl_rmain.c:1269` | `EXT_disjoint_timer_query` [I: Adreno has it] |
| `glDrawBuffer`, `glClearDepth`, `glDepthRange`, `glPolygonMode` | `vr_gfx_gl.cpp:1648`, `vr_glutil.cpp:273`, `vr_lighting.cpp:1547`, `gl_rmain.c:569`, `gl_rmain.c:2155` | `glDrawBuffers`, `...f` forms, wireframe debug off |
| `GL_FRAMEBUFFER_SRGB`, `GL_TEXTURE_CUBE_MAP_SEAMLESS`, `GL_SAMPLES_PASSED`, `GL_BGRA` | `vr_imgui.cpp:320`, `gl_vidsdl.c:1387`, `gl_rmain.c:1273`, `r_brush.c:467` | `EXT_sRGB_write_control`; always on; `ANY_SAMPLES_PASSED`; RGBA |

Absent from the tree [V: 0 hits]: geometry and tessellation shaders, `GL_TEXTURE_1D`, `GL_QUADS`, doubles, 16-bit
normalized formats, `GL_DEPTH_COMPONENT32`, primitive restart, `glMapBuffer`. NVIDIA's shading-rate image
(`vr_foveated.cpp:51`) is off under `-glescompat`; the Quest's foveation is `XR_FB_foveation` on the swapchain.

### 2.2 Platform, libraries, CPU

- **SIMD [V]:** the engine's SSE2 (`mathlib.h:168` `USE_SSE2`: `mathlib.c:703,871`, `gl_texmgr.c:1167,1225`,
  `gl_model.c:1431`, `sv_phys.c:855`, `main_sdl.c:99`) has scalar fallbacks already. Ours had none:
  `vr_ao.cpp:1124` (the bake's rays), `vr_audio.cpp:1116` (the anti-alias filter), `vr_audio.cpp:792` (`_mm_pause`),
  `vr_audio_io.cpp:271` (interpolation taps). **Done in C1:** `vr_simd.hpp` (section 5). `vr_sha256.cpp:10` is guarded
  (x86 SHA path, scalar otherwise). The AVX ray kernel stays x86-only.
- **Floating point [I]:** the PC build "contracts nothing" (no FMA); the NDK's clang defaults to `-ffp-contract=on`
  and AArch64 has FMA, which would change results (the AO bake's "same bits as the reference", QC physics). The
  Android build must pass `-ffp-contract=off` (C3).
- **Windows-only code [V]:** 34 VR files carry `_WIN32` blocks; the native Linux build (LINUX.md) already gave each a
  POSIX path (crash handler, run logs, runtime detection, RenderDoc, mimalloc, `posix_spawn`). Android differs from
  that Linux: no glibc `backtrace` (`vr_crash.cpp`: `<unwind.h>` instead), no X11/GLX (`vr_xr_linuxgl.cpp`), no
  `/proc` of other processes, **no child processes of our own binary** (`vr_motion_child.cpp:184`,
  `vr_relight_process.cpp:182` `posix_spawn` our exe: in-process fallbacks needed), no `xdg-open`.
- **OpenXR [V]:** we use OpenXR on the PC (`vr_backend_openxr.cpp`: WGL binding `:670`, GLX/EGL `:679-729`
  through `XR_MNDX_egl_enable`, visibility mask `:770`). The Quest needs `XR_KHR_opengl_es_enable`
  (`XrGraphicsBindingOpenGLESAndroidKHR`: EGL display, config, context), `XR_KHR_android_create_instance` and the
  loader's Android init. No `XR_FB_*` extension is used today [V: 0 hits].
- **SDL2 [V]:** window, input, sound and GL context (`gl_vidsdl.c` 95 SDL calls, `in_sdl.c` 289, `snd_sdl.c` 19).
  SDL2 runs on Android (its Java activity, EGL ES contexts, AAudio/OpenSL); on the Quest input comes from OpenXR,
  so SDL is the EGL context, audio and file paths.
- **Threads [V]:** Zancle's thread pool (`vr_jobs.hpp`); `Zancle/Concurrency/Thread.cpp:27` has a POSIX path for
  `ZA_SYSTEM_ANDROID`; `ZA_CPU_RELAX` covers ARM. Box3D has a NEON path (`external/box3d/src/simd_neon.h`); Recast and
  mimalloc are portable C/C++ (mimalloc supports Android [I]).
- **Audio [V/I]:** Steam Audio is loaded at run time (`external/steamaudio/lib` has `windows-x64` only); Valve ships
  `libphonon.so` for Android ARMv8 in its release zip [I]. Our own mixer (vr_audio) is portable after C1's SIMD work.
- **Doom module** (`qvr_doom.dll`), NVML/PDH GPU stats, OBS, the updater: Windows-only, stay off on Android.
- **Memory [V: ROUND21.md]:** working set 390-740 MB per map on the PC (e1m1 to ad_grendel), peak about 1.1 GB with
  mimalloc purging at once; textures and buffers come on top and share the Quest's RAM. Fits a Quest 3 [I]; the
  Quest 2 needs measuring (C6) [?]. PBR/detail textures, the shadow atlas (8192: 256 MB at 32-bit) and the envmaps
  are the big GPU items to scale down.
- **Shaders [V]:** 59 programs with multiview twins plus our passes' own; `GL_KHR_parallel_shader_compile` is used
  when present (`gl_vidsdl.c:1464`); Adreno compiles are slow [I]: a program binary cache (`glProgramBinary`, core
  ES 3.0) belongs in C5.

## 3. Decision: shared GLES profile, separate ES backend, or Vulkan

| | A. One renderer, an OpenGL ES 3.2 profile | B. Separate ES backend | C. Vulkan |
|---|---|---|---|
| Idea | The same C/GLSL code; a start-up profile (ES context, `#version 320 es` prefix, the gates above) picks ES-safe paths that already exist or are added once | A second, simpler GLES renderer (e.g. a QuakeSpasm-like path) beside the desktop one | Vulkan device layer and ports of every pass (VULKAN_MIGRATION.md option a/b) |
| Effort [I] | C2 gates and prefix: ~1-2 weeks of agent work; C5 on device: 2-4 weeks (driver bugs, perf passes) | 2-3 months, and every new effect twice forever | 4-6+ months for parity; months more for our ~80 passes |
| PC penalty | None: the profile is chosen at start-up; desktop keeps bindless, clip control, layered casters [V: the gates exist] | None | None if GL stays on PC; large if replaced |
| Features on Quest | All, as cut by the Quest preset | Only what is ported | All, eventually |
| Foveation | `XR_FB_foveation` when the scene renders into the swapchain texture (multiview) [V: Meta doc] | same | best: density map on any pass |
| Risk | GLES is "legacy" on Quest: works today, no new features [V]; Adreno GLES driver bugs (kblood's decal hang) [?]; tiler cost of our post passes | Divergence, double maintenance (against his "share code" rule) | Biggest effort; but the right long-term API on Adreno and for Steam Frame |

**Choice: A.** It reuses everything (his reuse rule), costs the PC nothing, and is validated on the PC
(`-glescompat`) before any install. The work that matters on the Quest is the budget, not the API: render the scene
with multiview straight into the swapchain (so foveation applies), keep the post chain short (each full-screen pass
is a store and reload on a tiler), 2x/4x MSAA resolved on tile, a Quest graphics preset. If C5 hits a driver wall
(Adreno GLES bugs we can't work around, or foveation that GLES can't reach), switch to C (Vulkan), starting with
the passes the Quest preset keeps; the GLES profile's gates are the same seams a Vulkan port needs. Quest 2 support
is the same build with a lower preset (half the GPU) [I].

## 4. Phases

| Phase | Scope | Needs installs? |
|---|---|---|
| C1 (done) | Research, audit, decision, this plan; `vr_simd.hpp` + `vr_simd_test`; `-glescompat` + `vr_gles_report`; `quest_budget.cfg` and the budget runs | No |
| C2 (done: C2a, C2b, C2c) | **PC portability layer**: the 14 ungated rows behind `VR_GLESCompat()` (one `#version`/precision prefix for all programs, `glGetTexImage`/`glGetBufferSubData`/`glClearTexImage`/`glTextureView` replacements, LOD bias in shaders, depth clamp, clip distance, noperspective, timer-query wrapper); the GLSL made ES-valid (checked by compiling every program as `#version 320 es` with the PC driver where it accepts ES profiles, else with glslang if he approves it); `-glescompat` runs every test suite clean; `-fp-contract`-free scalar check; a "no child process" mode for the relight and motion children; an `Android`-guarded skeleton for crash/files/paths that compiles on the PC (`QVR_FAKE_ANDROID`) | No |
| **C3** (next; needs his approval of section 7's installs) | Android build: CMake toolchain for `arm64-v8a`, Gradle project (`Android/`), SDL2's Android activity, `-ffp-contract=off`, assets path and permissions, logcat console, the APK; first boot in the Android emulator (flatscreen, ES 3.x) for files, sound and menus | **Yes** (list below) |
| C4 | OpenXR on Android: `XR_KHR_opengl_es_enable` binding, Android loader init, swapchains as GLES texture arrays, multiview into them, refresh rate, `XR_EXT_performance_settings`, `XR_FB_foveation`, Touch controller bindings (already OpenXR actions), visibility mask; first headset boot; a GL extension dump of the Quest's driver resolves the [?] rows | **Yes** + a Quest in developer mode |
| C5 | Renderer on device: driver bugs, the post chain merged into fewer passes for the Quest preset, program binary cache, MSAA on tile, invalidations at pass ends; Adreno profiling (`ovrgpuprofiler`, RenderDoc) | Yes (device tools) |
| C6 | Perf and defaults: a Quest graphics preset (Quest 3 and Quest 2 levels) from measurements, dynamic resolution and foveation level tied to frame time, CPU/GPU levels, thread affinity (big cores), memory caps (shadow atlas, textures); latency: late latch of poses, `xrWaitFrame` pacing | Device |
| C7 | Packaging and tests: APK signing (debug key), install docs, first-run data copy check, the mock backend on Android for automated runs over `adb`, the checklist's Quest scenarios | Device |

## 5. C1 groundwork (done, no installs)

- **`Quake/vr/vr_simd.hpp`:** four float lanes as one interface, backends `Sse2` (x86-64), `Neon` (AArch64) and
  `Scalar` (anything; the test reference), structs of static functions so kernels can be templates over them.
  `vr_ao.cpp`'s ray kernel (`raysX4<simd::Native>`), `vr_audio.cpp`'s anti-alias filter and `vr_audio_io.cpp`'s
  interpolation now use it; the spin wait uses Zancle's `ZA_CPU_RELAX`; the AVX ray kernel stays x86-only.
  Verified: `vr_simd_test` 58880 passed, 0 failed (every operation on special and random lanes, the three kernels'
  shapes, native SSE2 against scalar, bit for bit; NaN payloads excepted); audio goldens (`vr_snd_test all`, 42/42)
  bit-identical before/after (0 of 1,061,750 samples differ); `vr_ao_bench sse reference soldier`: the same bytes
  (9260808e) as before. The NEON backend compiles for `aarch64-linux-android29` with the PC's clang 22 (freestanding,
  `-Wall -Wextra -Werror`), no FMA emitted; it has not run on ARM yet (C3).
- **`-glescompat`** (or `QVR_GLES_COMPAT=1` for bench runs): bindless, multi-bind, clip control and layered casters
  off through the safe-mode gates (logged as "GLES compat"), NVIDIA's shading-rate foveation off; `VR_GLESCompat()`
  for C2's gates; **`vr_gles_report`** prints the live switches and the 14 pending rows. e1m1 draws correctly with it.
  PC defaults and behaviour unchanged without the flag.
- **`Misc/quakevr/bench/quest_budget.cfg`** (Low preset, no MSAA, scale 1, no dynamic resolution) and
  **`Misc/quakevr/gles_audit.py`**. Budget runs: section 6.

## 5b. C2a: the GLSL as ES 3.2, the first desktop-only calls (done, no installs)

- **Every program compiled as ES 3.2 GLSL under `-glescompat`** (`VR_GLShaderSource`, vr_glsafe.cpp: the one
  `glShaderSource` for gl_shaders.c and the four VR compile sites; imgui gets its own `#version 300 es` shaders). The
  `#version 430` line becomes `#version 320 es` plus `EXT_clip_cull_distance` and `NV_shader_noperspective_interpolation`
  where the driver has them, and precision statements go before the first line of code. The PC's NVIDIA driver
  (RTX 4090, 621.01) compiles ES shaders in our 4.3 core context (`ARB_ES3_2_compatibility`), so it checks every
  program the way an ES driver would. Under `-glescompat` a failed program is printed and counted, not fatal
  (`vr_gles_report: N pending, M shader errors`); `-glesverbose` prints each error with its source line.
- **The GLSL fixed for ES** (the same code on the desktop: the conversions it did implicitly now spelled out): int and
  uint literals and indices, `.length()` as uint, undefined macros in `#if` defined (`RETRO`, NIS's), no
  `layout(depth_any)` on ES (`EXT_conservative_depth`; it is the default anyway), FSR's `0u`. Image formats ES 3.1
  lacks (`VR_GLESImageFormat`): the light clusters `rg32ui` as `rgba32ui` (256 KB more), the palette LUT `r8ui` as
  `r32ui` (8 MB instead of 2; C5 may move it to a buffer), the TAA mask `r8` as `rgba8`. NVIDIA Image Scaling's GLSL
  leans on implicit conversions everywhere: off under ES (`vr_upscale 2` takes FSR). Start-up: 229 failing programs
  to 0. **PC fix found:** the particles' `Support[16]` sat on uniform locations 20-35 over the fragment shader's
  `Taper` (20) and `InkAlpha` (21): a link error on a strict linker; moved to 40-55.
- **ES forms of desktop-only calls:** `gfx::readBuffer` (glGetBufferSubData, or `glMapBufferRange(READ)` under ES:
  the water mesh, TAA stats, particle waste; the water mesh's hash is the same both ways, 0b34a576 on e1m1),
  `gfx::clearTexture` (glClearTexImage, or a clear through a framebuffer), `gfx::drawNoColour` (glDrawBuffers),
  `glClearDepthf`/`glDepthRangef` (function table; the same values on the PC), `VR_GLPolygonMode` (none on ES: the
  wireframe debug views draw filled), `GL_TEXTURE_CUBE_MAP_SEAMLESS` not enabled on ES (always on there).
- **Verified:** with `QVR_GLES_COMPAT=1`: scenario_check 129 run, 0 failed, 0 shader errors; load_models_test 4/0;
  e1m1 draws the same as the PC mode. PC mode: scenario_check 129/0, load_models_test 4/0, vr_simd_test 58880/0.
- **Open, for the device (C4) [?]:** `GL_MAX_FRAGMENT_IMAGE_UNIFORMS` on Adreno (the world and model fragment shaders
  read the light clusters as an image; ES 3.1's minimum is 0: a `usampler3D` + `texelFetch` form if it is 0);
  `EXT_clip_cull_distance` and `NV_shader_noperspective_interpolation` on Adreno (the shaders compile with them here).

## 5c. C2b: the rest of the desktop-only calls (done, no installs)

- **glGetTexImage's ES form** (`gfx::readTexImage`, vr_glutil.cpp; `VR_GLGetTexImage` for C): every caller
  (imagedump, imagehash, the envmap's average and dump, the PBR bakes, the shadow atlas checks) reads through it.
  Under `-glescompat` each layer (or cube face) is attached to a framebuffer and read with `glReadPixels` in the form
  ES guarantees (RGBA bytes from normalized textures, RGBA floats from float ones), then converted to what was asked
  (RGB, the pack alignment); a depth texture is drawn first, as bits (`floatBitsToUint`), into an R32UI texture and
  read in 256-row strips as `RGBA_INTEGER` (ES reads no depth). Integer and compressed textures cannot be read the ES
  way (zeros; 16 such textures at vrstart). **`vr_gles_readback_test`** (Debug > Startup) reads everything both ways
  and compares bytes: PC 4243 passed, 0 failed (4199 textures, 42 envmap face levels, 2 shadow atlases, bit for
  bit); `-glescompat -glesclipcontrol` the same 4243/0; plain `-glescompat` 4199/0 (no envmap or atlases: below).
- **The other rows:** `glMultiDrawElementsIndirect` replaced under `-glescompat` by a `glDrawElementsIndirect` loop
  (`VR_GLESInstallForms`; its callers, bindless and the layered casters, are off there anyway); no texture views
  (the envmap regenerates the whole cube's mipmaps; single-pass stereo off: two passes, C4 draws multiview into the
  swapchain's layers); no `GL_TEXTURE_LOD_BIAS` (`gl_lodbias`, `vr_menu_sharpen`'s panel bias: none on ES; C5 can
  put the panel's in its shader); `vr_model_fragments`/`vr_world_fragments` refuse (no pipeline statistics or sample
  counts on ES); `GL_FRAMEBUFFER_SRGB` not read (nothing turns it on); `GL_BGRA`: none left (lightmaps are RGBA).
  `vr_gles_report`: 5 pending, 0 shader errors.
- **Found: clip control gates whole features.** Without `EXT_clip_control` (off under `-glescompat`) the shadows
  ("shadows need GL_ARB_clip_control"), the weapon and water reflections (envmap) and the portals are off. So the
  section 6 "+ GLES compat" column ran without shadows, reflections or portal views. **`-glesclipcontrol`**
  (`QVR_GLES_CLIP_CONTROL=1`) keeps clip control under `-glescompat`, as an ES driver with `EXT_clip_control`.
  Whether the Quest's driver has it decides C2c's biggest item: a standard-depth (-1..1) path for the shadow atlas,
  envmap and portals if not.
- **Pending, the Quest's extensions [?] (C4's dump decides; fallbacks in C2c only for the missing ones):**
  `EXT_depth_clamp` (else a held item nearer than the near plane is cut, as with `vr_nearclip_held 0`),
  `EXT_clip_cull_distance` (else a varying and a discard, which costs early-Z on a tiler), `NV_shader_noperspective_
  interpolation` (else the light-tile coordinate multiplied by w in the vertex shader and divided in the fragment
  shader: exact), `EXT_clip_control`; timer queries: `EXT_disjoint_timer_query` has the same calls (C3 loads the
  EXT names and reads `GL_GPU_DISJOINT_EXT`).
- **Verified:** `QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1`: scenario_check 129 run, 0 failed; load_models_test 4/0.
  PC: scenario_check 129/0, load_models_test 4/0, vr_simd_test 58880/0; dev and ThinLTO builds 0 warnings.
- **Not done in C2b (moved to C2c):** the ES runs of the render tests beyond the read-backs (the envmap, PBR bake,
  shadow and TAA tests under `-glescompat`), the no-child-process mode for the relight and motion children, the
  `QVR_FAKE_ANDROID` skeleton.

## 5d. C2c: the render paths under ES, the Android layer's skeleton, no child processes (done, no installs)

- **The render paths under the ES profile** (`Misc/quakevr/gles_render_test.sh <agent>`, `gles_render_check.py`):
  e1m1's start with TAA on (`vr_taa 1`), the map lights' shadows (the player's own on the floor), the envmap and the
  PBR companions at their defaults, seeded, drawn once on the PC and once under `-glescompat -glesclipcontrol`; the
  left eye after 120 frames, `vr_envmap_dump`, `imagehash` compared. Result: the eye 0.0005 of its pixels moved by
  more than 60 of 765 (mean 1.03 per channel), the envmap identical, 3599 textures (the PBR companions among them)
  the same bits, 0 shader errors, no feature off. `taaparticles_test` (the flat-screen TAA's half-size particles)
  under ES + clip control: 2/0. `vr_shadow_layered_check` is not an ES test (no layered casters there: it would
  compare the face-at-a-time path with itself).
- **The Android platform layer's skeleton** (`vr_android.cpp`): the layer's logic is shared (the base folder: the
  app's external files folder unless `-basedir`, where the player copies `id1`; `<base>/quakevr/crash`; a missing
  `id1/pak0.pak` named in the log; a crash handler on SIGSEGV/SIGABRT/SIGFPE/SIGILL/SIGBUS that writes a report
  from fixed buffers, signal-safe: the signal, the build, the return addresses from `<unwind.h>`'s
  `_Unwind_Backtrace`, into `crash.txt` and logcat, then the default action, the system's tombstone). Its Android
  calls (`SDL_AndroidGetExternalStoragePath`, `__android_log_write`, the unwinder, `sigaction`, `open/write`) sit in
  one `os` namespace whose Android forms compile only under `__ANDROID__` (C3 is their first compile) and whose PC
  forms stand in: `QVR_FAKE_ANDROID_DIR` as the external folder, stderr as the log, the signal handlers recorded but
  not installed (the PC's own crash handler stays). On the PC the layer is off unless **`-fakeandroid`** (or
  `QVR_FAKE_ANDROID=1`, or a build defining `QVR_FAKE_ANDROID`). **`vr_android_test`** (Debug > Startup): under
  `-fakeandroid` 14 passed, 0 failed (folders, every signal's handler, a report written and read back as a crash
  writes it, no child processes).
- **No child processes** (`VR_ChildProcesses()`): none on Android, under `-fakeandroid` or with
  **`-nochildprocesses`** on the PC. The relighting (ericw-tools' light as a process) and the motion review's
  Re-evaluate (a second copy of the game) refuse with a message saying so (the motion review: run `vr_motion_eval` in
  the game instead); both are developer tools, so on the Quest they stay off (an in-process light would mean building
  ericw-tools as a library: not planned).
- **For C3:** `vr_crash.cpp`'s POSIX handler (Linux: glibc's `backtrace`, `dladdr`, `sigaltstack`) is the richer
  one; on Android `backtrace` exists only from API 33 (`execinfo.h`). C3 either skips it under `__ANDROID__` (main
  installs vr_android's first; VR_InstallCrashHandler would replace it) or gives it vr_android's unwinder and logcat
  and drops the skeleton's handler (the reuse rule favours the second). Sys_Printf to logcat is C3's too (`os::log`).
- **Not done (optional, waits for C4's extension dump):** the `noperspective` fallback behind `-glesnoext` (the light
  tile coordinate and the retro affine texturing: `v * gl_Position.w` out, `* gl_FragCoord.w` in, exact; 4 shader
  sites in gl_shaders.h and vr_glsl_alias.h), the clip-distance fallback, the standard-depth path.
- **Verified:** PC: scenario_check 129/0, load_models_test 4/0, vr_simd_test 58880/0; dev and ThinLTO builds 0
  warnings; ES + clip control: gles_render 8/0, taaparticles 2/0; `-fakeandroid`: vr_android_test 14/0.

## 6. Perf budget (PC, as a relative picture)

Runs: `bench.sh quest --scenarios core --reps 1 --hz 72 --eye 1760`, ThinLTO, exclusive; PC defaults, then
`--settings quest_budget.cfg`, then the same with `QVR_GLES_COMPAT=1`. A desktop GPU's numbers: what the Quest-like
settings and the ES profile cost relative to each other, not the Quest's frame time (C6 measures that).

Results, 2026-11-14 (one repeat each, so a few percent is noise; ms averages; `kit/benchresults/q1_pc`,
`q1_quest`, `q1_questes`; the frame is paced at 13.89 ms throughout):

| scenario | GPU 3D PC | Quest-like | + GLES compat | CPU PC | Quest-like | + GLES compat |
|---|---|---|---|---|---|---|
| combat_48 | 3.15 | 1.47 | 2.33 | 4.51 | 4.05 | 4.47 |
| combined | 5.24 | 2.98 | 3.57 | 8.54 | 7.36 | 7.06 |
| decals_1024_stream | 1.23 | 0.67 | 0.86 | 1.38 | 1.50 | 1.37 |
| gore_slash_32 | 2.38 | 1.62 | 2.25 | 4.03 | 4.30 | 4.45 |
| idle_e1m1 | 1.14 | 0.57 | 0.79 | 1.64 | 1.54 | 1.63 |
| idle_range | 1.11 | 0.64 | 0.77 | 1.35 | 1.33 | 1.34 |
| lights_32 | 1.59 | 0.94 | 1.53 | 1.51 | 1.67 | 1.34 |
| load_e1m1 | 1.09 | 0.54 | 0.80 | 6.31 | 4.99 | 5.14 |
| particles_dense | 2.71 | 1.31 | 1.90 | 2.97 | 2.56 | 2.46 |
| props_500_active | 1.92 | 1.06 | 1.36 | 3.91 | 3.82 | 4.24 |
| ragdolls_32_active | 2.40 | 1.44 | 2.34 | 4.13 | 4.11 | 4.44 |
| teleporter_loop_r2 | 3.84 | 0.69 | 1.49 | 2.49 | 0.84 | 1.02 |
| teleporter_start | 1.88 | 0.59 | 0.80 | 1.71 | 1.00 | 0.89 |

- **Quest-like settings** (Low preset: no normal/parallax maps, no portals, shadow atlas 4096 and 1024 units, no
  MSAA) halve the eyes' GPU time (-30 to -82%; the teleporter's portal views most). The CPU barely moves (-10% in
  combat): the CPU side (game logic, physics, gore) is the Quest's other budget and needs its own C6 work; the
  Quest's big cores are several times slower than this PC's [I].
- **The GLES profile costs GPU time** on top: +15 to +60% (lights_32 +63%, ragdolls +62%, gore +39%, combat +58%).
  The likely parts: the layered shadow casters off (one draw per cube face: lights and ragdolls), bindless off
  (a bind and a draw per texture batch instead of one multi-draw), and clip control off (standard depth). Each
  to be split in C2 (one gate at a time, `QVR_GLES_COMPAT` subsets) and its Quest form chosen there: the layered
  casters' ES replacement is multiview-style layered rendering or fewer shadowed lights in the Quest preset.
  The CPU cost of the bound path is within the run's noise here. **Correction (C2b, section 5c):** that column ran
  with clip control off, which turns the shadows, reflections and portals off; its extra GPU time is the rest (bound
  path, standard depth, ES precisions), with fewer effects drawn. Re-run with `QVR_GLES_CLIP_CONTROL=1` in C2c.
- **C2c re-run, the ES profile with clip control** (`kit/benchresults/q2_questesclip`; the same settings, ThinLTO,
  exclusive, one repeat): the same effects as the Quest-like column, so the difference is now the ES profile alone
  (layered casters off, bindless and multi-bind off, the ES precisions). ms averages:

| scenario | GPU 3D Quest-like | ES no clip (C1) | ES + clip | CPU Quest-like | ES + clip |
|---|---|---|---|---|---|
| combat_48 | 1.47 | 2.33 | 2.30 | 4.05 | 4.06 |
| combined | 2.98 | 3.57 | 5.47 | 7.36 | 7.31 |
| decals_1024_stream | 0.67 | 0.86 | 0.93 | 1.50 | 1.35 |
| gore_slash_32 | 1.62 | 2.25 | 2.42 | 4.30 | 4.02 |
| idle_e1m1 | 0.57 | 0.79 | 0.82 | 1.54 | 1.77 |
| idle_range | 0.64 | 0.77 | 0.84 | 1.33 | 1.41 |
| lights_32 | 0.94 | 1.53 | 1.69 | 1.67 | 1.44 |
| load_e1m1 | 0.54 | 0.79 | 0.81 | 4.99 | 5.00 |
| particles_dense | 1.31 | 1.90 | 1.73 | 2.56 | 2.52 |
| props_500_active | 1.06 | 1.36 | 1.82 | 3.82 | 3.75 |
| ragdolls_32_active | 1.44 | 2.34 | 2.18 | 4.11 | 3.95 |
| teleporter_loop_r2 | 0.69 | 1.49 | 1.30 | 0.84 | 0.84 |
| teleporter_start | 0.59 | 0.80 | 0.83 | 1.00 | 0.97 |

  The ES profile costs +32 to +88% GPU over the Quest-like PC settings (combined +84%, lights_32 +80%, props +72%,
  combat +56%, ragdolls +51%, idle +44%); the CPU is unchanged (within one repeat's noise). Only `combined` and
  `props_500_active` grew much with clip control back (shadows and reflections drawn again: +1.9 and +0.46 ms); the
  rest was the ES profile's already. Splitting it (one gate at a time: layered casters, bindless, multi-bind,
  precisions) is C5's first measurement on the device, where these PC numbers stop meaning much (a desktop GPU
  pays nothing for highp; an Adreno pays for draw calls differently).

## 7. Installs for his approval (none done)

Sizes are approximate download sizes [I]; versions are the current stable ones at install time.

| Tool | Version | Size | URL | For |
|---|---|---|---|---|
| Android SDK command-line tools (or Android Studio, ~1.2 GB, bundles them and a JDK) | latest | ~150 MB | https://developer.android.com/studio#command-line-tools-only | sdkmanager |
| Android SDK Platform + Build-Tools + Platform-Tools (adb) | API 34 (the Quest's Android 14 base [?]; min SDK 29), Build-Tools 34.x | ~250 MB | via sdkmanager; https://developer.android.com/tools/releases/platform-tools | APK build, adb |
| Android NDK | r27 LTS or newer; a clang that builds Zancle (our PC build uses clang 22; check `-std=c++23` features against the NDK's clang) | ~700 MB zip, ~2.5 GB installed | https://developer.android.com/ndk/downloads | C/C++ for arm64-v8a |
| JDK 17 (Microsoft Build of OpenJDK, or Android Studio's) | 17 LTS | ~180 MB | https://learn.microsoft.com/java/openjdk/download | Gradle |
| Gradle (wrapper) + Android Gradle Plugin | 8.x | ~150 MB | https://gradle.org/releases/ | APK packaging |
| Khronos OpenXR loader for Android | 1.1.x, matching our headers | ~2 MB (AAR) | https://central.sonatype.com/artifact/org.khronos.openxr/openxr_loader_for_android | `libopenxr_loader.so` |
| Meta OpenXR SDK (headers for `XR_FB_*`/`XR_META_*`) | latest | ~50 MB | https://github.com/meta-quest/Meta-OpenXR-SDK | foveation, refresh rate, perf extensions |
| SDL2 source (Android Java + C) | 2.30.x, our PC SDL's version | ~8 MB | https://github.com/libsdl-org/SDL/releases | the Android activity, EGL, audio |
| Steam Audio (Android ARMv8 `libphonon.so`) | same 4.x as our PC copy | ~150 MB zip | https://github.com/ValveSoftware/steam-audio/releases | spatial audio on Quest |
| Android Emulator + an arm64/x86_64 system image (optional) | current | ~1.5 GB | via sdkmanager | C3 boot tests without a headset (no OpenXR) |
| Meta Quest Developer Hub (optional) | current | ~400 MB | https://developers.meta.com/horizon/downloads/package/oculus-developer-hub-win | install, logs, OVR Metrics, `ovrgpuprofiler` |
| glslangValidator (optional, for C2) | Vulkan SDK's or the standalone release | ~30 MB | https://github.com/KhronosGroup/glslang/releases | ES-validating every program on the PC |

Also: a Quest 3 (or 3S) in developer mode (Meta Horizon app, an organisation account), a USB-C cable.

Next phase: C3 — the Android build (NEEDS INSTALLS: section 7's SDK, NDK, JDK, Gradle, SDL2 source; waits for his
approval): CMake toolchain for arm64-v8a with `-ffp-contract=off`, the Gradle project and SDL2's activity, the first
compile of vr_android.cpp's `__ANDROID__` forms (and the crash handler merged with vr_crash.cpp's POSIX one, section
5d), logcat console, the first boot in the emulator (flat-screen, ES 3.x). If the installs are not approved yet, the
no-install work left is small: the `noperspective` fallback behind `-glesnoext` (section 5d), a CMake
`QVR_FAKE_ANDROID` configure of the Linux build to keep the layer compiling there too.

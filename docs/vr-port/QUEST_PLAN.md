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
  multi-bind (loops of binds), clip control (reversed Z: `EXT_clip_control` on Adreno: **yes**, C4b), layered shadow casters
  (`gl_ViewportIndex` in a VS: no ES form; the non-layered path exists).
- **Multiview:** `vr_stereo_single.cpp:122-146` (`GL_OVR_multiview2`, `glFramebufferTextureMultiviewOVR`) and the
  `QVR_MULTIVIEW` twins of 59 programs (`gl_shaders.c:183`): the Quest's natural path. It uses `glTextureView`
  (`vr_stereo_single.cpp:146`) and `glTexStorage3DMultisample` (`:141`) on the PC; on the Quest it renders into the
  swapchain's array texture directly (`OVR_multiview_multisampled_render_to_texture` [I]).

Not gated yet (the 14 rows `vr_gles_report` prints) [V counts]:

| Desktop-only use | Where (first line, count) | ES form |
|---|---|---|
| `#version 430` literals | 52 lines in 17 files: `gl_shaders.c:171` (engine prefix), `vr_gfx_gl.cpp:53` (12), `vr_bloom.cpp:56` (5), `vr_temporal.cpp:72` (4), `vr_upscale.cpp:49` (4), `vr_glutil.cpp:63`, `vr_visor.cpp:418`... | one prefix function: `#version 320 es`, `precision highp float; precision highp int;` and sampler precisions |
| `MultiDrawElementsIndirect` | `r_world.c:445` (bindless only), `vr_lighting.cpp:703` | `EXT_multi_draw_indirect`: not listed, its `...EXT` entry point is there (C4b); the loop |
| `glGetTexImage` | `gl_texmgr.c:541`, `vr_envmap.cpp:245`, `vr_pbr.cpp:352`, `vr_lighting.cpp:1732` | FBO + `glReadPixels` |
| `glGetBufferSubData` | `vr_gfx_gl.cpp:1515`, `vr_temporal.cpp:503`, `vr_water.cpp:1019` | `glMapBufferRange(READ)` |
| `glClearTexImage` | `vr_temporal.cpp:504` | clear through an FBO |
| `glTextureView` | `vr_envmap.cpp:53`, `vr_stereo_single.cpp:146` | render to array layers directly |
| `GL_TEXTURE_LOD_BIAS` | `gl_texmgr.c:169`, `vr_gfx_gl.cpp:44` | bias argument in the shader |
| `GL_DEPTH_CLAMP` | `r_alias.c:53` (view model) | `EXT_depth_clamp`: **yes** (C4b) |
| `gl_ClipDistance` | `gl_shaders.h:659`, `r_world.c:694`, `r_alias.c:776`, `vr_lighting.cpp:102` | `EXT_clip_cull_distance`: **yes** (C4b) |
| `noperspective` | `gl_shaders.h:641`, `vr_glsl_alias.h:378` | `NV_shader_noperspective_interpolation`: **yes** (C4b) |
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
| C2 (done: C2a, C2b, C2c, C2d: the ES cost split and cut, the Quest preset's draft, sections 6b-6c) | **PC portability layer**: the 14 ungated rows behind `VR_GLESCompat()` (one `#version`/precision prefix for all programs, `glGetTexImage`/`glGetBufferSubData`/`glClearTexImage`/`glTextureView` replacements, LOD bias in shaders, depth clamp, clip distance, noperspective, timer-query wrapper); the GLSL made ES-valid (checked by compiling every program as `#version 320 es` with the PC driver where it accepts ES profiles, else with glslang if he approves it); `-glescompat` runs every test suite clean; `-fp-contract`-free scalar check; a "no child process" mode for the relight and motion children; an `Android`-guarded skeleton for crash/files/paths that compiles on the PC (`QVR_FAKE_ANDROID`) | No |
| C2e (done, section 6d) | The CPU side of the Quest budget measured (a fight's split, QuakeC per function), draws per scope in `vr_profile_dump`, the bound path's draws located; the `noperspective` fallback (`-glesnoext`) | No |
| C2f (done, section 6e) | The world's textures in texture arrays for the bound path (`vr_world_texarrays`): a draw a set of textures alike instead of a draw a texture | No |
| C3 (done but the boot, section 6g; installs approved and done, section 7) | Android build: CMake toolchain for `arm64-v8a`, Gradle project (`Android/`), SDL2's Android activity, `-ffp-contract=off`, assets path and permissions, logcat console, the APK; first boot in the Android emulator (flatscreen, ES 3.x) for files, sound and menus (no emulator installed: the first boot is C4's, on the headset) | **Yes** (list below) |
| C4a (done, section 6h) | OpenXR on Android compiled and packaged without the headset: the loader AAR, its Android init, the ES binding and swapchains, the Quest's extensions behind availability checks, the manifest's VR lines, `Android/check_apk.sh` | No (section 7's) |
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
- **Resolved (C4b): 8 fragment, 23 combined image uniforms on the Quest 3's Adreno 740.** Was open: `GL_MAX_FRAGMENT_IMAGE_UNIFORMS` on Adreno (the world and model fragment shaders
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
- **Resolved by C4b's dump (section 6i)** (was pending: the Quest's extensions; fallbacks in C2c only for the missing ones):**
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

## 6b. C2d: the ES profile's cost split per switch, and cut (done, no installs)

**`-gleskeep <list>`** (`QVR_GLES_KEEP`; under `-glescompat` only): the listed switches keep their PC form for the
run (`bindless`, `multibind`, `layer`, `clip`, `glsl` = the ES GLSL and the widened image formats together, since the
shaders pick the format by `GL_ES`; `foveation`, `views`), so one bench run per switch measures each ES form's cost.
`bindless` needs `glsl` too (the bindless shaders do not compile as ES: 98 errors). Runs: `bench.sh quest`, 8 core
scenarios, `--hz 72 --eye 1760`, `quest_budget.cfg`, ThinLTO, exclusive, one repeat (a few % is noise); all ES runs
with clip control (`kit/benchresults/c2d_*`). GPU 3D ms averages:

| scenario | PC Quest-like | ES | keep glsl | keep bindless+glsl | keep multibind | keep layer | keep foveation | **ES + batch fix** | ES + fix + foveation |
|---|---|---|---|---|---|---|---|---|---|
| combat_48 | 1.75 | 2.43 | 2.83 | 2.32 | 2.32 | 2.28 | 1.53 | 2.26 | 1.64 |
| combined | 3.27 | 5.82 | 5.93 | 5.45 | 6.02 | 3.80 | 4.36 | 4.33 | 3.69 |
| gore_slash_32 | 1.81 | 2.54 | 2.75 | 2.78 | 2.40 | 2.37 | 1.66 | 2.57 | 1.85 |
| idle_e1m1 | 0.56 | 0.81 | 0.82 | 0.77 | 0.83 | 0.83 | 0.57 | 0.81 | 0.57 |
| lights_32 | 0.93 | 1.88 | 1.65 | 1.76 | 1.80 | 1.65 | 0.95 | 1.65 | 0.93 |
| props_500_active | 1.07 | 2.08 | 2.11 | 1.81 | 1.80 | 1.41 | 1.54 | 1.46 | 1.23 |
| ragdolls_32_active | 1.42 | 2.18 | 2.45 | 2.25 | 2.20 | 2.15 | 1.52 | 2.51 | 1.45 |
| teleporter_loop_r2 | 0.68 | 1.29 | 1.18 | 1.22 | 1.17 | 1.17 | 0.70 | 1.43 | 0.70 |

- **NVIDIA's shading-rate foveation off was most of it** (keep foveation: back to the PC's numbers, -27 to -46%: the
  world, water and translucent passes halve). Not an ES cost: the Quest has its own fixed foveated rendering
  (`XR_FB_foveation` on the swapchain, `QCOM_texture_foveated` on our eye targets: C4/C5), and the budget below
  counts it at the saving measured here.
- **The layered shadow casters off was the other big one** (combined: the shadow pass 0.09 -> 2.0 ms, 3025 draws),
  and it was not the layering: the face-at-a-time path drew each face's alias casters in list order, so the alias
  renderer broke its instanced batch at every change of model (a pile of 135 casters: 1806 draws, 1.16 ms of GPU for
  12 faces). **Fixed for every GPU** (PC fix, commit "Shadows: face-at-a-time alias casters drawn in batch order"):
  each face's casters sorted by model and skin, as the layered path's: 1025 draws, 0.11 ms; the same depth
  (`vr_shadow_layered_check`: 0 texels differ). The PC with `vr_shadow_layered 0` (GPUs without a viewport-layer
  extension) had the same 1.8 ms. After it the ES shadow pass costs what the layered one does (0.10 vs 0.09 ms).
- **ES GLSL, precisions and the widened formats (r32ui LUT, rgba32ui clusters, rgba8 TAA mask): nothing measurable**
  here (a desktop GPU runs highp as fp32 anyway; the Adreno's mediump question is C5's). **Multi-bind: nothing.**
  **Bindless off: -0.1 to -0.37 ms** where many textures draw (props, combined; draws 1672 -> 1505), within noise
  elsewhere. **The multi-draw loop:** no caller under ES (bindless and the layered casters are off there).
  **NIS -> FSR:** no upscale at scale 1 (and the Quest preset uses none). **Texture views:** single-pass stereo is off
  by default and the envmap is off in the Quest preset.
- **What is left of the ES profile** (ES + fix + foveation against the PC's Quest-like): 0 to +15% (combined +13%,
  props +15%: the bound path's draws, 3.4x the PC's in an idle e1m1, 96 -> 330). Candidates, with their ES-legal
  forms, for C5 on the device where draw calls cost the driver more CPU: the world's and the models' textures in
  arrays by size (one bind for a batch of surfaces, `sampler2DArray` + a layer per draw: the bound path's one draw a
  texture becomes a draw per array), and the layered casters as instanced faces (a face per instance, its tile chosen
  by a clip-space offset and four `gl_ClipDistance` planes in the vertex shader: needs `EXT_clip_cull_distance`, and
  ES's `glDrawElementsIndirect` has no base instance, so the face index from `gl_InstanceID` and a uniform).
- **The CPU** did not move with any switch (within one repeat's noise).

## 6c. C2d: the Quest preset's draft and the expected budget (C6 groundwork; no installs)

**`vr_quest_preset <0|1|2|mine>`** (vr_gfxpresets.cpp, a second table on the presets' machinery; Debug > Startup:
"Quest Preset: Quest 3 72 Hz"): the Low graphics preset, then 27 rows, at Quest 3 72 Hz, Quest 3 90 Hz, Quest 2 72 Hz;
`mine` sets back what was there (`quest_my_settings.cfg`, then the graphics one). **Only under `-glescompat` or on
Android**: on the PC it refuses and changes nothing (checked: the PC's values stay their defaults). Bench settings:
`Misc/quakevr/bench/quest_preset_quest{3_72,3_90,2_72}.cfg` (dynamic resolution off there: the level's own scale).

| rows | Quest 3 72 Hz | Quest 3 90 Hz | Quest 2 72 Hz |
|---|---|---|---|
| `vr_render_scale` (of the runtime's eye size) / `vr_dynres` 1, `vr_dynres_min` | 0.9 / 0.65 | 0.75 / 0.6 | 0.65 / 0.55 |
| `vr_upscale` (0: bilinear, the compositor scales) / `vr_taa` | 0 / 0 | 0 / 0 | 0 / 0 |
| `vid_fsaa` (on a tiler MSAA resolves on chip; C5 measures 4x) | 2 | 0 | 0 |
| shadowed dynamic lights / map lights / atlas / distance | 1 / 0 / 2048 / 768 | 1 / 0 / 2048 / 640 | 0 / 0 / 2048 / 512 |
| bloom (the tone curve and grade kept: the look) | default | off | off |
| heat haze, water reflections, rim light (the envmap) | off | off | off |
| torch, lava nail, explosion debris lights | 2, 2, 2 | 2, 2, 2 | 1, 1, 0 |
| particles: `vr_particle_max_screen`, half-res | 0.08, on | 0.06, on | 0.05, on |
| decals: `vr_decal_max`, bucket | 256, 48 | 192, 32 | 128, 32 |
| bodies: ragdolls, small gibs, limbs, explosion debris, dents | 4, 32, 12, 32, 6 | 3, 24, 8, 24, 4 | 3, 16, 8, 16, 4 |

The Low preset under it: no normal or parallax maps, detail, AO, soft particles, portals, refraction, caustics.
Draw distance: none of its own (Quake's maps are small and PVS-culled; the shadow distance is the one that matters).
Light probes (`vr_model_light_grid`): kept (a lookup a model, CPU).

**Expected budget.** The PC's numbers under the ES profile (`c2d_p2_*`; ms averages, both eyes at 1760, 1832 for
the Quest 2), times the share fixed foveation leaves (measured in 6b: ES with and without the PC's foveation; the
Quest's FFR taken as saving the same [I]), times the GPU ratio, 12-20x for the Quest 3's XR2 Gen 2 against this RTX
4090 and 30-50x for the Quest 2's XR2 [I: public benchmark ratios; a tiler's bandwidth savings and a desktop's fixed
per-pass overheads pull both ways]; the CPU (main thread busy) times 3-4x (Quest 3) or 4-5x (Quest 2) per core [I].
Against 85% of the frame (11.8 ms at 72 Hz, 9.4 ms at 90 Hz; the rest the compositor's [I]): ok, edge (the range
straddles it), over.

| scenario | FFR share | Q3 72: GPU PC, Quest | CPU PC, Quest | Q3 90: GPU PC, Quest | CPU PC, Quest | Q2 72: GPU PC, Quest | CPU PC, Quest |
|---|---|---|---|---|---|---|---|
| idle_e1m1 | 0.71 | 0.84, 7-12 edge | 1.70, 5-7 ok | 0.58, 5-8 ok | 1.53, 5-6 ok | 0.54, 11-19 edge | 1.62, 6-8 ok |
| teleporter_loop_r2 | 0.49 | 1.14, 7-11 ok | 0.86, 3 ok | 0.75, 4-7 ok | 0.91, 3-4 ok | 0.68, 10-17 edge | 0.87, 3-4 ok |
| lights_32 | 0.57 | 1.54, 10-17 edge | 1.33, 4-5 ok | 1.15, 8-13 edge | 1.35, 4-5 ok | 0.91, 15-26 over | 1.37, 5-7 ok |
| combat_48 | 0.72 | 1.96, 17-28 over | 4.00, 12-16 over | 1.60, 14-23 over | 3.80, 11-15 over | 1.15, 25-41 over | 3.78, 15-19 over |
| gore_slash_32 | 0.72 | 1.83, 16-26 over | 3.68, 11-15 edge | 1.31, 11-19 over | 3.52, 11-14 over | 1.46, 31-52 over | 3.88, 16-19 over |
| ragdolls_32_active | 0.58 | 1.79, 12-21 over | 3.71, 11-15 edge | 1.31, 9-15 edge | 3.27, 10-13 over | 1.10, 19-32 over | 3.72, 15-19 over |
| props_500_active | 0.84 | 1.86, 19-31 over | 4.11, 12-16 over | 1.64, 17-28 over | 3.75, 11-15 over | 1.40, 35-59 over | 3.57, 14-18 over |
| combined | 0.85 | 3.62, 37-62 over | 7.06, 21-28 over | 2.45, 25-42 over | 5.89, 18-24 over | 2.03, 52-87 over | 6.29, 25-31 over |

- **Quiet play fits the Quest 3 at 72 Hz and at 90 Hz** (e1m1, the teleporters; the 32 lights at the edge), with
  dynamic resolution's room under it (0.65 of the scale: about 0.4 fewer pixels). **The bench's fights do not**: they
  are stress tests (48 grunts, 32 ragdolls, 500 live props, everything at once), 1.5-5x over on the GPU and up to 2.4x
  on the CPU. A Quest-sized fight (8-12 monsters) needs measuring on the device (C5/C6) with the caps above; the CPU
  side (game logic, Box3D, gore) is as much the Quest's problem as the GPU and has no Quest work yet.
- **The Quest 2 at 72 Hz** fits only the quietest scenes; its level needs a further step down (scale 0.55, fewer
  dynamic lights) once C6 measures it. The Quest 3 is the target.
- **Biggest GPU levers left**, in order [I]: the eyes' pixel count (scale, dynamic resolution, FFR's level), the post
  chain merged into fewer passes (the float scene's tone curve, bloom, grade: each a full-screen read and write on a
  tiler, C5), MSAA resolved on chip (`EXT_multisampled_render_to_texture`), the bound path's draws (6b).
- A first draft (scales 1 / 0.85 / 0.75, `c2d_p_*`) measured 0.91 / 0.67 / 0.61 ms on idle e1m1; the scales were
  lowered to the ones above from it.

## 6d. C2e: the CPU side, the bound path's draws, the noperspective fallback (done, no installs)

**The `noperspective` fallback** (section 5d's open item): the ES header defines `QVR_NOPERSP_W` when the driver lacks
`NV_shader_noperspective_interpolation`, and always under **`-glesnoext`** (`QVR_GLES_NOEXT=1`: an ES driver without
the optional extensions that have a fallback; `vr_gles_report` says so). The four sites (the light tile coordinate
in the world's and the models' shaders, the retro affine texture coordinate of `r_softemu_mdl_warp`) then go out
times `gl_Position.w` (after its last write), perspective-interpolated, and come in times `gl_FragCoord.w`
(`#define in_coord (in_coord_w * gl_FragCoord.w)`): the screen-linear value. PC unchanged (the macro is ES-only).
`gles_render_test` (now 12 checks): the ES scene with `r_softemu_mdl_warp 1` drawn with and without the extension,
**identical** (0 pixels moved, mean 0.000); the fallback without its `gl_FragCoord.w` fails it (0.0002 moved, mean
0.81), so the check sees a wrong form. The clip-distance fallback and the standard-depth path stay open (C4's
extension dump decides whether they are needed).

**Draws per scope** (`vr_profile_dump`'s new `draws` column: each scope's draw calls a frame, its children's in).
Idle e1m1 (`vr_profile` over 240 frames; PC defaults, then ES + clip control + `vr_quest_preset 0`):

| scope | PC draws | ES + Quest preset draws |
|---|---|---|
| eye L / eye R | 89 / 33 | 151 / 150 |
| world+brush (an eye) | 2 | **118** |
| alias (eye L) | 13 | 13 |
| map light shadows | 55 | 0 (the preset: no map light shadows) |
| 2D | 28 | 28 |

The ES profile's extra draws are **the world's bound path alone**: one `glDrawElementsIndirect` a texture batch (118
an eye here) where bindless makes one multi-draw. The models' batches are the same on both paths (bindless never
changed them). The ES-legal cut is the world's textures in arrays (section 6b: arrays by size and format, the
batch's layer in its call record, a bind per array), the next no-install phase; not done here: it touches every
world texture's upload (mips, external packs' sizes, normal and specular maps, the retro sampling's `textureSize`),
a phase of its own with its tests. The layered casters' instanced form is not needed by the Quest preset as drafted
(map light shadows off; one shadowed dynamic light).

**The CPU side** (the question section 6c left: the bench's fights over the Quest's estimated CPU). combat_48's
set-up (48 mixed monsters on vrfiringrange, god mode), 90 frames in, then 240 frames of `vr_profile` and
`profile_qc` (`vr_qcprofile 1`), ES + clip control + `vr_quest_preset 0` (Quest 3 72 Hz), the dev build (the
profilers' own overhead in: relative shares, not the ThinLTO's ms). Main thread 4.41 ms a frame:

| part | ms | share | what |
|---|---|---|---|
| QuakeC | 1.80 | 41% | **the armed humanoids' weapon props: `VR_AG_PropThink` 1.18 incl. (24 posed a frame: `VR_AGP_Pose` 0.77, the parry 0.18, the blows 0.18)**; AI and navigation 0.1; the rest small |
| alias models (both eyes' instance set-up) | 0.59 | 13% | eye L 0.46 (its max 5.2: a hitch frame), eye R 0.13 |
| client relink: armed arms | 0.36 | 8% | `VR_RelinkArmedGrunts`: their weapons on the drawn hands |
| view entities | 0.31 | 7% | |
| Box3D | 0.18 | 4% | step 0.07, sync 0.08 |
| the rest | 1.17 | 27% | shadows 0.11, particles 0.13, text3d and blob shadows 0.11, send 0.10, ... |

Idle e1m1 (23 monsters asleep, 26 armed props): QuakeC 1.19 of the frame's 1.71 ms, `VR_AG_PropThink` 0.82 of it
(each prop posed every server frame, asleep or not: the hands follow the interpolated stand frames). **The armed
humanoids are the Quest's CPU budget**: 1.5 ms of 4.4 in the fight (QC props + the client's relink), more than all
rendering's CPU. Nothing cut here: every candidate changes behaviour or needs his call (they are a key feature):
- an identical cache of `VR_AM_Row` (423 calls a frame, 0.1 ms: a kind's table row refilled each call) needs every
  cvar it reads to stay still within a frame (QC's `cvar_set` can change them): left;
- a sleeping or unseen holder's props posed less often, or fewer armed replacements on the Quest (a Quest preset
  row): behaviour changes, for him and C6 on the device;
- `findradius` (71 a frame, 0.09 ms: a scan of every edict each) through the area grid, sorted back to edict order:
  identical, 2%: not worth a change before the device says where the ARM cores' time goes.
- The `upscale` pass (0.21 ms GPU at the preset's 0.9 scale, `vr_upscale 0`) is the compositor's job on the Quest
  (the swapchain at the scaled size): C4.

## 6e. C2f: the world's textures in texture arrays for the bound path (done, no installs)

**`vr_world_texarrays`** (a tunable, Debug > Tunables: 0 off, **1 under the ES profile** (`-glescompat`, Android: the
default), 2 on any bound path; the PC with bindless textures never uses it) and `vr_world_texarrays_report` (the sets,
their memory, the last frame's draws). vr/vr_worldarrays.cpp; r_world.c `R_MergeBModelCallsArrays`,
`R_DrawBModelCallsArrays`; gl_shaders.c `GL_WorldArraysProgram`.
- **The arrays**: at the map's load (the first batch after `GL_BuildBModelMarkBuffers`), every world texture but the sky
  and the liquids, with its four maps as `R_AddBModelCall` picks them (shared code: `R_BModelCallTextures`), goes in a
  set of four `GL_TEXTURE_2D_ARRAY`s (the texture, its fullbright, its normal map, its specular map; units 0, 1, 3, 11)
  for the textures alike in size, mip levels and format: a texture a layer, the same layer in all four. A map shared
  by the set's textures (the black of no fullbright, the flat normal, the white of no specular map: 64 texels or fewer,
  or one map used by more than one texture) is a one-layer array: the shader's layer clamps to it, so its
  `textureSize` is the bound path's. Copies by `glCopyImageSubData` (ES 3.2 core): every mip level, external packs'
  sizes and compressed formats, the generated normal maps as made; TexMgr's unsized formats (`GL_RGB`, `GL_RGBA`) given
  their sized storage. Filtering and wrapping copied from each array's first map (again when `TexMgr_SetFilterModes`
  runs: `gl_texturemode`, anisotropy). An OpenGL error while making them: none (a console line), the bound path.
- **Checked at every call**: the call's four maps (pointers and GL names) must be the ones copied, else it draws on the
  bound path that frame and the arrays are made again at the next batch (a setting changed a map: normal maps on, an
  external pack, `gl_fullbrights`; at most 16 times a map).
- **The draws**: the world programs' array twins (the same sources with `QVR_WORLD_ARRAYS`: `sampler2DArray`s, the
  call's layer a flat varying; the texture-reading helpers, retro, parallax and bumps, spliced for both sampler types:
  `QVR_SAMPLER_TWINS` in vr_retro.h; the liquids' programs have none). A flush groups its calls by set (one group for
  the depth pre-pass, which reads no texture); each group's single-instance calls become one draw: a compute pass
  beside the gather (under its barrier: no compute pass of its own between draws, which on a tiler may end the render
  pass) copies their gathered indices one after another, each vertex finds its call from its command (a static
  vertex-to-command buffer from r_brush.c, the flush's command-to-call table: `DrawID` -1). Instanced calls, and a
  command's second call in a flush, draw alone from the arrays (`DrawID` set). Order: by set, then as before within one.

Results (idle e1m1; ES + clip control + `vr_quest_preset 0`; the dev build, `vr_profile` over 240 frames,
`scratch/c2f_prof.sh`):

| | arrays off (a draw a texture) | arrays on |
|---|---|---|
| world+brush draws an eye | 118 | **25** |
| eye L / eye R draws | 151 / 150 | **58 / 57** |
| world arrays | | 25 sets, 77 textures (2 left out: tiny or one image under two names, on the bound path), 8.9 MB |
| the left eye's image | | **identical** (0 pixels moved) |

GPU and CPU (`bench.sh quest`, the 8 core scenarios, `--hz 72 --eye 1760`, `quest_preset_quest3_72.cfg`, ES + clip
control, ThinLTO, exclusive, one repeat: a few % is noise; `kit/benchresults/c2f_q3_off`, `c2f_q3_on`). ms averages:

| scenario | GPU 3D off | on | CPU off | on |
|---|---|---|---|---|
| combat_48 | 1.80 | 1.87 | 3.92 | 5.81 (one 1.8 s frame: below) |
| combined | 3.41 | 3.49 | 6.89 | 7.18 |
| gore_slash_32 | 1.86 | 1.87 | 3.80 | 3.87 |
| idle_e1m1 | 0.84 | 0.83 | 1.74 | 1.81 |
| lights_32 | 1.65 | 1.60 | 1.40 | 1.40 |
| props_500_active | 1.73 | 1.80 | 3.66 | 3.82 |
| ragdolls_32_active | 1.83 | 1.84 | 3.71 | 3.67 |
| teleporter_loop_r2 | 1.16 | 1.16 | 0.84 | 0.85 |

- **This PC's GPU and CPU do not move** (-3 to +4%, the noise of one repeat): a desktop driver's draw call is cheap,
  and the merge's copy of the indices (a few tens of thousands an eye) is too. What the arrays cut is the Quest's
  draw calls (118 -> 25 an eye; ES drivers spend CPU per draw and per bind): C5 measures that on the device. A first
  form with the merge's own barrier cost +0.04 ms GPU an eye (0.10 -> 0.14, the dev build); beside the gather's: 0.11.
- **combat_48's 1.8 s frame** was an array twin compiled in play (the decals' retro program's, made when the first
  retro decal lands): the twins of the programs the view's settings use are now compiled when the arrays are made at
  the map's load (`R_PrewarmArrayTwins`). Rechecked after it, two repeats each (`c2f_idle_off`, `c2f_idle_on`):
  combat_48 GPU 3D 1.98 off, 1.92 on, CPU 4.23, 4.10, the worst frame 14.5 ms both; idle_e1m1 0.906, 0.907 GPU,
  1.77, 1.69 CPU.

- **Equality**: `gles_render_test` (now 18 checks) runs the ES scene (PC defaults: normal maps, PBR companions, retro
  textures on) with the arrays and with `vr_world_texarrays 0`: identical (0 pixels moved, mean 0.000), every texture
  the same; the ES scene against the PC's as before. Under the Quest preset (no normal maps) the same: identical.
- **Open, for the device (C4/C5)**: the vertex shader reads five storage blocks (the Quest's
  `GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS`: C4's limit dump); the arrays are copies (8.9 MB in e1m1 under the preset,
  15 MB with normal maps): the 2D world textures could be freed on the Quest once nothing else reads them (brush models
  of other files, the bound path's leftovers still do); the liquids still draw a texture at a time (few).
- **C2f's skipped checks, run in C2g** (merged with `origin/next` 4c4ac5b3f): PC scenario_check 129/0,
  load_models_test 4/0, vr_simd_test 58880/0; the release configuration (MSBuild ReleaseLTO,
  `-p:QvrReleaseVersion=1.1.0`) 0 warnings, 0 errors.

## 6f. C2g: the eyes' post chain under the Quest preset (done, no installs)

**The premise of 6c's lever was wrong**: the tone curve, the grade, bloom's composite, the underwater wobble and the
dither are already one full-screen pass (`GL_PostProcess`: one read of the float scene, one write). The eye's
full-screen passes under the Quest 3 72 Hz preset are: the MSAA resolve (`vid_fsaa 2`; "resolve/warp"), bloom's first
downsample (reads the scene, writes a quarter), the post-process, and **the bilinear resample** (render scale 0.9,
`vr_upscale 0`: the post-process's target scaled into the runtime's image, a full read and write; "upscale").
- **`vr_eye_subimage`** (a tunable, Debug > Tunables "Eye Sub-Image": 0 off, **1 under the ES profile** (`-glescompat`,
  Android: the default), 2 always; the PC never uses it at 1): an eye drawn smaller than its image and resampled
  bilinearly (`upscale::resamplesBilinearly`, shared with `upscale::resample`) is post-processed straight into the
  image's lower left part, and the projection layer shows that part (`Backend::setEyeImageRect`: OpenXR's
  `subImage.imageRect`; the mock accepts it). The compositor's own sampling (it resamples the image for the lenses
  anyway) scales it: **the resample pass is gone** (one full-screen read and write an eye; on the Quest 3 at 0.9, about
  1858 x 1987 x 4 bytes read and written twice a frame). The UI, the foveated debug overlay and the final eyeshot are
  drawn at the part's size (the UI's text at the eye's resolution, not the image's: the trade, C5 to judge).
- **Equality**: `gles_render_test` (now 21 checks): under the Quest preset (dynamic resolution off: a fixed size) the
  eye's part (922 x 922 in the mock) is **identical** (0 pixels differ) to the same scene's resample target with it off
  (`vr_eyeshot 2`); the PC at `vr_render_scale 0.9`, bilinear, still fills its image (1024 x 1024).
- **Cost** (idle e1m1, ES + clip control, the preset with `vr_dynres 0`, the dev build, `vr_profile` 240 frames,
  `scratch/questN_prof.sh`): the "upscale" scope (0.07 + 0.03 ms GPU a frame, both eyes) is gone; the rest within noise.
  On this PC that is small; on a tiler the pass is bandwidth (C5 measures it).
- **Not merged, and why**: bloom's downsample needs the whole scene before the post-process reads its result (a pass
  boundary on any GPU); the MSAA resolve is the tiler's own with `EXT_multisampled_render_to_texture` (resolved on chip,
  no pass: C4/C5, the device's extension).
- **Open, for the device (C4)**: OpenXR's `imageRect` for an OpenGL ES swapchain: the part drawn from the lower left (GL's
  origin); if the Quest's runtime reads the offset from the top, the offset is `imageHeight - height` (one line in
  vr_backend_openxr_frame.cpp; the image shows the eye's top cut off and black at the bottom if wrong).

**A PC hitch found on the way (fixed, its own commit bd2277d6c, may ship to `next`)**: C2f's 1.8 s frame (an array
twin made in play) has a PC cousin on `origin/next`: the translucent brush models' program with the decals' retro code
(`GL_WorldDecalRetroProgram`'s OIT variant, made by `R_ChooseBModelProgram` when a translucent brush model is first
drawn with a retro mark about; ROUND22.md left it lazy). Measured in play with the new developer line "GL program made
in play": **2.5-3.1 s** in ad_tears, ad_crucial and ad_necrokeep (19 of AD's maps have translucent brush models).
Now made in the load's last frame (`VR_SignonPrewarm` -> `decals::prepareTranslucent`, when a baseline brush model is
translucent and OIT is in effect): none in play after it; the load pays 7 ms with the driver's cache, up to 2.9 s
without (AD's start map, behind the loading screen).

## 6g. C3: the Android build (done but the first boot: no emulator; the installs of section 7)

- **The build** (`Android/README.md`): the top-level `CMakeLists.txt` under the NDK's toolchain includes
  `Android/android.cmake`: the engine as **`libquakevr.so`** (a shared library SDL2's Java activity loads; its
  `SDL_main` is `main_sdl.c`'s `main`), SDL2 2.30.9 built from its source as `libSDL2.so`, GLES 3.2 + EGL, liblog;
  **`-ffp-contract=off` on every target** (his approval; `compile_commands.json`: 793 of 793 files; the library's
  disassembly, 3.63 M lines: **0** `fmadd`/`fmsub`/`fmla`/`fmls`); `-Wl,--no-undefined`; mimalloc, user directories,
  codecs, curl and OpenXR off (OpenXR: C4). `Android/build_native.sh` builds it with the SDK's CMake 3.22.1 and Ninja.
- **The first arm64 compile of every source** (NDK 27.2, clang 18.0.3, libc++ 18, API 29): 877 objects; what didn't
  compile, fixed with the PC unchanged: Zancle's `reference_converts_from_temporary` (neither the builtin nor the trait
  in clang 18: `__reference_binds_to_temporary`) and its interference sizes (no `__GCC_DESTRUCTIVE_SIZE`); the one
  deducing-this lambda (`vr_climb.cpp`: clang 18 "cannot compile this l-value expression yet"); `__rdtsc` in
  `vr_findradius.cpp`'s stats (the monotonic clock off x86, as `pr_exec.c`); a format call without arguments
  (`-Wformat-security`, an NDK error). The link: `glGetTexImage`/`glPolygonMode` (desktop-only, never reached under the
  ES profile) compiled out on Android; warnings: 0 of ours (a fresh build: SDL2's own `ASensorManager_getInstance`
  deprecation; Zancle's GCC-only `-Wclass-memaccess` pragma now kept from clang), and **a PC bug: `VR_CrtHeapLoadHold` had no definition without mimalloc** (the
  Linux build with `-DQVR_MIMALLOC=OFF` failed to link; its own commit). vr_android.cpp's `__ANDROID__` forms compiled.
- **Android only**: an OpenGL ES 3.2 context (`gl_vidsdl.c`), the ES profile always on (`VR_GLESCompat`, without
  `-glescompat`), `vr_quest_preset 0` after `quake.rc` on the first run (no `ironwail.cfg` in the game folder yet:
  `VR_AndroidFirstRun`), the console in **logcat** (`Sys_Printf`, `Sys_ReportError` through `VR_AndroidLog`: the
  pieces joined into lines, tag `QuakeVR`; `adb logcat -s QuakeVR SDL`).
- **One crash handler** (5d's choice, the reuse rule): `vr_crash.cpp`'s POSIX handler runs on Android (sigaltstack,
  dladdr names, the map/GPU/VR lines, one report a process), its stack from `_Unwind_Backtrace` there (bionic has
  `backtrace` from API 33 only; Linux keeps glibc's, the same frames), the report into logcat as well, then the
  default action (the system's tombstone). vr_android.cpp's own handler is gone: `VR_AndroidStart` points the reports
  at `<base>/quakevr/crash`. `vr_android_test` (`-fakeandroid`): 12 passed, 0 failed (the folders, the reports' folder
  being the layer's, logcat's lines, the first-run test).
- **The APK** (`Android/package_apk.sh`: javac against android.jar + core-lambda-stubs, d8, aapt2, zipalign,
  apksigner with a debug key made once; no Gradle plugin download): `QuakeVR.apk` **20.2 MB**, `apksigner verify` OK;
  `aapt2 dump badging`: `com.vittorioromeo.quakevr`, min SDK 29, target 32, GLES 0x30002, launchable
  `QuakeVRActivity`, native code arm64-v8a. Inside: `libquakevr.so` 24.8 MB stripped (132.8 MB with its debug info,
  kept in `build-android/` for addr2line), `libSDL2.so` 1.4 MB, Steam Audio's `libphonon.so` 20.5 MB (optional),
  `classes.dex` (SDL2's activity and ours). The game's files: the app's external files folder, no permission.
- **The Gradle project** (`Android/settings.gradle`, `Android/app/build.gradle`: AGP 8.7.3, the same CMake project,
  SDL2's Java from its source, `libphonon.so` copied in when `qvrSteamAudioDir` is set): **not run**: its first run
  downloads the Android Gradle Plugin and its dependencies from Google's Maven (section 7, his approval);
  `package_apk.sh` makes the same APK without it.
- **Not done**: the first boot (no Android emulator installed; on the Quest the APK runs as a flat panel: C4's first
  step before OpenXR); codecs (music: WAV only), the Android runtime checks of the `[?]` rows.

## 6h. C4a: OpenXR on Android, compiled and packaged (done without the headset; no installs)

- **The loader**: Khronos' 1.1.53 AAR, unpacked (`F:\QuakeVRDev\openxr\aar`): `Android/android.cmake` links its
  `jni/arm64-v8a/libopenxr_loader.so` when `QVR_OPENXR_ANDROID_DIR` names the folder (else the mock backend alone, as
  in C3), defines `QVR_HAVE_OPENXR` and copies the loader beside `libquakevr.so`; `package_apk.sh` puts it in the APK;
  the Gradle module the same (`qvrOpenXrDir`; not run). Headers: the engine's own `Windows/OpenXR/include` (1.1.63, the
  PC's; the loader's 1.1.53 speaks the same API, the instance asks for 1.0).
- **The loader's Android init and instance** (`vr_backend_openxr.cpp`, `__ANDROID__`): `xrInitializeLoaderKHR` with
  `XrLoaderInitInfoAndroidKHR` (SDL's JavaVM, `SDL_AndroidGetJNIEnv`, and its activity, `SDL_AndroidGetActivity`,
  held as a global reference), once a process, before any other OpenXR call; the instance with
  `XR_KHR_android_create_instance` (`XrInstanceCreateInfoAndroidKHR`, chained after the debug messenger when that is
  on, and kept on the retry without it) and `XR_KHR_opengl_es_enable` instead of `XR_KHR_opengl_enable`;
  `xrGetOpenGLESGraphicsRequirementsKHR` (logged against the context's `GL_VERSION`).
- **The binding**: `XrGraphicsBindingOpenGLESAndroidKHR` from SDL's EGL display, config and context, found as on Linux
  (`vr_xr_linuxgl.cpp`: EGL alone on Android, its GLX parts compiled out; the same `contextReady` checks). Swapchains:
  `XrSwapchainImageOpenGLESKHR` (`SwapchainImageGl`, the PC's `XrSwapchainImageOpenGLKHR`: the same fields), the same
  format choice (`GL_SRGB8_ALPHA8` first: an ES 3 format). **Multiview: not used** (the renderer draws each eye into
  its own target and copies it into that eye's swapchain; a two-layer swapchain drawn in one pass,
  `GL_OVR_multiview2`, is a renderer change: C5); the session's log lists the driver's `GL_OVR_multiview2`,
  `GL_OVR_multiview_multisampled_render_to_texture`, `GL_EXT_multisampled_render_to_texture`,
  `GL_QCOM_texture_foveated`, `GL_EXT_sRGB_write_control` for C4b.
- **The Quest's runtime extensions** (`vr_backend_openxr_quest.cpp`, `QuestExtensions`; requested on Android alone,
  each when offered: the PC's runtimes never get them): `XR_FB_display_refresh_rate` (`vr_xr_refresh_rate`, the nearest
  rate offered), `XR_EXT_performance_settings` (`vr_xr_cpu_level`, `vr_xr_gpu_level`: 1 power savings .. 4 boost),
  `XR_FB_foveation` + `XR_FB_foveation_configuration` + `XR_FB_swapchain_update_state` (`vr_xr_foveation` 1-3: the
  eye swapchains made foveation-capable, `SCALED_BIN`, a fixed profile applied, again for new eye swapchains). All
  four cvars default **0, the runtime's own** (C6 sets the Quest preset's from measurements); applied while the session
  runs, again when changed (a compare a frame). All in Khronos' headers: Meta's SDK (`F:\QuakeVRDev\Meta-OpenXR-SDK`)
  adds only preview extensions (`meta_openxr_preview/`), none needed here.
- **The manifest**: the VR category (`com.oculus.intent.category.VR`), OpenXR's
  `org.khronos.openxr.intent.category.IMMERSIVE_HMD`, `android.hardware.vr.headtracking` (required), focus awareness,
  the loader's permissions (`org.khronos.openxr.permission.OPENXR`, `_SYSTEM`) and `<queries>` (the runtime broker,
  the runtime's and API layers' services: the AAR's own manifest, which no Gradle merge brings in here),
  `resizeableActivity` false. Those VR lines are marked `vr-only`: `QVR_ANDROID_FLAT=1 Android/package_apk.sh` leaves
  them out, the flat-panel APK for C4b's first step.
- **The check without a device**: `Android/check_apk.sh [apk] [vr|flat]`: the package, arm64, GLES 3.2, the activity,
  the three libraries in the APK, the loader's permission and queries, the 4 VR lines (none in the flat APK),
  `libquakevr.so` needing `libopenxr_loader.so` and importing `xrGetInstanceProcAddr`, `xrCreateInstance`,
  `xrCreateSession`, `xrCreateSwapchain` and SDL's JNI calls. **VR APK 18 passed, 0 failed; flat APK 18/0** (the VR
  APK checked as flat: 17/1, the check tells them apart).
- **Results**: the arm64 build with OpenXR: 0 warnings, 0 errors (`-Wl,--no-undefined`: every OpenXR, EGL and JNI
  symbol resolved); `QuakeVR.apk` 20.7 MB (`libopenxr_loader.so` 1.6 MB added), `apksigner verify` OK; the flat APK
  without Steam Audio 11.6 MB. The PC unchanged (the Quest extensions never requested there, its swapchain create info and image structs as
  before): scenario_check 129/0, load_models_test 4/0, vr_simd_test 58880/0, the Release build 0 warnings (the
  renderer untouched: no `-glescompat` rerun).
- **Left for C4b (the headset)**: the runtime plan on Android (`vr_xr_runtime.cpp`: no manifests there, so Auto's
  "no runtime installed" attempt, the loader's own choice: the broker; its label reads "none"), the session's states
  with SDL's activity (pause/resume, focus), the swapchain formats and the extension dump, whether `SCALED_BIN`
  foveation suits the eyes' copy, the `imageRect` origin (6f).

## 6i. C4b: the headset's first boot (done over adb, the headset unworn; no installs)

The device: Quest 3 (`eureka`), Android 14 / SDK 34, Horizon OS runtime "Oculus 210.117.0", Adreno 740, GL_VERSION
"OpenGL ES 3.2 V@0837.0.9". Everything over adb (`Android/README.md`, "Install and run"); the headset lay unworn,
so it slept after ~15 s and the app paused (SDL blocks in its pause): each run woke it every 5 s
(`input keyevent KEYCODE_WAKEUP`), no setting changed. **Nothing was looked at in the headset yet** (screencap shows
the sleeping compositor).

- **Game data**: his `PAK0.PAK`/`PAK1.PAK` as `id1/pak0.pak`/`pak1.pak` (51 MB; the case matters on Android) and the
  quakevr folder (git's files less `xr_layers`, `doom`, `sound_tests`, `.map`s, plus `progs.dat`: 4054 files,
  382 MB), into `/sdcard/Android/data/com.vittorioromeo.quakevr/files`. Found: adb's `push` of a folder fails part-way
  there (`secure_mkdirs`): a tar stream (`adb exec-in tar -x`); what adb pushes is the shell's and the game couldn't
  write (`quakevr/crash`: the first fatal error): chmod'ed; what the game makes is 0660 (FUSE), unreadable to adb.
- **Fixed on the way (Android only unless said)**: the base folder taken from the cwd ("/": `Sys_Init` overwrote
  `VR_AndroidStart`'s); `-game quakevr` not given (the PC's launch line: the activity adds it); the GL version check
  ("OpenGL ES 3.2 ..." parsed, ES 3.2 required); desktop entry points missing in ES taken by their `EXT`/`OES` names
  (`glMultiDrawElementsIndirectEXT`, `glViewportIndexedfOES`, `glMapBufferOES`, the `EXT` timer queries; none left
  missing); Adreno's GLSL: no `textureQueryLod` (the screens' shader: the level from the derivatives under `GL_ES`),
  **macros without parameters a syntax error** (`SCREEN_SPACE_NOISE()`, `SUPPRESS_BANDING()`: now functions, PC
  too, the same code), no program without a fragment shader (the sky stencil, the world's depth pass, the shadow
  depth program: an empty one under the ES GLSL form); `-glesverbose` reads Adreno's error lines; the OpenXR log
  (`qvr_openxr.txt`) into logcat too; the runtime chooser's label ("Auto: the system's runtime - Android: the
  loader's runtime broker (a Quest: Meta's)", was "none - no runtime installed"); `package_apk.sh` with a relative
  output folder, and one debug key for both APKs (an uninstall would delete the game data).
- **The flat APK**: boots to the console, the menu, e1m1 (logcat), sound (AAudio, 44.1 kHz), the crash report
  (`quakevr/crash`, logcat). It starts OpenXR too (the loader is in it; vr_enabled).
- **The VR APK**: the loader's init, `XR_KHR_opengl_es_enable`, the ES session, the swapchains, the session's states
  through sleep and wake (READY, FOCUSED, VISIBLE, STOPPING, READY again: SDL's pause/resume) all work; e1m1 runs.
  - The runtime: **75 extensions** (all in the log: `adb logcat -s QuakeVR`); enabled: `XR_KHR_opengl_es_enable
    XR_KHR_android_create_instance XR_EXT_performance_settings XR_EXT_debug_utils XR_FB_display_refresh_rate
    XR_FB_swapchain_update_state XR_FB_foveation XR_FB_foveation_configuration XR_META_touch_controller_plus
    XR_KHR_visibility_mask`. Offered and of use later: `XR_FB_space_warp`, `XR_META_recommended_layer_resolution`,
    `XR_FB_composition_layer_settings` (sharpening), `XR_KHR_composition_layer_depth`, `XR_EXT_local_floor`,
    `XR_EXT_hand_tracking`, `XR_META_performance_metrics`, `XR_EXT_user_presence`.
  - System "Meta Quest 3": 1680x1760 recommended per eye (rendered at `vr_render_scale` 0.9), 32 layers; refresh
    rates 72 80 90 96 100 120 144 180 200 207 Hz; swapchain formats led by `GL_SRGB8_ALPHA8` (taken).
  - **The four cvars work**: `vr_xr_refresh_rate 90` (VrApi `FPS=x/90`), `vr_xr_foveation 2` (`Fov=2`); the
    performance levels are accepted but VrApi showed CPU/GPU 4/4 throughout (the runtime raises them when frames
    are missed: not conclusive while the app is this slow).
  - **GL ES (123 extensions) resolves the `[?]` rows**: `EXT_clip_control` yes, `EXT_clip_cull_distance` yes,
    `EXT_depth_clamp` yes, `NV_shader_noperspective_interpolation` yes, `EXT_disjoint_timer_query` yes,
    `OVR_multiview2` + `OVR_multiview_multisampled_render_to_texture` yes, `QCOM_texture_foveated` yes,
    `EXT_multisampled_render_to_texture` yes, `EXT_buffer_storage` yes, `OES_texture_view` yes, `EXT_clear_texture`
    yes, `EXT_texture_compression_s3tc`/`bptc`/`rgtc` yes (the PC's BC textures load as they are), ASTC yes,
    `OES_get_program_binary` yes, `EXT_fragment_shading_rate` yes, `EXT_color_buffer_float` yes;
    `EXT_multi_draw_indirect` not listed (its entry point is); no bindless, no multi-bind; image uniforms 8 fragment /
    23 combined.
- **Frame timing, `vr_quest_preset 0`, e1m1's start, 72 Hz** (VrApi, 92 one-second samples over two runs):
  **24 FPS** of 72; the app's GPU time 13.0 ms (GPU 38% busy), CPU+GPU 34.6 ms, the CPU 22-26% busy: neither
  saturated, so the frame waits on something (a sync: a query read, a fence, a `glReadPixels`, or `xrWaitFrame`
  paced wrong). Shader compiles: ~55 s before the first frame (synchronous programs on Adreno; the first launch).
- **Open**: what the frame waits on (simpleperf from the NDK, no install; the engine's `vr_profile`); the first
  launch's compile time (the driver's blob cache on the second launch; `OES_get_program_binary`); "Save error." on
  e1m1's autosave (Android only? the save thread's string table); shadows off ("need GL_ARB_clip_control": the ES
  path to `EXT_clip_control`, `-glesclipcontrol`, is ready from C2b); the view model's depth clamp and clip
  distances with their extensions; the image in the headset, the `imageRect` origin (6f), the controllers, sound in
  the headset: need it worn.

## 6j. C4c: the black world on the headset (in progress; adb, the headset unworn; no installs)

His first look (2026-11-15, VR APK, e1m1): the ammo screens, the wrist gadget, loot beams and the menu panel drawn,
the world and the models pitch black, some menu shapes missing; ~55 s to the first frame; 24 FPS; the menu blurry.

- **Tools made for it** (both stay; Debug > Startup): the VR APK is **debuggable** (`adb shell run-as` reads the
  app's internal folder; simpleperf can attach), **`vr_android_export <path>`** copies game files (eyeshots, saves)
  into the internal `export/` folder (what the game writes in its external folder is 0660 to adb's shell), and
  **`vr_gl_probe 1`**: the next frame's eye framebuffers by numbers (complete or not, the share of pixels not black
  after the scene, the post-process and the UI; the scene's primitives and whether samples passed; the frame data's
  upload read back and compared; a clear-and-read self-test). `scratch/c4c_dev.sh` (cfg push, export pull).
- **OpenGL ES errors found with `-gldebug` (Adreno's debug output) and fixed** (the PC's `-glescompat` on a desktop
  driver accepts all of them, so `gles_render_test` never saw them):
  - every TexMgr upload refused (3199 a boot: "pixel buffer format is not compatible"): `GL_RGB` and `GL_RG8`
    internal formats from RGBA bytes are not ES; under the ES profile they are RGBA8 (a solid texture's alpha made
    opaque, as `GL_RGB`'s reads are), and no texture is compressed at its upload (`gl_compress_textures`);
  - the light clusters (both eyes') and the palette LUT bound as images while mutable (5950 a boot: ES binds only
    immutable textures as images): `glTexStorage3D` under the ES profile;
  - the eyeshot's `glReadPixels` as `GL_RGB` (refused by ES: the Quest's eye images were black whatever the eyes
    showed): read as RGBA, the alpha dropped (PC: the same files);
  - the world arrays' depth pre-pass twin and a vertex-only multiview twin linked without a fragment shader
    (Adreno: "Operation Failed"): an empty one, as the plain programs got in C4b.
- **Android defaults**: `EXT_clip_control` taken when the driver has it (the Quest 3 does; `-glesnoclipcontrol` for
  the standard-depth path): reversed Z as the PC, the map lights' shadows back; the app's window gets no flat image
  and no `eglSwapBuffers` while VR runs (`vr_android_window` 0; 1 for the flat-panel APK).
- **Save error.** (Android only: the PC's e1m1 autosave works) now says why: "string -14: a freed engine string"
  (`pr_save_error`; an edict field still points at an engine string slot freed since).
- **Still black after all of it** (`vr_gl_probe`, e1m1's start): the eye's scene draws 31-35 k primitives, the frame
  data's UBO reads back byte for byte (ViewProj sane), depth clear 0 / `GL_GEQUAL` (reversed Z), colour mask on,
  draw buffer `COLOR_ATTACHMENT0`, the framebuffers complete, no GL error; yet the composite is 0.00% non-black at
  every stage (the eye clears to 0 0 0 0, so a missing clear and a fragment output of 0 look the same). The vertex
  side is fine: the fragments are rejected (depth/stencil against the hidden-area mask or a stale depth) or write 0.
  Next probes: `GL_ANY_SAMPLES_PASSED` around the world alone (the mask's draw passes samples too), the eye's clear
  colour made non-black to see whether the clear lands, `vr_visibility_mask 0`, the world fragment shader with a
  constant output. Boot: still ~53-55 s on relaunches after a reinstall; FPS and the menu's sharpness: not reached.
- **The bisection (C4c continued, 2026-11-14; `vr_gl_probe` 2 to 8, `vr_gl_probe_fix`, Debug > Startup)**, e1m1's
  start, both eyes, every result the same in each eye:
  - **2** (blue painted after the clear, no depth test): the composite is 100% blue (0.25 0.5 1.0) after the scene:
    the scene target, its copy to the composite and the readback all work, and **not one scene fragment lands**.
  - **3** (red after the scene where the depth still holds the clear value): 100% red: **no scene draw writes depth**
    either (not the world, not its depth pre-pass, not the models with `r_drawworld 0`, not `r_showtris 1`, not with
    `vr_world_texarrays 0` or `vr_visibility_mask 0`). The fragments are not written-as-0: they never pass.
  - The world's GPU-written buffers read back as the PC's (the cull's indirect commands, 132 of 300 words; indices
    53298 of 65536 not zero, largest 30917; the vertices), the instance upload byte for byte, the frame UBO byte for
    byte; every buffer block's driver layout equals the range bound (FrameDataUBO 2080, CallBuffer 80/call,
    InstanceBuffer 176, LightBuffer 448, AOBlock 10512, RetroUBO 4112, TemporalBlock 96); the draw state at the world's
    draws is sane (scene fbo, viewport, depth range 0 1, `GL_GEQUAL`, depth write on, clip distance 1 only, no depth
    clamp, no alpha-to-coverage, sample mask all, no stencil/scissor/discard) and **every world program validates**.
  - `vr_gl_probe_fix 1` (clip distances off) and `3` (culling off too) at each world draw: still nothing.
  - **4** (the world's own VBO and IBO through the frame UBO's ViewProj, a minimal program, no depth test): 100% green;
    **5** (through the first instance's matrix from the InstanceBuffer SSBO too): 100% green; **6 to 8** (with the
    world's clip distances written, plane 1 on or off, distance 0 written or not): 100% green.
  - So the data, the transform, the UBO/SSBO bindings, the clip planes and the targets are good: **what drops every
    fragment is inside the real scene programs** (the world's and the models' alike), on Adreno only (the PC's ES mode
    draws them). Left to split: the world vertex shader's own path (`call_data[DRAW_ID]` through the `DrawID` uniform,
    `invariant gl_Position`, its ~13 outputs up to location 29, the temporal block) against its fragment shader (a
    discard every fragment takes, an output Adreno rejects). The next probe: a start-up switch defining a macro in the
    ES header (`vr_glsafe.cpp`) that the world/alias shaders test to output a constant colour at the top of the
    fragment shader's main (a VS fault keeps it black, a FS fault turns it magenta), then the VS's outputs cut down.

## 6k. C4d: the black world found and fixed; the program binary cache (2026-11-14; adb, the headset unworn)

- **The split** (`-glesdefine A,B,...`: `#define A 1` in every ES shader's header; the `QVR_PROBE_*` switches in
  `gl_shaders.h`; e1m1's start, the composite's share of pixels not black, `vr_gl_probe 1`):
  - `QVR_PROBE_FSCONST` (the world's and the models' fragment shaders write magenta first): 0%. A vertex-side fault.
  - `NOINVARIANT` (no `invariant gl_Position`), `DRAWID0` (every call as the first), `NOCLIP` (clip distances 1): 0%.
  - `VSPOS` (`ViewProj * in_pos`, nothing clipped): 100% magenta. The fault is in the world VS's own transform.
  - (Probes since removed:) the first instance's matrix read by its members from the SSBO: 100%;
    `Transform(in_pos, instance_data[0])` (the struct passed whole): 0%; `Instance ic = instance_data[0];` then its
    members: 0%; a local `Instance` filled member by member, given to `Transform`: 100%; its `alpha` alone: right.
  - **Cause: Adreno (driver 0837.0.9) reads a struct copied whole out of an SSBO wrong** (`Instance instance =
    instance_data[i];`, `InstanceData inst = instances[...]`): the copy's matrix drops every vertex. Every world and
    model program (and their depth pre-passes) does it, so no scene fragment was ever written.
- **Fix (ES form only, `vr_glsafe.cpp` `expandStructCopies`):** every `T v = b[i];` of a struct `T` the source declares
  becomes `T v; v.m = b[i].m; ...` member by member (array members element by element, the struct's own `#if` lines
  kept). `-glesstructcopy` leaves the copies whole (the bug back: a driver update's check). The PC's `-glescompat`
  compiles the same rewrite (`gles_render_test` 21/0); the PC's own GLSL is untouched. On the Quest: the composite
  99.5% not black, e1m1's start corridor lit and textured, a model drawn (eyeshots `scratch/c4d_eyes/`). Seen there and
  left for later: blocky, striped texel patterns on the walls and floor (dithering or a texture format; unchecked).
- **The program binary cache** (`vr_program_cache`, default 1: on under OpenGL ES, 2 also on the PC, 0 off; read before
  the window opens): each program's `glGetProgramBinary` kept in one pack per driver identity (vendor, renderer,
  version: `<cache root>/programs/<hash>.bin`), keyed by the very text the driver compiles (`VR_GLShaderHash`: the ES
  rewrite, the header's switches, the macros); given back with `glProgramBinary`, a binary the driver refuses made again
  from the sources and replaced. From harness4's stash (on the PC it saved ~20 ms, hence off there by default).
  On the Quest, `vr_startup_times`, process start to e1m1's first frame: **cold 65.1 s** (GL_CreateShaders 51.1 s, VR
  programs 1.7 s, map load 11.9 s), **warm 8.3 s** (103 programs from their binaries in 126 ms, GL_CreateShaders 0.18 s,
  VR programs 0.4 s, map load 7.2 s); the warm start draws the same (99.7% not black). The PC with
  `vr_program_cache 2`: GL_CreateShaders 11.5 s cold (a low-impact run), 0.42 s warm (106 programs).
- **The driver's own blob cache (`EGL_ANDROID_blob_cache`) is not in use**: after a dozen starts the app's cache folder
  has no `com.android.opengl.shaders_cache` (only `oat_primary`), and every start compiled for ~51 s.
- Not cached: the VR programs (`VR_PrecompilePrograms`: 1.7 s cold, 0.4 s warm), the multiview and texture-array twins
  (made later, in the background).

## 6l. C4e: from 2 FPS to ~39 (2026-11-14; adb, the headset unworn; no installs)

Method (all `scratch/c4e_*`): one launch on e1m1's start, a cfg of variants (`c4e_gen.py`: each set, 4 frames,
then `vr_profile` over N frames with `vr_profile_gpu 0`, `vr_profile_dump`, undone), summarised by `c4e_ab.py`
(frame period, CPU busy; the run's shader-error count: a probe that fails to compile draws nothing and looks fast).
simpleperf can't sample on the Quest (`cpu-clock`/`cpu-cycles` "not supported" for the app); the GPU timer queries
work but split a tiler's passes, so the cuts were by A/B. **`vr_profile_slow_ms`** (new, Debug > Profiling "Slow
Frames Kept", 250 as before): frames over it are left out of the averages; at 2 FPS every frame was one.
- **Before** (C4d's APK): 424-450 ms a frame (2 FPS): CPU 280 ms in "models" + "ui" (370 + 20), GPU ~800 ms.
- **1. The CPU: the frame buffers' uploads** (`GL_Upload`): ES has no `GL_ARB_buffer_storage`, so every upload was a
  `glBufferSubData` into the frame's host buffer while the GPU still used it, which Adreno copies whole. Now
  `GL_EXT_buffer_storage` (Android) gives the persistent mapped buffers the PC has: "models" 370 -> under 1 ms CPU.
- **2. The GPU: the world's fragment shader**, split with `-glesdefine` probes (gl_shaders.h `QVR_PROBE_*`, kept):
  the whole frame 383 ms; FSCONST (a constant colour) 35; cut after the texture read (CUT1) 82, after the decals
  (CUT2) 68, after the lightmap (CUT3) 72, before the lights' loop (CUT5) 100, after it (CUT4) 383; the loop with a
  plain point light read in place (SIMPLELOOP) 125; no shadow code (NOSHADOW) 397; the lights in a pixel's cluster
  (LCOUNT, an eyeshot): 0 almost everywhere. So the loop cost ~280 ms **without running**: the ES struct copy of
  6k (`Light l = Lights[i]`, member by member) copied `gateinverse[3]` with a `for` loop; that index into a local
  struct's array made Adreno keep the whole 240-byte `Light` in scratch memory for every pixel. **Fix**
  (`vr_glsafe.cpp` `appendMemberCopies`): a literal array size up to 16 is copied element by element, unrolled.
  The frame: 383 -> 140 ms.
- **3. MSAA**: `vid_fsaa 2` -> 0 took the frame from 138 to 41 ms (the float scene's 2x MSAA goes through memory, not
  resolved on chip: no `EXT_multisampled_render_to_texture` path yet). **`vr_quest_preset` 0 now has `vid_fsaa 0`**
  (as the 90 Hz and Quest 2 levels).
- **After** (the preset reapplied, `+vr_quest_preset 0`, 120 frames each): **25.6 ms (39 FPS)**; `r_drawworld 0`:
  14.1 ms (72 Hz); render scale 0.7 (dynres off): 29 ms (not pixel-bound now: the world's draws themselves).
- Checked and not the cause: parallax code present but unused (NOPARALLAX: no change, 138 vs 140 ms), the shadow code
  (NOSHADOW), dynamic lights (`r_dynamic 0`), the hidden-area mask, bloom, particles, the models (`r_drawentities 0`:
  ~the same as with them at every step).
- Open: the world's remaining ~11 ms over the rest (the uber-shader's size on Adreno: NORETRO + NOCARVE + NOSHADOW +
  NOPARALLAX together were 62 ms against 138 at MSAA 2x: a "lite" ES world shader with what the Quest preset turns
  off compiled out is the next lever; `vr_decal_carve` is on in the preset); MSAA on chip
  (`EXT_multisampled_render_to_texture`); dynamic resolution never lowered the scale (vr_dynres needs the GPU's time:
  check what it reads on ES); the striped texels and caching the VR programs: not reached.

## 6m. C4f: the coloured squares, black views; sRGB (2026-11-15; adb, the headset unworn; no installs)

His look (C4e APK, `vr_quest_preset 0`): world colours wrong and flickering, bullet time black, black after a
Resolution Scale change, choppy. Method: `scratch/c4f_seq.sh` (one launch, steps of commands each followed by an eye
image, `vr_eyeshot 2`: the post-processed eye before the copy into the swapchain), `c4f_shot.sh`, `c4f_img.py`.
- **The final eye image can't be read back on the Quest**: `vr_eyeshot 1`/`3` and `vr_gl_probe`'s "image" read the
  swapchain image as all zeros with no GL error (`eyeshot:` now logs a read error; none comes). Use `vr_eyeshot 2`.
- **World probes** (`-glesdefine`, gl_shaders.h): `QVR_PROBE_LOD0` (the texture's top level), `_LM` (the lightmap),
  `_UV`, `_RETRO` (retro set/on), `_LUTI` (the palette table's index): the world's textures, lightmap and UVs are
  right; retro textures and retro lighting are on in his config (`_RETRO`, `_UV`).
- **The squares are the bloom** (one launch, steps): the red/yellow/green/black squares (and magenta ones) come and go
  per launch and always follow a change of the eyes' size (render scale, dynamic resolution); `r_drawworld 0` keeps
  them, `vr_particle_halfres 0` keeps them, `vr_bloom 0` clears them at once; with bloom on a launch can be all black
  (f4). At scale 0.7 they were absent in every run. Tried and not the cause: the retro palette's dynamic component index
  (`RetroPal[i >> 2u][i & 3u]`: the same with a select), `vr_retro 0`, clearing the scene targets when made, a
  NaN/Inf filter in the bloom's bright pass (still squares: reverted). Cause inside the bloom chain (R11F_G11F_B10F
  targets, `vr_bloom.cpp`) not found. **`vr_quest_preset 0` now has `vr_bloom 0`** (as the 90 Hz and Quest 2 levels).
- **Bullet time**: before the swapchain copy the eye is drawn (tinted, `vr_eyeshot 2`); his black view is most likely
  the same bloom fault (the black launches); unconfirmed for want of a readable final image.
- **sRGB**: the swapchain is `GL_SRGB8_ALPHA8`; OpenGL ES encodes every write into an sRGB image (the PC with
  `GL_FRAMEBUFFER_SRGB` off writes as is), so the game's gamma-encoded colours were encoded twice (pale). Now
  `EXT_sRGB_write_control` (the Quest 3 has it) turns that off at session start; `-glessrgbencode` keeps it.
- **Frame time: not measured** this time: the unworn headset holds frames (`xrWaitFrame` 47-60 ms, 75 ms apart); C4e's
  25.6 ms stands. Dynamic resolution does lower the scale on ES (0.9 -> 0.65 seen in the eye images' sizes).

## 6n. C4g: the stretched, crawling world and opaque water were `centroid`; the lite ES world (2026-11-15; adb, unworn)

His look (C4f APK, `vr_quest_preset 0`, e1m1): the world's and the ammo boxes' (brush models') textures "incredibly
stretched", flickering, colours about right; the alias models perfect; water without transparency; too choppy.
Method: `scratch/c4g_seq.sh` (C4f's, `MAP=` for another map), `c4g_probes.sh` (one launch per `-glesdefine` set),
`c4g_crop.py` (full-size crops, brightened), `c4g_perf.sh` (a warm-up then 300 frames, `vr_profile_gpu 1`).
- **Not precision**: the ES header already declares `highp` for every type and stage (`vr_glsafe.cpp` esHeader).
- **The split**: the texture read alone (`QVR_PROBE_LOD0`, `CUT1`, `CUT2`) is right; once the lightmap code is live
  (`CUT3`, and `CUT3T`: the texture alone, the light kept live by a test no value passes) the texture is a smear; the
  texture coordinates read at the shader's end with all of it live (`UVLATE`) are noise, at its start (`UV`) smooth.
- **Cause: Adreno (driver 0837.0.9) interpolates the world's other inputs wrong once its `centroid` input (the lightmap
  coordinates, `in_lmuv`) is read**: `in_uv` garbage (the textures read at huge, crawling coordinates: low mips,
  stretched), the water's flat inputs too (its alpha and look: a flat green, no texture, nothing seen through). Not the
  interface's size (limits now in `vr_gles_report`: 128 components each way, 31 varying vectors; the world uses 22
  locations) and not the location numbers (prevclip moved from 29 to 23: the same).
- **Fix (ES form only, `dropCentroid`)**: `centroid` dropped from every shader's ES text; `-glescentroid` keeps it (a
  driver update's check). Without MSAA (the Quest preset's) centroid and centre sampling are the same. `QVR_PROBE_NOCENTROID`
  showed it first (UVLATE smooth, CUT3T the true texture). e1m1's walls and ceiling and vrtesthall's pool (its tiles seen
  through the water) now as the PC's; eyeshots `scratch/c4g_eyes/` (before: `a1_0.png`, `a1_crop.png`; after:
  `f2_1.png`, `fin_1.png`, `f2_crop.png`, `fin_crop.png`; water: `w2_crop.png` before, `w1_crop.png` after).
- **The lite ES world** (`VR_GLESWorldLite`, `GL_WorldLiteCheck`): under the ES profile the world programs leave out
  parallax mapping while `vr_parallax` or `vr_normalmaps` is 0 and carved decals while `vr_decal_carve` is 0 (the
  Quest preset's, from Low: `QVR_LITE_NOPARALLAX`, `QVR_LITE_NOCARVE`); a change of those settings makes the shaders again
  at the next frame (the program cache gives back the rest); `-glesfull` keeps the whole shader. e1m1's start, GPU a
  frame (unworn; frames held by the runtime, so the GPU's timer queries, not the frame period): **full 21.0 / 21.1 /
  21.5 ms, lite 18.6 / 18.4 / 17.7 ms**; the frame period ~20 ms (50 fps; 72 Hz needs 13.9).
- **The rest of the GPU frame** (one launch each, `-glesdefine`, against ~18.7 then): retro textures off (`NORETRO`, his
  setting) 16.9 ms, no shadow code (`NOSHADOW`; the preset keeps one shadowed light) 17.9, a constant world colour
  (`FSCONST`) 11.3: so ~11 ms is not the world's shading (the eyes' post chain, models, clears) and ~7 ms is.
- Not reached: MSAA on chip (the preset has none), dynamic resolution's floor (0.65), the bloom fault, bullet time.

## 6o. C4h: the frame outside the world's shading; the avatar's 10 ms; ovrgpuprofiler (2026-11-15; adb, unworn)

Method (`scratch/c4h_*`): `c4h_ab.sh` (one launch, variants in a cfg, `vr_profile_gpu 1`) with the GPU's clock and busy
share sampled from `/sys/class/kgsl/kgsl-3d0` alongside (`c4h_sampler.sh`, `c4h_sum.py`: the timer queries checked
against the busy share); eye images by `c4h_seq.sh` (`PREWAIT=700`: the first frames are not the eyes'; `EYE=R`);
**`ovrgpuprofiler`** (the Quest's own, `/system_ext/bin`, no install: `-e com.vittorioromeo.quakevr` once, then `-t`
for a render stage trace of every surface: its size, bits, binning or direct mode, bins, loads and stores;
`c4h_gpuprof.sh`; `-d` turns it off again). e1m1's start, `+vr_quest_preset 0 +vr_dynres 0 +vr_render_scale 0.9`.
- **Two regimes by launch, 29 ms or ~18 ms**: the timer queries put ~11 ms in the right eye's "scene setup" in most
  launches. Not the clear (`gl_clear 0`, the depth format, the hidden-area mask: the same), not the GPU clock (640 MHz
  both, busy 97%). It follows `r_drawentities`, then **`vr_body_mode 0`** (2 the same; `r_lerpmodels`,
  `vr_alias_cull_posed`, `vr_alias_player_first`: the same) and goes with `-glesdefine QVR_PROBE_ALIASCONST` (new: the
  models' fragment shader a constant): **the avatar's fragment shading, ~10 ms, in the right eye only, unseen**
  (`r_drawworld 0`: the body fills the right eye; with the world it is all behind it). The unworn headset's pose
  (lying, the controllers elsewhere) puts the right eye inside the stretched avatar. A test artefact for the cost, but
  it shows that **a model's hidden pixels are shaded in full on Adreno even when drawn after the world** (first0 kept the
  10 ms): early depth rejection does not happen for the alias programs (their `discard`s: alpha test, morph, gates?).
  Measured with `vr_body_mode 0` from then on. Worn, check the body looking down.
- **The split by A/B** (body off, GPU ms a frame, both eyes): 17.9 in all; the window's mirror **+3.4** (the immersive
  app's window is never shown); the world 13.4 of it (`r_drawworld 0`: 3.9 left); the 64-bit float scene vs 32-bit
  (`vr_tonemap 2`) **+1.4**; invalidation of the depth after the scene and of the image before the post-process -0.1 to
  -0.5; particles, the mask, grade, dither, retro textures (in-launch), the shadow, dynamic lights: ~0.
- **The passes** (`ovrgpuprofiler -t`; `vr_gl_passes 1`, new, Debug > Profiling: the frame's framebuffer binds with
  their draws and scopes): each eye is ~7 surfaces at 1512x1584 (64-bit colour, D32F+S8, 12 bins of 576x416): the
  scene's first part (0.73 ms, loads depth), a direct-mode one (0.28), the world's (5.4: its shading, LoadColor and
  LoadDepthStencil 0.22 ms), small ones, an 8-bit target with the scene's depth (one draw in "translucent", direct mode: fbo 17/22 in
  `vr_gl_passes`, then back to the scene: a store and a load), the translucent and
  post part (1.3, 17 bins). Splits without a framebuffer change come from compute between draws (GLAsyncCompute: the
  world's GPU cull, the light clusters) and each costs a depth load and store (~0.25 ms). The window's (4128x2208)
  passes are now empty (0.01 ms).
- **Cuts made** (all ES-only or Android-only; the PC unchanged):
  - **The mirror off under `VR_NoWindow`** (`vr_window.cpp`: the view Off; `vr_panel.cpp`: no window HUD or canvas
    over it): -3.4 ms.
  - **`vr_gl_invalidate`** (-1: ES; `glInvalidateFramebuffer` now in the GL list): the eye's depth and stencil after its
    scene, the eye image's colours before the post-process (when it writes it whole).
  - **`vr_tonemap 2` in the Quest preset** (R11F_G11F_B10F): -1.4 ms, eye images the same (0.24% of pixels differ,
    the noise between two shots 0.16%).
  - **`vr_eye_targets_split`** (default 0): the right eye's own scene targets (made for the 11 ms, which was the
    avatar): no gain measured, left off (one more set of targets in memory).
  - **`vr_gl_foveation`** (0; experimental): `GL_QCOM_texture_foveated` on the eyes' scene colour (bits before the
    storage, `VR_SceneAttachmentFoveation`; per eye the projection's centre as focal point): **no effect** (the bins
    stay 576x416, the GPU the same, the image the same): open (the driver may want the depth foveated too, or a
    different format than RGBA16F/R11G11B10F).
- **Results** (GPU a frame, e1m1's start): before (C4g's APK, 0.9 fixed): 21.0 with the body behind (29.2 with it in
  the eye); now 0.9 fixed: 17.6 with the body behind (16.5 without it); the preset as shipped (dynamic resolution on): **11.4 ms, the frame at 13.89 ms (72 Hz)**; eye images as C4g's.
- Next levers, by size: the world's shading (5.4 ms an eye: foveation working, or the world shader's cost on Adreno);
  the passes split by compute in the eye's scene (each ~0.25 ms of depth load/store: run the world's cull and the
  light clusters for both eyes before the first eye's scene); the alias programs without `discard` for opaque models
  (early depth rejection); MSAA on chip (`EXT_multisampled_render_to_texture`, only if he wants MSAA back).

## 6p. C4i: the world's shading cut by a third, opaque models, a fight measured; the compositor's picture (2026-11-15; adb, unworn)

Method (`scratch/c4i_*`): `c4i_ab.sh` (one launch: e1m1's start, then **a fight**: `setpos` into the computer room,
god mode, skill 2, 6 grunts and 3 dogs spawned beside the map's 4 grunts: their fire, muzzle flashes, blood; each at 0.9
fixed and with dynamic resolution, the body on and off), `c4i_probes.sh` (the world shader cut per stage, one launch a
probe), `c4i_fsplit.sh` (the fight split by A/B). **`adb shell screencap -p` reads the compositor's final picture**
(4128x2208, both eyes, after our swapchain: what the headset shows; no install): `c4i_cap.sh` (waits for a `C4I` marker
the cfg echoes; the cfg needs `chmod 666` or `exec` fails), `c4i_caps.sh`, `c4i_compose.py`.
- **The world's cost by stage** (0.9 fixed, body off, world+brush both eyes, ms): constant colour 2.6; the texture 4.3;
  + decals 5.2; + lightmap 6.3; + `QVR_WORLD_FS_LIGHT` (contrast, normal maps, deluxe, AO, caustics, cap) 8.8; + the
  lights' loop 9.6; the whole shader 12.8. His settings have retro textures, retro lighting, normal maps, dynamic AO,
  caustics and detail textures all **off** (uniforms): their code alone cost ~4.6 ms.
- **Lite world, all switches** (`vr_gl_world_lite` 2; **the default stays 1**, C4g's, 0 the whole shader): the world's main()
  takes each switch the settings turn off as a constant (`QVR_WORLD_FS_LITE`: `QVR_LITE_NOBUMP`, `_NOAO`, `_NOCAUSTICS`,
  `_NORETROLIGHT`, `_NORETRO`, `_NODETAIL`), the programs remade when one changes (`GL_WorldLiteCheck`). One launch:
  world+brush **12.9 -> 8.2 ms**. The PC's `-glescompat` compiles it (gles_render_test 21/0, but its scene keeps those
  features on, so the lite form is not compared there); the PC's own GLSL untouched. **But** the headset's captures
  under 2 (bullet time, the menu open) showed the walls flat, untextured, and 0 textured (`c4i_cap/k4.png`, `k3.png`,
  `k5.png`; the one capture of 2 alone failed to pull): unverified, hence default 1. First thing next phase: capture 2
  against 1 (`c4i_caps3.sh`), find the macro at fault (one `QVR_LITE_*` at a time by `-glesdefine`), then default 2.
- **Opaque models without `discard`** (`glprogs.alias_opaque`, made under the ES profile only; `vr_gl_alias_opaque` -1):
  an opaque, solid, non-morphing model's draw uses a form of the models' program with the morph and gate discards
  compiled out (a morphing entity never shares its batch), so Adreno rejects its hidden pixels before shading. With the
  eye near the body: 17.7 ms off, 16.8 on (body off 16.6). Left as it was: the player's models drawn before the world
  (`vr_alias_player_first`): shaded in full only when the eye is inside the body and the world in front (the unworn pose).
- **Permanent decals' bakes under a scissor** (`vr_permdecals.cpp scissorOf`): each (level, layer) of the atlas a bake
  touches was a render pass loading and storing the whole layer on a tiler (the fight's "permdecal clears" scope); now
  only the bins the slots cover (the same picture; also on the PC). Not measured apart.
- **Menu sharpness**: the panel is drawn into the eyes at the eye's scale (dynamic resolution's 0.65 blurs it).
  `vr_menu_render_scale` (-1: 1 under the ES profile, none on the PC): while a menu is open the eyes are drawn at least
  at that scale; dynamic resolution waits meanwhile. The capture with the menu open reads clearly.
- **Bloom (`vr_bloom 1`) after size changes: the whole view black** in the compositor's picture (the HUD alone drawn),
  the first readable proof (C4f only had squares in `vr_eyeshot 2`). Every bloom pass's output now clamped to 0..1e4 (no
  NaN or Inf goes down the chain); its capture afterwards failed to pull: unchecked. Bloom stays off in the preset.
- **Bullet time with bloom off: drawn**, desaturated and tinted in the compositor's picture (`c4i_cap/k4.png`): the black
  view was bloom's.
- **GPU a frame** (the timer queries; before = C4h's code: `vr_gl_world_lite 1 vr_gl_alias_opaque 0`; after = lite 2
  and the opaque models, so the "after" world is the unverified lite form; same launch set-up; the GPU's clock
  560-640 MHz, so +-1 ms):

| | e1m1 start, 0.9 fixed | start, dynamic res. | fight, 0.9 fixed | fight, dynamic res. |
|---|---|---|---|---|
| before | 14.7 | 15.7 (frame 29.5 ms) | 30.6-39.0 | 18.5 (frame 20.5 ms) |
| after | 11.3 | 12.3 (frame 14.8 ms) | 24.4-25.2 | 14.9 (frame 17.1 ms) |

- **The fight** is the next limit: the computer room's world is ~12-13 ms of world+brush at 0.9 (the start's 6), its
  dynamic lights ~2 ms (`r_dynamic 0`), particles and decals small; dynamic resolution holds 14.9 ms GPU but frames come
  17 ms apart.
- Not done: one render pass an eye (the splits come from the world's GPU-driven gather, `R_FlushBModelCalls`' compute,
  between draws; making it CPU-side under ES is the lever, ~0.25 ms a split); OIT's framebuffer switch (`r_oit 0` in
  the preset: untested).
- His Quest config had `vr_body_mode 0` (a test's leftover: his PC config has the default 3): set back to 3.

## 6q. C4j: the lite world's flat walls found (retro textures); its default 2; r_oit 0; the models' lite form; bloom checked (2026-11-15; adb, unworn)

Method (`scratch/c4j_*`): captures from the compositor (`c4i_cap.sh`, now re-waking the headset and retrying an empty
`screencap`: an unworn Quest 3 falls asleep mid-run and `screencap` then writes 0 bytes), `c4j_bisect.sh` (lite 1 plus
`QVR_LITE_*` macros by `-glesdefine`; `LITE=2` with probes), `c4j_compose.py` (left eyes side by side). Timings:
`c4j_ab4.sh`, **the fight frozen** (`sv_freezenonclients 1` once the 13 monsters closed in, the body off, 0.9 fixed,
each candidate between two baselines): C4i's moving fight drifted by +-8 ms between variants (dogs biting, grunts
walking up to the eye), the frozen one by +-0.4.
- **The flat walls were the retro bit (64, `QVR_LITE_NORETRO`)**: lite 1 + NORETRO + NORETROLIGHT flat, + NORETROLIGHT
  alone textured, NOBUMP + NOAO textured; under lite 2 the probe `QVR_PROBE_LOD0` (`textureLod` level 0) textured and
  `QVR_PROBE_CUT1` (the colour read alone) flat: with `RetroBegin` compiled out Adreno's compiler (driver 0837) reads the
  world's textures at their smallest mip, each wall its texture's average colour. `textureGrad` with main()'s own
  derivatives the same (tried: still flat), so the derivatives of `uv` themselves come out wrong in that compile; not
  chased further. **Lite 2 no longer sets the retro bit** (`vr_glsafe.cpp`; `-glesdefine QVR_LITE_NORETRO` still tries it);
  captured twice textured. **`vr_gl_world_lite` default 2** (ES only; the PC's shaders never take it).
- **Bloom after size changes: drawn** (`c4i_bloom.cfg`: bloom on, 0.8 then 0.9; the glow shows, no black view): C4i's
  clamp fixed it. Bloom stays off in the preset (its cost).
- **`r_oit 0` in the Quest preset** (all three levels): OIT's own target is a render pass more an eye. e1m1's start -0.65
  ms, the frozen fight -0.6 to -1.0 (26.9 vs 27.5-28.3).
- **The models' lite form** (`vr_gl_alias_lite` 1; ES only): the models' programs take the lite world's bits 1 (parallax:
  `vr_parallax` or `vr_normalmaps` 0 gives every model a 0 depth, `VR_ParallaxDepth`) and 32 (retro lighting: its frame
  data all 0) as `QVR_LITE_NOPARALLAX` / `QVR_LITE_NORETROLIGHT` (`GL_AliasLiteDefines`, `QVR_ALIAS_FS_LITE`). The fight
  captured with it on and off: models textured and lit alike. Frozen fight: -0.2 to -0.5 ms (27.5 vs 28.2, 27.7).
- **The models' shading is the fight's other big cost**: with their fragment shader a constant
  (`-glesdefine QVR_PROBE_ALIASCONST`) the fight's GPU fell ~5 ms (20-21 vs 23-28, the moving fight; the start's the
  same), and the `world+brush` scope fell with it (the models' work lands in its timer queries). The lite form's two
  switches are a small part: the rest is per instance (wounds, dents, stumps, gloves, skin tone, rim and reflections by
  `Surface`, the bumps by `AO.z`: an authored normal map may stay on with `vr_normalmaps 0`), so a lighter form needs
  each gate's CPU side checked first.
- **The CPU in the fight**: CPU busy 12.6 ms of a 19.7 ms frame with dynamic resolution (server 4.4: QuakeC 2.9; screen
  6.1: the left eye 3.5, its models 0.9, view entities 1.1; `xrWaitFrame` 6.8 waiting): the 17-20 ms frames are the GPU's
  (dynamic resolution at its 0.65 floor still 18-20 ms), not the CPU's.
- **One render pass an eye: not done.** The splits are `R_FlushBModelCalls`' gather (compute between draws, every flush)
  over the GPU cull's output (`r_world.c` 104/122): putting it on the CPU means a CPU cull of the world under ES. Its
  size (~0.25 ms a split) is below the models' and the world's shading.
- **GPU a frame** (timer queries; before = this phase's start: `vr_gl_world_lite 1`, `r_oit 1`, no models' lite form;
  after = lite 2, `r_oit 0`, the models' lite form; the body off; e1m1's start 0.9 fixed in its own launches, +-0.6 ms):

| | e1m1 start, 0.9 fixed | the fight frozen, 0.9 fixed | the fight frozen, dynamic res. |
|---|---|---|---|
| before | 14.7 | ~33 (lite 1: 32.6, + OIT and models' form ~0.5) | |
| after | 12.3 | 26.9 | 20.2 (r_oit 1; frame 21.3 ms) |

- Left running on the headset for his check: e1m1, `+vr_quest_preset 0` (29 rows: `r_oit 0` added), lite 2, the models'
  lite form; captured textured (`c4i_cap/c4j_final.png`).

## 6r. C4k: the models' plain form; the fight's split; the lite world's derivative bug narrowed (2026-11-15; adb, unworn)

Method (`scratch/c4k_*`): `c4k_ab.sh` (C4j's frozen fight: 13 monsters closed in, `sv_freezenonclients 1`, the body
off, 0.9 fixed; each candidate between baselines in one launch), `c4k_wprobe.sh` (one `-glesdefine` probe a launch on
the start and the frozen fight), `c4k_lights.sh` (cvar A/B of the room's lights), `c4k_fightcfg.py` + `c4i_cap.sh`
(the frozen fight captured from the compositor). **Every test launch now passes `-noconfigwrite`** (`c4e_run.sh`,
`c4i_cap.sh`): closing the console at the map's start saved the launch's `+vr_body_mode 0` into his config
(`configFrame`), which is how it came back after C4i; `c4k_cfgcheck`/`c4k_q.cfg` read his settings without writing.
- **His config**: `vr_body_mode` was 0 again and `vr_xr_unfocused` 0 (test leftovers); both set back to their defaults
  (3, 1) with one `writeconfig`. The preset's rows (his `vr_quest_preset 0`) left as they are.
- **The models' plain form** (`vr_gl_alias_plain`, -1: the ES profile only; `glprogs.alias_plain`): an opaque,
  non-morphing batch none of whose instances turns on a per-model feature is drawn by the opaque form with that
  feature's code compiled out (`QVR_ALIAS_FS_PLAIN`, `QVR_APLAIN_*`): skin tone, gloves, nails, glove shells and the
  forearm tint (1), glows: hot blades and barrels, power-ups, force grab (2), wounds and the wet coat (4), dents (8),
  stumps (16), the normal map (32), morphs (64), rim light and reflections (128). The CPU side: each instance's data
  checked where the shader tests it (`vr_render.cpp VR_AliasPlainNeeds`), OR-ed over the batch (`r_alias.c
  aliasneeds`); a batch with one wounded grunt keeps the full form. `vr_gl_alias_plain_mask` (255) chooses what it
  leaves out; 256 (dynamic occlusion) and 512 (dynamic lights) are probes. Debug > Profiling rows. Frozen fight, one
  launch: **plain off 19.8-19.9 ms GPU, on 17.9** (all monster batches qualified: forcing it on every opaque batch,
  `vr_gl_alias_plain 2`, the same); also leaving out the occlusion or the dynamic lights: no further gain. e1m1's start
  the same (12.4). Captured on and off: the monsters textured and lit alike (`c4i_cap/c4k_fplain.png`). Adreno's
  preprocessor took `#define DentsAt()` (no parameters) as an object-like macro (a syntax error in all 18 programs;
  a failed program then drew nothing, which looked like a 13 ms gain): the plain form uses `#ifdef`s at the call sites.
- **The fight's split** (frozen fight, separate launches: the scene differs by +-2 ms between launches): the models
  all of 4.5 ms (`r_drawentities 0`, plain on); their fragment shader a constant (`QVR_PROBE_ALIASCONST`) -5 ms;
  the world cut before its lights' loop (`QVR_PROBE_CUT5`) -6.5 ms in the fight (-3.3 at the start), the texture alone
  (`CUT1`) -7. The lights themselves cost little: `r_dynamic 0` -1.0 ms, `vr_shadow_dlights 0`, the torches', the
  debris' and lava nails' lights 0: so the loop's cost is its code's presence (registers), not its lights.
- **The lite world's flat walls (`QVR_LITE_NORETRO`)**, ruled out by captures (lite 2 + NORETRO + a probe each):
  `Retro`'s global initializer (`Retro = 0` assigned in main: still flat); the texture read of a raw input
  (`QVR_PROBE_WNOPF`, the coordinates moved through a uniform's branch: flat); where the derivatives are taken
  (`QVR_PROBE_WANCHOR`, consumed where RetroBegin took them: flat); the implicit level (`QVR_PROBE_WLOD`, `textureLod`
  at the level from main()'s `dFdx(uv)`: flat). `textureLod` at level 0 is right (C4j): main()'s `dFdx(uv)` is wrong in
  that compile. `QVR_PROBE_WLODFRESH` (`dFdx(in_uv)` taken at the read) drew no world (unexplained). Lite 2 keeps the
  retro code; NORETRO now assigns `Retro = 0`.
- **One render pass an eye: not done.** The splits are the gather's compute (`R_FlushBModelCalls`, every flush) over
  the GPU cull's output; making it CPU-side means a CPU mark of the world's surfaces and an index buffer built for
  each eye, a rewrite of `r_world.c`'s flush sites (738, 999-1016, 1180, 1443) for ~0.25 ms a split.
- **GPU a frame** (timer queries, 0.9 fixed unless said; the body off):

| | e1m1 start | the fight frozen | the fight frozen, dynamic res. |
|---|---|---|---|
| C4j (lite 2, r_oit 0, models' lite form) | 12.3 | 26.9 (its launch) | 20.2 |
| C4k, plain off (same launch) | 12.4 | 19.8-19.9 | |
| C4k, plain on | 12.4 | 17.9 | 12.8 |

- Left running for his check: e1m1, `+vr_quest_preset 0`, the plain form on (its default).

## 6s. C4l: the lights' loop's cost found in its shadow code; the lights' lite form (2026-11-15; adb, unworn)

Method (`scratch/c4l_*`): `c4l_batch1.sh`/`c4l_batch2.sh` (one `-glesdefine` probe a launch, `c4k_wprobe.sh`: e1m1's
start and C4j's frozen fight, 0.9 fixed, the body off), `c4l_ab.sh` (`vr_gl_lights_lite` 1/0 in one launch, then
dynamic resolution), `c4l_caps.sh` (compositor captures), `c4l_pclite.sh` (the PC under `-glescompat`). Separate
launches differ by up to +-2 ms (two baselines: the fight 20.4 and 22.1-24.1).
- **The world's lights' loop by probe** (GPU ms a frame, start / frozen fight): baseline 12.3-12.9 / 20.4-24.1; the
  loop compiled out (`QVR_PROBE_NOLOOP`, new) 9.7 / 16.1; a plain point-light loop (`SIMPLELOOP`) 10.3 / 14.5; the shader
  cut before the loop (`CUT5`) 8.7 / 15.0-15.4; no shadow code (`NOSHADOW`: `LightShadow` returns the cone,
  `MapLightShadow` 1) 12.1 / 17.4. The loop's lights cost little (C4k: `r_dynamic 0` -1 ms; `vr_shadow_dlights 0` 0 in
  this launch too): **the cost is the shadow code's presence** (Adreno pays for code it never runs: registers and
  size), not its work.
- **Which shadow code** (each against a baseline of 22.1-23.0 in the fight): the portal lights' path
  (`PortalLightPath`: a gate's matrix and its own shadow lookup; `QVR_PROBE_NOGATE`) **-3.7 ms**; the spot lights'
  shadow (`QVR_PROBE_NOSPOTSHADOW`) -3.2; the spot's and the point light's shadow maps read by one filter's code instead
  of two copies (`QVR_LITE_ONEFILTER`, the same result) -2.5; the map lights' shadows, DarkPlaces' falloff and the
  shadow filter's other three kernels together (the first `vr_gl_lights_lite`, `c4l_ab.sh` l1): 0. The start: none
  of these moves it (+-0.4).
- **`vr_gl_lights_lite`** (1; with `vr_gl_world_lite` 2, ES only; `VR_GLESWorldLite` bits 19-25, `GL_LightsLiteDefines`):
  the lights' loops (the world's; the models' with `vr_gl_alias_lite`) without the code the settings leave unused:
  portal lights (`vr_portals` 0, the Low and Quest presets': `lightGates` makes none), map lights' shadows
  (`vr_shadow_maplights` 0), DarkPlaces' falloff (`vr_dlight_falloff` 0), the shadow filter's kernel a constant
  (`vr_shadow_filter`, retro lighting off); and always one filter's code for a spot's and a point light's shadow
  (`DlightShadowMap`; `SpotShadow` split into `SpotShadowAt` and the filter, the PC's own code unchanged). Debug >
  Profiling row. PC under `-glescompat`, the Quest preset: 0 shader errors, the eye the same bits with it on and off.
- **Not measured on the headset yet**: its battery ran out at 11:05, mid-probe (the last, `NOGATE` + `ONEFILTER`
  together, lost its fight), and it stayed offline past 13:00. The shipped form is the two measured parts together
  (portal lights out -3.7, one filter -2.5: expected ~-5 to -6 ms of the fight's ~22, the start unchanged), unverified
  together. Ready to run in one command once it is back: `bash scratch/c4l_batch3.sh` (installs the built APK; the
  lite form's A/B with dynamic resolution, `c4l_ab.sh`; probes on top of it: the models' retro textures out,
  `QVR_ALITE_NORETRO`, and the shadow code left, `NOSHADOW`; then `c4l_caps.sh`: the start and the fight captured
  with it on and off, and the derivative probe below).
- **The models' remaining ~5 ms**: not cut. Their lights' loop takes the one-filter form too (with `vr_gl_alias_lite`);
  C4k's plain-mask probes (no occlusion, no dynamic lights) gained nothing, so the next suspect is their retro
  textures' code (`RetroBegin` and two `RetroSample`s, kept as the world's are for the derivative bug): probe
  `QVR_ALITE_NORETRO` built, its timing and capture in batch 3.
- **The derivative bug** (`dFdx(uv)` wrong with `RetroBegin` compiled out): a cheap workaround built as a probe,
  `QVR_PROBE_WIAO`: the coordinates' derivatives without `dFdx`, by `interpolateAtOffset` (ES 3.2 core: (uv at +1/4
  pixel - uv at -1/4) x 2), the colour read by `textureGrad`. Compiles under the PC's `-glescompat` (0 shader errors);
  its capture against plain NORETRO is in batch 3 (`l4l_wiao` vs `l4l_nore`). If textured, NORETRO (lite bit 64) can
  come back with it, its timing deciding.
- PC: gles_render_test 21/0, scenario_check 138/0, load_models_test 4/0, dev and LTO builds 0 warnings.
- Not left running for his check (the headset off). The APK installed on it is batch 2's (`vr_gl_lights_lite` with
  the filter, falloff and map-shadow bits, without the portal and one-filter bits); `build-android/apk/QuakeVR.apk`
  is this commit's.

## 6t. C4m: everything in one launch; the lights' lite form off; NORETRO no safe cut; the models' cost split (2026-11-15; adb, unworn)

Method (`scratch/c4n_*`): **`vr_gles_define`** (new, empty; the ES profile only): `"A,B"` adds `#define A 1` (and B) to
every ES shader as `-glesdefine` does at launch, and a change makes the shaders again in the session
(`VR_GLESDefineChanged`, `GL_WorldLiteCheck`). So a probe's A/B now runs **in one launch, each variant between
baselines** (`c4n_ab*.sh`): the baselines of a launch agree to +-0.1 ms (fight 19.07-19.19 in batch 2), where separate
launches differed by up to 3 ms, and the frozen fight itself differs between launches (18.2 to 21.5: where the monsters
froze). Eyeshots (`c4n_eyes.sh`: `vr_eyeshot` several times in one launch, exported, pulled; the compositor's capture
is black while the unworn headset sleeps) compared by pixel diff (`c4n_cmp.py`: two shots of one setting differ by
0.3-1.5% of pixels > 16 levels). All at 0.9 fixed, the body off, `sv_freezenonclients 1`; 0 shader errors throughout.
- **C4l's lights' lite form costs, not saves**: `vr_gl_lights_lite` 0 against 1 in one launch: the fight -0.6, -0.7,
  -0.6 ms (batches 1, 2, 4), the start 0; with dynamic resolution -0.9. Its parts taken back out one at a time
  (`QVR_PROBE_LLKEEP_*`, new): one filter +0.3 (the one part that helps), portal lights, map shadows, falloff, the
  filter's kernel 0 each; all five at once -0.9 (as off). C4l's -3.7/-2.5/-3.2 came from separate launches: variance.
  **Default now 0** (Quest only; the PC never took it). `QVR_PROBE_NOSHADOW` -0.3, `NOSPOTSHADOW` +0.1: the spot-shadow
  code left in the loop costs nothing measurable; the shadow work is done.
- **NORETRO is no safe cut**: the world's retro textures' code out (`QVR_LITE_NORETRO`) -0.8 ms in the fight, -0.4 at the
  start; with the models' (`QVR_ALITE_NORETRO`, -0.9 alone) -1.2. Its eyeshots were textured with the lights' lite form
  on (the same pixels as with the code in), but **flat with it off** (batch 3: the walls in their average colours, as C4j
  saw: detail 0.93 -> 0.51), so Adreno's wrong derivatives depend on the rest of the shader: any settings change could
  bring the flat walls back. Bit 64 stays never; the probes stay. The `interpolateAtOffset` workaround
  (`QVR_PROBE_WIAO`) is textured but costs +5.4 at the start and +7.8 ms in the fight: dropped.
- **The models' fragment shader** (C4k's plain form on; fight GPU ms against its baseline): a constant (`ALIASCONST`)
  -3.0 to -3.5; drawn at all (`r_drawentities 0`) -5.9 to -6.8. Bisected by early returns (new probes, batch 4, base
  21.5): the skin times the vertex light (`AMIN`) -2.9; ended after the look: skin, wounds, dents (`ALOOKEND`) -2.9, so
  the look's code costs 0 in the plain form; ended after the light (`ALIGHTEND`: normals, occlusion, own light, dynamic
  lights) -1.4, so **the light 1.4 ms**; before the glows, fog and output (`ARIMEND`) -1.0, so **the sheens 0.4 and the
  tail (glows, the gate's see-through, fog, OIT/temporal output) 1.0 ms**. Single parts of the light out gain nothing
  (the occlusion and dynamic lights by `vr_gl_alias_plain_mask` 256/512, the ambient `AAMB` -0.2, specular AA 0): its
  cost is the code together (registers).
- **GPU a frame, the final defaults** (one launch each; the fight's scene differs between launches):

| | e1m1 start | the fight frozen | dynamic res. |
|---|---|---|---|
| batch 2 (C4l's default, lite lights on) | 11.4 | 19.1 | |
| batch 2, lite lights off (now the default) | | 18.5 | |
| batch 4 (final: lite lights off) | 11.4 | 21.5 (its scene) | 14.2 |
| batch 4, lite lights on | 11.4 | 22.1 | |

- PC: gles_render_test 21/0, scenario_check 138/0, load_models_test 4/0, dev and LTO builds 0 warnings; the PC's
  `-glescompat` compiles every new probe (0 shader errors).
- Left running for his check: e1m1, `+vr_quest_preset 0` (this commit's APK).

## 6u. C4n: performance in real play; step 1, the perf recorder (2026-11-16; adb, worn by him)

His verdict (11-16): playable and looks OK; the problem is performance: frequent hitches and stretches where the game
struggles. The plan he approved: (1) an on-device recorder, then his recorded session; (2) every program compiled at
boot into the binary cache; (3) uploads off the frame; (4) fixed foveation through `XR_FB_foveation`; (5) more work on
the other cores; (6) the preset tuned from the numbers; (7, optional, off) SpaceWarp.
- **`vr_perf_record`** (new, 0, not saved; Debug > Profiling > Perf Recorder; `vr_perfrec.cpp`): 1 writes every frame
  to `perf_<date>_<time>.csv` (Android: the app's internal `files/perf/`, read by `adb exec-out run-as
  com.vittorioromeo.quakevr cat files/perf/<file>`; PC: `<game>/profile/perf/`), flushed a second at a time; 0 closes
  it. A row: period, host, CPU work, the runtime's display period and the periods it missed (`xrWaitFrame`'s predicted
  display times), dynamic resolution's scale, the GPU's span (first to last timestamp) and every always-on phase's CPU
  and GPU time (the profiler's timestamp queries, read back late, never waited for; hitch frames' too now, kept out of
  the profiler's sums), draw calls, dynamic lights alive, the main thread's heap events, and the frame's events with
  their counts, ms and first names: programs compiled / read from the binary cache (`GL_CreateProgramFromSources`),
  textures uploaded (`TexMgr_LoadImage32`), models (`Mod_LoadModel`), sounds (`S_LoadSound`), runtime lightmaps, map
  load commands, saves, decal bakes (tagged profiler scopes), navmesh jobs landing (their waits), the QuakeC (the
  outermost `quakec` scopes, now timed while recording too) and Box3D steps, and `VR_TimeAdd`'s work (`work`).
- **`Misc/quakevr/perf_report.py <csv>`**: the frame's mean/p50/p95/p99, frames over budget (CPU, GPU), the
  hitches (over 1.5 display periods, or periods missed) ranked by cause (the frame's or the frame before's events when
  their time explains 30% of the excess, else the CPU's or the GPU's costliest phase, else the runtime), every event
  kind's totals, the phases' means, the slowest seconds. Frames over 2 s (the app paused: the unworn headset asleep)
  are left out; `--map`, `--from-t/--to-t` select. `vr_perf_mark <label>` (new) marks a frame (an A/B's parts:
  `scratch/c4o_abrun.sh`, split by `c4o_ab.py`).
- **Scripted sessions** (unworn; `scratch/c4o_play.sh`: e1m1's start, a live fight of 5 grunts, 2 dogs and an ogre in
  god mode, an autosave, `changelevel e1m2` and a fight there, `playdemo demo1` (e1m3), the preset as shipped; the
  first run's player died before `god` took: the death menu's frames, discarded). The clean run (perf_..._223215):

| map | FPS | period p50/p95 | CPU work p50 (over budget) | GPU span p50 (over budget) |
|---|---|---|---|---|
| e1m1 + fight | 33.5 | 23.8 / 37.9 | 12.1 (35%) | 21.3 (81%) |
| e1m2 + fight | 33.9 | 14.8 / 27.3 | 9.5 (10%) | 11.1 (26%) |
| e1m3 (demo1) | 25.6 | 23.6 / 49.4 | 3.3 (1%) | 22.0 (96%) |

  **Hitch causes, ranked by the time over budget**: (1) the GPU over its budget, frame after frame (the world and brush
  pass the costliest: 13-55 ms in the worst frames; e1m1 ~13k ms over budget, e1m3 ~21k): the sustained cost, not
  spikes; (2) CPU spikes in a fight (`SV_Physics`, `view entities`: 20-90 ms; the CPU over budget in 35% of e1m1's
  fight frames); (3) one ~400 ms frame a map inside an eye's CPU (`screen`/`eye L`, no recorded event: something made
  lazily at a first draw, once a map: e1m1 t=15 s, e1m2 t=56, e1m3 t=142; to find with `vr_profile_hitch`); (4) loads
  at a spawn (the knight's sounds 81 ms, models 80-200 ms in the demo; the fight's own spawns, not real play). **Shader
  compiles mid-game: 1 in 4,500 frames (0.3 ms)**: the program cache already covers them (step 2 needs nothing more
  than the recorder's event); texture uploads mid-game: small (124 ms over the whole session, the worst 51 ms at a load).
  The body when in view costs a lot on the unworn headset (whose pose sees it): a run with `vr_body_mode 0` had the
  e1m1 GPU at 15 ms against 28 (not a clean comparison: that run's death menu).
- **Fixed foveation (step 4), not working yet**: `vr_gl_foveation` 0-3 in one launch (e1m1's start, 0.9 fixed, 240
  frames each): GPU 11.72 / 11.75 / 11.77 / 11.68 / 11.76 / 11.70 ms (0, 3, 0, 2, 1, 0): no effect with stronger gains
  (now 2/4/6, areas 0.5/0.25/0.1). The depth foveated too: the framebuffer incomplete (0x8BFF), reverted. **Why**:
  the foveated texture is the "composite colors", but `R_SetupGL` draws the scene into `framebufs.scene.fbo` whenever
  `GL_NeedsSceneEffects()` (and single-pass stereo draws into its own layered targets, `VR_StereoSingleEyeTargets`):
  the scene's real target is not foveated. Next: foveate the target the eye's scene is drawn into (the scene fbo's colour,
  or the single-pass array's layers per eye), then A/B again with `c4o_abrun.sh`.
- At e1m1's start (0.9 fixed, the body as the headset's pose shows it) the frame holds 72 Hz: period 13.9 ms, GPU 11.7,
  CPU 6.8: the struggle is in fights and busy maps (above).
- PC check (mock, e1m1, 400 frames): 400 rows, 397 with GPU times; the load's first frame (992 ms) and a decal frame
  found as CPU hitches; 0 warnings.

## 6v. C4o: foveation ruled out on our targets; the hitches were the runtime's; the fight's GPU is its big lights (2026-11-16; adb, unworn)

Method (`scratch/c4p_*`): A/Bs in one launch (`c4o_abrun.sh`); **live** fights (`c4p_fight.sh`: e1m1's hall, 5 grunts,
2 dogs and an ogre, god mode, the preset as shipped, 600-900 frames a part) compared at equal dynamic-light counts
(`c4p_abcorr.py`: each part's medians by the frame's `dlights`), since a live fight's parts differ in what happens;
frozen fights (`sv_freezenonclients 1`, 0.9 fixed) for the rest; `ovrgpuprofiler -t` traces (`c4h_gpuprof.sh`).
- **Foveation (step 1): no effect on any target of ours; dropped for C5.** `GL_QCOM_texture_foveated` on the eyes' scene
  colours (now the scene fbo's too, and every focal point set: the driver gives a texture 2, and an unset one's density
  could be full everywhere) takes (bits 3, offered 7, min density 0, no GL error) but changes nothing: GPU (frozen
  fight) 14.27 / 14.34 / 14.29 ms (off, high, off), the world 11.78 / 11.74; R11F_G11F_B10F and RGB10_A2 alike; the bins
  in `ovrgpuprofiler -t` the same (8 of 768x448) on or off; eyeshots the same. Foveating the depth/stencil too
  (`vr_gl_foveation_parts`, new, Debug > Profiling: 1 colours, 2 + depth, 4 + OIT) makes every eye framebuffer
  incomplete (0x8BFF), the composite one included: they are now made again without foveation (it was a `Sys_Error`).
  So on the Quest 3's GLES the driver foveates only the runtime's swapchain images (6's research): the scene has to be
  drawn into them (C5's multiview into the swapchain, which also needs the tone curve in the scene's shaders). And
  **`vr_xr_foveation` changed mid-game crashes in `xrEndFrame`** (the runtime's GL; twice, with and without
  `ovrgpuprofiler`'s layer): set at launch it worked (C4b); open.
- **The ~400 ms once-a-map frame (step 2) is the runtime's**: each one follows the session stopping and resuming (the
  unworn headset asleep and woken by `c4e_run.sh`'s wake-ups: `STOPPING, IDLE, READY, FOCUSED`), 0.1 s after `READY`
  (e1m1 t=15 s of C4n's session); `vr_profile_hitch` shows `xr acquire` 433 ms, `input` 461 to 8,000 ms. The recorder
  now marks session changes (`mark:xr session <state>`) and `perf_report.py` puts a hitch within 2 s of one under
  `xr:session` (a live fight: 54 hitches, 4.0 s of its 9.3 s over budget). Also once a launch: its first focused frame,
  736 ms (`world+brush` 563, 3D text 88, the gadget screen 42 ms): before play, left.
- **The fight's CPU (step 3) is not the problem**: in live fights CPU work p50 8.0-8.4 ms, p95 13.3, over budget in 4%
  of frames; `SV_Physics` caused 7 hitches (109 ms over budget in 26 s), view entities 2. C4n's 20-90 ms (and 532 ms)
  spikes were the session's frames above. Not moved onto the job system: QuakeC is serial (determinism) and there is
  little left to win; revisit with his recorded session.
- **The fight's GPU spikes are big dynamic lights**: the world's GPU at scale 0.65 by the lights in the frame: 2 lights
  6.7-7.0 ms, 3: 11.6-13.2, 4: ~16 (`r_dynamic 0`: 7.0 in the 3-light frames; the 2 always there cost 0.25). Not their
  shadows (`vr_shadow_dlights 0` -1.2 ms in those frames; the shadow pass is the +90 draws), nor `vr_dlight_falloff`,
  `vr_dlight_uncapped`, `vr_dlight_angle`, `vr_pbr`, `vr_specular`, `vr_dlight_models`, the enemies' muzzle flashes
  (their light count unchanged): it is the world shader's light code over the pixels a big light (an explosion)
  covers. The next GPU lever.
- **Dynamic resolution (step 4)**: fights sit at the floor (`vr_dynres_log 2`: the eyes' GPU 9-15 ms mean at 0.65
  against its 11.8 ms budget). Floor 0.55 against 0.65 (live fight, alternating): frames over 1.5 periods 4.6 / 6.7%
  against 7.4-16.6%, the period's p95 20.7 / 21.3 against 22.1-27.6 ms, GPU p50 8.5 against 9.3-11.8, a 3-light
  frame's world 9.5-10.0 against 12.2-13.2. **The preset's floor now 0.55 / 0.5 / 0.5** (was 0.65 / 0.6 / 0.55);
  `vr_render_scale` 0.9 kept (e1m1's start holds 72 Hz at 0.9: GPU 11.7 ms). SpaceWarp (step 5): not started.
- PC: gles_render_test, scenario_check, load_models_test, dev and LTO builds: see ROUND25's C4o section.
- Left running for his check: e1m1, `+vr_quest_preset 0 +vr_perf_record 1` (this commit's APK).

## 6w. C4p: big lights' cost was the shadow code beside them; the plain lights' loop; the runtime's foveation guarded; C5's probe (2026-11-16; adb, unworn)

Method (`scratch/c4q_*`): **`vr_test_dlights <count> [radius] [distance]`** (new; Debug > Profiling: "Add 3 Big Test
Lights", "Remove the Test Lights"): lasting lights as big as an explosion's (350, x1.5 `vr_explosion_light_scale`) in a
row 160 units ahead, so a big light's cost is measured on a still scene (e1m1's fight hall frozen, 0.55 fixed, the body
off) in one launch (`c4q_diag1-3.sh`), probes by `vr_gles_define`; then a live fight (`c4q_fight.sh`, the preset as
shipped, `c4p_abcorr.py`).
- **Why a big light costs so much on Adreno**: not its work but the code beside it. World GPU at 0.55 (both eyes):
  0 test lights 4.5-5.1 ms, 1: 9.2, 3: 14.7 (~+3.4 ms each covering the view; the cluster lists are fine: 32x16x32,
  a light in a pixel's cluster is the loop's one dynamic cost). Probes with 3 lights: the loop compiled out 3.0; a plain
  point light (`SIMPLELOOP`) 3.6-3.8; no shadow code (`NOSHADOW`) 8.7; no portal path (`NOGATE`) 11.9; the light's
  members read one by one (`LEANLOAD`) 13.2; the lights' lite form 11.8; **shadows off at run time** (`vr_shadow_dlights
  0`) **only -0.6**; a short path for plain lights inside the full loop (`FASTLIGHT`) 12.3 (with the lite form 13.3: more
  code, slower). The radius (350 against 525) changes nothing here (the view covered either way). So the shadow, portal,
  map-shadow and DarkPlaces code compiled into the loop makes every light's iteration slow (registers: latency not
  hidden), run or not.
- **The plain lights' loop** (`vr_gl_lights_plain`, new tunable, 1; ES lite bit 26, `QVR_LITE_PLAINLIGHT`; Debug >
  Profiling row): with `vr_gl_world_lite` 2, when no light can need that code (`vr_shadow_dlights` 0,
  `vr_shadow_maplights` 0, `vr_dlight_falloff` 0, no portal lights) the world's loop is Quake's falloff, a spot's cone,
  the angle and the sheen alone. 3 big lights: world **14.7 -> 7.1-7.6 ms**, the GPU span 16.7 -> 8.8-9.3 (0 lights 4.5
  -> 4.3). **The Quest preset's `vr_shadow_dlights` now 0** (was 1 / 1 / 0): no dynamic light casts shadows on the
  Quest (explosions, the flashlight, taken torches); the PC unchanged. A settings change that needs the code rebuilds
  the shaders (the bit changes).
- **Live fight** (dynamic resolution at the 0.55 floor, alternating old = one shadowed light and the full loop, new):
  period p50 14.1-14.4 ms (72 Hz held), CPU 8.2; the world 5.3-5.8 ms in 2-light frames either way; 3-light frames
  9.6 / 9.8 old against 9.0 / 8.6 new; GPU span p50 8.2-11.4 (under 13.9); frames over 1.5 periods 3.4-7.7%. A real
  fight's third light is mostly small or brief (the still scene's three full-view lights are the worst case: 9.3 ms
  span now). The span grew through the session in the left eye's own scope alone (4.3 -> 7.1 ms, no sub-scope: idle
  time inside it, the runtime's waits), not the lights.
- **`vr_xr_foveation` (step 3)**: changed mid-game it now makes new eye swapchains (`QuestExtensions::wantNewEyes`; the
  update on swapchains in use crashed) and that works (several times a launch: the EGL check `contextReady` now accepts
  the session's context current with the window's surface; making the session's own surface current failed). But
  **foveated eye images crash the runtime in `xrEndFrame` anyway** (SIGSEGV at 0x30 in libGLESv2_adreno under
  libvrapiimpl), set at launch too: at once with late acquire or the image's invalidate, after ~100 frames without them,
  in the post-process path and the direct one. **Guarded**: a level is applied only with `-xrfoveation` on the command
  line (a console note otherwise); with it, late acquire and the image's invalidate are off. Default 0 as before.
- **C5's first path** (`vr_xr_direct`, new, 0, not saved; Debug > Profiling "Scene Straight into the Headset's Images";
  ES only): each eye's scene drawn into its swapchain image (the image in place of the composite target's colour; 2: an
  FBO of its own with a depth renderbuffer), no post-process (no tone curve: colours over 1 clip; sRGB writes are off on
  the Quest, so the scene's colours land as the post-process's would without the curve), one pass an eye (single-pass
  stereo off), the image acquired first, only where the eye fills the image or its lower left part (`vr_eye_subimage`).
  Works: the post-process's 1.0 ms gone (GPU span 13.2 -> 12.3 at 0.9, single pass off). **Foveation through it: no
  effect** before the crash (world 10.7-10.9 ms with level 3 against 10.8-11.0 off), so C5's premise (the runtime's
  foveation reaching the scene) is unproven on the Quest 3's GLES; the depth renderbuffer form (2) crashed before its
  part. Design for C5 proper, if pursued: multiview (single pass) into 2-layer array swapchains, the tone curve and
  grade in the scene's last writes (world, models, sprites, particles; translucents blended after the curve), the UI
  into the same images, dynamic resolution by the layer's `imageRect` (already: `vr_eye_subimage`).
- PC: gles_render_test, scenario_check, load_models_test, dev and LTO builds: see ROUND25's C4p section.
- Left running for his check: e1m1, `+vr_quest_preset 0 +vr_perf_record 1` (this commit's APK).

## 6x. C4q/C4r: the plain loops with DarkPlaces' falloff; foveation's crash narrowed; the Quest's saves fixed (2026-11-16)

Method (`scratch/c4r_*`, C4q on the headset, unworn; C4r headless: the headset was offline): `c4r_frozen.sh` (6w's
still scene, 3 big test lights, 0.55 fixed, body off), `c4r_fight.sh` (a live fight, the preset as shipped),
`c4r_fov.sh` (foveation at launch), `c4s_check.sh` (C4r's checks, for the next session with the headset).
- **The plain loop was never on with the Quest preset**: it sets `vr_dlight_falloff` 1 (DarkPlaces' falloff), which
  6w's bit excluded. Now the plain loops take either falloff, and the models' loop has the plain form too (`vr_glsl_alias.h`
  `ModelDynamicLights`; `QVR_PROBE_NOAPLAIN` undoes it). Still scene, 3 big lights (GPU span / world, ms): full loops
  16.0-16.1 / 14.3-14.4, plain 9.8-10.3 / 7.7-8.3 (0 lights 6.2-6.4 / 4.6-4.9); the models' plain form alone: no measurable change
  (+-0.5). Live fight (dynamic resolution at the 0.55 floor): period p50 14.0-14.2 ms either way (72 Hz held), GPU span
  p50 9.4-10.4 against the full loops' 9.5-10.0, frames over 1.5 periods 1.3-2.2% against 3.0-3.3%, 3-light frames' world
  6.1-6.2 against 8.7-9.3 ms. The recorder's header now lists the Quest's tunables (`Cvar_FindVar` never found them).
- **Foveation's crash (`vr_xr_foveation`, guarded by `-xrfoveation`)**: two probes, not saved: `vr_xr_foveation_flags`
  (1, the scaled bins as before; 0 none, Meta's GLES sample's, read when the eye swapchains are made) and
  `vr_xr_foveation_probe` (bits: 1 late acquire and the invalidate allowed anyway; 2 the eye image released before it
  is detached, the old order). The Quest now detaches the eye image and binds no framebuffer of ours before
  `xrReleaseSwapchainImage`, as Meta's samples do (the PC keeps its order). One launch at level 3 (flags 1, late acquire
  off): 1,509 frames over four parts, no crash (6w's crashed after ~100); the GPU no lower (world 9.7 ms; the scene
  isn't drawn into the foveated images outside `vr_xr_direct`). The final APK's form is unchecked (`c4s_check.sh`).
- **The Quest's saves failed** ("Save error. string -14: a freed engine string", every e1m1 autosave): not a freed
  string but arm64's pointer tag. `SaveData_KnownString` rejected a pointer with bits above 48 set, and Android's
  malloc keeps a tag in the top byte (TBI, `0xb4...`), so every malloc'd engine string was saved as none and the save
  stopped. The check now ignores the top byte on arm64 (the PC's x64 code unchanged). Unchecked on the headset.
- PC: gles_render_test, scenario_check, load_models_test, dev build 0 warnings; Android: the APK builds (ROUND25).

## 6y. C4s: the Quest's saves work; foveation no longer crashes; the recorded session again; e1m3 was the demo (2026-11-16; adb, unworn)

Method (`scratch/c4s_*`): `c4s_check.sh` (save, load; foveation at launch), `c4o_play.sh c4s demo` (C4n's scripted
session, the preset as shipped), `c4s_e1m3.sh` (e1m3 played: a noclip tour of 8 places at 0.55 fixed, body off),
`c4s_gapab.sh` + `c4s_ab.py` (live-fight A/Bs with the eyes' GPU scopes), `c4s_gap.py`, `c4s_gcols.py` (a CSV's
scopes by map).
- **Saves (C4r's fix) work on the headset**: e1m1's autosaves ("Saving game to autosave/e1m1.sav", no "Save error"),
  `save` then `load` of the same game.
- **Foveation (`-xrfoveation`, level 3, the scaled bins, detached before the release)**: four parts, 1,041 frames, no
  crash (0 `SIGSEGV`); GPU span 11.4 ms, world 9.1 at 0.9 (the post-process path: no gain, as 6w). Still guarded.
- **The scripted session again** (the plain loops, the 0.55 floor), period / GPU span p50 against C4n's (ms): e1m1
  (walk and fight) 18.4 / 15.9 against 23.3 / 20.7; e1m2 14.1 / 10.1 against 14.7 / 11.1; e1m3 (demo1) 19.4 / 18.1
  against 23.6 / 22.0. Hitches: GPU world+brush 1,284 (13.4 s over budget in 118 s, was 36.4 s over 2,482), the
  rest the unworn headset's session (298) and the changelevel's load.
- **e1m3's cost was the demo, not the map**: during `playdemo` the frame has no eye passes and submits nothing to the
  runtime (`c_xr_submit` 0, a `swap`: the flat view drawn), so C4n's e1m3 row measured a flat render the headset never
  shows. **e1m3 played** (8 places, 0.55 fixed): period 13.9-14.1 ms everywhere (72 Hz held), GPU span 4.7-11.0, world
  3.3-9.3 (the east room the worst). Open: demos show nothing in the headset (a flat-only path); not perf.
- **The left eye's extra GPU is not a steady cost**: the frozen fight has none (vr_profile: eye L 2.92, eye R 2.72 ms
  at 0.55); a live fight's medians differ by 0.3 to 2.6 ms between parts of one launch, with no link to decals or
  particles (`vr_decals 0`, `vr_particles 0`, both: -1.2 to +2.6); the session's e1m1 7.2 against 7.0. The xr acquire
  wait inside the eye is ~0 (`c_xr_acquire` p50 0.02, the eye's GPU no larger in frames that waited). Not pursued.
- **vr_profile's means on an unworn headset**: the frames while the session is stopped are drawn flat and their
  timestamps are garbage (scene setup 99 ms mean, max 14,800): use the recorder's medians (`c4s_ab.py`).
- No code change; PC unchanged.

## 6z. C4t: e1m1 place by place; the world without the lights' loop; demos in the headset (2026-11-16; adb, unworn)

Method (`scratch/c4t_*`): `c4t_e1m1.sh` (10 places of e1m1 by `setpos`, noclip, 0.55 fixed, each with the body on then
off), `c4t_stages.sh` (the lite world shader cut by stage on the hall's still view at 0.9: `QVR_PROBE_CUT1/2/3/5`,
`NOLOOP`, `r_dynamic 0`), `c4t_unlit.sh` (the places with `vr_gl_world_unlit` 1 then 0), `c4i_cap.sh` + `c4t_capcfg.py`
(the compositor's picture, unlit against lit), `c4t_bins.py`, `c4t_bydl.py`, `c4t_cmp.py`, `c4t_maps.py` (CSV splits).
- **e1m1's walk is not the problem**: every place holds 72 Hz at 0.55 (period 13.9-14.2 ms, GPU span 4.7-8.4, world
  3.4-6.8). C4s's e1m1 row was (1) ~12 s of the unworn headset's asleep start (no eye passes, the flat view drawn at
  0.9: GPU 25.9 ms) and (2) the scripted fight (world 11.6-13.7 ms at 0.55, 220-240 draws: more of the map's own
  monsters joined it than in the A/B fights' 160-177 draws). **The body**: +0 to +0.9 ms at most places, +2.6 at one
  (the stairs; it is drawn inside the world's scope, `vr_alias_player_first`); a live fight's body on/off: 8.9 / 8.0,
  8.6 / 10.0 ms span (noise-sized).
- **The lite world's cost by stage** (hall, 0.9, world ms): texture 3.9; + decals and the permanent layer 0.9 (no marks
  laid; 1.3 with 128); + lightmap 0.6; + the light code (contrast, cap, sheen) 0.3; **+ the lights' loop 4.4**, of which
  the lights themselves 0.4 (`r_dynamic 0`: 10.8 against 11.2): the loop merely compiled in costs ~4 ms (Adreno:
  registers), and a walk has only the guns' screen lights (28 units, at the hands).
- **`vr_gl_world_unlit`** (new tunable, 1; Debug > Profiling row "Unlit World Shader (Quest)"; ES only): R_PushDlights
  tests each light's sphere against the world's BSP (R_MarkLights' walk, the surfaces by their bounds; not sky or
  liquids) and the brush entities' bounds; a shadowed map light or a portal light counts as reaching. None reaching:
  the world's programs without the loop (`glprogs.world_unlit[dither][alphatest]`, made under the ES profile only, with
  their array twins). The picture the same by construction (a light adds nothing past its radius). **Adreno**: with the
  loop out, the cluster inputs (`in_coord`, `in_depth`) went unread and the Quest 3 drew garbage (one pink blocky
  texture over the view: `c4t_pair_ul.png`); they are now read under a branch never taken, and the capture matches the
  lit one (`c4t_pair_ul2.png`). The recorder has an `unlit` column (views a frame).
- **Gain**: places it applies to (6 of 10; the others had a screen light on a wall): world 4.1-6.3 -> 2.4-4.2 ms, GPU
  span -1.7 to -2.9 ms at 0.55. The scripted session (`c4o_play.sh c4t demo`, eye-pass frames, against C4s's):
  **e1m1 period p50 16.9 -> 15.1 ms, GPU 15.1 -> 12.4, world 12.0 -> 9.1** (unlit in 77% of its frames, the fight's
  too); e1m2 unchanged (14.0 / 10.6: its lights reach the walls).
- **Demos in the headset (fixed)**: `hands::update` skipped every client without `PRFL_QUAKEVR`, so in id's demos (protocol
  15) `VR_RenderView` found no head and only the flat view was drawn. Demo playback now places the view; demo1 now
  has eye passes in 98% of its frames. Its GPU: span p50 20.9 ms, the world only 3.0: the eyes' scopes hold ~15 ms
  with no sub-scope, CPU waiting 17 ms in `xrWaitFrame` (period 21.5): not yet understood.
- PC: dev build 0 warnings, gles_render_test 21/0; the unlit programs under `-glescompat` on the PC: eye identical
  (but the mock's lights reached the world: the path itself is checked on the headset). PC programs unchanged.

## 6za. C4u: demo1's GPU was the player's own models; lights out of the view's PVS (2026-11-16; adb, unworn)

Method (`scratch/c4u_*`): `c4u_ab.sh` (an A/B in one launch on demo1 or a map; `ROUNDS=n` interleaves the variants n
times, pooled by `c4o_ab.py` `POOL`; the demo's own timeline swings its GPU 12-23 ms, so only interleaved parts compare),
`c4u_play.sh` (C4n's scripted session and its maps), `c4u_pcdemo.sh` (demo1 on the PC: the window against C4s's, and
under `-glescompat`), `c4u_col.py`. New debug command **`vr_dlights_list`** (Debug > Profiling "List the Dynamic
Lights"): each live light's key, radius, distance and colour.
- **demo1's "unattributed" eye GPU**: the eye scopes' GPU times are not where the work is on the Adreno (a tiler runs a
  render pass's draws when the pass resolves: the sub-scopes read ~0, the eye's scope holds the pass; `g_alias` read
  0.01 ms with ~50 models drawn). The A/B: `r_drawentities 0` 22.2 -> 3.5 ms span, `vr_dlight_models 0` -> 12.0,
  `r_dynamic 0` -> 10.7; the body, `vr_gl_alias_plain 2`, blob shadows, torch lights: no change. The models were the
  player's own: C4t's demo fix (hands placed in id's demos) drew the whole rig in the demo (hands, body, two guns,
  four holsters, the worn gear, from the controllers' poses) with its lights at the face (the guns' screen light, the
  lava gun's, the wrist gadget's two), lighting them per pixel. The 17 ms `xrWaitFrame` was the GPU-bound wait.
- **Fix** (`hands::demoViewOnly`): in a demo of another protocol the head is placed (the eyes drawn) but the player's
  view entities hidden (their lights go with them); and only on the Quest or under `-glescompat`: the PC's window
  showed the eye's mirror instead of the flat demo (its HUD and gun) since C4t: back to before (the window's
  screenshot at the same frame 0.000% different from C4s's build; the PC headset blank in id's demos, as before).
  **demo1 on the Quest 3: period p50 21.5 -> 14.0 ms, GPU 20.9 -> 6.5** (98% eye-pass frames; at the 0.55 floor: 4.3).
- **The world where lights reach it**: e1m2's start (0.55 fixed) has 4 lights: 2 torches 720 units off and the rig's 2
  (the screen and gadget lights, at the unworn controllers); the rig's off: still lit (the torches); all off: unlit,
  GPU 7.0 -> 4.9 ms, world 5.0 -> 3.1. The reach test (C4t) walked the whole BSP: a light round a corner counted.
  `VR_WorldLightsCheck` now also needs the light's sphere to reach a leaf of the view's PVS (`R_ViewVis`, the copy
  R_MarkSurfaces keeps; both eyes' under single-pass stereo): conservative (every drawn pixel is in such a leaf).
  On e1m2's start it changed nothing (still lit: 6.8 ms either way; the torches' leaves are in its PVS, and the rig's
  lights touch the floor); kept as a correct, cheap cut for rooms whose torches are out of sight.
- **The scripted session** (`c4u_play.sh`, eye-pass frames, against C4t's): demo1 above; e1m2 14.1 / 10.7 ms (14.0 /
  10.6); e1m1 16.7 / 15.0 (15.1 / 12.4), unlit 27% of its frames against 77%: the unworn controllers lay elsewhere
  this time and their small lights touched the floor (the rig's lights decide the unlit share in an unworn session;
  worn, the hands' lights reach a wall only near one). The scripted fight's monsters (220-255 draws) were not
  separated from that: the demo's models were the rig's, not monsters'.
- PC: dev build 0 warnings; scenario_check PASS (145 run, 0 failed), load_models_test 4/0, flat `timedemo demo1`
  1834 frames, the PC's demo window identical to C4s's.

## 6zb. C4v: the world split by surface where lights reach it; the rig's small lights' switch; the fight's monsters (2026-11-16; adb, unworn)

Method (`scratch/c4v_*`): `c4v_pc.sh` (under `-glescompat` on the PC: a map's start with the Quest preset, the left eye
with `vr_gl_world_unlit` 0, 1, 2 and 2 with `vr_rig_lights_world 0`, compared), `c4u_ab.sh` interleaved (`ROUNDS`) on
e1m2's start and on C4k's frozen e1m1 fight (`c4v_fight.sh`), `c4u_play.sh c4v` (the scripted session).
- **The split by surface** (`vr_gl_world_unlit 2`, the new default; ES only): under the ES profile the map's load makes a
  second indirect command for each of the world's solid and fence textures (`gl_bmodel_litcmd_first`, r_brush.c), with
  a copy of the world's index ranges after every other index (the runtime brushes' slots kept). The world's GPU cull
  (`cull_mark` with `QVR_LITSPLIT`, the ES form only) now runs after R_PushDlights (`R_MarkSurfacesLate`: the view's
  inputs taken in R_MarkSurfaces) and sends a surface whose box a reaching light's sphere touches (up to 16 spheres from
  `VR_WorldLightsCheck`; more, a shadowed map light or a portal light: all lit as before) to its texture's lit command.
  The opaque and fence passes draw the unlit commands (and the brush entities unless a light touches one) with
  `glprogs.world_unlit`, the lit ones with the world's program, **in one flush** (one gather: `R_BModelCallsSegment`;
  the array twins' groups kept per program; a lit call's vertices read their texture's command's call, the same
  data); the depth pre-pass draws both with its one program. No light reaching: the whole-view unlit program as in C4t.
- **PC equality** (`-glescompat`, the mock): e1m2 (the torches reach the walls: split in both views) and e1m1 (the
  guns' screen lights): the left eye with the split and unlit against all lit: max 4 of 765, 0.000% over 8. The PC's
  own programs and order unchanged (the cull's dispatch moves only when the lit commands exist: the ES profile).
- **e1m2's start** (0.55 fixed, 4 rounds interleaved, GPU span / world brush ms): all lit (`vr_gl_world_unlit 1`) 5.76
  / 4.08; **split 5.17 / 3.41**; split with `vr_rig_lights_world 0` 4.83 / 3.10; every light off (`r_dynamic 0`, the
  floor) 4.32 / 2.70. The split costs 18 draws a frame more (the lit part's groups and empty commands; 160 -> 178).
- **The rig's small lights** (`vr_rig_lights_world`, a cvar, default 1 = the current look; a Quest preset row, "1" at
  every level: his call): 0 marks the guns' screen lights and the wrist gadget's two (`lighting::dlightRig`;
  `VR_DlightShadow`: `shadow2.w` -1, unread for an unshadowed light) so the world's plain loop skips them
  (`QVR_LITE_RIGNOWORLD`, bit 27 of the lite form; only with `vr_gl_lights_plain`'s plain loop) and the reach test
  ignores them; they still light the hands and gear. **It would save ~0.34 ms at e1m2's start (5.17 -> 4.83), and on
  e1m1's walk it makes most views unlit** (the PC mock: e1m1's start went from split to unlit). Debug > Profiling: "Rig
  Lights Off the Walls (Quest)" / "Rig Lights on the Walls".
- **The fight's monsters** (C4k's frozen fight at e1m1: 6 grunts and 3 dogs closed in, the body off, 0.55 fixed, 4
  rounds): GPU span 7.38 ms, 231 draws; `r_drawentities 0` 3.26 (98 draws); the same place without the monsters 4.79
  (148 draws), 3.33 without entities: **the 9 monsters cost ~2.7 ms of GPU (~0.3 each) and 71 draws**; the models'
  plain form off (`vr_gl_alias_plain 0`) +3.1 ms (it already earns its keep); `vr_dlight_models 0` and the split
  (`vr_gl_world_unlit 1`) no change (the fight's views were unlit: the rig's lights touched nothing).
- **The scripted session** (eye-pass frames, period / GPU ms, against C4u's): e1m2 14.1 / 10.0 (14.1 / 10.7); e1m1
  16.1 / 14.5 (16.7 / 15.0; unlit 24%); the demo (e1m3) 14.0 / 6.1 (14.0 / 6.5). The recorder has a `split` column
  (views a frame drawn split).
- PC: dev build 0 warnings; gles_render_test 21/0; scenario_check PASS (145 run, 0 failed); load_models_test 4/0.

## 6zc. C4w: the monsters' draws batched, the world models' pre-pass off on the Quest, the split's empty lit draws (2026-11-16; adb, unworn)

Method (`scratch/c4w_*`): `c4w_pcfight.sh` (C4k's frozen e1m1 fight under `-glescompat` on the PC: the profile tree's
draws), `c4w_pceq.sh` (its left eye across variants, compared), `c4w_pclit.sh` (e1m2's start, the split's draws and eye),
`c4w_fight.sh` / `c4w_m2.sh` on the headset (`c4u_ab.sh` interleaved, 4 rounds, 0.55 fixed). The unworn headset now
lies looking 70 degrees down (`viewpos`): the fight's grunts were out of view, so `c4w_fight.sh` lifts the player 110
units (noclip) once the fight is frozen. **`vr_gl_passes`** now also lists the frame's alias batches (model, instances,
draws, entity, skin).
- **What the monsters drew**: every world model's batch is 2 draws (its depth pre-pass, then its shading; the Quest
  draws the eyes apart, so the multiview skip of the pre-pass never applied), and the 6 armed grunts were 6 batches
  (posed: their own bones), their 4 guns 4 batches drawn between them, the armour and its leather 3 batches each.
- **The alias sort key** (a PC bug, Ironwail's): `Mod_LoadAliasModel` masked the name's hash with `MODSORT_FRAMEMASK`,
  16 keys for every alias model, so models sharing one were drawn in entity order, one batch each (the grunts and their
  guns interleaved); and the models made in memory (`VR_SyntheticModel`: "#rig2", "#armor", "#bridge") kept no key of
  their own. Now `Mod_AliasSortKey` (15 bits of the hash, the alpha-test bit kept) for both: the guns one batch.
- **Posed models batched** (`vr_alias_batch_posed`, default 1, Debug > Profiling): the world's posed models of one
  model and skin (not your own) with as many bones, the same surfaces and skin mask join one batch; their bones go into
  one buffer, each instance's poses offset to its own (`R_AliasPosedJoin`, `R_FlushAliasInstances`): the 6 grunts 1
  batch, the armour 2 (two skins), its leather 2. PC eye against off: 0.000% over 8 (max 1).
- **The world models' pre-pass** (`vr_alias_prepass_world`, default 1 = the PC as before; **the Quest preset 0**): off,
  each batch is shaded as drawn (the Adreno's own early depth test does the rest). PC eye: 0.000% over 8 (max 1).
- **The headset** (the lifted fight, GPU span / draws): all three before 4.81 / 219; **now 4.00 / 149**; no entities
  2.61 / 108: the fight's models from 2.2 to 1.4 ms. Apart: the pre-pass off -0.49 ms and -20 draws, the posed batching
  -0.11 ms and -20 draws (the sort key's guns can't be toggled).
- **The split's empty lit draws** (`vr_gl_world_lit_skip`, default 1, Debug > Profiling; ES only): on ES each command
  is a draw of its own, so the split's lit commands (one per solid or fence texture) cost a draw each even when empty.
  `R_WorldLitTextures` (r_world.c) walks the view's PVS leaves on the CPU (each leaf's box round its surfaces', made at
  the map's load) and marks the textures with a surface whose box a reaching sphere touches (the cull's own test, a
  unit wider: a superset); only their lit commands are drawn. PC `-glescompat` e1m2: 174 -> 158 draws (both eyes),
  eye 0.000% over 8 (max 2). Headset e1m2 start: 192 -> 180 draws, GPU 5.40 -> 5.34 ms.
- Left at 1: `vr_rig_lights_world` (his look call).
- PC: dev build 0 warnings; gles_render_test 21/0; load_models_test 4/0; scenario_check PASS (138 run; a first run lost bench_crowd_ad's placed things once, the rerun passed).

## 6zd. C4x: the live fight's models were the whole shader (blood); the gore form; the frozen fight was not the session's (2026-11-16; adb, unworn)

Method (`scratch/c4x_*`): `c4x_fight.sh` (C4w's frozen fight with the ogre, not lifted, the body on: the session's own
view), `c4x_live.sh` (the same fight live: god mode, the monsters attacking, the ogre's grenades; `c4u_ab.sh`
interleaved), `c4x_trace.py` (a live frame's alias batches on the headset), `c4x_rounds.sh` (an A/B's rounds unpooled),
`c4x_buckets.py` (a session CSV's map in time buckets), `c4x_pcgore1.sh` (PC `-glescompat`: one launch, the switch off,
on, off, eyes compared). **`vr_gl_passes`** now gives each alias batch's program (full, opaque, plain, gore), its
instances' feature bits (`VR_AliasPlainNeeds`) and its triangles.
- **The frozen lifted fight's models** (C4w's ~1.1-1.4 ms): every monster already drawn by the plain form, 700-1000
  triangles each (the grunts' rig 694, the armour 2104, a dog 1028): not vertex-bound. The probes moved less than the
  launch-to-launch noise (~0.3 ms; rounds within a launch drift ~0.3 with the scene): no LOD worth making there. A
  first reading of the dynamic occlusion's untaken code (0.73 ms) did not repeat. The Quest preset has `vr_ao_dynamic`
  and `vr_ao_brush` 0, so the models' programs now compile `DynamicAO` out as the world's do (`QVR_ALITE_NOAO`, lite bit
  8; exact: no occluder means 1): no measurable gain, kept as a free cut.
- **The session's e1m1 was not that scene**: its fight sat at the dynres floor (0.55) at 15-19 ms of GPU. The same
  fight frozen as the session sees it (not lifted, the body on): 6.3 ms (entities 2.8, the body 0.3, view models 0.4,
  lights 0.4). Live: **12.7 ms, entities ~8.5** (`r_drawentities 0` 4.2; `r_dynamic 0` -1.6; `r_particles 0` -1.2).
  The live trace: the grunts, their guns and packs, the dogs and your hands and guns needed the wounds (bit 4: a
  wound mask or the wet coat) or dents (8), so they fell off the plain form to the whole shader (`vr_gl_alias_plain
  2`, every solid model plain, was -1.8 ms; the plain form keeping wounds and dents -0.7).
- **The gore form** (`vr_gl_alias_gore`, tunable, default 1; ES only, as the plain form: Debug > Tunables): a solid
  batch the plain form can't draw that needs only wounds and dents among its features is drawn by
  `glprogs.alias_gore` (the plain form's defines minus `VR_ALIAS_GORE_NEEDS`; r_alias.c, vr_render.cpp
  `VR_AliasGoreUse`). Your body (skin tone) keeps the whole shader. **Live fight, 6 rounds: GPU 12.90 -> 11.76 ms**,
  period 15.1 -> 14.8. PC `-glescompat` one launch, off/on/off: on 0.016% over 8 against off, the frozen scene's own
  drift 0.015% (the hands drawn by it).
- **The scripted session** (eye-pass frames, period / GPU ms, against C4v's): e1m1 **14.3 / 10.7** (16.1 / 14.5; unlit
  55% against 24%: the unworn controllers' small lights decide that share, so part of it is where they lay); e1m2
  14.0 / 9.6 (14.1 / 10.0); the demo (e1m3) 14.0 / 6.1 (14.0 / 6.1). e1m1's fight still sits at the 0.55 floor at
  10-12 ms of GPU, its period 14-15.
- Tried and dropped (`scratch/c4x_vertexao.patch`): the dynamic occlusion worked out at the models' vertices; moot while
  the Quest preset has it off.
- PC: dev build 0 warnings; gles_render_test 21/0; scenario_check PASS (138 run, 0 failed); load_models_test 4/0.

## 6ze. C4y: the live fight split; the skin form; the models' vertex light measured and left off; remakes' noise (2026-11-16; adb, unworn)

Method (`scratch/c4y_*`): `c4y_split.sh` and `c4y_bisect.sh` (the live fight, `c4x_live.sh`, 6 rounds), `c4y_frozen.sh`
(**new**: the live fight run 500 frames, the monsters close and bleeding, then frozen and particles off: a stable scene
with the session's near monsters; `c4y_fz1..4.sh`), `c4y_pcskin.sh` / `c4y_pcvl.sh` (PC `-glescompat`, off/on/off eyes).
- **The live fight's split** (GPU ms, 6 rounds): base 10.72, no entities 4.32, every solid model plain 9.39, plain with a
  constant fragment shader 5.33 (so the models ~6.4: ~1 of geometry, the rest their fragment shading), the body off
  -0.17, `r_dynamic 0` -0.36, `r_particles 0` and `vr_particle_halfres 0` nothing (C4x's lights ~1.6 / particles ~1.2
  were that launch's scene). **The live fight can't bisect**: its rounds drift by 1.5-3 ms as monsters close in and die
  (the bisect's parts swapped order round to round). The frozen live fight holds to ~0.1 ms within a launch.
- **The frozen live fight's plain form, bisected** (early-return probes, every solid model plain): constant 7.90, after
  the look 8.43, after the light 10.24, whole 11.18 (no entities 6.48): the per-pixel light ~1.8, the tail ~0.9.
- **The skin form** (`vr_gl_alias_skin`, tunable, default 1; ES only): a batch the gore form can't draw that needs only
  skin tone (gloves, nails) besides wounds and dents is drawn by the plain form keeping those (`glprogs.alias_skin`,
  `VR_ALIAS_SKIN_NEEDS`, `VR_AliasSkinUse`): your body. PC `-glescompat` eyes 0.000% over 8 off/on/off; headset +0.02 ms
  (nothing measurable: the body costs ~0.2 ms in all).
- **The models lit at their vertices** (`vr_gl_alias_vlight`, tunable, **default 0**, a probe; lite bit 28, only with
  plain lights and no dynamic occlusion): the plain, gore and skin forms' own light (ambient cube, occlusion) and dynamic
  lights worked out in the vertex shader (`QVR_ALIAS_VS_VLIGHT`; retro lighting keeps its levels, not its blocks; no
  sheen from the lights). PC eyes 0.000% over 8. **No gain on the headset**: every solid model plain 8.37 / 8.37 ms
  (4 rounds each, all within 0.1), the shipped forms within the remake noise. The probes' "light" step is not its math.
- **A remake of the programs is noise by itself**: the same settings, the shaders made again before each part (an unused
  `vr_gles_define`), the frozen scene's GPU jumps +0.3-0.6 ms in some parts (fz4; fz3's base 9.1 / 10.3 bimodal). Any
  switch that rebuilds the programs needs its gain consistent over every round, beyond ~0.5 ms.
- **SpaceWarp** (optional, not done): the runtime offers `XR_FB_space_warp` (startup log). It needs a motion-vector and
  a depth swapchain an eye (the runtime's recommended size), a motion pass (the TAA path's motion vectors, off on the
  Quest, are the start), `XrCompositionLayerSpaceWarpInfoFB` on each projection view, and the game at half rate. Feasible;
  not shipped.
- PC: dev and LTO builds 0 warnings; gles_render_test 21/0; scenario_check PASS (138 run, 0 failed); load_models_test 4/0.

## 6zf. The 1.1.0 refresh: a sideload package for his trip (2026-11-17; no headset)

- `origin/next` (the released 1.1.0, 5187f3e5) merged in: no conflicts (the Quest's ES paths and the PC's behaviour
  both as they were). The VR APK rebuilt (`scratch/c4b_build.sh vr`: 0 warnings, `check_apk.sh` 18/0, 20.8 MB).
- The package for a Linux laptop (`<kit>/scratch/quest_apk/`, made by `scratch/questpkg_make.sh`): `QuakeVR.apk`,
  `quakevr.tar` (the game folder as C4b staged it: git's `quakevr` files less `xr_layers`, `doom`, `sound_tests`, the
  `.map`s, plus `progs.dat`: 4045 files, 395 MB; no id data) and `README.txt`: adb on Linux, `adb install -r`, the
  paks through `/data/local/tmp`, the tar stream (`adb exec-in "tar -xf - -C $D" < quakevr.tar`) and the chmod,
  launching (Unknown Sources, `am start` with `--es args`), logcat, and `vr_android_export` + `run-as` for crash files.
- PC: dev build 0 warnings (QC 0); gles_render_test 21/0; scenario_check PASS (136 run, 0 failed); load_models_test 4/0.

## 6zg. The slow frames after the first launch; the second package (2026-11-17; his headset, no adb here)

- **His report** (the 6zf package, Quest 3, 72 Hz): the first launch after the install smooth (90/90); later launches
  `FPS=19/72`, App (GPU) 11.8 ms, CPU&GPU 38 ms, GPU% 0.31, CPU% 0.23, the main thread at ~37% of a core, the other
  game threads idle. **The cause is a value in his saved config**: the same launch with `-noconfigwrite +exec
  default.cfg +vr_quest_preset 0` (his config still executed first by quake.rc) ran smooth. So the value is among
  default.cfg's (binds, gamma, contrast, vid_fsaa, volume, viewsize, vr_defaults.cfg's) or the Low preset's and Quest
  rows. Which one: open (his config, or the two-launch bisect in the package README).
- **Not a GL sync**: `vr_gl_sync_count N` / `vr_gl_sync_report` (new; glquake.h's wrappers: glFinish, fence waits,
  query results not yet available, pixel and texture reads, synchronised maps, link status reads) on the PC's
  `-glescompat -fakeandroid` path: 1.00 a frame (the frame ring's fence) with the Quest preset, and 1.00 with Ultra,
  vid_fsaa 4, TAA, OIT, bloom, vr_tonemap 1, dynres off at scale 1. The engine's diff of the 1.1.0 merge was relight and
  installer code only. His numbers (GPU and CPU both mostly idle) point to a wait off the GL: a sleep, I/O or a join.
  His smooth launch also had no sound (being asked): `-nosound` is the next launch to try.
- **`vr_quest_check`** (each Android start after the saved config; Debug > Startup): the Quest rows that differ from
  the nearest Quest preset in the log, and MSAA (`vid_fsaa`: ~100 ms a frame, C4e) and bloom (squares, C4f) set back
  to 0, each said; `vr_quest_guard 0` only says. PC runs untouched (Android or `-fakeandroid` only).
- **The program cache's reason** in the start-up trace (`VR_StepNote`): "no pack for this driver yet", a damaged or
  older pack, "N from their binaries, M compiled (... their shaders changed since: a new build)". The 6zf install's
  first launch compiled everything (every program's text changed with the merge): the minute of dots.
- origin/next merged twice (the startup trace included); the Android build passes `-DQVR_QC=OFF` (the CMake build's
  new QuakeC step needs make and patch for FTEQCC). Package `<kit>/scratch/quest_apk2/` (README: what changed, the
  slow-frames steps). PC: dev build 0 warnings; gles_render_test 21/0; scenario_check PASS (136, 0 failed);
  load_models_test 4/0; APK 0 warnings, check_apk 18/0.

## 6zh. The safe-mode loop, the shader sets, the guard's two rows; the third package (2026-11-17; his adb findings)

- **His adb session** (laptop, Quest 3, the 6zf package): the 19/72 launches were **automatic GL safe mode** (persistent
  mapped buffers off: 34% of the main thread in Adreno's synchronous `GL_Upload` per draw); the minutes of dots were
  **full shader recompiles** (45-75 s each) whenever the shader set changed (safe mode on/off, `vr_shadow_dlights`, MSAA;
  also mid-exec: `Cmd_Exec_f -> Con_Printf -> SCR_UpdateScreen -> GL_CreateShaders`); a start stopped during the compile
  put the next in safe mode: the loop. Without safe mode his saved config ran 44/72; `vr_shadow_dlights 0` 51,
  `vr_dynres_min 0.55` too 63.
- **Safe mode judged** (`vr_glsafe.cpp judgeLastStart`): gl_startup.log carries each shader set's begin and end, `== alive
  at` every 5 s and the crash handler's `== crashed`. A start that missed its map counts only when it crashed or died
  within 20 s of its shaders without a report; stopped in a compile, after >= 10 s of compiles, or after living on: not
  counted, said ("GL safe mode off (...: not counted)"). Under OpenGL ES safe mode **keeps persistent mapped buffers**.
  `vr_glsafe_judge_test` 10/0; real stopped starts (`-gltestdie shaders|aftershaders`, PC `-glescompat -fakeandroid
  -glsafeauto`): stopped in the compile -> next start off (not counted); dead right after -> next on (automatic), its
  buffers "kept (safe mode under OpenGL ES)"; the start after that off (a dev build).
- **One shader set per start**: `GL_WorldLiteCheck` waits while commands run (`cbuf_executing`) and through the first
  frame; the start's first set takes the lite form the last session made last (`crash/gl_shader_set.txt`, ES only). PC
  ES path: a stable config 1 set (145 programs, 0.5 s from the cache); two lite changes mid-exec with prints between: 2
  sets (one after the exec, not one a print). Each set in the console and the trace: "GL shaders: set N this run, ...
  (H from the program cache, M compiled)".
- **The program cache keeps several sets**: pack version 2 (a session number, each program's last session); a program
  goes when unused 24 sessions or past 128 MB, oldest first; version 1 packs read. Four lite forms in turn, then the
  first again: 145 of 145 from the cache.
- **`vr_quest_check`** also caps `vr_shadow_dlights` at 0 and `vr_dynres_min` at 0.55 (said). **Refresh rate**: the
  Quest's is `vr_xr_refresh_rate`'s (0: the runtime's 72); `vid_refreshrate` is the window's, ignored, now said in the log.
- **`vr_android_export`**: paths in the app's own files too (`perf`; `profile/perf` as the app's perf); "not found"
  instead of "failed, 0 bytes"; vr_android_test 16/0.
- Open: the audio mixing on the render thread (~15% of the main thread: S_Update -> VR_SndPaint -> Mixer::render) to a
  thread of its own; compiling in the background after the first frame (not cheap: the first frame needs the set).
- Package `<kit>/scratch/quest_apk3/` (README: what changed, clearing a stuck safe mode). PC: dev build 0 warnings;
  gles_render_test 21/0; scenario_check PASS (136, 0 failed); load_models_test 4/0; APK 0 warnings, check_apk 18/0.

## 6zi. The shaders queued: the game boots with a progress panel; the fourth package (2026-11-18; no headset here)

- **Why so long** (his question): every start-up set is ~145 programs (ES path: 145; C4d's Quest count 103 before the
  lite/plain/gore/skin forms), and their sources are big über-shaders: **15.8 MB of GLSL** per cold set (~109 KB a
  program, the world's and the models' with every feature's code under `#if`). Adreno's compiler (`libllvm-qgl`) takes
  ~0.35-0.5 s each: 51 s (C4d, 103 programs), 45-75 s (his adb session). Standalone games ship Vulkan pipeline caches
  or small specialised shaders, and the driver's blob cache warms them (`EGL_ANDROID_blob_cache`: not wired, C4d); ours
  is the program cache (warm: 103 programs in 126 ms on the Quest). So: the first start after an update compiles, and
  now shows it.
- **Queued set** (`gl_shaders.c`: `GL_CreateShadersAsync`, `GL_ShadersPump`): the programs' GL names made at once (the
  renderer keeps them as before), each one queued; once a frame the pump checks those the driver finished
  (`GL_COMPLETION_STATUS_KHR`: never waits; statuses and errors as before, the binary to the program cache) and hands
  it more for 8 ms (one at least, at most 64 with the driver). With `GL_KHR/ARB_parallel_shader_compile` the driver's
  threads compile them; without it (`-noparallelshaders` tests that) one at a time, each compile on the frame that
  submits it. Quake VR's own programs (`VR_PrecompileProgramsBegin/Poll`) go to the driver at once and are polled too.
  The program cache's pack is written every 10 s meanwhile (`VR_ProgCacheFlush`: a start stopped mid-set keeps them).
- **The loading panel** (`vr/vr_shaderload.cpp`): while the set is pending `_Host_Frame` does the input, `VR_BeginFrame`
  (the OpenXR session starts and runs: no Meta dots) and the panel only: "QUAKE VR / Compiling shaders: N / M", a bar,
  the last program's name, the seconds; drawn on the CPU in the console font (no shader needed) into a 768x192 image,
  uploaded into the runtime's panel (the OpenXR quad layer) and blitted into the window (PC). No time passes and no
  random number is drawn meanwhile (the commands, server and client wait): what follows is the same however long the
  compile took. `SCR_UpdateScreen` draws nothing while pending (a print's redraw shows the panel). The trace: step
  "shaders queued: the loading panel's frames"; "GL shaders queued: N programs (H from the program cache, C compiled: K
  KB of sources) in T ms over F frames, the driver's threads|one at a time; the longest frame's work W ms (a submit, a
  check, the end), longest between frames; slowest: <4 programs>". `-shaderloadshot <file.tga>`: the panel's image.
- **On** under the OpenGL ES profile (the Quest; `-glescompat`), also for the settings' remakes (`GL_WorldLiteCheck`:
  the panel instead of a frozen frame); `-shadersasync 0|1` or `QVR_SHADERS_ASYNC` choose; the PC's own GLSL keeps the
  one-call set (its default unchanged). `vr_shader_reload` stays one call (same-frame A/B tests).
- **Priorities: not split.** The game waits for the whole set (the renderer's ~140 programs have no per-draw "not ready"
  path; a skipped draw per program is a large change): the menu and the first map come after it. On the Quest a warm
  cache makes the wait ~0.2 s; a cold one shows the panel for the compile's length. Next, if wanted: draw the menu as
  soon as `gui`/postprocess are done, the rest behind.
- **PC tests** (`-glescompat -fakeandroid`, `-shadersasynctest`: "shaders_async_test: N passed, M failed": >100
  programs, frames flowed, the progress moved, no frame's work over 50 ms): no program cache (the PC's default) 145
  programs in 648 ms over 37 frames, longest frame work 12.7 ms; `+vr_program_cache 2` cold 6.0-8.7 s (AMD compiles
  retrievable programs more slowly) over 1331-2027 frames, longest 25-39 ms (one earlier cold run 1.56 s: not seen
  again), warm 145 from the cache in 602-672 ms, 14 ms; `-noparallelshaders` 651 ms over 49 frames, 14.8 ms. 4/0 each.
  First loading frame at +0.55 s from the process start (was: the first frame after the whole compile).
- Debug > Startup > Recompile Shaders (Loading Panel) (`vr_shader_reload_queued`): the panel on demand (in e1m1 on the
  PC: set 2, 145 programs in 467 ms over 35 frames, the scene the same after).
- Package `<kit>/scratch/quest_apk4/` (bf2b1e90d; README: what changed, the log line to send). APK 0 warnings,
  check_apk 18/0. PC dev build 0 warnings; scenario_check PASS (136, 0 failed); load_models_test 4/0;
  gles_render_test 21/0 (its PC run queued too, `QVR_SHADERS_ASYNC=1`: the 2D pics fill the scrap in
  the same order as the ES run's).
- To check on the Quest (his launches): the log's "GL shaders queued" line (with "the driver's threads" and the longest
  frame's work: if Adreno compiles on the submitting thread, the submit times show it: then a shared-context compile
  thread is the next step), the panel in the headset from the first second, no dots.

## 6zj. The merge of next 2026-11-20: multiplayer and voice chat (no headset: `adb devices` empty)

- **origin/next merged** (610 commits: agent/mp's netcode, voice chat with Opus and RNNoise vendored, dynamic
  resolution's allocated/drawn eye sizes, the 1.1.x fixes). 10 files conflicted; both sides kept: `vr_stereo.cpp` (next's
  `useView`/`deleteSceneTargets`/`viewSize()` and the targets allocated at the dynres ceiling, with the Quest's foveated
  eye targets, the split right-eye targets (also freed by `releaseTargets` now), `vr_eye_subimage`, `vr_xr_direct` and
  the late acquire, all on the drawn size), `vr_gfx_gl.cpp` (next's `sharpLod()` on the Quest's `queryLod()`: ES has no
  `textureQueryLod`), `gl_shaders.c` (the queued set's `GL_DeferredCheckOne` with next's storage-block limits in the
  link error), `vr_upscale.cpp`, `vr_temporal.cpp`, `vr_glutil.*`, the debug menu, the OpenXR backend, the crash handler.
- **Android**: Opus and RNNoise were already in `vr.cmake` (plain C static libraries, every platform): they and the
  netcode compile for arm64 as they are, nothing left out. Two NDK-only warnings fixed: `vr_config_diag.cpp`'s
  range-for over a temporary's element (a real dangling read without C++23's lifetime extension, clang 18; the PC's
  clang 22 has it) and the Opus self-test's unused decode results. APK 0 warnings, check_apk 18/0 (21.7 MB).
- **Multiplayer and voice on the Quest: compiled in, untested.** The manifest has neither `android.permission.INTERNET`
  (sockets: needed for any network game) nor `RECORD_AUDIO` (the microphone; SDL asks at run time): left out, his call
  (one line each in `Android/app/src/main/AndroidManifest.xml`). The rendezvous/room service and LAN play would follow.
- **PC checks**: dev and ThinLTO builds 0 warnings (LTO relinked); scenario_check PASS (140, 0 failed);
  load_models_test 4/0; vr_simd_test 58880/0; flatmelee 12/0; mp_test.py suite sample (`--only` coop, coop_lag,
  avatar, campaign, predict, lagcomp, voice, dm; mp_netsafe first; loopback only) 9 scenarios 170/0;
  gles_render_test 21/0 (a first run beside the Android build's compile 18/3: the scrap's 2D pics in another order
  and a floor patch with arrays on/off; again alone 21/0: load-dependent, watch it).
- Then origin/agent/quest's own two merges of next (2026-11-19; the same resolutions, plus vr_xr_direct's target freed
  in `releaseTargets`, direct only when drawn == allocated, the screen shader's `sharpLod()` used) and next's 9 newer
  commits (ragdolls, Box3D, menus: no conflicts) merged; again dev and ThinLTO 0 warnings, scenario_check 140/0,
  load_models_test 4/0, gles_render_test 21/0, APK 0 warnings and check_apk 18/0 (21.7 MB).

## 6zk. C4z: the merge's first headset launch (2026-11-20; adb, unworn; the build c5cfb048e installed by the coordinator)

Method (`scratch/c4z_*`): his config copied out first (`vr_android_export ironwail.cfg` from a run: the app's own file is
660 now, shell and run-as can't read it; `scratch/c4z_backup/export/`, md5 c9bec08f; every run `-noconfigwrite`, so it
is unchanged); this build's changed game data (`progs.dat` and 7 files since 379b2e6) pushed as one tar
(`c4z_push.sh`); `c4z_run.sh` (c4e_run plus an early end when the app is paused), `c4z_cap.sh` (a screencap of the
compositor's picture at each `C4Z CAP` marker), `c4z_play.sh` (C4n's play session on his saved config), `c4z_maps.sh`,
`c4z_p50.py`, `c4z_scale.py` (per map: FPS, p50s robust to the GPU timer's bogus outliers, dynres scales).
- **The headset outside its boundary**: the first launch paused after 3.5 s (a Guardian dialog: "no 6dof tracking",
  the headset 1.84 m from his room-scale boundary, no active safety bound) and the next launch never came up (window
  placement timed out) until the runtime made an automatic stationary boundary (20:32). After that every launch ran.
  `prox_close` was not needed (and isn't used now: `PROX=1` turns it on).
- **Boot**: works. First start after the update: 40 s (two shader sets: 51 + 27 compiled; the second because the stopped
  first launch had recorded the pre-config lite form; "not counted" by the safe-mode judge: no safe mode). Later starts:
  **one set, 70 from the program cache, ~1 s; first frame at 5.3-6.7 s**. Logcat: 0 E/W/F lines from the app in the
  clean runs. The loading panel shows in the headset (seen in a screencap, "Compiling shaders: 39 / 70").
- **The queued set on Adreno is "one at a time"**: the driver has no `GL_KHR_parallel_shader_compile`; 51 programs in
  18.7 s over only 43 frames, the longest frame's work 1078 ms (one submit). The panel updates ~2 times a second while
  compiling. The next step if that matters: a shared-context compile thread (6zi's open item).
- **Eye images** (screencaps of the compositor's picture, both eyes, `scratch/c4z_cap/c4z_caps3.png`; eyeshots
  `vr_eyeshot 2` in `scratch/c4z_backup/export/e1m1_00[0-4]_*.png`): e1m1's start at dynres 0.90 (eyeshot 1512x1584),
  **dynres at 0.65 inside targets allocated at 0.90 (eyeshot 1092x1144, the eye sub-image)**, foveation 3, vr_xr_direct
  1 (dynres off), and back: **both eyes correct in all five, the same framing, no stretch, no black borders** (each
  picture's mean and near-black share equal within 0.2 / 1%). Foveation 3: "the eyes' scene foveated (1512 x 1584)";
  its periphery not visibly coarser with the retro textures (as C4h/C4n found).
- **Foveation under dynres fixed (code)**: QCOM's focal point and gain were in the whole texture's -1..1 while the eye
  is drawn into its bottom-left part (next's allocated/drawn sizes): at 0.65 of 0.90 the full-resolution area sat off
  the eye's centre. `foveateEye` now maps both into the drawn part. Not yet seen on the headset (needs the new APK).
- **vr_quest_check ran after the command line's +commands (code)**: appended after quake.rc, so a launch with
  `+map`/`+exec` ran it at the end of the whole +exec'd file (seen: a 45 s scripted session measured with
  `vr_shadow_dlights 1`). Now `Cmd_StuffCmds_f` runs it (and the first run's `vr_quest_preset 0`) just before the
  +commands (`cmd_before_stuffcmds`); PC `-glescompat -fakeandroid`: the check's lines between the autoexec's last line
  and the first +command, `vr_shadow_dlights` 0 there. His normal launches (no +commands) were not affected.
- **A crash, self-inflicted**: `vr_dynres_test_load 300` (300 units a pixel) stalled the GPU 39 s and the driver
  crashed in `xrEndFrame` (SIGSEGV in libGLESv2_adreno under vrapi). Not a game path; a report is in his crash folder.
- **Performance** (the scripted session, `c4o_mkplay.py` with demo1; 72 Hz; p50s; vs C4x's session of 11-16, same
  script and `+vr_quest_preset 0`):

  | | FPS | period p50/p95 | CPU p50 | GPU p50 | eye L / R | world | scale p50 |
  |---|---|---|---|---|---|---|---|
  | e1m1 (start, fight) C4x | 43.0* | 15.05 / 26.27 | 9.40 | 11.27 | 5.59 / 4.84 | 8.05 | 0.55 |
  | e1m1 now, preset | 59.7 | 14.78 / 22.84 | 8.94 | 12.99 | 6.58 / 6.28 | 8.39 | 0.55 |
  | e1m1 now, his config + guard | 62.8 | 14.23 / 20.31 | 8.87 | 11.28 | 5.77 / 5.68 | 8.10 | 0.55 |
  | e1m2 (fight) C4x | 54.8* | 14.00 / 17.45 | 8.23 | 9.65 | 5.00 / 4.47 | 6.67 | 0.90 |
  | e1m2 now, preset | 69.2 | 14.04 / 17.63 | 7.56 | 11.01 | 4.39 / 6.54 | 4.86 | 0.70 |
  | e1m2 now, his config + guard | 70.1 | 14.06 / 17.20 | 7.59 | 10.81 | 4.64 / 5.92 | 5.58 | 0.65 |
  | demo1 (e1m3) C4x | 54.2* | 13.99 / 16.46 | 2.63 | 6.04 | 3.05 / 2.91 | 4.43 | 0.90 |
  | demo1 now, preset | 71.2 | 14.02 / 16.32 | 3.00 | 6.11 | 3.18 / 2.97 | 4.46 | 0.85 |

  (*C4x's FPS includes four app pauses of 3-6 s; its periods are the fair comparison.) e1m1's idle start on his config
  without the guard (20 s): **72.0/72, 0 missed periods**, CPU 5.9, GPU 9.4 ms at 0.90. 6zh's 63/72 (his worn session,
  the guard's rows) is in line with the e1m1 session's 62.8 here.
- **What the merge moved**: demo1 (deterministic) the same within 0.1-0.2 ms a phase (dynres a step lower: 0.85 vs 0.90).
  The live fights draw more (e1m1 182 draws vs 165, e1m2 153 vs 140: next's content; the monsters are random) and the
  GPU's p50 is ~1.4-1.7 ms higher at the same or a lower scale; **the right eye now costs ~2 ms more than the left in
  e1m2** (6.54 vs 4.39; before 4.47 vs 5.00) with split targets on in both: not explained yet. The suspects: the right
  eye's split targets allocated at the dynres ceiling while drawn smaller (a tiler's loads/stores or clears over the whole
  allocation), the eye sub-image's rect changing per frame with dynres. The CPU side is unchanged or lower.
- Not done: multiplayer over loopback on the headset (the budget; the mp code is compiled in, 6zj); the frozen fight
  (`c4y_frozen.sh`).
- New APK built from this branch (the two fixes; `build-android/apk/QuakeVR.apk`): native 0 warnings, check_apk 20/0;
  not installed (his call / the coordinator's). PC: dev build 0 warnings.

## 6zl. questopt: next merged, body tracking standalone, the world's GPU cost and the resolution (2026-11-21; adb, unworn)

His worn session of 11-21 (`kit scratch/quest_perf_1121`): 61.3 FPS over 515 s, GPU over budget in 32% of frames,
"world_brush" the top hitch cause; FBT's elbows untracked standalone (tracked on the PC through Virtual Desktop).
Method (`scratch/qo_*`): `qo_world.sh` (e1m1's places, the scale fixed, monsters frozen, the body off, switches one at a
time, with `ovrgpuprofiler -r` metrics alongside: `qo_gp.sh`, `qo_gpsplit.py`), `qo_trace.sh` (render-stage trace of
a still view), `qo_fight.sh` (C4v's frozen fight on HIS config, the variants interleaved 3 times), `qo_rec.py` /
`qo_rec2.py` (his recording by map, and by lit/unlit world and dynamic lights). Every run `-noconfigwrite`; his config
exported first (`scratch/qo_backup/export/ironwail.cfg`, md5 8a6c9282); the APK installed with `install -r`.
- **next merged** (170 commits: no conflicts). PC dev 0 warnings; scenario_check PASS, load_models_test 4/0,
  gles_render_test 21/0. APK native 0 warnings, check_apk 20/0.
- **Body tracking's cause**: the manifest declared no body tracking permission or feature, so the headset's runtime
  made no tracker (`dumpsys package`: none requested). This OS knows `com.oculus.permission.BODY_TRACKING` (granted at
  install) and `com.meta.permission.FOCUSED_BODY_TRACKING` (run time, "dangerous"). **Fix**: both declared plus the
  `com.oculus.software.body_tracking` feature (optional; vr-only lines, check_apk counts 7), the grant asked for on its
  own thread before the tracker is made (the older name first: no dialog where it is granted at install; else the
  dialog). Headset: `tracker made (full body, 84 joints, high fidelity asked for)`, extensions fb/fullbody/fidelity/
  calibration 1, system 1. Unworn: 0 joints valid, calibration reset -2 (the runtime gives a body only when worn).
  **Worn check waiting on him** (also Settings > Movement tracking > Body tracking on).
- **What "world_brush" is on the headset**: a tiler's timer queries inside a binned pass measure the pass, so the
  recorder's world_brush holds what the eye's main pass draws (the world, the body drawn first inside its scope, the
  models, the particles), not the world alone. The render-stage trace (hall, 0.55): each eye 7 surfaces at 924x968
  (3 bins of 960x352), the main pass 1.39-1.46 ms, the eye ~2.3 ms, the frame ~5 ms; no surface drawn twice.
- **The culling audit** (the code and the switches; GPU ms a frame, 0.55, monsters frozen, body off):

  | finding | measured |
  |---|---|
  | PVS per eye (Mod_LeafPVS of each eye's leaf; the fat PVS only near water) | `r_novis 1`: world +0.6 to +2.0 ms (pool 2.93 -> 4.94), 15-20% more triangles: vis works |
  | frustum (each eye's own 4 planes) and backface culling, per eye on the GPU (cull_mark compute) | in place; no eye draws the other's |
  | visibility mask (the lenses' hidden area) | `vr_visibility_mask 0`: +0.3 to +1.0 ms at 3 of 4 places: it helps |
  | the window mirror (`vr_mirror 1` in his config) | its passes are empty (0.01 ms, the 4128x2208 surface): no cost (C4h) |
  | PBR (`vr_pbr 1`) on the world | within noise (bridge 2.35/2.31, hall 3.39/3.04, pool 2.93/3.31; fight 3.91/3.69): with vr_normalmaps 0 its companions are not sampled. No cut made |
  | dynamic lights in a frozen 9-monster fight (his config: body, rig lights, torches 2) | span 5.6 ms; body 0, rig lights 0, torches 0, r_dynamic 0 each within 0.3 ms; all models off 3.6 ms |
  | textures | mip levels used (1.5-2% non-base), GL_NEAREST_MIPMAP_LINEAR, anisotropy 1; MSAA off (a must: C4e) |
  | engine foveation `vr_foveated 3` | does nothing on Adreno ("GL_NV_shading_rate_image not available"); runtime foveation `vr_xr_foveation 0` |

- **The resolution**: the runtime's recommended eye is 1680x1760 (the panel 2064x2208); his `vr_render_scale 0.9` is
  1512x1584, and dynamic resolution's floor 0.55 is 924x968. **In his session e1m1 sat at the floor in 86% of its
  frames** (vrarena 43%, vrstart 78%): that is the low resolution he sees. At the floor e1m1's GPU was still 14 ms
  (p50) and 28 ms (p95): the scale cannot go up. The frames split by the world's light (`qo_rec2.py`): with 5-6
  dynamic lights reaching the walls (the lit world: 72% of e1m1's frames) the main pass 8.5 ms and the span 13.9-16.8
  ms at 0.55; with none reaching (unlit) 3.9-5.1 ms and the span 7.3-9.2 ms at 0.60-0.65. The static places and the frozen fight cost
  4.4-5.9 ms at 0.55 (72 Hz held, 13.9-14.2 ms periods): his live play costs 2.5-3x more than anything frozen.
  **Native resolution at 72 Hz**: the hall's still view at 0.55 is ~2.3 ms an eye; scaled by pixels (x3.3 at 1.0, x2.7
  at 0.9) ~15 ms at 1.0 and ~12.4 ms at 0.9 for both eyes: even a still view needs the per-pixel cost about halved for
  1.0, and live play ~5x.
- **Not cut here** (no measured win to take): PBR off on the world, the lights. The live play's extra cost was not
  reproduced by any frozen scene (unworn: the scripted pose and frozen monsters miss it).
- **Armed humanoids' CPU**: not measured here (the recorder has no per-subsystem scopes for pose solves, arm IK, disarm
  hulls, strike sweeps or ragdolls; his session: SV_Physics 2.0 ms mean, p99 7.2; view_entities 0.9, p99 5.0).
- The Android Gradle Plugin download: approved by him 2026-11-21 (build-time, under F:/QuakeVRDev or ~/.gradle); not
  needed this round (`package_apk.sh` packages without it).

- **The research's cheap steps (kit `scratch/quest_research_1121.md`), measured** (`qo_fight2.sh`: the frozen fight on his
  config at 0.9 fixed, interleaved 3 rounds; GPU span ms): base 11.40 (period 14.13: 72 Hz held at 0.9 frozen);
  **`vr_xr_gpu_level 4` + `vr_xr_cpu_level 4`: 11.21** (-0.2, noise: the runtime already clocks up under load; 545-690
  MHz seen by ovrgpuprofiler); **`vr_pbr 0`: 11.40** (no change; at 0.55 3.91 -> 3.69 world, 5.63 -> 5.36 span). Neither
  is put in the preset. D24S8 and the eye's compute splits are code changes, not done this round (the Next phase).

## 6zm. C5b: his worn session's switches; the dynamic lights in the lightmap (2026-11-21; adb, the headset in the dark)

Method (`scratch/c5b_*`): `c5b_stretch.py` (his recording by stretch), `c5b_export.sh` (the app started only to run
`vr_android_export` from its command line: works with the headset idle, no XR frames needed), `c5b_pc.sh` and
`c5b_pair.py` (the PC's -glescompat with the Quest preset, a picture A/B).
- **His worn session of 17:24** (`kit scratch/quest_perf_1121b`, 65.9 FPS, GPU over budget 17%): his run's console log
  (`logs/2026-11-21_17-24-06/console.log`, exported) dates the switches: he typed them in the console after restarting
  e1m1 each time, and they stayed off (cumulative; his saved config still has all three at 0). The stretches (e1m1, GPU
  ms, dynres at its 0.55 floor throughout):

  | stretch (s) | FPS | GPU over budget | span p50 | span p95 | world_brush p50 | lights reaching walls (frames) | draws |
  |---|---|---|---|---|---|---|---|
  | normal play (40-160) | 67.6 | 15.3% | 10.75 | 18.20 | 5.88 | 64% | 197 |
  | rig lights off (163-246) | 66.2 | 23.1% | 11.17 | 18.12 | 5.75 | 47% | 199 |
  | + torches off (258-386) | 69.0 | 11.7% | 10.52 | 16.65 | 6.23 | 21% | 188 |
  | + body off (392-500) | 70.4 | 5.9% | 10.11 | 14.29 | 6.12 | 28% | 186 |

  The same view after each restart (the spawn, scale 1.0, 72 Hz held): normal span 10.34 / world_brush 7.24 (292
  draws, the walls lit); rig + torches off 10.53 / 7.24 (278 draws, no light on the walls): **the lights on the walls
  cost nothing there**; + body off 10.12 / **6.23** (-1.0 ms of the main pass: the body is drawn inside it). Over the
  whole session the frames with lights on the walls cost ~1.5-2 ms more main pass than those without at the same scale
  (6.4-7.2 vs 4.9-5.9 ms at 0.55-0.65). The play differed between stretches (different rooms and fights), so their rows
  rank loosely: **the body (~1 ms a frame, the p95 -2.6 ms) and the lights on the walls (~1.5-2 ms where they reach)
  are the two costs; the rig's lights alone show nothing.**
- **Body tracking while worn**: his run's OpenXR log: the permission granted, `tracker made (full body, 84 joints, high
  fidelity asked for)`. Nothing prints during play (no `vr_bodytrack_status` run), so **the log can't tell whether the
  joints were valid or the elbows used**; he turned the body off (`vr_body_mode 0`) for the last stretch.
- **Step 1 built: `vr_gl_world_dlmap`** (default 0, Debug > Profiling and Memory, with `vr_gl_world_dlmap_info`;
  `vr/vr_dlightmap.cpp`): once a frame the dynamic lights (the frame's, not one eye's; the rig's skipped under
  `vr_rig_lights_world 0`) are added into the lightmap's texels on the CPU (the shaders' falloffs: DarkPlaces' or
  Quake's, `vr_dlight_angle`, a spot's cone, at each texel's place solved from the texture axes and the plane; the
  styles' packing kept), the blocks uploaded, last frame's put back; the world (and the unturned brush entities) then
  drawn by `glprogs.world_unlit` (no lights' loop) in every view. A map light or a portal light in view falls back to
  the loop. The PC's shaders unaffected (no unlit form). PC -glescompat with the Quest preset: e1m1's spawn 5 lights,
  17 blocks, 0.15-0.3 ms CPU; the hall 141 blocks, 1.35 ms (the quick build); the picture: the per-pixel rings
  replaced by the lightmap's smooth blob, the same brightness (mean 23.5 vs 23.3).
- **Not measured on the headset**: it sat in the dark ("Images too dark", "no 6dof tracking": the runtime idles the
  app; a map load never finished). Steps 2-4 and the resolution not done. Step 2 needs no code: D24S8 is
  `vr_depth_float 0` (clip control is on the Quest, so it runs D32F+S8 now; with D24 the near plane goes from 0.02 to
  1 unit; held items are never cut).

## 6zn. C5c: the body's cost, the lights in the lightmap batched, the ledger (2026-11-21; adb, unworn, room lit)

- **The body**: the bare forearms were 144-sided from the shoulder (`make_vrbody.py`), 90% of the body's triangles;
  now 24 sides and 144 only at the wrist and lip (a stitched band): 10,874 -> 4,154 triangles. PC mock (looking down,
  `vr_model_fragments`): 0.099 -> 0.072 ms a pass, vertex runs -52%, fragment runs -33%. Quest frozen fight
  (`scratch/c5c_run.sh`; the unworn view, the body mostly out of it): span 3.71 -> 3.55 ms (old mesh pushed as the
  brawny build, `c5c_push.sh MODE=ab`). How it is drawn: GPU skinning (bones in an SSBO, one upload a batch), a depth
  pre-pass then the shading (`vr_alias_prepass_body` 0: PC 0.091 ms, Quest +0.33 ms: the pre-pass pays), each eye
  separately (single-pass stereo leaves models near the eyes per eye), no shadow pass under the Quest preset, the
  mirror only with a mirror; 5 models (body, plates, pauldrons). His ~1 ms (6zm) was a worn view with the arms in it:
  the cut removes 60% of its vertices and a third of its fragments; re-measure worn.
- **The lights**: the per-pixel loop's cost is its presence (registers, 6w-6z) on every lit wall pixel, the light
  lists already clustered (32x16x32), the loop plain, the world split lit/unlit by surface. The Quake way (the lights in
  the lightmap, `vr_gl_world_dlmap`, C5b) removes the loop: 3 big test lights' GPU span **9.79 -> 3.96 ms** (no lights
  3.78) once its uploads are batched (`vr_gl_world_dlmap_batch`, new, 1: one upload per 16-row band; unbatched it was
  5.47 ms GPU and 6.61 ms CPU, frames missed), CPU **1.81 ms** a frame. Off by default: the 16-unit lightmap look is his
  call (QUEST_PLAN 6zm); recommended for the Quest preset. Not done: the additive light pass (the world unlit, the lit
  surfaces drawn again by a tiny shader and blended on the tile; the decals and the clamp make it a look change).
- **`vr_depth_float 0`**: not measured (a launch each: noise 2 ms between launches); left for C5d.
- **Unworn tests**: `prox_close` on a still headset loops the Guardian's dialog and the app never starts; without it
  the lit room's unworn headset ran at 72 Hz. The load no longer waits on throttled frames (`loadThrottled`).
- **The ledger**: `QUEST_OPTIMISATIONS.md`.

## 6zo. C5d: the body's worn cost found (2026-11-21; adb, unworn, the head posed)

Method (`scratch/c5d_*`): **`vr_debug_head_pose`** (new, Debug > Profiling and Memory, OpenXR only, with
`vr_debug_head_pitch` and `vr_debug_hands_front`): the rendered head set level at a pitch, 1.65 m high, the hands in
front at the waist, the layers still carrying the runtime's poses: an unworn headset draws a worn view.
`c5d_ab.sh` (e1m1's spawn, the body and lights as shipped over his headset config, interleaved rounds), `c5d_shot.sh`
(eyeshots of the posed views), `c5d_pc.sh` (PC mock, `vr_model_fragments` and the profiler, body on/off).
- **Where it goes**: the body's cost is its **fragment shading**, in proportion to the pixels it covers; nothing else.
  Not a second world pass, not its lights' world cost, not its vertices (the pitch 0 view, the body out of sight: on
  and off the same, 9.7 / 10.1 ms span), not the pre-pass (off: -0.3 ms at pitch 70, +0.33 in C5c's fight). Quest,
  4 rounds at 1.0: pitch 35 (the forearms, hands and holsters in view) span 11.24 -> 13.11 ms with the body (+1.9);
  pitch 70 (the torso filling the lower half) **11.68 -> 23.61 ms (+11.9)**, each eye 6.8/4.9 -> 12.1/12.1. The world
  hardly drops under it (main pass 5.33 -> 5.16): the floor the body hides was cheap; a body pixel costs ~3-4 ns, the
  world's ~1-2. His 17:24 "world_brush" includes it: a tiler's timers inside the binned pass measure the whole pass.
  PC (mock, looking down): the body 0.07 ms a pass, the eyes on/off within 0.02 ms: nothing to cut there.
- **The shader's parts** (pitch 70, 0.7 fixed, 3 rounds; span with the body 11.8-12.0, without 6.2-6.7): ended after the
  texture (`QVR_PROBE_AMIN`) 6.88, after the look (skin tone, wounds, dents: `ALOOKEND`) 7.03, after the light
  (`ALIGHTEND`) 10.99. So the look costs ~0.15 ms, **the light ~4 ms**, the tail (sheen, rim, fog) ~0.85. Of the light:
  the per-pixel dynamic lights on models (`vr_dlight_models 0`, his config 1; the rig's two lights reach every body
  pixel) -1.85 ms; the own light's ambient and bumps (`QVR_PROBE_AAMB`) -0.9; `vr_gl_lights_lite 1` -0.3; the skin's
  code out (`NOSKIN`) -0.3; no change: wounds, dents, dynamic AO, `vr_pbr`, `vr_shadow_self`, `vr_parallax_models`,
  the plain forms, the models drawn after the world; worse: the vertex light probe (`vr_gl_alias_vlight 1`, 17.5).
- **Not done this round** (the APK's full rebuild took 26 min, the budget went on finding the cause): the cut itself
  (a body-only light form: the dynamic lights on the body summed per vertex or Quake's flat add, the own light's cube
  per vertex; target the body under 0.3 ms at pitch 35), `vr_depth_float 0`, the resolution step.
- **Unworn throttling**: with the GPU span over ~11 ms the unworn runtime paced frames at 40-55 ms (`c_xrWaitFrame`);
  measure at 0.7 or below to stay under it.
- **Worn A/B for him**: `qvr-kit/scratch/quest_worn_ab.py push`, then in the game `exec quest_worn_ab.cfg` (3 rounds
  of base / body off / dlmap / both, 15 s each, 3 min, marked and echoed), then `quest_worn_ab.py report`.

## 6zp. C5e: the body lit at its vertices, measured slower (2026-11-22; adb, unworn, the head posed)

- **Built: the body form** (`glprogs.alias_body`, `vr_gl_alias_body`, a tunable, ES only; `r_alias.c`, `vr_render.cpp`
  `VR_AliasBodyLight`): your body's batches that the plain, gore or skin form would draw use the skin form with
  `QVR_APLAIN_VLIGHT`: the own light's ambient cube and the dynamic lights worked out per vertex (C4y's code), the
  texture and the look kept, no sheen from the lights; only with no dynamic occlusion and no shadowed lights. The look
  holds: PC `-glescompat` off/on 0.5% of pixels over 8 levels (max 26); Quest eyeshots at pitch 35 and 70 the same but
  for the gun's screen animating (`scratch/c5e_shot.sh`, `c5d_eyes/a_p35.png`, `a_p70.png`).
- **Measured slower** (`scratch/c5e_ab.sh` via `c5d_ab.sh`, e1m1's spawn posed, 0.7 fixed, the Quest's switches: no AO,
  no shadows, no normal maps; 3 rounds, span ms):

  | view | body off | body, per pixel | body form |
  |---|---|---|---|
  | pitch 35 | 7.35 | 8.09 | 11.24 |
  | pitch 70 | 6.68 | 13.31 | 16.80 |

  The same as 6zo's `vr_gl_alias_vlight` probe (17.5). The likely cause: the vertex shader's loop over every frame light
  (no clusters), run in each bin of each eye for every body vertex. **`vr_gl_alias_body` stays 0** (a probe).
- **Under the Quest's switches the body costs 0.74 ms at pitch 35** (8.09 vs 7.35 at 0.7) and 6.6 ms at pitch 70.
- **Next, not done**: the dynamic lights out of the body's fragment shader (`QVR_APLAIN_NODLIGHTS`) with Quake's flat add
  on the CPU for the body (`R_AliasLightFinish`; the per-pixel model lights were 1.85 of the ~5 ms at pitch 70), the
  ambient cube per vertex without the light loop (`AAMB` probe: -0.9); `vr_depth_float 0` A/B; the resolution step.

## 6zq. C5f: why the body costs, Quake's way built, the dlmap pairs (2026-11-22; PC only: the APK did not build in time)

- **Why the body is expensive (his question, from what is measured so far)**: not its triangles and not because it is
  close as such: it is the *number of pixels it covers times the work per pixel*. Close to the eyes it covers a big
  share of the view (the torso at pitch 70 the lower half of each eye), and each of its pixels runs the models' full
  per-pixel shader (texture, skin tone, wounds, dents, the ambient cube and bumps, a loop over every dynamic light,
  sheen, rim, fog): ~3-4 ns a pixel on the Quest 3 against ~1-2 for the lightmapped world (6zo). The world's pixels
  it hides were cheap, so the frame gains little back. What is already in place, so not the cause: the body is drawn
  **before the world** (`vr_alias_player_first 1`: the floor under it is rejected by depth, not shaded) with a **depth
  pre-pass** (`vr_alias_prepass_body 1`: no overdraw inside the body, arms over torso, shaded once a pixel); **fixed
  foveation does not reach it**: `vr_xr_foveation` is 0 and guarded (`-xrfoveation`), and the runtime's foveation
  scales only the swapchain images, while the scene is drawn into our own targets (QCOM foveation on them: no effect,
  6v), so the body at the view's bottom edge is drawn at full resolution; only `vr_xr_direct 1` (no post-process)
  lets it reach the scene. The part of the shader that costs is its light (~4 of ~5 ms at pitch 70: the per-pixel
  dynamic lights 1.85, the ambient and bumps 0.9; 6zo).
- **Built: `vr_gl_alias_body 2`** ("Body Lighting (Quest)" > "Quake's Way", Debug > Tunables; ES only, default still 0):
  the body form with only the ambient cube at its vertices (no light loop: `QVR_APLAIN_VFLAT`), the dynamic lights added
  to the body's light on the CPU once a frame as one colour (`r_alias.c` `R_AliasLightFinishBody`), each light as the
  per-pixel loop gives the body's origin facing it (`vr_lighting.cpp` `VR_DlightFlatLit`: DarkPlaces' or Quake's
  falloff, the spot cone; Quake's own add, before the light curve and unscaled, was 2-3 times too bright under the
  flashlight). A batch another form draws (translucent, a normal map, a glow) gets the light without them back
  (`aliasinstperpixel`), so the lights are never counted twice. Look, PC `-glescompat`, the Quest's switches,
  the flashlight on: looking down mean 4.4 levels, 16% of pixels over 8 (the forearms a little warmer); pitch 35
  2.1, 8%; flashlight off 1.7, 10% (max 51).
- **New probes** (`vr_gles_define`): `QVR_PROBE_AMEDIUMP` (main's floats at medium precision), `QVR_PROBE_ANOFOG`,
  `QVR_PROBE_ANOFB` (no fullbright fetch); `scratch/c5f_parts.sh` runs the full per-part table (off, full, const,
  texture, look, light, rim end, no dynamic lights, no ambient, no skin, no wounds/dents, no fog, mediump, no
  specular AA, no fullbright, Quake's way) and `SET=key` (off, full, Quake's way, vertex light, models after the
  world, no pre-pass) at `PITCH=35/70`, 0.7, 3 rounds.
- **Not done (blocked)**: the APK. The merged `next` made `vr_menu.cpp` a 34+ CPU-minute compile on the NDK's clang
  (16 min on 11-21) under the machine's load; the 40-minute job limit killed it twice-over its budget. So the
  headset still runs the C5e APK (`vr_gl_alias_body` default 1 there), and none of the Quest measurements (fragment
  counts from `QVR_PROBE_ALIASCONST` eyeshots, the per-part table, Quake's way timed, the FFR probe via `vr_xr_direct 1`
  + `-xrfoveation`, `vr_depth_float 0`) ran. First thing next: build the APK (alone on the machine, or `vr_menu.cpp`
  at a lower optimisation level), install, `c5f_parts.sh` at 70 and `SET=key` at 35 and 70.
- **dlmap pairs** (PC `-glescompat`, full-size 1024x1024 eyeshots, `vr_light_test` lights, left 0, right 1):
  `qvr-kit/scratch/dlmap_compare_{doorway,corridor_rocket,sweep1,sweep2,sweep3,torch_wall,ad_azad}.png`. In the
  lightmap a big light is much **brighter and flatter** (doorway mean 60 -> 105, AD 75 -> 103: no per-pixel angle,
  the side walls washed out); torches (small) nearly the same (1.9 levels). No `_lmscale` scene: the engine has no
  `LMSHIFT` support and the installed AD maps carry none (all 16-unit grids). Quest eyeshot pairs not taken (no APK).


## 6zr. C5g: dlmap as bright as the loop, in its own texture; the Quest preset takes it (2026-11-22; adb, unworn)

- `vr_menu.cpp` at -Os on Android (the NDK's post-RA scheduler: 1096 of 1199 s at -O3); APK after a full rebuild 5.6 min.
- `vr_gl_world_dlmap`'s light summed into its own RGB10_A2 texture (DlightTex, unit 15) the unlit world form adds as
  the loop's dynamic_light (after the light curve, the same cap): pairs now within 0-6 levels (c5f: +48 doorway, +28 AD,
  partly the lightmap's light counted on top of the loop's). Quest: frozen fight 3 big lights 11.33 -> 7.00 ms GPU,
  72 Hz held, +1.5 ms CPU: **the Quest preset sets vr_gl_world_dlmap 1**. Body Quake's way (vr_gl_alias_body 2) slower
  (+2.6 ms pitch 70): off. `vr_gl_quake_dlights` (one row, both) built, default 0. ROUND25 "Quest C5g".

## 6zs. C5h: the body's Quake way fixed (its own program was the cost) and on; D24 depth on (2026-11-22; adb, unworn)

- vr_gl_alias_body 2 was slower for two reasons: its vertex ambient (~2 ms on the vertex side) and its own program,
  which ran the same code ~3.3 ms slower than the skin form. Now the body keeps its usual form with the light loop
  skipped for its batch (`BatchDlightsFlat`): pitch 70 11.65 -> 10.63 ms, pitch 35 8.71 -> 7.98, the fight unchanged
  (7.02 vs 6.90). **The Quest preset sets vr_gl_quake_dlights 1 and vr_depth_float 0** (D24S8: pitch 0 -0.37 ms, pitch 35
  -1.19, in-session A/B). Resolution: no room for a higher base; the fight's floor could be 0.65 (7.99 ms) but
  questMusts caps it at 0.55: his call. ROUND25 "Quest C5h".

## 6zt. C5i: the dynres floor 0.65 (his call), installed for his test (2026-11-22; adb, unworn)

- Quest 3 72 Hz preset `vr_dynres_min` 0.55 -> **0.65** (90 Hz and Quest 2 unchanged, 0.5); `vr_quest_check`'s ceiling
  for it 0.55 -> 0.65 (else it set the new floor back). PC unchanged. The frozen fight (C5h's scene, the preset, 2
  rounds): dynres sits at its ceiling 0.90, 10.4 ms GPU, period 14.0 (72 Hz held); fixed 0.65 5.96 ms.
- His headset's saved config held the old preset rows (dynres_min 0.55, torch/rig world lights 0, dlmap/quake_dlights
  0, depth_float 1, prepass 1): kept as `ironwail_before_c5i.cfg` in the game folder, then `vr_quest_preset 0` saved.
- **Found**: `vr_gl_world_dlmap` and `vr_alias_prepass_world` are not archived, so after the first session a saved
  config starts them at their PC defaults (0 / 1); dlmap is covered by `vr_gl_quake_dlights 1`, the pre-pass is not
  (C4w: -0.49 ms lost). ROUND25 "Quest C5i".

## 7. Installs (done 2026-11-14 at his approval, by the coordinator)

Installed under `F:\QuakeVRDev` (`env.sh` / `env.ps1` there set `JAVA_HOME`, `ANDROID_HOME`, `ANDROID_SDK_ROOT`,
`ANDROID_NDK_HOME` and `PATH`; the downloads kept in `dl\`):

| Tool | Version | Path |
|---|---|---|
| Microsoft Build of OpenJDK | 17.0.21 (LTS) | `F:\QuakeVRDev\jdk-17` |
| Android SDK command-line tools | 12.0 | `F:\QuakeVRDev\android-sdk\cmdline-tools\latest` |
| Platform-Tools (adb) | 37.0.1 | `...\android-sdk\platform-tools` |
| SDK Platform | android-34 (rev 3) | `...\android-sdk\platforms\android-34` |
| Build-Tools | 34.0.0 | `...\android-sdk\build-tools\34.0.0` |
| CMake (+ Ninja) | 3.22.1 | `...\android-sdk\cmake\3.22.1` |
| NDK | 27.2.12479018 (clang 18.0.3) | `...\android-sdk\ndk\27.2.12479018` |
| Gradle | 8.10.2 | `F:\QuakeVRDev\gradle-8.10.2` |
| SDL2 source | 2.30.9 | `F:\QuakeVRDev\SDL2-2.30.9` (`QVR_SDL2_DIR`) |
| Steam Audio | 4.8.1 (Android ARMv8 `libphonon.so`) | `F:\QuakeVRDev\steamaudio\steamaudio` (`QVR_STEAMAUDIO_DIR`) |
| Khronos OpenXR loader for Android | 1.1.53 (AAR) | `F:\QuakeVRDev\openxr\openxr_loader_for_android-1.1.53.aar` (C4) |
| Meta OpenXR SDK | git `bbed2f2` (2026-02-12; headers) | `F:\QuakeVRDev\Meta-OpenXR-SDK` (C4) |

Not installed (still for his approval): the Android Gradle Plugin's Maven download (Gradle's first run of
`Android/`, about 200-300 MB in `~/.gradle` [I]; `package_apk.sh` does without it), the Android Emulator and a system
image (optional), Meta Quest Developer Hub (optional), glslangValidator (optional). The original list, for reference:

Sizes are approximate download sizes [I]; versions are the current stable ones at install time.

| Tool | Version | Size | URL | For |
|---|---|---|---|---|
| Android SDK command-line tools (or Android Studio, ~1.2 GB, bundles them and a JDK) | latest | ~150 MB | https://developer.android.com/studio#command-line-tools-only | sdkmanager |
| Android SDK Platform + Build-Tools + Platform-Tools (adb) | API 34 (the Quest 3's Android 14, SDK 34: C4b; min SDK 29), Build-Tools 34.x | ~250 MB | via sdkmanager; https://developer.android.com/tools/releases/platform-tools | APK build, adb |
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

Next phase: C5i (after 6zs) -- his look call on the body's even light (vr_gl_quake_dlights 1) and on D24 depth; the fights' dynres floor 0.65 (a live fight first); the dlmap eyeshots placed by a scenario; the FFR probe (`vr_xr_direct 1`). Was C5h (after 6zr) -- the per-part body table (`scratch/c5g_parts.sh`, pitch 70), why Quake's way on the body is slower, `vr_depth_float 0`, the resolution step, the FFR probe (`vr_xr_direct 1`), Quest eyeshots of the dlmap pairs. Was C5g (after 6zq) -- build and install the APK (vr_menu.cpp's compile, 6zq), then on the headset `scratch/c5f_parts.sh` (the per-part table, pitch 70) and `SET=key` at 35 and 70 (`vr_gl_alias_body 2` timed: default on for the Quest if faster), the fragment counts (ALIASCONST eyeshots), the FFR probe (`vr_xr_direct 1`, `-xrfoveation`, `vr_xr_foveation 3`), the Quest dlmap eyeshots, `vr_depth_float 0`, the resolution step; the bakes (skin tone into the skin on change, wounds/dents per instance) only where the table shows their cost. Was C5f (after 6zp) -- the body's cut by the other road: its dynamic lights compiled out of its form with Quake's flat add on the CPU, its ambient per vertex without a light loop (6zp: lighting it all per vertex was slower), re-measured with `vr_debug_head_pose` at pitch 35 and 70; then `+vr_depth_float 0` A/B and the resolution step; his worn A/B with `qvr-kit/scratch/quest_worn_ab.py`. Carried from C5d's line: his look call on `vr_gl_world_dlmap 1` (batched: 3 big lights 9.8 -> 4.0 ms GPU, +1.8 ms CPU) for the Quest preset; the body re-measured worn; `+vr_depth_float 0` A/B; then from C5c's line: — measure `vr_gl_world_dlmap 1` (APK of this branch) in the frozen fight and a live e1m1 play at the same spots, interleaved, with its CPU cost (`vr_gl_world_dlmap_info`, the recorder's lightmap events) and his look call on the 16-unit light; keep it in the Quest preset only if it gains. Then (2) `vr_depth_float 0` A/B (D24S8, no code); (3) the eye's compute splits; (4) the multiview/foveation probe; the body's ~1 ms (6zm) as its own item; then the resolution (floor ~0.8, scale toward 1.2). Also 6zk's C5a items. Still open: C2h, NORETRO. Waiting on his call: the armed humanoids' CPU cost (6d), his look call on `vr_rig_lights_world 0`. Decided 2026-11-21: dynamic lights stay shadowless under the Quest preset; the Android Gradle Plugin download approved.

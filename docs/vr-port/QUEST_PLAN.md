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

Next phase: C4c (continued, section 6j) — the black eye scene first: split the real world/alias programs on Adreno
(the ES-header macro probe of 6j's bisection: a constant fragment output, then the vertex shader's outputs and its
`call_data[DRAW_ID]` path cut down), then the boot's
compiles (a program binary cache), the frame's 35 ms (simpleperf, the frame fence in `GL_AcquireFrameResources`), the
menu panel's resolution, the save's freed engine string; no installs. The original C4c scope: the headset worn, and the frame (NEEDS him wearing the Quest 3 over USB for the visual part; no
installs: simpleperf is the NDK's): first what he sees (e1m1's image in both eyes, the `imageRect` origin of 6f, the
menus' panel, the Touch controllers, sound), then why a frame takes 35 ms with the CPU and GPU both under 40% busy
(simpleperf, `vr_profile`; the syncs: query reads, fences, readbacks, `xrWaitFrame`), the first launch's ~55 s of
shader compiles (the second launch's, the driver cache; `OES_get_program_binary`), e1m1's "Save error.", and the ES
paths to the extensions the Quest has (`EXT_clip_control` for the shadows, depth clamp, clip distances) on by
default there. Then C5 (multiview into the swapchain). Without the headset, C2h (no installs): the world textures'
memory under the ES profile (section 6e's open item). Waiting on his call: the Android Gradle Plugin download
(section 7), the armed humanoids' CPU cost (section 6d).

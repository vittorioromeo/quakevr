# The diff with Ironwail

Quake VR forks Ironwail at `1eabd0df` ("Change version to 0.8.2", tag `v0.8.2`; `git merge-base HEAD ironwail/master`).
Everything Quake VR is in `Quake/vr/` (the module) and its own files (`quakevr/`, `QC/`, `Misc/`, `docs/`,
`Windows/OpenXR/`, `Windows/VisualStudio/quakevr*.props`, `zancle.vcxproj`); what remains in Ironwail's own files is
hooks into the module and the engine changes the module needs. To list it:

    git diff --stat v0.8.2 HEAD --diff-filter=M -- . ':!Quake/vr' ':!external'
    git grep -n QVR -- Quake ':!Quake/vr'

## Totals (Ironwail's files changed)

| | Files | Lines added | Lines removed | Hunks |
|---|---:|---:|---:|---:|
| Before (vr-cleanup `852ff42c`) | 79 | 6817 | 354 | 826 |
| After | 73 | 3088 | 320 | 727 |
| With the spatial audio's hooks (`snd_dma.c`, `snd_mix.c`) | 74 | 3099 | 321 | 735 |

Six headers are Ironwail's again (`cvar.h`, `gl_texmgr.h`, `platform.h`, `progs.h`, `view.h`, `zone.h`).

## Where the code went

| Now in | Was in | What |
|---|---|---|
| `Quake/vr/vr_glsl.h` | `gl_shaders.h` (1600 lines) | Quake VR's GLSL as macros (`SHADOW_FUNCTIONS`, `LIQUID_FUNCTIONS`, `QVR_WORLD_FS_LIGHT`...), each spliced into its shader by one `// QVR` line; the 36 shaders' text checked identical before and after |
| `Quake/vr/vr_normalmaps.cpp` | `gl_texmgr.c`, `gl_model.c` | normal maps made from shading and skins, heights, islands, the skin cache key, coverage-kept alpha mips, `VR_LoadNormalMap` |
| `Quake/vr/vr_modelload.cpp` | `gl_model.c`, `r_brush.c` | `.lux` deluxemaps (load and texels), item boxes' texture ranges, external skins, skins' and MD5 skins' normal maps |
| `Quake/vr/vr_render.cpp` (`VR_AliasInstance`) | `r_alias.c` | the alias instance's VR data in one call (`vraliasinstance_t`) |
| `Quake/vr/vr_gfx_gl.cpp` | `gl_rmain.c` | `VR_OpaqueSceneTexture`, `VR_BindOpaqueScene`, `VR_SceneTarget` |
| `Quake/vr/vr_progs.cpp` | `pr_edict.c` | `VR_CheckLoadedReferences` (saved games) |
| `Quake/vr/vr_physics.cpp`, `vr_climb.cpp` | `sv_phys.c` | `VR_ClientSpecialMove` (teleport, climbing), their profiling |
| `Quake/vr/vr_cmdtoken.cpp` | `cmd.c` | the console's tokenizer for arguments of any length |
| `Quake/vr/vr_crash.cpp` | `pl_win.c`, `platform.h` | test runs' crash report and error file |
| `Quake/vr/vr_sleep.cpp` | `main_sdl.c` | the frame cap's high-resolution sleep |
| `Quake/vr/vr_api.h`, `vr_api_render.h` | per-file includes, `glquake.h`, `cmd.h`, `vr_ao.hpp`, `vr_sights.hpp`, `vr_tonemap.h` | `quakedef.h` includes both for every engine file; `VR_TIMED` (load timing) |
| `Quake/vr/vr_engine.hpp` | engine headers | declarations of engine functions only the module calls |
| `Quake/vr/vr.mk` | the three Makefiles | the module's objects, flags and rules |
| `Windows/VisualStudio/quakevr.toolset.props`, `quakevr.props` | `ironwail.vcxproj` | the ClangCL toolset; Release flags (evaluated project items checked identical) |
| `quakevr/.gitignore`, `Quake/vr/.gitignore` | `.gitignore` | the game folder's and the module's ignores |

## What stays inline, and why

- Hot paths: the world traces (`world.c`: narrower hulls, precise hits), the brush and alias draw paths (`r_world.c`,
  `r_alias.c`), the light loops in the shaders: moving them would add calls per entity or per surface.
- Code that needs a file's statics (`framesetup`, `glcanvas`, the console's text, the menus, the zone, the command
  buffer, the texture list): moving it would mean exposing more of the engine than the move saves.
- Engine fixes the module needs (a growing command buffer, lines of any length, no fixed cvar count, a bigger zone,
  atomic config writes): they change Ironwail's own logic.

## Marking

Every hunk in Ironwail's files has a `// QVR` comment on one of its lines (`/* QVR */` inside a macro, `# QVR` in
Makefiles, scripts and workflows, `<!-- QVR -->` in project files); a line Quake VR removed has a `// QVR:` comment in
its place saying why. Two files are not marked: `README.md` (Quake VR's front page) and `ironwail.sln` (the zancle
project; a solution file has no comments). No formatting-only changes.

## Checks (this round)

Clean Release rebuild: 0 warnings. Melee eval (`eval.sh --full`): the table identical to the one before the changes
(168/173). Smoke: e1m1, vrfiringrange, e2m1, e1m5 (screenshots within the runs' own noise). CPU busy a frame on the
profiler's e2m1 scene (`run.sh --exclusive`, 1800 frames, 11 runs each, alternated): median 0.398 ms before, 0.395 ms
after; `models` 0.028 / 0.026 ms.

## Per file

| File | Before | After | Hunks | What for |
|---|---:|---:|---:|---|
| `Quake/gl_texmgr.c` | +1002 -7 | +298 -7 | 28 | normal maps as textures (TexMgr_LoadNormalMap, a texture's normal map, filter modes), coverage-kept mips of alpha-tested textures, 16384 textures, sights' palette, reloads; the making: vr_normalmaps.cpp |
| `README.md` | +245 -37 | +245 -37 | 1 | Quake VR's readme (kept: the repository's front page) |
| `Quake/gl_shaders.h` | +1823 -43 | +192 -43 | 83 | the splice points of vr/vr_glsl.h's GLSL (one line each) and the few upstream lines changed in place |
| `Quake/r_alias.c` | +253 -14 | +218 -14 | 35 | the alias instance's VR data (VR_AliasInstance), normal maps, depth-only shadow casters, alpha to coverage, bone poses, wound painting (R_PaintAliasWounds), mirrored and transformed instances |
| `Quake/world.c` | +207 -8 | +206 -8 | 18 | narrower player hulls, precise model hits, hand touches (SV_AreaEdicts), gibs hit by shots, trace profiling; kept inline (the traces are hot) |
| `Quake/cmd.c` | +259 -49 | +163 -35 | 36 | the command buffer grows (Cbuf_Reserve) and reads from a position; lines and arguments of any length (VR_ParseToken: vr_cmdtoken.cpp); config migration; counts |
| `Quake/r_world.c` | +181 -4 | +179 -4 | 26 | depth pre-pass, deluxemaps, normal maps, parallax and detail per texture, alpha to coverage, the liquids' wave mesh and refraction |
| `Quake/menu.c` | +151 -13 | +149 -13 | 26 | the VR menu (m_vr), VR Calibration row, list scrolling for the controllers, text outlines |
| `Quake/cvar.c` | +93 -26 | +94 -26 | 18 | no fixed cvar count (grows), appended then sorted when needed, values of any length |
| `Quake/gl_draw.c` | +111 -4 | +110 -4 | 10 | lasting run-time pics (the big font), the menu canvas in the headset, glyph size and text outline |
| `Quake/gl_rmisc.c` | +95 -14 | +93 -14 | 14 | liquid alphas from the settings and the map's keys (R_UpdateLiquidAlpha), shader storage ranges |
| `Quake/gl_rmain.c` | +194 -11 | +89 -11 | 47 | eye rendering: scene format and samples, post-process gamma/target, projection, shadow maps, tone, profiling scopes |
| `Quake/host.c` | +94 -6 | +92 -6 | 51 | VR frame hooks, config written atomically and merged, motion takes' server frames, test-run pacing |
| `Quake/gl_model.c` | +613 -6 | +84 -7 | 30 | the model-loading hooks (vr_modelload.cpp, vr_normalmaps.cpp), relit maps, derived models, load timing |
| `Quake/gl_screen.c` | +74 -7 | +72 -7 | 18 | the HUD to the window only (the wrist gadget), async PNG screenshots, modal dialogs in the headset, test runs' unpaced frames |
| `Quake/console.c` | +68 -1 | +67 -1 | 11 | notify lines for the wrist gadget's log and hologram |
| `Quake/r_sprite.c` | +66 -1 | +65 -1 | 10 | soft sprites (vr_particles.cpp) |
| `Quake/sv_phys.c` | +65 -10 | +51 -10 | 28 | client move hooks (VR_ClientSpecialMove, climbing), props, rigid bodies riding pushers, second think timer |
| `Quake/image.c` | +44 -7 | +45 -7 | 9 | image decoding from memory, prefetch, PNG to a full path from any thread, load timing |
| `Quake/zone.c` | +45 -2 | +45 -2 | 9 | 32 MiB zone, usage counts, heap check only when PARANOID |
| `Quake/cl_tent.c` | +40 -5 | +39 -5 | 24 | decals, impact lights, heat haze, beams as drawn (ropes), temp-entity limit counted |
| `Quake/cl_main.c` | +41 -5 | +39 -5 | 17 | dynamic lights tuned, VR particles and blood trails, limits counted |
| `Quake/pr_edict.c` | +92 -7 | +36 -7 | 9 | NUM_FOR_EDICT_CHECKED, edict_print fix, a long "angle" safe |
| `Quake/glquake.h` | +66 -2 | +38 -2 | 9 | the lights' and frame data's VR fields (the shaders' structs), glprogs entries, draw-call counting, a few declarations |
| `Quake/sv_main.c` | +32 -2 | +31 -2 | 17 | VR weapon IDs, profiling and load timing, packet limit counted |
| `Quake/gl_mesh.c` | +33 -0 | +31 -0 | 4 | per-vertex occlusion and muzzle-flash references in the model VBO, one model's buffer deleted |
| `Quake/pr_exec.c` | +28 -1 | +28 -1 | 3 | QuakeC profiling scopes |
| `Quake/cl_parse.c` | +25 -3 | +24 -3 | 13 | wrist notifications, play-space turn, map-load profiling, server prints marked |
| `Quake/sv_user.c` | +21 -5 | +20 -5 | 10 | stamina, swimming and wading speeds |
| `Quake/view.c` | +21 -4 | +20 -4 | 4 | V_SetupView split out (headless frames) |
| `Quake/gl_vidsdl.c` | +23 -5 | +17 -5 | 4 | test runs hidden and in the background, 2D canvas alpha, shader timing |
| `Quake/sys_sdl_win.c` | +19 -2 | +18 -2 | 7 | directory-listing cache invalidation, atomic replace, ILFree |
| `Quake/r_part.c` | +19 -0 | +18 -0 | 6 | Quake VR's particles replace the old effects |
| `Quake/r_brush.c` | +97 -0 | +18 -0 | 3 | the deluxemap texture (texels: vr_modelload.cpp's VR_FillSurfaceLux) |
| `Quake/pr_cmds.c` | +14 -4 | +13 -4 | 7 | more temp strings, late model precache, torch placement |
| `Quake/common.c` | +15 -2 | +14 -2 | 7 | longer command line, game-folder listing cache, messages |
| `Quake/main_sdl.c` | +84 -4 | +9 -3 | 7 | crash report, start-up timing, high-resolution frame-cap sleep (vr_sleep.cpp), frames while unfocused |
| `Quake/sys_sdl_unix.c` | +13 -0 | +12 -0 | 4 | directory-listing cache invalidation, atomic replace |
| `Quake/host_cmd.c` | +12 -0 | +11 -0 | 7 | saved games: live edict during parsing, references checked (VR_CheckLoadedReferences) |
| `Windows/VisualStudio/ironwail.sln` | +10 -0 | +10 -0 | 2 | the zancle project (a .sln has no comments: not marked) |
| `Quake/Makefile.w32` | +28 -3 | +7 -2 | 4 | include vr/vr.mk; static libstdc++; clean |
| `Quake/Makefile.w64` | +28 -3 | +7 -2 | 4 | include vr/vr.mk; static libstdc++; clean |
| `Quake/quakedef.h` | +3 -3 | +6 -1 | 3 | MAX_NUM_ARGVS 256; enum-defining headers first; the module's C headers (vr_api.h, vr_api_render.h) for every engine file |
| `Quake/cl_input.c` | +5 -2 | +4 -2 | 3 | more commands per frame |
| `Quake/Makefile` | +27 -2 | +5 -1 | 3 | include vr/vr.mk; clean |
| `.gitignore` | +30 -0 | +6 -0 | 1 | build and packaging output (the game folder: quakevr/.gitignore) |
| `Quake/snd_dma.c` | +4 -1 | +11 -2 | 7 | 4096 known sounds, counted; the spatial audio's hooks (vr_audio.cpp: the listener, the hands' and moving sounds, a new sound, the statics not combined, whole frames) |
| `Quake/snd_mix.c` | 0 | +4 -0 | 3 | the spatial audio's voices painted, their channels skipped, vr_snd_capture |
| `Quake/world.h` | +9 -0 | +5 -0 | 1 | MOVE_HITGIBS, MOVE_HITMODEL flags |
| `Quake/gl_shaders.c` | +5 -0 | +5 -0 | 2 | the wound-painting programs |
| `Quake/gl_model.h` | +5 -0 | +4 -0 | 4 | texture uvclamp, deluxemap samples, Mod_ReloadAliasModel |
| `Quake/gl_rlight.c` | +4 -0 | +3 -0 | 2 | light-point hook |
| `Quake/menu.h` | +2 -1 | +2 -1 | 1 | m_vr state |
| `Quake/image.h` | +3 -0 | +3 -0 | 3 | Image_LoadImage hidden from C++, Image_WritePNGPath |
| `Quake/cl_demo.c` | +3 -0 | +2 -0 | 1 | world texts in demos |
| `Windows/VisualStudio/ironwail.vcxproj` | +20 -4 | +2 -0 | 2 | imports quakevr.toolset.props (clang-cl) and quakevr.props |
| `Quake/pl_win.c` | +191 -0 | +2 -0 | 1 | no error dialog in test runs (VR_ErrorDialogSuppressed) |
| `Quake/input.h` | +2 -0 | +2 -0 | 2 | enum hidden from C++ |
| `.github/workflows/mingw_ci.yml` | +1 -1 | +1 -1 | 1 | g++ for the module |
| `Quake/gl_refrag.c` | +2 -1 | +1 -1 | 1 | a prop's size in its bounds |
| `.github/workflows/linux_ci.yml` | +6 -2 | +1 -1 | 1 | CXX for the module |
| `Quake/sys.h` | +1 -0 | +1 -0 | 1 | Sys_ReplaceFile |
| `Quake/build_cross_haiku64-sdl2.sh` | +2 -1 | +1 -0 | 1 | export CXX |
| `Quake/build_cross_win64-sdl2.sh` | +3 -2 | +1 -0 | 1 | export CXX |
| `Quake/console.h` | +2 -0 | +1 -0 | 1 | Con_ServerPrint |
| `Quake/build_cross_win64.sh` | +3 -2 | +1 -0 | 1 | export CXX |
| `Quake/build_cross_haiku32-sdl2.sh` | +2 -1 | +1 -0 | 1 | export CXX |
| `Quake/gl_fog.c` | +2 -0 | +1 -0 | 1 | an eye in a liquid |
| `Quake/sv_move.c` | +2 -0 | +1 -0 | 1 | monsters' narrower hull at ledges |
| `Quake/draw.h` | +3 -0 | +1 -0 | 1 | draw_textoutline |
| `Quake/build_cross_win32-sdl2.sh` | +3 -2 | +1 -0 | 1 | export CXX |
| `Quake/build_cross_win32.sh` | +3 -2 | +1 -0 | 1 | export CXX |
| `Quake/cmd.h` | +12 -0 | +1 -0 | 1 | config_not_loaded |
| `CMakeLists.txt` | +1 -0 | +1 -0 | 1 | include Quake/vr/vr.cmake |
| `Quake/cvar.h` | +1 -0 | upstream | 0 |  |
| `Quake/gl_texmgr.h` | +1 -0 | upstream | 0 |  |
| `Quake/platform.h` | +5 -0 | upstream | 0 |  |
| `Quake/progs.h` | +2 -0 | upstream | 0 |  |
| `Quake/view.h` | +1 -0 | upstream | 0 |  |
| `Quake/zone.h` | +2 -0 | upstream | 0 |  |

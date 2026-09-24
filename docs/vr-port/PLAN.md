# Quake VR → Ironwail port plan

Branch `vr-ironwail` starts at Ironwail **v0.8.2** (`1eabd0df`). The old engine (branch `develop`) is Quakespasm-Spiked
`36b2046f57` converted to C++; the per-subsystem inventories of its *functional* changes are in [`inventory/`](inventory/):

| # | Inventory | Size of real change |
|---|---|---|
| 01 | [Server & physics](inventory/01-server-physics.md) | VR usercmd → entvars, hand/weapon touch, teleport & room-scale moves, pusher/toss/touch rewrites |
| 02 | [QC VM & builtins](inventory/02-qcvm-builtins.md) | 32 builtins #79–110 (8 collide with IW), custom progdefs ABI (CRC 52440), 40 spawn parms, 3 QC entry points |
| 03 | [Client, net, protocol](inventory/03-client-net-protocol.md) | `PROTOCOL_QUAKEVR` 8682: 131-byte `clc_move`, VR clientdata, vec3 scale/offset, particle2, world text |
| 04 | [Rendering](inventory/04-rendering.md) | per-eye FBOs, ~30 view entities, alias scale/offset/mirror/zeroBlend, particles rewrite, world text |
| 05 | [View, HUD, menus, input](inventory/05-view-hud-menu-input.md) | VR `V_CalcRefdef`, 2D as world quad, hand-attached sbar, ~30 VR menu pages, virtual keyboard |
| 06 | [VR core](inventory/06-vr-core.md) | `vr.cpp` architecture, ~60 engine call sites, OpenVR→OpenXR mapping, all `vr_*` cvars |
| 07 | [Misc & build](inventory/07-misc-build.md) | pak loading with gaps, start.bsp selector, autosave, cvar handles, build/release layout |

## Status

| Phase | State | Notes |
|---|---|---|
| P0 Scaffolding | ✅ done | `Quake/vr/`, mock backend, build integration (MSBuild + CMake) |
| P1 QC & game data | ✅ done | CRC-5927 progs, by-name VR builtins, entry points, spawn parms 17–40, `quakevr` game folder layering hipnotic/rogue, opt-in droptofloor. Flat-mode check: e1m1/e1m2/hip1m1/r1m1/start/vrstart load, changelevel and save/load work. |
| P2 Protocol & server | ✅ done | `PRFL_QUAKEVR` RMQ extension (VR move, stats 64+, entity scale/offset, `svc_quakevr`, beam ids, late model/sound precaches); hand/weapon touch on networked hand data, teleport, room-scale pass, head-relative movement, step size, `think2`, `lastwatertime`, touch rules. Flat-mode check: hand grab picks up a weapon on vrfiringrange; demos record/play; save/load/changelevel across all campaigns. |
| P3 View & entities | ✅ done (core) | Weapons in both hands, hands + fingers (anchored to weapon vertices via the rebuilt BuildTris order), holsters, holster slots, torso, weapon buttons; mirroring, per-model weapon scaling, zero blend (shader), light modifier; muzzles from anchor vertices; 32×66 `vr_wofs_*` table and 78 cvar defaults from the shipped config. Flat mode uses the old "fake VR" hand placement. |
| P4 Stereo + OpenXR | ✅ done (untested on HMD) | OpenXR backend (instance/session/stage space/grip poses/swapchains/frame loop); per-eye rendering through Ironwail's pipeline with eye-sized framebuffers, asymmetric projection and post-process into the swapchain; desktop mirror (`vr_mirror`); 2D canvas composited on the mirror and shown as a panel in the headset while menus are open; play-space yaw re-based on `svc_setangle`. Verified with the mock backend (both eyes, parallax, menu panel); OpenXR verified up to "no headset" against Virtual Desktop. |
| P5 Input & haptics | ✅ done (untested on HMD) | Controller buttons as bindable Quake keys by role (issue #12; `quakevr/vr_bindings.cfg`, `vr_checkbindings`); stick locomotion (head- or hand-directed, swim by pointing), snap/smooth turning, menu navigation; networked `haptic`. Runtime hand/head velocities; throw estimator and true-scale throws; sticky grip option (issue #31). Room-scale walking, hotspots (holsters, 2H grab, hand switch) with hover highlights, body yaw from the hands, two-handed aiming with virtual stock, flick reload, teleport, finger curls from trigger/grip/thumb sensors. Mock controller commands for desktop testing. Left: the 2H "fixed" display mode. |
| P6 Menus & HUD | next | |

Known gaps carried forward:
- `start` resolves to rogue's start.bsp (last mission pack layered); decide on a start-map selector.
- World text is networked but not rendered (P7); `particle2` draws plain blood puffs until presets (P7); `haptic` is networked (P5).
- `.weapon` holds weapon IDs, so Ironwail's HUD weapon highlighting is wrong until P2/P6.
- Hand model is drawn as a plain view model until P3.
- Deferred old-engine physics rewrites, to evaluate in VR first: `SV_PushMove` (Ironwail's `sv_gameplayfix_elevators` may suffice), `SV_PushEntity` tracing from origin−push, `SV_Physics_Toss` ground pre-check (items resting on moving platforms).
- Mid-demo recording does not re-emit VR state (world texts); VR stats are resent by the stat channel.
- P3 leftovers: two-handed "fixed" display mode, finger-tracking frames, hovered-holster highlight, body yaw from the hands (P5); weapon ammo text, beam model shrinking (P7); stair smoothing of view entities; anchor vertices use the current frame without inter-frame lerp.

## Principles

1. **Upstream stays pristine-looking.** Engine files keep Ironwail's C, style and names. Every engine edit is a
   *hook*: a call into the VR module, a guarded early-out, or a small data addition — marked with a `// QVR` comment
   so `git diff v0.8.2 -- Quake/` and `git grep QVR` show the entire engine footprint.
2. **VR logic lives in `Quake/vr/`.** Modern C++ (C++20, glm) is fine there; the engine sees only
   `Quake/vr/vr_api.h`, a plain-C header of `extern "C"` functions and POD structs.
3. **No wire/ABI compatibility with the old engine.** We own the QC, so we fix things the clean way (see
   *Decisions*) instead of reproducing old numbering clashes and latent bugs flagged in the inventories.
4. **Gameplay rule changes are opt-in** (cvars, defaulted on by the VR mod's config) so vanilla Quake still plays
   like vanilla Ironwail with `vr_enabled 0`.
5. **Every milestone builds and runs**, and each commit is one coherent step, so upstream Ironwail updates can be
   merged with minimal conflicts.

## Decisions

Confirmed by the author on 2026-09-24: OpenXR directly, clean compatibility break, a proper `quakevr` game folder,
gameplay rule changes opt-in via cvars. The remaining rows follow from those.

| Topic | Proposal | Why |
|---|---|---|
| VR runtime | **OpenXR directly** behind a small backend interface, plus a **mock backend** (desktop, keyboard/mouse-driven hands) | Core-profile renderer means the VR render/submit code is rewritten anyway; writing it once for OpenXR avoids throwaway OpenVR work. The mock backend enables testing without a headset (and in CI). |
| Progs ABI | VR fields/globals move **after `end_sys_fields`** in `defs.qc` and are looked up **by name** (like IW's ext fields); keep vanilla CRC 5927 | IW loads the progs unmodified; no mid-struct offset shifts. |
| VR builtins | Renumber to a **private high range** and/or FTEQCC named builtins (`#0:name`); use IW's existing `min/max/pow/strlen/substring/sin/cos` where semantics match, add degree-based trig under new names | Removes all 8 collisions; `WriteVec3` etc. become ordinary extension builtins. |
| Protocol | New protocol number derived from IW's FitzQuake/RMQ path; VR data in **new clc/svc numbers from a free range**; VR stats via IW's stat system; vec3 scale/offset in unused `U_` bits 24–31 | Avoids clashes with 2021-rerelease svcs/EF bits; drops old baseline bugs. |
| Spawn parms | Extend to the 40 parms the QC uses, only when VR progs are loaded | IW supports 16. |
| Controller input | OpenXR actions call **commands directly** (`+attack`, `+offhandattack`, impulses) instead of faking key presses | No dependency on a personal `config.cfg`. |
| Weapon offsets | Regenerate engine defaults from the shipped `config.cfg`; later move the 32×69 `vr_wofs_*` table to a data file | Removes dependence on a personal config. |
| 2D in VR | Render IW's GUI to an offscreen texture once per frame, draw it as a quad per eye (menus) / attach to hand (sbar) | Fits IW's NDC-space GUI batching. |
| Menus | Table-driven `vr_menu.c` on IW's list-menu helpers; drop "Change Map" (IW has a Maps menu) | Small hook in `M_Draw/M_Keydown/M_Mousemove`. |
| Old-QVR bugs | Do **not** port: `cl.items` clobber, baseline scale/model bits, broken extended-save parser, mirror blit rect | Flagged in inventories 03/06/07. |

Open questions that can wait until their phase are listed at the end of each inventory.

## Phases

Each phase ends with a build + run check. "Flat" = desktop with the mock backend; "HMD" = needs the author with a
headset.

### P0 — Scaffolding
- `Quake/vr/` with `vr_api.h` (C API), `vr_main.cpp` (init/shutdown/frame), `vr_backend.h` + `vr_backend_mock.cpp`.
- Build integration: VS project (C++20 for `vr/*.cpp`), CMake (`project(... C CXX)`), glm as a vendored header-only dep.
- Core cvars (`vr_enabled`, `vr_backend`, `vr_world_scale`), `Host_Init`/`Host_Shutdown`/`Host_Frame` hooks.
- ✅ Check: vanilla behaviour unchanged with `vr_enabled 0`; flat run with the mock backend.

### P1 — QC & game data (Quake VR mod runs in flat mode, no VR yet)
- `defs.qc`/`builtins.qc` updates per the ABI decision; VR field lookup by name; builtins module `vr/vr_builtins.c`.
- 40 spawn parms, QC entry points (`OnSpawnServerBeforeLoad/AfterLoad`, `OnLoadGame`), `setmodel` auto-precache,
  `droptofloor` variant, cvar handles.
- Game data layout: a proper `quakevr` game folder on top of id1 (no pak-gap hack); decide how mission-pack maps are
  reached (loader hook vs documented setup).
- ✅ Check (flat): `vrstart` and e1m1 load with `vrprogs.dat`, monsters/items/weapons behave, save/load works.

### P2 — Protocol & server gameplay
- VR usercmd (client → server), VR stats, scale/offset entity bits, `particle2`, world-text messages.
- Server: VR usercmd → entvars, `handtouch`/`vr_wpntouch`, teleport, room-scale move, step size, `think2`,
  pusher/toss/touch changes behind cvars. Weapon-touch uses the *networked* hand state (fixes the listen-server-only
  design).
- ✅ Check (flat + mock hands): demos record/play, a second client connects, hands can touch/grab items.

### P3 — View & entities
- VR `V_CalcRefdef` path; view-entity construction (weapons, hands/fingers, holsters, torso, buttons) in the VR module.
- IW alias renderer: per-instance scale, offset, mirror, light override, zeroBlend; anchor-vertex remap table.
- ✅ Check (flat): third-person-ish debug camera shows hands/weapons tracking mock poses.

### P4 — Stereo rendering + OpenXR
- OpenXR instance/session/swapchains (`XR_KHR_opengl_enable`), per-eye asymmetric projection in `R_SetFrustum`,
  per-eye setup→draw ordering, reversed-Z near plane, desktop mirror, gamma handling.
- 2D → offscreen texture → world quad; hand-attached sbar.
- ✅ Check: flat (mock stereo to window) + **HMD**.

### P5 — Input & haptics
- OpenXR action sets/bindings (Index, Touch, Vive, WMR, generic) replacing `actions.json`; locomotion, snap/smooth
  turn, teleport, 2H aiming, weapon buttons, flick reload; haptics (networked message for remote clients);
  finger curls via `XR_EXT_hand_tracking` / trigger-grip fallback.
- ✅ Check: **HMD**.

### P6 — Menus & HUD
- `vr_menu.c` pages, virtual keyboard, sbar off-hand ammo/clip, ammo-type semantics across IW's HUD styles.

### P7 — Effects & polish
- Particle presets/atlas on IW's particle path, world text rendering, laser/dot crosshair, debug helpers (`vr_showfn`)
  on a small core-profile line/point batcher, optional player/hand shadows.

### P8 — Release
- Packaging (exe + OpenXR loader + paks + default config), README/install docs, CI build.

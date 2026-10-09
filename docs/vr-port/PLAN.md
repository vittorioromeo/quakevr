# Quake VR → Ironwail port plan

Branch `vr-ironwail` starts at Ironwail **v0.8.2** (`1eabd0df`). The old engine (branch `develop`) is Quakespasm-Spiked
`36b2046f57` converted to C++. This file keeps the port's principles and the decisions they led to; what the game
does now is in [FEATURES.md](../FEATURES.md).

The plan's status table (a snapshot of 2026-09-25), its known gaps (all closed since) and its phases P0-P8 were
removed on 2026-10-09, together with `inventory/`, the old engine's functional changes per subsystem (2026-09-24,
seven files: server and physics, QC VM and builtins, client/net/protocol, rendering, view/HUD/menus/input, VR core,
misc and build). Both are in git history: `git show 079f50de8:docs/vr-port/PLAN.md` and
`git show 079f50de8:docs/vr-port/inventory/<file>`.

See also [PORTING.md](PORTING.md) (moving the module to another engine) and [MODS.md](MODS.md) (other mods'
progs in VR).

## Principles

1. **Upstream stays pristine-looking.** Engine files keep Ironwail's C, style and names. Every engine edit is a
   *hook*: a call into the VR module, a guarded early-out, or a small data addition — marked with a `// QVR` comment
   so `git diff v0.8.2 -- Quake/` and `git grep QVR` show the entire engine footprint.
2. **VR logic lives in `Quake/vr/`.** Modern C++ (C++20, glm) is fine there; the engine sees only
   `Quake/vr/vr_api.h`, a plain-C header of `extern "C"` functions and POD structs.
3. **No wire/ABI compatibility with the old engine.** We own the QC, so we fix things the clean way (see
   *Decisions*) instead of reproducing old numbering clashes and latent bugs the old engine's inventories flagged.
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

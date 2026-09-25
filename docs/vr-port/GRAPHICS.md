# Graphics: shadows, lighting, and why Quake VR looks flat

Research on what it would take to make the game look less flat: shadows, dynamic lighting and similar. It has two
parts: an audit of Ironwail's renderer (what it does today, where new work plugs in) and a survey of how other
Quake engines, the 2021 re-release and VR ports did it. Costs are estimates for PC VR on a Quest 3 (about 2064×2208
per eye, an RTX 3070-class GPU, 90 Hz: 11.1 ms per frame).

## Status

Done (first step, see "Done" below): #1 re-lit maps, #2 model lighting, #4 the muzzle flash at the gun, #5 blob
shadows for monsters and items, #6 anti-aliasing default. Second step, [LIGHTING.md](LIGHTING.md): #3 per-pixel
dynamic lights on models, #7 map lights' shadows of moving things, #8 shadowed dynamic lights.

## Why it looks flat today

- **Monsters, weapons, hands and the body are lit as a single colour each.** `R_SetupAliasLighting` samples the
  lightmap straight below the model (`R_LightPoint`) and adds dynamic lights as one number for the whole model
  (`r_alias.c:226-291`). The only shading is per vertex, from a **fixed** world direction ((+X +Z)/√2,
  `gl_shaders.h:1155-1158`), not from where the light is. Up close in VR this is the flattest thing on screen.
- **Dynamic lights don't touch models per pixel**, and on the world they use GLQuake's old formula: distance from the
  surface's plane, no angle term, no occlusion (lights leak through walls), and capped at `1 - lightmap`
  (`gl_shaders.h:666-710`), so a muzzle flash barely shows in a lit room. (They are per-pixel through a light-cluster
  grid, which is good infrastructure.)
- **No shadows at all.** GLQuake's `r_shadows` is gone from Ironwail; the only shadow is Quake VR's blob under the
  player and hands.
- **Baked lighting is id's 1996 lightmaps**: 16-unit luxels, no ambient occlusion, no bounce light, grey unless a
  `.lit` is installed. Ironwail reads `.lit` (version 1) but none of the extended lighting data ericw-tools can bake
  (BSPX: finer lightmaps, HDR, light direction, light grid).
- **No MSAA in VR by default** (`vid_fsaa 0`), and textures are sampled nearest with linear mips, which shimmers.
- **The muzzle-flash light is at the old flat-screen position** (16 up, 18 forward of the player origin,
  `cl_main.c:615-627`), not at the gun in your hand.

Structure worth knowing: both eyes re-run the whole view setup and scene (`vr_stereo.cpp`), so anything computed
once per frame (a shadow map) is shared by the eyes, while screen-space effects (SSAO, bloom) cost double. Ironwail
is a forward renderer — what Valve recommends for VR, with MSAA.

## What others did

| Engine / release | Lighting and shadows | Notes |
|---|---|---|
| **DarkPlaces** | Per-pixel dynamic lights with shadows (`r_shadow_realtime_dlight`, on by default), optional realtime world lights from map light entities or `.rtlights` files, shadow maps (replaced stencil volumes), deluxemaps, normal/gloss maps, bloom/HDR, bounce grid | QuakeQuest (Team Beef's Quest port) runs it with dynamic-light shadows on. Realtime *world* lights are "very slow" without curated `.rtlights`. |
| **FTE** | Realtime lights, shadow maps, deluxemaps, PBR materials, bloom/HDR | Wrote the BSPX spec (HDR lightmaps, light direction, finer/decoupled lightmaps, light grid). |
| **2021 re-release (KEX)** | Coloured lightmaps, ambient occlusion, AA, "dynamic shadows": monsters and players cast shadows from the map's static lights (`r_staticshadows`); more dynamic lights (monster muzzle flashes, glowing projectiles, quad/pent lights) | id1 maps re-lit with bounce light came out brighter, which some players dislike. Shadows and AO split opinion; DOF and motion blur get turned off. |
| **vkQuake / Quake: Ray Traced** | Ray-traced shadows / full path tracing | Needs Vulkan and RT hardware; not viable in stereo at 90 Hz. |
| **QuakeSpasm-Spiked** | Loads HDR lightmaps, decoupled lightmaps and the light grid | The reference to port BSPX loaders from. |
| **GLQuake / QuakeSpasm `r_shadows`** | Model flattened onto the floor along a fixed direction | Cheap, crude. |
| **ericw-tools** (`light`) | Re-lights an **existing compiled .bsp** from the light entities inside it: `-dirt` (baked AO), `-bounce`, `-extra4` (smooth shadows), `-lit`, `-lux`, BSPX lumps, `-lightgrid`, `-world_units_per_luxel`/`-lmshift` (finer luxels without sources) | GPL-2.0. No map sources needed. |

VR-specific findings: cast shadows measurably improve perceived contact with the ground in VR; SSAO differs between
the eyes and is a known discomfort source; Team Beef's Doom 3 on Quest could not hold 90 Hz with shadows (standalone
GPU); Valve's Lab renderer ran up to 18 shadowing lights in a forward renderer on PC.

## Recommendations

Ordered by value for effort. "New assets" means files beyond what the player already owns.

### Tier 1 — cheap, big impact

| # | Change | Impact | Frame cost | Effort | Classic look | New assets |
|---|---|---|---|---|---|---|
| 1 | **Re-light the maps at install time with ericw-tools**: `light -extra4 -dirt -lit` (gentle or no `-bounce`), writing into `quakevr/maps` from the player's own id1/hipnotic/rogue .bsp files | High: corners and contact areas darken, coloured light; the world stops looking flat | 0 | A script and a packaged tool; the plain lightmap lump and `.lit` need **no engine change** | Low with AO and colour; bounce brightens (the re-release complaint), so keep it optional | Generated locally (id's maps can't be redistributed re-lit) |
| 2 | **Model lighting from real lights**: the shading direction from nearby light entities (parsed from the map at load, the strongest few in sight of the model) instead of the fixed direction | High: monsters and weapons look solid | ~0 | Small: a per-instance direction in `aliasinstance_t` and the alias vertex shader | Low | None |
| 3 | **Per-pixel dynamic lights on models** (the alias shader walks the same light-cluster grid the world uses, with N·L) and **N·L plus a softer cap on the world** | Medium-high: muzzle flashes, rockets and explosions light monsters and your hands | +0.1–0.3 ms | Small: ~150 lines, 1–2 days | Low | None |
| 4 | **The muzzle-flash light at the VR gun**, and the re-release's extra lights in Quake VR's QuakeC (monster muzzle flashes, glowing ogre/hell knight projectiles, quad/pent light) | Medium | Negligible | Tiny | None | None |
| 5 | **Blob shadows for monsters and items** (extend `vr_shadows.cpp` to visible entities, sized by their box, placed below along the nearest light's direction) | High for grounding in VR | < 0.1 ms | Small | Low | None |
| 6 | **Defaults**: `vid_fsaa 4`, trilinear filtering in VR, a light default fog on maps without any | Medium: less shimmer, depth cues | MSAA ~0.5–1 ms | Trivial | Filtering softens the pixel look; make it a setting | None |

### Tier 2 — medium

| # | Change | Impact | Frame cost | Effort | Notes |
|---|---|---|---|---|---|
| 7 | **Static-light shadows like the re-release**: shadow maps from the few map lights nearest the player, with only moving things (monsters, player body, items) as casters, darkening the baked light under them | Highest for grounding | 0.5–1.5 ms, shared by both eyes | 2–3 weeks | Alias models re-render easily from a light (they are instanced); the world is not needed as a caster (its shadows are baked). |
| 8 | **Shadow maps for the strongest 2–4 dynamic lights** (cube maps, 256–512 px, once per frame for both eyes) | High, and fixes dynamic lights leaking through walls | 0.1–0.3 ms per light + 0.2–0.5 ms shading | 3–4 weeks | Needs the world drawn from the light: a second GPU cull pass (Ironwail's cull is camera-driven) or a static world index buffer. Hands and view weapons should not cast. |
| 9 | **BSPX loaders** (port from QuakeSpasm-Spiked): finer lightmaps (`LMSHIFT`/`DECOUPLED_LM`), HDR lightmaps (`LIGHTING_E5BGR9`), light direction (`LIGHTINGDIR`), light grid (`LIGHTGRID_OCTREE`) | Sharper baked shadows up close (matters in VR); the light grid fixes models going dark in the air; direction enables directional model shading and bump mapping from baked light | 0 | Medium per lump; the 16-unit luxel is hard-coded in several places | Pairs with #1 (`-lmshift`, `-lightgrid`, `-bspxlux`). |
| 10 | **Subtle bloom** (and tone mapping if HDR lightmaps are adopted) | Medium: fire, lava, fullbrights glow | 0.3–0.6 ms (per eye) | Small without HDR; medium with an RGBA16F scene | Keep it subtle; after the scene, before the VR overlays. |

### Tier 3 — big, or not recommended

- **Realtime lights for all map lights (DarkPlaces "rtworld")**: id's maps have hundreds of fill lights meant for
  baking; they need hand-made light files and cost 3–8+ ms. Not worth it; #1 + #7 get most of the look.
- **Normal/specular maps**: needs a texture pack (e.g. QRP, with attribution) and moves toward an HD look; only worth
  it with #9's light direction.
- **SSAO**: skip. #1 bakes ambient occlusion into the world for free, SSAO doubles in stereo (1–2.5 ms) and differs
  between the eyes. Per-model contact shadows (#5, #7) cover what it would add.
- **Ray tracing / path tracing**: needs Vulkan and RT hardware; not at 90 Hz in stereo.

## Suggested plan

1. #1 (re-light at install), #6 (defaults), #4 (lights at the gun and from monsters): days, no engine risk.
2. #2, #3, #5: models lit from real lights, per-pixel dynamic lights on models, shadows for everything: about a week.
3. #9's light grid and finer lightmaps together with a re-light using them.
4. #7, then #8: real shadows, shared between the eyes.
5. #10 if the look wants it.

Everything in tiers 1–2 fits Ironwail's existing structure (forward rendering, GPU light clusters, instanced
models) and keeps the classic pixel look; none of it needs new art.

## Done

- **Re-lit maps** (#1). `Misc/quakevr/relight_maps.py --quake <Quake folder> --light <ericw-tools light>` extracts
  the maps of id1, hipnotic and rogue from the player's own paks and re-lights them with
  `-extra4 -dirt -dirtdepth 96 -lit` with stronger ambient occlusion and no bounced light since round 10 (`--bright`: the
  earlier look with `-bounce -bouncescale 0.5`; about 4 s a map, 73 maps, 143 MB),
  into `quakevr/relit/<game>/maps/`. The engine loads `relit/<game>/maps/X.bsp` and its `.lit` in place of
  `maps/X.bsp` when that comes from `<game>` (`VR_ModelFile` in `Mod_LoadModel` and `Mod_LoadLighting`), per game
  because id1, hipnotic and rogue all have a `start.bsp`; a mod's own maps are left alone. `vr_relit_maps 0` plays
  the original lighting (next map). The relit files are generated locally and not committed (`.gitignore`).
  The mod's own maps are relit in place instead (committed), each with its own settings, by
  `Misc/quakevr/relight_quakevr_maps.py [--quake <Quake folder>] [--only vrtutorial]` (the palette of id1 is for the
  glowing textures' colours). `vrfiringrange`: its seventeen "light" 1200 lamps (a lightmap at 255 nearly everywhere:
  glaring, oversaturated yellow) become a sun from the east-north-east (`_sunlight` 150, `_sun_mangle` "200 -40 0"),
  a sky dome (`_sunlight2` 280) and `-dirt`, in the `.bsp` and its `.ent`; about 150 on the sunlit floor and 90 in
  shade, so models lit on a par with it do not glare. `vrtutorial` had never been lit (an empty lightmap: the engine
  draws that fullbright): now its own lamps light it, the strip lights over the boards with a longer reach (`wait`
  0.6 in the courtyard, 0.75 indoors) and the lamp posts' four lights at 60, plus the glowing textures' lights
  (`glow_lights`, coloured `.lit`), `-dirt -dirtscale 1.5`, no bounce, and a faint cool sky and moon over the open
  courtyard (`_sunlight2` 80, `_sunlight` 50); its sixteen "light" 1200 fill lamps 300 units up are dropped. Pools
  of light at the boards and lamps, dark corners and corridors between them. `vrstart` is left fullbright (no
  lightmap and no lights; its worldspawn `"light" "300"` is a minimum light for a menu-like hub).
- **See-through water.** id's maps were vised with liquids as walls, so the engine keeps their water, slime and
  teleporters opaque whatever `r_wateralpha` says (changing it prints "Map does not appear to be water-vised").
  The relit maps get water-vised visibility from the VisPatch data files (`id1.vis`, `hipnotic.vis`, `rogue.vis`,
  or `<game>/vispatch.dat`; vispatch.sourceforge.net): give their folder to the relight,
  `relight_maps.py ... --vis-dir <folder>` (or set `QUAKEVR_VISPATCH`), which patches every relit map, also ones
  already up to date. `Misc/quakevr/vis_maps.py --vis-dir <folder> [--relit quakevr/relit]` does it on its own
  (only the visibility and leaf lumps change; a patch whose leaves are not the map's is refused);
  `vis_maps.py --check quakevr/relit/id1/maps` reports, and in game `developer 2; map e1m1` prints
  "maps/e1m1.bsp is vised for transparent water tele slime". `quakevr.cfg` sets `r_lavaalpha 1` (lava stays
  opaque); `r_wateralpha` sets the rest. With `vr_relit_maps 0` the original maps load and water is opaque again.
- **Model lighting** (#2, `vr_model_lighting`, `vr_modellight.cpp`). The map's light entities (parsed at load;
  "start off" lights with a target name skipped) give each alias model the direction of the strongest four
  lights that reach it (Quake's linear falloff, `light - distance * wait`) and see it (a line through the world's
  BSP, hull 0, client-side). Weighted by brightness there; `w` is how much they agree. Recomputed when the model
  moves 12 units or every 0.3 s, eased over time. The alias shader gets it per instance (`aliasinstance_t.lightdir`,
  `InstanceData.LightDir`) and mixes it with the fixed direction by `w`.
- **Blob shadows for monsters and items** (#5, `vr_entity_shadows`): alias models in the scene except the player's,
  view entities, see-through ones and `r_noshadow_list` (flames, beams); sized from the model's bounds, pushed away
  from the model's light direction, fading with height. The traces are now client-side through the world and the
  moving brush models (lifts, doors), so shadows also work as a client; built once per frame for both eyes.
- **Muzzle flash light at the gun** (#4 in part): the local player's `EF_MUZZLEFLASH` light is at the main hand's
  muzzle (`VR_MuzzleFlashOrigin`). The extra monster and powerup lights wait for the dynamic-lights step.
- **Anti-aliasing** (#6 in part): `vid_fsaa 4` in `quakevr/default.cfg` (new configs), a setting in Advanced >
  Graphical Settings, and the eye framebuffers are rebuilt when it changes. Texture filtering stays Ironwail's.

## Sources

Engine audit: this repository (paths above). External: DarkPlaces documentation and technotes
(github.com/DarkPlacesEngine/darkplaces, icculus.org/twilight/darkplaces), FTE's BSPX spec (fteqw/specs/bspx.txt),
ericw-tools light documentation (ericw-tools.readthedocs.io), QuakeSpasm-Spiked (fte.triptohell.info/moodles/qss),
the Quake re-release QuakeC (id-Software/quake-rerelease-qc) and player reports, vkQuake and vkquake-rt READMEs,
QuakeQuest's source (Team-Beef-Studios/QuakeQuest), Alex Vlachos' Advanced VR Rendering talks (GDC 2015, 2016), VR
shadow-perception studies (TVCG 2021, arXiv 2201.01889), SSAO stereo issues (ACM 2022, Unity and Godot trackers),
Ironwail issues #329 and Hexenwail #78/#97.

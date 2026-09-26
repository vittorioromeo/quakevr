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

- **Re-lit maps** (#1). `Misc/quakevr/relight_maps.py --quake <Quake folder> [--light <ericw-tools light>]` extracts
  the maps of id1, hipnotic and rogue from the player's own paks and re-lights them with **ericw-tools 2.0.0-alpha11**
  (since round 17; https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11, the win64 zip; GPL; v0.18.1
  before, which still works, without the light grid). `--light` defaults to `ERICW_LIGHT`, then `light` on `PATH`,
  then `relight_maps.DEFAULT_LIGHT` (`C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64/light.exe`). The look's options
  are `-extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -lit`, stronger ambient occlusion and no bounced light since round
  10 (`--bright`: the earlier look with `-bounce -bouncescale 0.5`); `OUTPUT_ARGS` adds `-lux` (the light's direction
  at every luxel, `<map>.lux` beside the `.lit`: deluxemaps, `vr_deluxemap`, LIGHTING.md) and `-lightgrid` (the
  light in the air every 32 units, the `LIGHTGRID_OCTREE` BSPX lump in the `.bsp`, about 0.3 MB a map). About 1 s a
  map, 73 maps, 209 MB (143 MB before the `.lux` and the grid), into `quakevr/relit/<game>/maps/`. The 2.0 tool
  gives the same light with the same options: see "Relight tools" below. The engine loads `relit/<game>/maps/X.bsp` and its `.lit` in place of
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
- **Light fixtures** (round 15, `glow_lights` in `relight_maps.py`, `Misc/quakevr/relight_textures.cfg`). Lamps,
  light panels and strip lights (textures named `*light*`/`*lamp*` and those the file names: `tlight*`, `light1_*`,
  `light3_*`, `ceil1_1`, `sfloor4_4`, rogue's `metal8_3`/`metal9_2`) each get a light of their own: one point light
  for a small fixture (in front of it, or on top of a lantern), one every 128 units for a big one; 250 for a lone
  fixture, half by a mapper's light of 300 that is on from the start and less reduced by a weaker one (a "start off"
  light, like those of e1m1's rising lanterns, does not count), less for a small or faintly glowing one, divided by
  the square root of the fixtures in its room; without ambient occlusion (`"_dirt" "-1"`: `-dirt` left a lamp in a
  recess a fifth of its light). A lamp in a recess or slot (solid past two opposite edges of its face: e1m6's
  coffered ceiling lamps) has its light stepped out past the recess walls, up to 48 units (`recess_depth`). Before, fixtures shared half a glow budget over the whole map, so none got a light (e1m1's `tlight11`
  lanterns: 222 x 0.5 / 67 faces = 1.6, under the cut-off of 12): they looked lit and lit nothing. What glows also
  comes from replacement textures: where a texture has no fullbright pixels, its `_luma` image (QRP's
  `textures/<name>_luma.tga`, looked up like the engine: `textures/<map>/`, `textures/`, the game then id1) decides,
  so `light1_*`, `tlight05`/`09`/`10`, shootable and floor buttons and stained-glass windows glow too
  (`--no-luma`: the 8-bit pixels only). The file raises `light1_4` and `ceil1_1` (x2). Tuning: `--fixture-scale` (1), `--fixture-lit` (0.5), `--glow-scale` (both
  kinds), and per texture or map in `relight_textures.cfg` (`<[game/][map/]pattern> kind=fixture|glow|off scale= light=
  color=r,g,b reach=`, documented in the file); `relight_maps.py --quake <Quake> --list-glows [--only e1m1]` lists
  each map's glowing textures, where their glow comes from, their kind and lights, without relighting. Rerun the
  relight after a change (only the maps whose lights changed are relit) and reload the map. In game the fixtures'
  own glow is bloom: `vr_bloom_white` (Graphics, "Bloom: White Lights") for white and pale lamps, `vr_bloom_color`
  for coloured ones; baked light itself cannot change without relighting.
- **See-through water.** id's maps were vised with liquids as walls, so the engine keeps their water, slime and
  teleporters opaque whatever `r_wateralpha` says (changing it prints "Map does not appear to be water-vised").
  The relit maps get water-vised visibility from the VisPatch data files (`id1.vis`, `hipnotic.vis`, `rogue.vis`,
  or `<game>/vispatch.dat`; vispatch.sourceforge.net): give their folder to the relight,
  `relight_maps.py ... --vis-dir <folder>` (or set `QUAKEVR_VISPATCH`), which patches every relit map, also ones
  already up to date (once: a map patched already is left as it is, its lumps packed). `Misc/quakevr/vis_maps.py --vis-dir <folder> [--relit quakevr/relit]` does it on its own
  (only the visibility and leaf lumps change; a patch whose leaves are not the map's is refused);
  `vis_maps.py --check quakevr/relit/id1/maps` reports, and in game `developer 2; map e1m1` prints
  "maps/e1m1.bsp is vised for transparent water tele slime". `quakevr.cfg` sets `r_lavaalpha 1` (lava stays
  opaque); `r_wateralpha` sets the rest. With `vr_relit_maps 0` the original maps load and water is opaque again.
- **Relight tools: ericw-tools 2.0.0-alpha11** (round 17; https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11,
  `ericw-tools-2.0.0-alpha11-win64.zip`: `light.exe` beside its DLLs). Why: `-lux` (deluxemaps, LIGHTING.md), the
  `-lightgrid` BSPX lump, and the maintained code. The options are the same as with v0.18.1
  (`-extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -lit`, plus `OUTPUT_ARGS` `-lux -lightgrid`); none of the defaults
  that changed matters here: 2.0's `-extra4` is `-extra 4`, `-dirt` also darkens minlight (none used), the sky's
  `-sunsamples` default is 64, `-visapprox` culls lights by the vis data (`none` gave the same luxels). Compared luxel
  by luxel on all 73 maps with v0.18.1's: the median correlation 0.996, the mean brightness within 1.3% on every map
  (e1m1 +0.2%), screenshots of e1m1, e1m2, e1m6, e2m1 and start at the fixture views differing by 0.01 to 0.3 of 255
  on average. What differs, and what the scripts do about it:
  - A light with `"light" "0"` is dark in 2.0; id's light and v0.18 gave it the default 300. e1m4's six torches went
    dark (its luxels 5% darker on the whole): `relight_maps.id_light_values` gives such lights 300 again (for `light`
    only; the map keeps its entities).
  - 2.0 places the samples of a face strip thinner than a luxel on its edges; where a brush stands at that edge, its
    face shades them, and the strip comes out half as bright or black (0.02% of id's maps' luxels, more in e1m7 and
    the mission packs, all small; brighter luxels elsewhere as often: 2.0 also fixed samples 0.18 put inside
    solids). vrtutorial is built of 4- to 8-unit trims (the wall between each board and the strip light over it):
    0.6% of its luxels, a dark band over every board. So `relight_quakevr_maps.py` lights vrtutorial with v0.18.1
    (`--legacy-light`, default the old path, or `ERICW_LIGHT_LEGACY`) and takes 2.0's light grid and its `.lux`, the
    latter moved face by face to the old lightmap's layout (`relight_maps.remapped_lux`); without the old tool it
    warns and uses 2.0 alone.
  - The `.bsp` gets BSPX lumps after its 15 lumps; engines find them only right after the last lump, so replacing
    the entities or the visibility by appending (as the scripts did) would lose them: `vis_maps.packed` repacks
    every lump and keeps the BSPX ones after them (`with_entities`, the water-vis patch).
  - Each map's stamp (`<map>.relit`) holds `light`'s version (`light_version`, from its banner) and a `REVISION` of
    the script: the new tool relit every map once.
  - `relight_quakevr_maps.py` uses a 64-unit light grid (`-lightgrid_dist 64 64 64`): the maps are committed, and the
    firing range's open air made a 1.6 MB grid at 32 (0.2 MB at 64). vrtutorial.bsp 0.97 -> 1.08 MB, vrfiringrange.bsp
    1.59 -> 1.78 MB, plus their `.lux` (0.24 and 0.20 MB).
  No source patch was needed (nothing in `Misc/quakevr/tools/`). Sizes: `quakevr/relit` 143 -> 209 MB (the `.lux`
  files as large as the `.lit`s, the grids about 0.3 MB a map); about 1 s a map.
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

# Real-time shadows and dynamic lights

The second step of [GRAPHICS.md](GRAPHICS.md): shadows from dynamic lights, shadows of moving things from the map's
lights, and dynamic lights on models per pixel. Every piece can be turned off or tuned (Advanced > Graphical
Settings > Lights and Shadows, or the cvars below), with presets from "Off (Quake)" to "Ultra".

## Research

### Other Quake engines (from their source)

| Engine | Dynamic lights | Shadows | Notes |
|---|---|---|---|
| **DarkPlaces** | Real-time lights: the scene is redrawn once per light, additively. Dynamic lights never touch lightmaps. | Shadow maps only (stencil volumes were removed in 2018). One 2D atlas (8192²); each point light takes 2×3 cube faces. Face size is `radius × precision / √(distance / radius)`, clamped 32–512. A 5-texel guard band per face. When the atlas is full, every light is halved and repacked. Faces are rendered only if they have casters and receivers and can reach the view. Bias 0.03 plus a slope factor of 2. Filters are 1–4×4 PCF through hardware compare. | Redraws shadow maps for every view (water, each eye). Its falloff is not Quake's. |
| **FTE** | Real-time lights, per pixel. It turns off lightmap dlights on the world to avoid counting light twice. | Shadow maps: one reused 3×2 texture per light, faces up to 512, the DarkPlaces LOD. The world's shadow mesh is cached per static light. 1, 5 or 9 PCF taps. | Also reads the BSPX light grid for model lighting. |
| **QuakeSpasm / QSS** | Lightmap dlights. | `r_shadows`: the model flattened onto the floor, drawn in black. | No real-time lights. |
| **Quake 2021 re-release (KEX)** | More dlights (quad, pentagram, projectiles). | Shadow atlas (`r_showshadowatlas`). Mapper-placed shadow lights cast the shadows of monsters and players (`dynamiclight` entities in MG1; Q2's `shadowlight*` keys: radius, resolution, intensity, fade distances). | Static and dynamic shadows are separate settings. |
| **Quetoo** (Quake 2) | Per pixel, up to 512. | Atlas tiles per light. Each tile is redrawn only when a hash of the light and its casters changes. Poisson PCF with a normal offset. | The closest match to this design. |
| **Hexenwail** (an Ironwail fork) | Froxel lights on world and models. | Planned: the top N lights, 64–1024 px. | |
| **Ironwail** | A GPU-clustered grid (32×16×32, up to 64 lights, one bitmask per cluster), lightmap-style falloff on the world, flat on models. | None. | The base of this work. |

Xonotic's effect presets turn on dynamic lights at "medium", dynamic-light shadows only at "ultra", and world-light
shadows only at "ultimate". DarkPlaces' own menu has Flares / Normal / High / Full. The settings that matter most
are: dynamic lights on or off, their shadows on or off, and world lights.

### VR rendering

- **Valve, "The Lab" (GDC 2016):** forward rendering with MSAA, up to 18 shadowing lights, one shadow atlas shared
  by both eyes. Point lights are six square tiles. 3×3 Gaussian PCF, with an early out when the four corner taps
  agree.
- **id Tech 6/7 (Doom 2016, Eternal):** one shadow atlas; tile size set by importance and distance. Static casters
  are cached, and only moving casters are redrawn over them. 3×3 PCF.
- **Filtering:** hardware PCF, 3×3 to 5×5, is the practical choice. VSM and EVSM need blur passes and leak light.
  Shimmer in VR comes mostly from camera-dependent cascades, which point lights don't have. The risks for point
  lights are resolution pops (use tiers and hysteresis) and low-resolution moving lights (soften).
- **Both eyes must see the same thing.** Effects that differ between the eyes (SSAO, SSR, screen-seeded noise)
  cause binocular rivalry. Shadow maps and clustered lights are in world space; use fixed filter kernels.
- **Work once per frame, not per eye:** choosing lights, packing the atlas, rendering the shadows.
- **Popping:** lights that gain or lose a shadow should fade, with hysteresis on the choice.
- **Other wins for later:** the hidden-area mesh (`XR_KHR_visibility_mask`, about 17% fewer pixels), adaptive
  render scale instead of switching effects, 4× MSAA (not TAA), fixed foveation (NV_shading_rate_image).
  Multiview matters little for a Quake engine.
- **Shipped games** expose shadow quality, MSAA and render scale. Team Beef's Doom 3 Quest makes shadows optional.

Sources are in the three research reports summarised here: the DarkPlaces, FTE, Quetoo, yquake2, QuakeQuest and
id re-release sources; Vlachos' GDC 2015/2016 talks and `the_lab_renderer`; Courrèges' and Coenen's Doom
studies; MJP's "A Sampling of Shadow Techniques"; Olsson's clustered shading papers; Meta's display and Unreal
renderer notes; Unity HDRP/URP docs; Ironwail issue #329; Hexenwail issues #78 and #97.

## Design

- **One shadow atlas** (`vr_shadow_atlas`, 4096²; D32F, reversed depth `1 / distance`, hardware compare). A shadowed
  light takes six square faces in a 3×2 block, each with a 4-texel border (the face projection is widened to
  match), so filtering never reads the next face. The block goes in its light's `gpulight_t.shadow` (origin and
  size); the world and model shaders read it in the clustered light loop they already have. Everything is once
  per frame, shared by both eyes (`VR_RenderShadowMaps`, in `R_SetupView` guarded by the host frame).
- **Shadowed dynamic lights** (`vr_shadow_dlights`, 4):
  - The chosen lights are those with the largest `radius / distance`, with a 25% bonus for lights chosen the
    frame before (hysteresis).
  - Your own muzzle flashes are excluded (`vr_shadow_muzzleflash`), as is anything beyond `vr_shadow_distance`.
  - Face size uses DarkPlaces' formula, rounded down to a power of two between 64 and `vr_shadow_dlight_size`.
    The previous size is kept unless the new one is far off.
  - Casters are:
    - the world's surfaces within the radius (a BSP walk);
    - doors, lifts and platforms (`glvert_t` positions from Ironwail's brush vertex buffer, one small depth-only
      shader);
    - monsters and items, through Ironwail's own alias renderer with the face's view-projection and frustum.
  - The light's own entity (a rocket) and your hands and gun don't cast. Faces whose cone can't reach the view
    are skipped.
  - When the atlas is full, everything is halved and repacked, as in DarkPlaces.
- **Map lights' shadows of moving things** (`vr_shadow_maplights`, 2). The light entities near you (reaching near
  your eye, in your PVS, brightest there first, with hysteresis) each keep a slot with two depth maps:
  - the **world's** depth, drawn once into `staticAtlas` and cached;
  - the **moving things'** depth (monsters, items, and you: `vr_shadow_self`), drawn each frame, and only when
    something is near.

  The world shader removes the light's share of the baked light only where a moving thing blocks the light and the
  world does not. The share is estimated as `(light − distance × wait) × (0.5 + 0.5 × N·L)`, the same formula the
  map compiler uses. Without the second map, shadows behind walls would darken light the lightmap never had.
  Shadows fade in and out over a quarter of a second. `vr_shadow_maplight_strength` sets how dark they get.
- **Dynamic lights on models per pixel** (`vr_dlight_models`):
  - The alias shader walks the same clusters, with world normals and an angle term, and shadows the light.
  - `R_SetupAliasLighting`'s flat brightening is skipped.
  - The units follow Quake's: `(radius − distance) / 200`, doubled for overbright models.
- **Dynamic lights on the world:**
  - Quake's formula is kept.
  - An angle term (`vr_dlight_angle`) and the shadow are added.
  - `vr_dlight_uncapped` lifts Quake's cap against the lightmap (bright walls get more light).
- **Filtering** (`vr_shadow_filter`): 1, 4, 9 or 16 bilinear compare taps (about 2×2, 3×3, 4×4 and 5×5). The
  kernels are fixed, so both eyes match. **Bias:**
  - a normal offset of a texel, doubled at grazing angles (`vr_shadow_bias`);
  - a 0.2% depth scale.

## Settings

| Cvar | Default | |
|---|---|---|
| `vr_graphics_preset` | (not saved) | 0 Off (Quake), 1 Low, 2 Medium, 3 High, 4 Ultra: sets the others |
| `vr_shadow_dlights` | 4 | dynamic lights casting shadows (0 off) |
| `vr_shadow_dlight_size` | 512 | their largest face, texels |
| `vr_shadow_precision` | 1 | texels per unit of radius close by |
| `vr_shadow_muzzleflash` | 0 | your muzzle flashes cast shadows |
| `vr_shadow_maplights` | 2 | map lights casting the shadows of moving things (0 off) |
| `vr_shadow_maplight_size` | 512 | their face size |
| `vr_shadow_maplight_strength` | 0.7 | how dark those shadows get |
| `vr_shadow_self` | 2 | your shadow from map lights: 0 none, 1 body, 2 body and hands |
| `vr_shadow_filter` | 1 | 0 hard … 3 softest |
| `vr_shadow_bias` | 1 | acne vs. peter-panning |
| `vr_shadow_distance` | 1536 | lights farther away cast none |
| `vr_shadow_atlas` | 4096 | 2048 / 4096 / 8192 (16 / 64 / 256 MB) |
| `vr_dlight_models` | 1 | dynamic lights on models per pixel |
| `vr_dlight_angle` | 1 | angle falloff of dynamic lights (0: Quake's) |
| `vr_dlight_uncapped` | 0 | dynamic lights add fully to bright walls |
| `vr_shadow_stats` | 0 | prints lights, faces, model draws, GPU and CPU time each second |
| `vr_light_test [radius] [seconds] [distance]` | command | a dynamic light in front of you |

| Preset | Dyn. shadows | Face | Map-light shadows | Face | Filter | Atlas | Distance | Model dlights, angle, model lighting, blob shadows |
|---|---|---|---|---|---|---|---|---|
| Off (Quake) | 0 | – | 0 | – | – | – | – | off |
| Low | 2 | 256 | 0 | – | 4 taps | 4096 | 1024 | on |
| Medium | 4 | 512 | 2 | 512 | 4 taps | 4096 | 1536 | on |
| High | 6 | 512 | 4 | 512 | 9 taps | 4096 | 2048 | on |
| Ultra | 8 | 1024 | 4 | 1024 | 16 taps | 8192 | 3072 | on |

The defaults are Medium.

## Cost

Measured with `vr_shadow_stats` on the development PC (e1m1, mock headset, Ultra, three test lights plus four map
lights: 37 faces, about 300 model draws):
- the shadow pass takes **0.04 ms of GPU** and **0.1 ms of CPU** per frame;
- a map light's first frame, which fills its world cache, takes about 1.7 ms of CPU.

The per-pixel cost is in the world and model shaders: a few texture taps per shadowed light, per pixel it reaches.
Quake's scenes are small; the fill rate of two 2064×2208 eyes is what to watch, so compare `vr_shadow_filter` and
the light counts with your headset's frame timing.

## Engine side

- `gl_shaders.h`:
  - `SHADOW_FUNCTIONS` (lookups, filtering, bias, map-light shadow) and `ALIAS_FRAMEDATA_BUFFER`;
  - the world light loop;
  - the alias vertex outputs (normal, cluster coordinates) and fragment lighting;
  - `Light` gets `shadow` and `shadow2`, and the frame data `ShadowBias`, `DlightAngle`, `ShadowFlags`.
- `glquake.h`: the same in `gpulight_t` and `gpuframedata_t` (its padding).
- `gl_rmain.c`: `VR_RenderShadowMaps` before `R_PushDlights`.
- `gl_rlight.c`: `VR_DlightShadow` for each light and `VR_PushMapLights` after them.
- `r_alias.c`: `VR_ModelDlightsPerPixel` skips the flat add, and map-light entries are skipped there.
- `Quake/vr/vr_lighting.cpp`: all the rest. It is renderer-specific (OpenGL, Ironwail's buffers); a vkQuake port
  rewrites it and the shader parts (see [PORTING.md](PORTING.md)).

## Next

- **Caching dynamic-light faces** whose light and casters didn't move (Quetoo's hash), and rendering only faces with
  receivers and casters (DarkPlaces' side masks).
- **Map-light shadows on models,** so a monster standing in another's shadow gets darker.
- **The hidden-area mesh and adaptive render scale** (see "VR rendering" above).
- **Mapper-placed shadow lights**, as in KEX: `_shadowlight` keys on light entities.
- **BSPX light grid and direction** for model lighting (GRAPHICS.md #9).

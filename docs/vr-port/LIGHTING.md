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
| **q2pro-ng** (Quake 2) | 64 per-pixel lights for world and models. | A D32F atlas with a quadtree allocator; a static atlas caches world-only depth, copied under the moving casters each frame; 4-tap PCF; `glPolygonOffset`. | The closest existing design; ours keeps the two maps separate so the shader can tell world from moving occlusion. |
| **vkQuake** | Dynamic lights on the GPU into the lightmap. | `r_rtshadows`: ray queries in the lightmap compute shader (Vulkan RT only), shadows at lightmap resolution. | Its author prefers soft, lightmap-resolution shadows for Quake's look. |
| **RBDOOM-3-BFG** | | An 8192 atlas; LOD from projected radius (`r_shadowMapLodScale`); Vogel-disk PCF with 1�16 samples; point faces at 92�. | |
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
  light takes six square faces in a 3×2 block (a spot light, such as the flashlight, one tile round its cone), each
  with a 4-texel border (the face projection is widened to match), so filtering never reads the next face. The block goes in its light's `gpulight_t.shadow` (origin and
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
- `r_alias.c`: `VR_ModelDlightsPerPixel` skips the flat add, and map-light entries are skipped there; a spot light's
  flat add is times its cone (`VR_SpotCone`).
- Spot lights: `Light.spot` / `gpulight_t.spot`, `SpotCone` and `SpotShadow` in `SHADOW_FUNCTIONS`, the cone's
  bounding sphere in the light clustering compute shader (see "The chest flashlight").
- `Quake/vr/vr_lighting.cpp`: all the rest. It is renderer-specific (OpenGL, Ironwail's buffers); a vkQuake port
  rewrites it and the shader parts (see [PORTING.md](PORTING.md)).

## The look (round 7)

The shadows and lights worked but the maps still looked flat: Quake's lightmaps light rooms evenly,
dynamic lights are capped against bright walls, and nothing glows. Four changes, all in Graphics:

- **Light contrast** (`vr_light_contrast`, 2). The static lightmap is raised to a power about Quake's full
  light (a lightmap value of a half, before the doubling) in the world shader. Shade gets darker, well-lit walls
  stay as they were, and the brightest spots get a little brighter, so the level's own lamps carry the scene.
  Models get the same curve on the light at their feet (`VR_AliasLightCurve`, before the minimum light). 1 is
  Quake's look.
- **Shooting lights up rooms.** A muzzle flash's light is `vr_flash_scale` (1.8; 1 since round 10) times Quake's size. It fades out
  over a tenth of a second rather than blinking off, and is coloured by the weapon: warm for shotguns, orange for
  nailguns, red-orange for rockets, blue for the lightning gun. Explosions and rockets are
  `vr_explosion_light_scale` (1.5; 1 since round 10) times bigger and warm (`vr_colored_lights`). Dynamic lights are now uncapped
  by default (`vr_dlight_uncapped` 1): Quake's cap hid them on anything already lit. All of this is set by one
  hook, `VR_TuneDlight`, called where Quake sets these lights up.
- **Bloom** (`vr_bloom` 0.8, `vr_bloom_threshold` 0.6, `vr_bloom_radius`; `vr_bloom.cpp`), per eye, after the
  scene and before post-processing:
  1. a bright pass at a quarter of the eye's size (the brightest of four taps, with a response that rises
     steeply towards white, since the scene is low dynamic range: a lamp is 1, not 10);
  2. two further halvings;
  3. a separable 9-tap Gaussian on each of the three levels (five linear taps);
  4. all three levels added onto the scene.

  Lamps, glowing buttons and panels, flashes and explosions glow. It costs a few passes at a sixteenth of the
  pixels or less.
- **Glowing textures light their surroundings** (the relight, `Misc/quakevr/relight_maps.py`, on by default;
  `--no-glow`, `--glow-scale`):
  - Textures with at least 3% fullbright pixels (palette 224-254) light in the average colour of those pixels,
    a quarter of the way to white for white and pale ones, a tenth for strongly coloured ones (red buttons, blue
    panels), which also get up to twice the light, a cap of 169 instead of 130 and twice the reach (`wait` 0.5).
  - Each texture has a budget of light, larger the more of it glows, shared by the glowing things in the room:
    faces closer than 64 units are one thing (a button, a slipgate's frame), counting as their area in lights
    (one every 128 x 128 units) or the square root of their faces; the things of every glowing texture within
    256 units add up. A button alone in its room lights it (round 11: e1m1's red buttons had 15, as their
    texture's budget was shared by every button of the map; now 169, the room around tinted red); a slipgate's
    many faces do not flood their room. (A first version without the budget turned e1m1's arrival room red.)
  - Textures whose faces are all small (at most 2 x 128²: buttons, panels, signs, runes) get a point light 2 units
    in front of each face not in a wall, as bright as its own room allows; others get ericw's surface lights
    (`_surface` light entities: one template a texture, so as bright as its most crowded room allows).
  - Fixtures named `*light*` get half, shared by all their lights in the map, since mappers put lights by them.
  - The entities are given to `light` only: the relit map keeps its own, so neither the game nor the model
    lighting sees them. They are part of each map's stamp, so a change to how they are made relights the maps.

The presets set all of this: "Off (Quake)" restores Quake's contrast, no bloom, Quake's white capped flashes;
the others the new look.

## DarkPlaces look (round 10)

DarkPlaces looked moodier than this port. From its source: it never brightens (gamma and contrast 1, no bounced
light); models are lit flat at the floor's brightness; its dynamic lights are strong, coloured and fall off smoothly;
textures are filtered smoothly; and `r_shadow_gloss 2` gives dynamic lights a faint sheen. The same, each switchable
(Graphics menu):

- **Neutral headset output.** The eyes use their own `vr_gamma` and `vr_contrast` (1), not the desktop's `gamma` and
  `contrast` (often raised for a monitor: 0.95 and 1.2 in the author's config), which the desktop window keeps. The
  OpenXR backend logs the swapchain format once and warns if it isn't sRGB (the compositor would then show the image
  paler and brighter). The palettized software-emulation modes still use the desktop's values.
- **Moodier relight** (`relight_maps.py`): no bounced light, ambient occlusion 1.5 (was 1.0), glowing textures half
  the light (`--glow-budget 300`, was 600). `--bright` gives the earlier look. The relight was rerun for every map.
- **Smooth replacement textures** (`vr_texture_smooth` 1): images from `textures/` (QRP's 512² ones) and normal maps
  are linear with trilinear mipmaps; Quake's own 8-bit textures keep `gl_texturemode` (2: all textures smooth).
- **Models lit as the world** (`vr_model_light_parity`): the light at a model's feet over 128 (the world's full light),
  shaded 0.6 .. 1.4 by the normal (on average the light given), instead of over 200 with MH's 0.7 .. 2 and the 96 cap:
  models were 1.5 to 2.6 times as bright as the floor under them. Hands and weapons keep at least
  `vr_viewmodel_minlight` (8; Quake's 24). `vr_light_contrast`'s curve still applies first.
- **DarkPlaces' dynamic lights** (`vr_dlight_falloff` 1; 0 is Quake's linear falloff):
  - falloff `clamp((1 - d) × 2 / (1 + d²), 0, 1)`, d = distance / radius: full light to about 40% of the radius, then
    smoothly to none, by the 3D distance, never capped, Lambert's angle term (`vr_dlight_angle`);
  - colours brighter than 1 (`VR_TuneDlight`): muzzle flash radius 150, white 4, fading out over 0.05 s; rocket 200,
    (3, 1.5, 0.5); explosion 350 shrinking 700/s over 0.5 s, (4, 2, 0.5), a quarter of it reaching what faces away.
    `vr_colored_lights` 0 makes them white as bright; `vr_flash_scale` and `vr_explosion_light_scale` (now 1) scale
    DarkPlaces' sizes. Lights not tuned (EF_DIMLIGHT, the powerups' glows, `vr_light_test`) get DarkPlaces' 1.5, 3 from a
    radius of 400 (EF_BRIGHTLIGHT).
  - `VR_DlightShadow` hands the shaders the per-light part (the ambient share in `minlight`, the fading colour).
- **Sheen** (`vr_specular` 0.125): Blinn, exponent 32, white, from dynamic lights only, towards each eye's own
  position, on the world and models, shadowed like the light.
- **Normal maps made at load** (`vr_normalmaps` 1, `vr_normalmap_strength` 1, `vr_normalmap_baked` 1): for world
  textures and model skins, from the texture's luminance taken as height (a Sobel gradient; DarkPlaces'
  `r_shadow_bumpscale_basetexture`), 4 units deep from black to white (2 before round 11) for a texture of average
  brightness 0.35, relative to its own (a dark texture up to twice as deep, a bright one down to 0.75); an authored
  `<image>_norm` beside a replacement image is used as it is, and a `<image>_bump` height map instead of the texture.
  The surface's frame is a cotangent frame from the derivatives of the position and texture coordinates (exact on the
  world's flat faces, the same in both eyes, no vertex data). World calls carry the map's bindless handle (or unit 3),
  alias skins use unit 2. Made maps are at most 256² and 2 texels a world unit (a QRP 512² texture on a 64-unit wall
  gets 128²: its finer grain looked like noise as bumps), stored RG8 (the shader makes z). New maps get them as they
  load (the setting applies next map; model skins already loaded keep what they have).
  - **On the baked light too** (round 11, `vr_normalmap_baked`, 0..2, "Bumps in Map Light"; the author saw walls flat
    where no dynamic light was): a fake deluxe map. The world shader guesses where the baked light comes from: towards
    where the lightmap gets brighter over the surface (the screen derivatives of its brightness, made a gradient per
    unit with the same cotangent frame, times 96 over the brightness) and a little from above, at most 45° off the
    normal; the light is shaded `dot(bumped, l) / dot(n, l)`, so the flat is as bright as before and bumps facing the
    light are brighter, those facing away darker. Alpha-tested surfaces have no gradient (after the discard), liquids
    none of it. DarkPlaces' `r_glsl_deluxemapping 2` is the case of light straight on (floors and ceilings under flat
    light). Models: see "On models' own light" below.
  - Round 11 fix: a replacement (RGBA) image is mipmapped in place by its upload, so the normal map was made from a
    scrambled copy (the upper part of the image overwritten by its smaller mips); it is now made from a copy taken
    before.
  - They bend the dynamic lights' angle and sheen as well.
  - **On models' own light** (round 14, `vr_normalmap_models` 1, "Bumps on Models"; times `vr_normalmap_baked`):
    the alias shader shades the skin's bumps by the model's light direction (`vr_modellight`'s, or Quake's fixed
    one): `1 + strength x lean`, the lean being the bent normal's part along the surface towards the light (it
    averages out over a skin, so the model is as bright as before), less on the side facing away. Held weapons and
    hands get half (`VR_ModelBumps`). The instance's `Glow.y` carries it (sign: `vr_model_light_parity`). Skins'
    luminance is grown out of their islands first (`TexMgr_DilateIslands`), so seams make no ridges.
- **Bloom by colour** (`vr_bloom_white` 0.5, `vr_bloom_color` 1.5, times `vr_bloom`): the bright pass weighs a pixel
  by its saturation, so red buttons and blue panels glow more, white lamps and flashes less.

| Cvar | Default | Off (Quake) |
|---|---|---|
| `vr_gamma`, `vr_contrast` | 1, 1 | (not in presets) |
| `vr_texture_smooth` | 1 | 0 |
| `vr_model_light_parity` | 1 | 0 |
| `vr_viewmodel_minlight` | 8 | 24 |
| `vr_dlight_falloff` | 1 | 0 |
| `vr_specular` | 0.125 | 0 |
| `vr_normalmaps` | 1 (0 on Low) | 0 |
| `vr_normalmap_strength` | 1 | (not in presets) |
| `vr_normalmap_baked` | 1 | (not in presets; nothing without `vr_normalmaps`) |
| `vr_parallax` (see Parallax below) | 1 (0 on Low) | 0 |
| `vr_parallax_depth`, `vr_parallax_distance`, `vr_parallax_steps`, `vr_parallax_items`, `vr_parallax_models` | 3, 512, 16, 1.5, 0 (0.75 before round 14) | (not in presets) |
| `vr_bloom_white`, `vr_bloom_color` | 0.5, 1.5 | (bloom off) |
| `vr_flash_scale`, `vr_explosion_light_scale` | 1, 1 (were 1.8, 1.5; a config holding those takes 1) | 1, 1 |
| `vr_projectile_lights`, `vr_weapon_screen_light`, `vr_weapon_glow` | 1, 1, 1 | 0, 0, 0 |

**Glows** (`vr/vr_emissive.cpp`; small, unshadowed: `lighting::dlightNoShadow` keeps them out of the shadow slots):
- `vr_projectile_lights` (size): monsters' glowing projectiles carry a light where they are drawn (`VR_ProjectileLight`
  in `CL_RelinkEntities`), by trail flag: scrag spit (EF_TRACER) green (0.8, 1.8, 0.3) radius 150, hell knight flames
  (EF_TRACER2) orange (2, 1, 0.3) 150 flickering a little, vore balls (EF_TRACER3) DarkPlaces' purple (1.2, 0.5, 1) 200;
  the EF_DIMLIGHT lasers are tinted (the enforcer's yellow-orange, the laser cannon's red). A scrag's or hell knight's
  spike hitting a wall (TE_WIZSPIKE, TE_KNIGHTSPIKE) flashes its colour, 120, fading out over 0.25 s. With Quake's
  falloff the colours are scaled to at most 1; without `vr_colored_lights` they are white. Nails and grenades stay
  unlit (as in DarkPlaces); rockets and lava balls have the rocket light.
- `vr_weapon_screen_light` (strength): each weapon's ammo screen casts a small spot light (`lighting::dlightSpot`) the
  way it faces: 1 unit out from the screen, radius 28, cone 45/85 degrees, in the screen's colour
  (`vr_gadget_screen_hue`, `vr_gadget_screen_brightness`), lighting what is in front of the screen (the hand, the arm,
  a wall close by; per pixel with `vr_dlight_models`) and nothing beside or behind it (the gun's sides stay dark).
- `vr_gadget_light` (strength): the wrist gadget's screen lights mostly where it faces: a light 14 units out along the
  screen's facing, radius 36, and a faint one (radius 16) just in front of it for the hand and forearm
  (`gadget::setPose`, before each eye's scene; key -0x5C10).
- `vr_screen_glow` (strength): a soft glow round the gadget's and the ammo screens' edges (additive, in the
  translucent pass).
- `vr_weapon_glow` (strength): the held and holstered weapons' fullbright texels get
  `fullbright × (1 + 3 × strength × (1 − brightest channel))` in the alias shader (instance `Glow.z`; the fullbright
  is the fullbright texture's, or for Ironwail's ALPHABRIGHT skins the skin's unlit share, where its alpha is 0): the
  shotgun's dim red sights (palette 226–228) reach full red, so that the coloured bloom makes them glow; bright
  fullbrights (muzzle flashes) hardly change.

**Cost.** Per world pixel: one normal map tap, two more derivatives and about twenty instructions for the bumps on the
baked light (round 11); per model pixel, only where a dynamic light's cluster reaches: one normal map tap; a few
instructions per dynamic light (four derivatives are taken for every world and model pixel). Loading: a Sobel pass over
at most 256² per texture, and a look for `_norm`/`_bump` images beside each replacement. Memory: about 24 MB of normal
maps on e1m1 with QRP before round 11, less since (at most 2 texels a unit).

Not done: DarkPlaces' quad-damage explosion colour (the client can't tell).

## The chest flashlight

`vr_flashlight.cpp` (`vr_flashlight`, Body page): a right-angle torch (`progs/vrflashlight.mdl`,
`Misc/quakevr/make_flashlight.py`) clipped to the chest on the off hand's side, lighting where the torso faces
(`vr_flashlight_tilt` degrees lower). Trigger with a hand at it switches it (a click, `sound/vr/flashlight_*.wav`, and a
tap); grip with an empty hand takes it, held through the fist by its body (`vr_flashlight_hand_forward` and `_up`,
metres from the tracked hand: back and down into the drawn fist's grip), and it lights where the hand points; let go,
it flies home on its cord (critically damped, about 0.4 s). The keys it takes never reach the game, so the holding hand
grabs nothing else.
**A spot light** (after round 12; the author: models in the beam stayed dark, lit only behind them, and sweeping the
beam over stairs flickered between fully lit and unlit, as the light was placed where one trace from the lens landed).
Now it is lit as games light torches, with no traces:
- One **spot light** at the lens (`lighting::dlightSpot`): range `vr_flashlight_range`, full within 10 degrees of the
  axis and smoothly down to none at 22 (`1 - smoothstep` of the cosine), DarkPlaces' falloff with the distance, per pixel
  on the world and on models (pickups, monsters, hands and guns in front of the lamp) with the usual angle term, sheen
  and bumps.
- **Its shadow** (`vr_flashlight_shadows`, on by default): one square tile in the shadow atlas, a perspective
  projection round the cone (tangent of 22 degrees and 6% more), twice a cube face's size (1024 at Medium, at most
  2048): a third of a point light's texels at four times its angular resolution. Its casters are those within the
  cone's bounding sphere (the world by the BSP, doors and lifts, monsters and items; not your own body, hands or
  gun). The lamp is at your eye, so it always ranks first among the shadowed lights.
- A faint **spill** (a tenth, half the range, out to 45 degrees, unshadowed), as a torch's reflector gives round the
  hotspot, and a faint glow just in front of the lamp (half a metre).

**In the renderer** (any dynamic light can be a spot): `gpulight_t` / GLSL `Light` gained `spot` (xyz the direction
over `cos inner - cos outer`, w `cos inner` over the same; zero for a point light, so every other light is exactly as
before). `LightShadow` in `SHADOW_FUNCTIONS` returns the cone times the shadow, so the world's (both falloffs) and the
models' light loops apply it with no other change: one `inversesqrt`, a dot product and a smoothstep per light. A spot
with a shadow has `shadow2.x`, the tangent of its tile's half angle; `SpotShadow` projects onto its one tile (its right
and up from the direction by a fixed rule, `spotFrame` on the CPU) with the same filter (`ShadowFilter`, taken out of
`ShadowLookup`) and normal-offset bias. The light clustering compute shader culls a spot by the smallest sphere round
its cone (`R / (2 cos outer)` for cones up to 45 degrees: 0.55 of the range for the flashlight), not the full range.
Models lit flat (`vr_dlight_models` 0) get the cone too (`VR_SpotCone` in `R_SetupAliasLighting`). The atlas packer
takes blocks of any shape (a point light's 3 x 2 faces, a spot's one tile), tallest first. With Quake's falloff
(`vr_dlight_falloff` 0: `(radius - distance) / 256` of the colour) the flashlight's colours are scaled by 200 / radius,
about as bright close by as with DarkPlaces'.

Cost (RTX 4090, mock eyes, `start` at the stairs, flashlight on / off): the shadow pass one tile instead of six faces
(11 / 10 tiles drawn in all), 0.01 ms of GPU and 0.04 ms of CPU for it; world+brush 0.10 / 0.08 ms an eye (the cone,
shadow taps and the extra light where it reaches). The visible beam (drawn as before) is most of what the flashlight
costs: translucent 0.14 / 0.02 ms for both eyes, as it covers much of the view.

Check in the headset: models in the beam (pickups, monsters, the other hand and gun) lit and shading with it; the
beam swept over stairs and doorways with no flicker; its shadows (a monster's on the wall behind it) steady; the
brightness (`vr_flashlight_brightness`), the cone's edge, and how the visible beam lines up with the lit cone.

The beam is visible in the air (`vr_flashlight_beam`, its strength, 0.35 by default; 0 none): two open cones from the
lens, the spot light's outer 22 degrees and its inner 10-degree core, 16 sides by 10 rings (closer together near the lens), out to
where the beam lands (at most 10 m; one trace, eased over about an eighth of a second, so it does not jump at an edge),
added onto each eye's scene after the translucent pass (`VR_DrawSceneTranslucent`),
depth-tested, writing no depth. Each ring's glow thins as the light spreads (1 / (1 + d / 0.5 m)) and fades out over the
last third; each eye shades every point by the square of the cosine between its view ray and the cone's surface (seen
face-on the surface stands for a long way through the lit air, edge-on for none), so it reads as a soft volume with no
edges, brighter along the middle where the core adds in, and nothing within 10-40 cm of the eye. Where a wall or the
floor cuts the cone (a short client-side line trace out from the axis for each point, once a frame), the point is pulled
in to the wall and dark there, fading in over the next 30% of the radius: no hard line where the cone meets the world.

## Next

- **Caching dynamic-light faces** whose light and casters didn't move (Quetoo's hash), and rendering only faces with
  receivers and casters (DarkPlaces' side masks).
- **Map-light shadows on models,** so a monster standing in another's shadow gets darker.
- **The hidden-area mesh and adaptive render scale** (see "VR rendering" above).
- **Mapper-placed shadow lights**, as in KEX: `_shadowlight` keys on light entities.
- **BSPX light grid and direction** for model lighting (GRAPHICS.md #9).

## Bloom, faster (after round 11)

The first profile put bloom at about 80% of the world's GPU time (0.047 ms an eye at 1024 x 1024). It was
eleven passes an eye: the bright pass, two halvings, a separable 9-tap blur on each of three levels, and an
add at the full size that also read 16 texels a pixel for vr_bloom_adapt. Now (`vr_bloom.cpp`), in the way
of Call of Duty: Advanced Warfare (Jimenez, SIGGRAPH 2014) and dual filtering (Bjorge, SIGGRAPH 2015): the
bright pass to a quarter, three halvings of five bilinear taps down to a thirty-second, and three
doublings back up to the quarter, each a 3 x 3 tent of the level below plus its own level by its weight
(0.5, 0.8, 1, 0.4 from the quarter down); how much of the view glows is one texel, made once an eye from the
smallest level. The glow is added in Ironwail's post-processing (GL_PostProcess, `VR_PostProcessBloom`: four
taps), so no pass runs at the full size; the window's mirror draws the eye with its glow the same way.
Measured: 0.03 ms an eye at 1024 x 1024 (mostly the fixed cost of the passes), about a quarter of the
texture reads per pixel, so the saving grows with the headset's larger eyes. The look is the same, with
slightly tighter halos. The glow now also lies over the HUD panel, as it's added after it.


## Parallax (after round 11)

The author found the bumps flat ("something more advanced than bump mapping that still reacts to light but gives the
impression of the geometry becoming more 3D"): parallax occlusion mapping (POM; relief mapping), on the world and
brush models (`vr_parallax`, Graphics page: Parallax, Parallax Depth, Parallax Distance; needs Bump Maps).

- **Heights** (`gl_texmgr.c`, `TexMgr_ShadingToHeights`): made with the normal map, from the same luminance ("darker
  is deeper"), blurred a little (1 2 1 across and down, twice when finer than a texel a unit: about a unit either
  side, so grain doesn't make spikes), then stretched to the texture's own range (2% of its texels at the bottom, 2%
  at the top; a range of at least 0.2, so an almost flat texture stays shallow): 1 the surface, 0 the deepest. They
  are the world's normal maps' alpha (RGBA8 instead of RG8: 7.5 MB instead of 3.8 on e1m1 with QRP; skins' were RG8,
  8.5 MB, and since round 13 carry heights too). An authored `_norm` gives its own alpha (DarkPlaces' convention; none: flat), a `_bump` its luminance.
  Quake's own 8-bit textures get heights only if drawn smooth (`vr_texture_smooth 2` or a linear `gl_texturemode`):
  drawn sharp, the shifts bend their square texels into curves. Made as a map loads (the next map, like the bumps).
- **The shader** (`ParallaxUV`, world shader, solid surfaces only: not fences or liquids): the ray from each eye's
  own position (stereo-correct) through the pixel, down into the height field `vr_parallax_depth` units deep (3). The
  texture's axes on the surface are the gradients of its coordinates from the screen derivatives (the bumps' frame;
  exact on flat faces, and on moved or rotated brush models). It walks the ray in 8 to `vr_parallax_steps` (16) steps,
  more at grazing angles, then two secant refinements. The diffuse, fullbright and normal map are read at the point
  found, with the surface's own mip level (the moved coordinates jump at occlusions); the lightmap is read where it
  was (shifting it too made no visible difference: a few units is a fraction of a 16-unit luxel, and it would put
  jumps into the baked light's direction guess). It fades out over the last quarter of `vr_parallax_distance` (512;
  beyond, no cost but the distance test) and from 70 to 83 degrees off the normal (where it swims and smears); the
  shift along the surface is at most 3 times the depth.
- **Presets:** off for "Off (Quake)" and Low, on for Medium, High and Ultra.
- **Cost** (RTX 4090, mock eyes 1024 x 1024; world+brush GPU time for both eyes, off / on): start's riveted wall at
  the note's spot 0.125 / 0.148 ms, its hall 0.096 / 0.121, the wall at a grazing angle 0.122 / 0.156; e1m1's start
  0.124 / 0.173 (0.207 with 32 steps). So about 0.02 to 0.05 ms a frame, 20 to 40% of the world pass; times 4.35 for
  the headset's eyes (2064 x 2208), about 0.1 to 0.2 ms a frame. Per world pixel within reach: 2 to 18 reads of the
  height (1 where it is at the top), and the textures read with explicit gradients.
- **Limits:** heights from luminance are a guess: dark details sink (rust, grooves, dark rivets, thin scratches become
  thin grooves) and bright ones stand, which is right for most of Quake's metal and stone but not for everything.
  The walls' outlines stay flat (no silhouettes), and at a face's edge the height field continues the texture's
  tiling, not the next face. No self-shadowing.

### Items and models (round 13)

The author's notes: the ammo boxes looked extreme and had "a black box around" them (the medkit less so), the
expansions' ammo had none, and weapons (held too), pickups and the player's body could have a subtle depth, each
with its own dial.

- **Why the ammo looked worse than the medkit:** the depth is in world units, and the boxes are drawn a quarter size
  (`vr_forcegrabbable_box_scale`): 3 units deep on an 8-unit box (37% of it; the rays shifted up to 3 times that at
  grazing angles, more than a whole face). The shells' and nails' sides show only the lower three quarters of their
  32 x 32 textures, whose top quarter is black and unused; the rays walked past the face's lower edge, wrapped into
  that black band (the deepest height too), and sank the sides into a black box. The medkits' textures are used
  whole, so they only looked too deep.
- **A depth per instance** (`VR_ParallaxDepth`, vr_lighting.cpp; the brush instance's spare float and the alias
  instance's `Glow.w`): the world `vr_parallax_depth`; the ammo and health boxes (`maps/b_*.bsp`)
  `vr_parallax_items` (1.5) in their own units, times the scale they are drawn at (0.375 units at a quarter size);
  other brush entities (doors, lifts) the world's times theirs; alias models `vr_parallax_models` (0.75) in their own
  units times their scale (the entity's, the networked one and the held weapons' own, not the vertex scale). 0 turns
  each off; all need `vr_parallax`. Graphics page: Parallax Items Depth, Parallax Models Depth.
- **The boxes' edges** (`Mod_ItemTextureClamp`, gl_model.c): for a `b_*` model, the part of each texture its faces
  show (from the vertices, per axis, moved by whole textures; none on an axis a face tiles), sent with the draw call
  (`uvclamp`, a vec4 in the call data); the shader shrinks the shift so that the ray stays inside it, a half texel
  from the edge: the relief flattens towards the face's edges instead of reading past them. The expansions' boxes
  (lava nails, multi rockets, plasma: 8-bit, no replacement textures) get heights even when drawn sharp: their depth
  is a texel or two.
- **Models:** the skins' normal maps carry heights too (RGBA8: 8.5 MB more). A skin's islands (the texels its
  triangles cover, the back's seam vertices half a skin right; `Mod_LoadSkinNormalMaps`) rise to the top over their
  outer 2 texels, so that a ray stops at a seam instead of reading the skin's other parts (without it the shotgun's
  edges got dark bands and a red streak from the sights at depth 1.5). A normal map remade by `vid_restart` has no
  rims. The alias shader walks the same `ParallaxUV` in the triangle's own frame (its derivatives), with half the
  world's steps (8 at most); the skin and fullbrights are read at the point found. Not in the affine
  (`r_softemu_mdl_warp`) mode.
- **Fewer steps:** everywhere, no more than 1.5 steps for each texel of the height field the ray crosses (4 at
  least), and nothing where the whole shift is under a third of a pixel: small boxes and models cross a few texels.
- **Round 14: models off by default** (`vr_parallax_models` 0; configs holding 0.75 take 0 once). On Quake's
  8-bit skins, drawn sharp, the shifts bend their square texels (the reason the world's 8-bit textures get no heights
  when drawn sharp), and luminance isn't the skins' shape: it looked like the skin swimming. The bumps on the models'
  own light give them relief instead. When on, it now fades out from 50 to 70 degrees off the triangle (a model's
  sides are grazing all round). See ROUND14.md, "Model bumps and parallax".

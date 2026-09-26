# Round 17: graphics techniques

Eleven inexpensive techniques, chosen for VR (no screen-space effects that differ between the eyes), each with a
switch, a menu entry and preset values (Graphics page), and measured. The relight moved to ericw-tools 2.0.0-alpha11.

| # | Technique | Setting | Cost at the headset (estimate) |
|---|---|---|---|
| 1 | Real light directions (deluxemaps from the relight) | `vr_deluxemap` | cheaper than the old guess |
| 2 | Detail textures up close | `vr_detail*` | ~0.03 ms per eye, only within reach |
| 3 | Directional ambient on models | `vr_model_ambient_dir`, `_contrast` | ~0.01 ms CPU |
| 4 | Specular anti-aliasing (Toksvig + derivatives) | `vr_specular_aa` | ~0.1 ms |
| 5 | Fences: coverage-kept mips, alpha to coverage with MSAA | `vr_alpha_coverage` | ~0 |
| 6 | Tone mapping (float eye scene), dither, colour grades | `vr_tonemap`, `vr_dither`, `vr_grade` | ~0.07 ms per eye |
| 7 | Soft particles and sprites | `vr_soft_particles` | 0.04–0.2 ms |
| 8 | Shoreline foam, heat haze | `vr_water_foam`, `vr_heat_haze` | 0.15 / 0.33 ms when in view |
| 9 | Rim light on models | `vr_rim_light` | ~0 |
| 10 | Reflections on held weapons (a small world cube) | `vr_weapon_reflections` | 0.02 ms |
| 11 | Flickering torch and flame lights | `vr_torch_lights` | ~0.1 ms per eye with torches |

Integration: the rim light was capped (the model's light at 1, the ambient cube at 1.5, a steeper falloff): in the
firing range's daylight the sky behind made the training dummy glow white at 0.5.

## Detail textures

**Why.** In VR walls are always close, and Quake's textures (QRP's 512² ones too) turn into big blurry texels there.
As in DarkPlaces and Quake 3, a fine tiled grain is multiplied over the world's and brush models' textures close to
the eye, round mid-grey, so a texture's average brightness is unchanged (`vr_detail.cpp`, `DetailFactor` in
`gl_shaders.h`).

**The grains.** Six kinds, made by `Misc/quakevr/make_detail.py` (numpy; deterministic) into
`quakevr/textures/vr/detail_<kind>.png`: 512², grey, averaging exactly 128 (so their smallest mips are 128 and add
nothing: the grain fades by itself as it gets small on screen), tileable by construction (noise filtered in the
frequency domain, cell noise with wrapped distances, lines drawn wrapped round the edges).

| Kind | Look | Tile (units) | Strength |
|---|---|---|---|
| stone | lumpy rock, sand grain, pits, faint cracks in places | 64 | 0.8 |
| metal | brushed streaks along x, scratches (most along the brushing), wear blotches, pits | 48 | 0.6 |
| wood | fibres, wavy growth rings along x, dark pores | 64 | 0.8 |
| dirt | grime: stains, grit, faint streaks down along y | 48 | 0.75 |
| organic | soft cells, a ridged network, pores (flesh walls, swamp) | 48 | 0.75 |
| plaster | soft fine grain, tiny pits (ceilings, the medkits, the fallback) | 32 | 0.6 |

They are the layers of one mipmapped R8 texture array (2 MB), on texture unit 12, 8x anisotropic.

**Which texture gets which** (`quakevr/textures/vr/detail.cfg`, documented there; the relight's pattern style):
`kind <name> size= strength= grain=1` lines define the kinds; rules `<[map/]pattern> kind=<name>|none|auto scale=
strength= grain=s|t|auto` name id's, hipnotic's and rogue's textures by their names (`rock*`, `wall*`, `city*`,
`brick*`: stone; `metal*`, `wizmet*`, `tech*`, `twall*`, doors, buttons, the ammo boxes at half size: metal; `wood*`,
`wizwood*`, `crate*`: wood; `ground*`, `grass*`: dirt; `bodies*`, `dem*`, `wswamp*`: organic; `ceiling*`: plaster;
sky, lamps and light panels, stained glass, signs, `clip`, `trigger`: none). An animated texture's `+0`/`+a` prefix
is matched with and without. Every matching rule applies, a later one's settings winning. A texture no rule names
(a mod's) is judged by its 8-bit pixels: mostly fullbright: none; grey: metal if bluish or smooth, stone if busy;
green or strongly red: organic; a strong grain one way: wood; very dark: dirt; else stone (Quake's palette is mostly
brown, so this is a rough guess). Liquids and sky never get any. A grain kind (wood, metal) is turned to run along
the texture's own grain, the way its pixels change least (id's planks are mostly upright: the wood runs along t).
`vr_detail_list [pattern]` lists the map's textures, their kind, whether from the cfg or by colour, tile, strength
and direction; the file is read again at each map, `vr_detail_reload` rereads it and the images at once.

**The shader** (world and brush models, solid and alpha-tested surfaces; not liquids, not the software-emulation
modes): the array is read at the texture's own coordinates after parallax (`puv`), scaled to world units (the
texture's size over the kind's tile: `Call.detail`, per draw call, 16 bytes), with explicit gradients (so the branch
is safe), and a finer octave (3.7 times smaller, off the coarse one's grid) within half the distance. The albedo is
multiplied by `1 + strength x (2 x detail - 1)` before lighting; fullbright texels (Ironwail's alpha-bright ones) and
fullbright textures are left alone. It is full within a third of `vr_detail_distance` (144 units: about 1.2 m full,
gone at 3.7 m) and fades out smoothly; past it the pixel does only the distance test. Normal maps and parallax are
untouched (no detail normal: the grain is in the colour only).

**Settings** (Graphics, after Parallax: Detail Textures, Detail Strength, Detail Distance): `vr_detail` 1,
`vr_detail_strength` 1 (times each kind's), `vr_detail_distance` 144, `vr_detail_fine` 1 (the finer octave).
Presets: off for "Off (Quake)" and Low, on for Medium and up.

**Engine side:** `Detail` in the frame data (after `CausticsScale`: strength, fade start and end, the fine octave's
scale), `Call.detail` (r_world.c's two call structs), `VR_DetailView` in `R_SetupView` (frame data, binds unit 12)
and `VR_DetailCall` in `R_AddBModelCall` (a texture's detail, cached per texture, cleared at a new map).

**Checked** in the mock (e1m1 tech08_1 and uwall1_2, e1m2 wizwood1_5 and wbrick1_5, start rock4_1 and wmet4_4, 36
units from the wall; QRP and the 8-bit textures): the brushed metal runs along the panels, the wood grain along the
planks, stone gets grain inside each 8-bit texel; the screen's mean brightness changes by -1.5 to +1% (a crop sees
part of a tile), +0.2% on a medium view, nothing past the distance. No repetition shows on a wall seen along its
length (the tile is 48-64 units and only the nearest metre or two has any). With QRP it is subtle except on metal
(QRP's textures are already busy); on the 8-bit textures it clearly fills the big texels.

**Cost** (RTX 4090, mock eyes 2048², `vr_profile` world+brush per eye, off / coarse only / with the fine octave): a
wall 36 units away filling the view 0.278 / 0.278 / 0.285 ms; a hall 0.155 / 0.165 / 0.165 ms. So about +0.01 ms an
eye at 2048², about +0.03 at the Quest 3's 3292 x 3524: one or two texture reads per pixel within reach, none past
it. CPU: a hash lookup per draw call. Loading: the six PNGs once, as the first map is drawn; the cfg at each map.

**Check in the headset:** walls, floors and doors up close (a hand's reach to two metres): the grain should read as
the material (stone grain, brushed metal, wood fibres) and not as noise or a screen-door pattern; that it fades in
without a visible edge as you walk up (`vr_detail_distance`); no shimmer at grazing angles on floors; the strength
(`vr_detail_strength`, 0.5 to 1.5) with QRP and with the 8-bit textures; textures given the wrong kind
(`vr_detail_list`, then a rule in `detail.cfg`).

Not done: model skins (alias models) get none; a detail normal map.

## Directional ambient on models

**Why.** Monsters, items, your hands and weapons take one brightness, the lightmap under them (`R_LightPoint`),
shaded 0.6 .. 1.4 by the direction of the map's lights (`vr_modellight`): a grunt beside a lit wall is as bright on
the wall's side as on the dark room's; a weapon turned from a lamp to a dark corner changes little. As Half-Life 2
does (the "ambient cube"), each model now takes the light around it from six sides and shades its normals with it.

**The cube, on the CPU** (`vr/vr_ambient.cpp`): 26 rays from the model's middle (the axes, the cube's edges and
corners; up to 1024 units): its frame's box through the drawn matrix, so held weapons and hands sample where they
are drawn; a point inside a wall (a hand pushed into it) is moved towards the eye, a monster's towards its origin.
Each ray is `RecursiveLightPoint`'s BSP descent along any direction: the first world surface it crosses that faces
it, and its lightmap there (every light style at its current value, `.lit` colour, bilinear between luxels, as
`InterpolateLightmap`); liquids are seen through except lava (a hot orange), the sky counts as half again the
average of the walls, a miss as that average. The rays are folded into +X -X +Y -Y +Z -Z by cosine lobes, through
the world's own lightmap contrast (`vr_light_contrast`, so a side matches how bright the wall it faces looks), then
divided by their average per channel (the model stays as bright as `R_LightPoint` makes it, with the floor's colour;
the sides keep their tints), sharpened by `vr_model_ambient_contrast` (a power), kept within 0.2 .. 2.5 (no side
black) and brought back to 1 on average.

**Cached per entity:** traced again when it moved 16 units (held things and hands 6) or a second passed (doors,
switched lights), staggered between entities; at most 24 new entities and 8 others a frame (held things always),
the rest keeping theirs a frame longer. What the rays hit is kept, and re-read every frame when a surface hit has a
light style other than 0, so flickering lights flicker on the model's side in step with the wall (a first version
sampled them once a second: the side jumped). After a new trace the cube fades from the old hits to the new over
about 0.2 s (both read at the current styles): no popping. The map changing clears it.

**On the GPU** (`gl_shaders.h`, alias shaders): the instance data gained `vec4 Ambient[6]` (xyz the six faces; `[0].w`
how much applies, 0 off; `[1].w` how much of the old directional shading stays: half, so a lamp in sight still lights
its side and the surroundings give the rest; the bumps on the model's own light are halved to match). The vertex
shader passes `gl_InstanceID` (`in_instance`, location 10); the fragment shader's `AmbientCube(n)` evaluates the
cube at a world-space normal (`n²` weights, the face on the normal's side of each axis; `vec3(1)` when off: a stable
name, the rim light tints with it), and `ModelAmbient` calls it at the normal bent by the skin's normal map as far as
the model's bumps go (held weapons half). It multiplies the model's own light (`in_color`); dynamic lights, their
shadows and sheen, fullbrights and the force grab glow are as before. Fullbright models (`MOD_FBRIGHTHACK`),
`r_fullbright`, `r_lightmap` and `r_showtris` get none. The cube is worked out once a frame per entity, at its first
draw (often the shadow pass), and shared by both eyes.

**Settings** (Graphics, after Model Lighting: Directional Ambient, Ambient Contrast): `vr_model_ambient_dir` 1,
`vr_model_ambient_contrast` 0.75 (0: one brightness all round; 1: the surroundings' own ratios; up to 1.5 in the
menu). Presets: off for "Off (Quake)", on for the others. `vr_model_ambient_show [1]` prints the cost so far and the
cubes of the models nearest the view (1: not the held ones).

**BSPX light grid.** Not used: the alpha11 `bspx.html` documents only `FACENORMALS`; `LIGHTGRID_OCTREE` (FTE's
format) stores occlusion-aware light per point with no direction, so it could replace `R_LightPoint`'s floor sample
for things in mid-air (a later step), but the directions come from the rays, which work on every map (custom and
not relit ones).

**Checked** in the mock (e1m1, the room after the start corridor: a grunt from `impulse 244` beside the lit walkway
under the east wall, seen from in front and from behind; the hammer held towards and away from the walkway;
`vr_model_ambient_show`): the grunt's arm and shoulder towards the lit walkway and the head under the ceiling
lamps are brighter, its legs over the dark floor and the side towards the dark wall darker, on the whole as bright
as before; the hammer's head brighter on the side facing the walkway, whichever way it is turned. Before/after
crops: `ambient_before_after.png` (the round's shots). Cubes seen: a grunt by the walkway +X 1.36 -X 0.74 +Z 1.74
-Z 0.51; the armour +Y 1.56 -Y 0.52; your own body parts differ by a few tenths where they stand
half against a ledge.

**Cost** (RTX 4090, mock, `vr_profile`): a trace of 26 rays takes 7 to 10 µs; the "model ambient" scope averaged
0.007 ms a frame (worst 0.06 ms, as a monster and the hands were first seen) with about 30 models in view. Traces
cap at 24 + 8 a frame (under 0.3 ms at worst, when a map's first view shows many new monsters); flickering-lit
models are re-read every frame (26 lightmap lookups, about a microsecond). GPU: two buffer reads and a few
instructions per model pixel; the instance grew by 96 bytes.

**Check in the headset:** monsters by lit walls and in doorways (the side towards the light brighter, no
flat-lit look), your hands and weapons turned towards and away from a lamp or a lit wall, pickups on lit floors
(their undersides darker), nothing popping as monsters walk from lit to dark (a fade of about 0.2 s) or as you
move your hands, flickering lights on a model's side in step with the wall. If it looks too strong or too weak,
Ambient Contrast (0.5 .. 1). Things to report: a model side lit from a room behind a wall (a sample point inside
the wall), a model much brighter or darker than before on the whole.

## Specular AA and alpha-to-coverage

In VR the head never stops moving, so a highlight that aliases sparkles, and an alpha-tested fence whose bars are
thinner than a pixel crawls.

### Specular anti-aliasing (`vr_specular_aa`)

**Why.** The sheen (`vr_specular`: Blinn, exponent 32, from dynamic lights) and the water's glints use the filtered
normal with the full exponent. Bumps near a pixel's size (the floors' diamond plate at mid distance, walls at
grazing angles) turn into single bright pixels that jump as the eye moves.

**What.** Two filtered-roughness terms, added as variances to Blinn's lobe (a lobe of exponent p has a variance of
about 1/p, so `p' = 1 / (1/32 + spread)`), with the intensity scaled by `(p' + 1) / (p + 1)` so the lobe keeps its
energy. A patch of sparkles becomes a steady, dimmer sheen, about as bright on the whole.
- **Toksvig, from the mip** (`BumpedNormal`): the normal maps are box-mipmapped in RGBA (gl_texmgr.c), and z is
  already stored in blue (the shaders only read x and y). The length of the mip's averaged normal therefore says how
  much the normals under it vary. `(1 - len) / len` times the bump strength squared is the spread. This needed no
  storage change, because the world's and skins' maps are RGBA8 (heights in alpha). Quake's own 8-bit textures drawn
  sharp have RG8 maps (blue reads 0) and skip it. It applies only while the map is minified (a magnified bilinear
  blend of two texels is a slope, not roughness), and 1/255 of the length is ignored as 8-bit rounding.
- **Geometric, from derivatives** (`NormalSpread`, Kaplanyan and Tokuyoshi): the variance of the final normal over
  the pixel, `0.25 x (|dFdx n|² + |dFdy n|²)`, at most 0.09. This covers bumps about a pixel wide, curved models, and
  the water's waves. It is skipped after a discard (alpha-tested surfaces keep the Toksvig part only).

It applies to the world, brush models, alias models (`SpecularAA` before the light loops, `SpecLobe` read by
`LightSpecular`; `SPECULAR_AA_FUNCTIONS`, fragment shaders only) and the liquids' glints (`LiquidShade`: both lobes,
200 and 24). `vr_specular_aa` (1; 0 off, 2 twice the spread) is carried in `Parallax.w` of the frame data (was
unused; both copies of the block already had it). Menu: Graphics, Sheen Anti-Aliasing, after Light Sheen.

**Measured** (mock, e1m1 from 480 -352 88 looking down the north corridor, QRP textures, the author's
`vr_normalmap_strength 1.5` and `vr_specular 0.2`, two `vr_light_test` lights, game paused). Five frames were taken
0.03° of head yaw apart. The sheen layer is each frame with the sheen minus the same frame without it, and the
flicker is the mean change of that layer from one frame to the next (99.5th percentile in brackets):

| Region | Off | On | 2 |
|---|---|---|---|
| The far highlight on the floor (the sparkly patch) | 4.23 (80) | 1.57 (20): -63% | 1.13 (19) |
| The floor, near to far | 2.08 (41) | 1.48 (25): -29% | 1.33 (24) |
| The whole view | 1.04 (24) | 0.88 (16): -15% | 0.85 (15) |

What remains is the real motion of bumps that are resolved on screen. The sheen's mean over the view is 10.6 with it
off and 10.1 with it on (-5%). The water glints in id1's start pool were too faint in that light to measure.

**Cost** (vr_profile, both 1024² mock eyes): world+brush 0.25 → 0.26 ms (+0.005 to 0.01 ms). That is about +0.1 ms
at the Quest 3's 2 × 3292×3524, from one extra derivative pair and a few ALU per pixel.

### Fences: coverage-preserving mips and alpha to coverage (`vr_alpha_coverage`)

**Coverage-preserving mips** (gl_texmgr.c, `TexMgr_AlphaCoverageMip`; Castaño's method). Alpha-tested textures are
the world's `{` textures and holey models' skins (TEXPREF_ALPHA with TEXPREF_UNCOMPRESSED, mipmapped; not their glow
maps). Each of their mips has its alpha scaled so that the share of texels passing the test (0.666, from 170 in 8
bits) is the top level's. The box filter averages thin bars into alphas under the cutoff, so sparse fences vanish
with distance. For example, mg1's `{stag_x1` girders (15% coverage) drop to 3% at level 3 and 0% at level 4. With
coverage kept they stay at 16% and 12%. The scale is found from the level's alpha histogram (the middle of the range
of alphas giving the closest count), and each level is still made from the unscaled one above it. On small levels
the choice is coarse: `{mgrate2` at 8×8 is either 44% (box) or 100% (kept), for 74% at the top. The textures are
reloaded when the cvar changes (`TexMgr_ReloadAlphaTested`). It is off in the "Off (Quake)" preset.

**Alpha to coverage** (only with MSAA: `vid_fsaa` 2 or more). With MSAA on, the world's alpha-tested pass
(r_world.c, opaque draws) and holey alias models (r_alias.c) enable `GL_SAMPLE_ALPHA_TO_COVERAGE` and
`GL_SAMPLE_ALPHA_TO_ONE` (`VR_AlphaToCoverage`). The shaders then output the alpha sharpened to about a pixel's width
round the cutoff, `(a - 0.666) / fwidth(a) + 0.5`, instead of discarding under it (they still discard where it is
0). ShadowFlags bit 64 tells them, and translucent entities (alpha < 1) keep the test. Without MSAA the test is
Quake's, unchanged: `coverage < 0.5` is the same as `a < 0.666`.

**Seen** (mock, mg1's `start` and `mge2m2` copied into a scratch game folder, `r_fullbright 1`):
- **Smooth-filtered grates, MSAA 4×.** With alpha to coverage the holes' edges are smooth, where the alpha test left
  them stair-stepped. MSAA alone does not change them.
- **Nearest-filtered (the author's `GL_NEAREST_MIPMAP_LINEAR`).** The texel edges stay blocky, as they should, with
  no fringe.
- **Mip coverage in the 1024² mock.** It changes only fences past about level 2 (for a 64-texel grate, farther than
  about 1500 units, less at grazing angles), plus the chains' ropes. At the headset's 3292 px per eye this starts
  about 3 times farther away, so in practice it matters for far and grazing fences.

**Cost** (vr_profile, mg1 start, both 1024² eyes): MSAA 4× GPU 0.80 → 0.91 ms, 2× 0.86. Alpha to coverage on top
adds nothing measurable. The mip scaling runs at load, over a 256-bin histogram per level.

**The author's settings** (`quakevr/ironwail.cfg`): `vid_fsaa 0` and `vid_fsaamode 1`. So in his headset only the
coverage-preserving mips are active. Alpha to coverage needs `vid_fsaa 2` (or 4) with `vid_fsaamode 0` (edges
only). With `vid_fsaamode 1`, every sample is shaded (supersampling) and fences are already anti-aliased by it, at
2 to 4 times the shading cost. Scaled from the mock, 2× MSAA at 3292×3524 would cost about +0.6 ms and 4× about
+1.2 ms.

**In the headset:**
1. The sheen on e1m1's diamond-plate floors and the metal walls under your flashlight or a muzzle flash, at mid
   distance. It should not sparkle as you turn your head. Compare with Sheen Anti-Aliasing off.
2. The sheen close up should look as before. Check that the bumps at distance aren't too dull (2 is stronger).
3. Distant fences and grates (the mission packs', mg1's): their bars should stay put instead of thinning out.
4. With Anti-aliasing 2× and `vid_fsaamode 0`: the grates' edges should be smooth. Check the frame time.

## Torch lights

**Why.** Quake's torches and flames are fullbright models with a baked light beside them: they burn, but the room
round them never moves. A small flickering dynamic light at each flame near you makes corridors come alive.

**Which** (`VR_TorchLights`, `vr_emissive.cpp`, called from `CL_ReadFromServer` after the temp entities): static
entities (`cl_static_entities`, their leaves from `cl_efrags`) and ordinary entities with a flame model:

| Model | Entity | Fire (above the origin) | Radius | Colour |
|---|---|---|---|---|
| `progs/flame.mdl` | `light_torch_small_walltorch` | 20 | 150 | (0.45, 0.28, 0.135) |
| `progs/flame2.mdl` frame 1 | `light_flame_large_yellow` | 14 | 170 | (0.5, 0.31, 0.15) |
| `progs/flame2.mdl` frame 0 | `light_flame_small_yellow`, `_white` | 3 | 130 | (0.42, 0.26, 0.125) |
| `progs/candle.mdl` | Rogue's `light_candle` | 10 | 80 | (0.3, 0.19, 0.09) |
| `progs/lantern.mdl` | Rogue's `light_lantern` | (5, 0, 4), turned by its yaw | 120 | (0.38, 0.24, 0.115) |

Hipnotic's torches are the same models. The colours are DarkPlaces' brightness (a muzzle flash is white 4 over 150):
a tenth of a muzzle flash, warm but not saturated. A first try at a third (the task's guess) doubled the brightness of
e1m3's door between two torches and turned it orange; the baked light is already there, so the dynamic one only has
to move it a little. Additive lights can't flicker round the baked level (nothing can be taken away), so they are
kept small instead; `vr_torch_light_scale` tunes them.

**Choice.** Each client frame: the flames whose leaves are in the viewer's PVS within 1200 units, the nearest
`vr_torch_lights` of them (a torch lit last frame counts as 20% nearer: hysteresis). A chosen torch fades in over 0.3
s, a dropped one out over 0.3 s (the four brightest of those fading keep their light until they are out, the rest go
out at once), and they fade with distance over the last quarter (900 to 1200 units): nothing pops. The map changing
(or the client's time going back, a load) clears the state.

**The light.** Placed once per torch: at its fire, then moved out to 18 units from walls close by (eight horizontal
traces of 18 units, the hits' normals summed; the client's own BSP, `worldtrace::world`), since a wall torch's fire is
a few units from its wall and an unshadowed light there would shine through into the next room. The flicker is
`1 + 0.3 × n(t)` on the colour, where `n` is two smooth value noises at 9 and 14 per second and a slow sway at 2.3
(weights 0.5, 0.3, 0.2; mostly within ±20%), from the client's time, so the same at any frame rate and seeded by the
torch's position, so no two torches flicker together. The radius moves ±5% with it and the light jitters by up to
0.8 units sideways and 1.2 up and down (slower noises). Unshadowed (`dlightNoShadow`) unless `vr_torch_light_shadows`
is set: then the nearest that many compete for `vr_shadow_dlights`' slots as any other light (explosions and muzzle
flashes too). With Quake's falloff (`vr_dlight_falloff 0`) the colour is capped at 1, so the flicker and the scale
change the reach instead. Keys -0x6000 - the static entity's index, -0x7000 - an entity's number.

**Settings** (Graphics, after Lightning Beam Lights: Torch Lights, Torch Light Brightness, Torch Light Shadows):
`vr_torch_lights` 8 (0 off, up to 16), `vr_torch_light_scale` 1, `vr_torch_light_shadows` 0 (up to 4). Presets:
Off (Quake) 0, Low 4, Medium and up the default.

**Checked** in the mock: start (the large flame in the brazier room and the walltorch on the pillar, from 546 700 80
looking west) and e1m3 (the door between two wall torches, from 1300 -352 -30 looking east; the hall at 600 -350),
off / on and three frames 15 frames apart: the walls round the torches and the door get a warm light (+36% on the
door's view, +7% on start's), and each frame differs by about 1-2 grey levels on average (the flicker), each torch on
its own.

**Cost** (RTX 4090, mock eyes 1024², e1m3 at the door, 1200 frames each, `vr_profile`; GPU ms per eye):

| | torches off | 8 lights | 8, the nearest shadowed |
|---|---|---|---|
| world+brush, left / right eye | 0.082 / 0.082 | 0.092 / 0.092 | 0.098 / 0.097 |
| dlight shadows (once a frame) | 0.001 | 0.001 | 0.007 (CPU 0.021) |
| torch lights (CPU) | 0.002 | 0.005 | 0.005 |

So about +0.01 ms an eye here (12% of the world pass), about +0.1 ms an eye at the Quest 3's 3292 x 3524 in a room
full of torches; the clustered light loop only shades what each light reaches. A shadowed torch costs another 0.006 ms
an eye and a shadow render every frame (its casters are redrawn: no cache yet). Models in reach are lit per pixel as
by any other light (`vr_dlight_models`).

**Check in the headset:** e1m3, e2m1, e4m1 and start's corridors: does the flicker read as fire (a gentle living
light, not a strobe or a pulse)? Is the room brighter on the whole than before (Torch Light Brightness 0.5 .. 1.5)?
Light showing through a wall on the far side of a torch (the floor of the next room just past the wall: unshadowed
lights go through walls; Torch Light Shadows 1 fixes the nearest); torches fading in and out as you walk round
corners (they should not pop); monsters and your hands near a torch lit warm and flickering.

Not done: darkening the baked light near a torch so that the flicker can go both ways (the lightmap doesn't know
which of its light is the torch's; the map-light shadows' estimate of a light's share could do it); a torch's
shadow cached (it never moves), which would make shadowed torches cheap.

## Tone mapping, dither, grading

**What clipped.** The eyes' scene was Ironwail's `GL_RGB10_A2`: unsigned, so nothing above 1 survived it, and the
world, model and liquid shaders clamped each channel to 1 before that anyway (`clamp(result, 0.0, 1.0)`). Overbright
lightmaps (x2), uncapped DarkPlaces-style dynamic lights (colours up to 4), lava's glow and the bloom added in the
post-process were all cut per channel. So a red or orange light on a wall went yellow and then white, and the
texture on the lit wall went flat. Static scenes rarely go over 1: 0.01-0.03% of the pixels at e1m1's start, in its
corridors and at the firing range, with vr_light_contrast 2.3. Explosions do: 4.3% of the pixels over 1 and 0.2% over
2 ten frames after a rocket hits a wall close by, up to 4.0. Lava's hot parts reach 1.9-2.7 as it pulses.

**The approach.** Tone mapping in the world and model shaders' outputs would compress each surface before blending,
so additive effects, the translucent pass and the bloom's bright pass would all see compressed colours. Instead the
eyes' scene became a float target and one curve runs in the post-process, the full-screen pass that already exists:

- `vr_tonemap` 1: the eyes' scene and composite colours are `GL_RGBA16F` (`VR_SceneColorFormat` in
  `GL_CreateFrameBuffers`; `vr_stereo.cpp` remakes the eye framebuffers when the format changes). `vr_tonemap` 2 uses
  `GL_R11F_G11F_B10F`, which costs the same as RGB10_A2 but has a 6-bit mantissa (5 bits for blue): steps of
  1/128 (blue 1/64) between 0.5 and 1, coarser than the 8-bit output, and no dither added later can hide that.
- The world, alias and liquid shaders clamp to `SceneTone.x` (a new frame-data member): 1 is Quake's clamp (the
  window, `vr_tonemap` 0), 8 in the eyes with `vr_tonemap`. The alias shader's own copy of the frame UBO
  (`ALIAS_FRAMEDATA_BUFFER`) was extended up to it. The OIT resolve still clamps translucent layers to 1, so the
  float target keeps additive effects that pile up above 1.
- The bloom's bright pass takes at most 2 from the float scene, so explosions glow a little more but specks don't
  flicker.
- The post-process (`GL_PostProcess`, variant 0; `VR_PostProcessTone`) does scene + glow, then `x vr_contrast x
  vr_exposure`, the tone curve, the headset's gamma, the grade and the dither. The mirror shader in `vr_stereo.cpp`
  does the same curve and grade (the window's post-process then adds the desktop's gamma and contrast; no dither).
  Both take the GLSL from `vr/vr_tonemap.h` (`QVR_TONE_GLSL`).

**The curve** (`QvrTonemap`) works on Quake's gamma-encoded colours as they are shown:

- Up to the knee, 0.8 on the brightest channel, nothing changes, so the tuned look stays: no toe, no contrast, no
  exposure change. Measured on the eyes' images with the author's settings (contrast 2.3, bloom 0.1), off/on:
  - e1m1 start: mean luma 0.0203/0.0203, p95 0.069/0.069;
  - e1m1 wall: 0.0454/0.0454;
  - e1m1 corridor: 0.0260/0.0259;
  - e1m7 over the lava: 0.1171/0.1159;
  - firing range: 0.2607/0.2595.
- Above the knee, an extended Reinhard shoulder on the brightest channel: slope 1 at the knee, reaching 1 at the
  white point (4). The colour is scaled with it, so its hue and saturation stay. What was 1 is now about 0.9: the
  price of room for everything brighter.
- Past 1, the colour goes towards white by 0.8 x (m - 1) / 3, so a flash's core and lava's hottest parts still look
  hot and yellow-white instead of flat saturated red. The first try, a quadratic path at 0.6, turned lava dull red.
- Effect: lit walls in an explosion keep their texture and their orange instead of a flat yellow; lava's glow keeps
  gradations. Clipped pixels in an explosion frame went from 10-25% to 0.02%.
- `vr_exposure` (1) scales before the curve. The knee and white point are constants in `vr_tonemap.cpp`.
- ACES and AgX were not used. Both have a toe and change mid-tone contrast and saturation, which would undo the
  vr_light_contrast tuning, and per-channel ACES is itself what turns orange to yellow.

**Dither** (`vr_dither`). The last step, into the 8-bit swapchain:

- Triangular-PDF noise of 1/255 either way, the sum of two hash noises (Ironwail's `whitenoise01`), the same for R,
  G and B (no colour noise), faded out below 1/255 so black stays black.
- The eyes get unrelated noise, so no common pattern fuses into a layer "in the lens". The first try, interleaved
  gradient noise with the other eye's transposed, showed a regular diagonal lattice when magnified.
- With `vr_dither` on, Ironwail's scene-level 8x8 Bayer (`r_dither`, `ScreenDither`, the same in both eyes and
  added before the glow and the curve) is off in the eyes.
- Modes: 1 fixed on the screen (default: an unchanging pattern is cheapest for Virtual Desktop's encoder), 2
  changing each frame, 3 the same noise in both eyes (to compare in the headset).
- In e1m1 with heavy fog (`fog 0.6 0.08 0.1 0.12`), the dark blue gradient quantized without any dither shows
  contour rings. Ironwail's Bayer and the new dither both remove them, and the new one has no pattern. Noise
  standard deviation 0.58 of a step.
- In ordinary textured scenes there was no measurable banding either way: the low-frequency error was about 0.04
  of a step with or without dither.

**Grades** (`vr_grade`, `vr_grade_strength`; `Misc/quakevr/make_grades.py` -> `quakevr/gfx/vr/grade_*.png`):

- 32x32x32 tables stored as 1024x32 strips (x = b x 32 + r, y = g), loaded as a 3D texture when first used and
  applied after the gamma.
- 1 "Quake VR" (film): a touch of contrast about 0.12, saturation +8%, cool shadows, warm highlights.
- 2 cold, 3 warm.
- 4 by episode: e1 cold, e2 moss (green-grey), e3 warm, e4 violet; start, end, mission packs and mods get film.
- Each keeps every colour's luma except for the contrast curve. The first pivot, 0.3, darkened e1m1 by 9-10%; at
  0.12 a frame's mean stays within about 3% (0.97-1.04 over the test shots).
- They are subtle by design: on e1m7 the grades are hard to tell apart at a glance.

**Menu and presets.** Advanced > Graphical Settings, after Headset Contrast: Tone Mapping, Exposure, Colour Grade,
Grade Strength, Dither; all apply live. The "Off (Quake)" preset sets `vr_tonemap` 0 and `vr_grade` 0; the others use
the defaults (1, 1). `vr_dither` is in no preset.

**vr_eyeshot** (1; 2 also the float scene as .pfm) saves the next frame's eye images, as the headset gets them after
the post-process and before the UI, to `<gamedir>/eyeshots`. Window screenshots show the mirror, which has no dither.

**Cost** (RTX 4090, mock eyes at 2048x2048 with `vr_render_scale 2`, e1m1's start, `vr_profile`, left/right eye):

| | eye | postprocess |
|---|---|---|
| off (RGB10_A2) | 0.557 / 0.515 | 0.040 / 0.051 |
| RGBA16F, curve | 0.574 / 0.526 | 0.041 / 0.040 |
| RGBA16F, curve, grade, dither | 0.585 / 0.536 | 0.054 / 0.053 |
| R11G11B10F, curve, grade, dither | 0.574 / 0.526 | 0.053 / 0.054 |

- The curve is almost free.
- The grade's 3D lookup and the dither add about 0.013 ms an eye to the post-process: about 0.04 ms at 3292x3524
  (x2.77 the pixels).
- The float scene adds about 0.011 ms an eye (RGBA16F over R11G11B10F or RGB10_A2): about 0.03 ms at the headset's
  size.
- Memory: 4 more bytes a pixel for the scene and composite colours, about 93 MB at 3292x3524 (4 times the scene's
  share with 4x MSAA).

**To check in the headset:**

- Rockets against a wall and the lava in e1m7 with Tone Mapping on and off. Walls should stay orange and textured.
  Is the lava hot enough?
- Dark rooms and fog or underwater gradients for banding, with Dither Per Eye, Moving and Same in Both Eyes.
  Does the fixed per-eye noise show through Virtual Desktop's compression, or shimmer?
- The grades, By Episode across e1-e4. They may be too subtle to matter; strength and the tables are easy to push
  (`make_grades.py`).
- Whether the brightest non-overbright things (white lamps, sky; 1 is now about 0.9) look duller. A higher knee or
  `vr_exposure` 1.05 would bring them back at the cost of range.

Not done: a world-stable dither (it would need the position per pixel); auto-exposure (Quake's levels are lit for a
fixed exposure); tone mapping in the palettized (`r_softemu`) modes, which keep Ironwail's own path.

## Soft particles

**Why.** Smoke, fire, blood mist, splashes and the explosion sprite are flat quads. Where one crosses a wall or a
floor it cut a hard straight line, which is very visible in VR. Now each fades out as the opaque scene comes close
behind it: `alpha *= smoothstep(saturate((sceneDistance - particleDistance) / fade))`, with both distances measured
along the view.

**The scene's distances.** These come from the liquids' pass in `vr_water.cpp` (round 14/15's refraction), now shared
through `water::opaqueSceneDistances()`. It is an R32F texture at half the scene's size. Each texel holds the view
distance (clip w, from the eye's own projection, reversed-Z or not) of the nearest of its four depth samples. It is
made once per view:
- When a translucent liquid draws (the refraction), or when the shoreline foam, the heat haze or the particles first
  ask for it.
- The foam's copy is made before the opaque liquids draw, so it is invalidated and made again.
- It is made again after the soft sprites write their depth (`water::sceneDepthChanged`).

It now works in every alpha mode (not only OIT) and with MSAA: a second program reads sample 0 of the multisampled
depth (`sampler2DMS`). The particles never sample the depth attachment they are tested against. Cost of one copy:
0.014 ms per eye at 2048², so about 0.04 ms per eye on the Quest 3 (3292x3524). It is free when the refraction
already made one that view.

**Particles** (`vr_particles.cpp`, `softness`). Each particle's `Vertex::soft` (new in `vr_gfx`: a fourth attribute,
and `State::sceneDistances` on unit 1) is its fade distance in units:
- **Drops, sparks, dots:** 1.5 units. **Chips:** 1 unit.
- **Puffs** (smoke, fire, blood, blood mist, gun smoke, glows, lightning): a third of their radius, clamped to 1.5–16
  units.
- **Ripples** lying flat on a liquid: 0. They stay hard, because on an opaque liquid they would vanish.

Puffs are also pulled towards the eye by their fade distance, capped at their radius and at 12 units. The pull moves
all four corners along the eye's own rays, so nothing moves on screen, in either eye. Without the pull, a puff born
against a wall (a bullet puff, an explosion's smoke) would lose its middle. With the cap, a puff more than 12 units
behind a wall does not show through it. `vr_soft_particles_scale` (0.25–3, default 1) multiplies all of these
distances.

**Sprites** (`r_sprite.c`, `gl_shaders.h`'s sprite shader). The explosion sprite (`s_explod`), bubbles and other
sprites were alpha-tested quads drawn in the opaque pass. With soft particles on:
- `R_DrawSpriteModels` leaves them to `R_DrawSpriteModelsSoft`. That runs first in `VR_DrawSceneTranslucent`, after
  the translucent pass.
- They keep their cutout (alpha under 2/3 is discarded) and are drawn premultiplied, writing depth, with the same
  fade as a puff of their radius.
- They are not pulled: they write depth, and pulled they would hide the explosion's fire and smoke particles.
- Oriented sprites (lying on walls) stay hard.
- Side effect: like Quake VR's particles, a sprite behind translucent water or an alpha entity is now drawn over it,
  not under it.

Ironwail's own particles (`r_part.c`) are unchanged. They are drawn into the OIT buffers before the distances exist,
and they are small dots mostly replaced by Quake VR's particles.

**Settings.**
- `vr_soft_particles` (1) and `vr_soft_particles_scale` (1), in Graphics after "Explosion Light".
- The presets turn it off in Off (Quake) and Low, and on in Medium and up.
- Without a depth texture (a desktop view with no post-processing) it stays off.

**Cost** (mock, RTX 4090, `vr_render_scale 2` = 2048² per eye, paused on 30 big smokes, 30 blood puffs, 3
explosions and a splash filling the view: GPU per eye). "vr particles" went from 0.137 ms to 0.171 ms, of which the
distances copy is 0.014 ms. The per-pixel fetch adds about 0.02 ms, about 15% of that pass. Whole frame: 1.45 to
1.50 ms. Scaled to the Quest 3 (2.8x the pixels), that is about 0.2 ms a frame in that worst case. With rockets
exploding in view every 0.8 s it was 0.04 to 0.05 ms per eye (averaged).

**Tested** on the mock backend, with before/after pairs taken on one paused frame (`pause`, then `vr_soft_particles
0/1`). The pairs covered:
- e1m1: rockets into the corridor floor. The fireball sprite and the fire lost their straight cut line along the
  floor, and the smoke settles into the floor.
- A rocket into a corridor wall at a grazing angle.
- The same with `vid_fsaa 4` (the multisampled path) and with `r_oit 0`.
- e1m2's moat: nails and `vr_particle_test 14`. Ripples unchanged, foam softer.

No shader warnings. Crops are in the session's scratchpad (`soft6_floor.png`, `soft6_msaa.png`, `soft8_crop.png`).

**To check in the headset:**
- Rocket explosions against walls and floors up close. There should be no cut line, and fire should not show through
  thin walls or doors.
- Bullet and nail puffs on walls seen head-on. They should keep their middle.
- Smoke trails grazing floors, and blood mist over bodies.
- The fade distance. If puffs near walls look too thin, try `vr_soft_particles_scale 0.5`; if lines still show, try
  1.5.
- A thin one-pixel gap can appear in the smoke along the edge of something in front of it (the half-size distances).
- Explosions under translucent water, now drawn over the surface.
- The "vr particles" and "vr scene distances" rows of `vr_profile` with heavy smoke.

## Relight tools and deluxemaps

**Why.** The relit maps were made with ericw-tools v0.18.1. Version 2.0.0-alpha11
(https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11) also writes where the light comes from at every
luxel (`-lux`, deluxemaps) and a grid of the light in the air (`-lightgrid`), so the bumps on the baked light can be
lit from the real direction instead of a guess.

**The tools** (`Misc/quakevr/relight_maps.py`, `relight_quakevr_maps.py`, `vis_maps.py`; GRAPHICS.md, "Relight
tools"). `--light` defaults to `ERICW_LIGHT`, `light` on `PATH`, then `DEFAULT_LIGHT`
(`C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64/light.exe`). The look keeps its options,
`-extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -lit`, and `OUTPUT_ARGS` adds `-lux -lightgrid`. With the same options 2.0
gives the same light: I compared all 73 maps luxel by luxel with v0.18.1's (median correlation 0.996, the mean
brightness within 1.3% on every map; e1m1, e1m2, e1m6, e2m1 and start at the fixture agent's views differ by 0.01 to
0.3 of 255 on average; `relight_2_0_before_after.png`, v0.18.1 left, 2.0 right; start's spawn row differs more only by
a passing dynamic light). No tuning of dirt, gamma, range, minlight or the sky was needed: none of 2.0's changed
defaults touches these options. Three things did differ:
- `"light" "0"`: dark in 2.0, the default 300 in id's light and in 0.18. e1m4's six torches went dark (5% darker on the
  whole). `id_light_values` gives them 300 again, for `light` only.
- Strips thinner than a luxel with a brush beside them: 2.0 samples them on their edges, where the brush's face shades
  them. They come out half as bright or black: 0.02% of id's maps' luxels, but 0.6% of vrtutorial's, a dark band
  between every board and the strip light over it. vrtutorial is therefore lit by v0.18.1 (`--legacy-light`, the old
  path by default), with 2.0's light grid and its `.lux` moved face by face to the old layout (`remapped_lux`).
- BSPX lumps have to sit right after the 15 lumps. The scripts replaced the entities and the visibility by appending
  them, which would lose the lumps, so `vis_maps.packed` now repacks and keeps them.
The stamp holds `light`'s version, so every map was relit once. No source patch was needed.

**Outputs.** All 73 maps (id1, hipnotic, rogue) with `--vis-dir`, into this worktree's `quakevr/relit` and
`C:/OHWorkspace/quakevr-iw/quakevr/relit`: 209 MB, 143 MB before. The `.lux` files are as large as the `.lit`s, and the
grids add about 0.3 MB a map. They are all still water-vised as before (`vis_maps.py --check`: the same list). The
committed maps are `quakevr/maps/vrtutorial.bsp/.lit/.lux` and `vrfiringrange.bsp/.lux` (`.ent` unchanged), with a
64-unit grid: vrtutorial.bsp 0.97 -> 1.08 MB, vrfiringrange.bsp 1.59 -> 1.78 MB, and the `.lux` files 0.24 and
0.20 MB. They are copied to `C:/OHWorkspace/quakevr-iw/quakevr/maps`.

**The .lux format.** "QLIT", version 1, then 3 bytes for every lighting byte: every style has its own directions. They
are in the face's texture space (s, -t, normal), not world space and not orthonormal (ericw-tools `light/write.cc`).
DarkPlaces' `.dlit` for Q1 maps is also tangent space. The engine turns them into world space at load, adds the styles
weighted by their light, and stores them in a second atlas laid out as the lightmap's, in a frame the shader rebuilds
from its derivatives. Checked numerically on e1m1: the rebuilt directions and the directions to the lights reaching
7232 luxels agree to 3.7 degrees at the median (46 with the t axis's sign flipped). The details are in LIGHTING.md,
"Deluxemaps".

**Light grid.** The `LIGHTGRID_OCTREE` BSPX lump is inside each relit `.bsp` (`quakevr/relit/<game>/maps/<map>.bsp`,
and the committed `quakevr/maps`): the light every 32 units (64 in `quakevr/maps`), per style, with no direction. Its
layout is in LIGHTING.md. Nothing reads it yet: the models' directional ambient above traces the lightmaps. alpha11's
`-lightgrid_format lightgrids` would write six directions per point (an ambient cube) instead, but that format is
marked "wip, non-final".

**Engine** (`vr_deluxemap` 1; Graphics, "Real Light Directions", after Bumps in Map Light; 0 or a map without a `.lux`
keeps the guess): `Mod_LoadLux` (gl_model.c), `GL_FillSurfaceLux` and `lux_texture` (r_brush.c, unit 9, bound in
r_world.c), `ShadowFlags` 128 (vr_lighting.cpp), `LuxDirection`/`LuxLight` in the world shader. Both eyes read the same
direction, and only the sheen follows each eye. The bumps lean at most 50 degrees: at 60 the lit sides of ridges
clipped white, and the guess leans 45. The baked light now also glints off the bumps, at a quarter of the dynamic
lights' `vr_specular`: at half, rough walls by a lamp looked hazy and 7 to 12% brighter. Liquids and the translucent
passes are untouched, and every shader variant compiled (no errors in the runs).

**Before / after** (`deluxemap_before_after.png`, guess left, deluxemap right): e1m1's wall by the lantern, e2m1's
riveted wall under the blue lights, start's note spot (start_2026-09-26_02-21-12) and a vrtutorial board. The relief
now faces the lamp that lights it. The e1m1 wall is lit from the lantern at its side, not from above. The note spot's
ledge top has a grazing relief from the torch, and its front is flatter where the light is straight on (the guess
tilted it upward). The mean brightness of 12 views changes by -3 to +7% with the sheen, and within 4% (most within 1%) without it. No dashes
or rings at the note spot (round 15's artifacts came from the guess's slope, which the deluxemap doesn't use).

**Cost** (RTX 4090, mock eyes 1024², both eyes' world+brush GPU): start's note spot 0.206 ms with the guess, 0.195
with the deluxemap, 0.185 with Bumps in Map Light off. e1m1's first corridor: 0.284 / 0.248 / 0.235. The deluxemap
is cheaper than the guess, with one texture read in place of 4 to 16 gathers. At the headset's size it costs about
0.1 ms over no bumps, and the guess about twice that. Load: one pass over the luxels, and a second atlas as large as the
lightmap's.

**Check in the headset:**
- At start's note spot and on e1m1's walls by the lanterns, the relief should light up on the lamp's side. Compare with
  Real Light Directions off.
- At the vrtutorial boards, the strip of wall between each board and its strip light should be lit, not a dark band.
- A glint moving on the bumps near lamps as you move your head: too much? It is set by Light Sheen (`vr_specular`).
- Ridges lit at grazing angles (ledge tops, floors by a wall lamp) clipping white: tell me (the 50-degree limit).
- Straight lines in the bumps' shading along a face's edge (faces with different directions side by side).
- Custom maps and `vr_relit_maps 0` should look as before (the guess).

## Shoreline foam and heat haze

### Shoreline foam

**What.** A soft, moving foam line where water meets walls, steps, pillars and things standing in it. Slime gathers a
dark green scum there instead. Lava gets a hot orange-yellow rim where it meets the rock (it blooms), with a darker
crust in flecks just beyond. On water, faint whitecaps show on the crests of the geometric swells.

**How far the shore is.** It comes from two sources, and the nearer one wins (`LiquidFoam` in `gl_shaders.h`, in both
liquid programs, lit and unlit):
- **The rim.** The geometric waves' mesh already knew each vertex's distance to the pool's rim: a wall, a pillar,
  another liquid. The pin attribute now carries it, as `1 + distance` in units (capped at 32); the shaders make the
  pin from it (`smoothstep(1, 33)`, the same curve as before). This part doesn't depend on the view: it works on opaque
  and translucent liquids, with MSAA or without. The mesh is now also built when the foam or the haze is on, even with
  Real Waves off (no swells then). A level face with more liquid over it (two liquid brushes stacked, like start's
  pool: the face 96 units down drew a foam octagon through the see-through water) gets -1: no foam and no swell.
- **The scene behind the surface.** This is the shared half-size copy of the scene's distances (`sceneDistances`). It
  gives the distance along the view ray from the surface to what is behind it: a submerged step, a monster's legs,
  a wall seen at a slant. It is now also made for opaque liquids, in any alpha mode, and with MSAA (sample 0). When
  opaque liquids draw, they aren't in it yet, so it is made again later for whatever else reads it in that view (the
  translucent liquids, the soft particles, the haze).

`foam = 1 - d / width`, with a width of 10 units for water, 7 for slime and 8 for lava, scaled by `vr_water_foam`. It
widens by up to 30% where a swell pushes against the shore. Two octaves of world-space value noise that crawl over
time break the edge up. On top of that there is a solid line at the shore itself, sparse flecks out to twice the
width, and a finer noise for texture. It only draws on level faces seen from above, and fades out from 600 to 1000
units. The mesh's far-away flat fans start past 1024 units, so their coarse rim distances never show.

**Colour.** Water foam is off-white, lit by the lightmap on lit water. On Quake's fullbright water it takes the
texture's brightness (3x the texture's luminance + 0.2, clamped to 0.4..1), so it doesn't glow in a dark pool. Foam
makes translucent water opaque where it is (alpha to 0.9).

**Settings.** `vr_water_foam` (1; 0 off, up to 2 wider and denser). Menu: Graphics > Water and Liquids > Shoreline
Foam. The presets turn it on from Medium up (like Real Waves).

### Heat haze

**What.** The air over lava shimmers: the far wall seen across a lava pit wobbles, most just above the lava. Looking
down, the lava itself swims. An explosion leaves a ball of hot air for about 0.9 s. It grows from 36 to 96 units,
rises and fades, with a brief shock ring pushing outwards in its first 0.27 s. Flames (`progs/flame.mdl`,
`flame2.mdl`) within 640 units shimmer above.

**How** (`vr_haze.cpp`, called from `R_RenderScene` right after the translucent pass: `VR_DrawHeatHaze`):
1. **Volumes.**
   - Over every level lava face of the world, a layer 72 units deep: its top at +72, and walls on the rim edges only
     (built from the mesh's lava tops, `water::lavaTops`, and culled by the PVS the mesh marks, `lavaTopInPvs`).
   - An ellipsoid for each explosion (`VR_HazeExplosion` in `cl_tent.c`: TE_EXPLOSION, TE_EXPLOSION2 and the tarbaby's;
     none under water or slime).
   - An ellipsoid over each of up to 12 flames.
2. **The copy.** The part of the screen the volumes' boxes cover (plus 32 pixels) is copied from the scene into a
   texture of the scene's own format, resolved when multisampled.
3. **The draw.** The volumes are drawn over the scene (front faces, depth tested). The shader follows the view ray
   through the volume:
   - **Lava layer:** from where the ray enters (or from the eye, when it is inside or within 12 units of it), until it
     leaves the layer, meets the scene (the scene's distances) or has gone 192 units. The heat is `(1 - height/72)²`,
     integrated exactly along the ray.
   - **Ellipsoids:** the chord through a `1 - r²` density.
   - It then shifts a point on the ray in the world by a rising field of warped sines (15- and 24-unit waves, all
     three axes), projects the shift, and reads the copy there.
   - The shift is the same world-space shift in both eyes, so the shimmer sits in the air at a depth, not on the
     screen.
   - Where the shifted pixel is something in front of the hot air (a hand, a gun), it reads its own pixel: no halo.
   - The particles drawn after it (Quake VR's fire, smoke and embers) stay sharp.

**Settings.** `vr_heat_haze` (1; 0 off .. 1). Menu: Graphics > Water and Liquids > Heat Haze. The presets turn it
off in Off (Quake) and Low, and on from Medium up. Lava's bend is half an explosion's. At most about 0.4° of bend,
typically 0.1–0.2°.

### Cost

Mock, RTX 4090, `vr_render_scale 2` (2048² per eye), GPU ms per frame for both eyes, `vr_profile`, 300 frames per
row, repeated.

| scene | off | on | the new scope |
|---|---|---|---|
| start's lava from beside the pit, grazing (haze) | frame 1.08 | 1.17 | haze 0.118 = copy 0.046 + distances 0.028 + draw 0.044 |
| start's lava pit from above (haze) | | | haze 0.115 |
| start's pool, see-through, `r_oit 1` (foam) | water 0.128 | 0.183 | +0.055 (half of it the extra distances copy) |
| start's pool, opaque (foam) | water 0.067 | 0.117 | +0.050 (same) |
| torches in view (haze) | | | 0.03 |

At the Quest 3's 3292x3524 (2.8x the pixels):
- Haze: about 0.33 ms a frame when lava fills the view. A third of that is the copy, and a quarter is the distances,
  which the soft particles need anyway.
- Foam: about 0.15 ms.
- Nothing is spent without lava, explosions, flames or liquids in view.
- CPU: the per-view volume list, a few dozen boxes at most.

### Tested

Mock, with the before/after pairs taken on one paused frame. The crops are in the session's scratchpad (`foam/final_*.png`):
- `final_foam_pool.png`: start's pool from above, see-through (top) and opaque (bottom), foam off/on.
- `final_foam_e1m2.png`: e1m2's shallows (the 172 surface round 1776 -150), see-through, foam off/on.
- `final_lava_rim.png`: start's lava pit, rim off/on.
- `final_haze_lava.png`: start's lava at a grazing angle, haze off/on, and their difference (the far wall's band just
  over the lava and the lava itself).
- `final_haze_msaa.png`: the same with `vid_fsaa 4`.
- `final_haze_explosion.png`: a rocket against e1m1's riveted wall, haze off/on (the rivets and the wall's edge bend
  round the blast).

Also checked:
- The eye inside the lava layer (the full-view pass from the eye).
- Flames at start's wall torches and fire bowls.
- `r_oit 0`, `r_softemu 1`, `r_litwater 0` and MSAA all draw, with no shader errors in any run.
- The underwater view shows no foam (the foam is on surfaces seen from above only).

### Check in the headset

- **Foam.** Walk round start's pool and e1m2's moat, with see-through water (`r_wateralpha 0.5`) and opaque.
  - Is the width right (Shoreline Foam 0.5–2)? Is the crawl too fast or too slow?
  - Does the foam sit still on the walls as you move your head, the same in both eyes?
  - Is there a foam ring round your legs and round monsters in see-through or opaque water?
  - Are there odd straight lines of foam on open water (a rim where two water brushes meet)?
- **Slime and lava edges.** Is the slime's scum visible? Is lava's hot rim too bright with bloom?
- **Haze over lava** (start's pit, e1m7): look across the lava at eye level and from above.
  - Is it visible but comfortable (Heat Haze 0.5 for less)?
  - Does it stay put in the air as you turn your head?
  - Any halo round your hands or gun held over the lava?
- **Explosions.** Fire rockets at walls near and far: does the blast bend the wall round it for about a second? Is the
  shock ring too much?
- **Torches.** Is the shimmer over torches too subtle to notice, or distracting?
- **`vr_profile 1`:** the "haze" row (with "copy" and "distances" under it) and "water", with lava filling the view.

## Rim light and weapon reflections

Two additions to the alias shader (`ModelRimReflect`, `MetalMask` in gl_shaders.h), with their per-instance values
from `vr_envmap.cpp` (`VR_AliasSurface`, the instance's new `Surface` vec4 after `Ambient[6]`).

### Rim light (`vr_rim_light`, 0.5)

- **The light:** a fresnel falloff, `pow(1 - saturate(dot(n, v)), 2.5)`, with `v` towards each eye's own position,
  so each eye sees it on its own silhouette. The smooth vertex normal is used, not the bumped one, so the rim stays
  steady.
- **The tint:** the model's own light (`in_color`) times the directional ambient cube (`AmbientCube`, vr_ambient.cpp)
  in the direction behind the edge (`n - v`: away from the eye, round the side). That is the light around the model
  from behind it, so the rim fits the room. With `vr_model_ambient_dir 0` the cube is 1, and the rim takes the
  model's own light. The rim is added as light on the skin (`skin + 0.2`, so dark skins still get some). It is
  weighted by the lit share (fullbright texels get none) and added before the fog. There is no rim in the dark.
- **Per model:** monsters, items and projectiles get the full strength. Held and holstered weapons get 0.6 of it,
  your hands and fingers 0.35, and your body none (seen from inside, its edges are everywhere).
- **Menu:** Graphics ("Graphical Settings"), after Ambient Contrast: Rim Light. **Presets:** 0 on Off (Quake),
  0.5 on the others.

### Weapon reflections (`vr_weapon_reflections` 1, `vr_weapon_reflections_strength` 1)

- **The cube** (`vr_envmap.cpp`): 6 x 64 x 64, R11G11B10F, 7 mip levels (`glGenerateMipmap` after each face).
  - It holds the lightmapped world only. It is drawn from Ironwail's own brush vertex buffer (`gl_bmodel_vbo`: the
    same position, lightmap coordinates and styles attributes), plus one extra buffer of per-vertex colours. Each
    texture gets its average colour: its smallest mip, read back once as the map loads, so replacement textures
    give their own colours. It also gets its fullbright part as an unlit glow.
  - The lightmap is read as the world shader reads it (1 to 4 styles, `vr_light_contrast`). Liquids are unlit. The
    sky is not drawn: the cube is cleared to a dim grey-blue there.
  - Left out: brush entities, models, particles, dynamic lights, bumps, parallax and shadows. There is no culling:
    the whole world is drawn per face (12 to 15k triangles on id maps).
  - Reversed infinite depth, as the engine's `glClipControl`.
  - It is built once per map, 2 ms of CPU (e1m1: 14,551 triangles and 59 textures; e4m3: 11,692 and 48).
- **Where and when:** it is seen from between your hands, or from the head if that point is inside a wall. Updates
  happen once per frame, before the eyes (`envmap::update` in `VR_RenderView`), so both eyes share the same cube.
  - Each round of six faces is seen from one point.
  - While the hands move more than 4 units from that point, one face is drawn per frame, so the whole cube renews
    every 6 frames.
  - While they don't, one face is drawn every 8 frames. This picks up flickering light styles and doors.
  - Nothing is drawn while no model has asked for reflections in the last 30 frames (fists only, or the setting is
    off). Reflections are off until the cube's first six faces are done.
- **The shader:**
  - The sample is `textureLod(EnvCube, reflect(-v, n'), lod)`. `n'` is the smooth normal half bent by the skin's
    bumps, and `v` comes from each eye's own position: physically right per eye, same cube.
  - It is scaled by Schlick fresnel. `F0` is between 0.5 and the skin colour, so metal is tinted by its paint
    straight on and turns white at grazing angles.
  - It is also scaled by the metal mask and the model's metalness × `vr_weapon_reflections_strength` × 1.5.
  - It is darkened by the light at the model (`smoothstep(0.02, 0.35, luminance(in_color))`), so a weapon in a dark
    corner doesn't glow even when the cube sees a lit room.
  - The mip level sets the roughness: 1.5 for the swords' blades, 2 for the axe, 2.5 for guns, 3 for the rest.
- **The metal mask** (per texel, from the skin's colour, so it works on 8-bit skins and replacement skins alike).
  Quake's metal is its greys (palette 0-15), blue-greys (32-47) and green-greys (176-191). Its skin, wood, leather
  and rust are warm colours:
  - Texels where red leads blue (warm) count as metal only below 6-14% saturation.
  - Cool texels count up to 22-40%.
  - Near-black texels don't count (below 3-12% brightness).
  - On top of the mask, a per-model metalness (`weaponMetal`): the shotguns, nailguns and swords 1, the lightning
    gun 0.9, the axe 0.9, the grenade and rocket launchers 0.8, the Mjolnir 0.8, other weapons 0.7. Your hands and
    body get none.
  - Weapon pickups (`g_*`) and thrown weapons (`v_*` in the world) get 0.8 of their metalness, fading out from 96 to
    256 units from the cube's position (farther, the cube's view is not theirs).
- **Menu:** Weapon Reflections and Reflection Strength, after Rim Light. **Presets:** off on Off (Quake) and Low,
  on for Medium, High and Ultra.
- **Debugging:**
  - `vr_weapon_reflections 2` turns the whole weapon into a sharp mirror of the cube. The cube's orientation was
    checked with it.
  - `vr_envmap_dump` writes the six faces (+X -X +Y -Y +Z -Z) to `<gamedir>/envmap.tga`, which is the repository's
    `quakevr` folder: delete it after.

### Cost

On the RTX 4090 in the mock, with `vr_profile 1` ("env cube" row), the hands swinging (`vr_mock_swing 1`), so a face
is drawn every frame:

| Map | CPU | GPU avg | GPU max |
|---|---|---|---|
| e1m1 | < 0.01 ms | 0.02 ms | 0.15 ms |
| e4m3 | < 0.01 ms | 0.02 ms | 0.16 ms |

With the hands still, it costs an eighth of that. The cost does not depend on the eyes' resolution: one 64² face per
frame for both eyes. The alias shader's extra work is one cube read per metal texel of weapons, and the rim light's
few instructions per model pixel. Very large custom maps (hundreds of thousands of world triangles) cost in
proportion, since nothing is culled.

### Tested

Mock, e1m1 at 960 x 540, with 8-bit (`shots.ps1`) and QRP (`shots_r10.ps1`) textures. The shotgun, double shotgun,
super nailgun and sword were each held close to the view under the corridor's ceiling lamp (480 -144) and in the dark
spawn area, reflections on and off. A grunt (`impulse 244`) stood against the dark corridor with the rim at 0, 0.5
and 1.

- The double shotgun's blue-grey body and the shotgun's barrels pick up a soft grey sheen along their top edges,
  from the lit floor and lamps. The sword's grey parts shine a little; its blood-red parts don't (warm).
- In the dark, the reflections nearly vanish.
- The grunt's shoulders, arms and helmet get a faint warm edge at 0.5 and a clear one at 1.
- QRP's darker weapon skins reflect less than the 8-bit ones.
- No shader errors, 8-bit and QRP.

### Check in the headset

- **Rim light.** Do monsters in dark rooms read against the background at 0.5, without looking outlined or glowing?
  Does the rim sit on the true silhouette in both eyes (no shimmer between eyes)? Are your own hands' edges fine at
  0.35 of it?
- **Reflections.**
  - Move a shotgun or the double shotgun under a lamp and turn it. Does a sheen slide along the barrels as you tilt
    it, and does it look like metal rather than paint?
  - Is it the same in both eyes, apart from parallax?
  - Is it too strong or too weak (`vr_weapon_reflections_strength` 0.5 to 2)?
  - Any wrong parts shining (hands, wood, the ammo screens)?
  - In a dark corner, does the weapon stay dark?
  - Do reflections lag or pop when you walk (the cube renews every 6 frames while you move)?
- **`vr_profile 1`:** the "env cube" row, while waving the gun around.

# Feedback round 18: plan and notes

From the seventh batch of voice notes (21 notes, a memory log and a profile).

| # | Note | Request | Status |
|---|---|---|---|
| 1 | vrfiringrange 14-13-13, 14-13-44 | The button on a gun carried by its foregrip | done |
| 2 | vrfiringrange 14-16-22 | Sword bash from a level hold; a two-handed blade grip; distinct bash/parry/shove sounds | done |
| 3 | e1m1 14-22-24 | Bigger, modern splashes; ripples in the waves' geometry | done |
| 4 | e1m1 14-24-48 | Blood drips leave floor marks; menu knobs | done |
| 5 | e1m1 14-25-16 | Brutal Doom gore: sprays, stuck gibs, drips, pools | done (`vr_gore`, Gore page) |
| 6 | e1m1 14-26-48 | Agents' test runs silent and not taking the focus | done (`-nosound`, `QVR_TEST_BACKGROUND`) |
| 7 | e1m1 14-27-38 | One hue for the player's effects (force grab too) | done (`vr_player_hue`) |
| 8 | e1m1 14-28-36 | Hands sinking into held boxes | done (`vr_held_surface_fit`) |
| 9 | vrfiringrange 14-30-05 | Shotgun: ejection port, pump grooves | done |
| 10 | vrfiringrange 14-30-52 | Super shotgun: dark receiver, better grip texture | done |
| 11 | vrfiringrange 14-32-01 | Shotguns' muzzle flames | done (they were missing from the models) |
| 12 | vrfiringrange 14-32-32 | Rocket launcher's see-through back | done (winding) |
| 13 | vrfiringrange 14-33-16 | Grappling hook grip | done |
| 14 | vrfiringrange 14-35-15 | Holsters: curved, attached, darker | done (their angle on the body: next) |
| 15 | vrfiringrange 14-36-43 .. 14-38-19 | Alternate models (lava, multi-rocket, plasma) like the normal ones | done |
| 16 | vrfiringrange 14-34-28 | Slower again after 15–20 minutes | see below |

## The slowdown (memstats_2026-09-26_14-04-49.csv)

From 14:30 to 14:38 in vrfiringrange the frame period rose from 8.4 to 10.6 ms while the game's own work stayed
flat: our CPU `busy_ms` 0.3–0.5 ms, the eyes' GPU 2.1–3.0 ms. What grew is the runtime's: `xr_submit_ms` (xrEndFrame)
6.8 to 8.9 ms and the GPU span around the submit 4 to 6.5 ms. VRAM, RAM and GL objects were flat. It is SteamVR's
compositor or Virtual Desktop's encoder (or the GPU shared with them), not the game: try the VDXR runtime (VR Settings
> Headset > OpenXR Runtime: Virtual Desktop), which skips SteamVR, and compare the same log.

## Alternate weapon models

Voice notes vrfiringrange 14-36-43 (rocket launcher), 14-37-13 (lightning gun), 14-37-48 (super nailgun, and
indent its lava grooves), 14-38-19 (nailgun): round 16's model changes only reached the normal models; the models
the guns switch to when the button on them selects the secondary ammo were the old ones, and the gun jumped in the
hand when switching.

**The mapping** (QC `WeaponIdToModel` / `SelectModelByFlags`, `WeaponSupportsSecondaryAmmoFlag`; each model has its
own `vr_weapons.inc` slot, numbered from 0 here, `_NN` in the cvars is one more):

| Gun | Normal model (slot) | Alternate (slot) | Generator |
|---|---|---|---|
| Nailgun | `v_nail.mdl` (3) | `v_lava.mdl` (11), lava nails | `improve_weapons2.py` |
| Super nailgun | `v_nail2.mdl` (4) | `v_lava2.mdl` (12), lava nails | `improve_weapons2.py` |
| Grenade launcher | `v_rock.mdl` (5) | `v_multi.mdl` (13), multi grenades | `improve_weapons3.py` (round 16) |
| Rocket launcher | `v_rock2.mdl` (6) | `v_multi2.mdl` (14), multi rockets | `improve_weapons.py` |
| Lightning gun | `v_light.mdl` (7) | `v_plasma.mdl` (15), plasma | `improve_weapons2.py` |

(The knight's sword's `v_hksword.mdl` is the hell knight's sword, not an ammo toggle.)

**Measured, not assumed.** The alternates are kept byte for byte in `Misc/quakevr/src_models/` now. `v_lava.mdl` and
`v_plasma.mdl` are their normal models with another skin: the same triangles in the same order at the same
positions in every frame (only UVs differ, so some vertices are split along more seams). `v_lava2.mdl` and
`v_multi2.mdl` are re-exports: the same parts re-quantised (the super nailgun's body 0.23 units further forward, its
barrels where they were; the multi-rocket launcher's tube 0.04..0.09 units off, 0.4 where a vertex rounded to the
next step), plus their own paint: the lava windows on the barrels, the hazard band on the tube. The grenade
launcher's alternate was modelled apart and was already done in round 16; the only difference left in its slot was
the two-handed zero blend (0.75 on the grenade and rocket launchers, 0 on their alternates), now 0.75 on both.

**Generators.** `Misc/quakevr/improve_weapons_alt.py` holds what the alternates share; each generator builds the
alternate right after its normal model, with the same builder on the alternate's own source:

1. The vertex correspondence (corner by corner when the triangle lists match, else by UV and position over every
   frame) and the offset between the two models' hand anchor vertices; the alternate is moved by it, so its model
   space is the normal model's and every part lands where it does on the normal gun.
2. The builder runs with the correspondence translating its vertex indices (`V` in `nail2`, `light`, `nail`;
   `build_rocket_launcher(out_dir, alt=...)`), so grips, guards, triggers, the folded lightning-gun stub, the
   super nailgun's cut grooves and the rocket launcher's nozzle with the round-18 open mouth (see-through back
   fixed in the same function) are the same code on both.
3. `align_slot` sets up the alternate's slot from the normal one: every setting copied, except the anchor
   indices (the same vertex, looked up in the alternate's own strip order), the hand offsets (from the
   alternate's anchor to the same hand position) and the weapon offsets (compensating the header origin about
   which Scale applies). It prints the lines that change.

Rerunning `improve_weapons.py` or `improve_weapons2.py` rebuilds both models of each pair, so a later change to a
normal model's builder reaches its alternate; if a normal slot's settings change, paste the printed alternate lines
into `vr_weapons.inc` too.

**The lava super nailgun's windows** (`lava2_windows` in `improve_weapons2.py`): the lava is painted as a capsule in
the barrels' shared texture (bright fullbright core, dark red frame round it) on the back half of each barrel's outer
face. Each face is re-cut round it: the frame's outer edge stays on the face, the core sinks `LAVA2_DEPTH` (0.5
units) into the barrel, and the frame's texels become the sloping walls, so the skin is unchanged. The face round the
rim is zipped to the face's old vertices (no new vertex on its edges, no cracks); the windows follow each barrel's
own frame through the spinning firing animation. The body gets the same modelled grooves as the normal super
nailgun.

**Settings.** New defaults: slot 11 (lava nailgun) only its two-handed hold (TwoHFixedHandRoll -86.399879,
TwoHFixedOffsetY 0.2, as the nailgun's); slot 12 (lava super nailgun) anchors hand/button/screen 28, muzzle 148,
two-handed 652, Offset (1.305568, 4.568497, 2.068202); slot 13 TwoHZeroBlend 0.75; slot 14 (multi-rocket) HandOffset
(2.115883, -0.862214, 0.209794), Offset (10.285302, 7.135272, 5.793625), TwoHZeroBlend 0.75; slot 15 (plasma) anchors
hand/button/screen 38, two-handed 222 (muzzle 104), HandOffset and Offset as the lightning gun's (the same header
origin). Drawn from the controller, each alternate's hand position is now the normal one's to 0.001 units, its muzzle
and two-handed anchors within 0.09 (they were up to 0.9 apart on the rocket launcher and the lightning gun).
`settingsVersion` 10 resets slots 11..15 in existing configs once; the engine needs a rebuild for the defaults.

**Tested with the mock** (vrfiringrange, `vr_weapon_grip_mode 1`, `impulse 156/157/158/160/161; impulse 9`, then
`impulse 43` to switch the main hand's gun to its secondary ammo): each pair, shot from the side and from behind,
shows the same gun in the same place with the hand on the same grip; only the paint changes. The lava windows read as
sunk slots (walls and shadowed edges) with and without `r_fullbright`.

**In the headset:** on each of the five guns press the button on top: nothing should move, the hand stays on the
same grip, the index finger in the same guard; only the paint (lava, hazard band, plasma coil) changes. Fire each
alternate once (recoil carries its grip and guard; the super nailgun's windows spin with the barrels). Look into the
multi-rocket launcher's back-blast nozzle from behind: closed as the rocket launcher's. Are the lava windows deep
enough (LAVA2_DEPTH)? Two-handed holds and ammo screens on the alternates: same places as the normal guns.

## Player colours and held objects

Voice notes `e1m1_2026-09-26_14-27-38` (force grab colours to match the gadget) and
`e1m1_2026-09-26_14-28-36` (the hand sinks into held ammo and health boxes).

### One hue for the player's effects

**New settings:** `vr_player_hue` (degrees, shipped as 110, the gadget's tuned green) and
`vr_player_saturation` (1 as made, 0 white, up to 2). Helper: `Quake/vr/vr_hue.hpp`.

Every effect that belongs to the player now takes that hue. Each one also has its own hue setting,
where **-1 (the default) follows the player hue** and 0..360 sets its own colour:

| Effect | Own setting | Old colour |
|---|---|---|
| Wrist gadget screen, its light and glow, the wrist log, the weapons' ammo screens and their light | `vr_gadget_screen_hue` | 128 green (shipped 110) |
| Shotgun and double shotgun glowing iron sights | `vr_sight_hue` (saturation: `vr_sight_saturation`) | 30 orange |
| Force grab: the aiming beam, the tendril, the target's outline glow, the trail and aim sparkles | `vr_forcegrab_hue` (new) | about 215 blue (sparkles gold) |
| Teleport arc where it can land (it stays red where it can't) | `vr_teleport_hue` (new) | 228 blue |
| Laser/dot crosshair | `vr_crosshair_hue` (new) | 0 red |
| Menu laser pointer | `vr_menu_laser_hue` (new) | 35 amber |

These are not player effects and are left alone: the gadget casing's tint (`vr_gadget_tint_hue`),
the map's text boards (`vr_worldtext_hue`), the menus' own amber widgets, and the voice notes' white "REC" text.

- The force grab colours are scaled so every hue is about as bright as the old blue (green is far
  brighter to the eye than blue at the same HSV value). The outline glow's colour reaches the
  world and model shaders through the frame data's `SceneTone.yzw`, which used to be unused
  (`VR_EntityGlowColor`, `gl_rmain.c`).
- **Menu:** Wrist Gadget > Colours starts with **Player Effects Hue** and **Player Effects
  Saturation**. After them come the per-effect hue sliders (Screen, Weapon Sight, Force Grab,
  Teleport Arc, Crosshair, Menu Laser), whose leftmost step reads "Player's" (-1). Changes show
  live. While the player hue, saturation or force grab hue row is selected, a force-grab tendril
  crackles from the off hand as a preview. The Crosshair and Menu pages have their own hue sliders
  too.
- **Migration (config version 12):** a config's `vr_gadget_screen_hue` becomes `vr_player_hue`,
  and the screen then follows it. So your 110 stays 110 and nothing you tuned changes, except that
  the force grab, the teleport arc, the crosshair and the menu laser now match it. A
  `vr_sight_hue` still at the old default of 30 now follows the player hue. A custom value is
  kept: yours is 145. Set it to -1 (the slider's "Player's" step) if you want the sights to match
  exactly. `vr_defaults.cfg` now ships `vr_player_hue 110` instead of the screen hue.
- In multiplayer, the sparkles (sent by the server) are drawn in each viewer's own hue.

### Held objects sit against the palm

**New setting:** `vr_held_surface_fit` (1 on, 0 the old placement).

When a hand grips a box, gib, head or backpack, and also when a force-grabbed object is caught,
the object is pushed the way the palm faces until it clears the fist:

- **Direction:** palms face in, as held. A right hand's palm faces its left, and `vr_lefthanded`
  is respected.
- **How far:** the object's drawn box (turned with it, at its scale) must clear a ball of 4 cm
  round the grip, so the push is the distance out of that grown box. It is capped at 25 cm.
- **Otherwise unchanged:** the rest of where it was gripped is kept, so you can still grab it from
  anywhere. It keeps its turn in the hand. Holding, throwing (the release velocity comes from the
  hand), taking at a holster, and the client's in-hand drawing all work as before, from the new
  offset.
- **Where the code is:** QC's `VR_Carry_Start` (`QC/vr_carry.qc`) calls a new builtin,
  `carryfit`. It is implemented as `held::surfaceFit` in `Quake/vr/vr_held.cpp`.

In the mock, a shells box gripped by the main hand used to sit around the fist. Now it sits beside
it, on the palm side. A force-caught box sits the same way beside the off hand's fist, and a gib
in the off hand is no longer over the hand.

**Please check in the headset:**

1. Do the force grab's beam, tendril, glow and sparkles match the gadget?
2. Does Player Effects Hue recolour everything live, with the tendril preview on the off hand?
3. Do your sights still look as you set them?
4. When you grab a box from different sides, does it rest against the palm or fingers, without
   jumping too far or leaving a visible gap? The fist size is one constant (`fistRadius`,
   0.04 m, in `vr_held.cpp`).
5. Do throws and catches feel the same?

## Splashes and ripples

Voice note e1m1_2026-09-26_14-22-24: the splashes are too small, especially the particles, which look like the old
circle particles. It asked for more and bigger particles, and for a ring that spreads faster and further. It also
asked for the splash to make a real ripple in the water's geometry, with everything configurable.

**Why they looked old.** The splash was Quake VR's own preset (`vr_particles 1`, which is your setting). But every
drop used `CellCircle`, the old engine's generated dot. That dot fills only a quarter of its quad. The drops were
0.45 to 1.1 scale, so their dots were 0.2 to 0.4 units across, about a centimetre. The ring's crest spread at about
5 units/s and ended about 10 units out. A body splash made about 80 particles.

**The new look** (`vr_particles.cpp`, `splash`). There are three new generated atlas cells (the atlas gets a
fourth row):

- **Drops** (`CellDrop`): a clear ball that fills its quad, with a bright, more opaque rim, a glint and a faint
  halo. Drops are drawn **streaked**: the quad is stretched along the drop's motion as seen from the eye, by 16 ms
  of that motion (8 units at most). Fast drops read as streaks and slow ones as beads. The crown has 10 + 22·s
  drops of 2 to 5 cm. There is also a fine spray of 8 + 30·s small drops thrown wider and lower, and a jet of
  3 + 4·s big drops. (s is the strength / 10: 0.4 for a shot, 1 for a hand's slap, 2 to 5 for a body or a rocket.)
- **Spray** (`CellSpray`): a cloud of fine droplets in a haze. 2 + 3·s of them are thrown up round the crown,
  spreading and falling. A plume of it rises with the jet, and a faint mist hangs over hard splashes.
- **Foam** (`CellFoam`): patches of bubbles with a ragged edge. 1 + 0.8·s of them lie on the surface, spreading
  and thinning out over 2 to 3 s.
- **Rings**: faster and bigger. They ride the geometric ripple's crest, at the ripple speed (32 units/s) times
  `vr_water_splash_ring_speed`, so they now reach 40 to 60 units. The ring texture is thinner and broken a little
  along its length. A quarter of the crown's drops, and half of the jet's, leave a small ring where they fall back.
- **Lying on the waves**: rings and foam are drawn as a grid of pieces, each corner at the surface's height there.
  The height comes from the geometric swells and the ripples, worked out on the CPU the same way as the mesh
  (`water::surfaceRise`). Drops vanish at that raised or sunk surface, not at the flat one. Before this, with your
  12-unit swells, a flat ring sank into the crests and floated over the troughs.

A body splash now makes about 270 particles, and a shot about 50. Lava keeps its hot blobs, embers and smoke. It
gets the streaks, and its foam and rings are a dark crust.

**Geometric ripples** (`vr_water.cpp`, "Ripples"; `gl_shaders.h`, `LiquidRipples`):

- Every splash the client receives starts a ripple, whether or not Quake VR's particles are on. That covers shots,
  thrown things, bodies, a rocket going off under the surface, hand slaps, strokes and wading steps. They all come
  through `particles::spawn` (`Preset::Splash`). The ripple's height is `vr_water_ripple_amplitude` ×
  √(strength / 10), from 0.35 to 2.6 times, and at most 8 units. The last 32 are kept. A new one within 16 units
  of one made in the last 0.15 s makes that one stronger instead. When all 32 are in use, a new one replaces the
  weakest.
- Each view, the live ones go into the frame data (`Ripple`, `RippleAt[32]`, `RippleAmp[8]`, appended after
  `Water3`): centre, surface height, age, and the height now. The height rises over 0.1 s, then falls by e every
  `vr_water_ripple_decay` seconds. Everything is from the client's time, so both eyes see the same thing at any
  frame rate.
- **The wave.** Round each ripple there is a packet of crests 0.7 wavelengths wide. The packet spreads at
  `vr_water_ripple_speed`, and its crests run out through it at twice that speed, as real ripples do. It gets lower
  the farther it has spread (1/√distance). The splash point dips first. For each ripple, the height is
  h = −A·exp(−u²)·cos(k(r − 2ct)), with u = (r − ct)/width. Its slope comes from the exact derivative. Only level
  liquid faces at the ripple's height (within 4 units) are affected. Lava's ripples are 1.5 times longer, move at
  0.35 of the speed and are 0.6 as high. Slime's move at 0.7 of the speed. Teleports get none.
- **In the geometry**: `LiquidDisplace` adds the ripple to the swells. So it is held still at the rim by the same
  pin (no gaps), and fades out between 512 and 1024 units, as the swells do. The grid is 16 units, so the ripples
  are in the geometry fully from 3 cells per wavelength (48 units, the default), partly from 2, and not below.
  With Real Waves off but ripples on, the mesh is still built, and only the ripples move it.
- **In the shading**: `LiquidWaves` adds the ripple's slope to the normal. So the fresnel, the glints and the
  refraction follow it, and brush-entity liquids get it too, though they stay flat. Close by, finer ripples a third
  as long ride in the same packet. They are shading only, and fade out from 300 to 800 units.

**Settings** (Graphics → Water and Liquids, after Splashes):

| Cvar | Default | Menu | What it does |
|---|---|---|---|
| `vr_water_splash` | 1 | Splashes | how many particles (0 off) |
| `vr_water_splash_size` | 1 | Splash Size | how big the drops, spray, foam and rings are |
| `vr_water_splash_ring_speed` | 1 | Splash Ring Speed | times the ripples' speed (1: riding their crest) |
| `vr_water_splash_ring_size` | 1 | Splash Ring Size | how big the rings start and how long they last (0: none) |
| `vr_water_ripples` | 1 | Ripples | on/off |
| `vr_water_ripple_amplitude` | 3 | Ripple Height | units for a hand's slap |
| `vr_water_ripple_speed` | 32 | Ripple Speed | units/s the rings spread at |
| `vr_water_ripple_decay` | 2 | Ripple Duration | seconds to die down to a third |
| `vr_water_ripple_wavelength` | 48 | Ripple Wavelength | units between crests |

The graphics presets turn the ripples off in Off (Quake) and Low, and on from Medium up, like Real Waves.
`developer 2` prints each ripple. `vr_particle_test 14 <strength>` now puts the splash where your view meets a
liquid's surface.

**Cost** (mock, e1m2's big pool at a grazing angle, `vr_render_scale 2`, your 12-unit swells and
`r_wateralpha 0.6`, 16 ripples live at once, paused):

- GPU: the water went from 0.15 to 0.31 ms a frame, both eyes. That is the loop over the live ripples, per vertex
  and per pixel, and it costs nothing when there are none.
- CPU: +0.1 ms, for the heights under the rings, foam and falling drops. Each grid crossing is worked out once per
  view. Before I added that cache it was +4.7 ms.

One splash at a time costs a few hundredths of a millisecond.

**Tested** in the mock (scratchpad `ripple/`):

- `before_after.png`: round 15's splash next to the new one in the start map's pool, from above, at 0.1, 0.35 and
  0.7 s.
- `ripples_on_off.png`: a low grazing view over e1m2's pool with the swells off, over 2.3 s. With ripples off the
  ring is flat. With ripples on, the splash point dips, then a bulge and a ring of crests travel out with the ring
  on them.
- `shoot2_c.png`: a super shotgun into the water (strength 4, one merged splash, a 1.9-unit ripple), then a rocket
  (strength 18.9, `splash_big`, two ripples: the entry and the explosion under the surface). Taken with your swells
  and water alpha.
- `graze2_c.png` and `start_c.png`: the ring riding the ripple's bulge. `slime_c.png` (second half): a
  wading-sized splash (strength 3).

I did not see wading, strokes or slaps in the mock (the scripted player fell out of the pool). They send the same
`Preset::Splash` as the shots (strengths 2 to 16), so they start ripples the same way. Lava was only checked by
reading the code.

**What to check in the headset:**

1. Shoot the water and jump in: are the drops big and dense enough, and do the streaks look like water, not rain?
   Use `vr_water_splash_size` and `vr_water_splash` (the count) to tune them.
2. Is the ring's speed right, and does it stay on the ripple's crest? Ring Speed 1 rides the crest.
3. At a pool's edge, crouch and look across while someone (or a grenade) splashes. You should see the surface dip,
   then a travelling ring of crests. Are Ripple Height (3), Speed (32) and Duration (2) right next to your 12-unit
   swells?
4. Where ripples reach a wall, look for gaps. There should be none: they are pinned like the swells.
5. Close by, check for sparkle or shimmer from the fine ripples' glints on the crest (the specular AA should keep
   it calm).
6. Wade in the e1m2 shallows and slap the water: there should be small rings at your legs and hand.

## Sword grips, bash, sounds

Voice notes vrfiringrange 14-13-13 and 14-13-44 (a gun held two-handed, then let go by the hand on its handle:
the button on top of the nailgun and of the lightning gun is in the wrong place and turned wrong) and 14-16-22
(the sword held level across and pushed forward doesn't bash; hold the sword with the off hand near the end of the
blade, "sparring", and have that parry and bash too; weapons may need several two-handed grips; different sounds for a
bash, a parry-bash and a shove, so each can be told from a blow).

### The weapon button on a carried gun

**Why.** Since round 16 a gun let go by its handle hangs from the other hand's foregrip, drawn from the pose of the
hand that let it go (`twohand::carriedWeapon`). The ammo screen already used that pose, but the button
(`setupButton`, `vr_view.cpp`) still took the angles of the hand whose slot the gun is in (the carrying hand, on the
foregrip, its wrist turned) and that hand's mirroring (a gun from the main hand carried by the off hand was drawn
unmirrored, its button mirrored, the roll offset flipped). Its position was right (an anchor on the drawn model), its
angles were not: the tilted, sideways button in the notes' screenshots.

**Now.** The view keeps, per hand, the pose each weapon is drawn from this frame (`drawnAs`: the hand's own, or the
carried pose). The button and the ammo screen are placed from it, mirrored as the drawn weapon is, and their per-weapon
angles are composed with it (`composeAngles`; the same as the old addition while the offsets are a roll, which all
buttons' are). The muzzle, the crosshair and the laser have no carried state (a carried gun has no aim, as in round
16); the shell ports and the flash are points on the drawn model already.

**Test (mock, vrfiringrange; `sw18/buttons.txt`).** Nailgun, lightning gun and shotgun held two-handed, the main hand
let go, then the carrying hand turned. `vr_dumpview`: the nailgun's button is at the same place and angles two-handed
and carried ((-7 -175 -3), the gun (-5 -175 -3)), and turned with the gun when the carrying hand turns (gun
(-15 -150 -20), button (-17 -150 -20)); the lightning gun's the same ((-9 -151 -20), with the gun (-9 -151 -20)); its
mirroring is the gun's. Screenshots: the button stays on top of the gun, as held.

### Two-handed grips: the sword's blade grip

**Several grips per weapon** (`vr_twohand.cpp`). A weapon's two-handed grips are now a list: the grip point of the
"fixed" display mode (a gun's foregrip; a sword's grip below the hand, round 15) and, new, its blade (weapon key
`TwoHBladeGrip`, `vr_wofs_2h_blade_NN`: where the helping hand may hold the blade, as a share of the way from the hand
to the tip; 0.75 for both swords, 0 for everything else). While the helping hand isn't holding, it takes the nearest
grip in reach: the grip point within 5.5 units as before, the blade within 6 units of its outer part (from 0.45 of its
length to just past the tip). Once held, the grip holds on (the grip point within 20 units as before; the blade while
the hands are 0.25 to 1.35 of the blade's length apart) until the hand lets go.

**The blade grip.** The blade lies along the line from the holding hand (at the hilt, leading) through the helping
hand, whatever the holding hand's wrist says: hold it across your body like a staff, both hands on it. The
helping hand is drawn on the blade where it is (`bladeGripHand`): its tracked pose turned by the least rotation that
lays its grip along the blade (either way round: thumb to the tip or to the hilt), and slid onto the blade's axis
between the hilt and the tip. The server sees it as any two-handed hold (the 2H aiming bit), so:
- **parry**: the blade (and the line through the hands) level across in front parries (`VR_Parry_Blocks`);
- **bash**: pushed forward, it bashes (below);
- **swings** are two-handed sword swings, the tip where the blade is drawn; the helping hand doesn't punch.
Letting go with the hand on the handle hands the sword off to the other hand as before (round 16): it then holds it
normally, by the grip, so the sword jumps from the blade to the hilt in that hand.

The sword's other grip (below the hand) is unchanged; it still wants the holding wrist roughly along the line
between the hands (`vr_2h_angle_threshold`). The blade grip doesn't.

`vr_dumpview` prints each hand's muzzle point, its distance from the hand and "held two-handed by its blade".

### Bash: the parry's guard

**Why it failed.** The bash's guard (`VR_Parry_Guard`, `combat.qc`) still used the parry's pre-round-16 test: the
hand's *forward* across the attacker's line, within 30 degrees of level, at chest or face height. A sword's blade is
some 70 degrees off the hand's forward, so a sword held level across had its hand's forward pointing ahead or up:
no guard, and the push was nothing (no bash, and no blow: a push forward is no swing). In the mock the one-hand level
sword (`-30.5 90 0`) had its hand's forward 30.5 degrees up, just over the old 30.

**Now.** The guard is the parry's own test (`VR_Parry_Blocks` with a blow from straight ahead): the weapon's line
(its pommel or stock to its tip or muzzle), or, held with both hands, the line through the hands, about level
(`vr_parry_angle`), across (not pointing ahead) and in front. One hand or two, the sword by its grip or its blade,
the axe, a gun held across by its grip. A gun carried by its foregrip is no guard. Both empty hands together
(the shove) and the one-hand palm shove are unchanged. The push is more lenient:
- the guard counts as still below 1.5 m/s (was 1.0), and the push may come up to 0.75 s after (was 0.4);
- a guard lost for up to 0.25 s during the push (the blade tilting as the arms drive) is still the guard held;
- the push must be within 53 degrees of straight ahead (was 45) and faster than `vr_bash_speed`, now 1.2 m/s
  (was 1.6; config version 11 moves a saved 1.6 to 1.2);
- held with both hands, the bash lands between the hands.

**Parry-bash.** A weapon bash within 1.5 s of parrying a blow is a counter, a *parry-bash*: 1.3 times the damage,
throw and stagger, its own sound; one per parry. `developer 1` prints "bash: 1 (parry-bash)", and the firing range's
dummy "parry-bash with the ...".

### Sounds

New sounds (`Misc/quakevr/make_sounds.py`, `quakevr/sound/vr/`), each unlike the blows' (Quake's axe and punch hits):
- `shove.wav`: a whoosh of air into a heavy low thud and the slap of cloth; no metal (both hands; one open palm at 0.7);
- `bash.wav`: a short whoosh, a dull clang (low metal partials, dying fast) on a heavy thud (a weapon bash);
- `bash_parry.wav`: the blade's scrape rising into its ring, over the bash's clang and a heavier thud (a parry-bash);
- `parry.wav`: a sharp strike, a bright long ring of steel and a short scrape (a parried blow; with Quake's axe hit
  under it at 0.35).
The crossed-arms parry keeps its fleshy thud. `vr_bash_sound` (Gameplay > Parry and Bash, "Bash and Parry Sounds")
is their volume; 0 brings back the old sounds.

### Tests (mock)

Scripts and logs in `scratchpad/sw18/`.
- **Blade grip** (`sword.txt`, `show.png`): the sword in the main hand level across (`-30.5 90 0`), the off hand
  gripping 60 cm to the left: "helping two-handed", "held two-handed by its blade"; the helping hand is drawn on
  the blade near the tip; raising and lowering the off hand turns the blade with it. `impulse 249`: "PARRIES ...
  two hands" (6 degrees off level). The round-16 hand-off test's sword pose (`handoff/all.txt`) still takes the grip
  below the hand.
- **Bash** (`bash1.txt`, a knight from `impulse 248`, god mode): guard held, both hands pushed 40 cm forward in 8
  frames: "bash: 1 (weapon)" with the blade grip, and again with the sword in one hand.
- **Parry-bash** (`knight2.txt`, no god mode): blade grip, pushes every 0.4 s: "bash: 1 (weapon)" twice, then
  "parry: monster_knight with hand 1 (two hands)", then "bash: 1 (parry-bash)". (As in round 16, once the knight
  circles to the player's right the blade points at it and no longer parries.)
- **Dummy** (`dummy.txt`, vrfiringrange's training dummy): "bash with the Knight's Sword, at the body" (blade grip, then
  one hand), "shove with both hands, at the body".
- **Sounds**: `soundlist` with sound on lists all four loaded (`snd.log`).

### In the headset

- Hold a gun by its foregrip and let go of the handle (nailgun, lightning gun): the button stays on top of the gun,
  where it was; turn the carrying hand: the button and the ammo screen turn with the gun.
- Sword, one hand, level across in front (knuckles to the enemy, as in the note's screenshot), then shove it forward:
  a bash (`developer 1`: "bash: 1 (weapon)"), with the new clang-and-thud. Is the push now easy enough? Too easy
  (a guard pushed by accident while advancing)? Bash Speed is in Gameplay > Parry and Bash.
- Take the blade near its tip with the off hand (grip): the sword lies between the hands, the off hand on the blade.
  Move the hands: does the blade follow without jumps? Is 6 units (23 cm) from the blade easy to catch, and does it
  let go only when you open the hand? Parry a knight with it held level across, and bash with it.
- After a parry, bash within a second and a half: the parry-bash's ring over the thud.
- Listen: blows (Quake's hits), a shove (whoosh-thud), a weapon bash (clang-thud), a parry-bash (scrape-ring-thud),
  a parry (bright ring). Loud enough? "Bash and Parry Sounds" sets their volume.

## Shotguns, rocket launcher, hook, holsters

Voice notes vrfiringrange 14-30-05 (shotgun: an ejection port where the shells leave; the pump's grooves
are only painted), 14-30-52 (double shotgun: the grey receiver does not go with the gun; the shotgun's
grip is nicer), 14-32-01 (the shotguns have no 3D muzzle flame while the other guns do), 14-32-32 (the
rocket launcher's back end is see-through), 14-33-16 (the grappling hook: a handle, a trigger guard and
a trigger) and 14-35-15 (the holsters: straight, flat and plain; a curve that adapts to the body; no
floating parts; grimmer and darker).

The models are still built by the generators from `Misc/quakevr/src_models/` (running them again gives
the same files): `improve_weapons.py` (double shotgun, rocket launcher and, with
`improve_weapons_alt.py`, its alternate), `improve_weapons3.py` (shotgun, grappling hook: `v_grpple.mdl`
is now a source there too) and `make_holster.py`. The round's changes are separate functions in them.
Every frame is kept, every old triangle and vertex keeps its index (new parts are appended), and the
scripts check every anchor against the engine's strip order.

**Muzzle flashes: why the shotguns had none.** Quake draws a gun's muzzle flash as part of its view
model: fire-coloured, fullbright triangles collapsed into one point in every frame but the firing ones
(frame 1, `player_shot1` / `player_rocket1`, and for some guns frame 2). The nailguns, launchers and
lightning gun have them; Quake VR's `v_shot.mdl` and `v_shot2.mdl` had none (the only fullbright texels
on their skins are the sights). No code excludes them: nothing to fix in the engine. Both now carry a
flame (a twisted, spiky star cone out of the bore: one on the shotgun, one per barrel on the double
shotgun), full size in frame 1 and at a third in frame 2, collapsed at the muzzle otherwise, carried
with the recoil. Their texels use the fullbright indices the sights do not (240, 241, 243, 250, 254: white
core, yellow, gold, a red tip): `vr_sights.cpp` recolours 224..239, 252 and 253 on these two skins, so
the flash keeps its colour whatever the sight hue. The engine's muzzle flash light is unchanged.

**Shotgun** (`v_shot.mdl`, slot 2 in the cvars):
- *Ejection port* on the receiver's right side, in the plane of its upper side face, round x 14.5, z
  4.55: a raised, chamfered bezel (0.3 units) round a black opening with the bolt's steel face at its
  back. `vr_shells.cpp`'s port moves onto it: (14.5, -2.8, 4.3) -> (14.5, -2.75, 4.65), 0.7 units out
  of the opening's middle (its anchor 70, vertex 53 on the receiver's top edge, is unchanged).
- *Pump grooves in the geometry*: ribs 0.4 units proud round the pump's sides and bottom, between the
  three painted grooves and to its ends, so the grooves are real cuts. The ribs follow the pump's own
  edges in every frame and take its texels at the same place (the walls the grooves' dark rows).
- Anchors: hand 165, two-handed 48, muzzle 1, button 0, screen 159: unchanged.

**Double shotgun** (`v_shot2.mdl`, slot 3): the receiver, knuckle, breech face, top lever, caps and
guard are blued steel as dark as the barrels (palette 0/32..36 up to greys 5..7), with a soft highlight
along the top bevels and a little wear; the butt plate dark steel. The grip is painted as the shotgun's
(ribbed, the pump's browns), the wrist and the sawn end dark stained wood in the same browns. Geometry
unchanged but for the flashes. Anchors: hand 472, muzzle 13, two-handed 29, screen 0, shells 17.

**Rocket launcher** (`v_rock2.mdl`, slot 7): the back-blast nozzle's bell was lofted as if it were an
outside surface: its flat rim had an undefined winding, its inner wall faced outwards and its floor
faced into the gun, so from behind one saw through the mouth into the model. `open_mouth()` (with
`band()`) in `improve_weapons.py` builds a tube's open end with the rim and floor facing out of it and
the wall facing the axis; the rocket launcher (and its alternate, `v_multi2.mdl`, which the same
builder makes) use it. Bounds, offsets and anchors (hand 12, muzzle 17, two-handed 3) are unchanged.

**Grappling hook** (`v_grpple.mdl`, slot 18): the thin leaning stick is replaced by the grip the other
guns have (`GRIP_PROFILE`, ribbed in the launcher's browns, a steel butt plate) laid out in the drawn
fist, with the trigger guard and trigger (`GUARD_PATH`, `TRIGGER_PATH`); the stick folds inside the
grip. The grip, guard and trigger follow the body as it slides back when the hook flies. The hand and
the gun stay where they were. Anchors: hand 68, muzzle 90, others 0: unchanged.

**Holsters** (`legholster.mdl`, drawn at the hips and the upper holsters): the plate now curves round
the body (7 units radius, concave on the body's side), is tapered to a toe, and has a raised welt round
its outer face and stitching on both faces (the face that the player looks down on is the body side's).
The weapon loops run from the plate round the weapon and back into it, riveted at both ends (the old
clips' outer bars floated apart from them); the strap that rose into the air is a belt loop folded
over the plate's top. Dark oiled leather (Quake's darkest browns, 16..20 and 171..175, creases and
scuffs), darker reddish straps, blackened iron with rust flecks; no fullbright texels. The engine
scales the holster about the model's header origin: the new model keeps the old one's (-4.4, -1.02,
-5.2), and all of it lies inside those bounds, so the `vr_leg_holster_model_*` cvars place it as before.

**Settings.** The flashes widen the shotguns' bounds, so their offsets follow (slot 2 OffsetY 1.932775,
slot 3 OffsetY 2.164223); the grappling hook's grip reaches lower (slot 18 OffsetX -2.34475, OffsetZ
1.815691). `settingsVersion` 11 resets slots 2, 3 and 18 in existing configs once. The engine needs a
rebuild (the defaults and the shell port); the models none.

**Tested** with renders of the models (back faces culled, as the engine) and the mock in e1m1: the
shotgun's port and ribbed pump from its right side, its flash and the double shotgun's (with
`host_timescale 0.1` to catch frame 1), the launcher's mouth from behind, the grapple's grip in the fist,
and the holsters looking down with the body (`vr_body_mode 3`).

**In the headset:**
- Shotgun: the port reads as a recess on the right side and the shells leave from it (fire and pump,
  and a flick reload); the pump's grooves are cut in, not painted; the ring and post sights keep the
  sight hue and the flash does not change colour with it.
- Both shotguns: the flash at the muzzle when firing, size and colour next to the nailgun's and the
  launchers' (FLASH in `improve_weapons.py`; the lengths in `shotgun2_flashes` and `SHOT_MUZZLE`'s
  `flame` call).
- Double shotgun: the receiver as dark as the barrels, the highlights not too strong; the grip.
- Rocket launcher: look into the nozzle from behind: a dark mouth, no see-through.
- Grappling hook: the fist round the grip, the index finger in the guard; fire the hook: the grip
  moves with the body.
- Holsters: look down at the hips and the upper holsters (with and without the body): the curve, the
  loops attached, and whether the leather is now too dark to read (`leather()` bases in
  `make_holster.py`).

## Gore

Voice notes e1m1 14-24-48 (the player's dripping blood should mark the floor; knobs for how much and how often) and
14-25-16 (much more over-the-top gore, after Brutal Doom: big sprays on the walls, gibs stuck to the ceiling and
dripping, pools under corpses, big splats where gibs hit).

**One master setting and its knobs** (menu: Advanced VR Options > Gore, a new page after Body):

| Setting | Default | What |
|---|---|---|
| `vr_gore` | 2 | 0 Quake VR's blood as before, 1 more, 2 over the top |
| `vr_gore_spray` | 1 | how many splats a hit or a gibbing throws (0 none) |
| `vr_gore_size` | 1 | size of the gore's splats, pools and runs |
| `vr_gore_pools` | 1 | pools under corpses, gibs and bursts (0 none) |
| `vr_gore_drips` | 1 | how long and how much splats on the ceiling and hanging gibs drip (0 none) |
| `vr_gore_stick` | 8 | seconds (about) a gib flung into a ceiling or a wall sticks there before it falls (0 never) |
| `vr_body_blood` | 1 | the wounded arms' drip rate (a slider on the Gore page) |
| `vr_body_blood_amount` | 1 | how big the drops are and how much they splash |
| `vr_body_blood_floor` | 1 | while wounded (below 75 health), drops from the body round the feet: how often (0 none) |
| `vr_body_blood_marks` | 1 | the chance a drop marks the floor |
| `vr_body_blood_mark_size` | 1 | how big those marks are |
| `vr_decal_max` | 1024 (was 512) | also on the page, with the lifetime and the gib blood settings |

Note: an existing config keeps its archived `vr_decal_max` (512 in the author's): the gore makes many marks, so set
Max Decals to 1024 or more. Graphics presets don't touch any of this.

**What happens.** The QC sends events as `particle2` presets 40-42, far above the particle presets; the client
draws them as decals only (`vr_gore.cpp`).

- **Hits** (`VR_Gore_Hit` in `T_DamageImpl`: monsters and players, not damage from the world). Lines of blood are
  thrown on from the hit along the blow: a shot's own direction, a missile's flight, or away from an explosion.
  They spread in a cone (wider for shotgun blasts and hands' blows, a hemisphere for explosions) and fall a little.
  Over the top there are 3 + damage/8 lines (at most 10; half as many again for explosions), 96-200 units long. Each
  leaves a splat where it meets a wall, the floor or the ceiling; meeting nothing, it falls to the floor at the end
  of its flight. A glancing line leaves a spray drawn out along its way; one head on, a splat with spikes all round.
  A splat shows when the blood would arrive (later the further it flew). On a wall, half of them run down it (a run
  growing over 2-5 s, stopping at the floor). On the ceiling, a third of them drip for 2.5-6 s.
- **Gibbing** (`ThrowHead`) and **gibs bursting** (`VR_Gib_Burst`: shot, struck, or thrown hard at a wall). First a
  big splat along the blow (a gib thrown at a wall: on that wall). Then 14 lines all round, a quarter of them
  straight up (320 units, for the ceiling), leaving big splats and spiky splotches. Every ceiling hit drips, walls
  run, and a pool spreads under it over 4-7 s.
- **Corpses** (`VR_Gore_Killed` in `Killed`: a monster or player that died whole, not a swimming monster). 1.2 s
  later, once it has fallen, a pool spreads under the corpse over 12-18 s and darkens as it does. A second pool
  spreads off to one side a little later. The pool is about 4.5 times the monster's half width across (72 units for
  a grunt). None in liquids.
- **Gibs.** Flung from a body, they fly harder and higher (x1.3, and 150-250 up over the top). The QC watches them
  for their first 2.5 s (`VR_Gib_FlingThink2`) for a velocity turned by a ceiling or a wall. Quake's touches don't
  report it, and from their first touch they are rigid bodies. Such a hit sticks them there (75% of ceiling hits,
  35% of wall hits) for `vr_gore_stick` x 0.5-1.5 s. Shot, struck or knocked, they fall at once, and a hand can take
  them. The client sees a gib hang still with nothing under it (stuck, or held) and drips blood from it (up to 24
  drops). A gib coming to rest on the floor leaves a small pool spreading under it. A gib hitting the world (thrown
  or bouncing) leaves a spiky splotch 24-64 units across, which drips from a ceiling and runs down a wall.
- **Drips.** A drop is traced once, down, when it starts to fall: that gives where it lands and when. The falling
  drop is three particles (a short streak, lit as the place is). Where it lands it throws specks; the first few
  from a source leave marks, and the first starts a puddle spreading under it.
- **The player's wounds.** The drops from the arms (round 10's) now mark the floor as they land, with the chance
  and size in the settings. The marks were 1.5-3 units and at most one every 0.3 s; now they are 3.5-6.5 units, at
  most one every 0.08 s. The fallback asked for is there too: drops fall from the body round the feet (from about
  the waist, 4-11 units out: seen when looking down, never before the eyes). They fall 0.5, 1.1 or 2.2 a second by
  the wound skins, more after a hit.
- With `vr_gore` 0 the old behaviour stays: the particles' own splats, and no floor drips from the body.

**Decals now lie on the world's faces** (`clipToWorld`). Each mark's footprint is cut out of every face of the world
under it, as triangles. The faces are found down the BSP: those facing about its way, within a few units of its
plane (not sky, liquids or fences). Before, a mark that would hang over an edge was shrunk or dropped. The big gore
marks never fit on e1m1's panelled walls (all six wall hits of a shotgun blast were dropped). Now they lie across
panels and recesses and are cut where a face ends. This applies to all decals, scorches and chips too. Marks can
also show late, spread from their middle or from one end (runs), and darken as they dry.

**New marks in the atlas.** They are drawn at start-up in `vr_decals.cpp` as signed-distance shapes; the atlas is
now 8 x 4 cells of 256 (2048 x 1024).

- 4 sprays: a blot and droplets flung one way, some with tails.
- 3 runs down a wall: a blot and 2-4 wavering runs ending in drops.
- 3 pools, thick and dark in the middle.
- 3 splotches: a ragged blot, spikes ending in drops, drops round it.

`vr_decal_atlas` writes the atlas to `<gamedir>/decal_atlas.png`, as it looks on a grey wall. The blood marks are
redder than before: overlapping marks multiply, and many darkened to black.

**Budget.**

- Lines of blood are traced 24 a frame, at most 320 queued.
- At most 48 drip sources, 192 drops in the air, 32 pools to come, and 20 drop marks a second.
- Drops (from wounds, gibs and ceilings) take at most a quarter of `vr_decal_max`. The oldest drop goes first, not a
  pool or a splat.
- At most 64 new marks a frame, and 32 triangles a mark.
- Decals that no longer change are kept in their own vertex buffer, rebuilt only when marks come or go or one starts
  fading. Only the spreading, darkening and fading ones are rebuilt each frame.

**Measured** (mock, 960 x 540 window, e1m1, `host_maxfps 250`, `vr_profile 1`). Six rocketed grunts in the room at
944 1008 -248 made 766-814 decals, about 150 a gibbing (280 of them drops).

| | CPU (ms/frame) | GPU (ms/frame) |
|---|---|---|
| `decals` scope (the gore's work inside it), settled | 0.03 | 0.02-0.03 |
| the same while pools spread | 0.07-0.08 | 0.03 |
| the same before the static buffer | 0.07 | 0.03 |
| the whole frame, settled | 0.42-0.48 busy | 0.70 |

The frame is the same as before the carnage but for the blood particles for a few seconds (0.13-0.26 ms CPU, not
new). In the headset, with about 2.8 times the pixels, the decals' GPU time should stay near 0.1 ms.

**Tested in the mock.** `vr_gore_test [damage | burst | corpse]` makes one 64 units ahead; `developer 2` prints each
line of blood.

- A grunt shot with the shotgun: a spiky splat across the panelled wall behind it, the step and the floor, and the
  pool spreading under its corpse over 15 s.
- Rocketed grunts: the walls, the floor and the ceiling splattered, gibs stuck to a wall, drops falling from the
  marks on the ceiling (18 drip sources at once).
- A gib thrown at a wall: it bursts, with the big splat on that wall.
- Wounded to 20 health: drops round the feet marking the floor (40 in the 3 s after the hit).

**In the headset:**

- Shoot grunts next to walls with each gun: the sprays behind them, their sizes, whether it's too much or too
  little (Blood Sprays, Splat Size).
- Rocket or gib monsters in low rooms: gibs stuck to the ceiling and walls, the drops falling from them and from the
  splats on the ceiling, the puddles under them. Are the drops visible enough? They're particles 1-1.5 units across.
- Throw gibs hard at walls and the ceiling: the big splat where each hits.
- Watch a corpse's pool spread and darken (about 15 s).
- Get hurt below 75, 50 and 25 health and look down: the drops round your feet and their marks (Drips Round Feet,
  Drops Mark Floor, Floor Mark Size), and the arm drips' marks.
- Blood on panels, steps and edges: marks are cut to the faces now. Look for floating or missing pieces.
- Set Max Decals to 1024-2048 and check the frame time in a big fight (`vr_profile 2`).

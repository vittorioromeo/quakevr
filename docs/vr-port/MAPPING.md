# Making maps for Quake VR with TrenchBroom

TrenchBroom has no plugins: Quake VR is a **game configuration** ("Quake VR") with its own **entity definitions**
(`quakevr.fgd`), generated from the QuakeC so that every entity the mod can spawn is in TrenchBroom's entity
browser with help, key types, choices and a model preview.

## Your steps (the author's)

1. **Close TrenchBroom** (it rewrites `Preferences.json` when it quits).
2. **Install** from your checkout: `powershell -ExecutionPolicy Bypass -File Misc\trenchbroom\install.ps1`
   It refuses to run while TrenchBroom is open, backs up `%APPDATA%\TrenchBroom\Preferences.json`
   (`Preferences.json.<date>.bak`), copies `Misc\trenchbroom\QuakeVR\` to `%APPDATA%\TrenchBroom\games\QuakeVR\`, and
   sets only the four `Games/Quake VR/...` keys (your "Quake" game keeps its settings). It finds by itself:
   - the game path: the Steam Quake with a `quakevr` folder (`C:\Program Files (x86)\Steam\steamapps\common\Quake`);
   - the tools: `C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64\qbsp.exe`, `vis.exe`, `light.exe` (2.0, not the old
     0.18.1 your Quake game points to);
   - the engine: the `ironwail.exe` of the checkout that `Quake\quakevr` links to.
   It prints what it set. Options: `-Quake <folder>`, `-EricwTools <folder>`, `-Engine <ironwail.exe>`, `-Force`
   (replace compile and engine profiles you have edited). Run it again after pulling a change to the FGD or the config.
3. **Start TrenchBroom**, *New map*, pick **Quake VR** (the headset icon), format **Valve**. If it asks for the game
   path, check *Preferences > Games > Quake VR*: Game Path = the Quake folder above; under *Compilation Tools* qbsp, vis and
   light = the three 2.0 exes. Nothing else to set there.
4. The new map starts with a floor, a player start, the mods **hipnotic, rogue, quakevr** (*Map > Mods*, or the
   worldspawn key `_tb_mod`: the engine layers the mission packs under quakevr the same way) and the Quake VR texture
   WAD (`wad` = `quakevr/wads/quakevr_dev.wad`, relative to the game path). Check in the **console** (the Info panel at the bottom of the window,
   its *Console* tab) that there is no red line: no "Could not load wad", no FGD error, no "Could not get
   entity model".
5. **Entities**: the entity browser (right panel, *Entity* tab) lists them all; the Quake VR ones are
   `func_worldtext_banner` (text boards), `func_weapon_grabbable` (a weapon lying about), `func_weapon_dispenser`,
   `func_enemy_dispenser`, `func_particle_emitter` (point entities despite their names), `vr_dummy` (training dummy)
   and `weapon_shotgun`; `func_button` has the label and command keys. Select one to read its help at the bottom of the
   entity inspector; keys with choices get a drop-down, spawnflags get check boxes.
6. **Open the example**: `quakevr\maps\vrexample.map` (in your checkout; TrenchBroom finds its WAD through the game
   path). Look at the weapon models on the `func_weapon_grabbable`s (change `weapon` and `weaponflags` and the model
   should follow), the ammo boxes (tick "Large box"), the dispensers.
7. **Compile**: *Run > Compile Map*, profile **Quake VR: full (ericw-tools 2.0)** (or **fast** while blocking out). It
   exports the map, runs qbsp, vis and light with Quake VR's settings, and leaves `<map>.bsp`, `.lit` and `.lux` in
   `<Quake>\quakevr\maps` (the checkout's `quakevr\maps`). **Run**: *Run > Launch*, **Quake VR (headset)** or
   **Quake VR (flat screen)**.
8. **Report back**: any red line in TrenchBroom's console panel (with the entity or file it names), any entity whose
   model is missing or wrong, the compile output if a tool fails, and anything that looks wrong in game.

## What is where

| File | What |
|---|---|
| `Misc/trenchbroom/QuakeVR/GameConfig.cfg` | the game ("Quake VR"): TrenchBroom 2026.2's Quake config (version 9) with its own name, icon, FGD and initial maps |
| `Misc/trenchbroom/QuakeVR/quakevr.fgd` | the entity definitions, **generated**: do not edit |
| `Misc/trenchbroom/QuakeVR/initial_valve.map`, `initial_standard.map` | a new map: mods, WAD, floor, player start |
| `Misc/trenchbroom/QuakeVR/CompilationProfiles.cfg`, `GameEngineProfiles.cfg` | profile templates (the installer fills in the engine's path) |
| `Misc/trenchbroom/QuakeVR/Icon.png` | the game list's icon (drawn by `make_assets.py`) |
| `Misc/trenchbroom/fgdgen.py` | generates `quakevr.fgd`; `--check` is the build's check |
| `Misc/trenchbroom/entities.fgd` | the hand-written definitions merged into it (help, types, choices, previews) |
| `Misc/trenchbroom/vendor/Quake.fgd` | TrenchBroom's own Quake.fgd, unchanged: the id entities' base definitions |
| `Misc/trenchbroom/make_assets.py` | draws `Icon.png` and `quakevr/wads/quakevr_dev.wad` |
| `Misc/trenchbroom/install.ps1` | the installer |
| `quakevr/wads/quakevr_dev.wad` | Quake VR's own textures for mapping |
| `Misc/trenchbroom/make_id_wad.py` | makes `quakevr/wads/id_textures.wad` (git-ignored) from your own `id1` paks: every texture of id's maps |
| `quakevr/maps/vrcalibration.map` (`.bsp`, `.lit`, `.lux`) | the VR Calibration room (`Misc/quakevr/make_vrcalibration_map.py`; id's textures) |
| `quakevr/maps/vrtesthall.map` (`.bsp`, `.lit`, `.lux`) | the test hall: the old calibration room's setting buttons, pool and climbing (`Misc/quakevr/make_vrtesthall_map.py`) |
| `quakevr/maps/vrexample.map` (`.bsp`, `.lit`, `.lux`) | the example map and its compiled files |
| `quakevr/maps/vrclimb.map` (`.bsp`, `.lit`, `.lux`) | the climbing test map (`Misc/quakevr/climb/make_vrclimb_map.py`) |
| `quakevr/maps/vrslipgates.map` (`.bsp`, `.lit`, `.lux`) | the slipgate test map, id's textures (`Misc/quakevr/slipgates/make_vrslipgates_map.py`; ROUND21.md, "Slipgate test map") |
| `quakevr/maps/vrstart.map` (`.bsp`, `.lit`, `.lux`) | the island hub at night (`Misc/quakevr/maps/vrstart_gen.py`, its geometry library `mapgeom.py`; the sky box `make_vs2_sky.py`, `quakevr/gfx/env/vs2night*.png`): below |
| `Misc/trenchbroom/make_id_wad.py` | writes `quakevr/wads/id_textures.wad` (git-ignored) from your paks: above |

## The entities

`quakevr.fgd` has **244** entities: every spawn function of the QuakeC (237: 102 of id's Quake, 65 of the Scourge
of Armagon, 38 of the Dissolution of Eternity, 24 of Honey, 8 of Quake VR's own) and 7 compiler entities (func_group,
func_detail and its ericw-tools variants, func_illusionary_visblocker, misc_external_map). Six QC functions look like
spawn functions but are not entities (`//! internal` in `entities.fgd`, with the reason).

**Quake VR's own:**

| Entity | What | Keys |
|---|---|---|
| `func_worldtext_banner` | a floating text board (a small screen); point entity | `worldtext` (`\n` new line, `$` new page), `worldtext_halign` (left, centre, right), `worldtext_scale` (letters are 8 units x this), `speed` (seconds per letter before the next page: needed with pages; 0.085 reads well), `angle` (the way it faces; it turns round for a reader behind); `{menu:<page title>}` (or `{menu:<title>><row label>}`) in the text shows that VR Settings page's path from the main menu as the menus are ("Options > VR Settings > ... > Locomotion"; `vr_menu_path_check` lists them and flags one gone: ROUND21.md, "VR Calibration") |
| `func_weapon_grabbable` | a weapon as a physics object: it drops, settles, is gripped, holstered, thrown, force-grabbed; no ammo | `weapon` (1 hook, 2 axe, 3 Mjolnir, 4 shotgun ... 12 laser cannon, 13 sword, 14 chainsaw: a full tank, 15 a grunt's shotgun, 16 an enforcer's laser rifle: full, of their own ammo, 17 the crowbar), `weaponflags` (1: the lava nailguns, multi launchers, plasma gun, the hell knight's sword) |
| `func_weapon_dispenser` | throws out a new weapon when triggered | `targetname`, `weapon` |
| `func_enemy_dispenser` | spawns a monster when triggered (vrfiringrange's buttons); refuses, with a message, a monster whose game is not installed | `targetname`, `weapon` (0 grunt ... 17 Overlord), `angle` |
| `func_particle_emitter` | smoke, forever | |
| `vr_dummy` | a training dummy (a grunt) that cannot be hurt and shows every hit's damage | `angle` |
| `weapon_shotgun` | the shotgun as a pickup (id's Quake has none) | |
| `weapon_crowbar` | a crowbar lying ready to be picked up (a melee weapon, one hand or two): `func_weapon_grabbable` with weapon 17; QC places one with `VR_Crowbar_Spawn` | `angles` (how it lies before it falls; none: any way) |

**Climbing (what counts as a ledge).** With Climbing on, a hand takes hold of a ledge (ROUND21.md, "Ledge map"; the
engine finds them when the map loads, `vr_debug_ledges 1` shows them):
- the lip of a walkable top (a drawn face at most about 45 degrees from flat; not sky, not a liquid's surface) with open
  space beyond it and under it, room over it for a hand (8 units), and a **drop of at least 32 units** within 16 units
  out from the lip. A trim or a step under the lip is passed over on the way out (a moulding, the next rung down); a
  wall within 16 units at hand height (2 over the top) means no drop.
- so: ledges, window sills over a drop, beams, rungs (a rung against a wall: its front and its ends), a wall's top.
  Not: floors, stairs (their treads come every 8 to 16 units down), the edge of a step, a sill over a floor within 32.
- clip brushes don't count (they aren't in the hands' collision); `func_illusionary` doesn't either (not solid); sky
  and liquid tops don't.
- brush entities (`func_plat`, `func_train`, `func_door`, `func_wall`...) have their own ledges, which move with them:
  a hand holds on and rides it. A ledge on one only counts where the world (or another brush entity) doesn't fill its
  drop now: a plat level with the floor is no ledge. Rotating brushes don't rotate their ledges (Quake's collision
  doesn't rotate brush models; the mission packs' rotating things collide through `func_movewall` boxes, which move).
- the mantle needs the player's box (32 by 32, 56 tall) to fit on the top: a top at least 3 units deep with the box's
  middle over it, and room above.
- Lowest Ledge (`vr_climb_min_height`, 30 units over the feet) is the player's setting: ledges lower than that aren't
  taken.

**What Quake VR changed in id's entities** (all in their help):
- `func_button`: pressed by a hand or a weapon (vertical ones also by stepping on them); a label (`worldtext`,
  `worldtext_halign`, `worldtext_scale`, drawn on its face); `buttonEffect` 3 runs its `targetname` as a console
  command (vrstart_old's hub buttons; end it with `\n`); Honey's "Starts disabled" flag and `items`.
- Weapons, armour, keys, powerups: they float at torso height (`vr_item_float_height`, 26 units) and are taken by hand:
  a weapon by a grip, armour by letting go of it over the torso, keys and runes at a holster. Place them on the floor.
- Ammo and health boxes: physics objects (carried, thrown, nudged, force-grabbed), taken at a holster or by walking
  over them.
- Knights and hell knights always drop their swords (not statues); ogres always drop their chainsaws; grunts their
  shotguns and enforcers their laser rifles.
- **Rocks and bricks** (ROUND21.md, "Rocks and bricks"): at map load, loose rocks and bricks are placed at the foot of
  walls by what the textures are named (grass, dirt, ground, `rock<N>_*`: rocks; `*brick*`, `city2_*`: bricks; the full
  table is in ROUND21.md), clear of doors, lifts, teleporters, triggers, items, monsters and starts, the same places at
  every load. The worldspawn key **`_vr_debris`** (Rocks and bricks) turns them off for a map (`0`) or scales their
  chance (`0.5` half as many places, `2` twice); name your textures so (a grass floor `grass_...`, a brick wall
  `..._brick...`) and they come by themselves. Single player only.
- **Wooden crates** (ROUND21.md, "Wooden crates"): at map load, small and large crates (sometimes two stacked) are
  placed by walls and in corners, clear of every entity, with open floor in front of them (a passage stays passable),
  the same places at every load; they break into light pieces, may hold a small box of ammo or health, and hide the
  player from monsters. The worldspawn key **`_vr_crates`** turns them off for a map (`0`) or scales their chance. Place
  your own with **`vr_crate`** (spawnflag 1: LARGE; `angle`, `skin` 0-2). Single player only (placed ones everywhere).
- Monsters and items: Honey's trigger-spawn flags (appear when triggered, teleport fog, angry, several copies with
  `cnt`, remove corpse, silent wake-up...). Items' "Floating" and "Secret" flags only work on Honey's maps and are not
  listed.
- The mission packs' entities come with their help and a note that their models and sounds come from hipnotic or
  rogue; Honey's with a note that Quake VR does not ship Honey's files (`misc_tree`'s model is missing: the map would
  not load without Honey).

## VR scale

- **1 metre = 32.8 units** at the default `vr_world_scale` 1.25 (26.25 units x the scale). The player's collision
  box is Quake's (32 x 32 x 56, the origin 24 units above the floor): corridors and doors as in Quake, but in VR the
  player sees them at real scale, so rooms feel small: give them height (the example's ceiling is at 160, ~4.9 m).
- **Hand reach** is the player's real arm: roughly 0.6-0.7 m (20-23 units) from the body. A hand takes a box, a weapon or
  armour when the fist touches it (`vr_carry_grab_bias` 0 cm). Put buttons and things to grab **16-48 units** above the
  floor (0.5-1.5 m: waist to chest), and within an arm's length of where the player's body can stand (the box's
  edge is 16 units from its centre).
- **Force grab** pulls things from up to `vr_forcegrab_distance` (200 units, ~6 m) away.
- **Pickups float** at `vr_item_float_height` (26 units) above the floor they drop to.
- **Text**: letters are 8 units x `worldtext_scale`: 0.3 (2.4 units, ~7 cm) is readable at arm's length, 0.45 across a
  room. Place boards at eye height (about 50-60 units above the floor) or a little above.
- The **example map** is laid out with these numbers: a 64-unit floor grid (16-unit lines) to judge distances, a
  40-unit barrier with buttons on its face at 8-40, a 32-unit table.

## Textures and id's data

id's textures, models and maps are not distributed: nothing of them is committed. Maps reference them from your own
install (models through the game path and the mods; textures from a WAD you add). A compiled `.bsp` **embeds** its
textures, so a committed `.bsp` must only use textures we may distribute. Hence `quakevr/wads/quakevr_dev.wad`: 14
textures drawn by `Misc/trenchbroom/make_assets.py` (grids at Quake VR's scale, trim, panel, a glowing strip, a
button, water, the calibration spot `qvrc_pad`, and qbsp's clip, skip, trigger, hint and origin), mapped to Quake's palette read from your
`id1/pak0.pak` (the palette itself is not committed; the WAD holds indices). `vrexample.bsp` embeds only these.
For your own maps you may add id's textures (a WAD of your own, as `C:\OHWorkspace\TrenchBroom\Q.wad`): then the
`.bsp` carries id's textures and should not be committed or shipped. **The one exception** (the author's decision,
round 21): the VR Calibration room, `vrcalibration.bsp`, is built from id's base textures and committed with them.
Its WAD is made from your own paks, never committed: `python Misc/trenchbroom/make_id_wad.py` writes
`quakevr/wads/id_textures.wad` (every texture of id1's maps, git-ignored; the same bytes from the same paks), and the
map's worldspawn names both WADs (`quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad`).

**id's textures, built from your paks** (`Misc/trenchbroom/make_id_wad.py`): `python Misc/trenchbroom/make_id_wad.py
[--quake <Quake folder>] [--list]` reads every map in `id1`'s paks (and `hipnotic/pak0.pak`, `rogue/pak0.pak` when you
have them), takes each texture they embed (by name: id1's first, then the mission packs') and writes them as they are
(8-bit, their mip levels) to `quakevr/wads/id_textures.wad`: 893 textures, 8.4 MB. The WAD is **git-ignored**: id's
data never enters the repository. A map lists it in its worldspawn `wad` before Quake VR's own
(`quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad`); qbsp finds both with `-wadpath <checkout>`. The
compiled `.bsp` then embeds id's textures: **the author's decision is that the hub maps may ship so** (vrstart:
`Misc/quakevr/maps/vrstart_gen.py`). Keep that to the maps that need it. `--list` prints each texture, its size and
the map it came from (find names there; TrenchBroom shows them once the WAD is built).

The WAD path in the worldspawn is **relative to the game path**: TrenchBroom looks for it there (and beside the map);
the compile profiles pass `-wadpath <game path>` to qbsp. It resolves through `Quake\quakevr` (the link to the
checkout's `quakevr`). `make_assets.py` makes the same bytes every time (fixed seeds).

## Compiling

The profiles (`CompilationProfiles.cfg`) work in `<game path>\<last mod>\maps` (`quakevr\maps`): the `.bsp`, `.lit` and
`.lux` land where the game loads them, and the temporary `<map>-compile.map`, `.prt` and `.json` files are deleted.
**Full**, as for the committed maps (`relight_maps.py`'s look, and its light grid):

```
qbsp  -nolog -nopercent -wadpath "<Quake>" <map>-compile.map <map>.bsp
vis   -nolog -nopercent <map>.bsp
light -nolog -nopercent -extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -lit -lux -lightgrid -lightgrid_dist 64 64 64 <map>.bsp
```

qbsp compiles liquids see-through by default (`-transwater`), so vis leaves water see-through; the engine loads the
`.lit` (colour), the `.lux` (light directions, `vr_deluxemap`) and the light grid. **Fast** skips vis and uses plain
`light -lit -lux`. Keep entity values under 127 characters (qbsp warns; split long banner texts).

To rebuild the example from the checkout (what was committed):
`cd quakevr\maps`, copy `vrexample.map` to `vrexample-compile.map`, run the three commands with
`-wadpath <checkout>` (the WAD path is relative to a folder that has `quakevr\`), delete the temporary files.

## Adding an entity

1. Write its spawn function in the QuakeC. Give it a `/*QUAKED name (r g b) (mins) (maxs) FLAG1 FLAG2 ... help */`
   comment (`?` in place of the sizes for a brush entity): colour, size, flags and help come from it.
2. If it has no QUAKED comment, or to do better (key types, choices, a model preview, better help), add a definition to
   `Misc/trenchbroom/entities.fgd` (FGD syntax; it is merged into the generated one, key by key). A model preview
   that depends on keys uses TrenchBroom's expression language: `&&` and `||` take Booleans only, so test a bit as
   `(spawnflags & 1) == 1`, a key as `weapon == "6"` (keys are strings; unset keys are null).
3. `python Misc/trenchbroom/fgdgen.py` regenerates `quakevr.fgd`; `--report` lists the keys spawn functions read that
   no definition documents; `--verify-models <Quake folder>` checks every model path in the paks and folders.
4. Run `install.ps1` again (TrenchBroom closed).

**The check.** `QC/build.sh`, `QC/build.bat`, `Windows/package-quakevr.ps1` (and the agent kit's `build.sh`) run
`fgdgen.py --check` after compiling the QuakeC (0.3 s; skipped without Python; `NO_FGD_CHECK=1` skips it). It fails when
a spawn function has no definition, when `quakevr.fgd` lacks an entity or has one the QuakeC no longer spawns, when a
definition in `entities.fgd` names no spawn function, and when the FGD or the game config is broken: classes defined
twice, unknown base classes, missing help, repeated keys, spawnflags that are not single bits, and every `model()`
expression evaluated the way TrenchBroom does (types included) for each value of the keys it reads (unset, each
choice, the default, each spawnflag, pairs, all). It only warns when `quakevr.fgd` is merely out of date (help or keys
changed): run the generator then.

A spawn function is a `void()` function that is not a frame function and has a QUAKED comment, or whose name has a
map entity's prefix (`info_`, `item_`, `weapon_`, `monster_`, `func_`, `trigger_`, `light`, `misc_`, `path_`,
`ambient_`, `trap_`, `vr_`...), is never used as a value (think, touch, use...) and has no helper suffix (`_think`,
`_use`...). A helper that still looks like one goes in `entities.fgd` as `//! internal <name> <reason>`.

## vrstart: the island hub (a generated map)

`quakevr/maps/vrstart.map` is written by `Misc/quakevr/maps/vrstart_gen.py`: **edit the script, not the .map** (it
rewrites it). The .map opens in TrenchBroom (Valve format; each part a group: the terrain's three func_detail groups,
each structure a func_detail, and the things spread over the map, pines, boulders, torch posts, crystals,
func_details by 768-unit tile; ropes and brackets func_detail_illusionary, not solid; the barrels `vr_barrel` physics
props; the structural world is only the sealing box and the water)
for looking, measuring and trying things; carry what you keep back into the script.

```
python Misc/trenchbroom/make_id_wad.py                 # once: id's textures from your paks (git-ignored WAD)
python Misc/quakevr/maps/vrstart_gen.py               # the .map (about 10 s)
python Misc/quakevr/maps/vrstart_gen.py --compile --preset fast    # iterating: vis -fast, plain light (~1.5 min)
python Misc/quakevr/maps/vrstart_gen.py --compile     # the shipped build (preset final: full vis, light -extra4
                                                       #   -dirt -bounce, the light grid): ~8 min
python Misc/quakevr/maps/vrstart_gen.py --compile --check 1000000  # and the hole test over the result
python Misc/quakevr/maps/bsp_holes.py quakevr/maps/vrstart.bsp --rays 300000   # the hole test alone, any BSP
python Misc/quakevr/maps/make_vs2_sky.py               # the sky box (quakevr/gfx/env/vs2night*.png, committed)
```

**Compiling**: qbsp is **ericw-tools 0.18.1's** (`-bsp2 -splitturb`; `--qbsp`), vis and light 2.0's (the author's
decision). 2.0-alpha11's qbsp makes faces from its BSP's portals and fills through them; on this map, even with the
geometry cleaned up for it (below), a few portals failed in every build: missing faces and air made solid, 0-9 holes in
600,000 rays and moving with any edit. 0.18.1's makes faces by CSG: 0 holes in a million rays. `-splitturb` cuts the
water's faces to lightmap size and leaves them lit (no patch needed). The presets (`--preset`, `--help`): **fast**
(vis -fast; light -lit -lux and a 128-unit light grid: no ambient occlusion, bounce or extra samples) and **final**
(full vis; light -extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -bounce -lit -lux -lightgrid 64). The shipped .bsp is a
final build. `bsp_splice.py` (hull 0 from one compile, the clipping hulls from another) is from the 2.0 attempt, unused.

**What the geometry avoids** (kept from the 2.0 attempt: fewer brushes, and 2.0 could be tried again; 2.0 makes faces from portals: a portal whose brush side it
can't find gets none, "N sides not found"; a sliver thinner than its epsilons breaks its portals): no two faces nearly
but not exactly coplanar, no corner a fraction of a unit through another brush's face, no nearly straight runs of
terrain edges. The generator ensures it (`mapgeom.py`): `terrain_mesh` (neighbouring tops exactly coplanar or at least
1.5 degrees / a unit apart, corners moved onto each other's planes, the island's walkable ground left as it is; tops
within 0.6 degrees of level made level; corners near the water's surface put on it, the clearance 4 units across the
slope); `unbend` (the terrain's points moved off nearly straight lines of edges); `hull` (a brush's faces within 3
degrees of each other merged, faces within 1.5 degrees of an axis turned to it, corners within 1.5 units of the ground
or the water's surface moved clear of it); pines' cones end inside the cone above (they shared a tip); boulders' flat
undersides buried 4 units; ropes in two pieces. Exactly coplanar terrain triangles of one texture are merged into convex
prisms. Check a build with `--check` (or `bsp_holes.py`): rays from random open points; a hit where the contents change
with no face there is a hole.

- **Layout** (x east, y north, the water's surface at z 0): a lake 8000 units across ringed by cliffs and mountains,
  the island in its middle (about 3100 x 2200), and the path from the south-west: the pier (the player's start) ->
  the arrival beach (welcome board, VR TUTORIAL and VR CALIBRATION buttons, a campfire) -> a staircase up the bank ->
  the campaign terrace (four lecterns, the slipgate) -> a bridge over the ravine -> the settings pavilion (19
  `vr_setup_option` buttons, each with its value screen) -> a staircase down -> the firing range (guns and
  ammunition on the benches; boards, crates, a dummy, an explosive box, rocks on a shelf) -> the shore path -> the
  lookout tower (a ladder of rungs at vrclimb's heights; a diving board over deep water). The places are dicts at the
  top of the script (`PIER`, `TERRACE`, `GATE`, `BRIDGE`, `PAVILION`, `RANGE`, `TOWER`, `STAIR1`, `STAIR2`).
- **Terrain**: a height function (the island's coast spline, hills, the flattened places and paths, the ravine, the
  lake's floor, the cliffs' ring), sampled on a jittered lattice (64 units on the island, 128-512 further out) plus
  the places' and paths' outlines, simplified away from the island (`simplify_points`: a point is left out where the
  surface without it is within 3 units on the lake's floor, 2 on the cliffs, 6 on the mountains), triangulated
  (Delaunay, exact integer arithmetic), cleaned for qbsp (above) and built as convex prisms down to z -1024 (the tops
  written as exact reals, 6 decimals). Textures by slope and height: rock5_2/rock3_8 on
  slopes over 44 degrees, grass1_1/ground1_2 on the island,
  ground1_8 on the paths, rock3_2 on the beach, ground1_5 under water.
- **Campaigns**: the lecterns run `vr_activestartpaknameidx 0/1/2` (the slipgate, a `trigger_changelevel` to
  `start` without intermission, starts that one; SELECTED shows over the chosen lectern) and `vr_campaign_select dopa`
  (Dimension of the Past starts at once).
- **The hub**: it is `vrstart`, the hub VR starts in and the menus' VR Hub returns to (it was `vrstart` until
  2026-10-07: `map vrstart`, a config's `vr_hub_map vrstart` (vr_cfg_version 99 moves it) and old saves made there
  still load it, `VR_MapAlias`). The old hub is `vrstart_old` (`vrstart_old.bsp`, `.ent`): Debug > Tests > Hubs, or
  `vr_hub_map vrstart_old` to make it the hub again.
- **Checks**: `vr_menu_path_check maps/vrstart.map` (the boards' `{menu:...}` names: 4 found, 0 missing).

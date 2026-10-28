# Round 23: after 1.1.0-beta4

Notes for the work after the 1.1.0-beta4 prerelease (2026-10-27, commit c9aedff6). Round 22 (`ROUND22.md`) covers
1.0.1 to 1.1.0-beta4; grep it for anything older. Append new sections at the end of this file, one per shipped task:
what changed, how it was verified (numbers), settings and their defaults, what to test in VR, decisions.

## Survival Arena: the map

*Survival mode needs its own map (`vrarena`): the space its waves of armed humanoids fight in. The wave code is the
next task; this one is the map and the menu row that reaches it.*

**What changed.** `Misc/quakevr/maps/vrarena_gen.py` writes `quakevr/maps/vrarena.map` (130 brushes, 40 entities)
and compiles it. Play > Maps gained "Survival Arena" -- `command("Survival Arena", "map vrarena")` with a help line,
in `pagePlay()` in `Quake/vr/vr_menu.cpp`, after the Melee Recording Room.

The room is an octagon 1728 units across (49 m at 32.8 units to the metre), 800 high with its corners cut 200, the
floor at 0, walls 64 thick out to 864 and a ceiling at 800: a solid shell, so the arena is sealed and the fight
happens in the middle of it. In it:

- a central dais, 232 to its flat sides and 96 (2.9 m) high, climbed from the north, the south, the east and the
  west -- six steps each, 16 high and 24 deep -- and the player starts on top of it (`info_player_start` 0 0 120);
- four corner towers, 180 square and 256 (7.8 m) high at the four cut corners, each climbed by an outer pinwheel
  staircase of sixteen steps, 16 high and 16 deep, hugging the tower's faces;
- a wall walk at 256: a 128-wide deck along the east and the west wall (y -576..576) joined to the tower tops it
  passes by 64-wide bridges, so the high ring is reached only through the towers and a step off it is a 256 drop;
- eight covers, 48 high and 80 wide, 520 out at 45 degrees apart -- (480, 200), (200, 480) and their mirrors -- each
  its own `func_detail`, so sight lines break up but the open ring stays walkable;
- 24 lights: 8 ceiling lamps (a 64-square `qvr_light` block flush with the ceiling) with a light 300 and falloff
  0.47 forty units under each; one of 180 over each tower top and 160 over each walk landing; 150 in each corner of
  the floor and 140 over the dais, one facing each staircase; worldspawn `light 90`.

12 `info_arena_spawn` points carry a `kind`: 1 for the four ground points (640, 640, 0 -- the corners of the open
ring) and 2 for the eight high ones (the tower tops at 402 402 256 and the walk landings at 736 +-288 256). 3
`info_arena_item` points sit at 196 196 48, -196 196 48 and 196 -196 48. Worldspawn: `quakevr_dev.wad`, `mapname
vrarena`, `message "Quake VR Survival Arena"`, `worldtype 2`, `_tb_mod`, `_vr_debris 0` and `_qvr_prelit 1`.

**Verified.** `python Misc/quakevr/maps/vrarena_gen.py --check` reads the map back and counts it: 12 spawns (8 kind
2, 4 kind 1), 3 items; 88 stair steps, 0 unclimbable (every step's rise over the tread before it is at most 16 and
its depth at least 16); 130 brushes with 0 duplicates and 0 running into another -- a separating-axis test over
their faces' normals, so brushes meeting face to face, as `MAPPING.md` asks, do not count -- and 0 feature brushes
standing in the open ring. It also reads off the brushes what each point stands on: the ground spawns have nothing
to fall (0), the high ones fall 256, the player start 96. `arena_map: 12 spawns, 3 items, 0 bad steps`, `arena_check:
done`.

Compiled with ericw-tools 2.0.0-alpha11 (qbsp `-bsp2 -wadpath <worktree>`, then vis, then light `-extra4 -dirt
-dirtscale 1.5 -dirtdepth 96 -bounce -lit -lux -lightgrid -lightgrid_dist 64 64 64`; the fast preset ran first):
2209 world faces, 678 leaves, no leak, no missing-texture line -- the only qbsp warnings are "No entities in empty
space -- no filling performed" for hulls 0/1/2, which is what a fully solid shell gives. `bsp_holes.py` sent 400128
rays: 0 missing-face hits, 0 undrawn hits, 0 unlisted faces, 0 holes.

`build.sh arenamap` says `built`: the QC with 0 warnings, fgdgen covering every spawn function (374 entities), the
statics, test-number and QC-precedence checks passing. A headless run of that build -- `map vrarena; wait120;
screenshot; quit` -- exited 0 with "map vrarena spawned" in the console and no error, Host_Error, SV_Error or
Missing lines.

**Settings and defaults.** Nothing to set: no cvars, no QuakeC yet. The map carries its own choices as worldspawn
keys, and `_qvr_prelit 1` keeps the shipped relight batches from re-lighting it -- `vrarena.map` with its
`.bsp/.lit/.lux` is in `quakevr/maps`, all of it rebuilt by the generator.

**To test in VR** (it is a space only: nothing spawns in it yet). From Play > Survival Arena: the view from the dais,
towers, bridges and covers in view across 49 m; down a dais flight and up a tower staircase -- the 16-deep treads
are the climbable minimum, so watch for a foot or a controller snagging; across a bridge onto the wall walk and look
down the 7.8 m drop, and step off a cover-height ledge to see how a fall feels; walk the open ring (700..1000 out)
and check the covers break sight lines without ever blocking the way round; check the lamps read as the light's
source and the lightgrid gives an even floor.

**Decisions.** The brief put `info_player_start` at 0 0 0, but the dais fills the centre, so the start sits on its
top at 0 0 120 -- floor plus the house's 24. The same brief's "(4 kind 2)" contradicts its own layout (4 tower tops
and 4 walk landings), so the map has 8 kind 2 and 4 kind 1 and `--check` prints the breakdown. Every brush touches
its neighbours face to face and overlaps none, so no qbsp merging can alter a step or a ledge; that is what the
interpenetration test holds. "The floor annulus is free of solid brushes" was read as the useful version of the
rule: no *feature* -- dais, stairs, towers, decks, bridges, covers -- stands between 700 and 1000 out below 160
high; the shell is the room, and the wall walk starts at 192, above the ring. qbsp is ericw-tools 2.0.0-alpha11 with
`-bsp2`, the tool vrtesthall and vrcalibration use, with `quakevr_dev.wad` alone (this checkout has no
id_textures.wad). Spawn points sit 24 above their floor like every other entity.

*Open for the wave code (W2):* how kind 1 and kind 2 are used, whether 12 points suit its wave sizes, and whether
the 3 `info_arena_item` points are where its drops should land.

*Suggested `quakevr/checklist.txt` line:* `+ Survival Arena: vrarena, the arena's own map (generator, compiled, Play
menu row)`

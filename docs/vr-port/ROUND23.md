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

## Survival Arena: the wave director

*The mode: waves of armed humanoids that get thicker and quicker, Quake's own monsters joining as the waves go, a boss
wave every fifth wave, and a player who starts with nothing and climbs the weapon ladder off the bodies.*

**What changed.** `QC/vr_arena.qc` (1022 lines) is the director. `world.qc` starts it at world spawn
(`VR_ARENA_WorldSpawn()`), `combat.qc` tells it about a kill (one line in `Killed()`, next to `MGH_Kill`), and the Play
> Survival Arena row from the last task is how it is reached. Nothing runs unless the map is `vrarena` or `vr_arena` is
set, deathmatch is left alone, and the mode adds no engine code: it is QuakeC on top of the armed humanoids, the AD
weapons and the grabbable items that already exist.

*The plan.* A wave is written down before it is sent (`VR_ARENA_Plan`): 12 recipes for waves 1 to 12, then one rule
(wave 13 and on: the same ladder, thicker -- 1 to 7 humanoids, 1 to 3 enforcers, 1 to 3 of Quake's own monsters, plus
the boss). Wave 1 is one or two grunts, all unarmed. Wave 2 is an unarmed grunt and one with a nail pistol. Wave 3
brings 3 or 4 grunts with the bottom rungs of the ladder and the first zombie. After that the classics arrive at their
waves and never before: zombies 2, knights 4, dogs 6, ogres 8, fiends 12, death knights 16, each wave cycling the
kinds unlocked by now. The humanoids' guns are rolled per wave from the rungs it has reached -- axe, shotgun and nail
pistol until wave 10, super shotgun, nailgun and super nailgun from wave 10, rocket launcher, lightning and grenade
launcher from wave 18 -- and mixed within a wave, so a wave's five humanoids do not all carry the same gun.
`vr_arena_seed 0` (the default) rolls a different ladder every run; a number replays that run's ladder exactly (the
waves are rolled with a Levy triple reseeded from the seed and the wave, not with `random()`).

*The boss.* Every `vr_arena_boss_every` (5) waves, the wave's plan starts with its big one: an enforcer carrying a
rolled gun until wave 10, an ogre until wave 16, a death knight from there -- `vr_arena_boss_health` (1.25) times its
kind's health, on the field with the rest of the wave, worth four kills on the score. What else a boss does (its perk
pedestal) is the next task.

*Where they come in.* A wave's monster is put on the map's own place of its kind: the floor places (`kind 1`) for
everything that walks, the high places (`kind 2`: a tower top, a wall walk) for the fiend and the enforcer, 96 over the
point so it arrives by dropping onto its ledge. Places are gone through in order from where this wave started and each
is used at most once -- 12 foes do not arrive in 4 corners -- and a place already holding one of this wave's foes is
passed over, not shared. A high foe whose high places are all held comes in on a floor place rather than be stepped off
its ledge. A map with no places of its own gets the ring 400 around the player, stepped 48 aside and walked round
until a spot has ground under it (read off the brushes, 600 down) and no foe on it. The alive cap (`vr_arena_max_alive`
8, for Quest 3) holds the field: the rest of a thick wave trickles in, 1.5 s apart, as places free up; a wave that
nothing has moved for 300 s is called off rather than stall the run. Every wave monster keeps its kind's own start --
its `walkmonster_start_go`, its `takedamage`, its full health, its AI -- and is given the player once that is through
(`VR_ARENA_HuntPlayer`), because a monster started any other way is something you cannot hit.

*The player's side.* The run starts stripped (`VR_ARENA_StripPlayers`: no weapons, fists only) and the weapons come
from the foes: an armed humanoid drops the gun it was holding when it dies, as it always has (`VR_AG_SoldierDrop` /
`VR_AG_DropHeld`), so the ladder is climbed by picking up what fell. Each wave clear drops `vr_arena_medkits_after` (2)
medkits and, every `vr_arena_armor_every` (4) waves, an armour -- the light one until the waves reach fiends and the
heavy one and the light one by turns after that -- on the map's `info_arena_item` points (or the ring, off the player).
Score: `vr_arena_score_kill` (100) times the rung its gun was on (fists/shotgun/nail pistol 1, super shotgun/nailgun/
super nailgun/ogre/fiend 2, rocket launcher/lightning/grenade launcher/death knight 3), a boss four of that, plus 250
times the wave for clearing it. The readout is centerprint, with the same in the console for a log: `ARENA - unarmed,
wave 1 coming`, `ARENA - Wave 2: 2 foes (score 450)`, `ARENA - Wave 1 clear: 2 foes down (score 450)`. If the player
falls, the run says so (`ARENA - you fell at wave 2 (score 450)`) and restarts after 10 s (`restart fresh`), the way
the horde does.

*The map's fix.* `vrarena_gen.py` built its 40 points and never handed them to the map writer, and `--check` counted
the lists in memory rather than the file it wrote -- so the `vrarena.map`/`.bsp` the last task shipped had 20 entities:
worldspawn, 11 `func_group`, 8 `func_detail`, and nothing else. No `info_player_start`, no spawn places, no item points,
no lights; every foe came in on the ring and the player had no start. Now the writer is given the points (`mw.add(e)`)
and `--check` reads the points back off the written file, prints what is missing and refuses to compile until they are
there. `vrarena.map` has 60 entities: the same 20 solid ones plus 12 `info_arena_spawn` (4 kind 1, 8 kind 2), 3
`info_arena_item`, 24 `light`, 1 `info_player_start`. Its solid geometry is untouched -- 980 solid brush lines, sha1
`cad99ec111884e2fe8ec74a08e88bdf06b018897` before and after -- but qbsp's output is not: with no entities in the map it
padded the shell with filler, which is what the last section's "No entities in empty space" warnings were. Compiled the
same way (`-bsp2 -wadpath`, vis, light `-extra4 -dirt -bounce -lit -lux -lightgrid 64 64 64`): 780 visible faces, 1142
faces, 231 leafs, 1349 clipnodes, 13 texinfos (was 2209 faces, 678 leafs, 10119 clipnodes, 14 texinfos), no fill
warnings, and `.lit`/`.lux` now come from real light entities. `bsp_holes.py` with 2000 rays: 0 missing-face hits, 0
undrawn hits, 0 holes.

**Verified.** `build.sh arenadir`: QC with 0 warnings, fgdgen over every spawn function (376 entities), the statics,
test-number and QC-precedence checks passing. Four tests, run with `developer 1` and `vr_arena_test 1..4` (the QC's own
beat in `world.qc` under `QVR_TESTS`):

- test 1, the plans, the hooks, the score and the supplies (vrtesthall, seed 7): **254 passed, 0 failed**
- test 2, the waves live -- every foe can be hit, the alive cap holds, each wave comes in at its own place: **13, 0**
  (`arena: foe kind 0 gun -1 tier 1 at -640 640 0`, `arena: foe kind 0 gun -1 tier 1 at -640 -640 0` for wave 1's two
  corners; `arena: foe kind 0 gun 29 tier 1 at 640 -640 0` for wave 2's nail pistol; `arena: wave 1 clear kills 2
  medkits 2 armour 0 score 450`)
- test 3, jumped to the boss wave -- its kind, its health, its tier, the cap: **52, 0** (`arena: wave 5 score 0 alive
  5`, `arena: boss kind 1 health 101 tier 1`, `arena: foe kind 1 gun 4 tier 1 at -736 288 352` -- an enforcer on a wall
  walk: a kind 2 point at -736 288 256, 96 over it)
- test 4, the places a wave comes in on: vrarena (4 ground, 8 high) **7, 0** -- `arena: t4 ground place -640 640 0,
  -0.03125 down` (on the floor), `arena: t4 high place -402 402 352, 95.9688 down` (a tower top, dropped onto it),
  `arena: t4 past a taken place: -640 -640 0` (a held corner passed over); vrtesthall, which has no places of its own
  and gets the ring: **5, 0** (`arena: t4 ground place 200 60 25, 24.9688 down`).

Live on vrarena (seed 3, `god` after the map loads): `ARENA - unarmed, wave 1 coming`, wave 1 and 2 as above, boss
waves at 5, 10 and 15 (`arena: boss kind 5 health 251 tier 2` at 15), the medkit and armour lines, and the fall path
(`arena: fell wave 2 score 450` then `ARENA - you fell at wave 2 (score 450)`, restart 10 s later).

**Settings and defaults.** All in the engine's cvar list, none of them needed to play: `vr_arena` 0 (1: its waves run
in this map too -- vrarena runs them on its own, so a mode map never needs it), `vr_arena_wave_delay` 6 s from a wave
clear to the next, `vr_arena_max_alive` 8, `vr_arena_boss_every` 5 (0: none), `vr_arena_boss_health` 1.25,
`vr_arena_medkits_after` 2 (0..4), `vr_arena_armor_every` 4 (0: none), `vr_arena_score_kill` 100, `vr_arena_seed` 0 (a
different ladder every run), `vr_arena_test` 0 (1 the plans, 2 the live waves, 3 a boss wave, 4 the places).

**To test in VR.** Play > Survival Arena: does it start unarmed and does wave 1 feel right (one or two unarmed grunts
-- the fight should read as a shove, not a gunfight); pick their guns up off the bodies and check a dropped gun is
reachable where it fell, since that is how the ladder is climbed; wave 2 with the nail pistol, then the waves where the
humanoids carry shotguns and the classics arrive -- ogres, dogs, knights, fiends, death knights, and the high ones
(fiends and enforcers) arriving on a tower top or the wall walk; the alive cap (8): a thick wave should come in trickles
with room to breathe, not all at once; a boss wave (5) -- whether a 101-health enforcer is a fair step up at that
point; the medkits and armours landing on the three item points; the readout legible in the headset; the fall -- die and
see the run say so and restart. Play it twice with `vr_arena_seed 0` and check the two runs differ, then once with a
seed and check it repeats.

**Decisions.** A wave monster is spawned through its kind's own spawn function and never started another way: the first
version set `think = FoundTarget` at spawn, which skipped `walkmonster_start_go` and produced foes with `takedamage 0`
-- things that stand there and cannot be hit. High foes drop 96 onto their ledge instead of being placed on it, because
that is how the fiend and the enforcer arrive in Quake's own maps. The alive cap (8) is a Quest 3 choice, not a design
one. `vr_arena_seed 0` rolling its own ladder rather than a fixed one is what makes two runs differ, which the brief
asks for; the Levy triple rather than `random()` is what makes a seeded run replayable and a soak reproducible. The
300 s stall call-off exists because a headless soak cannot fight: without it a wave that nobody kills never ends.
Enemies are armed by the director's own pools, so `vr_agrunt_repl` stays off. No new performance caps.

*Open for the perks (W3):* the boss wave's reward -- its pedestal and its 1-of-3 choice -- is not in yet, so a boss wave
today is only a thicker one; and whether a dropped gun should be kept off the `vr_dropcap` fade in the arena, since a
player who is slow to the next weapon can find the gun they wanted gone.

*Suggested `quakevr/checklist.txt` lines:* `+ Survival Arena: the wave director (12 wave recipes and the ladder past
them, a boss wave every 5, the map's own spawn places, medkits and armour between waves, score and readout)` and
`+ Survival Arena: vrarena's point entities (spawn places, item points, lights) written for real, and its checks reading
the file they went into`

## Survival Arena: the boss-wave perks

**What is in.** Clearing a boss wave puts three lit items in the arena, each named for a perk, and walking over one is
the choice: that perk is yours for the rest of the run and no later boss offers it again. Seven perks, three offered at
a time (`vr_arena_perk_count` 1-4; an empty pool offers nothing): **Quad Strike** (a melee kill gives 2 s of Quad
Damage), **Neck Snapper** (a snapped neck restores 20 health), **Iron Skin** (armour you pick up is worth 25 more --
125/175/225), **Second Wind** (health tops at 15 past normal), **Feeder** (5 health per kill, no more than once a
second), **Quartermaster** (every wave clear tops up the ammo of the gun in your hand by 15, to the ordinary caps) and
**Spoils** (a foe's dropped gun stays rather than fading under the arena's drop cap). Every one hangs off a place the
game already reports -- `Killed` in combat.qc, the decap hook in vr_decap.qc, `armor_touch` in items.qc,
`VR_EnemyWeapons_MakeRoom` in vr_enemyguns.qc, and the director's own wave clear -- and each returns at once unless the
arena is running and that perk was taken (`VR_ARK_Running`). `vr_arena_perks 1` (on by default), with `vr_arena 1`. A
pedestal does not expire: it stands until it is walked over, and the next boss wave's offer replaces the ones left.

**Where the pedestals stand.** Off the map's own `info_arena_item` points, 96 units further from the centre than the
medkits and armour a wave clear drops there (`VR_ARENA_PedPlace`), so the two never occupy one spot; that placement
never walks the drop cursor, so a wave's drops keep every point they had. On vrarena: 263.9 263.9 48, -263.9 263.9 48
and 263.9 -263.9 48, from the three points at 196 196 48, -196 196 48, 196 -196 48. A map with no item points gets the
drop ring at 220 plus the same 96 (vrtesthall: 306.2 -367.6 24 and its two neighbours).

**How it is tested.** `vr_arena_perks_test 1..4` (`arena_perks: N passed, 0 failed`; test 4 is the full-map check and
prints `arena_full:`). Test 1 is the pool and the offer (19), test 2 the pedestals themselves (15), test 3 walks every
perk through the game's own calls -- `Killed`, the decap hook, `armor_touch`, a wave clear (40 on vrtesthall, 42 on
vrarena), test 4 drives the live director to its first boss wave, checks the offer there, takes one pedestal by
touching it and asks the next boss wave for another offer (14, on both maps). On vrarena the director's own family
still passes (`arena: test 2 over: 13 passed`, `arena: test 3 over: 49 passed`), as does `vr_scenario validate` (59
files, 3 errors -- all three ad_* files, which need Arcane Dimensions mounted) and `vr_scenario runall quit`.
`quakevr/scenarios/arena_intro.txt` is the one-press version: vrarena with `vr_arena_boss_every 1`, so the offer
arrives with the first wave instead of the fifth.

**Deviations.** Iron Skin is +25 at the moment `armor_touch` sets the value, so it is never more than 25 above the
shipped armour, and it does nothing to a suit already worn. Second Wind raises `max_health` from the director's frame
(client.qc puts `max_health` back on a level change) and lifts a player who was full at the old top; a megahealth's 250
is untouched. Quartermaster adds ammunition through vr_ammoutil rather than dropping a new item class -- the brief's
"reuse the arena's own drop path" read as "the arena's own wave clear", which is where the ammo now comes from. A
heavier boss was tried through `.vr_heavy_move` and nothing else the engine exposes: that is a slowdown knob
(vr_fatigue.cpp), while `sv_maxspeed` is a server-info change that fights the stick calibration in vr_input.cpp and
overrides the player's own setting, so a boss is bigger in count and health, not in scale.

*Open:* whether Quartermaster should reach a player holding nothing; whether the offer should hold the next wave until
somebody chooses (today the next wave starts on its normal delay regardless); and whether a taken perk deserves its own
line in the readout, which today only says "A BOSS is with this wave".

*Suggested `quakevr/checklist.txt` lines:* `+ Survival Arena: a boss wave's clear offers three pedestals, and the one you walk over is the perk you keep {test:arena_intro}` and `+ Survival Arena: what a taken perk does -- the melee kill's 2 s of quad, the snapped neck's 20 health, armour worth 25 more, a wave clear's ammo`

## Survival Arena: the waves follow the game's difficulty

*The arena is now a different arena at each of Quake's four difficulties: Easy is smaller, slower and more generous,
Nightmare bigger, quicker and meaner -- and one knob, the arena's own Difficulty Impact, says how far.*

**What is in.** `vr_arena_skill_impact` (1) is the arena's impact knob, in the shape of the armed humanoids'
`vr_agrunt_ai_skill`: `VR_ARENA_Skill(easy, hard, nightmare)` takes the game's difficulty (`skill`, from the `skill`
cvar) and returns the difficulty's step, Normal always as the knobs are set. At impact 1 the steps are **0.75 Easy / 1
Normal / 1.25 Hard / 1.5 Nightmare**; at 0 the skill changes nothing about the arena; at 2 the step from 1 is doubled
(0.5 at Easy, 2 at Nightmare); it is never below 0. The house shape is copied, not its numbers -- this knob has nothing
to do with `vr_agrunt_ai_skill`, and what the armed humanoids' AI does at each difficulty is that feature's business and
is untouched. Two things are not the factor: a wave's *gates* move in whole waves (`VR_ARENA_WaveAdj`: 3 waves later at
Easy, 2 sooner at Hard, 4 sooner at Nightmare, times the impact, never earlier than wave 2), and the boss waves have
rules of their own (an Easy run's big ones a wave less often; a Nightmare run brings two). The readout says which
difficulty the run is on: the opening line is `ARENA - Nightmare: unarmed, wave 1 coming`.

**What the difficulty moves, at impact 1** (Easy / Normal / Hard / Nightmare):

- *a wave's size* -- every group of every recipe and of the waves past the table, whole foes, a group never emptied:
  wave 6 is 5 / 5 / 5 / 6 foes, wave 11 5 / 7 / 8 / 10, waves 14 and 15 5 / 6 / 7 / 8.
- *how many are up at once* -- `vr_arena_max_alive` 8 is a field of 6 / 8 / 10 / 12 (and 12 is 9 / 12 / 12 / 12), never
  more than the wave plans.
- *the pacing* -- `vr_arena_wave_delay` 6 s is 8 / 6 / 4.8 / 4 s of rest, and a thick wave's trickle 2 / 1.5 / 1.2 / 1 s
  apart.
- *when the ladder opens* -- the second rung of the guns at wave 13 / 10 / 8 / 6 and the third at 21 / 18 / 16 / 14; a
  knight 7 / 4 / 2 / 2, a dog 9 / 6 / 4 / 2, an ogre 11 / 8 / 6 / 4, fiends 15 / 12 / 10 / 8, death knights 19 / 16 / 14
  / 12. Where the waves past the table start thickening stays wave 13 at every skill.
- *how the run starts* -- the share of the first waves' humanoids that come unarmed is 0.30 / 0.20 / 0.15 / 0.10, and
  the unarmed window (wave `4` adjusted) reaches to wave 7 / 4 / 2 / 2.
- *the boss waves* -- `vr_arena_boss_every` 5 is a boss every 6 waves at Easy and every 5 at every other skill; a boss's
  kind arrives earlier (an ogre boss from wave 13 / 10 / 8 / 6 and a death knight boss from 19 / 16 / 14 / 12, an enforcer
  with a rolled gun before that); `vr_arena_boss_health` 1.25 stands an enforcer boss at 76 / 101 / 126 / 151 health; and
  a Nightmare boss wave carries two, the second the same kind at half its bulk (38 / 51 / 63 / 76).
- *what a wave clear gives* -- the other way round, because a softer run is the more generous one: `vr_arena_medkits_after`
  2 drops 3 / 2 / 2 / 1, and `vr_arena_armor_every` 4 is an armour every 3 / 4 / 5 / 6 waves.
- *what it pays* -- `vr_arena_score_kill` 100 is 75 / 100 / 125 / 150 for a first-rung kill and 900 / 1200 / 1500 / 1800
  for a third-rung boss; a wave clear's 250 x wave bonus is 563 / 750 / 938 / 1125 at wave 3.
- *what a boss clear offers* -- `vr_arena_perk_count` 3 puts up 2 / 3 / 4 / 5 pedestals (the pool's 7 is the clamp; the
  perks section's "three at a time" is Normal's number).

**What it does not touch.** The armed humanoids' own AI and `vr_agrunt_ai_skill` (their feature, its own impact knob);
which guns and which species a wave *rolls* for its size (the seed's business); the map's spawn and item places, and the
pedestals' places; the heavier armour's own wave (a run earns it at wave 12 whatever the difficulty -- only how often an
armour comes moves); the fall path and its 10 s restart; the 300 s stall call-off; the tests.

**The clamps.** The factor is never below 0, and a factor of 0 leaves a span or a supply as its knob sets it rather than
emptying the arena. A group of foes is never below 1 (a thinned wave still brings its zombie and its boss), the field cap
1..`AR_PLAN_MAX` (12), a wait never below its floor (1 s between waves, `AR_TICK` 0.5 s in the trickle), medkits 0..4
(`AR_KIT_MAX`), an armour gap 0..12 (`AR_ARMOR_MAX`) and 0 when the knob is 0, a boss's health never below 1, a gate
never earlier than wave 2, a kill's score never below 1. The knobs themselves keep the clamps they had, and the skill
itself is clamped 0..3 by the engine (`Quake/sv_main.c`).

**Verified.** `build.sh arenaskill`: QC with 0 warnings, fgdgen (376 entities), the statics, test-number and
QC-precedence checks passing, and the engine rebuilt with the menu page below. `vr_arena_test 1..5` with `developer 1`
and `god`, on vrtesthall (`vr_arena 1`) and on vrarena -- the QC's own beat in `world.qc` under `QVR_TESTS`, e.g.
`bash /c/OHWorkspace/qvr-kit/run.sh arenaskill -Filter "arena:" -QuitOn "arena: test 5 over" -Timeout 300 -Script
"developer 1; god; vr_arena 1; vr_arena_seed 7; map vrtesthall; wait15; vr_arena_test 5; wait6000"`. Test 5 is the
difficulty family (its sections pin the skill each one needs); every family starts at Normal, where the factor is exactly
1 (`VR_AT_Start`), and puts the run's own skill back at the end (`VR_AT_End`), so tests 1..4 keep the expectations they
were written against. Test 5: **467 passed, 0 failed** on both maps. The families it could have broken, run again after
it: test 1 (the plans) **254**, test 2 (the live waves) **13**, test 3 (a boss wave) **52**, test 4 (the places) **7** on
vrarena and **5** on vrtesthall, `vr_arena_perks_test 1..4` **19 / 15 / 40** and **14** (`arena_full:`). The perk tests
pin the impact at 0, so the perk numbers stay what that section documents. One finding, logged and fixed: the first live
run on vrarena at `skill 3` printed `ARENA - Nightmare` while its start-of-run line said `max alive 6, boss every 6` --
Easy's numbers, because `world.qc` reads the skill cvar into the `skill` global only at the top of each frame, so at
world spawn the global is still 0 (the same reason `QC/triggerS.qc` reads the cvar). `VR_ARENA_SkillLevel()` takes the
cvar where the global is not set yet; the same run then read `arena: vrarena on (skill 3 Nightmare, impact 1, max alive
12, boss every 5, seed 3)`.

**Settings and defaults.** Combat > Survival Arena is the page (`vr_menu_gameplay.inc`): Difficulty Impact (0..3,
extendable to 10), the Skill cycle, the waves, the boss waves, what a clear drops and pays, the perks and the seed --
`vr_arena_skill_impact` 1, `vr_arena_medkits_after` 2, `vr_arena_armor_every` 4, `vr_arena_perk_count` 3 (1 to the pool's
7), the rest as the wave director's section lists them. The Play page's Survival Arena line says the waves follow the
difficulty.

**To test in VR.** Play the same map at each difficulty and feel the difference: Easy -- wave 1 and 2 read as a shove,
more unarmed humanoids in the first waves, a boss at wave 6 rather than 5, three medkits after each clear; Nightmare --
fiends and death knights in the waves within a few clears, a cap of 12 with the wave arriving in trickles, 4 s of rest,
two bosses in a boss wave, one medkit. Normal is what the last two sections describe. Play it at a seed and at each
skill and compare the same wave's size, which is the number the difficulty is for.

**Decisions.** A wave's *gates* move in whole waves rather than by the factor, because "fiends from wave 10" is what a
player can tell, and a gate earlier than wave 2 would put a fiend in wave 1. The 13-and-on thickness rule keeps its
anchor at wave 13 at every skill -- the difficulty makes those waves thicker, not the ladder start sooner. In the fixed
recipes (waves 2..12) a boss wave *adds* its big one rather than trading away a foe the recipe wrote, so Easy's wave 6 is
its 4 foes plus its boss. The armour's *kind* stays the table's (the heavier one from wave 12 of the run): what the
player earned is theirs, and the difficulty moves how often supplies come, not what they are worth. An interval is
therefore multiplied by the factor while a count is divided -- dividing an interval would make Easy the less generous
run, which is the wrong way round. A kill's score never falls to 0, so a Nightmare run can never pay nothing. And the
difficulty is asked of the cvar at world spawn, because the QC's `skill` global is not set yet there.

*Open:* whether Easy should soften the boss *kinds* as well as their gap (today an Easy run meets its first ogre boss at
wave 13, a Nightmare one at 6); whether a Nightmare run's second boss should be its own knob rather than a rule; and
whether the impact should reach the pedestal count at all, since a run that offers 2 perks has less to choose from.

*Suggested `quakevr/checklist.txt` lines:* `+ Survival Arena: the waves follow the game's difficulty -- size, field cap,
rest, the ladder's gates, a boss's bulk, a clear's supplies and a kill's score (vr_arena_skill_impact)` , `+ Survival
Arena: Nightmare brings two bosses and Easy one fewer boss wave; the run says which difficulty it is on (ARENA -
Nightmare: unarmed, wave 1 coming)` and `+ Survival Arena: the arena's own Difficulty Impact knob, and nothing about the
armed humanoids' AI (vr_agrunt_ai_skill) touched by it`

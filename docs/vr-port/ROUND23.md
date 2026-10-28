# Round 23: after 1.1.0-beta4

Notes for the work after the 1.1.0-beta4 prerelease (2026-10-27, commit c9aedff6). Round 22 (`ROUND22.md`) covers
1.0.1 to 1.1.0-beta4; grep it for anything older. Append new sections at the end of this file, one per shipped task:
what changed, how it was verified (numbers), settings and their defaults, what to test in VR, decisions.

## Survival Arena: the map

*Survival mode needs its own map (`vrarena`): the space its waves of armed humanoids fight in. The wave code is the
next task; this one is the map and the menu row that reaches it.*

**What changed.** `Misc/quakevr/maps/vrarena_gen.py` writes `quakevr/maps/vrarena.map` (130 brushes, 40 entities; as
the map now stands 230 brushes and the same 40 entities -- see "the map's look" below) and compiles it. Play > Maps
gained "Survival Arena" -- `command("Survival Arena", "map vrarena")` with a help line, in `pagePlay()` in
`Quake/vr/vr_menu.cpp`, after the Melee Recording Room.

The room is an octagon 1728 units across (49 m at 32.8 units to the metre), its corners cut 200, the floor at 0, walls
64 thick out to 864 and a ceiling at 384 (11.7 m): a solid shell, so the arena is sealed and the fight happens in the
middle of it. (This paragraph used to say the room was 800 high: 800 is the octagon's half-width -- `H` in the
generator; the shell has topped out at 384 -- `ZH` -- since the map's first commit.) In it:

- a central dais, 232 to its flat sides and 96 (2.9 m) high, climbed from the north, the south, the east and the
  west -- six steps each, 16 high and 24 deep -- and the player starts on top of it (`info_player_start` 0 0 120);
- four corner towers, 180 square and 256 (7.8 m) high at the four cut corners, each climbed by an outer pinwheel
  staircase of sixteen steps, 16 high and 16 deep, hugging the tower's faces;
- a wall walk at 256: a 128-wide deck along the east and the west wall (y -576..576) joined to the tower tops it
  passes by 64-wide bridges, so the high ring is reached only through the towers and a step off it is a 256 drop;
- eight covers, 48 high and 80 wide, 520 out at 45 degrees apart -- (480, 200), (200, 480) and their mirrors -- each
  its own `func_detail`, so sight lines break up but the open ring stays walkable (they are 96 wide at 512 out now,
  grown inwards -- the dressing in "the map's look" below explains why);
- 24 lights: 8 ceiling lamps (a 64-square `qvr_light` block flush with the ceiling, now eight coffered recesses) with
  a light 300 and falloff 0.47 forty units under each; one of 180 over each tower top and 160 over each walk landing;
  150 in each corner of the floor and 140 over the dais, one facing each staircase; worldspawn `light 90`.

12 `info_arena_spawn` points carry a `kind`: 1 for the four ground points (640, 640, 0 -- the corners of the open
ring) and 2 for the eight high ones (the tower tops at 402 402 256 and the walk landings at 736 +-288 256). 3
`info_arena_item` points sit at 196 196 48, -196 196 48 and 196 -196 48. Worldspawn: `mapname vrarena`, `message
"Quake VR Survival Arena"`, `worldtype 2`, `_tb_mod`, `_vr_debris 0` and `_qvr_prelit 1`; its WAD list and its
textures were the `quakevr_dev.wad` placeholders and are id's base set -- see "the map's look" below.

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
**Spoils** (a foe's dropped gun stays rather than fading under the arena's drop cap; since "the cleanup" below:
twice `vr_arena_gun_max` of them lie about). Every one hangs off a place the
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
"developer 1; vr_arena 1; vr_arena_seed 7; map vrtesthall; wait15; god; vr_arena_test 5; wait6000"`. Test 5 is the
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

## Survival Arena: review (agent/arenareview)

A review of the arena's commits (`8aedc7766..next`). The shared-file hooks (combat.qc's Killed, items.qc's
armor_touch, vr_decap.qc, vr_enemyguns.qc, world.qc) all return at once off the arena (`ar_mgr` is world unless the map
is vrarena or `vr_arena` is set); e1m1 with every monster killed (`impulse 205`) prints no error, and eval.sh passes.
Plan arrays survive a save and load mid-wave (the second foe after loading keeps its planned gun).

**Fixed.**
- *Quad Strike cut a real quad short*: a hand kill set `super_damage_finished = time + 2` whatever was running, so a
  picked-up Quad Damage with 25 s left dropped to 2 s. It now only ever adds quad time (`vr_arena_perks_test 3`: 43
  passed on vrarena, 41 on vrtesthall).
- *A stalled wave's clear took too much*: `VR_ARENA_ClearField` walked `findflags(ar_slot, wave)`, which matches any
  shared bit, so it also removed other waves' foes and nearly every fallen body (a negative `ar_slot` shares bits with
  almost any wave), and it removed a foe held in a hand without `VR_Carry_Forget`. It now removes exactly
  `ar_slot == wave`, let go of first (`vr_arena_test 1`: 257 passed; test 3 still 52).

**New test.** `vr_arena_test 6`, the long run: the live director through 32 waves (1 s apart, every foe felled by the
test's blow), edicts counted every 5 waves; `arena: test 6 over: P passed, F failed`. Run:
`run.sh <name> -Filter "arena: (t6|test|fail)" -QuitOn "arena: test [0-9]+ over" -Timeout 1500 -Script "developer 1;
vr_arena_seed 7; map vrarena; wait15; god; vr_arena_test 6; wait60000"`. **It fails today, on purpose**: nothing in
the arena ever cleans up its fallen bodies or the player weapons its armed humanoids drop -- 99 edicts at wave 2, 928
at wave 32, with 233 bodies and 196 `thrown_weapon`s lying in the map (about 28 edicts a wave; the unpicked medkits
and armour are only 67 of them). The Horde (vr_mg_horde.qc, `MGH_corpse_expire`) fades its bodies after 11..16 s;
how the arena should (a fade, a cap, bodies older than two waves) is the author's call, so it is left open. (Since
closed: "Survival Arena: the cleanup", below.)

*Suggested `quakevr/checklist.txt` line:* `+ Survival Arena: a Quad Damage picked up is not cut to 2 s by a Quad Strike
hand kill`

## Survival Arena: the map's look: id's base set, and the rework it needed

**The first pass, kept as the record (his ask, 2026-10-28: "texture the new arena map -- give it an idbase look").**
Until then `vrarena` wore `quakevr/wads/quakevr_dev.wad`'s six flat grids, the stand-in set for a map being laid out
(`MAPPING.md`, "Textures and id's data"); `vrcalibration.bsp` and the hub maps were already built from id's own base
textures, and this was that decision taken for the first time on a map players fight in. `vrarena_gen.py` set a role
table instead of six names and worldspawn began listing two WADs -- `"wad"
"quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad"`, the pair `make_vrcalibration_map.py` uses. id's textures
come from the git-ignored `id_textures.wad` that `Misc/trenchbroom/make_id_wad.py` builds from your own paks, so nothing
of id's is committed: what ships is the compiled `vrarena.bsp`, which carries the mip chains it was lit with (15
texinfos then -- 13 of id's, our `qvr_light`, qbsp's own `skip`). No brush moved: 130 brushes, 40 entities, 1587 lines
both ways, 63 scale numbers different. It compiled with the final preset (1161 world faces, 231 leaves, `holes: 0`;
`vrarena.bsp` 272,176 -> 346,028 bytes, lightmaps 198,236 -> 200,384), loaded clean, and gave `vr_arena_test 4` 7/0,
`vr_arena_test 5` 467/0 and `vr_arena_test 1` 256/1, the known harness case. Two sentences of that write-up were wrong
and are put right here: it called the room 800 units high (800 is the octagon's half-width `H`; the shell tops out at
`ZH = 384`, as it has since the first commit), and it said the engine keys light emission off the name `qvr_light` --
nothing in `Quake/` mentions that name, and what glows comes from `quakevr/relight_textures.cfg` plus fullbright pixels
or a QRP `_glow`/`_luma` map. The paint itself is what his playtest answered, below.


*His playtest of the arena as textured above, 2026-10-28: "Few issues with the texturing: the crate textures are
misaligned. The wall textures do not fit well, they look more like computers, and the wall geometry is too flat. Use
proper wall textures with nice trims and light fixtures. The spawn square in center pillar has misaligned textures. The
ceiling has same textures as the wall. It should have texture that fit a ceiling and also have light fixtures." Four
complaints, one of them about geometry rather than paint: an 11.7 m wall that is one flat prism reads flat whatever is
put on it.*

**Why the first pass looked wrong.** mapgeom lays a face world-aligned: the picture's own grid sits on world
coordinates, which is what a floor wants and what a discrete object does not get on. The covers were 80 wide and 48
high with `crate0_side` at 0.75 -- 1.25 tiles across a face -- so a crate's rim ran across the middle of the block and
its outer edges were cut short, and the lid at 1.25 tiled only where the world grid happened to line up. The spawn pad
was 96 wide and 2 thick with `plat_side1` (a 64x16 lip) at 0.125: id's 16-unit lip squeezed into 2 units and repeated
eight times along the face. Each cardinal wall was one 384-high prism in `twall1_1` and each cut corner one in
`twall2_1`: panels with a control-desk look, nothing proud of them, no fixture anywhere. The ceiling wore `sfloor4_1`,
a floor plate, and its eight lamps were plain 64-square boxes. (One sentence in the section above was wrong with it:
"the engine keys light emission off that name". Nothing in `Quake/` mentions `qvr_light`. What glows is decided by the
relighting system -- `quakevr/relight_textures.cfg`, whose patterns say `kind=fixture|glow|liquid|off` with `scale`,
`light`, `color` and `reach`, and which falls back on the name only as a rule of thumb -- plus fullbright pixels
(palette 224..254) or a QRP `_glow`/`_luma` map. Measured in the WAD: `tlight01` has 2.3% fullbright pixels and
`ceil1_1` 14.1%; `light3_3`, `tech04_*`, `metal*`, `crate0_*` and `plat_*` have none.)

**What changed: the palette, the laying, and 100 new brushes.** `vrarena_gen.py` keeps one role table and a small
laying layer under it. `texture_sizes()` reads the width and height of every texture out of the WADs the map names;
`laid()` writes a face's texture axes, its offsets and its scales from the face's own extent and anchor, in three
modes:

- **world** -- mapgeom's paraxial projection with the picture's corner asked to land at `anchor`: coplanar faces
  continue into one another, which is what a floor, a ceiling and a deck want, and what lets a field of wall panels
  butt exactly with the mouldings that bound them;
- **fitv** -- `reps`[1] tiles exactly down the face at the same scale across it: id's way of making a band of plates or
  of trim land where the band is (make_vrcalibration_map.py's `fitv`);
- **fit** -- whole tiles across and down, one for a face that is a single thing: a crate, a lamp, the lit back of a
  coffer. `reps` 0 means "as many as come nearest at this scale", so nothing gets squashed.

| the part of the room | texture (size) | how it is laid |
|---|---|---|
| the floor | `ground1_2` (64) | world, anchored at the origin: id's 64-unit plates, 49 m of them |
| the eight walls | `tech14_1` (128) | world, anchored at z=96: three panel rows, 96 to 352, butted with the rail under them and the cornice above |
| the four towers | `tech14_1` (128) | fitv, 2 tiles down a 256-high face |
| the wainscot band (16..80) | `metal4_4` (64) | fitv, one plate exactly in the 64-high band |
| skirting (0..16), rail (80..96), cornice (352..368), the ceiling's border moulding (368..384) | `tech04_1` (128x16) | fitv, one 16-row exactly, mitred round the octagon |
| the twelve uprights | `tech04_3` (16x128) | fit, one flute across the upright and as many down it as it is tall (2 down a 256-high run, 1 down a 96-high one), measured along its wall |
| the dais' eight sides | `tech04_3` (16x128) | fit, whole flutes across -- 17 along a 272-wide face, 6 along the 96 a cut corner measures -- and one 128-tall flute scaled 0.75 down the 96-high face |
| the ceiling, the coffers' linings, every step | `metal1_1` (64) | world: the ceiling's own plates, not the walls' |
| the eight coffers' frames | `tech04_1` (128x16) | fitv, 2 rows in a 32-high frame |
| the rings under the coffers | `ceil1_1` (16, 14.1% fullbright) | fit: whole 16-unit glowing tiles |
| a coffer's back | `light3_3` (64) | fit: 2 x 2 panels in the 128-square recess |
| the twelve lamps | `tlight01` (32, 2.3% fullbright) | fit, one lamp per upright, measured along the wall so a cut-corner lamp is not squashed |
| the dais' top, the tower tops, both walks, the four bridges | `sfloor4_2` (64) | world: everything you stand on up high is deck plate |
| the walks' and bridges' 64-thick edges | `metal4_4` (64) | world: one plate spans the thickness |
| the eight covers | `crate0_side` (64), lids `crate0_top` (64) | fit at 0.75: 2 crates along 96, 1 up 48, 2 x 2 across the lid |
| the spawn pad (128 wide, 16 thick) | `plat_top1` (64), lip `plat_side1` (64x16) | world: 2 plates across, 1 lip row up, 2 across |


**The dressing: 130 brushes to 230.** Nothing a fight uses moved. What was added is the room's own trim, 4 to 16 units
out of a wall, all of it flush-mounted on the shell:

- 40 band segments -- five bands (skirting 0..16, wainscot 16..80, rail 80..96, cornice 352..368, and the ceiling's
  border moulding 368..384) mitred round the octagon, each band eight runs corner to corner;
- 12 uprights in fluting, 16 wide and 8 proud, standing on the rail and stopping where the cornice begins: at the
  third and the two-thirds mark of each cardinal wall and at the middle of each cut corner. On the east and the west
  wall each upright is two brushes (96..192 and 256..352) so the walkway bridges pass between them -- 16 brushes in
  all. Their first version ran up to 368 and pierced the cornice: `--check`'s interpenetration test printed all 12
  clashes, and they now stop at `PIL_TOP = 352`. Their first *texture* was wrong too, and only a probe found it:
  `tech04_3` and the dais' sides were laid `fitv`, which pins the pattern vertically and forces the horizontal scale to
  match it, so a flute was cut in the middle wherever an upright or a dais face ended. They are laid `fit` now --
  `reps=(1,0)` across an upright, `(0,1)` down the dais -- with each upright's U taken along its own wall, which is
  what took `tech04_3` from 24 clean faces to 68;
- 12 lamp hoods, 8 proud, one on the front of every upright, carrying `tlight01` at 272..304 -- the lamp's 32 units fit
  the hood exactly, and on a cut corner the face is measured along the wall (its own U axis, taken from the wall's
  direction) so a 45-degree lamp is not squashed to 11 units wide;
- 40 coffer parts: 8 coffers, each a frame of four 32-high arms around a 128-square recess lined with `metal1_1` and
  backed by `light3_3`, ringed under the frame by `ceil1_1`. They reach out to about 746 from the centre, clear of the
  ceiling's border moulding (which starts at 784 out).

The eight covers grew from 80 wide at 520 out to 96 wide at 512 -- **inwards only**: their outer faces are exactly where
they were (x 144..240 and y 424..520 for the north-east one, and the same for the rest), so the 8-unit clearances to
the tower staircases are unchanged and they are still 48 high (64 would block a standing player's eye at 56). The spawn
pad grew from 96 to 128 wide and from 2 to 16 thick for the same reason as the covers: at that size `plat_top1` and
`plat_side1` land on whole tiles. The shell, the dais and its four flights, the towers and their pinwheel stairs, both
decks and all four bridges, the 12 `info_arena_spawn` points, the 3 `info_arena_item` points, `info_player_start` and
all 24 light entities are where they were -- the number of lights did not even change, because the lamps and the
coffer rings bring their own fullbright pixels.

**How the fit was checked, not just looked at.** `scratch/arena/align_probe.py` reads every face in the written
`.map`, takes the tile size out of the WAD, and for every corner computes `S = dot(U,P)/uscale + uoff` and asks how far
it is from a whole texel; `flute_probe.py` and `bad_faces.py` then say, per texture, how many tiles a face spans and
which faces do not span whole ones. The 106 faces that must read as one object -- 40 `crate0_side` and 8 `crate0_top`,
2 `plat_top1` and 4 `plat_side1`, 12 `tlight01`, 32 `ceil1_1` and 8 `light3_3` -- are clean at every corner, worst
residual 0.0017 texels (the rounding in the file's 6 decimals: a thousandth of a unit). The fluting next: 68 of its 89
faces carry whole flutes at their own edges, one flute across each of the 12 uprights and 17 across each 272-wide face
of the dais. The 21 left are its 12 upright bottoms and the dais' footprint, all buried in the brush they stand on, and
the four cut-corner uprights, whose 45-degree corners snap to the 8-unit grid and so measure 15.9 or 16.1 instead of
exactly 16 wide -- their flutes are whole to within 1.8% and their 5.75-unit flanks carry a flute scaled 1.06. The
room's other 1276 faces are laid on the world grid: `tech14_1` panels are exact where the rail at 96 and the cornice at
352 bound them and crop at the mitres, `metal4_4` and `tech04_1` are exact across each band's height and crop along its
run as mitred trim does, and `ground1_2`, `metal1_1` and `sfloor4_2` crop only at corners buried in the shell.
`scratch/arena/face_census.py` then census-checks the *assignment*: 230 brushes, 225 of them 6-faced, 4 five-faced (the
corner wedges) and 1 ten-faced (the dais' octagonal prism), and `tech04_3` on exactly 16 upright brushes (5 faces each)
and the dais (9) -- no face wearing a texture that was meant for something else.

**The room itself is untouched where it matters.** 40 entities as before, the shell still sealed, every point where it
was, `_qvr_prelit 1` and `light 90` kept. `vrarena.map` went 1588 lines to 2488 -- 100 new brushes.

**Compiled** with ericw-tools 2.0.0-alpha11 with the **final** light preset -- new surfaces mean new radiosity, so the
lighting is rebuilt with the map: `--compile --preset final --holes 400128` after a fast-preset pass (qbsp 0.2 s, vis
0.0 s, light 0.1 s) to prove the shell first. Final: qbsp `-bsp2 -wadpath <tree>` 0.2 s, vis 0.9 s, light `-extra4
-dirt -dirtscale 1.5 -dirtdepth 96 -bounce -lit -lux -lightgrid -lightgrid_dist 64 64 64` 2.2 s, no warning, error or
leak. `bsp_holes.py`: 400256 rays, 1948 world faces, 391 leaves, 0 missing-face hits, 0 undrawn, 0 unlisted, `holes: 0`.
`vrarena.map` 76,522 -> 137,694 bytes, `vrarena.bsp` 346,028 -> 445,872, the lightmaps 200,384 -> 229,652
(`vrarena.map` with its `.bsp`, `.lit` and `.lux` is committed together: the lighting belongs to the surfaces it was
computed for).
`--check` before compiling: 230 brushes, 0 duplicates, 0 brushes running into another, 0 features standing in the open
ring, 12 spawns (8 high, 4 ground), 3 items, 88 stair steps with 0 unclimbable, 112 under the player start.

**Verified in the game**, on the new BSP: `map vrarena` loads with no missing-texture and no bad-surface line;
`vr_arena_test 4` 7 passed / 0 failed (ground 4, high 8, a place with ground under it, a taken place skipped) and
`vr_arena_test 5` 467 / 0. `vr_arena_test 1` is 256 / 1 here -- the same single failure, `a fallen monster is not
alive`, that reproduces on the previous BSP at the same commit and gives 257 / 0 on `vrtesthall`: the live arena in the
room, not its surfaces.

**For the author.** `vrarena.bsp` is a committed Quake map carrying id's artwork, which is what `MAPPING.md`'s
carve-out for `vrcalibration` and the hub maps allows and what its "Keep that to the maps that need it" line is about --
that rule is left as written, for you to extend or walk back. The generator keeps the flat look buildable for anyone
whose checkout has no `id_textures.wad`: `--dev-textures` swaps the whole palette back to `quakevr_dev.wad`'s grids and
lists that WAD alone, and without the file and without the flag it says so and stops; either way `main()` now refuses to
write a map naming a texture that is not in a WAD it lists. What your eye is still the judge of: whether `tech14_1`
panels with `tech04_1` bands and `tech04_3` fluting are the trim families you want (that is `make_vrcalibration_map.py`'s
own vocabulary, so it matches the calibration room); uprights at the third and two-thirds mark of a 16 m wall; coffers
128 square with a `ceil1_1` ring; and whether 96-wide covers read better than 80. What came out is in
`scratch/arena_dress.png` (a headless frame from the player start on the dais, composed by `run.sh -Out`) and
`scratch/arena/idbase_spawn.png` from the first pass, if you want to look before walking it -- this session's model
cannot see images, so every number above is a number, not a verdict.

## Survival Arena: the cleanup (agent/arenaclean)

The leak the review found (928 edicts by wave 32) is closed. Everything below runs only while an arena runs (`ar_mgr`
set: the map vrarena or `vr_arena 1`); every hook returns at once otherwise. Settings (archived; Combat > Survival
Arena > *Cleanup*):

| Setting | Default | What |
|---|---|---|
| `vr_arena_corpse_fade` | 14 | s a fallen foe's body (or its head, gibbed) lies before it fades out over 1 s, as the Horde's do; give or take 3 s (a quarter of it at most); 0: never by time |
| `vr_arena_corpse_max` | 12 | bodies lying at most; past it the oldest fades first; 0: no limit |
| `vr_arena_item_waves` | 2 | a wave clear's medkits and armour still lying there this many clears later fade out; 0: never. Ones already picked up (items.qc leaves them as invisible edicts in single player) go at the next clear |
| `vr_arena_gun_max` | 24 | guns the foes dropped lying about at most; past it the oldest fades (vr_enemyguns.qc's fade); 0: no limit |

**How.** `VR_ARENA_MonsterDie` hands the body to `VR_ARENA_BodyDown` (its time, its place in the fall order, then
the cap); `VR_ARENA_WaveClear` calls `VR_ARENA_DropsExpire` before its new drops; `VR_ARENA_Frame` walks the leftovers
each frame (`findflags(ar_expiring)`, the Horde's pattern) and fades and removes what is due (`VR_Carry_Forget`,
`VR_Head_StopFlies`, `remove`). **Never taken:** a thing a hand holds (`carry_player`, or a player's
`mainhand_held`/`offhand_held`: a ragdoll's limb), one a force grab pulls (`fg_state`), or a body still dying or being
gibbed as a corpse (`vr_corpse 1`, `vr_corpse_dying`; waited for 10 s at most). Held, its time waits; let go of, it
lies 3 s more and fades. Past the body cap a held one stays and the next oldest goes.

**Guns.** In a running arena `VR_EnemyWeapons_MakeRoom` takes `VR_ARENA_GunMost` instead of the shipped
`vr_enemy_weapon_drop_max`, and counts every gun a monster dropped (`.vr_monster_drop`), not only the single-use enemy
guns: the arena's humanoids carry the player's own weapons, which the shipped cap never counted (196 of them by wave 32).
A gun a player dropped or threw is never counted; one in a hand or holster is not lying about; one carried, pulled or in
flight is spared (the shipped rules). **Spoils** used to switch the cap off (the leak's worst case); now it doubles
`vr_arena_gun_max` (48 by default), and its pedestal reads "Twice as many fallen foes' guns stay lying about". The stale
comment about a "`vr_drop_fade` knob" (it is a field) is gone.

**Tests.** `vr_arena_test 6` (the soak) now counts its growth from wave 16 (`AR_T6_FROM`, the map settled: gibs,
limbs and backpacks remove themselves on their own timers and sit at a steady count) and also checks the three caps.
`vr_arena_test 7` (vrarena): stand-in bodies, drops and guns through the real hooks -- the body cap fades the oldest not
held, a held / pulled / dying body outlives its time and goes within 3 s of being let go, drops two clears old fade and
picked-up ones go, the gun cap (a pulled gun spared, a player's own never counted) and Spoils doubling it.
`vr_arena_test 8` (e1m1): every monster felled whole, 20 s on, nothing carries an arena cleanup mark.

**Verified.** `build.sh arenaclean`: 0 QC warnings. `vr_arena_test` 1 **257**, 2 **13**, 3 **52** (vrtesthall), 4 **7**,
5 **467**, 7 **19**, 8 **3** (e1m1), all 0 failed; `vr_arena_perks_test` 1 **19**, 2 **15**, 3 **45** (two new Spoils
checks), 4 **14**, all 0 failed. eval.sh: no verdict changes. Soak (`vr_arena_test 6`, seed 7):

| wave | 4 | 8 | 12 | 16 | 20 | 24 | 28 | 32 |
|---|---|---|---|---|---|---|---|---|
| edicts (before: 99 at wave 2, 928 at 32) | 183 | 269 | 296 | 298 | 293 | 287 | 288 | 299 |
| bodies (before: 233 at 32) | 6 | 5 | 7 | 7 | 7 | 6 | 6 | 6 |
| `thrown_weapon` (before: 196 at 32) | 5 | 23 | 25 | 25 | 25 | 25 | 25 | 25 |
| drops lying | 4 | 4 | 4 | 4 | 4 | 4 | 4 | 4 |

6 passed, 0 failed; flat from wave 12 (298 -> 299 over waves 16..32). What fills the ~200 over the empty map at the
plateau (an end-of-soak dump): ~115 gibs and small gibs, ~44 backpacks, ~10 limbs -- every one on its own removal timer
(gibs 10..20 s, backpacks 120 s), so a steady count, not a leak; and 25 `weapon_inst` records, one per gun lying. The
bodies stay few in the soak because its blows gib (only the heads lie). Off the arena, e1m1's bodies go 23 -> 8 in 20 s
with or without the arena (its own systems: ragdoll cap, corpses); test 8 checks only that the arena marked none.

*Suggested `quakevr/checklist.txt` line:* `+ Survival Arena: fallen bodies fade after ~14 s (12 kept at most), a wave
clear's medkits/armour go two waves later, foes' dropped guns capped at 24 (48 with Spoils); a body you hold never
vanishes`

## Survival Arena: the wave flow (agent/arenaflow)

Vittorio: more foes per wave, not all at once; an "alive pool" so the flow feels natural. `QC/vr_arena.qc`:

- **Bigger waves.** `vr_arena_wave_size` (1.75): every crowd group of the ladder (grunts, enforcers, wave 11's ogres,
  the 13+ groups) times it, rounded, then the difficulty's `VR_ARENA_SkillCount` as before (`VR_ARENA_WaveCount`). A
  wave's one signature foe (wave 3's zombie, 4's knight ...) and its boss stay one. `AR_PLAN_MAX` 12 -> 40 (a
  Nightmare wave 40 plans 32, uncut); `vr_arena_max_alive` keeps its own clamp, now `AR_FIELD_MAX` 16.
- **The alive pool.** `vr_arena_alive_max` (6) up at once at wave 1, one more every `vr_arena_alive_grow` (4) waves,
  by the difficulty, never past `vr_arena_max_alive` (8, by the difficulty): `VR_ARENA_Pool`. A wave opens with
  `vr_arena_burst` (3) at once; after that, a free slot is filled `vr_arena_spawn_gap` (1.5) to twice that at random
  after it freed up (2-4 / 1.5-3 / 1.2-2.4 / 1-2 s by the difficulty). A foe's fall wakes the director at once, and
  its think lands exactly on the gap's end, so the gap is the gap.
- **Where.** A foe comes in out of the player's sight (outside ~60 degrees of his view, or a wall between) and at
  least 320 units off his eyes when the map has such a free place; then a far one in sight; then any free one
  (the same three passes on the ring for maps without spots).
- **The boss** is planned first as before but comes in halfway through its wave, after the burst, announced
  (`ARENA - A BOSS is coming!`) 3 s before.
- **The stall** (300 s) now counts from the wave's last arrival, not its start (a big wave trickles in).
- Menu: Combat > Survival Arena > The Waves: Wave Size, Alive Pool, Pool Grows Every, Opening Burst, Spawn Gap, and
  Foes Up At Once (now the pool's ceiling, 1..16), each with help.

At Normal, seed 7:

| wave | 1 | 5 (boss) | 10 (boss) | 20 (boss) |
|---|---|---|---|---|
| total (before) | 4 (2) | 8 (5) | 10 (6) | 13 (7) |
| pool | 6 | 7 | 8 | 8 |

**Tests.** The tests' defaults pin `vr_arena_wave_size 1` (the ladder's own counts, so tests 1-5 and 7 check what
they did); test 6 (the soak) and the new test 9 run at 1.75. Changed by design: test 3 no longer wants the boss at
the plan's head (it checks the boss is an enforcer and comes after the opening burst: one check more); test 5's
spawn-gap checks read `vr_arena_spawn_gap`, and its clamp check `AR_FIELD_MAX`. **New `vr_arena_test 9`** (vrarena,
the wave flow): the table above, the pool's growth, cap and difficulty share, a Nightmare wave 40 planned whole;
then waves 1, 5, 10, 20 live, the test felling one foe every 8 s: each wave puts its whole plan in the field, ends
with all of it felled, never has more up than its pool (and does fill it), trickles after its burst, and every gap
is within 1.5..3 s.

**Verified.** `build.sh`: 0 QC warnings. `vr_arena_test` 1 **257**, 2 **13**, 3 **9** (was 52: its plan checks ran
every frame while it waited for the boss, now once when it is in), 4 **7**, 5 **467**, 7 **19**, 8 **3**, 9 **37**,
all 0 failed; `vr_arena_perks_test 2` **15**/0. Test 9 live (peak / pool, gaps): wave 1 4/6, one gap 2.98 s; wave 5
7/7, 1.84..2.57 s; wave 10 8/8, 1.87..2.69 s; wave 20 8/8, 2.30..2.91 s. Soak (test 6, 1.75): 6 passed, edicts 308 ->
286 over waves 16..32 (peak 329; before 298 -> 299 at size 1), bodies 6, guns 25, drops 4, longest wave 48 s.

*Suggested `quakevr/checklist.txt` line:* `+ Survival Arena: waves are ~1.75x bigger but come in a few at a time (3 at
once, then one every 1.5-3 s after a kill, up to 6-8 up), out of your sight; the boss arrives mid-wave with a warning`

## Armour for armed humanoids, P1: limb plates and gameplay (worktree `armor`, branch `agent/armor`, 2026-10-28)

Builds on P0 and the Ranger's helmet (ROUND22.md) and the research's decisions (`qvr-kit/scratch/research_agarmor.md`).
The grunt only (the enforcer later).

**Limb plates** (`vr_agarmor.cpp`): pauldrons (upper arms), bracers (forearms), thighs, shins, left and right, made like
the cuirass from the authored rig's weights (each the triangles weighing past 0.5 on its bone), not subdivided (a
quarter of the triangles), painted with the cuirass's lames. Ten pieces, a bit each (`vr_agarmor_pieces` 1023 all; 1
cuirass, 2 helmet, 4/8 pauldrons, 16/32 bracers, 64/128 thighs, 256/512 shins). vrgrunt: 3460 triangles a body (was
2140), 54 bone slots (`maxSlots` 128). `vr_agarmor_check` 5/5 (its cap now 4000 triangles).

**Server to client**: QC keeps `.vr_ar_type` and `.vr_ar_mask` (vr_fields.inc); `agarmor::serverFrame` (VR_ServerFrameEnd)
sends each entity's change as `QVR_SVC_ARMOR` (reliable), `serverWriteAll` the whole set after a clear with the spawn
state (loads, new maps). The client draws: `vr_agarmor_set` (debug, per entity) > `vr_agarmor` (debug, forced) > the
server's word. `vr_agarmor_set` in a live game sets the QC armour (`VR_AgArmor_SetCmd`: pools and all).

**No builtin slots left** (the table's static assert): the piece at a point is `ragdollbone(e, p, QVR_ARMOR_PIECE_AT)`
(-1000; `box3d::boneAt`'s bone mapped to a piece: head the helmet, spine/chest the cuirass, the limb bones theirs) and a
piece's place `limbpiece(e, piece, 10 / 11)` (origin / angles of its own model `"<model>#plate<n>"`, a derived .mdl made
from the shell, rigid on three body vertices under it, as drawn by `hitmodel::vertexPoint`, else its rest pose).

**Gameplay** (`QC/vr_agarmor.qc`): pools from Quake's totals (100/150/200) by share (cuirass 30, helmet 12, pauldrons 7,
bracers 6, thighs 8, shins 6, out of 96). `T_DamageDeal` before the target's own save (after Quad: it does not pass
through): one hit's piece by its point; a shotgun's pellets each on their own piece (`AddMultiDamage`), the blast split
by their counts; a blast or a beam by the pieces facing it (each its share of the facing body's shares). `vr_agarmor_rule`
0 the player's rule (ceil(type x part) from the pool), 1 the plate stops the part while its pool lasts. Helmet
(`vr_agarmor_helmet_head` 1): halves the headshot multiplier's extra and stops head pops. `vr_agarmor_block_cuts` 1: no
cut or pop under an unbroken limb plate (living and corpses: `VR_AgArmor_Guard`). Absorbed: sparks, a ricochet. At 0:
the bit off, `items/armor1.wav`, the plate as debris (MOVETYPE_BOUNCE, `vr_rigid` 1, not FL_FORCEGRABBABLE, fades after
`vr_agarmor_debris_time` 10 s over a second). Corpses keep their plates; `ThrowHead` sheds them. Spawn roll
(`VR_AM_Spawn`): `vr_agarmor_skill0..3` (chance 0 / 0.15 / 0.35 / 0.6), `_type` (best type 1/1/2/3, rolled up to it),
`_pieces` (2/2/5/10: the cuirass, the helmet, the rest at random among the limb plates). Not `random()`: a
low-discrepancy sequence offset by the spawn point, so the game's other rolls don't shift. Menu: Combat > Armed Grunts >
Armour; Debug > Tests > Armour on Armed Grunts.

**Test** `vr_agarmor_test 1` (18/18): pieces found at their places; rule 0 green 10 -> 7 (pool -3), red 10 -> 2; rule 1
10 -> 0, 60 past a pool of 21 -> 39 and the cuirass breaks (mask 1022), its debris `#plate0` rigid, not force-grabbable;
headshot 15 on a helmet -> 9 and the pop stopped, without -> 15 and the pop stands; pellets 3+3 split over the cuirass
and a pauldron (-4 each, health -16); a cut stopped under a pauldron, let through after it broke; an unarmoured grunt
takes 10 of 10 and 13 of 13; a gibbing sheds 8 of 8; skill rolls over 200 spawns 0/30/70/121 (chances 0/.15/.35/.6);
the debris gone after its fade. `vr_agarmor_test 2`: three grunts in full green, yellow, red (`scratch/armor_sheet.png`,
a low-impact eyeshot). Build 0 QC warnings.

**Open**: the enforcer's shells; the toolgun's spawn page row; damage skins at 66/33%; a cut limb (block_cuts 0) does not
shed its plate as debris (it collapses with the limb); the piece-at-point uses the entity's current frame while a
plate's place uses the kept poses (the test puts its frame back between hits; a real hit traces the same frame);
the default Normal 15% changes old tests that kill armed grunts at skill 1 by exact damage (none run here).

Checklist:
- `[ ] Armour P1 {test:vr_agarmor_test}: Normal/Hard/Nightmare maps with armed grunts: some wear plates (Hard: up to yellow, 5 pieces; Nightmare: red full sets); shots on a plate spark and ring and do less; a plate broken falls off (you can't force-grab it) and fades; helmets stop head pops; blades don't cut an armoured limb until its plate breaks; Combat > Armed Grunts > Armour's rules`

## Armour P2: the enforcer, damage marks, the toolgun's option (worktree `armor`, branch `agent/armorp2`, 2026-10-28)

**Enforcer** (`vr_agarmor.cpp` `wearers`): the same ten pieces from `vrenforcer.rig2.txt` (and `enforcer.mdl`'s, the same
mesh). Its power pack (chest-weighted, x -10.5 to -23 and up past its head) is its own: the enforcer's cuirass stops at
its back's surface (`Wearer::backCut` -10.5, a plane folded into the clip field), a breastplate; its pads are under the
pauldrons. A hit on the pack is no plate's (`pieceAt`: the point taken back to the rest pose by the cuirass's frame as
drawn; behind the cut -1). Gameplay identical: `VR_AgArmor_Spawn` takes kinds 0 and 1 (decision: no free set, the
grunts' chances). `vr_agarmor_check` with no model checks both (10/10: enforcer 3408 triangles, 54 slots, at rest 116
of 1097 plate vertices hidden, 0.27 deep); `vr_agarmor_check <model>` one.

**Damage marks**: QC `.vr_ar_wear` (bits 0-9 at two thirds of a pool or under, 10-19 at a third; `VR_AgArmor_Wear` after
each hit, cleared by Give, Break, Shed) sent in `QVR_SVC_ARMOR` (two more shorts). `make_agarmor.py`: nine skins,
type - 1 + 3 x wear (1: scratches and dents; 2: more, chips to bare metal, scorch and soot). The client draws a body's
pieces in up to three armour entities, one per wear (`maxWorn` 128); a broken plate's debris uses the battered skin.
`vr_agarmor_wear` 1/2 (debug) draws every piece so. The 66% look is subtle at a distance (the 33% one reads).

**Toolgun**: Spawn > Monsters > Armed Humanoid has Armour (None / Green / Yellow / Red / Random) and Pieces
(Chest+Helmet / Full / Random: the cuirass, the helmet, each limb plate at 50%) under the hands (`vr_toolgun_armed_armor`
0, `_pieces` 1; QC `VR_AgArmor_Toolgun` on each one spawned; None strips the skill's roll too). Reset and Clear put both
back. `vr_toolgun_armed_armor_test`: grunt and enforcer by every Armour x Pieces, then Reset and Clear (32/32).

**Tests**: `vr_agarmor_test 1` 26/26 (P1's 18, the wear thresholds, the enforcer's places, its pack no plate, its debris
and skin, the skill's roll dressing it, its wear reaching the client: `ragdollbone(e, [what], QVR_ARMOR_CLIENT)` -1001);
`vr_agarmor_test 3` enforcers in green (facing), yellow (side on), red (back: the pack), `4` yellow grunts whole, at 2/3
and 1/3 (Debug > Tests). `scratch/armor_p2_sheet.png` (low-impact eyeshots). Build 0 QC warnings.

**Open**: the default Normal 15% now also dresses enforcers (old tests that kill enforcers by exact damage at skill 1 may
shift); the limb plates share the cuirass's atlas, so their marks repeat its pattern.

Checklist:
- `[ ] Armour P2 {test:vr_agarmor_test}: armed enforcers wear plates too (Hard/Nightmare), the cuirass a breastplate that leaves the power pack bare (shots on the pack go through); plates look dented and scratched at 2/3, battered and scorched at 1/3 (Debug > Tests > Armour Damage Marks There); toolgun Spawn > Monsters > Armed Humanoid: Armour and Pieces apply to grunts and enforcers, Reset/Clear put them back`

## Survival Arena: armoured foes (agent/arenaarmor)

Vittorio: armoured enemies in the arena, more for bosses, rising with the wave and the difficulty. `QC/vr_arena.qc`:
the plan now dresses its grunts and enforcers (`VR_ARENA_PlanArmour`, after `VR_ARENA_PlanFoes`), and a wave foe
gets the plan's armour at spawn (`VR_AgArmor_Give`) while the skill's own roll is held (`vr_ar_hold_roll` around its
spawn call: no double roll, the skill's sequence does not move). Outside the arena nothing changes.

**The roll.** No armour before `vr_arena_foe_armor_from` (4) as the difficulty counts gates (`VR_ARENA_WaveAdj`: 7 /
4 / 2 / 2). The chance: 10% at that wave, 3% more a wave, times the difficulty's factor (`VR_ARENA_Skill` 0.75 / 1 /
1.25 / 1.5) and `vr_arena_foe_armor_scale`, capped at 85%. The best type follows the weapon ladder: green until the
foes have carried the second rung two waves (`AR_W_TIER2 + 2`: wave 12 at Normal, when a super shotgun or nailgun can
be off a body), yellow from there, red from `AR_W_TIER3 + 4` (22: the launchers four waves about); a foe's type 1..best
evenly. Plates: the cuirass and the helmet, one limb plate more every 3 waves by the factor. A grunt or enforcer
**boss** is always armoured from the armour's first wave, a type above the wave's best, 4 more plates; a boss wave's
other humanoids have 15% more (its guard). The rolls are the wave seed's own salted stream (the chance and the type a
low-discrepancy pair off it), so a seed replays them and the wave's guns and gaps are exactly what they were.

| wave | 2 | 4 | 7 | 10 | 12 | 15 | 20 | 25 | 30 |
|---|---|---|---|---|---|---|---|---|---|
| Easy (from 7) | -- | -- | 8% G 2 | 14% G 2 | 19% G 3 | 26% G-Y 4 | 37% G-Y 5 | 48% G-R 6 | 59% G-R 7 |
| Normal (from 4) | -- | 10% G 2 | 19% G 3 | 28% G 4 | 34% G-Y 4 | 43% G-Y 5 | 58% G-Y 7 | 73% G-R 9 | 85% G-R 10 |
| Hard (from 2) | 12% G 2 | 20% G 2 | 31% G 4 | 42% G-Y 5 | 50% G-Y 6 | 61% G-Y 7 | 80% G-R 9 | 85% G-R 10 | 85% G-R 10 |
| Nightmare (from 2) | 15% G 2 | 24% G 3 | 38% G 4 | 51% G-Y 6 | 60% G-Y 7 | 74% G-Y 8 | 85% G-R 10 | 85% G-R 10 | 85% G-R 10 |
| boss (grunt/enforcer) | always from the first armour wave: a type above the wave's best (yellow at 5), the wave's plates + 4 (6 at Normal wave 5, 7 at Hard/Nightmare) | | | | | | | | |

(chance, the types a foe may wear, plates; G green, Y yellow, R red.) The enforcer bosses are the armoured ones (waves 5
and, at Easy, 12; both of Nightmare's two); the ogre and death knight bosses have no plates (the shells exist for the
grunt and the enforcer only), so their waves' armour is their guard's +15%.

**The fit.** Green at wave 4 against the shotgun and the nail pistol: a green cuirass (31) saves 30% of a hit, so a
shotgun blast on the chest (24) loses 8 to it and four break it -- a grunt takes about one blast more. Yellow
arrives as the super shotgun and the nailgun do, red after the launchers; a plate is a pool, so the stronger rungs
break it in a hit or two. The drops stay as they are (an armoured foe is a few more shots, which the guns lying about
and Quartermaster cover); the wave sizes (x1.75) need nothing: the chance is per foe. **Score:** an armoured kill pays
a quarter more per type (`AR_ARM_PAY`: 1.25 / 1.5 / 1.75; the wave 5 boss in yellow 600, not 400). **Perk:** Can Opener
(the pool's eighth, `ARK_OPEN`, a crowbar on its pedestal): a plate pays twice from its pool for what it saves from
the player's hits (`VR_ARK_PlateCost`, asked in `VR_AgArmor_Damage`), so it breaks in half the hits; the save while
it lasts is unchanged. **Readout:** `ARENA - Wave 12: 9 foes, 3 in armour (score ...)` when any are.

**Settings** (Combat > Survival Arena > Armour, archived, with help): `vr_arena_foe_armor` 1 (Armoured Foes),
`vr_arena_foe_armor_scale` 1 (Armour Chance, 0..4), `vr_arena_foe_armor_from` 4 (Armour From Wave, 2..30). Perks
Offered now goes to 8.

**Tests.** New `vr_arena_test 10` (vrtesthall with `vr_arena 1`, or vrarena): the table's numbers at each skill;
400 rolls per (wave, skill) cell (the first wave and the one before, 12, 24, 36, a boss before and at the first armour
wave and at 24, a boss wave's guard at 15) within 3% of the table, every type within it, every armour the cuirass,
the helmet and the table's plates; plans of waves 1..30 at each skill: none before the first wave, only humanoids, each
humanoid boss a type above; the off switch, the scale, the first wave knob; the plan's guns the same with it on or off;
live: wave 5 at Normal with `vr_agarmor_skill1 1` (the skill's roll certain) -- its foes wear exactly the plan's
armour, the skill's sequence did not move, the boss enforcer in yellow with 6 plates, its kill 600; a grunt spawned
outside the director still gets the skill's roll. Fixed on the way: `VR_ARENA_BossMidway`'s rotation now carries the
armour with its foe (the first live run had the boss bare and a grunt in its armour). The test defaults pin the three
new knobs. `vr_arena_test 5`'s perk-count clamp check reads `ARK_POOL_N`.

**Verified.** `build.sh`: 0 QC warnings. `vr_arena_test` 1 **257**, 2 **13**, 3 **9**, 5 **467** (vrtesthall), 4
**7**, 6 (the soak, 32 waves) **6**, 7 **19**, 9 **37**, 10 **160** (vrarena; also 160 on vrtesthall), 8 **3** (e1m1);
`vr_arena_perks_test` 1 **19**, 2 **15**, 3 **43** (vrtesthall: two checks need a map's item points), 4 **14**; outside
the arena `vr_agarmor_test 1` **26** -- all 0 failed. The rolls land within 0.025 of the table in every cell (e.g. Normal
wave 12: 0.3425 for 0.34; wave 24: 0.70 for 0.70).

*Decisions:* the gates move in whole waves like every arena gate (`VR_ARENA_WaveAdj`), so Hard and Nightmare start at
wave 2 (12% / 15%, green chest and helmet); an Easy run's first boss (wave 6) comes before its first armour wave and
is bare. Bosses past the enforcer cannot wear plates, hence the guard bonus. A roll's type is even over 1..best (the
skill roll's rule), so yellow and red come in gradually.

*Suggested `quakevr/checklist.txt` line:* `+ Survival Arena: armoured foes from wave 4 (Normal; 7 Easy, 2 Hard and
Nightmare), more of them, more plates and heavier kinds as the waves go; the enforcer boss in a kind above; armoured
kills pay more; Can Opener perk (Combat > Survival Arena > Armour) {test:vr_arena_test 10}`

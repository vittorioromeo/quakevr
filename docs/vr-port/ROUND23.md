# Round 23: after 1.1.0-beta4

A per-feature reference (condensed from the per-worker notes; current behaviour only). Round 22 (`ROUND22.md`) covers
1.0.1 to 1.1.0-beta4. Add new work as a section per feature (or fold it into its feature's section): what it does,
settings and defaults, files, tests, open issues. Run tests with `bash C:/OHWorkspace/qvr-kit/run.sh <name> -Filter
"..." -QuitOn "..."`; arena tests need `developer 1; vr_arena 1` (vrtesthall) or `map vrarena`, and `god`.

Sections: 1 Survival Arena: map; 2 director and waves; 3 difficulty; 4 foe armour and perks; 5 cleanup and loot;
6 polish (readout, countdown, arrivals, guns); 7 Armour for armed humanoids; 8 Armed-humanoid motion and strafe;
9 Ammo-screen colours; 10 Suggested checklist lines still pending.

## 1. Survival Arena: the map (`vrarena`)

Reached from Play > Maps > Survival Arena (`command("Survival Arena", "map vrarena")`, `pagePlay()` in
`Quake/vr/vr_menu.cpp`). Generator: `Misc/quakevr/maps/vrarena_gen.py` writes `quakevr/maps/vrarena.map` and (with
`--compile --preset final --holes 400128`) its `.bsp/.lit/.lux`, all committed together. Compiled with ericw-tools
2.0.0-alpha11 (`-bsp2 -wadpath`, vis, light `-extra4 -dirt -bounce -lit -lux -lightgrid`). `--check` reads the written
file back (spawns, items, steps, brush interpenetration, features in the open ring) and refuses to compile if points are
missing; `bsp_holes.py`: 0 holes.

- **Shape.** Sealed octagon 1728 across (half-width `H` 864), corners cut 200, floor 0, ceiling 384 (`ZH`, 11.7 m).
  Central dais (96 high, four 6-step flights; `info_player_start` 0 0 120); four corner towers 256 high with pinwheel
  staircases (16 high x 16 deep treads, the climbable minimum); wall walk at 256 along the east and west wall joined to
  tower tops by 64-wide bridges; eight covers 48 high, 96 wide at 512 out (45 degrees apart; 48 high so a standing eye
  at 56 is not blocked); open ring 700..1000 out has no feature below 160.
- **Points.** 12 `info_arena_spawn`: `kind 1` ground (four at +-640 +-640 0), `kind 2` high (tower tops 402 402 256,
  walk landings 736 +-288 256); 3 `info_arena_item` (196 196 48, -196 196 48, 196 -196 48); 24 `light`; worldspawn
  `light 90`, `_qvr_prelit 1`, `worldtype 2`, `_vr_debris 0`. 230 brushes, 60 entities (20 solid, 40 points). Spawn
  points sit 24 above their floor.
- **Look (id's base set).** Worldspawn lists `quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad`
  (`id_textures.wad` is git-ignored, built by `Misc/trenchbroom/make_id_wad.py` from the player's own paks; only the
  compiled bsp ships). `--dev-textures` rebuilds with the dev grids; `main()` refuses a texture not in a listed WAD.
  Faces are laid by `laid()` in three modes (world grid, `fitv` whole tiles down a band, `fit` whole tiles both ways) so
  crates, lamps, flutes and trim land on whole tiles. Floor `ground1_2`, walls `tech14_1` between `tech04_1` skirting/
  rail/cornice bands and a `metal4_4` wainscot, `tech04_3` fluted uprights (12, with `tlight01` lamp hoods), `metal1_1`
  ceiling with eight `light3_3`/`ceil1_1` coffers, `sfloor4_2` decks, `crate0_*` covers, `plat_top1` spawn pad. Light
  emission comes from `quakevr/relight_textures.cfg` plus fullbright pixels, not from a `qvr_light` name (nothing in
  `Quake/` uses it). Texture fit was verified numerically (106 object faces within 0.0017 texel).
- **Open (his eye):** whether the trim families, the upright spacing, the coffers and 96-wide covers read well;
  `MAPPING.md`'s rule about committed bsps carrying id art is left as written (vrarena joins vrcalibration and the hubs).
- **Tests:** `python Misc/quakevr/maps/vrarena_gen.py --check` (`arena_map: 12 spawns, 3 items, 0 bad steps`,
  `arena_check: done`); `vr_arena_test 4` (below) 7/0.

## 2. Survival Arena: the wave director (`QC/vr_arena.qc`, `vr_arena_perks.qc`)

Runs only when the map is `vrarena` or `vr_arena 1` (`ar_mgr` set, `VR_ARENA_ON`); every shared-file hook returns at once
otherwise (combat.qc `Killed`, items.qc `armor_touch`, vr_decap.qc, vr_enemyguns.qc, world.qc `VR_ARENA_WorldSpawn`).
No engine code. Plan arrays survive save/load. Foes are spawned through their kind's own spawn function and handed the
player by `VR_ARENA_HuntPlayer` (any other start leaves `takedamage 0`). The skill is read from the cvar at world spawn
(`VR_ARENA_SkillLevel`; the `skill` global is not set yet).

- **Plan** (`VR_ARENA_Plan`): 12 recipes for waves 1..12, then one rule (13+: same ladder, thicker). Wave 1 one or two
  unarmed grunts; wave 2 an unarmed grunt and a nail pistol; wave 3 the first zombie. Classics at their waves (Normal):
  zombie 2, knight 4, dog 6, ogre 8, fiend 12, death knight 16. Waves are rolled with a Levy triple from `vr_arena_seed`
  and the wave (0: new ladder each run; N: replays). A wave's size = recipe groups x `vr_arena_wave_size`, then the
  difficulty; signature foes and the boss stay one. `AR_PLAN_MAX` 40 (a Nightmare wave 40 plans 32).
- **Alive pool and flow** (`VR_ARENA_Pool`): `vr_arena_alive_max` 6 up at once at wave 1, +1 every `vr_arena_alive_grow`
  4 waves, capped at `vr_arena_max_alive` 8 (clamp `AR_FIELD_MAX` 16). A wave opens with `vr_arena_burst` 3; then a
  freed slot is refilled `vr_arena_spawn_gap` 1.5 to 2x that at random later (a fall wakes the director; its think lands
  on the gap's end). A wave stalled 300 s after its last arrival is called off.
- **Where.** Floor places (`kind 1`) for walkers, high places (`kind 2`) for fiends and enforcers (dropped 96 onto the
  ledge), each place used once per wave. A foe comes in out of the player's sight (outside ~60 degrees of view or behind
  a wall) at least 320 units off his eyes; else far in sight; else any free place. Maps without places use a ring 400
  around the player (same three passes). A place is skipped if a non-brush model stands in the foe's box
  (`VR_ARENA_BoxBlocker`, hull 64 wide for ogre/fiend/death knight/dog, 8 pad, plus the 96 column for high places); no
  free place: the foe waits a beat. Medkits/armour/pedestals use `VR_ARENA_FreeNear` (point, or 64/128 off it in 8 ways).
  Every arrival gets the teleport flash and a `misc/r_tele*.wav` (`VR_ARENA_Tfog`); `VR_ARENA_ScanSpawn` counts
  `ar_overlaps`.
- **Boss.** Every `vr_arena_boss_every` 5 waves (Easy 6); an enforcer with a rolled gun until wave 10, an ogre until
  16, a death knight after (earlier by difficulty); health x `vr_arena_boss_health` 1.25; worth 4 kills. Planned first
  but enters halfway through its wave after the burst, announced 3 s before. Nightmare brings two (second at half bulk).
- **Player side.** Run starts stripped (`VR_ARENA_StripPlayers`); guns come off bodies (`VR_AG_SoldierDrop`). Each clear
  drops `vr_arena_medkits_after` 2 medkits and every `vr_arena_armor_every` 4 waves an armour (light, then heavy and light
  by turns from the run's wave 12) on the item points (or the ring). Score: `vr_arena_score_kill` 100 x the gun's rung
  (fists/shotgun/nail pistol 1; SSG, nailgun, super nailgun, ogre, fiend 2; RL, lightning, GL, death knight, proximity,
  laser 3), boss x4, armoured kill x1.25/1.5/1.75 by type (`AR_ARM_PAY`), plus 250 x wave per clear. A fall ends the run
  (`YOU FELL AT WAVE n`) and restarts after 10 s.
- **Readout** (polish): every arena line is the engine's `viewmessage` (eye level in the headset, held centre print on
  a flat screen; `VR_ARENA_PrintAll` / `ShowAll` / `Show`), first line highlighted: `SURVIVAL ARENA` / `Normal: you start
  unarmed`; `WAVE 7` / `9 foes, 3 in armour` / `score ...`; `WAVE 5 - BOSS WAVE`; `WAVE 6 CLEAR`; `A BOSS IS COMING`;
  `YOU FELL AT WAVE 9`. One message at a time; console copy `ARENA> ...`; nothing on the wrist.
- **Rest and countdown.** `vr_arena_wave_rest` 5 s (difficulty-scaled 6.7/5/4/3.3; replaces `vr_arena_wave_delay`) then
  `vr_arena_countdown` 5 s (`Wave 7 in 5...`; boss: `BOSS WAVE 10 in 3...` / kind). The first wave has both, so a run
  starts ~10 s after the map. `VR_ARENA_FastForward` skips both.
- **Weapon schedule** (`VR_ARENA_PickGun`, `PlanTrickle`): each gun has its own wave (Normal; +3 Easy, -2 Hard, -4
  Nightmare, never before 3), first as a trickle (2 waves on 1 foe, 2 waves on 2, dealt at random over the humanoids,
  newest first), then in the pool: a newcomer starts at 35% of its share, ramps over 4 waves, holds 6, fades 8% a wave to
  a 20% floor. Bosses use `VR_ARENA_PickBossGun` (tiers at `AR_W_TIER2` 10 / `AR_W_TIER3` 18). Pack guns only with their
  data (`VR_AG_Usable`).

| gun | eligible | on 1 foe | on 2 foes | in pool from |
|---|---|---|---|---|
| shotgun, nail pistol, axe | base | - | - | 3 |
| super shotgun | 3 | 3-4 | 5-6 | 7 |
| nailgun | 5 | 5-6 | 7-8 | 9 |
| super nailgun | 7 | 7-8 | 9-10 | 11 |
| grenade launcher | 10 | 10-11 | 12-13 | 14 |
| proximity gun (Armagon data) | 12 | 12-13 | 14-15 | 16 |
| rocket launcher | 13 | 13-14 | 15-16 | 17 |
| lightning gun | 16 | 16-17 | 18-19 | 20 |
| laser cannon (Armagon data) | 19 | 19-20 | 21-22 | 23 |

**Settings** (engine cvars; menu Combat > Survival Arena, with sub-pages The Waves, Armour, Cleanup):

| cvar | default | meaning |
|---|---|---|
| `vr_arena` | 0 | run the waves in any map (vrarena needs no flag) |
| `vr_arena_wave_size` | 1.75 | multiplier on crowd groups |
| `vr_arena_alive_max` / `_alive_grow` / `_max_alive` | 6 / 4 / 8 | opening pool, waves per +1, ceiling (1..16) |
| `vr_arena_burst` / `_spawn_gap` | 3 / 1.5 | opening burst; refill gap (to 2x) |
| `vr_arena_wave_rest` / `_countdown` | 5 / 5 | s rest, s countdown (0..10; 0 none) |
| `vr_arena_boss_every` / `_boss_health` | 5 / 1.25 | 0: no bosses |
| `vr_arena_medkits_after` / `_armor_every` | 2 / 4 | medkits 0..4; armour gap, 0: none |
| `vr_arena_score_kill` | 100 | base score per kill |
| `vr_arena_seed` | 0 | 0: random ladder each run |
| `vr_arena_test` | 0 | test selector below |

**Tests** (`vr_arena_test N`, verdict `arena: test N over: P passed, F failed`; defaults pin `wave_size 1`,
`wave_rest 5`, `countdown 5`; 6 and 9 run at 1.75, 6 and the perk tests without countdown). Latest results, all 0
failed: 1 plans/hooks/score/supplies **257**; 2 live waves **13**; 3 boss wave **9**; 4 places (vrarena **7**, vrtesthall
**5**); 5 difficulty **491**; 6 32-wave soak **8** (edicts plateau ~290-330, bodies ~6, guns ~25, drops 4, longest wave
~48 s); 7 cleanup (vrarena) **19**; 8 e1m1 untouched **3**; 9 wave flow (vrarena) **37**; 10 foe armour **160**; 11 loot
gate (vrtesthall) **25**; 12 gun schedule **56**; 13 arrivals/overlaps (vrarena) **12** (56 spawns, 0 overlaps); 14 rest and
countdown timed **7**. Example: `run.sh <name> -Filter "arena:" -QuitOn "arena: test 5 over" -Timeout 300 -Script
"developer 1; vr_arena 1; vr_arena_seed 7; map vrtesthall; wait15; god; vr_arena_test 5; wait6000"` (test 6 needs
`-Timeout 1500`). Known: test 1 is 256/1 on vrarena (`a fallen monster is not alive`, harness case; 257/0 on vrtesthall).

**Open:** whether the rest + countdown (10 s before wave 1) is too long; whether Quartermaster should reach a player
holding nothing; whether the next wave should wait for a pedestal choice (today it does not).

## 3. Survival Arena: difficulty (`vr_arena_skill_impact`)

`VR_ARENA_Skill(easy, hard, nightmare)` maps the game's skill to a factor: at impact 1, **0.75 Easy / 1 Normal / 1.25
Hard / 1.5 Nightmare**; impact 0 = no effect; 2 doubles the step from 1; never below 0. It is the arena's own knob and
touches nothing of `vr_agrunt_ai_skill` (the armed humanoids' AI). Gates move in whole waves (`VR_ARENA_WaveAdj`: +3
Easy, -2 Hard, -4 Nightmare, x impact, never before wave 2). Opening line names the difficulty. Menu: Combat > Survival
Arena > Difficulty Impact (0..3, extendable to 10), Skill cycle.

Effects (Easy / Normal / Hard / Nightmare): wave size (wave 11: 5 / 7 / 8 / 10); alive cap 6 / 8 / 10 / 12 (12 -> 9 /
12 / 12 / 12); rest 8 / 6 / 4.8 / 4 s (spawn gaps 2-4 / 1.5-3 / 1.2-2.4 / 1-2 s); second gun rung at wave 13 / 10 / 8 /
6, third 21 / 18 / 16 / 14; knight 7 / 4 / 2 / 2, dog 9 / 6 / 4 / 2, ogre 11 / 8 / 6 / 4, fiend 15 / 12 / 10 / 8, death
knight 19 / 16 / 14 / 12; unarmed share of early humanoids 0.30 / 0.20 / 0.15 / 0.10; boss every 6 / 5 / 5 / 5 with
Nightmare's two; ogre boss from 13 / 10 / 8 / 6, death-knight boss from 19 / 16 / 14 / 12; enforcer boss health 76 / 101 /
126 / 151; medkits 3 / 2 / 2 / 1; armour every 3 / 4 / 5 / 6 waves; kill score 75 / 100 / 125 / 150; perk pedestals 2 /
3 / 4 / 5. Wave 13+ thickening stays anchored at 13. Clamps: group >= 1, field 1..`AR_FIELD_MAX`, wait >= 1 s, medkits
0..4 (`AR_KIT_MAX`), armour gap 0..12 (`AR_ARMOR_MAX`), boss health >= 1, score >= 1, skill 0..3. Not touched: AI, guns
rolled, places, the heavy armour's wave, the fall path, the 300 s call-off.

**Open:** whether Easy should soften boss kinds; whether Nightmare's second boss deserves a knob; whether the impact
should reach the pedestal count.

## 4. Survival Arena: foe armour and boss-wave perks

**Armoured foes** (`VR_ARENA_PlanArmour` after `PlanFoes`; spawn gives the plan's armour via `VR_AgArmor_Give` while the
skill's own roll is held, `vr_ar_hold_roll`; `VR_ARENA_BossMidway` carries armour with its foe). Grunts and enforcers
only. None before `vr_arena_foe_armor_from` 4 (as gates move: 7 / 4 / 2 / 2). Chance 10% at that wave + 3% a wave, x the
difficulty factor x `vr_arena_foe_armor_scale`, cap 85%. Best type: green until `AR_W_TIER2 + 2` (wave 12), yellow
after, red from `AR_W_TIER3 + 4` (22); a foe's type is even over 1..best. Plates: cuirass + helmet, +1 limb plate every 3
waves (x factor). Grunt/enforcer **bosses** are always armoured from the first armour wave, a type above the wave's best,
+4 plates; a boss wave's other humanoids get +15% (the ogre and death knight bosses have no shells). Rolls use the
wave seed's salted stream, so guns and gaps are unchanged. Normal: wave 4 10% green 2 plates; 12 34% G-Y 4; 20 58% G-Y
7; 25 73% G-R 9; 30 85% G-R 10. Readout: `Wave 12: 9 foes, 3 in armour`. Settings (Combat > Survival Arena > Armour):
`vr_arena_foe_armor` 1, `vr_arena_foe_armor_scale` 1 (0..4), `vr_arena_foe_armor_from` 4 (2..30). Test: `vr_arena_test
10` (160/0: table within 3% over 400 rolls per cell, plans at each skill, live wave 5 foes and the 600-point yellow boss).

**Perks** (`vr_arena_perks` 1, `vr_arena_perk_count` 3, 1..the pool of 8; scaled by difficulty 2/3/4/5). A boss wave's
clear lights that many pedestals (`VR_ARENA_PedPlace`: off the item points, 96 further out; a map without item points
uses the drop ring at 220 + 96); walking over one takes the perk for the run and no later boss offers it; unclaimed
pedestals stand until the next boss offer. Perks (each hooks a place the game reports and returns unless taken,
`VR_ARK_Running`): **Quad Strike** (melee kill adds 2 s Quad; only ever adds time), **Neck Snapper** (snapped neck +20
health, decap hook), **Iron Skin** (picked-up armour +25: 125/175/225, at `armor_touch`), **Second Wind** (max health
+15), **Feeder** (+5 health per kill, at most 1/s), **Quartermaster** (each clear +15 ammo to the held gun, via
vr_ammoutil), **Spoils** (twice `vr_arena_gun_max` fallen guns stay), **Can Opener** (plates pay twice from their pool
for what they save, `VR_ARK_PlateCost`). Tests: `vr_arena_perks_test 1..4` (pool and offer 19, pedestals 15, every perk
through the game's calls 45 (43 on vrtesthall), full-map drive 14, prints `arena_full:`); scenario
`quakevr/scenarios/arena_intro.txt` (boss every wave, so the offer comes at once).

## 5. Survival Arena: cleanup and loot gate

Active only while an arena runs. `VR_ARENA_MonsterDie` -> `VR_ARENA_BodyDown`; `VR_ARENA_WaveClear` ->
`VR_ARENA_DropsExpire`; `VR_ARENA_Frame` fades and removes what is due. Never taken: a thing a hand holds
(`carry_player`, `mainhand_held`/`offhand_held`), a force-grabbed one (`fg_state`), a body still dying/gibbing (waited
10 s at most); let go of, it lies 3 s more. Settings (Combat > Survival Arena > Cleanup):

| cvar | default | meaning |
|---|---|---|
| `vr_arena_corpse_fade` | 14 | s a body (or head) lies before a 1 s fade, +-3 s; 0 never by time |
| `vr_arena_corpse_max` | 12 | bodies at most, oldest fades first; 0 no limit |
| `vr_arena_item_waves` | 2 | clears later that uncollected medkits/armour fade; 0 never |
| `vr_arena_gun_max` | 24 | foe-dropped guns lying at most (Spoils doubles it); 0 no limit |

In an arena `VR_EnemyWeapons_MakeRoom` uses `VR_ARENA_GunMost` and counts every `.vr_monster_drop` gun (player-dropped
guns never count). `VR_EnemyWeapons_MendFades` (vr_enemyguns.qc, run by every cap and twice a second by the cleanup)
resumes a stalled fade and removes one long overdue; humanoids no longer go for a gun fading past the cap (answers the
"stuck, non-interactable shotgun" report; not reproduced headless, so the cause is unconfirmed: if it shows again,
`developer 1` and look for a `dropcap:` line and note how the gun got there). Soak result: edicts flat ~290-330 over 32
waves (was 928).

**Loot gate.** `VR_WeaponUtil_EntIsEligibleForWeapon` (fist/axe/shotgun always, else "player holds it") answers with
`VR_Pack_WeaponAvailable(id)` while `VR_ARK_Running()`, so an ogre (`OgreDrop`, from wave 8) and ammo boxes can drop strong
guns whatever the unarmed player holds; guns the pack lacks stay out; `vr_enemy_drops 2` / `vr_ammobox_drops 2` still
stop drops; outside a run nothing changed. The foes' own guns never went through the gate. A blanket "eligible for
everything" grant was rejected (owning is the hand inventory; it would undo the stripped start). Test: `vr_arena_test 11`
(25/0). `vr_armedrepl.qc` (replacement, default off) uses the same gate; `vr_agrunt_repl_rare_map` keeps vrarena out of
the rare tier. **Open:** an ogre can leave a grenade launcher/rocket launcher from wave 8 (a wave-tier gate would be a
short follow-up); ammo boxes/crates in other arena maps can hold ammo for guns not yet held.

## 6. Survival Arena: review fixes (kept as constraints)

`VR_ARENA_ClearField` removes exactly `ar_slot == wave` (it used `findflags`, which matches shared bits) and lets go of
held foes via `VR_Carry_Forget`; Quad Strike only adds time. e1m1 with all monsters killed (`impulse 205`) prints no
error; eval.sh passes.

## 7. Armour for armed humanoids (grunt and enforcer; `vr_agarmor.cpp`, `QC/vr_agarmor.qc`, `make_agarmor.py`)

- **Pieces.** Ten, one bit each in `vr_agarmor_pieces` (1023 all): 1 cuirass, 2 helmet, 4/8 pauldrons, 16/32 bracers,
  64/128 thighs, 256/512 shins; limb plates built from the authored rig's weights (triangles weighing > 0.5 on a bone),
  painted with the cuirass's lames. vrgrunt 3460 triangles / 54 bone slots (`maxSlots` 128); enforcer 3408 triangles
  from `vrenforcer.rig2.txt` (its power pack is no plate's: the cuirass stops at `Wearer::backCut` -10.5, a breastplate).
  Checks: `vr_agarmor_check` (no model: both; 10/10, cap 4000 triangles).
- **Server to client.** QC `.vr_ar_type`, `.vr_ar_mask`, `.vr_ar_wear` (vr_fields.inc); `agarmor::serverFrame`
  (`VR_ServerFrameEnd`) sends changes as reliable `QVR_SVC_ARMOR`, `serverWriteAll` the whole set after a clear. Client
  priority: `vr_agarmor_set` (debug, per entity) > `vr_agarmor` (debug, forced) > server. Builtins are full: piece at a
  point is `ragdollbone(e, p, QVR_ARMOR_PIECE_AT)` (-1000), a piece's place `limbpiece(e, piece, 10/11)`, wear to the
  client `ragdollbone(e, what, QVR_ARMOR_CLIENT)` (-1001); plate models `"<model>#plate<n>"`.
- **Damage marks.** `.vr_ar_wear` bits 0-9 at <= 2/3 of a pool, 10-19 at <= 1/3 (`VR_AgArmor_Wear`); nine skins (type -
  1 + 3 x wear); the client draws up to three armour entities per body (`maxWorn` 128); `vr_agarmor_wear` 1/2 debug.
- **Gameplay.** Pools from Quake's totals (100/150/200) by share (cuirass 30, helmet 12, pauldrons 7, bracers 6, thighs 8,
  shins 6, of 96), `T_DamageDeal` before the target's save (after Quad): single hit by point, shotgun pellets each by
  piece (`AddMultiDamage`), blasts/beams by facing pieces. `vr_agarmor_rule` 0 = the player's rule (ceil(type x part))
  from the pool, 1 = plate stops the part while its pool lasts. `vr_agarmor_helmet_head` 1 halves the headshot extra and
  stops pops; `vr_agarmor_block_cuts` 1: no cut/pop under an unbroken limb plate (`VR_AgArmor_Guard`). Hits spark/ricochet;
  a broken plate leaves the body with `items/armor1.wav` as debris (bounce, `vr_rigid` 1, not force-grabbable, fades over
  1 s after `vr_agarmor_debris_time` 10 s); corpses keep plates, `ThrowHead` sheds them.
- **Spawn roll** (`VR_AM_Spawn`, grunts and enforcers): `vr_agarmor_skill0..3` chance 0 / 0.15 / 0.35 / 0.6,
  `_type` best 1/1/2/3 (rolled up to it), `_pieces` 2/2/5/10. Low-discrepancy sequence offset by the spawn point, not
  `random()`. The arena overrides it (section 4).
- **Toolgun.** Spawn > Monsters > Armed Humanoid: Armour (None/Green/Yellow/Red/Random) and Pieces (Chest+Helmet/Full/
  Random) via `vr_toolgun_armed_armor` 0, `_pieces` 1 (`VR_AgArmor_Toolgun`; None strips the skill roll); Reset/Clear
  restore. Menu: Combat > Armed Grunts > Armour; Debug > Tests > Armour on Armed Grunts / Damage Marks.
- **Tests.** `vr_agarmor_test 1` **26/26** (places, rules 0/1, helmet, pellets, cuts, enforcer pack, debris, wear
  thresholds, skill roll, client wear); `2` sheet of three grunts, `3` enforcers by facing, `4` yellow grunts at 2/3 and
  1/3 (eyeshots); `vr_toolgun_armed_armor_test` **32/32**.
- **Open:** a cut limb (block_cuts 0) does not shed its plate; limb plates share the cuirass atlas so marks repeat; the
  66% look is subtle at range; default Normal 15% changes old tests that kill armed humanoids by exact damage.

## 8. Armed-humanoid motion: choppy fix and strafe turn

- **Choppy fix.** `rig2::restoreModels` (vr_rig2.cpp) restored every saved lerpflag including `LERP_RESETMOVE`, which
  drawing the skinned copy spends; restored, the grunt/enforcer's move lerp reset every frame (steps up to 15 units, 10
  times a second, with arms, guns and armour). Now the move lerp's reset flag is left as drawing left it; frame lerp flags
  are restored as before. Test `vr_rig2_motion_test 300` (Debug > Automation > Monsters' Drawn Motion; vrtesthall,
  `vr_agrunt_test 1`, a walking knight): fails a drawn origin stepping > 8 units/frame; 3 passed 0 failed (grunt step 15.03
  -> 2.13, jerk 2.01 -> 0.15). Rig Studio subjects 13/14, aim 3/4, posekeys 13/14 fail identically without the fix.
- **Strafe turn.** `vr_agrunt_strafe_turn` 28 deg (AI Behaviour > Manner > Strafe Turn, beside Strafe Lean):
  `aglegs::lean` eases a yaw toward the sideways motion (sideways speed only, 0.18 s ease, never flipped) with the lean on
  every bone; `agbody::upper` (`hipsTwist`) takes it out of the hips' turn its chest shares (`vr_agrunt_upper_hips` 0.3)
  so chest, arms and gun stay aimed. `vr_agrunt_test 87` prints `aglean twist: ... PASS` (pelvis yaw 7.8 / 14.2 / 19.0 /
  21.4 deg at 20-40 / 40-60 / 60-80 / 80+ u/s, chest <= 0.01 deg, roll 4.1 at 80+).

## 9. Ammo-screen colours (`vr_view_weapon.cpp` `ammoWarning`, `idleAmmoWarning`)

A gun's ammo numbers go red when empty and orange under a quarter of its magazine, on by default: `vr_ammo_color_warn` 1,
`vr_ammo_color_low` 30 and `vr_ammo_color_empty` 0 (hues in degrees, or `"r g b"`), `vr_ammo_low_frac` 0.25. Menu: HUD and
Menus > Screens > Weapons' Ammo Screens. With reloading on: magazine rounds vs the gun's own magazine size (QC
`VR_ClipSizeOf`, sent to the client: a prop's extra byte in `U_QVR_WEAPONUID`, a holster's `STAT_QVR_HOLSTERCLIPSIZE0`;
shotgun 8, nailgun 24, nail pistol 12, revolver 6, Widowmaker 16, SSG 2: one shell left is not low); reserve never
counts. Reloading off: the ammo it fires from vs a magazine's worth. Guns with no magazine size (flare gun) and the
chainsaw go red only at 0. One rule, `gunWarning` (vr_view_weapon.cpp), serves held, holstered and lying guns, enemy-dropped
and never-held guns included (no "seen in a hand" limit). The tint goes through text3d `queue(..., tint)`; the CRT image is
keyed on it. Test: `vr_ammocolor_test 1` (Debug > Tests > Ammo Screen Colours; `vr_ammo_color_debug 1` logs) on e1m1,
**22 passed 0 failed** (cases 19..24 never hold the gun; Widowmaker cases skip without AD), `-QuitOn "ammocolor: .* passed"`;
`2` an eyeshot. The nail
pistol and revolver have no ammo screen by default (their Ammo Screen weapon setting).

## 9b. Per-weapon recoil kick and rise (`vr_weapons.inc` `RecoilKick`, `RecoilRise`; `vr_weaponfx.cpp` `recoilAmount`)

His backlog: per-weapon recoil kick and rise. Each weapon has two new multipliers, `vr_wofs_fx_recoil_kick_NN` and
`vr_wofs_fx_recoil_rise_NN` (Weapon Offsets > Effects: Recoil Kick Back, Recoil Muzzle Rise; 0..3x, extendable to 10x),
default 1 for every slot. A shot's drawn recoil is now: kick back = `vr_recoil_kick` cm x the weapon's Recoil Strength x
its Recoil Kick Back; tip up (and the small sideways turn) = `vr_recoil_rise` deg x Strength x its Recoil Muzzle Rise. The
globals stay what they were (his config holds 4.5 cm and 7.5 deg) and act as the base the per-weapon values scale;
Recoil Strength stays as the per-weapon overall (the AD guns' fire feel, the 2.5 burst cap and old migrations use it).
At the defaults the arithmetic is bit-identical to before, so no feel change and no migration (new cvars; no
`settingsVersion` bump). The heavy-weapon aim kick (`vr_heavy_recoil_*`, `w_hrecoil`) is separate and untouched.
Test: `vr_recoil_test` (Debug > Tests > Per-Weapon Recoil Test) with a weapon in the main hand: `map e1m1;
vr_weapon_grip_mode 1; impulse 9; wait2; impulse 154; wait5; vr_recoil_test`, `-QuitOn "vr_recoil_test: [0-9]+ passed"`:
**9 passed, 0 failed** (all 51 slots at defaults recoil as before; the held shotgun's Kick Back 2x / Rise 0.5x double its
drawn kick back 0.45 -> 0.90 units and halve its pitch and yaw through `recoilOffset`; no other slot's recoil moves;
another slot's 3x leaves the held one's; values restored).
## 9b. Relighting: Ambient Light Level (`vr_relight_minlight`, backlog "a basic ambient lighting level")

The Relighting page's "Minimum Light" row is now **Ambient Light Level** (same cvar, `vr_relight_minlight`, archived,
default 0 = unchanged; slider 0..64 step 4, light units: 32 a dim fill, 64 a clear one). It used to pass light
`-minlight N`, which (a) overrode a map's own brighter worldspawn `"light"` (command line beats map keys in ericw-tools 2)
and (b) was darkened by `-dirt` (ericw-tools: `-dirt` implies `_minlight_dirt 1`), so the darkest corners stayed black.
Now `scaledLights` (vr_relight.cpp) raises the worldspawn's `"light"`/`"_minlight"` to the level only where the map's own
is lower and sets `"_minlight_dirt" "0"` (a true floor). `relight_maps.py --ambient N` (alias `--minlight`) does the same
(`with_ambient`; the stamp unchanged at 0). Level 0 adds nothing, so the defaults' hashes and stamps are as before.
Test (`Misc/quakevr/review/relight_ambient_stats.py a.bsp b.bsp`: style-0 luxels, BSP29/BSP2 + .lit, verdict
`relightamb: PASS`): e1m1 relit in game (`vr_relight_quality 0`, AO 1.5 on) at 0 then 32 (`-QuitOn "relit in"`):
min 0 -> 16, p1 0 -> 16, p5 0.3 -> 16, luxels under 16: 19.7% -> 0%, median 49 -> 47 (faces unlit before now counted);
relight_maps.py at 0 / `--ambient 32`: the same (min 0 -> 16, p5 1 -> 16). `vr_menu_path_check "{menu:Graphics -
Relighting>Ambient Light Level}"`: 1 found; `writeconfig` writes `vr_relight_minlight "32"`.
## 9b. Nails smaller and faster (`vr_props.cpp` `Own::Nail`, QC `VR_NailSpeed`, `QC/vr_nail_test.qc`)

His backlog: nails "30-40%" smaller with an accurate hit box, and "15-20%" faster, the player's and the enemies'.

- **Size, drawn only:** `vr_nail_scale` 0.65 (new; VR Settings, the Rocket Size in Flight page: "Nail Size in Flight"),
  an own scale like `vr_rocket_scale` (props `drawnSize`): `progs/spike.mdl`, `s_spike.mdl`, `lspike.mdl` and, AD support,
  AD's `proj_ng.mdl`, `proj_ngred.mdl`, `proj_sng.mdl` (new model IDs), whoever fires them (traps, Scourges, Gremlins,
  armed grunts, MG3 and AD content). Engine-side, so AD's QC is untouched.
- **Hit box: already accurate, unchanged.** Every nail is a point (`setsize` 0). With precise hits (default) it meets a
  monster's drawn model grown by `vr_hit_tolerance_guns` 2; with `vr_hit_precise 0`, the monster's box grown by
  `vr_gameplayfix_missilesize` 2 (quakevr.cfg; Quake's 15). The player's hit box for enemy nails. Nothing bigger than the
  visible nail, so the smaller model needs no hit box change.
- **Speed:** `vr_nail_speed` 2065 (new, Combat > Weapon Damage > "Nail Speed"; was a constant 1750, x1.18) in
  `launch_spike` and `launch_lava_spike`: the nailgun, super nailgun and DoE lava nails, the player's and every monster's
  that fires them that way (armed grunts' nailguns, Scourges, Gremlins). `vr_nailpistol_speed` 2600 -> 3070 (x1.18;
  configVersion 215 moves configs still at 2600; his cfg holds 2600). Missiles have no per-axis `sv_maxvelocity` clamp
  (only a 3464 u/s length bound), so 3070 plus the hand's is not bent.
- **Not changed (decisions):** traps' nails (500 u/s: map timing), AD's own nail monsters (nail ogre, snowman, sentinel,
  Freddie: AD's skill speeds, decision 9 "AD's rules stay with AD's content"), and callers that set their own speed after
  `launch_spike` (wizard spit, hell knight bolts, Chthon, MG3 bosses: not nails).
- **Test:** `vr_nail_test 1` (Debug > Tests > Nail Pistol > Nail Speed and Size Test; `developer 1`, vrtesthall,
  `-QuitOn "nailtest: [0-9]+ passed"`): **7 passed, 0 failed**. Speeds flown (two frames' way): player nailgun, super
  nailgun 2065, nail pistol 3070, armed grunt's nailgun 2065 (before: 1750, 1750, 2600, 1750: the same run with the old
  values fails exactly those 4, x1.000). Hit box: a point; a sweep of nails at 2-unit offsets across a grunt met it
  from -10..14 (precise) and -12..14 (boxes), the middle always, never past 18. Drawn: `vr_physics_list` "drawn x0.650"
  (before x1.000). `vr_nail_test 2` hangs three nails 40 units before your eyes for 30 s (Debug > Tests > Nail Pistol).
  Seen in the test: nails spawned side by side in the same frame meet each other as they leave (the sweep fires them
  one at a time); no real weapon does that.
## 9b. Rig Studio: a pose's Off Hand On the Gun now holds (branch `agent/rigoffhand`)

His words: setting the off hand's behaviour per pose "doesn't seem to work" (free, or on the weapon's grip). The combo,
the key's storage (`vr_agrunt_poses`: 30 numbers when an off-hand part is set), its reload and QC's choice of the deciding
key were all fine (Free works in every pose on every gun category). **Root cause: On the Gun was refused where it was
most wanted.** (1) A pistol carried low (Carry, Run, the take's lowest: barrel down past 27 degrees) is never cupped
(`lowCarry`, `vr_agarms_arm_clear`), and the key's On the Gun reached the client only as the Weapon Holds' Two-Handed
flag, so it was refused as well: his own shipped pistol Run key (17020, On the Gun since 2026-10-27) never cupped (9.3
units off the grip, not cupped). (2) A gun's second grip out of the off arm's reach (the Pickup's lowest: reach 1.21 of
the arm after sliding 5.4 back) let go, On the Gun or not. Free looked broken where Auto is already free (a pistol carried
low, the Pickup): nothing to change there.

**Fix.** QC's off-hand block (`QVR_SVC_ARMPOSE`, VR_AGP_FREE | VR_AGP_TWO announcing it) gains bit 32: its deciding key
chose On the Gun (`vr_armedgrunt_pose.qc`, sent with a single gun only). The client (`vr_hitreact.cpp`, `ArmPoseMsg` /
`ArmPose::keyGun`, in `offKeyed`'s padding: the Live tape's room unchanged) cups a pistol carried low when keyed, and
passes `Targets::keyGun` to `vr_agarms.cpp`: with the second grip out of reach (let go, not easing) the off hand cups
the right hand instead (as the `OffGun` comment always said: "its second grip, else cupping the right hand"), its goal
change eased as any. `Placed::cupped` says the hand is cupping (the Pose Studio's `offHandOnGun` / `offHandFree` and the
Rig Studio tests read it). Auto and Free unchanged, and every message without an On the Gun key byte for byte as before.
Also: `vr_rigstudio_test_offhand` was registered and ticked twice per frame (a merge's duplicate lines): once now.

**Test.** New `vr_rigstudio_test_offmodes` (open the studio with a gun; ends on `rigstudio offmodes: N passed, M
failed`): Carry, Aim, Run, Pickup, the Shove's push, each with its key's Off Hand Auto, On the Gun, Free, each settled
(still 12 frames, or 2 s: a run sways), On the Gun on it, Free off it by 4+ units (and 4+ from On the Gun); then saved
and reloaded (the keys' text with Carry Free, then On the Gun, taken away and read back: the mode and the hand again);
every key and the text put back exactly. **20/20** on the shotgun (4), nail pistol (29), rocket launcher (10) and laser
cannon (12) grunts; before the fix 18/20 shotgun (Pickup) and 13/20 pistol (Carry, Run, Pickup On the Gun). The pistol
now: Carry On the Gun cupped 4.9 units from the grip (was 13.1, free), Run 2.5 (was 9.3), Pickup 3.9; Free 13-14 off.
The shotgun's Pickup On the Gun: cupping, 2.9 (was 27.6). `_test_offhand` 5/5 (pistol On the Gun, shotgun Free),
`_test_stretch` 6/6, `_test_posekeys` 13/14 (step 5, the right elbow: the known failure).
Seen in passing: Auto's cup at the Pickup's lowest depends on the pose before it (held after Run: cupped; after Carry:
free), from the drawn-weapon feedback; not changed here.

## 9b. Checklist: folding sections (branch `agent/clviewer`)

His ask: the Checklist page (247 items in 9 sections) is long to scroll in VR. **Each section's header now shows its
counts** ("1. New: ammo screens, grunt mov..  23/23": the name cut to fit 40 columns, then open/total), **and the row
under it folds or unfolds the section** ("[-] 34 open of 35 (hide)" / "[+] 23 open of 23 (show)", tinted green when
all passed): a plain selectable row, picked by the laser or Enter, so the menu's Header kind and the cursor anchoring are
unchanged. Folded: the header and that row only. **Collapse All** and **Expand All** rows sit under End Test.
Default: a section with nothing open when the list was read (the start, Reload List, the file edited) starts folded;
ticking the last item of an unfolded one doesn't fold it under the cursor. A section folded or unfolded by hand keeps
that, by its name, in `quakevr/checklist_sections.txt` (ignored, beside the ticks: `collapsed <name>` / `expanded
<name>` lines; names no longer listed kept, as the ticks), read at the first refresh and written at each change.
Ticks, Reload List and the tick-by-text scheme untouched. `vr_checklist` prints `CLSEC|s|open|total|collapsed|name` per
section; `vr_checklist collapse|expand <s|all>`, `vr_checklist folds_default` (every fold back to its default).
`vr_menu_dump` now prints a list row's text (it printed "" for every Checklist row).

**Test.** `python Misc/quakevr/checklist_fold_check.py <agent>` (two runs, each ending on its CLFOLD_END line; puts the
ticks and folds files back as found): per-section totals against the file and opens against CLITEM; section 1 folded:
its header (with its count) and "[+]" row on the page, no item of it, all 9 headers, Collapse/Expand All; still folded
after Reload List and after a restart; ticking its first item drops its count 35 -> 34 (then back); the smallest section
(5 items) all passed + folds_default + reload: folded by default, unticked + reload: unfolded. **29 passed, 0 failed.**
On the mock: Collapse All, Expand All and the first section's fold row add 3 rows before the first item (eight
`vr_mock_key downarrow` now select it).

## 10. Suggested checklist lines still pending (coordinator owns `quakevr/checklist.txt`)

- Survival Arena: map, wave director (readout, 5 s rest + 5 s countdown, teleport arrivals, never inside anything, guns
  arriving one at a time, waves ~1.75x bigger in trickles out of sight, mid-wave boss with a warning), difficulty knob
  (Nightmare two bosses, Easy one fewer boss wave), cleanup (bodies fade ~14 s, 12 kept, items go two waves later, guns
  capped 24/48 with Spoils, a held body never vanishes), loot gate `{test:vr_arena_test 11}`, armoured foes and perks
  `{test:arena_intro}` `{test:vr_arena_test 10}`.
- Armour P1/P2 `{test:vr_agarmor_test}`: plates on Normal/Hard/Nightmare grunts and enforcers, sparks and less damage,
  broken plates fall and fade, helmets stop pops, blades blocked until the plate breaks, breastplate leaves the pack
  bare, dents at 2/3 and scorch at 1/3, toolgun Armour/Pieces rows.
- In VR: a walking armed grunt glides like the knights; strafing grunts turn hips and feet into the step; an ammo screen
  goes orange at the last shell and red when empty (also on holstered/lying guns).
- Per-weapon recoil `{test:vr_recoil_test}`: Weapon Offsets > Effects' Recoil Kick Back and Recoil Muzzle Rise change
  only that weapon's kick back and muzzle tip (burst rifle, plasma gun); at 1x it feels as before.
- Relighting > Ambient Light Level at 32, Relight This Map: no pitch-black corner left, the rest of the map as before.
- Nails `{test:vr_nail_test 1}`: nails drawn ~35% smaller (Nail Size in Flight 0.65, `vr_nail_test 2` to look), 18%
  faster for you and enemies (Nail Speed 2065, Nail Pistol Speed 3070), still hitting what they visibly touch.

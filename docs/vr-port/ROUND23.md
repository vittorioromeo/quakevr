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
holding nothing. (Whether the next wave should wait for the perk choice: it does now, "Perk cards" below.)

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
clear offers that many **perk cards** on a floating panel (below, "Perk cards"); the pick takes the perk for the run and
no later boss offers it. (Until the perk cards pass, the offer was pedestals on the item points, walked over.) Perks (each hooks a place the game reports and returns unless taken,
`VR_ARK_Running`): **Quad Strike** (melee kill adds 2 s Quad; only ever adds time), **Neck Snapper** (snapped neck +20
health, decap hook), **Iron Skin** (picked-up armour +25: 125/175/225, at `armor_touch`), **Second Wind** (max health
+15), **Feeder** (+5 health per kill, at most 1/s), **Quartermaster** (each clear +15 ammo to the held gun, via
vr_ammoutil), **Spoils** (twice `vr_arena_gun_max` fallen guns stay), **Can Opener** (plates pay twice from their pool
for what they save, `VR_ARK_PlateCost`). Tests: `vr_arena_perks_test 1..5` on vrarena (pool 19, offer and picks 22,
every perk through the game's calls 43, full-map drive 15 `arena_full:`, the card panel 13 `arena_cards:`); scenario
`quakevr/scenarios/arena_intro.txt` (boss every wave, so the offer comes at once).

**Perk cards** (`Quake/vr/vr_arenacards.cpp`, QC `VR_ARK_Card*` in `vr_arena_perks.qc`). Why the pedestals went: they
stood 96 units out past the item points, often behind the player or across the room; their names were view messages
that the WAVE CLEAR line, the countdown's once-a-second lines and the next wave's lines overwrote within a second or two;
the rest (5 s) and countdown (5 s) ran on regardless, so the choice was made while the next wave arrived (or not at all:
an unclaimed offer was silently replaced at the next boss); walking over one by accident (on the way to a medkit) took
it; and the picture was a model lying on the floor (gibs, view models). Now: the clear's cards go up on a panel 320 x 200
virtual px (1280 x 800 texels), `vr_arena_cards_width` 32 units wide, `vr_arena_cards_distance` 34 in front of the head
at its yaw, a tenth of that below the eyes, leaning back to face them, not depth tested (Combat > Survival Arena > Perks:
Perk Cards Distance / Width). Placed once when it comes, world-locked after; placed in front again after a menu, or when
the view is more than 60 degrees off it (or 3x its distance away) for 1 s. A card: its perk's colour band and name
(`VR_ARK_Colour`), its item's thumbnail (`VR_ARK_Model` via the toolgun's icons), what it does, its number. Either
hand's laser lights a card (a haptic tick); its trigger picks it (a trigger on the panel is never the gun's); number keys
1..n too (keys.c `VR_ArenaCardsKey`); presses in the first 0.5 s after it comes up are ignored. The view message is
hidden while the cards are shown. The arena waits: the offer comes only from a wave's clear (nothing alive; a stalled
wave's clear removes its foes), so no monster needs freezing; `VR_ARK_Hold` parks the director on `VR_ARK_HoldThink`
with its rest's remaining seconds, `VR_ARENA_Between` stays true, the pick (`VR_ARK_CardPick`) resumes it with exactly
that remainder (`ar_rest_at` moved on by the hold). An offer no panel reports seeing (`VR_ARK_CardsSeen`, said each
frame by the engine with a headset) for 3 s is withdrawn, so a game without the panel never waits for ever. The arena's
own tests run with `vr_arena_perks 0` (a card offer would hold their waves). Debug > Automation & Self-Tests > Survival
Arena: Offer Perk Cards Now (`vr_arena_cards_offer`), Perk Card Panel Test (`vr_arena_perks_test 5`). Test aids:
`vr_arena_cards_mock_laser <card>|<x y>|off`, `vr_arena_cards_pick <n>`, `vr_arena_cards_dump`. Test 5: jumps to wave 5,
kills it, waits for the panel, 3 s untouched (wave, countdown, foes, rest remainder all unchanged), mock laser on card 2,
screenshot, mock trigger, perk taken, countdown exactly the 1 s remainder after the pick, wave 6: 13/0.

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

## 9c. Per-weapon recoil recovery (`vr_weapons.inc` `RecoilRecovery`; `vr_weaponfx.cpp` `envelope`, `recoveryOf`)

His words: "a per-weapon recoil time recovery multiplier so that I can make some weapons feel weightier/lighter when
firing". The drawn recoil's kick comes up in at most 35 ms, then eases back to rest by the weapon's Recoil Return Time
(`vr_wofs_fx_recoil_time_NN`). New per-weapon `vr_wofs_fx_recoil_rec_NN` (Weapon Offsets > Effects: **Recoil
Recovery**, 0.25..4x step 0.05, extendable 0.1..10x; default 1 for every slot) scales only the way back from the peak:
above 1 slower (weightier), below 1 faster (lighter); the kick up and its peak stay. It is stored per kick when fired,
so a change applies from the next shot. At 1 the envelope is bit-identical to before (new cvars: no migration, no
`settingsVersion` bump). The heavy-weapon aim kick (`vr_heavy_recoil_*`) is separate and untouched.
Test: `vr_recoil_test` (same command line as 9b) now **17 passed, 0 failed**: the 9 before, plus every slot's Recoil
Recovery 1 by default; at 1 the envelope equals the old one at 1203 samples (Return Times 0.06, 0.15, 0.325 s); the
drawn kick of a 0.15 s Return Time is back at rest 0.115 s after its peak at 1x, 0.230 s at 2x, 0.0575 s at 0.5x
(ratios 2.00 and 0.50), the peak 4.0 deg each time; value put back. Baseline (recoil, collectfx, selfchecks,
slider_step, update_notice, autopump, reload): only `recoil` changed (9 -> 17 checks).

## 9d. Punchier gunfire: sound and haptics (branch `agent/gunsfx`; `vr_gunsfx.cpp`, `vr_sndlayer.cpp`)

His approved ideas 7-12. Audit first: every gun's report is one recording (QC `sound` on CHAN_WEAPON/CHAN_WEAPON2),
the layered sounds made at load (`VR_SndLayered`) existed for three sounds, the casings had one tink set (stone) and a
water plip, the dry click (`gunclick.wav`) existed with no tick in the hand, the actions' sounds (break-open, magazine)
played from the muzzle, the room reverb is Steam Audio's on the listener (nothing per shot), and each shot's haptic was
the QC's one flat buzz (`VRGunHaptic`). What's new (Sound > **Gunfire**, every part off with `vr_gunsfx 0`):
- The layered sounds moved out of `vr_audio.cpp` into `vr_sndlayer.cpp` (a pure move) and grew effects: a two-pole
  low-pass (thumps), a Schroeder room tail (four damped combs scaled with the decay, two all-passes, a predelay), a
  distant echo (twice, dulled) and a peak normalisation. All from the game's own and our shipped sounds, nothing new.
- A shot (a listed report on a weapon channel, anyone's: the player's, grunts', enforcers', ogres', the armed humanoids')
  goes through `VR_SndShot` at the start of `S_StartSoundPitch`: varied (`vr_gunsfx_vary` 4: pitch up to 4 %, volume
  2 %), plus its close mechanical layer (super shotgun and grunts' a hammer, nailguns a feed tick, launchers a clunk;
  none where the shot has its own action or the shotgun's auto pump clacks) and a low thump (`vr_gunsfx_layers` 1), and a
  tail by room (`vr_gunsfx_tail` 1): 13 rays round the shooter (cached 0.75 s / 48 units an entity), outdoors when two
  of the five upward ones reach the sky, else small under a 320-unit mean reach, else large. Tails: small ~0.5-0.65 s,
  large ~1.5-1.7 s, outdoors ~1.8-2 s with the echo at 0.42 s; at most one every 0.25 s for the light guns' streams.
  The player's layers play from the firing hand (CHAN_HAND/HAND2). Not the lightning gun (a beam).
- Casings (`vr_gunsfx_casings` 1): metal's ring (tink brighter plus `phys/metal_h`), wood's knock (tink lower plus
  `phys/wood_h`), by the texture under the landing (`debris::materialOf`); stone/other the tinks as before.
- Dry click (`vr_gunsfx_dryfire` 1): its volume, varied, a 12 ms tick (0.35) in the clicking hand; 0 silent.
- Actions (`vr_gunsfx_action` 1): break-open/close at 0.4 of the way hand-to-muzzle, magazine in/out 0.15, shell in 0.3
  (`vr_audio.cpp handSoundAt`). The ejects on CHAN_AUTO stay where they were.
- Haptic punch (`vr_gunsfx_punch` 1, per weapon `vr_wofs_fx_punch_NN` 1, Weapon Offsets > Effects > **Haptic Punch**):
  a 12 ms 200 Hz spike at 0.85 * punch * sqrt(strength * mean(kick, rise)), then a 70 Hz tail from half that to nothing
  (quadratic) over Return Time * Recovery, re-sent each frame; the QC's flat buzz for that shot is dropped (`punchOwns`,
  60 ms). At 0: the flat buzz as before.
Decisions: every group is a 0-off slider or a toggle (the codebase's pattern) under one master toggle; sounds derived
at runtime (his 2026-10-29 decision); the punch replaces the flat buzz rather than adding to it (one feel, set by the
recoil). Test: `vr_gunsfx_test` needs sound (`run.sh -Sound`, `map e1m1;...;impulse 9;wait2;impulse 154;wait5;
vr_gunsfx_test`): **24 passed, 0 failed** (21 layers made, 12 shots' layers and tails started, variation 4 % / none at 0,
tails small < large < outdoor for each kind, e1m1's start room small and (-160 608) outdoors with different tails, the
floor's metal casing, the dry click's tick and silence, every weapon slot's punch curve, Recovery 2x doubles the tail, Haptic
Punch 0 none, the spike sent on the shot, action place, all off). `vr_recoil_test` 17 passed as before.
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

## 9c. Survival Arena: supplies anywhere, armour shards, countdown beeps; the Ambient Light Level on vrarena (branch `agent/arenaspawns`)

His voice notes (2026-10-29): randomise where health, armour and other pickups spawn (ledges too, anywhere walkable)
so the player explores instead of camping, name what spawned in the middle of the view, add one or two armour shards
a wave; a sound for the wave countdown; the Ambient Light Level made no difference on vrarena (0 vs 64).

- **Supplies anywhere** (`vr_arena_scatter` 1, new, Gameplay > Survival Arena > Wave Clears "Supplies Anywhere";
  `QC/vr_arena_supply.qc`). At the run's first frame the engine walks the map from the player start
  (`navplace(-1, start, grid, most)`, `vr_builtins.cpp`): a 48-unit lattice, every move between cells walked with the
  player's box (step up/down 18, never off a ledge, floor normal >= 0.7, a narrow bridge or stair looked for 16 to
  either side). vrarena: **1203 places, 274 high, 5.6 ms**. So a place is somewhere he can walk to: tower tops and wall
  walks yes, the 48-high covers' tops no. A wave clear's medkits, armour and shards each take a random place: 35% of the
  picks look up high (>= 128 over the lowest place), never within 256 of the player, 160 of another supply of the same
  clear, in a liquid, or on anything lying/standing there (`VR_ARENA_SupplyRoom`). The rolls are their own Levy stream
  (`vr_arena_seed` salted 2000), so the waves' plans do not move. Weapons (the foes' guns with their loot pillars) are
  untouched, as he decided. No walk / no room: the old `info_arena_item` points, then the ring. The medkit (corner-origin
  box) is now centred on its place in every path. The walk is held by the engine: the QC's globals are at the id
  format's limit (a 1536-place QC array forced FTE32 progs, which the engine rejects).
- **Armour shards** (`vr_arena_shards` 2, new, "Armour Shards" 0..4): every clear drops 1 to N at random on top
  (+5, up to 200: `MG3_armor_shard_touch`). Dawn of the Machine's `armorshard.mdl` when there, else the green armour model
  at the pickups' size stands for it (`VR_ARENA_PlainShard`). `ar_drop` 2 so the supplies' cleanup takes them.
- **What came, in the view**: the clear's line gains "Supplies around the arena: 2 medkits, green armour, 2 armour
  shards". The view text is not drawn over the perk cards (`vr_worldtext.cpp`), so after a boss clear's pick the line is
  said again (`VR_ARENA_SupplyFrame`).
- **Countdown beeps** (`vr_arena_countdown_sound` 1 = volume, new, "Countdown Sound" under Wave Countdown):
  `misc/menu1.wav` each second at 100%, 120/130/140% for 3-2-1, `misc/talk.wav` at 80% as the wave comes in
  (`sound_pitch`, ATTN_NONE; nothing new shipped).
- **Ambient Light Level**: nothing wrong with the relight step or BSP2; vrarena's worldspawn has `"light" "90"` and the
  level was a floor (raised only where the map's own was lower), so 0..64 all left 90. It is now **added** to the map's
  own minimum (cap 255) with `_minlight_dirt 0` always (`scaledLights`; `relight_maps.py --ambient` the same). id maps
  have none, so they relight exactly as before. Menu help and cvar text updated.
- `firstVrBuiltin` 900 -> 800 (the VR builtins had filled 900..1079; Ironwail's highest numbered is #627).

Tests (headless, fast mode, developer 1): **`vr_arena_test 15`** (new, vrarena, `-QuitOn "arena: test [0-9]+ over"`):
**130 passed, 0 failed**: 1203 places, 0 without the player's room or floor, highest floor 256 (tower tops, wall
walks), none on a cover's top; 6 picks each for seeds 3, 11, 29: all found, apart, off the player, different per seed,
seed 3 twice the same, some up high; live clears of waves 3 and 4: 1 and 2 shards, every supply on a walkable place,
settled on its floor, clear of the brushes, dry; the line "Supplies around the arena: 2 medkits, green armour, 2 armour
shards". `vr_arena_test 14`: 9/0 (6 beeps per countdown at Normal and Hard). Re-run: 1 257/0, 2 13/0, 6 8/0 (its drop
bound now 5 a clear), 7 19/0, 9 37/0, 13 12/0. Relight (`vr_relight` in game, `vr_relight_quality 0`, AO on, ericw 2.0
alpha11), `relight_ambient_stats.py` 0 vs 64: **vrarena p1 0 -> 77, p5 15 -> 77, median 47 -> 77, under 16 6.05% ->
0.47%: PASS**; e1m1 min/p1/p5 0 -> 32, under 16 19.7% -> 0%: PASS (unchanged: no own minimum). The verdict no longer
needs the single darkest luxel to rise: a few corner luxels whose samples fall inside a touching brush (vrarena's crates
and trim, 2-6 a face) stay 0 at any level. Note: at level 0 a relit vrarena is darker than shipped (its own 90 is
darkened by -dirt, as any map's minlight is at 0); not changed here.
## 9b. Armour: ragdolls, dislodging, debris, sparks, the break's sound (branch `agent/armorfx`)

His voice notes of 2026-10-29 (five items).
- **Break sound.** `vr/derived/armor_break.wav` is made as it loads from `items/armor1.wav` (vr_audio.cpp
  `layeredSounds`, one layer with the new `reversed` flag; merged onto 9c's layered mechanism, so snd_mem.c is
  unchanged): the pickup chime backwards at 0.7 rate (~6 semitones down), drive 0.1 (clean). Nothing shipped. `VR_AgArmor_Break` plays it (the ricochet on the hit stays).
- **Ragdolls.** A corpse's hit (`VR_Corpse_Damage`) now goes through `VR_AgArmor_Damage` first (it never did: corpse
  plates were inert, and the limb wear popped limbs under them). A wear-pop is held (`VR_AgArmor_Holds`, from
  `VR_Limb_WearInto`) while an unbroken plate rides a bone the cut would take, or when the hit itself landed on a plate
  that was on (so the breaking hit pops nothing; the next one does). `vr_agarmor_block_cuts` / `_helmet_head` gate it.
  Active (knocked-down, alive) ragdolls already went through T_Damage: Guard stops the armed killing cut as the plate
  breaks. Blows at a ragdoll point (`vr_limb_wear_at`) now count as a single-point hit for the plates.
- **Dislodging.** `VR_Limb_Cut` / `VR_Decap_Cut` take the plates on the bones they cut (new builtin query
  `ragdollbone(e, [piece,0,0], QVR_ARMOR_PIECE_BONES)`: agarmor::pieceBones via box3d::boneNamed) as debris, placed
  before the cut, thrown with the limb. Gibbing sheds before the limbs fly (ThrowHead) and now also on a headless
  corpse's gib (that path shed nothing: the plates vanished), with gib-like velocities.
- **Debris.** Hand-grabbable (model box, SOLID_NOT_BUT_TOUCHABLE, `VR_Carry_Setup`, carry_use nothing), never
  FL_FORCEGRABBABLE or an item; `VR_Carry_Start` resets its fade (`VR_AgArmor_DebrisGrabbed`), and a fade due while
  held is put off.
- **Sparks.** `VR_AgArmor_HitFx` at TraceAttack, spawn_touchblood and VR_HitBloodSplash: on an unbroken plate, sparks
  where it struck, and the blood (and a pellet's wound) scaled by the predicted share let through (none when it all
  stopped). `VR_AgArmor_Damage`'s own sparks remain for hits with no blood site (blasts, beams), larger.
- **Test.** `vr_agarmor_test 5` (QC/vr_agarmor_fx_test.qc): **19 passed, 0 failed**; `vr_agarmor_test 1` still 26/26.
- **Found, not fixed.** (1) A thigh plate's middle reads as the shin's piece (`pieceAt` by nearest vertex): hits on a
  thigh plate's lower half count on the shin plate. (2) The authored rig has 20 bones but corpse limb wear keeps 16
  (`vr_limb_corpse_dmg[16]`, `VR_Limb_WearPiece` `bone < 16`): rig2 joints 16-19 (shins/feet) wear as torso hits.
  (3) The sound can't be heard headless (no sound device): check it in VR.

## 9c. Weapon fixes from his voice notes of 2026-10-29 (branch `agent/weaponfixes`)

1. **Ammo screen warning colour on the face too** (vr_text3d.cpp `tintedFace`, `screenLook`): a low/empty warning's tint
   now colours the screen's face (its brightness and saturation in the tint's hue), not only the text. Eyeshot
   (`vr_ammocolor_test 2`, hands posed first): the shotgun's face at a fifth rgb 59.5 29.9 0.1 (orange) vs 43.8 66.5 29.9
   (green) with `vr_ammo_color_warn 0`.
2. **Enemy lava nails set you on fire** (vr_burning.qc `VR_Burn_LavaNail`: a monster-owned nail ignites too).
   `vr_burn_test 16` (e1m1): `burning: test 16: ... PASS you burn`.
3. **Empty reloadable weapons' loot beam red** (vr_lootbeams.cpp; `vr_lootbeam_hue_weapons_empty` 0, below 0 off; menu
   Graphics > Loot Beams "Empty Weapons Hue"): magazine empty or out, reloading on, not a spent enemy gun. Debug >
   Tests "Loot Beams: Empty Every Other Weapon" (`vr_lootbeam_empty_test`); `vr_lootbeams_report` prints each weapon's
   clip and colour: RL 0/4 EMPTY 1.00 0.25 0.25 beside nailgun 24/24 and SSG 2/2 gold 1.00 0.75 0.25.
4. **Nail pistol magazine: eject button only** (vr_reload.qc `VR_Reload_MagFrame`, `VR_Reload_HitFrame`;
   `vr_nailpistol_mag_pull` 0, 1 the old pull and knock-out). `vr_np_test 1` (vrtesthall): 21 passed 0 failed, incl. a
   pull at 0 (stays in) and the control at 1 (comes out), then re-seated.
5. **Widowmaker held art darker** (vr_envmap.cpp `weaponMetal`: AdVrWidowmaker 0.2, lod 3.5; was the default 0.7): its
   grey painted panels mirrored the bright test hall near-white in the hand (the reflection is added in proportion: ~70%
   less). Not measurable headless (no AD data in the kit's base: the upgrade and its model need it); for him to check.
6. **Plasma Gun's shot sound** (vr_ad_bridge.qc `AD_PlasmaFireSound`, vr_ad_enemyguns.qc): without AD's data mounted
   AD's `weapons/plasma_fire.wav` was never precached (his log: "not precached"): now `vr/derived/plasma_fire.wav`.
7. **Revolver's own shot** (vr_revolver.qc): `vr/derived/revolver_shot.wav` instead of the super shotgun's.
   Both are layered at load (vr_audio.cpp `layeredSounds`, `VR_SndLayered` from S_LoadSound): id's sounds at their own
   rate, gain, cut and fade, summed and tanh-saturated, a 16-bit 22 kHz WAV in memory; nothing shipped or written. Plasma:
   enforcer/enfire 0.72x + weapons/lstart 1.15x (0.22 s) + weapons/grenade 1.25x (0.3 s); revolver: weapons/shotgn2 0.8x +
   weapons/r_exp3 1.35x (0.35 s) + weapons/guncock 0.72x, drive 1.8. `soundlist`: both load (16b, 81664 and 158764).

## 9d. Armour: smaller helmets, limb plates by the plate struck, the right leg's wear (branch `agent/helmets`)

His voice notes of 2026-10-29: the grunt's helmet too big (eyes hidden, a hat), the enforcer's too wide with side flanges.
- **Helmet fit** (`helmetOf`, vr_agarmor.cpp). The enforcer's flanges were its cheek guards fitted round collar/shoulder
  vertices blended onto the head (head weight 0.2-0.5, out to 9.3 units each side): the guards now fit only the head
  proper (weight >= 0.5) and are never wider than the ledge over them + 0.1 head lengths. The side brackets are gone.
  Brow higher (`vr_agarmor_brow_front` 0.5 -> 0.62: the eyes show), face opening 0.34 -> 0.4, taper 0.08 -> 0.03, ledge
  0.04 -> 0.015, rounder chamfers (0.7 of the side, the guards 0.5), a sloped cap chamfer (0.18, fitted per point up
  it), guards end higher (jaw row -0.07 -> 0.04). Its own gap and thickness: `vr_agarmor_helmet_gap` 0.15,
  `vr_agarmor_helmet_thick` 0.45 (the body's plates keep 0.25 / 0.6). The helmet fit line now ends with its outside size.
- **Size** (`vr_agarmor_check`, outside front-back x side-side x high, units): grunt 11.11 x 11.43 x 8.50 -> 10.38 x 9.39
  x 7.47 (front area -28%, volume -32%); enforcer 14.39 x 21.73 x 8.70 -> 13.56 x 10.63 x 7.67 (front area -57%). The
  depth is near the head's own (the enforcer's head with its baked visor is 11.5 deep). Check 10/10 passed both
  models; over all the poses (rest, neck/head/spine/arm/leg joint-limit sweeps, random poses) the body inside the helmet
  144 -> 133 vertices, deepest 0.40 -> 0.24 (under the 0.45 thickness: nothing through), helmet inside the body 4019 ->
  2178. Eyeshots: `scratch/helmets_before_after.png` (left before, right after; rows front, side, three-quarter; each
  pair grunt | enforcer), from `vr_agarmor_test 6` + `vr_agarmor_headcam 1-3 20`.
- **Thigh plate read as the shin's** (`pieceAt`). The ragdoll rig's nearest vertex read a thigh plate's knee end as the
  shin's (and a few bracer/pauldron ends as their neighbours'). Now a hit on a limb (or on no piece's bone) takes the
  nearest limb plate within the plate's reach (gap + thickness + 1.5) by the plate's own triangles, each plate's frame
  as drawn (`plateNear`); a point on/just under a limb plate takes that plate whatever bone is nearest (a ragdoll's arm
  lying on its chest). Cuirass and helmet hits stay their bones'.
- **Right leg's wear on corpses** (QC). The per-joint arrays (`vr_limb_corpse_dmg`, `_bdmg`, `_wn`) and the joint loops
  kept 16 slots; the authored rig's right leg is joints 16-18 (thigh_r 17, shin_r 18 measured), so its hits wore the
  whole body (and `VR_Limb_TopJoints` never threw it on a gib). `VR_LIMB_JOINTS` 24 (ragdoll::maxBones) everywhere.
- **Tests.** `vr_agarmor_test 5` gains: on the dead ragdoll, the active one and a standing grunt, every limb plate's
  outer face near both ends and at its middle (`limbpiece` 12/13/14, new) reads as its own piece: 24/24 each (the old
  lookup: 17, 14, 16 of 24); the corpse's right thigh and shin worn as their own joints 17 and 18, body health 160 kept
  (old: joint -1, body 160 -> 150). **23 passed, 0 failed.** `vr_agarmor_test 1` **26/26**.
- **Debug.** Debug > Tests > Helmets There: Grunt and Enforcer (`vr_agarmor_test 6`); `vr_agarmor_headcam <1 front | 2
  side | 3 three-quarter | 4 behind> [distance]` puts the mock eyes on the nearest grunt's (left) and enforcer's
  (right) heads, 0 back.
- **Found, not fixed.** `vr_limb_test 28/29` (Misc/quakevr/ragdoll/limb_wear_test.sh) fail the same with the old 16
  slots (a blunt blow wears nothing into joint 7 of the firing range's monster_army): not this change.

## 10. Suggested checklist lines still pending (coordinator owns `quakevr/checklist.txt`)

- Survival Arena supplies `{test:vr_arena_test 15}`: after a wave clear the medkits, armour and 1-2 armour shards lie
  at random all over the arena (floor, tower tops, wall walks), never at your feet, never in a wall or a crate; the
  clear's text names them (again after a boss wave's perk pick).
- Survival Arena countdown `{test:vr_arena_test 14}`: a tick each second, higher for 3-2-1, a chime as the wave starts
  (Gameplay > Survival Arena > Countdown Sound).
- Relighting > Ambient Light Level on vrarena: 0 vs 64 visibly brighter in its dark corners after Relight This Map.
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

## Armed humanoids: the held weapon on an active ragdoll's hand

His notes (2026-10-29): shoved into a stagger or an active ragdoll, an armed humanoid's weapon "floats in the wrong
position", still attached to it but not to its hand; sometimes the weapon and hand get into or behind its torso.

**Cause.** The client draws a ragdoll's held weapons on its hands (vr_hitreact.cpp `ragdollProps`), but only from the
rigid arms' model (`armModelOf`, vr_agrunt_ik_rigid), which is never made with vr_agarms on (its rig checks fail for
the authored grunt). So every ragdoll weapon was left to the server's placing (QC `VR_AG_RagProp`): riding the limb
nearest it as the ragdoll began, at the server's pose, not the drawn hand's (the grunt's 14-16 units and ~100 degrees
off). Using the bone alone (agarms' hand bone and grip) was still 0.6 units and 20 degrees off on the grunt: the
authored rig blends the hand's vertices over hand, forearm and fingers.

**Fix.** With vr_agarms, `ragdollProps` takes agarms' grip frame (`agarms::ragdollGrip`, as the posed arms held it alive)
and places it on the hand as drawn: the rigid fit of the hand's own vertices (`agarms::handSkin`'s) from rest to the
ragdoll's drawn vertices (`drawnHandFit`), every frame. Physics: `vr_ragdoll_weapon_collide` 1 (new): a living
ragdoll's welded weapon (its loose piece) meets its own chest, pelvis, spine, neck and head (`selfMeets`; before, a loose
piece met nothing of its own), so an arm can't fold it into the chest.

**Test 88** (`vr_agrunt_test 88`, QC/vr_armedgrunt_ragweapon_test.qc; `vr_agrunt_ragweapon_check` 1 / 2 per frame,
`vr_agrunt_ragweapon_err`): grunt and enforcer with a shotgun, a stagger and a topple (lying, getting up), every drawn
frame: the weapon's grip against the drawn hand's fitted grip, the hand's vertices and the weapon's line inside the
torso proxies (vr_agbody). Weapon grip off the hand, max (mean) units / degrees:

| case | before | bone only | after |
| --- | --- | --- | --- |
| grunt stagger | 19.1 (13.8) / 96 | 0.62 (0.29) / 18.5 | 0.00 / 0.1 |
| grunt topple | 24.2 (16.2) / 115 | 0.62 (0.52) / 22 | 0.00 / 0.1 |
| enforcer stagger | 1.09 (0.34) / 15 | 0.15 / 1.3 | 0.00 / 0.0 |
| enforcer topple | 1.86 (0.49) / 15 | 0.19 / 3.3 | 0.00 / 0.1 |

In the torso (most, units): enforcer and grunt stagger 0 throughout; the grunt topple, before 2.0 hand / 2.0 weapon,
after the fix (no weapon collision) 1.4 / 2.1, with it 1.8 / 0 and 2.3 / 0.8 (two runs): the weapon is mostly kept
out, the grunt's **hand still dips up to ~2 units into its chest for ~0.5 s of its topple** (test 88: 3 passed, 1
failed). Open: why the hand-chest contact lets it in there (selfMeets skips contacts already deep at the 0.7 core;
selfTouches' living exceptions), and the welded loose piece is the .mdl's own gun on forearm_l (looseHolder), not the
drawn weapon on the right hand. Test 74 (disarm by grab): PASS 18/19, stuck 0.

Checklist:
- Shove an armed grunt and an enforcer into a stagger and over `{test:agrunt_fall}`: the weapon stays in the drawn
  hand through the stagger, the fall, lying and the get-up; never floats off it.
- Shoved over, its weapon never goes into its chest; its hand stays out of it (the grunt's may still dip in briefly).
- A downed armed grunt's weapon can still be grabbed off it `{test:agrunt_fall}`.

## Armed humanoids shove up close; dual guns' shots from each muzzle (branch `agent/shovemuzzle`)

His notes (2026-10-29): armed humanoids with guns "don't really shove the player away anymore because they prefer
backpedaling"; really close they should walk up and shove, a bit further backpedal, otherwise as now. And dual-wielding
enforcers with two laser rifles fire both bolts "from the same weapon's muzzle... the right weapon".

**Shove up close.** Cause: a gun's holder backs off whenever you are nearer than its weapon's least distance
(`VR_AG_RangeMove`, and `VR_AG_WalkFire` as it fires), so you never stayed in its shove's reach (50 units) for the
shove's 0.5 s delay. Now (`vr_agrunt_shove_close`, default 24, Combat > Enemy Shoves > Armed Shove: Step In; 0 the old
behaviour): you within its reach plus 24 units (74 for a grunt) and its shove ready (cooldown over, on its feet, not
in a tug), it holds its fire and steps in facing you (`VR_AG_ShoveCloseIn`, from `VR_AG_Run` before the explosive
back-off and `ai_run`), then shoves once the delay passes (`VR_EnemyShove_Check`); its walk-fire doesn't back away
there either. Between that and its least distance it backs off as before; during the shove's cooldown too. Melee
weapons and fists are unchanged (their own blows). Gun holders only (his standing decision).

**Dual guns' muzzles.** Cause: a replacement's own gun (`vr_ag_native`: the enforcer's laser rifle, the grunt's gun)
fires its monster's own shot from the monster's own place (`VR_AGR_NativeShot`: the enforcer's `v_right * 8.5`, the
grunt's `EnemyFireBullets` middle), whichever hand holds it. Every other dual pair already fired from each gun's
`muzzlepos` (measured). Now with a gun in each hand the native shot leaves that gun's drawn muzzle
(`VR_AG_DrawnMuzzle`, split out of `VR_AG_SetHand`: the server muzzle turned onto the drawn gun under
vr_agrunt_shot_drawn); `EnemyFireBullets` takes it through `vr_agr_native_from`. A single native gun is unchanged.

**Tests.** Test 89 (`QC/vr_armedgrunt_shoveclose_test.qc`): shotgun, nail pistol, nailgun, enforcer's laser rifle;
in reach (40 apart) and just beyond (64): 8/8 shoved in 0.6-0.9 s, 0 backing steps; the band (101-149, halfway to its
least): 4/4 backed off, 0 shoves; far (420): 4/4 no step-ins. With `vr_agrunt_shove_close 0` the near check fails (it
backs off). Test 85 (`QC/vr_armedgrunt_muzzle_test.qc`): every pair for grunt and enforcer (shotguns, super shotguns,
nail guns, super nail guns, nail pistols, grenade launchers, thunderbolts, laser rifles, both natives; AD's plasma
skipped without AD data), 3 shots a gun: every shot 0-2.4 units from its own drawn muzzle, 11-22 from the other: 15/15;
before the fix both native pairs fail (the left gun's shots 22 units from it, nearer the right gun). Test 48 (fighting
distance) case "walking at it, axe": now passes when it shoves you up close (it used to require never within 64).
Tests 84 (shove) 20/20, 42 (dual guns) 20/20. Test 48 run one weapon a run (`scratch1 N`, each ending on its own
`test 48 over` line): weapons 1-3 6/6, 4-8 2/2 each, 9-10 1/1 each (all pass; the whole test in one run outlasts a
590 s low-impact run). Numbers 85 and 89 are `vr_agrunt_test`'s; `vr_ad_test` 85/89 (vr_ad_enemyguns_test.qc) are
another cvar's dispatcher, no clash.

Checklist:
- Walk right up to a grunt holding a shotgun (or an enforcer with its rifle): it stops backing off, steps in and shoves
  you; stand a little further (about two body widths) and it backs off as before.
- Dual-wielding enforcer with two laser rifles, a grunt with two of its own guns `{test:agrunt_dual_guns}`: each bolt
  or burst leaves its own gun, the left gun's from the left hand.

## Refactor pass, phase 0: the test and bench baseline (branch `agent/agtests`)

The baseline every refactor phase is checked against (the kit's `scratch/refactor/baseline.md`, its rerun script
`scratch/refactor/run_baseline.sh <agent>`: about 2.3 hours of tests, then the LTO bench).
- **Test-number registry** (`QC/test_numbers.txt`): every QC test cvar that takes a number now has its dispatcher
  listed: 26 more (`vr_arena_test`, `vr_arena_perks_test`, `vr_limb_test`, `vr_decap_test`, `vr_agarmor_test`,
  `vr_mg3_test`, `vr_burn_test`, `vr_smallgibs_test`, ...), as `loose` dispatchers (their set-up branches share numbers
  with the cases: only the registry's claims, coverage and the clashes between dispatchers are checked), plus next's
  `vr_agrunt_test` 85, 88, 89 and the Debug menu's fall-through numbers (26 `N*` lines). 422 registered, 0 problems; a
  copy with a second claim, a second dispatch branch or an unclaimed branch fails the check (exit 1, the build fails).
  Not numbered tests: `vr_shock_hit_test` (a damage amount), `vr_crouch_test`, `vr_dmgcredit_test`.
- **Fixed: tests 69 and 74 hung** on a QC bounds check (`0 <= 19 < 17 is false`): `vr_rimp_next[17]`
  (vr_ragimpact.qc) was missed when the limb arrays grew to every rig joint (`VR_LIMB_JOINTS` 24, commit 0b7f067ae):
  the right leg's joints 16-18 overran it. Now `[VR_LIMB_JOINTS + 1]`; 69 10/0, 74 1/0. Any ragdoll impact on the
  authored rig's right leg hit it in play (the ragtest suite's `Host_Error: Program error` was likely the same).
- **Test 31 (ragtug pull), not fixed: a tuning question.** The physical tug's pull never reads above ~110 N now (peak
  109-112 N; a 0.6 m/s pull to 0.7 m), under `vr_agrunt_tug_pull_force` 270 N, so the steady pull never wins and the
  test times out its 5 s, the grunt still armed. ROUND22 measured 1185-1650 N for the same pull. The drawn hand stays
  on its grab (0.5 units), the grunt's arm follows the hand (test 39: the hold's lag 15 cm at a 32 cm controller gap),
  the grunt's body does not move. Likely since the tug's hand drawn on the weapon (670a9305b, 10-26) or a later spring
  change; not the close shove (`vr_agrunt_shove_close 0`: the same). Whether a real pull in VR still wins needs him.
- **Test 60 (parries), not fixed: geometry.** The grunt (kind 0) fails the ogre case 4 runs of 5: its parry's pose
  (up 1.00) holds the sword 9-11 units to its right of the blow's line (weapon middle 16 -10 22 body space, upright),
  outside `VR_AGPAR_RMON` (5 units), "missed its weapon". The enforcer (kind 1) passed 4 of 5. Fixed since: "The
  parry's guard meets the blow" below.

Checklist:
- Throw or slam a grunt so its right leg hits the floor (or a wall) hard: the dent and wound land, no stall.

## Mock-hand failures: two causes, both real bugs (branch `agent/mockhand`)

The baseline's "the mock hand's punch registers nothing" group (phase 0 above) bisected (first-parent, on next from
a09cc8df3) to two merges:

- **c47f9ad1b (bladedefaults: his fist line as the default, config 198).** The author's fist blow line
  (`vr_melee_fist_*`: -0.34 0.50 -1.00 to -0.41 0.77 -4.22) runs down the hand's own up axis, not out ahead of it.
  `VR_Melee_PulledBack` judged "drawn back" along the blow line (far end minus grip): with the line pointing down the
  hand, any motion against the hand's up read as a pull-back. On the mock (controller level, `vr_gunangle` 70: the hand
  frame pitched 70 deg down) the line pointed back and down (cos -0.94 for a straight punch) and every forward punch was
  dropped: no blow, no whoosh, no show_hostile. In VR the same rule would drop uppercuts and let a retracting fist
  count. Fix (`QC/vr_melee.qc`): a fist's pull-back is judged along the hand's forward (`mh_rpunch`, play space), the
  punch's way, as with the old line; blades, heads and guns keep their line. No tuning changed (the line, the
  threshold -0.35 as they were). fist_alert's punch wakes at the same time as with the old line (3.880556).
- **92c017e83 (rig2only: the authored rig the grunt's and enforcer's only rig).** Live grunts and enforcers are now
  always drawn swapped to their skinned copy (`<model>#rig2`, bone poses), and the model collisions
  (`vr_modelcollide.cpp posed`) skipped any bone-posed alias: the hands and held weapons went through every live
  grunt (melee_phase: "drawn held out up to 0.00 units"). Fix: `rig2::liveVertices(num, out)` skins the .mdl's rest
  vertices with the bones it is drawn with (the renderer's 3x4 matrices and the rig's weights), placed with the copy's
  entity matrix; model collisions use them with the .mdl's triangles. melee_phase: drawn held out 6.10 / 3.90 / 5.45 /
  6.09 units (6.24 / 6.21 at a09cc8df3).

Verdict (rerun of the 12 cases): **3 now pass** (fist_alert 4/0, swim_sound 11/0, melee_phase 7/0); **9 still fail**,
other causes, not followed (budget): cellcord 7/3 (the pouch shows 6 cells), weapon_catch 3/2 (lying shotgun not
taken), secret_hits (door punch), agrunt 3 (yank), 29 (weapon blow: 0 sounds), 36 (grabs: the fist 23-24 cm off the
weapon after `vr_mock_hand_to agweapon`, `vr_agrunt_grab_gap`), 63 (0/7, was 0/14), 65 (parry raised 0), judo 12/4.
Release suite (43): 34 pass; changed against the baseline: the 3 above fixed, reload 111/3 and parry_pose 0/1 newly
failing, but both fail the same on plain origin/next 2f9544e17 (reload: the rear sight's ring, the super shotgun's
blood, the super nailgun's well; parry_pose: the dragon 0 parries), so they came with next since the baseline's tree.

Kit: `run.sh`'s agent-lock loop read the lock's age with `stat` after `[ -d ]`; a lock removed in between (its run
ended) made an empty operand in `$(( ))`. The age is read once into a variable and the loop retries at once when it
is gone.

Checklist:
- Punch straight, uppercut and hook with an empty closed fist at a grunt: each lands and whooshes; pulling the fist
  back towards you after a punch does not hit or whoosh again.
- Push a hand or a held gun slowly into a live grunt or enforcer: the drawn hand stops at its body (Debug > Show Model
  Collisions logs the push), as it does at a corpse.
## Refactor pass, wave A2: canonical engine-access helpers (branch `agent/refaccess`)

The helpers (table in CODE_STYLE.md, "Canonical helpers", "Engine access"): `progs::ServerVm`, `localPlayer`, `edictAt`,
`forEachEdict`, `nearestBy`, `callQc/qcFloat/qcText/qcVec`, `vecParm/returnVec/setVecParm/returnedVec`, `handFields`
(vr_progs.hpp); `worldtrace::contents/clientContents/liquidContents/isLiquid/serverLine` (vr_trace.hpp);
`haptics::pulse` (vr_backend.hpp, vr_main.cpp); `cvars::forEach/Override/Snapshot` (vr_cvars.hpp); `client::entityNumber`
(vr_client.hpp); 21 fields moved from `ED_FindFieldOffset("...")` at each use into vr_fields.inc (weaponflags, weapon2,
weaponinst, handtouch, vr_drop_born, vr_kd_until, vr_ag*, vr_rigseq*, vr_rs_aim*, stl_*).

Copies removed: 13 VM guard structs, 2 sets of QC-call helpers (arenacards by name, toolgun by func_t), 2 serverEdict,
scenario's player(), 4 numberOf, 9 haptic wrappers (bladeedit, flashlight, holstersetup, posestudio, posing, motion,
menuui, toolgun::haptic, voicenotes) plus 7 inline gated blocks, avatar/physics isLiquid, physics contentsAt,
debris::contents, box3d_step liquidAt and its two declarations (P25), debris::traceLine and ragtest's traceLine,
ragtest's CvarOverride, motion_play's saved-settings vector, 3 nearest-monster loops, VR cvars read by name (vr_ad,
toolgun, toolgun_panel, menu, tracy), 69 `hand ? f.x : f.offx` pairs. The liquid wrappers that carry their own rule
(bloodprints, bloodstream, gore, knockdown, particles, shells) stay, their bodies on the worldtrace primitives (D3).

Bugs fixed: bullet time's and the gear lights' buzzes ignored vr_disablehaptics (R01 W02); vr_hit_precise is put back as
it was after the precise-hits diagnostic (M45: it was left 0 when on but enabled() off); the menu's hipnotic/rogue
monster choices read their always-registered cvars directly (S14).

Left on purpose: vr_timescale.cpp's VmScope (switches only when another VM is current); vr_gamedir.cpp's
`Cvar_FindVar("vr_hipnotic_available")`/`("vr_campaign")` (they run from the filesystem init, before the cvars are
registered: not always true); engine cvars read by name (host_timescale, host_maxfps, vid_vsync, sv_friction...: no
extern in the VR headers); ragtool's player() (EDICT_NUM(1), unguarded); climb's traceLine (its own traceBox);
dynamic-name field lookups (box3d_api keys, edictindex, notectx) and `mangle` (portals: also read with non-VR progs).
Inline spellings not yet on the helpers (each differs in its guards or needs a hand conversion: wave B or a follow-up):
EDICT_NUM(1) 79 sites / 33 files, svs.clients[0].edict 27/15, manual PR_PushQCVM pairs 87/40, `num_edicts` loops
101/55, SV_PointContents 19/7, ungated or combined-condition be->haptic 12/9, G_VECTOR(OFS_*) pointer reads 173/4.
Behaviour notes: haptics::pulse also drops a hand number outside 0..1 (some wrappers passed it on to the backend);
hitreact's entity number of the world reads -1 (was 0; only its prints, never for the world).

## Refactor phase 4, wave A4: QC canonical helpers (branch `agent/refqc`)

No behaviour change intended; CODE_STYLE.md, "Canonical helpers", "QC", lists them.
- `vr_util.qc`: `VR_Player`/`VR_NextPlayer` (inline; 195 `find(world, classname, "player")` and 74
  `find(x, classname, "player")` spellings replaced outside QC/ad and frikbot), `VR_PlayerAlive` (was
  VR_ARENA_FindPlayer, VR_ARK_Player), `clamp` (was honey_subs2's, VR_ARENA_Clamp, MG_Clamp), `VR_Wrap180` (was
  VR_AG_Wrap180), `VR_Cross` (5 copies), `VR_Lerp`/`VR_Smooth` (were VR_AGP_*), `VR_PointSegmentDistance` (walltorch's,
  melee's), `VR_SegmentDistance`/`VR_SegmentDistanceEps` (juice's; burning's passes its own `denom > 0`). Per-file player
  getters gone: MG3_BT/CT/MT/WT_Player, MG3_Shub_Player, VR_GPT_Player.
- `vr_testutil.qc`: `VR_Test_Begin/Check/Verdict` with one tally replace 12 per-test checks (mg3 bestiary, chthon,
  monsters, shub, mg3test, weapons; mg horde, hub, trigger; nail, nail pistol, revolver). Their lines are unchanged.
- Cvars: 149 `cvar("...")` reads in game code (tests kept theirs) use handles; 26 handles added for per-frame and think
  reads (aim cone, enemy melee arcs, AD tanks and flails, kdtrig/guard/deadanim switches, `developer`...).
  `vr_cvars.qc` is compiled right after `defs.qc` (combat.qc, fight.qc and vr_aimcone.qc came before it).
- `Misc/quakevr/check_builtins.py` (kit build.sh): every `= #0` prototype is bound by an engine table and every VR
  builtin has a prototype (191 prototypes, 180 VR builtins today).

Left on purpose (different results): VR_AG_SegDist (eps 0.001, `den != 0`), VR_Reload_TestSegDist (0.001 floor),
VR_RV_AngleNorm (wraps 180 to 180, not -180), hip_mon_arma/honey_subs2's one-step +-180 wraps, VR_RV_Wrap (chambers),
VR_AGP_Nlerp/Ease (pose only). Tests: the agtest family (VR_AGT_*, VR_AGAM/AGST/AGPT2 copies print `agtest:`-style by
bprint), VR_FLT_Check (its tag depends on the mode), VR_ART/APK/AT/Reload checks (print only failures) keep their own
checks. Not done: VR_HandVec (the ~120 `xHand == cVR_MainHand ? p.a : p.offa` selects), one VR_TestsFrame, generating
vr_cvars.qc's handles from vr_cvars.inc.

## Super shotgun: a loaded one flicks shut a little more easily (branch `agent/ssgflick`)

The author: a loaded super shotgun should be a tiny bit easier to flick shut than one that still needs loading.
`vr_reload_ssg_close_flick_loaded` (default 1.15; VR Settings > Weapons > Reloading > Super Shotgun, under "Close by": "Flick Close
Leniency When Loaded", 1-2 in 0.05) divides Flick Close Speed (`vr_reload_ssg_flick_close_speed`, 400 deg/s) while the
open gun's clip is full (both barrels loaded): 400 needs about 348 deg/s loaded. Bigger is easier; 1 is as before. Only
the wrist speed is eased (the engine's vr_flick.cpp `flickSpeed`): the direction test (the barrel 0.6 towards the hand's
up at rest) stays, since loosening it makes ordinary turns of the open gun shut it. The rest speed follows (at most 3/4
of the flick's).

Test: reload_test.sh section 7, a new check: the mock's 1800 deg/s flick under Flick Close Speed 1950 leaves the open
gun open empty and loaded at leniency 1, and shuts it loaded at 1.15 (opens `110`).

Checklist:
- Break the super shotgun open, flick it gently without loading: it stays open; load both barrels and flick about as
  gently: it shuts (Flick Close Leniency When Loaded 1.15; at 1 both need the same flick).
## Faster headless tests: where a case's time goes, parallel lanes (branch `agent/harness`)

**Measured** (2026-10-29, 3-4 other agents' games running, CPU 70% busy before ours): a case alone is ~2.2 s of
start + quit (`quit` only, through run.sh/PowerShell), ~2-3.5 s of map load and `wait30`, the test body, and with
`-QuitOn` up to 2.5 s more (the log written out a second after its last write, polled every 0.5 s, then 1 s more).
A game has 43-46 threads (`vr_jobs_threads 0`: 31 pool workers, the GL/render, audio and SDL threads) but uses about
1.5 cores in fast mode; capping the pool (`vr_jobs_threads 3`) changed little (agrunt_20 14.7 -> 12.0 s at 8 at once).
The "10x under load" was two things: (1) **exclusive runs drain the slots**: an `--exclusive` request stops new shared
runs until every running one ends, so one long shared run (74 s) made 16 others wait 70-80 s for a 4 s timing run
(slots.log, 23:11-23:13); (2) **the CPU is shared**: at 8 lanes beside the other agents a case took 1.6x (short ones)
to 3-8x (agrunt 1: 25 s alone, 87 s) its time alone. Undrawn frames (`vr_mock_fast 2`) don't avoid (1).

**Lanes** (kit): `QVR_LANE=k` makes run.sh take the lock `agentlocks/<agent>.lane<k>` and run.ps1 (also `-Lane k`)
use the game folder `bases/<agent>/<base>.lane<k>`: id1's paks hard-linked, the other game folders junctioned, and
its own `quakevr`: the worktree's read-only folders junctioned, the ones the game writes its own (cache, screenshots,
eyeshots, profile, diagnostics, logs, crash, saves), loose files copied when the worktree's is newer, `ironwail.cfg`
from the baseline and the tips from the agent's tips baseline before each run. Without QVR_LANE nothing changes.
`run_baseline.py --jobs N` (0: 8, 2 while the author plays) runs every case as one pool, longest first; the scripts
that read the agent's own folder (`bases/<agent>/qbase/...`, the worktree's `quakevr/`) or play copies (testbatch,
`-Instance`, eval.sh) stay in lane 0 (31 cases, ~1100 s alone: the critical path). Console-case timeouts scale with
the lanes (x4 at 8). Full run at 8 lanes: 39.3 min against ~52 min one at a time; 17 cases (agrunt 11, 46, 50-59,
67, 71, 76, arena 6: 34-195 s alone) hit the old 300 s timeouts, and with the scaled ones all 16 rerun match the
baseline (71's counts 6/4 -> 7/3), but took 337-619 s each at 8 at once: 10-15x their time alone, far more than the
CPU share. These draw many grunts' shots: likely the shared GPU (8 drawn games); `vr_mock_fast 2` (undrawn) for the
cases with no pixel check is the next thing to measure. The other verdict changes are next's own (reload,
reload_gunshape, parry_pose fail; fist_alert, swim_sound now pass) and counts of known failures.

**Engine**: `vr_log_flush_frames` (0): the console log written out every frame that printed; run.ps1 -QuitOn sets it
on builds that have it, reads only the log's new lines every 0.1 s and ends the run 0.3 s after the match. agrunt
5/20/40 alone in a lane: 5.7/6.2/4.6 s -> 3.1/3.5/3.5 s, the same verdicts.

Left: the lane-0 scripts made lane-aware (their log path from the lane, testbatch's copies `qbase.lane<k>.<n>`),
which would bring the wall time near the free cases' ~5-10 min; exclusive runs that don't drain behind long runs.

## Refactor wave C2: the switches' old sides removed, D2's config step, one tool colour (branch `agent/refswitches`)

His answers of 2026-10-29 (refactor TRIAGE.md, D1 D2 D5), his config on the new side of every switch. Retired
(retiredCvars: a config line setting one is dropped quietly), each now always as its default was:
vr_torso_mode (the old engine's VR_GetBodyYawAngle estimate gone, vr_torso_report's "old" column with it),
vr_ragdoll_active_stagger_walk (the old glide: straight legs, arms wheeling ahead), vr_ragdoll_posed_arms,
vr_box3d_hulls_ahead (with its compare mode 2; the prop hulls are built ahead whenever there is a pool),
vr_struggle (the switch for all: each monster group's Struggle stays; a group with it off still has the lifted flail,
struggle_test.py's `_off` cases now turn the eight struggling groups off), vr_gore_wash_once, vr_decap_own_motion,
vr_limbs_blast (vr_limbs_blast_chance 0 turns it off), vr_water_foam_particle_spray (its "1" side, never shipped on:
the spray branch, its atlas cell CellShoreSpray, Particle::plain and vr_water_foam_particle_retro), vr_snd_antialias,
vr_particle_halfres_dense, vr_parallax_plane (the world at the relief's bottom, 1; the shaders' ParallaxPlane stays, 0
for every other program), vr_parallax_depth_split, vr_magholster_chain_sim, vr_2h_grip_edge,
vr_toolgun_physgun_ragdoll (its by-origin drag stays as the fallback when no part is hit;
toolgun_physgun_ragdoll_test.py's --old gone), vr_autopump (autopump_test.sh's two "Auto Pump off" checks gone),
vr_ad_death_precache. vr_knockdown_weld keeps 0/1; its 2 (welded where the frame had it) is gone, a 2 now holds it in
the hand as 1. Their menu rows went with them (Torso Follows, Walked Backpedal in Rig Studio, Armed Ragdolls Keep Their
Posed Arms, Prop Hulls Ahead, Struggle, Keeps Its Own Motion, Explosions Pop Limbs, Particles as Spray, Spray in Retro
Look, HRTF Anti-Aliasing, Half-Res In Densest Frames, Parallax Surface Level, Parallax Depth Split, Simulated Chain,
Grip Must Close On It, Auto Pump, AD Monsters' Death Assets Early); vr_menu_path_check maps/vrcalibration.map: 0 missing.
Config rows of retired cvars dropped (148 vr_parallax_plane, 202 vr_water_foam_particle_spray).

Kept, not switches with an old side: vr_decap_chance_scale (the head's counterpart of vr_limbs_chance_scale, a
multiplier with no old branch; vr_limbs_wear_test sets it 0). vr_limbs_mass_scale keeps its multiplier, its 0 (the
pieces weighed by their volume) gone: at least 0.05 (C++ headPropMass, QC VR_Limb_SetMass, the slider's low end).

D2: configVersion 216. The rows that did nothing dropped (135 vr_suit_glow and _light "1", 146 vr_legsweep_topple
"200", 169 vr_kdtrig_tackle_on "1"); 216 moves a config still at vr_suit_glow 2, vr_suit_glow_light 2 or
vr_legsweep_topple 180 to today's defaults (1, 1, 200).

D5: one `toolgun::toolColor(Tool)` (vr_toolgun.hpp) with the beam's values: the panel's Remove (was 1 0.3 0.2) and
Physgun (0.45 0.6 1) icons and highlights now match the beam, and its no-tool colour is white (was 0.9 0.85 0.75). The
physgun's and scale tool's sticks go through vr_deadzone (`input::stickAxis`, the menus' and turning's: past it
rescaled from 0), not a fixed 0.15 with the push kept as it was.

## Faster headless tests, round 2: lane-aware scripts, undrawn cases, fair exclusive runs (branch `agent/harness2`)

- **Lane-aware scripts**: the suite scripts that read the agent's game folder by its path (logs, saves, screenshots,
  eyeshots, id1 pull files, update cache) build it with `QVR_LANE` (`qbase${QVR_LANE:+.lane$QVR_LANE}`, Python the
  same): mapflameburn, reload, xr_runtime, teleporters/teleporter_edges, ragdoll_test, fist_alert, swim_sound,
  grip_gap, flash_grab, parryinterrupt, secret_hits, update_notice. testbatch's copies in a lane are
  `qbase.lane<k>.<n>` (testbatch.py, and kit run.ps1 `-Instance` under QVR_LANE) and run_batch.sh takes the lane's
  lock. Lane 0 only now: eval.sh and the two teleporter suites (both rebuild maps/vrteleporters.bsp).
- **Undrawn console cases**: the 16 long agrunt/arena cases at 8 lanes took 337-619 s each drawn (the shared GPU), 19-128
  s undrawn (`vr_mock_fast 2`), 16.9 -> 2.1 min, the same verdicts. `run_baseline.py --jobs` runs every console and
  agrunt case undrawn except Rig Studio's (flat screen; `--nodraw REGEX` to choose), at most `--drawn-jobs` (4) drawn
  scripts at once, one testbatch suite at a time (it plays up to 8 copies), and the suites with `--exclusive` timing
  runs (reload magcollide/gunshape, ragdoll stagger/liquid_active) first, alone.
- **Fair exclusive runs** (kit slots.sh): an exclusive claim not drained within `EXCL_DRAIN` (20 s) is let go for
  `EXCL_BACKOFF` (15 s) so the waiting shared runs start, then made again; after `EXCL_PATIENCE` (180 s) it is held
  until drained, as before. A shared run behind a 45 s run and an exclusive request: waited 4 s (was ~41 s). Over a
  full baseline the agents' shared runs waited at most 31 s (was 848 s with 8 lanes and the old claim).
- **Full baseline** (`run_baseline.py`, now `--jobs 0` = 8 lanes by default; `--jobs 1` the old serial run): 30.0 min
  (round 1: 39.3; serial ~52), same verdicts as baseline.tsv but for next's known changes (fist_alert, melee_phase,
  swim_sound pass; reload 111/4, parry_pose fail) and the flaky counts (agrunt 60, 63, 71, 86, ragdoll_ragtest). Not
  the 10 min hoped for: the critical path is single drawn scripts, 4-10x their quiet-machine time beside the other
  agents' games (teleporter_edges 783 s, stealth 829, reload 622, flash_grab 517, eval 461, xr_runtime 466).
- **Start and map load, profiled** (`vr_startup_times`, vrtesthall): start to the first command 0.8 s, of it the
  window, GL context and shaders 478 ms (shaders 290), VR init 80, input 61, filesystem 60. The map load 1.31 s:
  entities spawned 757 ms (363 alias models loaded 702 ms: their normal maps 313, skins 179, md5/md3 replacements
  looked for 127), limb models 319, view models 163, 2304 image files looked for and not found 68 ms. Not cut yet.
## Refactor wave C1: the armed humanoids' old arms gone (branch `agent/refarms`)

His D1/D7/D8 answers (scratch TRIAGE.md, 2026-10-29) for the armed humanoids. About 2200 lines gone, no new behaviour.

- **D1, the vr_agarms 0 path removed.** The animation's arms bent onto the weapons and the rigid arm pieces
  (`vr_hitreact.cpp`: findLeftHand, solveArm, armsNow, misfit, reachIn, ArmModel and its pieces, solveArmPiece,
  relinkRigid, the hands' HandFit and the arm numbers it fed), `vr_animsmooth.cpp`'s piece and hand rows, the alias
  shader's `RigKickOrPiece` (now `RigKick` alone), `vr_agrunt_ik_info` and `vr_agrunt_grip_fit` (the rigid solve's;
  its Grip Editor button), the old per-weapon grip seeds (`vr_aggrip.cpp defaults`: used only with vr_agarms 0;
  `vr_agrunt_grip_dump all` prints the global defaults now), QC's `VR_AG_PlacePropOld`. `relinkArms` and `boneTurns`
  keep the skinned path only; `vr_agrunt_ik_err` prints the posed arms' numbers (`agarms::printErr`), `agprop`, `agrag`.
- **The posed arms' switches' old sides gone:** vr_agarms_body, _second_hold, _reach_ease, _jump_ease, _aim_line,
  _second_release, _shoulder_skin, _limits (the cone and twist stay: cone 0 turns the limits off), _arm_clear (QC's
  carry too), _cup_handle; vr_rig2_arm_limits (the authored rig's limits never clamp the arms). The numeric tunings
  stay (vr_agarms_elbow_rate, _wrist, _second, _shoulder_cone ...).
- **Retired** (retiredCvars): vr_agarms, vr_agrunt_ik_rigid, _ik_hands, _ik_wrist, _ik_twist, _ik_elbow, _ik_second,
  _ik_drawn, _ik_misfit, _ik_reach (the last two only tuned the old path), the vr_agarms_* switches above,
  vr_rig2_clavicle, vr_rig2_arm_clear, vr_rig2_arm_limits. Debug menu rows gone: Shoulder Skin, Shoulder Limits, Upper
  Arm Clear, Cup Handles; Automation: Posed Arms; Gameplay's Grip Editor: Fit Angles to Its Pose.
- **D8:** vr_rig2_clavicle merged into vr_agarms_clavicle, default 0.35 -> 0.6 (the value both armed rigs used: both
  have authored rigs, which took the larger of the two), config 217 (216 at first, renumbered at the wave C merge) moves a 0.35 to it. The held wrist's clearance
  (vr_rig2_arm_clear 6 and vr_agarms' constant 6) is one constant, `armClearHeldMost`.
- **D7:** one decode, `mdldequant::modelVertex(hdr, pose1, pose2, blend, v)` (model units, dequantised as drawn): the
  arms' skin and solve (`vr_agarms.cpp`), the legs' rest (`vr_aglegs.cpp`) and the arm pose's gun frame and anchor
  (`vr_hitreact.cpp`) read it instead of three raw trivertx copies; the skins call `mdldequant::prepare` first (built
  before the model's first upload they read raw bytes before). Fits move by under half a byte step of the model's
  scale (about 0.1 unit on the grunt).

Kept, decided here: `vr_agrunt_upper` (the Rig Studio's "Upper Body Held" layer toggles it), `vr_agrunt_strike_torso`
(a scale, with `_shoulder`; test 45's sheet sets it to 0 for its "without the body's turn" view). Left for a later pass
(they need their tests rewritten, which compare the old side): `vr_agrunt_aim_exposed` (test vr_armedgrunt_expose_test
runs both sides), `_aim_converge`, `_shot_drawn`, `_swing_one_hand`, `_tug_ragdoll` (tests 21, 22 run the old tug; it
is also the fallback when the physical tug can't begin); also kept on purpose: `_elbow_rate`.

- **QC's old pose branches gone** (second pass): `vr_agp_arms` and every 0 branch in `vr_armedgrunt_pose.qc` (the
  fists', the melee weapons' and the left blade's old blends, the old gun pose `VR_AGP_Gun`/`VR_AGP_GunTurn` and its
  `VR_AGP_LEFTMIRROR`, `VR_AGP_BladeTilt` and its `.vr_ag_tilt` fields, the old left carry `VR_AGP_CARRY_L`).
- **B10:** `Quake/vr/vr_agpose_shared.h` (the pattern of `vr_melee_shared.h`: #defines both compilers read) holds the
  weapons' attack classes `VR_AG_CLASS_*`, the pose keys' classes, poses, off-hand modes and code strides `VR_AGK_*`.
  `agpose::Class`/`Pose`/`OffMode`/`code` and the toolgun's melee checks take them from it; QC includes it once
  (`vr_armedgrunt_defs.qc`), its own float copies and `VR_AG_CLASS_MELEE_DEF` gone, the keys' pose numbers named.
- **ragdoll_grab** "grab rag enforcer: no grab": flaky, not this branch. The same failure (knight 5.3 m/s, ogre 2.9,
  enforcer none: one alternative outcome) is in arenamap's run of 2026-10-29 20:44; here the full script failed once
  and passed twice, origin/next's passed twice, the `rag` case alone passed on both. The enforcer's ragdoll falls at
  1.6 m/s when it is grabbed: near the grab's floor.
- Second pass's run (run_baseline --only agrunt|agarmor|rigstudio|ragdoll_grab, 8 lanes): 8 of 90 changed. Better:
  rigstudio_aim PASS, 63 (14 -> 7 fails), 71 (4 -> 3). Flaky (the known ones): 67 a timeout, 76 one more fail,
  ragdoll_grab as above. agrunt_1 (Thunderbolt 0 shots) and agrunt_47 (mjolnir timed out, 0 blows) failed under the
  lanes and passed alone (17/0, 77/0).

Tests (run_baseline --only agrunt|agarmor|rigstudio|limb|ragdoll_grab|ragdoll_ragtest, 8 lanes, twice): same verdicts
as baseline.tsv except: ragdoll_grab "grab rag enforcer: no grab" FAIL in both runs (open: not yet checked on next under
lanes); agrunt_86 (AI accuracy/reaction/retreat stats) 2 to 4 fails that change between runs; agrunt_11 a timeout in
the second run only (passed in the first); agrunt_74 failed once (downtug 20/21), passed in the second. Better: 76 and
rigstudio_aim pass, 63/71/72/ragtest one or more checks better.
## Refactor wave C3: his rules applied (branch `agent/refrules`)

- **D3, one liquid rule** (`worldtrace::liquidKind`/`isLiquid`, vr_trace.hpp; QC `VR_IsLiquid`, vr_util.qc): water,
  slime and lava, a current is water, the sky never. Every site uses it, client leaf or server contents: blood streams
  and gore (they counted the sky), bloodprints, shock, the water faces, view and leaves, wounds and haze (they missed
  currents), avatar, particles, shells, knockdown, box3d, physics (its swim check counted the sky), scenario, the rope's
  eye check, ragtest, propthink, the AD tank's probe; QC vr_grapple's swimmer (the sky was water there).
- **D6, gameplay randomness from the server's seed**: `srvrandom::Stream` (vr_srvrandom.hpp), each stream seeded from
  `derivedSeed` and restarted with the server's stream at every map load, so QuakeC's random() sequence is untouched.
  The audit found one C++ gameplay draw on libc `rand()`: the legsweep's Knockdown Chance roll (`Stream::Legsweep`).
  Cosmetic draws moved off `rand()` to local streams: decals (turn, mirror, atlas cell; their tests restart it,
  `markSeed`), the vr_particles 0 presets' colours, the water sounds' variants and pitch, physsound's picks and jitter.
  Explosion debris (P51) was already seeded per map (`serverReset` runs after the map's seed: `derivedSeed`;
  `vr_particle_seed` only for tests): left. Left on purpose: appearance random, hash and noise patterns, the
  clock-seeded effect streams, bench and diagnostic draws.
- **D4**: AD's spider and scorpion (QC vr_ad_monsters.qc) take the scourge's corpse health and the crawler group's
  knockdown chance (`vr_knockdown_chance_crawler`, Spiders and Crawlers; it was the Rottweilers'), as the engine's class
  alias has it. The boil is unchanged.
- **D8**: QC owns the stamina pool: `progs::staminaMax/staminaLeft` call `VR_Stamina_Max/_Left` (the caller's self, first
  parameter and return kept), for climbing and fatigue (the speed scale, vr_stamina_set, the debug hold). Weapon ids:
  `Quake/vr/vr_weapon_ids.h` (#defines, included by QC vr_defs.qc and the C++: flick, twohand, toolgun, lootbeams,
  box3d_hands, motion_play, notectx). notectx names the nail pistol, flare gun and revolver now; motion_play's weapon
  classes are as they were (those three still "other").

Tests: build 0 warnings. Decal stress and carve test twice with fixed seeds: identical (59 marks, 28 carved). The AD
monster test could not run here (AD's data is in neither qbase nor qrp: "no AD monster"). Baseline cases (explosion_debris, swim_sound,
weapon_catch, parryinterrupt, stealth, np_1, rv_1, flare_1, ragdoll_stagger/ragtest/climb/liquid_active, limb_wear,
prop_climb): 13 as the baseline; swim_sound FAIL 9/2 -> PASS 11/0 (its known failure, the under-water punch, a mock-hand
case: next has the mock-hand fix since the baseline).

## Faster headless tests and loads, round 3 (branch `agent/harness3`)

- **Timing runs no longer hold the slots while the author plays** (kit slots.sh): `acquire_exclusive` waits for his
  game (`ps -W`, a path in `quakevr-iw\Windows`; the cleanup agent's `quakevr-iw-cleanup` is not his) before claiming
  the exclusive slot, holding none, up to QVR_AUTHOR_WAIT (540 s, then exit 3 as run.ps1, freeing run.sh's agent lock);
  his game started while the slots drained: the claim let go, waited for again. run.ps1's own wait only catches a game
  started in the second between. (Was: run.ps1 waited inside the exclusive slot, every shared run behind it.)
- **Map load** (vrtesthall, `vr_startup_times`, exclusive runs, no-LTO build, normal-map cache warm): 1.31 s -> 1.17 s;
  entities spawned 766 -> 613 ms. Measured first with temporary sub-timers: of the 363 alias models' 700 ms, the
  normal maps' 310 (their cache's key hash 38, reading the cache 43, mips and upload 41, authored-map lookups 47),
  external skins 116 (PNG decodes 83), md5 hands/body 101 (3 models, 6 skins); limb models 323 (170 limbs of 22 kinds:
  their files built one by one on the main thread 116, their skins and normal maps the rest).
  - Limb models (vr_limbmodel.cpp prebuild): every kind's limb files built on the pool at once (startKind, as for a kind
    first seen in play), then precached on the main thread in the same order as before: 323 -> 186 ms.
  - The skin normal-map cache's key (vr_normalmaps.cpp): the texels and the islands' corners hashed by
    `diskcache::wideHash` (four lanes of xxHash64's round; FNV-1a a byte was 38 ms) -> 2.5 ms; NORMALCACHE_VERSION 2
    (the old files go once). `vr_normalmap_cache 2` on vrtesthall: 345 checked, 0 differed.
  - Pixels: four screenshots (vrtesthall both ways, e1m3, e2m1), before vs after: 769 pixels differ, all in a particle
    cloud; the same build twice: 573 (the particles' own randomness). Nothing else changed.
  - Not done: the rest of the limbs' cost (their skins' uploads and normal maps on the main thread), PNG decodes of
    external skins (vr_imgprefetch's list covers start-up and world textures, not model skins), the cache's 345 file
    opens, view models (md5 skins) and smooth rigs (116 ms, animsmooth::prepare per kind). Quit not profiled.
- **Critical path** (run_baseline.py): the two teleporter suites no longer lane 0 only but one at a time in any lane
  (key `tele`, limit 1; they are lane-aware since round 2), so lane 0 is eval.sh's alone; the queue weighs a drawn
  case's baseline seconds x4 (DRAWN_WEIGHT: 4-10x slower beside the lanes) so the long drawn scripts start first.
  stealth_tests.sh plays its scenes undrawn (`vr_mock_fast 2`) but dogs' (drawn moves logged) and horde's (a profile;
  STEALTH_DRAWN=1: all drawn), and gates, seethrough, hunt, props end on their `done` lines (-QuitOn): alone 59 passed,
  0 failed as drawn, ~1.5 min. run_baseline.sh's header says the lanes (a same-length edit: it was running).
- **Full baseline** (default, 8 lanes; another agent's full baseline ran at the same time): 29.1 min (round 2: 30.0),
  same verdicts as baseline.tsv but next's known changes (fist_alert, melee_phase, swim_sound pass; reload 112/3,
  parry_pose fail) and flaky counts (agrunt 63, 67, 72, 76, 86, ragdoll_ragtest). stealth 829 -> 194 s, reload 622 ->
  188, eval 461 -> 87; the critical path is now teleporter_edges (927 s: drawn screenshot runs) and flash_grab (760 s,
  already undrawn: its many passes beside the other games), then xr_runtime 577. Next: those three split into parallel
  parts or undrawn steps.

## Faster headless tests and loads, round 4 (branch `agent/harness4`)

- **Where the three long cases' time went** (alone, a quiet lane): flash_grab 376 s, of it `mounted` one game of
  240,630 frames (11 torso turns x 729 off-hand spots x 30 frames, 129 s at ~0.5 ms an undrawn frame), `timing` three
  of 12,045 frames (28 s each), `returning` 31 s, and a calibration game (`fg_cal`) before each section. The frames are
  game frames (the command buffer is already a read cursor, no copy a line); none only pads (24 frames: the hand still
  for the grab's speed check), so none was cut. teleporter_edges 165 s: 40 games of 2-6 s, 13 sections (head 23 s,
  push 20, recursion 18, ragdoll 16, corpse 15); xr_runtime 120 s: 27 games of 2-6 s. Beside the other lanes and
  agents each took 4-6x that.
- **Sections as arguments** (scripts): `flash_grab_test.py <agent> mounted:0-2,timing:bullettime,...` (comma-separated;
  mounted:A-B the torso turns A..B, timing:SCALE the time scales); `xr_runtime_test.sh <agent> [1|2-3|4,5|...]` (its
  numbered sections; 2b/2c are 2's; the fake runtime built once under a lock folder, and not again when newer than its
  sources: parts loading its DLL meanwhile); `teleporter_edges_test.sh <agent> clip,held,...` (the first entity found
  only for clip/push/held; cross, ragdoll, corpse and monster read only the game's log and now play undrawn,
  `vr_mock_fast 2`, TE_DRAWN=1 draws them: the same numbers within their usual run-to-run spread).
- **Parts** (kit run_baseline.py, uncommitted): PARTS runs those three as 9, 7 and 8 parts in parallel lanes, one row
  each when the last part ends (the parts' outputs joined in logs/<name>.log, the verdict by the case's group's rules,
  its seconds the longest part's); `--no-parts` whole as before. The three alone in 8 lanes: 2.1 min, the same verdicts
  (xr_runtime FAIL 101/2, flash_grab PASS 1/0, teleporter_edges PASS 0/0). The "tele" one-at-a-time key is gone from
  teleporter_edges: neither teleporter script builds vrteleporters.bsp (`--tests` writes nothing).
- **Lane folders made first** (coordinator's report: flash_grab 0/0 in wave C's baseline, `qbase.lane2/id1` missing):
  run_baseline.py starts one quick game per lane before the cases (run.ps1 builds the folder), makes the lane's id1
  before each case, and flash_grab_test.py makes its id1 before writing its .cfg.
- **Found, not fixed**: teleporter_edges' drawn-split lines are often missing (clip's control "0 frames", push "ends at
  y" empty, held "0 frames"; also in the phase-0 baseline's log): the script's verdict is its exit code, so it passes.
- **Loads**: vrtesthall (no-LTO, exclusive, warm caches): start 1.98 s, of it shaders 295 ms, map load 1.15 s (spawn
  599, view models 159, smooth rigs 110, limbs 191, md5 102). A GL program binary cache (glProgramBinary from one pack
  file per driver, 106 programs, 15 MB) was tried: 273 ms against 296: the driver's own cache already skips the
  compile, the cost is its program creation; set aside (git stash on the branch), not committed. Next for the shaders:
  KHR_parallel_shader_compile (status read only at the end). The other load items are still open.
- **Full baseline** (default, 8 lanes, wave B's agents running): 29.1 -> 19.5 min; teleporter_edges 927 -> 171 s,
  flash_grab 760 -> 59, xr_runtime 577 -> 97. No case is the critical path now (the longest 185 s): the cases' summed
  time was 6036 s (3881 in wave C's quieter run), 12.6 min of 8 lanes' work, so the rest is the shared machine's
  throughput. Verdicts as baseline.tsv but next's known changes (fist_alert, melee_phase, swim_sound pass; reload
  111/4, parry_pose fail), flaky counts (agrunt 63, 71, 72, 76, 86; ragdoll_ragtest passed), and two new: agarmor_5
  22/1 (a corpse's right thigh plate worn: the engine here is next's, likely its limb-plate work) and ragdoll_guard
  7/1 (guard stagger).
## Refactor wave B1: QC if-chains to tables (branch `agent/reftables`)

No behaviour change. Each step was checked by an A/B dump: a temporary QC function (not committed) printed every answer
of the functions touched, from the build before and the build after, and the two logs were diffed (identical, apart from
`random()` picks that differ run to run).

- **W17** `QC/vr_weaponutil.qc`: the 15 `WeaponIdTo*` chains read one table, `VR_WEAPONS[WID_LAST + 1]` (a
  `vr_weapon_row` a weapon id: pickup classname, item, ammo and secondary ammo, model and secondary model, name, throw
  multiplier, damage, bounds, starting ammo, clip, flags for reload / flick / secondary ammo / stabilised throw). The AD
  guns' model, name and clip and the clips a setting sizes (grunt gun, enforcer rifle, nail pistol, flare gun,
  revolver) stay code before the table read. Out-of-range ids still ASSERT and return the old fallback. Dumped: all 32
  ids with and without the secondary-ammo flag, and the 17 pickup classnames.
- **W20** `ClientObituary` (client.qc): the monster kills (`VR_OBIT_MONSTERS`), the traps (`VR_OBIT_TRAPS`) and a
  player's weapon kills (`VR_OBIT_WEAPONS`, with secondary-ammo words) are tables; the words that depend on more (MG3's
  variants, the vore's shove, the gib, the water, the laser's coin toss) stay code. Dumped: 33 monster classnames x 2
  variant states, 14 traps, the water deaths and 32 weapons x 4 flag/gib states.
- **W21** `CheckPowerups` (client.qc): `VR_Powerup_Warn/Expire/Glow(field, clock, ...)` for the seven timed powerups
  that share Quake's pattern; the lava suit's end, the shield and the antigrav (different rules) stay inline.
- **W19** `ImpulseCommands` (weapons.qc): `VR_Impulse_HandReady`, `VR_Impulse_CycleHand(hand, reverse)` and
  `VR_Impulse_WeaponButton(hand)` replace the five copies of the per-hand cycling and the two button blocks.
- **W18** `W_AttackImpl` (weapons.qc): the firing tail is `W_AttackImpl_Fire`, a dispatch to one `W_Fire_<weapon>` a
  weapon; the checks before it are unchanged. Melee: a pure move of the untracked swings.
- **C14** `vr_arena_perks.qc`: the perks' pool order, name, text, card model and colour are one table, `VR_ARK_PERKS`.

QC globals: 55869 before, 56505 after (+636: each table value is a global; the id format's limit is 65536); statements +982 (the compiler's array readers).

### Part 2

- **C10** `vr_mg_horde.qc`: `MGH_SpawnSquad2` reads `MGH_SQUADS` (a row a monster in MG1's spawn order: squad, skills
  0 / 1+ / 0 only, monster, place); the double demon's roll stays code. `MGH_SpawnWave2`'s ladders are `MGH_WAVES`
  (army or not, category, the roll's bound, squad, spawn point type) read by `MGH_WaveRow(army, cat, random())`: one
  `random()` at the same place, the category's last bound its range (4, 2, 3), its last row the else.
- **C11/C12** `VR_WAVE_KINDS` (vr_arena.qc): a row an arena kind (`AR_SP_*` is the row) then the horde's others; horde
  name, classname, the horde's spawn (id's monster) and the arena's (the armed grunt and enforcer).
  `MGH_SpawnMonster`, `VR_ARENA_ClassName` and `VR_ARENA_CallSpawn` read it.
- **C13** `MGH_horde_key_set_all(key, give)`; **C15** `AD_KEY_BITS` / `AD_KEY_WIDS`; **C16** `AD_UPGRADES` {weapon, AD bit,
  VR model "#vr" (vr_adweapons.cpp keeps its own list of the three), pickup model, classname, name} read by
  `AD_UpgradeFor`, `AD_VRModelName`, `AD_EnsureVRModel` (the made-bit is the row's) and `AD_UpgradeItem(row)`; **C17**
  `AD_UpgradeGate()` for the pickups and the toolgun's props.
- **C25** `AR_RING_DIRS` / `AR_ASIDE_DIRS`; **C32** `VR_AT_CVARS` {name, test value} feeds Save, Restore and SetDefaults;
  **M53** `VR_SGIB_INFLICTORS` (+ kinds), `VR_SGIB_NAMES`, `VR_SGIB_CDMG`; **B64** `VR_AG_SAFE_DIST(_MINE)`.
- **C04** `VR_AD_MonsterSetup`: each row one call (`VR_AD_MonKd`: VR_AD_Mon + VR_Knockdown_SetupReverse), the hell
  knight's row shared with the death lord and Quoth's death guard. Not a QC table: ~40 rows x 11 values would cost
  ~450-640 globals (this part's whole budget). **C06/C07** vr_monstermods.cpp `knownSwords`: vertex ranges
  `{first, count}` (no static-init allocations) and the dropped name in the row; `knownOnly` and `droppedName` read the
  table (the 20-term chain and the 16-case switch are gone).
- **W26** `VR_HAND_FIELD(e, hand, mainField, offField)` (vr_util.qc, a macro: inline functions with a branch do not
  inline in fteqcc) for the 48 `hand == cVR_MainHand ? e.a : e.offa` selects: progs.dat byte-identical. The 32
  `hand == cVR_OffHand ? e.off : e.main` selects stay (they differ for a hand that is neither).
- **C29** `VR_TestsFrame()` (vr_testutil.qc): the 42 QVR_TESTS frames, in their order, StartFrame's last step (they ran
  between production frames before; production is unchanged: the test frames do nothing without their request).
- **C30** `VR_World_Precache()`: worldspawn's Quake VR precaches, same order. No numbered-sound helper: precache_sound
  keeps the string's pointer (pr_cmds.c) and the QC has no strzone, so names built at run time are unsafe.
- **S01** `Misc/quakevr/gen_vr_cvars_qc.py` writes `QC/vr_cvars_gen.qc`: a handle for each literal `QVR_CVAR` of
  vr_cvars.inc the QC uses (1188); vr_cvars.qc keeps the 46 of the engine's cvars and the macro-made ones. `--check`
  fails a stale file: the kit's build.sh should run it.

Checked: an A/B dump of every squad at skills 0, 1, 3 (identical), `MGH_WaveRow` over 65 rolls a group against a model
of MG1's ladders (325, 0 mismatches), a seeded horde1 run with the wave monitor before and after (identical, 3 waves),
AD upgrade readers for every weapon id, small gib kinds x 12 hit kinds x 16 inflictors, ring/aside, the knownSwords
log on vrarena (identical). QC globals 56505 -> 56929 (+424); no AD data in the agent base (no AD run).

Not done: C24 (AD death precache as name arrays: ~85 globals), C48 (the test ladders have inline bodies: a {number,
function} table needs each its own function and two globals a row).
## Refactor wave B2: C++ kits and allocations (branch `agent/refkits`)

No behaviour change intended; the kits are listed in CODE_STYLE.md, "Kits (refactor B2)".

- **`gfx::RenderTarget`** (vr_glutil.hpp): a texture + framebuffer made for a size from a `RenderTargetFormat`
  (internal format, depth/shadow compare, clamp, max level 0, the framebuffer's label), completeness always checked
  (R05). Used by bloom's chain, the shadow atlases (lighting), the stereo resample target (now checked: a warning, it
  had none) and the half-size particles' targets (now checked: an incomplete one draws them full size from then on). The
  GL calls per site are the old ones (same parameters, the same final labels); the only state difference is texture
  unit 0 left unbound after making a half-size particle target (it was left bound to the new texture; the pass binds
  what it samples). Not moved: haze's scene copies (their own unit 6 and a format key), envmap, permdecals, water,
  wounds, upscale, stereo_single, gfx_gl's under-target (multi-attachment or other shapes).
- **`vr_toolpanel.hpp`**: `Frame` + `Grid` + `buttonCentre`/`pressedBy`/`buttonUnder` (Holster Setup, Edit Blade Shape,
  Pose Studio's button grids: the same float expressions), `pushUndo(stack, max, item)` (the three tools' bounded
  stacks; Pose Studio's two stacks pushed alike), `say(line, until, tool, text, seconds)` (each tool's 2.5 / 3 s kept),
  and the panel images' `Rect`/`fill`/`text`/`textFit` (arena cards and the toolgun panel had identical copies).
  Not merged: the hover functions (each tool's own zones/handles), placePanel/drawPanel (different shapes), the doom
  panel (S22) and the other tools' say lines (posing, rigstudio, relight: other signatures).
- **`audio::playAt` / `playLocal`** (vr_audio.hpp): the client's one-shot sounds (AD crossbow bolt, auto pump, shells'
  plips, chainsaw's weak pull, highlights' beep, vr_snd_play/_dir/_burst/bench). goresound (pitch) and bullettime
  (S_LocalSound by cvar name) keep theirs.
- **`SlotTable<Key, Slots, Keys>`** (vr_slottable.hpp): the weapons' and props' per-slot cvars (names kept, `define`,
  `keyOf` for the ID-changed callbacks).
- **Bindings**: the arena cards' 12 `VR_ARK_*` functions resolved once per map (`progs::bindings().arenaCards`), not by
  name each frame (findFunction scans every function).
- **VR_NewMap's prewarm** as an ordered `prewarmSteps` table (name, function, only with vr_load_prewarm).
- **Allocations**: sweepReach's `found`/`struck` in a `mem::Scratch` ("sweepReach"; were two vectors per fast swing);
  text3d's board/screen image lines in one kept string. Timescale's `wasFree` is a member vector (keeps its capacity: no
  allocation once warm) and probeKinds' is once a load: left.
- **Entities::members** (R08, vr_view_internal.hpp): the view entities in one list in forEachEntity's order (the draw
  order into cl_visedicts), `zip(...)` for the arrays visited per index (pauldron cap and lames, the SSGs' frame and
  barrels, the revolver's parts); a static_assert on the sizes catches a member left out. Checked once with both walks
  side by side: 346 entities, the same addresses in the same order.
- **Allocations (2)**: the decals' repack into `scratch.packed` (swapped with the settled vertices: the old buffer kept for
  the next, released at a map change); wounds' splats (wound, bloodyHead, smearHand, splashOnto) in one `mem::Scratch`
  ("wounds") and facingBlow writes into its `blown` (was a vector copy per blow); a brush bake's lit faces in
  `bakeScratch` (the queue keeps its own copy); retro pick's overlay line and picked label in char buffers (were a
  String + va + hitLabel's String each frame while picking). The brushes' autoload `text`/`warning` run once a map: left.
- **footTrace** (B53, vr_avatar.cpp): `progs::ServerVm` + `worldtrace::serverLine(a, z, player)` for the raw push, SV_Move
  and pop; one push per call as before (climbing.traces counts the same calls).
- **R51 not merged**: the five sites detect five different events. view's patchModelFlags already uses the canonical
  world + `worldGeneration()`; dynres resets after any frame gap (a hitch too, not only a map); lootbeams and highlights
  reset when their clock runs backwards (also cl.time's snap back in CL_LerpPoint, and a loaded save's later clock is not
  caught); temporal resets on a new world pointer only (not on the same map loaded again). Moving any of them to
  `VR_OnClientClearState` (the de facto client registry, listed in CODE_STYLE) changes when it resets.
- Checked (second pass): the covering cases as baseline.tsv but reload 112/3 (known) and rv_1 34/1 once (a slug's damage
  roll of 29.1 against 30; 35/0 run alone); a console check (scratch/b2check.sh: decal repack twice, wounds/spatter/
  smear/arm tests, retro pick, a brush made and baked) printing the same lines as the base exe (the decal tallies and
  edict numbers vary run to run on one exe: map-start chips). Bench (LTO, exclusive, idle_e1m1 + combat_48 +
  decals_1024_stream, 3 reps; this branch, base e30e689a2, this branch again): frame p50/p99 and GPU the same; combat
  heap allocations -5.5% (10.9 -> 10.3 a frame); idle CPU busy +3.7% / +5.2% (0.03 ms; combat -3.8% / -0.4%, decals -0..-6%).
- **C26 left**: VR_ARENA_Leftovers' per-frame findflags drives the fade's per-frame alpha ramp and the per-frame up-again
  and held checks; a next-due gate would move those, so the fade timing is not provably unchanged.
- **Left for a later pass** (no row details found, or a merge would change output): R14 surfaceAbove (particles: client
  contents, up only, 8 halvings) vs physics' surfaceOver (server contents, both ways, 12 halvings to 0.1 u, sky stops):
  six differences, a policy object for little gain; R33 beyond the prewarm table; R51 and C26 (above).
- Tests: the covering cases (arena_perks 1-5, autopump, selfchecks, weapon_catch, held_over_props, pickup_mag,
  throw_spin, vrtrailer, stats_levelchange, teleporters_chase, explosion_debris, reload_contact, ammocolor_1, cellcord,
  walltorch_grab) as baseline.tsv but autopump 7/7 (known); Holster Setup and Edit Blade Shape opened, pressed, undone,
  cancelled by their commands (say lines as before).
- Bench (LTO, exclusive, idle_e1m1 + particles_dense + explosions_3s, 3 reps; this branch, origin/next, this branch again): frame and GPU times within noise, heap allocations the same; CPU busy +3-5% in the first run and -5-14% in the rerun (order noise, benchresults/b2_new, b2_base, b2_new2). A render-scale 0.8 eyeshot printed no incomplete-framebuffer warning (its PNG hash differs between two runs of the same exe, so hashes cannot compare builds).

## Refactor wave B3, group G1: hands/weapons/locomotion (W), platform/QC bridge/menus (S) (branch `agent/refb3ws`)

No behaviour change intended. The scripts that did it (the worktree's scratch/, not committed): b3_scan.py (the inline
spellings per area), b3_vm.py / b3_loop.py / b3_vec.py (conversions that refuse a site they cannot prove the same),
b3_split.py / b3_qcsplit.py (moves), b3_progscmp.py (two progs.dat compared with string offsets resolved).

- **Inline spellings**: 21 manual `PR_PushQCVM(&sv.qcvm)`/`PR_PopQCVM` pairs -> `progs::ServerVm` (only where the pop
  closed its block, no return/break between); 7 `num_edicts` loops -> `progs::forEachEdict` (the free check folded,
  `continue` -> `return`; no early exit, the index unused); 150 `glm::vec3{p[0], p[1], p[2]}` -> `geom::vec(p)` (float
  operands only: 8 byte-array sites the compiler refused were put back); `vr_ad_mounted` read directly (2); carry2h's
  test uses `progs::localPlayer()`. Left (guards differ): 26 `EDICT_NUM(1)` and 15 `svs.clients[0].edict` sites
  (localPlayer() also wants client 0 active), 15 push/pops with code after the pop in the same block or a second push
  (backend_mock x8, builtins, fatigue, flashlight restoreState, server x2, trace, view_hands), 31 loops with early exits or the
  index used, 9 SV_Move calls (boxes or computed ends), by-name lookups that test registration (gamedir) or bind the
  mock hand's fields (progs.cpp).
- **Splits** (pure moves: each block `#include`d where it was, so the translation unit is the same text; none uses
  `__LINE__`/`__FILE__`): vr_gadget.cpp -> vr_gadget_holo.inc, vr_gadget_fps.inc; vr_menu.cpp (6742 -> 5036 lines) ->
  vr_menu_checklist.inc, vr_menu_help.inc, vr_menu_paths.inc, vr_menu_instances.inc; vr_builtins.cpp (3405 -> 1078) ->
  vr_builtins_melee/ragdoll/nav/held/hitmodel.inc (the table stays: check_builtins.py reads it); vr_toolgun.cpp ->
  vr_toolgun_pick.inc, vr_toolgun_physgun.inc. QC: weapons.qc's tail (holsters, throw/drop, give/take, the weapon
  frame) -> vr_wpnholster.qc, listed right after it in progs.src (progs.dat compared: see the test line). Not split:
  the OpenXR backend (one 2400-line class: by phase means moving its inline members out), vr_cvars.inc (six tools parse
  it, and its test cvars are scattered: a vr_cvars_test.inc would reorder registration).
- **Long functions**: mockHandTo_f (508 -> 202 lines): its 11 sub-commands' blocks (each ends in a return) are
  handToMag, handToWallButton, handToBladehandle, handToPosePart, handToZone, handToHolster, handToPouchPort,
  handToPouchMag, handToAmmoOrLoadPort, handToNearest, handToRagdollOrBy (the locals they read passed in). Left: the
  menu page builders (one braced list each: data, long by nature), the hot per-frame climbPreThink / VR_ClientClimb /
  foegrab::serverFrame / input update / setupHand / buildMove (no proof of "unchanged" short of a bench pair each).
- Tests: build 0 warnings and every check; 31 covering cases (reload*, autopump, pickup_mag, mapgun_*, grip_*,
  ragdoll_grab, flash_grab, weapon_catch, held_over_props, throw_spin, slider_step, selfchecks, update_notice, arena_1,
  stats_levelchange, limb_wear, prop_climb, walltorch_grab, collectfx, fist_alert, swim_sound, xr_runtime) as
  baseline.tsv but the known changes (autopump 7/7, fist_alert and swim_sound pass, reload 111/4: the same four lines
  as other branches' runs). progs.dat before and after the QC move: statements, functions, defs and globals identical
  (b3_progscmp.py). No hot per-frame function was split, so no bench pair.
## Refactor wave B3, group G2: body, physics, melee (branch `agent/refb3bpm`)

No behaviour change intended; areas B (body, rigs, tools), P (ragdoll, physics) and M (melee, gore) only.

- **Canonical spellings** (step 1; scripts rewrote only the exact forms, the compiler rejected the rest): 14 edict
  loops of the form `for(i = first; i < qcvm->num_edicts; i++) { e = EDICT_NUM(i); if(e->free) continue; ... }` (or
  `if(e->free || x) continue`, `if(!e->free && x) {...}` as the whole body) -> `progs::forEachEdict(first, ...)`
  (bodies without break/return and not using the index; their `continue` -> `return`). 15 server VM push/pop pairs ->
  `progs::ServerVm` where the pop was the scope's last statement or right before a bare `return`. 144
  `glm::vec3{p[0], p[1], p[2]}` of float arrays -> `geom::vec(p)` (byte arrays, e.g. trivertx `.v`, stay).
- **Left** (not identical): the `EDICT_NUM(1)` / `svs.clients[0].edict` player reads (51; `localPlayer()` also needs
  client 0 active and returns null, which those sites never check); loops that break, return or use the index (25);
  push/pop pairs with code after the pop (Con_Printf, if) or the old VM read (rigid, motion, agarmor, crates, wounds,
  hitmodel, hitmodel_diag, debris, chainsaw:402); `vec3_t v{x,y,z}` stores.
- **Splits** (step 2, pure moves): vr_physics.cpp (2282 lines) -> vr_physics.cpp (1199) + vr_physics_water.cpp (1108:
  splashes, sounds, wading, swimming, VR_AllowWaterSplash); the helpers both use (`active`, `f`, `vec`, `finite`,
  `hasFlag`, `solidOf`, `isClient`) are inline in vr_physics_internal.hpp (`physics::detail`), `waterFeedback`
  declared there. vr_melee.qc (4933) -> vr_melee_params.qc (numbers, what a hand holds), vr_melee.qc (following,
  deciding a blow), vr_melee_bash.qc (swish, stamina/counter, stance/bash, each frame), listed in that order in
  progs.src: progs.dat is byte-identical.
- **Splits, second pass** (pure moves; the moved code's anonymous-namespace types go to `<file>_internal.hpp` in
  `<ns>::detail`, the functions and state the other file uses are wrapped in `namespace detail` where they stand, so the
  order of definitions and of static initialization in each file is unchanged; the moved state is all constant-initialized,
  avatar's dynamic `bindPose` stays in vr_avatar.cpp behind `bind()`): vr_agarmor.cpp -> + vr_agarmor_check.cpp (the
  vr_agarmor_check self-check); vr_motion_play.cpp -> + vr_motion_eval.cpp (vr_motion_eval, vr_motion_header_check,
  evaluating); vr_avatar.cpp -> + vr_avatar_legs.cpp (clock, gait, water, stance, swimFoot, solveLeg) + vr_avatar_climb.cpp
  (footTrace .. updateClimbing, logClimbing); vr_box3d_diag.cpp -> + _piles, _perf, _ragdoll, _player.cpp (commands
  declared in vr_box3d_diag_internal.hpp); vr_wounds.cpp -> + vr_wounds_net.cpp (the event queue, queueInfo, parseEvent,
  parseClear). Checked: 0 warnings; agarmor_1/5, ragdoll_*, prop_climb, limb_wear, selfchecks, swim_sound, slopes, judo,
  agrunt samples, dead_gear, collectfx, melee_phase, fist_alert unchanged against baseline.tsv but for the expected passes
  and the listed flaky ones; eval.sh full 163/171 before and after (the rows that differ, damage in the last digit, differ
  as much between two runs of the old build).
- **Not done**: hitreact (arms, skin) and ragactive (swim, hold, cut) splits (budget); combat.qc's
  parry block sits between T_DamageDeal and T_Damage, so a file of its own changes the compile order (no byte-identical
  check). Long functions (step 3): none split (derive, createRagdoll, skinOf and the plans have no comment-marked phases
  over a context that could be proven unchanged within this pass).
## Refactor wave B3, group G3: render/fx/audio and campaigns (branch `agent/refb3rc`)

No behaviour change. Steps of the brief (refactor_plan.md, TRIAGE.md wave B):

1. **Inline spellings onto the canonical helpers.** 98 `glm::vec3{p[0], p[1], p[2]}` and three-line `out[i] = v.x/y/z`
   stores in 27 R files -> `geom::vec` / `geom::store`. 14 PR_PushQCVM/PopQCVM pairs (lootbeams, mgroups, view_diag,
   portals, and portals' three local `PopVm` structs) -> `progs::ServerVm`; 4 `num_edicts` loops -> `progs::forEachEdict`
   (lootbeams empty test, mgroups live list and VR_MonsterHealth_Frame, brushes' stray-entity sweep); 6 point SV_Move
   calls -> `worldtrace::serverLine` (portals x5, brushes' floor probe). QC: MGH_RespawnAllPlayers -> VR_Player /
   VR_NextPlayer.
   Left as they are (guards or semantics differ): `EDICT_NUM(1)` / `svs.clients[0].edict` where the guard is only
   `sv.active && maxclients >= 1` (localPlayer() also needs client 0 active: lootbeams test, brushes, portals x4);
   box traces (SV_Move with mins/maxs: brushes_diag, portals x4) and portals' own trace hooks (MOVE_HITPASS with
   const inputs); loops with an early stop or a second bound (brushes' adopt loop `&& !num`, view_diag's
   `i < cl.num_entities`, portals' trigger_teleport scan); vr_ambient's byte lightmap read and vr_lighting's matrix
   rows; by-name lookups with no handle yet: `ED_FindFieldOffset("mangle")` (portals; needs a vr_fields.inc row, S
   area), `Cvar_VariableValue("skill")` (voicenotes; the engine has no extern for `skill`), one-off findFunction calls
   in test commands and VR_AD_WorldOnDemand (once a map).
2. **File splits.** vr_glsl.h (3698 lines) -> vr_glsl_lighting.h (shadows, AO, zfix), vr_glsl_surface.h (parallax,
   specular AA, detail, liquids), vr_glsl_world.h, vr_glsl_alias.h; vr_glsl.h keeps its head (post-process, frame data)
   and tail (gates, sprites, cluster spots, temporal) and includes the four in the old order. Macros only, so every
   shader's text is unchanged. Second pass (pure moves; what more than one part uses went to an `_internal.hpp`):
   - vr_ao.cpp -> vr_ao_cache.cpp (the disk cache: FileHeader, bakeKey, fnv64, the file's read/write, pruneOnce);
     vr_ao_internal.hpp (namespace qvr::ao::detail) holds the bake's numbers, rayDirs (defined in vr_ao.cpp), Tri,
     BakeJob, aoFnvStart, BAKE_VERSION/CACHE_BUDGET and the cache's declarations. The bake log's texel hash (~1822) is
     FNV-1a **32-bit** (its `%08x` in the log): diskcache::fnv is 64-bit, so it stays (the output would change).
   - vr_audiotest.cpp (measurements, made sounds, vr_snd_test) -> + vr_audiotest_spatial.cpp (circle, ild, clicks),
     vr_audiotest_room.cpp (occlusion, reverb, doppler, nearfield, distance, hands), vr_audiotest_mix.cpp (bench,
     full band, golden); shared pieces in vr_audiotest_internal.hpp (namespace qvr::audio::testkit).
   - vr_view.cpp's body pieces (setupBody, vr_body_wrist_info, dripBlood, setupPauldrons, the weapon buttons) ->
     vr_view_body.cpp. Into vr_view_internal.hpp: drawnBuild/drawnBare/BareLight/bareLit and ButtonState with
     buttonStates/frontButtonStates (inline), bodyModelName, bareForearms, bareLitHand, armorWorn, showPlayerState and
     the moved functions (out of their anonymous namespaces, as view_detail's).
   Left (budget): particles, relight, audio, portals, brushes' file split, vr_arena_test.qc.
3. **Long functions** (a phase a function, over a context struct; each body moved as it was, bound to the struct by
   references):
   - vr_water.cpp buildMesh (466 lines) over MeshBuild: readSource, gatherFaces, findRims, bucketRims, pinAt (was a
     lambda), makeFaces, placeFaces, findLeaves, uploadMesh. The same mesh: faces/vertices/triangles/rims and the
     developer hash identical on e1m1, e1m4, e1m7/e3m3 and e4m3 (`developer 1`, "geometric waves' mesh" line).
   - vr_haze.cpp draw (215) over HazeView: gatherLava, gatherEllipsoids, coveredPart, copyCovered, drawLava,
     drawEllipsoids (QVR_GPU_PROFILE("haze") still round the drawing). Eyeshots over e1m7's lava before/after differ
     no more than two runs of one build do (1-2 % of pixels, the haze-off shot as much as the haze-on ones): the scene
     isn't bit-reproducible run to run, so this is a weak check.
   - vr_vram.cpp report_f (230) -> sumUp, processVram, printTotals, printLists, printDiff, remember: vr_vram_report
     and its diff print the same on e1m1.
   - vr_brushes.cpp build (283) over BrushBuild: buildHulls, buildEdges, buildTextures, buildSurfaces;
     vr_brush_selftest the same (PASS, 80 checks). fromText (157 lines) left: under 200.
   Not benched: buildMesh and build run once a map / brush; haze draw does the same work once a view, with six more
   calls (only with a flame or lava in view).

Checks: build 0 warnings and all checks; covering baseline cases (teleporters, collectfx, mapflameburn, swim_sound,
vrtrailer, selfchecks, stats_levelchange, dead_gear, explosion_debris, arena 1/5) as in baseline.tsv.

## Refactor follow-up: the player edict (branch `agent/playeredict`)

No behaviour change. Waves A-B3 left ~100 `EDICT_NUM(1)` / `svs.clients[0].edict` reads: `progs::localPlayer()`
also needs client 0 active, and most of these sites guard only on the server running. New
`progs::playerEdict()` (vr_progs.hpp) = `sv.active && svs.maxclients >= 1 ? svs.clients[0].edict : nullptr`; CODE_STYLE
"Engine access" says when to use which. With the server's VM current and `sv.active` true, `EDICT_NUM(1)` and
`svs.clients[0].edict` are the same pointer (SV_SpawnServer sets it; the edicts are never reallocated in a map) and
svs.maxclients is never under 1, so an `if(!sv.active) return;` guard followed by `EDICT_NUM(1)` is exactly
`playerEdict()`.

- **playerEdict() (55 uses, 32 files):** every site behind `sv.active` (console commands, tests, server-frame code,
  QC calls with the player as self), the inline `sv.active && svs.maxclients >= 1 ? ... : nullptr` copies
  (bloodstream, bullettime calm, physics grabBench, cheats, vr_input shoveSlide, the toolgun menu, portal stuck), and
  bullettime's single-player `meterPlayer` (`svs.maxclients == 1 ? playerEdict() : nullptr`).
- **localPlayer() (11 sites):** the reads behind `localPlayer()` or `sv.active && svs.clients[0].active` (avatar climb
  foot trace, climb try/probe, vr_trace's pass edict, motion heldProp / serverFrame, motion play / review, note
  context, the quicksave tool's dead check).
- **Left (35 reads; guard differs):** no `sv.active` in the function, the caller's guard or a test's state instead:
  backend_mock nearestThrownWeapon / handToNearest / turnFrame; box3d presstest frame, striketest frame (4),
  box3d_ragdoll heldLoadout and struggle holdsOf (`svs.maxclients` only); foegrab startTrace, foegrab_diag walk test /
  nearestMonster / neck and grab diag frames; held carryCheck; ragtest callQc / turnAway / fireGrab / nearestTarget;
  ragtool player(); rigseq entityArg; rigstudio findStage; timescale VR_PhysicsEntityBegin (per entity, hot);
  toolgun heldByPlayer, updateAim, removeTarget. Other guards: chainsaw pulledField (the current VM's num_edicts),
  continue VR_SaveContext (`svs.clients` only), menuui monsterHunting (none), portals crossPlayer (a comparison under
  `cls.state`), portals infoBody (no `sv.active`), menu.c (C).

Checks: build 0 warnings and all checks; covering baseline cases (agrunt 1/2/12, ragdoll_*, selfchecks, reload,
arena_1, stats_levelchange, prop_climb, teleporters, agarmor_1, rigstudio subjects/aim) vs baseline.tsv.

## Refactor wrap-up: ARCHITECTURE.md, the canonical-helper guard, the baseline refreshed (branch `agent/archguard`)

No code change. **docs/vr-port/ARCHITECTURE.md** (185 lines; linked from docs/README.md, CODE_STYLE.md and
BUILDING.md): the layers, the seven areas' files, the QC <-> C++ bridge (builtins, fields, Bindings, cvar handles,
the shared `.h` headers, the move/entity protocol hooks), a host frame from `VR_BeginFrame` to `VR_RenderView` with
the player's physics hooks in order, the canonical helpers in one table, where new code goes, the build's checks, the
tests, the baseline and the A/B bench method. Worker briefs point at it.

**`Misc/quakevr/check_canonical.py`** (run by the kit's build.sh after the cvar-handle check; ~2 s): counts per file
the sites of 12 families (`geom-vec`, `geom-math`, `rnd-wrapper`, `strings-copy`, `haptic-direct`, `qcvm-push`,
`player-edict`, `cvar-by-name`, `field-by-name`, `qc-find-player`, `qc-math-copy`, `qc-test-check`; comments ignored)
and fails when a file holds more than `Misc/quakevr/canonical_allowlist.txt` allows. The allowlist is today's tree:
155 sites, each line with its reason (canonical, left on purpose with ROUND23's reason, or not migrated yet: 31
manual `PR_PushQCVM` pairs, 34 player-edict reads with other guards, 11 ungated or combined `be->haptic`, the
angleBetween/percentile/quakeFromTracking copies, 8 old per-test `_Check` functions). `--list <family>` prints the
sites; `--tighten` lowers counts after a migration. Proven on a temp copy: one planted copy per family (a `vec(const
float*)`, `wrap180`, a smoothstep lambda, `rnd`, `be->haptic`, `PR_PushQCVM`, `EDICT_NUM(1)`, a QC player `find`
and `VR_Carry_Cross`) gave 8 failures and exit 1; the clean copy passed.

**Baseline** (kit, `scratch/refactor/`): `baseline.tsv` is now the full run 20261030-090721 on next dbff6520c (170
cases, pe_full.log; the bench row carried from phase 0), the old file kept as `baseline_phase0.tsv`.
`run_baseline.py --compare` reports the cases that vary run to run (`FLAKY`: agrunt 1/47/60/63/67/71/72/76/86,
ragdoll_ragtest/grab/guard, rv_1, judo, stealth, agarmor_5) apart as "FLAKY CHANGED", not counted in its exit status.

## Refactor leftovers L1: hitreact and ragactive splits, derive's phases (branch `agent/leftsplit1`)

No behaviour change.

- **vr_hitreact.cpp** (2752 lines) -> vr_hitreact.cpp (1880) + vr_hitreact_arms.cpp (450: the arm-pose slots, drawn
  poses, placeArmPose, bodyTurn, fitRigid, specOf ...) + vr_hitreact_skin.cpp (348: relinkSkinned) +
  vr_hitreact_internal.hpp. The anonymous namespace was at global scope, so the shared part is `hitreact_detail` at
  global scope too, with `using namespace ::hitreact_detail;` (vr_menu_internal.hpp's pattern): name lookup is the
  same as before. The armed monsters' table (its types, `inline constexpr armedRigs`, its four small lookups made
  `inline`) and the arm-pose types moved to the header; the shared functions and state (`armPoses`, `ragErr`) are
  declared there and defined where they stood, wrapped in `namespace hitreact_detail`. The canonical allowlist's
  geom-math line moved with its site (vr_hitreact_skin.cpp).
- **vr_ragactive.cpp** (5221) -> vr_ragactive.cpp (3617) + vr_ragactive_swim.cpp (600: the Liquids section, the living
  swim and flail entry points) + vr_ragactive_hold.cpp (494: Grabbing others, Climbs, climbing/beginClimb) +
  vr_ragactive_cut.cpp (695: Dismemberment) + vr_ragactive_internal.hpp (`qvr::ragactive::detail`: the shared
  constants, SwimParts, the b3glm/geom using-declarations; the shared functions declared, defined where they stood;
  massCentre's default argument now on its declaration). Staggers stayed in vr_ragactive.cpp (the inventory's
  "stagger/swim/liquid" was an eighth of the file, not a section). The moved state (`envOthers`, `armPoses`) was
  checked constant-initialized (a temporary `constinit` compiled), so no static-init order changed.
- Both splits made by script (kit worktree arenatex `scratch/split_gen.py`, `split_hr.py`) and checked by a
  line-multiset compare: every original line in exactly one file, the only differences the new declarations,
  namespace lines and the moved default argument.
- **ragdoll::derive** (420 lines) split at its five comment-marked phases over `DeriveWork` (its locals):
  derivePieces, deriveLoose, deriveClusters, deriveBones, deriveRig; each phase's text unchanged under reference
  aliases of DeriveWork's members, its locals' declarations turned into `resize`/assignment. Proven on the rigs
  themselves: the old exe derived 32 rigs (vrtesthall, e1m3, e4m3, e2m6, e3m4: 23 models with MG3's) into a fresh
  `vr_cache_dir`, the new one derived them again with `vr_ragdoll_rig_cache 2` (derive and compare every field
  byte for byte): no "differs".
- **Checked**: 0 warnings, all build checks. run_baseline `agrunt_|ragdoll_|limb_wear|swim_sound|agarmor` (91 cases):
  unchanged against baseline.tsv but agrunt_11 (TIMEOUT in a run where four lanes stalled 1000+ s at once; rerun
  alone with 36 and 86: PASS, 0 changed) and the flaky 63/71/72/ragtest. A/B (ThinLTO exes, ABBAABBAAB, 5 reps,
  cpu_busy median): armed_16 A 3.129 / B 3.161 ms (+1.0%, sd 0.11-0.17), ragdolls_32_active A 3.207 / B 3.188 ms
  (-0.6%; B's mean +4.5% from one 3.81 outlier): within noise.
- **Second pass: createRagdoll and the ragactive frame functions**, each split at its comment-marked phases over a
  context struct (`CreateWork`, `DeathWork`, `StaggerWork`, `EnvWork`, `DriveWork`, `SwimWork`, `DismemberWork`, in an
  anonymous namespace above the function), made by one script (kit worktree arenatex `scratch/phasesplit.py`, the
  specs `split_create.py`, `split_drive.py`, `split_frames.py`): a local used by a later phase becomes a member, its
  declaration an assignment under a reference alias of the same name (a reference local: a pointer member); each
  phase's text otherwise unchanged. Only by hand: createRagdoll's `drawnBone` lambda (used by two phases) is made by
  `drawnBoneOf` (its body the same but for `w.`), mass/friction's one declaration split in two, staggerOutcome's one
  bare `return;` (the stagger's frame over) a `return false;` that the caller acts on. createRagdoll 418 -> 82 lines
  (6 phases), deathFrame 221 -> 9 (3), staggerFrame 286 -> 23 (5), envFrame 313 -> 55 (4), drive 322 -> 45 (6),
  swimFrame 197 -> 59 (5), dismemberFrame 287 -> 24 (5).
- **Proof**: `vr_physics_hash ragdolls` (new: every ragdoll part's transform, velocity and spin, FNV-1a) every 30
  frames over seven scenes (`scratch/hashrun.py`: 32 grunts blasted, staggers/knockdown/get-up, a judo throw, four
  drowned in vrtesthall's pool, a climb, four limb cuts (arm, leg, head, forearm), a grunt killed and landing; 193
  checkpoints, 145 with live ragdolls): the old exe twice gives the same 193 hashes (bit for bit: the ragdolls are
  deterministic here; `vr_bench_statehash` is not, run to run), and the split one gives them again (a no-LTO build
  against the ThinLTO baseline: the same bits). Committed one function at a time, re-made by the same scripts; the
  last commit's files compared equal to the tested ones.
- **Checked**: 0 warnings, all build checks; run_baseline `agrunt_|ragdoll_|limb_wear|swim_sound|agarmor` (91 cases): 0
  changed against baseline.tsv (flaky apart: agrunt_63, agarmor_5, ragdoll_grab).
- **A/B** (ThinLTO exes, ABBAABBAAB, 5 reps each, cpu_busy median): ragdolls_32_active A 3.628 / B 3.568 ms (-1.6%),
  armed_16 A 3.286 / B 3.150 ms (-4.1%; A had one 4.52 outlier): within noise.
- **Skipped**: climbFrame (one comment-marked block of 171 lines: nothing to split on), agarms skinOf/solve and avatar
  solveArm (budget; they need their own proof: an arm pose dump over a grunt pose sweep, and the avatar's arms).

## Refactor leftovers L4: load-time items (branch `agent/leftload`)

Baseline (no-LTO, exclusive, warm caches, median of 3): vrtesthall map load 1173 ms, e1m1 879, vrstart 1484 (the big
map here: AD isn't in the kit's base); of each, the alias models 659 / 418 / 674 (their normal maps 276 / 159 / 274,
skins 178 / 102 / 182, md5/md3 lookups 126 / 123 / 123), limbs 198 / 22 / 196, view models 163 / 159 / 159, smooth rigs
107 / 20 / 109; the start-up's shaders 294.

- **Smooth rigs** (`animsmooth::rigData`'s rebuild, 75 of its ~100 ms on vrtesthall, sub-timed): `refit` loops over each
  bone's own vertices (a list made once, in vertex order: the same sums in the same order), the places by vertex chunk
  and the refits by pose on the pool (`jobs::Site` "anim smooth rebuild places/refits"; each item writes its own). A
  temporary dump hashed every rigged model's rest rows, rebuilt keys and unfit masks: identical (18 models). Interleaved
  ABBAAB on vrtesthall: smooth rigs 109 -> 47 ms, map load 1177 -> 1103.
- **Shaders, GL_KHR_parallel_shader_compile** (or ARB's; `gl_parallel_compile_able`, gl_vidsdl.c): GL_CreateShaders
  compiles and links without reading any status, then `GL_DeferredCheck` reads them in the order made (the same
  errors: a shader's compile, else the link). The same run: 296.5 -> 291.6 ms, within noise: NVIDIA's own disk cache
  already skips the compile (round 4); kept for a cold driver cache (after a driver update), not measured (that cache
  can't be cleared here).
- **Quit profiled** (`vr_startup_times`): `VR_TimeQuitBegin/Mark/End` from `quit` (Host_Quit_f, else Host_Shutdown)
  through Host_Shutdown's stages; printed at the quit (the log still open) and kept in `<game>/cache/quit_times.txt`,
  printed by the next session's `vr_startup_times`. vrtesthall: 77 ms, of it the config written 35, the video (GL
  context, window) 36, VR shutdown 4.5.
- **Drawing**: four screenshots (vrtesthall at 60 and 150 frames, e1m3, e2m1), before vs after: 1199 pixels differ; the
  before build twice: 1342 (particles). Low-impact eyeshots (the author's game ran), so identity only.
- **Not done** (measured, left): the normal-map cache's 345 reads (42 ms: the key needs the skin's texels and islands,
  so a read-ahead would need a list of last session's keys and holds their maps in memory); the external skins' 203
  decodes (80 ms: all 192 of the start-up list are taken from the read-ahead already; what's left is the byte compare
  re-reading each file and the copies); limb skin uploads (limbs 193 ms) and md5 view models (104 ms: their 6 skins'
  uploads and normal maps): main-thread GL work. e1m1/vrstart after-times not taken: the exclusive slot waited on the
  author's game (their before-times above; smooth rigs is per model kind, so ~60 ms off vrstart, little off e1m1's 20).

## Punchier gunfire (agent/gunfx)

Per-shot gunfire particles keyed off the weapon id (Quake/vr/vr_gunfx.cpp), one path for your guns (vr_weaponfx.cpp
playerFired / frame: the drawn muzzle), the armed grunts' held weapons and the grunts' and enforcers' own guns
(monsterFired: the prop's Muzzle anchor, monsterMuzzle). Table: shotguns (shotgun, grunts' gun, AD defender) sparks,
smoke and a port puff; super shotgun / Widowmaker each barrel's sparks, smoke, the open breech's puff; revolver sparks,
smoke; nailguns and nail pistol a few small sparks, a small quick puff, barrel heat (wisps once sustained fire passes
0.35 heat, cooling 0.35/s); launchers a few sparks and a large cloud; rocket launchers also the backblast (smoke
behind, dust off the floor under it: a worldtrace::line down 80 units); flare gun redder sparks, smoke; energy guns
(thunderbolt, laser cannon, enforcer rifle, AD plasma, centurion) ElectricSparks arcs, no smoke. Excluded (kind
none): flamethrowers, crossbow, melee, grapple, toolgun, Mjolnir. Existing effects kept, not duplicated: the QC's
GUNSMOKE puffs, AD guns' adGunBurst (your AD guns skip our sparks/backblast), the shells' port puff and trails
(vr_shells.cpp now also calls gunfx::portSmoke per casing, revolver excluded, and scales the trail by
vr_gunfx_shell_trail 1.6). Budget: each shooter's time since its last shot scales its counts (1 at 0.3 s or
slower, 0.35 at 0.1 s: the nailguns, SNG); particles come from the pool (particles::gunParticle, no allocation).
Smoke is drawn emissive grey (lit-by-flash look; Lit smoke vanished in dark maps); a warm glow sits in the cone
for 0.07 s; smoke inherits 0.8 of the shooter's velocity.

Settings (Weapons > Weapon Effects > Gunfire Particles): vr_gunfx 1, vr_gunfx_intensity 1, vr_gunfx_sparks 1,
vr_gunfx_smoke 1, vr_gunfx_port 1, vr_gunfx_backblast 1, vr_gunfx_heat 1, vr_gunfx_enemies 1,
vr_gunfx_shell_trail 1.6. Debug > Tests: three vr_gunfx_test rows, Print Gunfire Particles (vr_debug_gunfx).
Tests: python Misc/quakevr/gunfx_test.py <agent> (player 16/16, armed grunts 10/10, grunt 1/1, enforcer 1/1);
impulse 149 gives weapon id vr_test_give_wid (any id; impulse 150+id reaches 19 only; AD ids need AD's data).

Not done: idea 5 exists already (vr_tracers_enemies 1); AD soldiers' own attacks send no weaponfired (nothing for
them); an armed grunt's thunderbolt is a stream (no shot event).

### Second pass: visible smoke, impacts by material, ozone, barrel glow

- Smoke now reads as a dense quick cone (it was a faint puff 5 frames after the shot): each shot's puffs are spread
  along the cone (smokeCone: the first slow, wide and dense at the muzzle, the last fast and narrow at its tip, reach
  about speed / 6 units), start 2-3x their size and grow 35-50x a second as they slow, alpha 0.5-0.6 (was 0.3), 14
  puffs for the shotgun (was 7); gone in 0.4-0.6 s. Its grey takes the place's light once a shot (placeLight:
  R_LightPoint at the muzzle, 0.3 + 0.3 x light): a lighter cloud in a lit room, a dim one in the dark, never black,
  never glowing. Sparks 2x bigger, faster (420 u/s the shotgun's), streaked longer, shorter-lived. Port puffs and the
  backblast use the same cone (bigger). Strips: kit `scratch/gunfx/{before,after}_<class>_{lit,dark}.png` (frames 0,
  2, 5, 10, 20 after the shot; lit = e1m2's start, light 1.37; dark = e2m2's, 0.52), made by the tree's
  `scratch/shots.py`. `vr_debug_gunfx 1` also prints `gunfx light <x>` per shot.
- Impacts by material (idea 1): `Quake/vr/vr_impacts.cpp/.hpp`, ONE table `material x hit kind -> particles, decal,
  sound` (design_review.md proposal 4: the QC ladders can move onto it later; its decal and sound columns are `Own`,
  the hit's own, today). Materials: stone, dirt (debris' Natural), metal, wood, flesh; hit kinds: bullet (TE_GUNSHOT),
  nail (TE_SPIKE), super nail (TE_SUPERSPIKE), bolt (the thunderbolt's beam). A world hit's material: the texture of
  the surface nearest Quake's puff point (7 short traces, then physsound::surfaceOnSegment, debris::materialOf's name
  table), at most 16 lookups a frame. Stone: grey dust; dirt: more, browner; metal: hot sparks glancing off; wood:
  chips and pale dust; flesh: nothing new (the gore's blood mist, vr_gore_mist, already comes with every blood
  burst). Dust and chips are drawn as their colour shaded once by the place's light (dim in the dark, never black).
  Added on top of each hit's own puff, decal and tink (cl_tent.c: one hook call after each). New
  `particles::GunParticle::Chip` (the spark's shape, not hot).
- Ozone haze (idea 6): where the thunderbolt's beam (progs/bolt2.mdl, CL_ParseBeam) ends on a surface, a faint
  bluish emissive puff every 0.06 s a beam, lingering 1-1.6 s and rising.
- Barrel glow (idea 3/6): past heat 0.5 a dull red glow at the muzzle, made each frame (alpha 0.3 x heat over 0.5).
  The nailguns' heat a shot 0.07 (was 0.05: at 10 shots a second it now passes 0.35, the wisps, after about 1 s of
  fire and 0.5, the glow, after about 1.4 s).
- Settings: vr_gunfx_impacts 1, vr_gunfx_ozone 1 (Weapons > Weapon Effects > Gunfire Particles: Impacts by Material,
  Lightning Ozone Haze). Debug > Tests: Impact Where You Look (`vr_impact_test <0 bullet|1 nail|2 super nail|3 bolt>
  [count] [material]`: on what you look at, as `material` if given; prints `gunfx impact <kind> on <material>
  (<texture>) dust .. sparks .. chips .. haze ..`). Strips: kit `scratch/gunfx/after_impact_*.png`, `after_ozone_*`
  (the tree's `scratch/impshots.py`).
- Tests: gunfx_test.py 27/28 (player 16/16; armed grunts 9/10: the rocket launcher's grunt fired no judged shot in
  that run, wid 10, AI spread or a real miss: rerun `--only armed`; grunt 1/1, enforcer 1/1).
- Left for the next worker (out of budget): merge origin/next (10 commits behind) and rebuild; the covering suites
  vs baseline.tsv (collectfx, shot_shape, reload_gunshape, console nail_1: run_baseline.py needs Misc/quakevr/qvrtest.py,
  absent here, so run `Misc/release/run_test_suite.py gunfx --only ...` instead); the A/B bench (scripts ready:
  the tree's `scratch/build_ab.sh` builds origin/next and this branch ThinLTO into scratch/benchA|B, then
  `scratch/ab_bench.sh gfx ABBAABBAAB combat_48,play_gore_fight`; play_gore_fight is the super nailgun scenario);
  his VR notes of 2026-10-30 12:06-12:22: (1) a few muzzle sparks per shot for every shell/nail gun he can see
  (check the player's sparks reach the drawn muzzle and are not hidden by the flash model); (2) a turbulent frontal
  cone synced with the flash for all guns (smokeCone is the start: more forward, less rise); (3) nail pistol smoke
  from its side vents (model anchors), out and up, light; (4) rocket launcher rear exhaust, vent smoke facing the
  player, a turbulent cone out of the back, AD's enemy launcher's tube too; (5) persistent thin ribbon wisps per
  barrel after firing stops (shotgun 1, SSG and nailgun 2), following the muzzles for seconds, with fade, thickness
  and persistence settings.

### Third pass: his VR notes of 2026-10-30 (sparks, turbulent cone, nail pistol vents, rocket exhaust)

- **Sparks** (he saw none): at least 3 a barrel for every shell and nail gun however fast it fires (the super
  nailgun's budget made 2 tiny ones), 1.5-1.9 units (were 0.9-1.2), 0.12-0.28 s (were 0.05-0.16), whiter, and they
  start 1.5-4 units ahead of the muzzle, clear of the flash model and the cone's dense core (the old strips' frame 0:
  the cone's first puffs covered them). Counts a little lower ("a few": shotgun 10, SSG 8 a barrel, revolver 8).
- **Turbulent frontal cone**: the muzzle's cone now fades in over ~0.035 s (with the flash, not a blob before it:
  GunParticle::fadeIn, Custom's ramp), its puffs lead forward (the tip's start 4 units out), barely rise (only the
  slowest, at the muzzle, rise as before) and ride a swirling flow: `particles::swirl`, a divergence-free (ABC-like)
  field, ~55 units a swirl, changing over time, each puff its own phase (from its `die`), stored as one byte in
  Particle's padding (`turb`: the struct is capped at 152 bytes). The QC's rising GUNSMOKE puffs and the ports'
  rising puffs are kept. Setting: Smoke Turbulence (`vr_gunfx_turbulence 1`, 0-3; Weapons > Weapon Effects >
  Gunfire Particles).
- **Nail pistol vents**: the QC's random muzzle wisp (`QVR_PARTICLE_PRESET_SMOKE` after shots, vr_nailpistol.qc)
  is gone; light pale smoke comes out of exactly its six side slots (v_nailpistol.mdl: x 4.91..7.57, z rows 3.08,
  3.83, 4.59, the faces at y +-1.2), drifting out and up, swirling a little: 6 puffs a shot, then ~22 a second for
  0.45 s a shot (a string: longer, up to 1.5 s), following the gun. `vr_nailpistol_smoke` (1) scales it.
- **Rocket exhaust**: the backblast's cone (denser: 16 puffs, alpha 0.6) now comes out of the launcher's real back
  end, `exhaustOf`: v_rock2.mdl's tube end (-6.3, -0.04, 4.78; found from the model: its back ring), an AD
  launcher's own FireFeel exhaust, else behind the muzzle as before (Rogue's v_multi2). The exhaust then smokes
  ~0.9 s, rising (your hand: each frame following the gun; an armed grunt's held launcher, `Muzzle::gun`: at the
  shot, fading in one after another). The AD launcher in your hand gets the cone and exhaust smoke too now (its own
  burst's flames and sparks stay). `vr_debug_gunfx 1` prints `gunfx exhaust at ...`, 2 also the after-smoke.
- Not done: AD's own rocket soldier (monster_army_rocket, mon_soldier.mdl): its attack sends no weaponfired and its
  tube's place on the monster model is unknown here (no AD data on this machine): needs a hook in
  ad_army_fire_rocket and a monsterGuns entry with the tube's anchors. Persistent muzzle wisps: the next step.
- Strips (kit `scratch/gunfx/`): `c1_<class>_lit.png` (shotgun, nailgun, rocket, nail pistol from the front),
  `c1_<class>_side.png` (the gun held across the view: nail pistol, rocket launcher, shotgun; frames 0, 2, 5, 10,
  20), made by the tree's `scratch/shots.py c1 side ...`.
- Tests: gunfx_test.py 29/29 (now: sparks >= 3, the nail pistol's vents, a monster rocket's exhaust; the armed
  rocket grunt alone in vrtesthall, `vr_agrunt_test 111`: on e1m1 the wall 360 units ahead brought it close and it
  clubbed you, the failure of the second pass). Covering cases vs baseline.tsv (run_baseline.sh --only): collectfx,
  reload_gunshape, shot_shape, console nail_1, np_1 all PASS (np_1 21 -> 23 passes: the merged slide tests).
- A/B bench (ThinLTO, origin/next vs this branch, ABBAABBAAB, exclusive, medians of 5): combat_48 cpu 2.470 ->
  2.478 ms, gpu 3.361 -> 3.362, p99 11.230 -> 11.217; play_gore_fight (super nailgun) cpu 2.375 -> 2.347, gpu
  2.627 -> 2.622, p99 11.214 -> 11.234: within noise (scratch/ab_out3.txt, ab_stats.py).

### Persistent muzzle wisps; rocket vents; AD's rocket soldier (branch `agent/wisps`)

His note: after a gun stops firing, very thin, faint, blurry lines of smoke off each barrel (the shotgun's one, the super
shotgun's and the nailgun's two), following the muzzles, lasting a few seconds, everything adjustable.

- **Technique.** What shooters do: CS:GO's barrel smoke after a burst is a particle system drawn by Source's rope
  renderer (particles let go continuously at the muzzle attachment, joined in order into a camera-facing strip, the
  texture scrolling along it, faded by age); Battlefield's and Tarkov's barrel smoke are likewise trail/ribbon emitters
  (Unreal's and Frostbite's "ribbon" particles), not sprite puffs, which read as blobs, not lines. Their motion is
  advected noise: curl noise (Bridson et al., "Curl-Noise for Procedural Fluid Flow", 2007), a divergence-free field,
  curls the smoke without bunching or spreading it. Thin lines far off shimmer and break into dots; the usual cure is
  "phone-wire AA" (Persson): never thinner than a pixel or so, the alpha lowered as much instead. Chosen: a ribbon of
  points per barrel, each let go at the muzzle (30 a second, or each 2.5 units the muzzle moves), carried up by its
  warmth and about by a slow two-octave ABC flow (divergence-free; stronger as it ages: a laminar stream breaking up),
  the stream let go swaying a little (the air wavering at the muzzle: even a still gun's wisp snakes up); drawn as a
  Catmull-Rom-smoothed camera-facing strip, its alpha falling to nothing at its edges (no texture: Shade::SoftEdge),
  widening and fading with age, emerging softly out of the barrel, the newest point always at the muzzle now (the
  trail follows the gun at frame rate). One draw for all of them; points in fixed rings, the vertices in a reused
  scratch buffer: nothing allocated a frame.
- **Code.** `Quake/vr/vr_ribbons.cpp/.hpp`: the reusable renderer (points with half width and colour -> triangles;
  `sceneView(minPixels)`, `append`, `draw`). `Quake/vr/vr_muzzlewisps.cpp/.hpp`: the wisps (drawn in the scene's
  translucent pass, vr_particles.cpp, after the loot beams; moved on once a frame there). vr_gunfx.cpp `barrelWisps`:
  each gun's barrels (`barrelEnds`: the bores' front rings on v_shot2.mdl, y -1.53 / 1.47, and v_nail.mdl, y -3.67 /
  3.77, found from the models; other two-barrelled guns either side of the muzzle) and charge: a shot's share of full
  smoke (shotguns 1, revolver and flare gun 0.7, nail pistol 0.2, nailgun 0.12, super nailgun 0.08: a burst of the
  nailguns fills them). Wisps: the shotguns, revolver, flare gun, nail pistol, nailgun (2), super nailgun (1, its
  turning barrels' middle); none for launchers, energy guns, melee. Your guns only.
- **Settings** (Weapons > Weapon Effects > Gunfire Particles): Muzzle Wisps `vr_gunfx_wisps 1`, Wisp Delay
  `vr_gunfx_wisp_delay 0.25` s, Wisp Persistence `vr_gunfx_wisp_time 4` s (fewer shots: down to 0.4 of it), Wisp
  Fade Time `vr_gunfx_wisp_fade 2.2` s (the trail's length), Wisp Thickness `vr_gunfx_wisp_width 0.6` cm, Wisp Spread
  `vr_gunfx_wisp_spread 3`x, Wisp Amount `vr_gunfx_wisp_amount 1`x, Wisp Rise `vr_gunfx_wisp_rise 22` cm/s; Smoke
  Turbulence (`vr_gunfx_turbulence`) curls them too; `vr_gunfx_wisp_pixels 1.5` (no row) the thinnest drawn.
  `vr_debug_gunfx 1` prints `gunfx wisps start hand H barrels B after S s ...` and `gunfx wisps faded ... points P
  drawn V`; 2 also `gunfx wisps stop` when a shot cuts them.
- **Rocket vents** (his note 4): v_rock2.mdl's two slots in the back face of its head (the shoulder at x 33.3 round the
  tube; slots y +-3.4..4.7, z 6.6..7.5: they face you) smoke lightly: 7 puffs at the shot, then one in two of the
  exhaust's after-smoke puffs, back out at you and up, curling (`gunfx light .. vent N` counts them for a rocket too).
- **AD's rocket soldier**: QC `ad_army_fire_rocket` now sends `weaponfired(self, QVR_FIRED_ROCKET)` (4, builtins.qc;
  `// AD support`), the server passing hand 4 through; the client fires the monster's own gun as a rocket launcher
  (WID_ROCKET_LAUNCHER: sparks, cone, backblast out of the gun's back, the exhaust smoking, its flash and muzzle smoke).
  Its gun: a monsterGuns entry for AD's `mon_soldier.mdl` (read from AD's pak0 in memory, nothing extracted:
  soldier.mdl's mesh, the gun vertices 463..548 numbered alike, frame 0 placed 0.07-0.12 further along x; the anchors
  are those vertices' places in AD's file; in its fire frame shoot6 the gun is 29.1 units long, pointing up 16 degrees).
  Not run with AD's data (no AD in this agent's game folder): `vr_gunfx_monster_test 1` (Debug > Tests: Nearest Monster
  Fires a Rocket Tube) checks the same path on a grunt (the same mesh): backblast 16, exhaust 6, out of the gun's back.
- **Strips** (kit `scratch/gunfx/`): `wisps_<shotgun|ssg|nailgun>_<lit|dark>.png`, the gun held across the view, 0.5,
  1, 2, 3 s after the burst (lit e1m2's start, dark e2m2's); `rocket_vents_lit.png` (frames 3, 13, 33, 63 after a
  shot; the labels say 0.5-3 s, wrongly). Made by the tree's `scratch/strips.sh` (wispshots.sh, crop.py).
- **Tests**: gunfx_test.py `--only wisps` 7/7 (each gun's wisps start once, after its last shot, 0.26 s later, one a
  barrel: 1, 2, 2, 1, 1; drawn and faded; none for the rocket launcher), `--only tube` 1/1, `--only player` 16/16
  (now also your rocket launcher's vents). Covering cases vs baseline.tsv: collectfx, shot_shape, nail_1, np_1 PASS
  (np_1 21 -> 23 as before), reload_gunshape FAIL 23/1 in the 5-lane run ("lying shotgun dropped onto: the loose round
  slides in", 0 frames drawn going in), PASS 24/0 alone: flaky.
- **A/B bench** (ThinLTO, origin/next vs this branch, ABBAABBAAB, exclusive, medians of 5; the tree's
  `scratch/ab_wisps.sh`, `ab_out.txt`): combat_48 cpu 2.544 -> 2.525 ms, gpu 3D 3.200 -> 3.220 (means);
  play_gore_fight (super nailgun) cpu 2.576 -> 2.439, gpu 2.471 -> 2.476: within noise.

### Gunfire VFX settings; the wisps' creases (branch `agent/gunfxopts`)

His asks: more options for the new gunfire effects (wisps' opacity; sparks' amount, size, speed; muzzle smoke's size,
speed, directionality, thickness, opacity), per-weapon multipliers, and the wisps' creases on a quick swing.

- **Settings** (Weapons > Weapon Effects > Gunfire Particles; all 1 by default: today's look, by construction: the
  same counts, sizes and random draws): Spark Amount `vr_gunfx_spark_amount` (0-4), Spark Size `vr_gunfx_spark_size`,
  Spark Speed `vr_gunfx_spark_speed` (0.25-4); Smoke Amount `vr_gunfx_smoke_amount` (the muzzle cone's puffs: its
  thickness, 0-4), Smoke Size `vr_gunfx_smoke_size` (size and growth), Smoke Speed `vr_gunfx_smoke_speed` (its reach),
  Smoke Directionality `vr_gunfx_smoke_direction` (1 the forward cone, 0 every way: each puff's way mixed from a random
  one towards the cone's), Smoke Opacity `vr_gunfx_smoke_alpha` (0-3); Wisp Max Opacity `vr_gunfx_wisp_opacity` (a cap,
  0.02-1; Wisp Amount scales them). The muzzle's sparks and frontal cone only (not the ports, backblast, vents, impacts,
  the energy guns' arcs). Read once a shot in emit (vr_gunfx.cpp `tuneFor`), nothing allocated.
- **Per weapon** (Weapon Offsets > Effects, after Stream Rumble; vr_weapons.inc keys, 1 by default): Gunfire Spark
  Amount `fx_gfx_sparks`, Spark Size `fx_gfx_spark_size`, Smoke Amount `fx_gfx_smoke`, Smoke Size `fx_gfx_smoke_size`,
  Muzzle Wisp Opacity `fx_gfx_wisps` (0: no wisps at all), each times the global one. Your shots use the hand's slot; a
  monster's the slot of its gun's model, else of its weapon id's view model (`slotOf`).
- **Creases**: a quick swing folds the wisp's ribbon over itself (the hairpins at each turn, the legs on top of each
  other just after), and the overlapping fragments blended twice: denser knots and lines. Now each pixel is drawn
  once: `gfx::State::once` (vr_gfx_gl.cpp drawVertices) marks the drawn pixels in the stencil's top bit 0x80 (taken from
  the particles' reverse-order marks: `underStencilBits` 0x7c, 31 generations) and draws Shade::SoftEdge in four bands
  of its edge falloff, the cores first, so a pixel takes about its strongest fragment (a plain stencil let the first
  fragment win: an old leg's faint fringe cut a gap through a new leg's core); the same vertices again (colour masked)
  clear the marks. One upload, 5 rasterisations of the thin strips, only while wisps show. `vr_debug_ribbon_overlap 1`
  (Debug > Tests: Wisps Overlap Again) blends them twice again, to compare. Eyeshots (kit `scratch/gunfx/`, the mock
  hand swung +-30 degrees twice in 33 frames): `crease_swing_0s_before_after.png`, `crease_swing_0.55s_before_after.png`,
  zooms `crease_hairpin_zoom_before_after.png` (the knot at the turn gone) and `crease_swingend_zoom_before_after.png`
  (the doubled legs one density, their soft edges kept). Made by the tree's `scratch/swing2.sh`, `compose.sh`
  (`crop.py`: PNG crop/diff without PIL).
- **Defaults**: eyeshots of a shotgun's vr_gunfx_test shot and a still gun's wisp, origin/next vs this branch: mean
  pixel difference 0.21 / 0.13, the same as two runs of one build (0.21 / 0.16: the particles' random draws differ
  run to run); `defaults_shot_before_after.png`.
- **Tests**: gunfx_test.py 43/43 (new `--only slots` 6/6: the shotgun's slot at 0 no sparks or smoke, at 2 twice the
  counts (20/28 vs 10/14), the global 0.5 times the slot's 2 as at 1, your fired shotgun at 0 no sparks, smoke, wisps).
- **Not done**: the A/B bench (his game was running: exclusive runs wait); the ribbon pass costs 4 more rasterisations
  of the wisps' thin strips while they show (none in combat_48 unless you fire), the particles' reverse-order pass
  clears its stencil marks every 31 draws instead of 63.

### The thunderbolt's wisps and settings; wisps on guns that leave the hand (branch `agent/lgfx`)

His words: the lightning gun should have a smoke wisp too and read the spark/smoke per-weapon tweaks; then the wisps
should persist on thrown, dropped and holstered weapons.

- **Thunderbolt wisps** (vr_gunfx.cpp `wispLook`): one barrel, charge 0.05 a shot (it fires every 0.1 s: 2 s of beam
  fill them), opacity 0.2 (the shotgun's 0.32), tinted blue-grey (0.82, 0.9, 1 times the grey: `muzzlewisps::Gun::tint`,
  taken when they start). Every wisp setting applies (vr_gunfx_wisp_*, slot 7's Muzzle Wisp Opacity `fx_gfx_wisps`).
- **Its arcs and haze read the settings**: the energy guns' arcs (thunderbolt, laser cannon, enforcer rifle, AD plasma)
  now take Spark Amount (count) and Spark Size (`particles::gunArcs`: Preset::ElectricSparks with a size), global times
  the slot's; the thunderbolt's ozone haze where the beam plays (vr_impacts.cpp) takes Smoke Amount (puffs) and Smoke
  Size (size and growth), global times slot 7's (`gunfx::boltSmoke`). The QC's GUNSMOKE puff at its muzzle each shot
  (weapons.qc W_FireLightning, like every gun's) is not ours and untouched. Menu helps say so. `vr_debug_gunfx 1`:
  `gunfx light .. size <spark> <smoke>` per shot, `gunfx impact .. haze N size S`.
- **Loose guns** (vr_muzzlewisps.cpp `release`/`looseFrame`, vr_gunfx.cpp `LooseGun`): when a hand's gun changes or goes
  (handFrame's slot change) while it still holds smoke (in the delay too), its trails, smoke and time left move to one of
  4 loose sources (the least smoke given up); the hand starts afresh. Within 0.5 s the gun is found, the nearest to its
  muzzle's last place within 160 units: a client entity drawn with the same weapon slot's model (QC's thrown_weapon:
  dropped or thrown) or a holster's gun (`view::holsterGun`); then followed by entity number or holster index, its
  muzzle found as in the hand (`muzzleOf`: the slot's Muzzle Anchor and offset on that entity, `wispGun`: the barrels).
  Not drawn (picked up, gone): its smoke stops, what it let go fades. `vr_debug_gunfx 1`: `gunfx wisps loose hand H as
  K`, `gunfx wisps found prop|holster N after T s D units off`; 2 also `gunfx wisps loose K at <muzzle> on <origin>`.
  Not done: a gun taken back up keeps no smoke (the loose one stops); a hand gun invisible for one frame while held
  would hand its smoke to a loose source that finds nothing (not seen in the tests).
- **Tests**: gunfx_test.py `--only wisps` 8/8 (the thunderbolt's: 5 shots, 1 barrel, 0.26 s after, drawn, faded),
  `--only slots` 10/10 (new: slot 7 at 1, 0, 2: arcs 5/0/10, sizes 1/2, haze 1/0/2 puffs, sizes 1/2; the case moved to
  vrtesthall: a wall ahead for vr_impact_test 3), `--only loose` 4/4 (new: a shotgun dropped, a super shotgun thrown
  (vr_mock_swing), a shotgun holstered at the right shoulder: released, found 0.00 s later 11-22 units off, followed
  97-127 frames within 48 units of the gun's origin, the props falling 36-55 units, faded, drawn).

## Nail pistol: moving slide, slide lock, auto re-rack (branch `agent/npslide`)

His words: a slide that moves with each shot, locks open on an empty magazine, closes by itself with a sound when a
loaded magazine goes in (no manual racking), travel and time configurable.

- **Before:** QC set the weapon frame 1 for vr_nailpistol_slide_time (0.07 s) after a shot and while empty; the
  renderer's frame lerp ran it home; frame 1 was the slide 1.0 back. Barely visible, fixed travel, silent re-close.
- **Server (QC vr_nailpistol.qc):** a shot sends the slide back (frame 1) for the cycle's first third
  (`VR_NailPistol_SlideBack`: vr_nailpistol_slide_cycle / 3, one frame at least); empty (`VR_NailPistol_Empty`, as
  before: clip out of rounds, or immersive with no magazine in) holds it there. `VR_NailPistol_HandFrame` remembers
  each hand's slide (.vr_np_lock: home/locked); locked then loaded: `VR_NailPistol_Rack` plays
  vr/nailpistol_rack.wav (new, `make_nailpistol_sounds.py rack()`: the slide stop's tick, a short scrape, the slam home
  and a body thunk, 0.26 s) on the hand's channel. Any reload mode loads it (a pouch magazine, a refill).
- **Client (Quake/vr/vr_nailslide.cpp, new):** per hand, the slide's place 0..1 stepped on vr_gametime from the
  server's frame: an edge to back sends it all the way back over cycle/3 (even if the server's moment ends sooner),
  then home over the other two thirds; held back while frame 1 (locked); a lock let go runs home the same.
  `VR_NailSlidePose` (r_alias.c, after the crossbow's pose override) draws any v_nailpistol.mdl as frames 0 and 1
  blended: the hands' guns by their slide's place, any other (an armed grunt's, holstered, a viewthing) by its own
  frame 1 share, both times vr_nailpistol_slide_travel / the model's frame-1 travel (measured from the model once).
- **The model:** progs/v_nailpistol.mdl's frame 1 moved from 1.0 to 3.0 back (the most travel; no Blender: the slide's
  84 vertices' offset scaled and the MDL re-quantised as mdlgen.write_mdl does, frame 0 within a quantum; measured
  2.986). build_nailpistol.py's `travel` is 3.0 to match on the next export.
- **Armed grunts:** `VR_AG_ShootProp` sends its prop's slide back (`VR_NailPistol_PropShot`); `VR_AG_PropThink` runs
  `VR_NailPistol_PropFrame`: back after a shot, locked while its clip is out, the rack on a refill.
- **Settings** (Combat > Weapon Damage, under Nail Pistol Magazine): Nail Pistol Slide Travel
  `vr_nailpistol_slide_travel` 1.5 (model units, 0..3; the gun is ~13 long), Nail Pistol Slide Cycle
  `vr_nailpistol_slide_cycle` 0.14 s. vr_nailpistol_slide_time retired (retiredCvars; no config step: new names).
- **Debug:** `vr_nailslide_debug 1` (Debug > Tests > Nail Pistol > Slide: Print) prints every frame it moves.
- **Tests:** `vr_np_test 1` (23 checks, was 21): re-racked once by the new magazine and never before; the armed
  grunt's prop frame 1 after its shot. Console trace (vr_nailslide_debug 1, fast mode at 72 Hz, defaults): a shot
  0 -> 1 in 4 frames (0.298 a frame), home in 7 (0.149 a frame); locked through the 99 walking frames at 1.000; the
  re-rack line, then home in 7 frames. Eyeshots (qvr-kit/scratch/npslide/): `viewthing_closed_open_travel3.png` closed,
  locked at 1.5, at 3.0; `closed_open_L.png` in hand.
## Faster map loads, round 5: the file lookups that find nothing (branch `agent/load5`)

Profile (no-LTO, warm caches, `vr_startup_times`; hip2m5 is the kit base's map with the most monster kinds, 11):
vrtesthall 1123 ms (spawn 619, alias models 662 of 363: normal maps 278, skins 178, md5/md3 lookups 126 of which md5
hands/body loaded 123, so the failed md5/md3 lookups ~3; limbs 196, view models 164, smooth rigs 48, image lookups that
found nothing 68 ms / 2304); e1m1 881 (misses 16 / 2036, limbs 22, rigs 8); vrstart 1427 (misses 72, limbs 193, rigs
45); hip2m5 1096 (misses 19 / 2460, limbs 84, rigs 26). Sub-timed (temporary timers): of vrtesthall's load, 12,900
names were looked up in every search path and found nowhere (~3 us each), and 810 lookups of an owned pack's files
(owned/mg3/progs/rknight.mdl_0.png, its _norm, _bump...) cost ~0.1 ms each, 80 ms: each listed the folder's paks and
read every pak's directory.

- **Owned packs' files** (vr_gamedir.cpp, VR_OwnedFile): the folder's paks read once a load window into a name index
  (`ownedIndexes`, mem::Cache, GameDirChange), on the main thread inside the file system cache's windows
  (VR_FileCacheWindow, new at each enable and each full forget: a pak added meanwhile is seen at the next load); the
  loose files by the window's listings (VR_FileCacheHas). Other threads and lookups outside the windows as before. A
  temporary check ran both ways on every lookup of a vrtesthall load: 641 compared, 0 differed. COM_FindFile notes an
  owned name not found as a miss too.
- **An image's five lookups at once** (COM_StemNowhere, image.c): Image_LoadImage asks png, tga, jpg, pcx and lmp in
  every search path; now, inside the windows, a name that no pak has with any extension (a stem table beside each pak's
  name hash, COM_PackIndex) and no listed loose folder has (Listing::stems) makes none of them (model-keep's recorded
  lookups still noted). Checked by running the five lookups anyway each time it said "nowhere": vrtesthall 2077, e1m1
  1874, hip2m5 2284, none found a file.
- **Misses kept when an unrelated file is written** (vr_fscache.cpp): a file written forgot every miss known; now only
  the names its path ends with (`forgetMisses`, after folding the path: case, separators, "." levels; a name spelt
  oddly is forgotten anyway). Sys_mkdir forgot every listing on each call, a folder made or not (every file written
  asks for its folders: six times in a vrtesthall load, the server's 7700 misses looked up again by the client); now
  only when it made one. `vr_fscache_test`: 22 passed (new: misses kept and forgotten by name, on another thread too).
- **Limb models and smooth rigs, built only for what the map uses** (brief item 4): already so. Limbs are made for the
  kinds the map's monsters are and those its triggers (probe pass 1) and, with vr_limbs_prebuild 2, its dispensers can
  make (vrtesthall and vrstart place no monsters: their 16 kinds are the dispensers'/spawners'), and a kind first seen
  later is built on the pool and precached one a frame (spawnFrame); smooth rigs are made for the precached alias
  models with a rig (the same 16-18 kinds and the player). Nothing to cut without moving the author's chosen load-time
  work (vr_limbs_prebuild 2) into play.
- **Not done** (measured, left): a disk cache of the limb models and smooth rigs (brief item 3): the limbs' files are
  built on the pool while the rigs warm (22 ms of vrtesthall's 137 ms limbs, sub-timed; the other 115 their 170
  precaches on the main thread: skins, normal maps, draw lists, which a cached .mdl doesn't save), and the smooth rigs
  are ~2-5 ms a kind (18 kinds, 42 ms): a cache would save at most ~60 ms of vrtesthall/vrstart, ~30 of hip2m5, ~10
  of e1m1; with rigData's outputs (unfit masks, rebuilt keys, rest rows, fit statistics) and its inputs (the model's
  poses, the derived ragdoll rig, three settings) it wasn't in this round's budget. Running each kind's rigData on the
  pool at once (as the limbs' builds) is the cheaper next step. Item 5 (uploads off the main thread) skipped. The
  normal maps of authored maps with heights need islands made each load (76 on vrtesthall, 24 ms).
- **Checks**: eyeshots of vrtesthall, e1m3, e2m1, hip2m5 (dither and particles off), before vs after: 1522 pixels
  differ, the before build twice: 2195 (the same regions). Covering cases (`run_baseline.sh --only` limb, ragdoll,
  agrunt 1-6, animsmooth, selfchecks, stats_levelchange): 0 of 16 changed against baseline.tsv.
- **Times** (interleaved ABBA, 5 rounds, exclusive, no-LTO, medians; cold: the session's first load, warm: the same
  map again): vrtesthall cold 1173 -> 1147 ms (precache 164 -> 131, limbs 192 -> 148: their precaches' lookups),
  vrstart 1536 -> 1440 (spawn 750 -> 681, limbs 213 -> 154), e1m1 916 -> 919 and hip2m5 1074 -> 1103 (within the
  runs' +-100 ms spread; hip2m5's per-round differences -63, -11, -633, -38, +79). "Image files looked for, none found"
  70 -> 6 ms (vrtesthall), 77 -> 6 (vrstart), 16 -> 5 (e1m1), 20 -> 6 (hip2m5). Warm loads unchanged (144-293 ms: the
  models are kept). Scripts: scratch/load5_ab.sh, load5_abstats.py (not committed).

## Design review 5 (b): one throw, the haptic presets (branch `agent/heldthing`)

The review's proposal 5 (scratch/refactor/design_review.md), its first sub-step. His decisions: the flashlight and the
physgun get the shared throw smoothing; the haptic presets keep today's exact values (no feel change).

- **One throw (vr_throw.hpp):** `throwing::release(handMask, at, centre)`: both hands' bits, estimateBothAt; one,
  estimateAt (the client's one-hand throw and held::bothHandsThrow go through it). `throwing::releaseAt(hand)`: the
  grip's own release time if it is this release, else the newest sample's (was inline in vr_client.cpp; the lamp uses
  it too). `throwing::Kind` and `kindCap(kind, vel)`: the per-kind top speeds in one table (Hand: none here, its
  weight's; Flashlight 3 m/s; Physgun 1000 u/s), replacing the two inline caps.
- **The flashlight** let go flies with the hand's smoothed throw (`release(1 << hand, releaseAt(hand))`: the peak in
  vr_throw_window, as any prop) instead of the hand's raw velocity of that frame; the cap unchanged (3 m/s).
- **The physgun** samples its held point each drag frame into a point history (`throwing::pointSample`, on the drag's
  own clock: dragFrame's dt summed) and lets go with `pointEstimate` (the hands' releasePeak: the peak in
  vr_throw_window fitted over vr_throw_peak_span, not pitched, no flick) instead of its own 12/s EMA; cap unchanged.
  A swing let go just after it stops now keeps the swing (as a hand's throw does); one held still past the window
  drops as before.
- **Haptic presets:** QC/vr_haptics.qc (new, after vr_util.qc): 25 named pulses `VR_Haptic_*(hand)` and
  `VR_Haptic_Both(preset)` (off hand first, as the pairs were) for the hand-held things: pickups and holsters (the
  holster's fading triple, written twice before, in items.qc and vr_wpnholster.qc), carrying, the lamp, grenades in
  the hand, the force grab, foe grab and its throws/neck snaps. 51 literal calls in 9 files replaced, each with the
  very values it had. Combat, melee, burning, juice and the rest keep their calls (not the held thing's; melee QC
  untouched). C++: `haptics::meet()` (both hands 0.04 s, 120 Hz, 0.35), the pulse vr_held.cpp and vr_view_hands.cpp
  each wrote out.
- **Kept out (behaviour changes the review lists that he did not decide):** two-handed ragdoll throws still get the
  one-hand estimate; `vr_2h_throw_velocity_mult` (QC: a two-handed weapon's speed x1.3) and `vr_throw_2h_strength`
  (C++: a two-handed prop's mass / 2) both kept, they are not one setting (merging them changes either throw); the foe
  grab's `twoHandsOf` is a gesture measure (turn, way), not a release velocity, and stays.
- **Not done (next sub-steps):** (a) the hand slot (`.vr_hold_kind`/`.vr_hold_ent` per hand, VR_Hand_Take/Release,
  VR_IsHandEmpty on the slot, the per-frame flag-derivation check first), (c) the grab query.
- **Verified (against scratch/refactor/baseline.tsv, low-impact runs while his game ran):** the same verdicts and
  counts in held_over_props 6/0, pickup_mag 3/0, throw_spin 1/0, reload_contact 23/0, reload_eject 28/0,
  reload_front 34/0, grip_gap, grip_state, mapgun_pickup 16/16, empty_melee 2/0, melee_phase 7/0 (one earlier run
  6/1, dummy_fast_on's ease-out 32 frames against 27: it varies run to run), weapon_catch 3/2 (its known 2),
  flash_grab PASS, ragdoll grab 19/0, ragdoll climb 24/4 (its known 4), prop_climb 12/0, judo 12/4 (its known 4),
  walltorch_grab 117/120 taken (3 "knocked off first: a slap"); eval.sh 163/171 as the baseline (10 takes' numbers
  differ, the baseline's own runs 9-11). The physgun ragdoll test PASS, let go still: 0 u/s; let go while the stick
  moves it at 256 u/s: 256 u/s. Rerun once his game closed: reload_gunshape 24/0, reload_magcollide 11/0,
  reload_pouchgren 15/0, reload_worldparts 6/0, reload 112/3 (its known 3): 0 of 5 changed. No A/B bench: no
  per-frame code changed but the physgun's drag (a sample a frame while dragging).
- **Checklist:** throw the flashlight (a flick and a lob): it flies as a thrown prop would, never over 3 m/s, then
  home; physgun swing a crate and let go at the end of the swing: it keeps the swing (before: an eased 83 ms average,
  slower); every grab, pickup, holster, lamp and foe-grab buzz feels as before.

## Design review 5 (a): the hand slot (branch `agent/heldthing`)

- **The slot (QC/vr_fields.qc):** the thing a hand holds besides its weapon: `.mainhand_held`/`.offhand_held` (kept: the
  engine and saves read them) and its kind, `.vr_hold_kind_main`/`.vr_hold_kind_off` (VR_HOLD_NONE, PROP, RAGDOLL,
  TORCH, ROUND, GRENADE). Read with `VR_Hand_Thing(e, hand)` / `VR_Hand_Kind(e, hand)` (vr_util.qc, macros).
- **Its only writers (vr_carry.qc):** `VR_Hand_Take(p, hand, thing)` (the kind from `VR_Hand_KindOf`),
  `VR_Hand_Release(p, hand, why)`, `VR_Hand_ReleaseThing(p, thing, why)` (whichever hand: VR_Carry_Forget and
  VR_Grenade_Detach each wrote that out). VR_Carry_SetHeld is gone: its 40 calls and the tests' 8 direct writes moved
  to them (`why` a word per file: letgo, reload, grenade, throw, revolver, pickup, test). `vr_debug_carry 2` prints
  each take and let-go (`handslot: off hand takes item_shells (kind 1)`).
- **The per-frame check (VR_Hand_Frame, PlayerPostThink):** each hand's kind against its thing's; a kind that
  disagrees (a write past the writers, a save from before the slot) takes its thing's, with a `handslot: mismatch`
  line, always printed. None in any covering case below.
- **VRIsHandEmpty** reads the slot (weaponless and no thing: the very test it was). `VR_Hand_CanTake(p, hand)`
  (vr_wpnforcegrab.qc): empty and not locked on to a pull, the conjunction the carry grab, the ragdoll grab, the box
  touch and the foe grab's hand-over each wrote out.
- **Kept out (behaviour changes, his call):** folding the other busy flags (`*_fgheldfoe`, `vr_foegrab_hands`,
  `vr_mag_rehold`, `vr_ssg_held`, `vr_gren_held`, `vr_stick_tug`, `*_wtgrip`) into the slot as kinds (Foe, Physgun,
  Flashlight, a gripped wall torch) would make VRIsHandEmpty false for them at every caller: today each caller adds
  its own few, so "can this hand grab?" would change answers in places (one answer for all is the review's aim; which
  callers should see which flags is his decision). The weapon stays in `.weapon`/`.weapon2` (about 140 writers,
  monsters' too); the C++ mirror of the kind is not added (no engine reader needs it yet: the engine's held readers
  compare entity numbers). Not done: (c) the grab query.
- **Verified (scratch/refactor/baseline.tsv):** 23 covering cases (suite, scripts: held_over_props 6/0, pickup_mag
  3/0, throw_spin 1/0, weapon_catch, flash_grab, grip_gap, grip_state, mapgun_pickup 17/0, empty_melee 2/0, reload
  112/3 and all 7 reload_* as the baseline, ragdoll_grab 19/0 on its rerun (flaky: 18/1 once), ragdoll_climb 24/4,
  prop_climb 12/0, judo 12/4, walltorch_grab): the same but melee_phase, whose grunt_fast_on fails now and then in
  both trees (without (a): PASS then FAIL; with: FAIL, FAIL, PASS). eval.sh 163/171, 11 differ (the baseline's own
  runs 9-11). No `handslot: mismatch` line in any log. A/B bench (one LTO exe, the progs before/after, interleaved,
  scratch/dr5_ab.sh): combat_48 ABBAABBA +4.1% (sd 0.23-0.25 ms, one 3.1-3.2 ms outlier each side), then BAABBAAB
  +0.8% (2.484 -> 2.503 ms, sd 0.03-0.04); idle_e1m1 -8.9% (noise): within noise (the check is two field reads a hand
  a frame).
- **Checklist:** pick up a box, a gib, a round, a grenade and a wall torch in either hand, swap and throw them, and
  load a save made while holding one: each is held and let go as before, no `handslot: mismatch` line in the console.
## Design review 5 (c): the grab query (branch `agent/grabquery`)

- **One vocabulary and one order (Quake/vr/vr_grab.hpp):** `grab::Kind`, numbered in precedence order (smaller takes
  the grip): the client's ZoneDrag, ChainsawCord, Lamp; the server's Climb, Foe (before the move); the touch stage's
  Ragdoll, Prop, Round, ArmedWeapon, Weapon, Box; W_Frame's SecondHand, Torch, GrenadePouch, AmmoPouch, Holster,
  ForceCatch. `grab::Candidate {kind, ent, distance (units), priority()}`. The header lists each stage's search and how
  it swallows or hides the grip for the stages after it: today's order, written down in one place.
- **The touch stage (vr_physics.cpp):** `physics::handOn`'s if-chain is a table of providers (`touchProviders`: claims +
  on), tried in order; the first that claims a thing decides (a Weapon with no jointed fist passes to the Box test, as
  before). `grab::touch(target, player, hand)` returns the Candidate (the armed weapon's fist gap, a weapon's distance
  from its handle); handOn is its bool.
- **W_Frame's searches (QC/vr_grab.qc, VR_GRAB_* = the same numbers):** `VR_Grab_Query(hand, upTo)` asks the providers in
  order: `VR_WallTorch_GrabCandidate`, `VR_HandGrenade_GrabCandidate`, `VR_Reload_GrabCandidate` (side-effect free;
  `vr_grab_ent`, `vr_grab_dist`). Each site takes when `VR_Grab_Answers(hand, itsKind)`. The pouches' shared "empty to
  take" test is one function (`VR_HandGrenade_PouchHandEmpty`, written out twice before).
- **Debug:** `vr_debug_grab` (Debug > Log Grab Query, default 0): `grabquery: main hand, torch (light_torch_small_walltorch,
  3.2 u)` per W_Frame take.
- **Not folded (behaviour or timing would change):** climbing, the foe grab and the lamp stay where they run (before the
  move, or on the client: moving them to one call point changes the hand positions they see); they are stages in the
  table, not providers called from one place. SecondHand, the holster draw (inside DoHandImpl) and the force catch (the
  flying thing's think) likewise. The other busy flags are not touched (the fold is the next step).
- **Old against new (a check build, not committed):** the old handOn kept beside the touch query on every call, and each
  W_Frame site's old test beside the query's answer, printed with vr_debug_grab; the console lines starting "grab"
  collected over the covering cases: 0 differ: the touch query against the old handOn over at least 299,993 calls (28,895 on: counted at powers of two per run, so more), every W_Frame take (79 torch, 7 grenade pouch, 63 ammo pouch) answered alike by the query, 0 MISMATCH lines. (Calling both handOn versions in one frame twice notes the fist's probe,
  so the check build's own behaviour is not the reference; the bool answers are.)
- **Covering cases (scratch/refactor/baseline.tsv):** the check build's run (the new answers taking): as the baseline but melee_phase (flaky, (a)'s note), ragdoll_grab 18/1 (its known flake), grip_state (below), reload_front 33/1, reload_gunshape 23/1, reload 111/4 (one failure each, low-impact runs; the clean build's rerun was still going when this was written: scratch/grabcheck_final.txt)
- **grip_state fails on origin/next too** (off hand: swap_gripped, shotgun_pump2, shotgun_anywhere "not held by a blade",
  "the grips reset", "taken anywhere"; the same 6 lines with this branch stashed): baseline.tsv's PASS is older. Not this
  branch's.
- **Verified after merging origin/next (7683f75c3), the clean build:** the 36 covering cases (scratch/grabcheck_run.sh:
  carry, held_over_props, weapon_catch, flash_grab, grip_gap, grip_state, ragdoll_grab/climb, prop_climb, judo,
  walltorch_grab, every reload_*, the tug/disarm agrunt cases): 0 of 36 changed against baseline.tsv (ragdoll_grab 18/1,
  its known flake; grip_state passes again on this next). Outside the baseline: foegrab/onehand_drag ALL PASS,
  toolgun_physgun_ragdoll PASS. eval.sh: full 163/171, 9 differ from the baseline (its own spread: (a) saw 9-11), neck
  snaps 6/6, shoulder charges 22/22, 0 false.
- **A/B bench (scratch/grab_ab.sh, ThinLTO, A = 7683f75c3, B = this branch, ABBAABBA, run 1's warm-up gave no result):**
  combat_48 cpu A 2.425/2.399/2.469 ms (2.431), B 2.475/2.421/2.433 (2.443, +0.5%) and one 2.834 outlier (p99 13.8: a
  hitch); idle_e1m1 A 0.918/0.785/0.956, B 0.991/0.775/0.788: within noise. The per-frame change: W_Frame asks the
  query at the torch site each frame (the torch provider stops at its grip test, as the old search did).
- **Checklist:** grab a torch off its wall (near it, and closing the fist on the way), a grenade and a round from the
  pouches, a box, a gun on the floor and in the air, an armed grunt's weapon and a ragdoll's limb: each taken as before.

## Design review 5: the busy flags folded into the hand slot (branch `agent/grabquery`; his approved fold)

- **The busy kinds (QC/vr_fields.qc):** after the carried kinds, `VR_HOLD_FOE` (an enemy held), `FLASHLIGHT`,
  `SSGGRIP` (the second hand on a super shotgun), `MAGREHOLD` (still gripping the magazine it just seated), `TUG` (a
  force grab's tug on a stuck axe; the axe in the new `.vr_hold_ent_main/off`). A busy hand carries nothing
  (`.mainhand_held` stays world: the engine's carried-thing readers see nothing new). Writers (vr_carry.qc):
  `VR_Hand_Busy(p, hand, kind, ent, why)` / `VR_Hand_Unbusy(p, hand, kind, why)` (only that kind); `VR_Hand_Release`
  leaves a busy hand busy; `VR_Hand_Take` over a busy kind is told (`handslot: clash`, always; the foe grab's hand-over
  takes the ragdoll over FOE, expected). The engine's two (`.vr_foegrab_hands`, `.vrbits0`'s `*_BUSY`) are read into
  the slot at the top of PlayerPreThink (`VR_Hand_Derive`), before anything reads them; the frame's check
  (`VR_Hand_Check`) ends a stale TUG/SSGGRIP (the axe no longer tugged by that hand, the gun no longer a super shotgun)
  with a `handslot: stale` line.
- **Flags gone:** `vr_mag_rehold`, `vr_ssg_held` (now kinds), `mainhand_wtgrip`/`offhand_wtgrip` with the torch's
  `wt_gripper`/`wt_grip_*`, `VR_WallTorch_Gripped/SetGripped/Ungrip` and the pull-out branch: no writer had set the
  grip on its wall since the immediate take (dead state); `vr_walltorch_pull` is no longer read (its tunable slider in
  vr_tunables_rows.inc stays: removing it is his call). Readers now on the slot: `VR_FoeGrab_HandHolds`,
  `VR_Melee_HoldsTorch` (pure moves: same answers), the force grab's flashlight test, `VR_Reload_SsgHit`, the magazine
  pull's rehold. An old save's line for a removed field is skipped ("is not a field", developer only).
- **Not folded:** `*_fgheldfoe` (a time: the force grab's pause after holding an enemy, not a busy state; its writer
  reads the slot's FOE), `vr_gren_held` (on the grenade: its last tick's carry, an edge), `*_fglocked` (the force
  grab's lock: the engine reads it, climb and the stats; VR_Hand_CanTake still asks it), the physgun (no QC state: the
  toolgun is the hand's weapon, the drag the engine's), the C++ conjunction in vr_box3d_ragparts.cpp:498 (no C++
  mirror of the kind yet).
- **Callers whose answer changes:** VRIsHandEmpty (about 70 callers): none (a busy hand is still empty: it makes a fist,
  grips a hotspot, steadies a gun; the super shotgun's own `holding` test needs that). `VR_Hand_CanTake` now refuses a
  busy hand: the carry grab, the box's touch, a box's second hand (it already refused the flashlight), the ragdoll
  grab: a hand holding an enemy, the flashlight, the super shotgun's fore-end, a just-seated magazine or an axe's tug
  takes no box or limb now (before: it could). The foe grab's hand-over asks `VR_Hand_CanTakeFrom(.., VR_HOLD_FOE)`:
  unchanged. Hand-written conjunctions now `VR_Hand_CanTake` (the point of the fold, one answer): the wall torch's grip
  (adds the enemy, the flashlight, the fore-end, the magazine, the tug), the pouches' empty hand and the grenade catch
  (they already refused the flashlight: adds the other four), the magazine pull (adds the flashlight, the enemy, the
  fore-end, the tug). Kept as they were (his call): the force grab's start (empty and not the flashlight: a hand on
  the fore-end or holding an enemy may still point one; the enemy has its fgheldfoe pause), items.qc's pickup (the
  lock alone).
- **Test:** `vr_pickup_test 7` (VR_SlotTest_Run in vr_axestick_test.qc, for the axe's fields): 16 passed, 0 failed (busy
  empty but not taking, a release keeps it, unbusy by kind, stale fore-end and tug ended, the foe bit and the
  flashlight bit through VR_Hand_Derive, the hand-over's exception, a tug start and end).
- **Verified:** the covering cases (scratch/foldcheck_run.sh, 40: grabcheck's 36 plus fist_alert, parryinterrupt,
  agrunt_2/33): 1 changed, reload 111/4 ("the ammo button from behind: 1 of 20 approaches", the same line in one of
  (c)'s runs before the fold: a flake), agrunt_67 1/1 (listed flaky); no `handslot: clash/stale/mismatch` line in any
  log. foegrab/onehand_drag ALL PASS, toolgun_physgun_ragdoll PASS. eval.sh full 163/171 as before the fold (differ
  counts 10 and 17 on two runs of one build: event details, PASS -> PASS; necksnap 6/6, charges 22/22). No A/B: the
  per-frame cost is two field reads and a compare a hand in PreThink and PostThink.
- **Checklist:** hold the flashlight in one hand and grab a box, a pouch round and a wall torch with it: none taken
  (the other hand takes them); the flashlight hand still punches. Grab a grunt, let it go, then grab a box with that
  hand: taken. Two-hand the super shotgun, then reload it from the pouch with the fore-end hand (let go first): as
  before. Seat a magazine and pull it straight out again without letting go (Release Seated Magazine off): as before.
## Sparse config: only what the player changed (branch `agent/sparsecfg`; design review proposal 1)

His decision: forgotten defaults now reach players (yes).

- **Before:** `Cvar_WriteVariables` wrote all ~24,000 archived cvars (19k of them weapon/prop slots): a config pinned
  its player to the defaults of the day it was written, so every changed default needed a `defaultChanges` row and a
  `configVersion` bump (362 rows; D2 found two forgotten).
- **Now (cvar.c, behind `// QVR`; vr_cvars.cpp):** the config starts its settings with `vr_config_defaults` (every
  `vr_*` setting back to its default, ROM/locked ones left), then writes every Ironwail setting as before and a `vr_*`
  one (slots included) only when `VR_ConfigSaves`: its value differs from its default (as numbers when both are, the
  file's `sameValue`), or it is a version marker (`vr_cfg_version`, `vr_wofs_version`, `vr_props_version`,
  `vr_bindings_version`: a missing one would read as its default). The reset line keeps a config executed again, or
  after a `game` switch, giving what a full one gave (no previous folder's values left over). The two-copies merge
  (`readConfigSettings`) reads a vr_ setting with no line as its default, so another copy's "back to the default" is
  merged too.
- **configVersion 218**, the last: the rows apply to configs written before it, then the tables (`defaultChanges`,
  `namedDefaultChanges`) are frozen. A changed default is now an edit of the default alone; a row only to override a
  player's own choice on purpose. The slot tables' steps likewise only for transforms. CODE_STYLE.md "Settings and the
  config", ARCHITECTURE.md "Where to put new code" updated.
- **Sizes:** the run baseline config (14.5k lines, version 141) writes 466 lines (299 vr_, 282 of them slots); his
  config (24k lines, version 215) 291 lines (124 vr_: his choices, 40 slot values; all else equals today's defaults).
- **Test:** `Misc/quakevr/config_sparse_test.sh <agent> [full config]` (`vr_config_test`, vr_config_diag.cpp; ends on
  `config_test: N passed, M failed`): the full config loaded with its migration; the game folder's config written,
  edited as another copy would (a changed line removed, a line added) and written again: both merged; snap; written
  sparse: every vr_ line changed, every changed one written, values as snapped, the reset line first; 4,854 plain vr_
  settings scrambled, the sparse config executed: all 24,007 archived settings as snapped (0 differ; his: 2 the same
  number in other text); three defaults moved (one unchanged, one slot, one changed), executed again: the two take the
  new defaults, the changed one keeps its line. 16 passed, 0 failed with the baseline config and with a copy of his.
- **Old loader vs new:** the base exe (origin/next) and this one each loaded the baseline config, then his (exec +
  migration), and dumped every archived setting: identical but `vr_cfg_version` 217 -> 218 (24,007 settings each).
- **Covering cases** as baseline.tsv: selfchecks (all PASS), stats_levelchange 4/0, slider_step 3/0. Build 0
  warnings, check_canonical no new copies. (run_baseline.py needs qvrtest.py, not on origin/next: run directly.)
## Refactor leftovers L2: render/fx/audio file splits (branch `agent/leftsplit2`)

No behaviour change intended: pure moves (each moved item's text unchanged; a moved file's anonymous namespace became
`<ns>::detail`, declared in `<file>_internal.hpp`, its variables defined where they stood; small anonymous helpers that
the parts share moved to the header as `inline`, internal-linkage constants as `inline constexpr`/`inline const`; a
default argument kept on the header's declaration only). The worktree's scratch/split.py, split2.py, mk_internal.py
did the moves (not committed).

- vr_particles.cpp (4162 -> 2007 lines) + vr_particles_blood.cpp (blood/smoke/spark emitters, the gore's drips and
  droplets, chainsaw/contact/battery/torch smoke), vr_particles_trails.cpp (trails, impacts, splashes, mist, fire, AD
  streams, tank fire, electricity, embers, shells), vr_particles_diag.cpp (the test and report commands);
  vr_particles_internal.hpp: Particle and its enums, pool/lastRun/rng (extern; rng's dynamic initializer stays in
  vr_particles.cpp), rndi..onSphere and make<> inline, ShoreNear/LightStats and the report's counters.
- vr_relight.cpp (4181 -> ~2190) + vr_relight_bsp.cpp (lumps, entity text, map lights, texture rules) and
  vr_relight_glow.cpp (faces, things, fixtures, glow images, glowLights); the glow cache (a mem::Cache) stays in
  vr_relight.cpp. Allowlist: startsWith's strings-copy moved to vr_relight_bsp.cpp (+ its declaration in the header).
- vr_audio.cpp (3980 -> ~3500) + vr_audio_io.cpp (derived sounds, the band-limit tables and scratch, the full-band
  resampler; the layered sounds and readPcm went to vr_sndlayer.cpp with agent/gunsfx, kept there at the merge). The jobs-site block is `audio::detail`; BandLimitStats in the header. The two
  mem sets (band-limit tables/scratch) now register from vr_audio_io.cpp: their place in the mem report's list moves.
- vr_portals.cpp (3224 -> ~2210) + vr_portals_gates.cpp (crossing for players, monsters and what flies, unstick, the
  AI's gate images/routes, pulls, tosses, VR_PortalMonsterCross); aiRoute lost its `static` (both files call it).
  Allowlist: player-edict 3 -> 2 + 1 in vr_portals_gates.cpp.
- vr_brushes.cpp (3605 -> ~2180) + vr_brushes_model.cpp (polygons from planes, bevels, the brush model: hulls,
  edges, textures, surfaces, build/apply; its anonymous namespace became `brushes::detail`) and vr_brushes_text.cpp
  (the default alignment, box/shape/rotate/scaleTo/forEachPolygon, toText/fromText with Tokens and facePoints; its
  own anonymous namespace kept). vr_brushes_internal.hpp gained Winding, the four epsilons/extents as
  `inline constexpr`, and baseWinding/polygons/worldTexture/bevels' declarations.
- vr_backend_openxr.cpp (2657 lines, one class) in two commits: first every member function's body moved out of the
  class body below it in the same file (no other change: the declarations kept their comments and `[[nodiscard]]`/
  `static`/`override`; the template `guarded` stays in the class; the Linux/Windows contextReady/logGlInterop pairs keep
  their `#ifndef _WIN32` round the definitions), then by phase: the class and the fault helpers (now `inline`) in
  vr_backend_openxr_internal.hpp, namespace `qvr::oxr` (was anonymous: a class shared by four files needs a name);
  vr_backend_openxr.cpp (~1150: start/stop, instance, system, session, spaces, the state trail, events, the debug
  messenger), vr_backend_openxr_frame.cpp (~700: begin/end frame, eye images, timing, hidden area, swapchains),
  vr_backend_openxr_actions.cpp (~370: actions, bindings, profiles, input, haptics, hand poses),
  vr_backend_openxr_panel.cpp (~70: the panel's swapchain, images and place).
- vr_audio.cpp (~3500 -> ~2950) + vr_audio_cmds.cpp (vr_snd_info, the captures, scene_obj, the test plays with
  playAt/playLocal, vr_snd_dump, burst, the vr_snd_bench run). The mixer's state (Follow .. BenchRun, Live) and
  dynamicChannels (`inline constexpr`) moved to vr_audio_internal.hpp; `live`, wanted, running and writeWav16 became
  `audio::detail` where they stand (declared in the header).
- Checked: build 0 warnings and every check (check_canonical included); run_baseline.sh --only on collectfx,
  mapflameburn, spentshake, vrtrailer, swim_sound, flare_1, explosion_debris, selfchecks, teleporter_edges,
  walltorch_grab, teleporters_chase: 0 of 11 changed against baseline.tsv (0 flaky).
  Second batch (brushes, openxr, audio commands): build 0 warnings, check_canonical unchanged (no allowlist
  change); brush_test.py PASS (every check); run_baseline.sh --only xr_runtime, selfchecks, swim_sound: 0 of 3
  changed (xr_runtime 101/2 as in baseline.tsv); vr_snd_info/vr_snd_dump run.
  A/B bench (ThinLTO both, interleaved ABBAABBAAB, 5 reps each, the armor tree's copy of ab_bench.sh swapping only the
  exe: the QC is the same), this branch (B) vs origin/next 9e559363a (A), CPU ms median: idle_e1m1 0.722 -> 0.734
  (+1.7 %, sd 0.08), combat_48 2.311 -> 2.333 (+1.0 %, sd 0.17-0.37); means -3.2 % / +4.1 % (one B outlier at 3.26):
  within the noise, no regression from the splits (particles, relight, audio, portals included).
## Refactor leftovers L3: the hot hands/weapons/locomotion functions split (branch `agent/lefthot`)

Each split at its comment-marked phases; each body moved as it was (a tool cut the line ranges against the original
numbering, kit-less: `scratch/splice.py` in the worktree), the shared locals bound to a context struct by reference
where several phases share many of them, plain parameters where few. A checker (`scratch/movecheck.py`) compared every
file's stripped lines with next's: the only lines gone are a loop body's `continue;` (now `return;`, the body being the
whole iteration) and the declarations that became struct members; every other added line is a signature, a binding,
a call or a brace.
- vr_hands.cpp update (220 -> ~90 lines): easeStairs, placeTracked, placeFlat, logBodyDrift.
- vr_client.cpp buildMove (206 -> ~60): handMove, setVrBits, setButtons, poseMove.
- vr_input.cpp update (307 -> ~75): buttonChanged (the button's dispatch chain), routeSticks.
- vr_climb.cpp climbPreThink (299 -> ~70): takeKnockOff, letGoIfForced, gripHand, fallFlung, hideHeldGrips;
  VR_ClientClimb (417 -> ~75): mantleStep, then over `Hang`: hangPull, hangMove, keepWithinReach, settleOwed,
  reachCheck, tryMantle.
- vr_foegrab.cpp serverFrame (261 -> ~120): releaseBroken, shoveCarry, markDragging, followHands, pullToHands,
  rememberPalms (Follow's cvar read moved before the shove's carry: a pure read); legCheck (227 -> ~140): legLog,
  and over `LegTry` legPull, legLift.
- vr_view_hands.cpp setupRigHand (the brief's "drawRigHand", 311 -> ~150): solveBrush, traceGrasp, over `RigFrame`
  placePalm, poseFinger; setupHand (294 -> ~95): placeController, over `HandPose` steadyOther, heldBy.
- vr_view_weapon.cpp setupWeapon (315 -> ~110) over `WeaponFrame`: placeLoadPort, placeHotspots, magazineSpot,
  printGrips, placeCarried, chooseGrip, placeMorph.
- vr_twohand.cpp applyHotspots (215 -> ~165): over `GripCheck` printGripState, reportWhyLetGo; aimCup. The two-handed
  aim's last 16 lines left in place (they call the `aimFor` lambda: moving them changes code, not just its place).
- Bench: new scenario `climb_shimmy` (group climb): vrclimb's long ledge, both hands take it, four shimmy strokes each
  way, then hanging (climb_plays.py shimmy at 1.52 m: at its 1.638 m the mock's hands passed 4 units over the ledge,
  "no hold"; checked: 15 holds taken, every stroke).
- Tests: build 0 warnings, all checks. 42 covering cases (climb, grab, grip, reload*, mapgun, pickup, weapon_catch,
  held_over_props, throw_spin, slider, melee, parry, recoil, judo, walltorch, selfchecks, turned_box, shot_shape, swim,
  slopes, rigstudio_aim, autopump, fist_alert, collectfx, stats_levelchange, eval, limb_wear, teleporters_chase,
  stealth) as baseline.tsv; reload_gunshape timed out once waiting for an exclusive run (no output), alone: PASS 24/0.
  progs.dat unchanged.
- A/B (LTO, ABBAABBAAB, 5 reps each, cpu_busy_ms median, A = next 2d76cbe00): climb_shimmy 0.614 -> 0.600,
  armed_16_melee 2.549 -> 2.490, play_e1m1_fight 1.569 -> 1.527 (within noise; one A melee run at 4.5 ms).

## Nail pistol: a full-slide look (branch `agent/npslide2`)

His words (2026-10-30): the model should have a full slide on top like an actual pistol, as a testable alternative, the
current one kept to fall back to.

- **The setting:** `vr_nailpistol_model` 0 (default: Classic, the copper slide on top) / 1 (Full Slide); VR Settings >
  Combat > Weapon Damage > Nail Pistol Model, under Slide Cycle. Live: every copy of the gun (hands, holsters, armed
  grunts' props, items, the toolgun's) changes at once.
- **One file, two looks:** `progs/v_nailpistol.mdl` keeps its classic frames 0 `rest` / 1 `rest1` byte for byte (same
  vertices, triangles, box, scale_origin; the skin's first 256 rows) and gains frames 2 `slide` / 3 `slide1` and 364
  triangles. Each look's own triangles are folded to one point inside the gun in the other look's frames (zero area,
  nothing drawn); the grip, guard, trigger, muzzle and sights are shared. `VR_NailSlidePose` (vr_nailslide.cpp) draws
  frames 0/1 or 2/3 by the setting, with the slide logic unchanged (the server's frame 1 = back, its share either way).
  Why not a second .mdl: the model's scale_origin is the corner the game scales the gun about (vr_render.cpp), its
  bounds the held boxes; a first try that grew the box moved the drawn gun in the hand (seen in eyeshots), so the box
  is the classic one. Its frame 3 can then hold the slide only 12 x steps back (0.651 units; the box ends there):
  the game blends past frame 3 (blend up to 3 / 0.651 = 4.6, a pure translation in whole byte steps, every vertex alike;
  `mix` in the shader extrapolates), so Slide Travel 0..3 works the same in both looks.
- **The model** (`Misc/quakevr/models_src/nailpistol/build_nailpistol_slide.py`, new, pure Python: run after
  export_mdl.py; on its own output it writes the same file again): a frame (lower receiver z 1.0..1.7 in design units,
  a dust cover in front, hiding the grip's top; dark iron), a slide (x -4.6..7.3, the seam at z 1.7, walls +-1.2 to
  4.7, chamfered to a 1.8 wide top at 5.0: the three vents as slide cuts where they were, an ejection port on the right
  side with the copper feed block and two nails in it, rear cocking serrations both sides, two rivets a side; the
  sights ride on it at their classic places, so the sight-alignment table holds), a copper hexagonal barrel on the
  bore that shows when the slide is back, a ring closing the nozzle's back. Walls and caps are gridded on every break
  (no T-junctions). The skin grows to 256x512: the new parts painted below row 256 by the script, after
  build_nailpistol.py's painter (rust, pitting, worn edges, rivet domes; crevices by distance instead of ray-traced
  occlusion; tone matched to the classic receiver's texels: mean colour 62 46 31 vs 65 48 33).
- **Vents** (agent/gunfx's vent smoke): at rest exactly where they were; in the full-slide look they ride the slide
  during a shot's cycle (0.14 s) and while locked open (Slide Travel back).
- **Checks:** build 0 warnings, all checks. The classic look against the old file (eyeshot, same pose, the gun's
  crop): mean 1.3 per channel (the two runs' own noise: the sky crop 0.0), drawn in the same place; before the box was
  kept it was 36 (the gun moved). Eyeshots in qvr-kit/scratch/npslide/: `fullslide_left.png` (left side) and
  `fullslide_right.png` (right side, the port): classic closed, full slide closed, full slide locked open (travel 1.5),
  classic locked open; travel 3 classic, full slide.
- **For him to try:** VR Settings > Combat > Weapon Damage > Nail Pistol Model; fire it dry to see it lock open, put a
  magazine in to see the re-rack; Slide Travel 3 to see the barrel. If he picks Full Slide, its default becomes 1.
## One test harness, part 1: one library, one parser, one tally (branch `agent/oneharness`; design review 7)

- **Library**: `Misc/quakevr/qvrtest.py` (KIT, tree, run, run_sh, lanes, `Tally`, the parser `FAIL_LINE` / `counts` /
  `verdict_near`, FLAKY, mock builders `hand/button/grip/waits/motion`) and `qvrtest.sh` (`check`, `verdict`,
  `expect_verdict`). One flaky list: `Misc/quakevr/test_flaky.txt` (the baseline's 16 plus teleporters_chase).
- **Runners**: run_test_suite.py runs and parses through qvrtest (its FAIL_LINE also reads `<name>: FAIL`: 0 of 286
  recent logs judged differently); the kit's run_baseline.py loads the tree's qvrtest.py (a tree without one: the kit's
  copy `qvrtest_fallback.py` and the old flaky set).
- **Migrated**: 25 shell suites source qvrtest.sh (their `check()` copy gone; lines now `<name>: PASS|FAIL <what>` and a
  final `<name>: N passed, M failed`); judo, leggrab, legsweep, ragimpact, struggle and ad_active use `qvrtest.Tally`
  (`FAILS n` / `n failed` became the verdict line). QC: `VR_Test_BeginFlags` (LOUD: bprint; QUIET: failures only);
  the armed grunts' tests, the arena's and the perks' checks count on vr_testutil's tally (4 pass/fail global pairs
  gone; `agtest: <what>: PASS` became `agtest: PASS <what>`, `arena: fail <what>` became `arena: FAIL <what>`).
  Verdict lines unchanged, so the -QuitOn patterns are too.
- **Guard**: check_canonical.py families `qc-test-tally`, `qc-test-verdict`, `cpp-test-verdict`, `py-test-copy`,
  `sh-test-copy` (today's 121 sites allowlisted; a new copy fails the build).
- **Left (part 2)**: one spawner (impulse 241; 16 helpers), the scenario's settings on testbatch's snapshot, the 64
  QC hand-printed verdicts, 23 Python files' kit paths, the C++ `_diag` printers; the other workers' test files
  (held things, config) left alone.

## One test harness, part 2: one spawner, one snapshot, the QC verdicts (branch `agent/oneharness2`; design review 7)

- **QC verdicts**: 62 of the 63 hand-printed `%g passed, %g failed` lines (the 64th site is VR_Test_Verdict) now print
  through vr_testutil (`VR_Test_Verdict`, `VR_Test_VerdictPart("caps")` for a test of parts); 14 own pass/fail
  global pairs and 2 local ones are gone
  (`VR_Test_Count(ok)`: a check whose line keeps the test's own format; `VR_Test_Nest/Unnest`: agtest's inner
  tests agammo, agshot and weaponshot keep their tallies apart from agtest's "test N over", as before). Every line is
  byte-identical (only `mgworldtest: ...; barrel test needs e5m1` became the verdict plus a line of its own), so no
  -QuitOn or run_baseline pattern changed. `npaim`'s checks now carry its own tag (`npaim: PASS`, as its comment says).
  Left: rigseq's `rig2anim` check (it quotes the engine's totals).
- **One spawner**: `VR_Test_Spawn(player, kind, at, yaw)` (vr_testutil.qc) is impulse 241's dispenser spawn; the
  MG3 tests' four `*_Spawn`, the Shub test's, `VR_STT2_Spawn`, the crate-cover and crouch tests' grunts and the small
  gibs' crowd call it (same entity order: dispenser, then monster). New family `qc-test-spawn`. Left as they are:
  `VR_AGT_Spawn`/`VR_ART_SpawnKind` (armed monsters, `VR_AM_Spawn`: not the dispenser's), impulse 244's grunt, the
  C++ spawners (`vr_physics_spawn`, `vr_toolgun_spawn_test`, `grabDiagPlace`).
- **One snapshot**: `testbatch::CvarSnapshot` (take, takeAll, restore) is vr_test_snapshot/reset's and the scenario's
  (its settings put back, and swapped out of a config write). Checked: a scenario's setting 0 -> 1 -> 0.
- **Python**: 17 tests read the kit from `qvrtest.KIT`. Left (the held-thing worker's): carry/grip_gap, twohand,
  flashgrab, foegrab/gripcrowd, menu_grab, toolgun_physgun_ragdoll. Allowlist tightened after each step.
## Weapon fixes of 2026-10-30: laser bolts on walls, stream shake (branch `agent/wpnfix1030`)

- **Laser bolts bled on walls and floors** (vrfiringrange_2026-10-30_12-16-02). Not a recent code change: the firing
  range's worldspawn (`quakevr/maps/vrfiringrange@3647.ent`, since the map came in) carries stray `"health" "9"` (with
  `count` and `dmg`), so `world.health` was 9 and every touch that asks `other.health` (HIP_LaserTouch, the enforcer's
  Laser_Touch, Rogue's and MG3's shots) read the world as flesh: blood (`spawn_touchblood`) and the bolt stopped instead of
  sparking and bouncing. Fix at the cause: `worldspawn()` (world.qc) clears `self.health`, for any map with such a key
  (nothing reads world.health; AD's start health is its own `ad_reset_health`/`max_health`). The .ent is left as is (it
  keeps the test honest). Test: `vr_mg3_wtest 16` in vrfiringrange (developer 1): world health 0, 12 bolts at the floor
  ahead bleed nothing (vr_blood_sent), then one at a soldier spawned ahead bleeds it; each phase ends when its bolts are
  gone; verdict `mg3wtest: laserblood N passed, M failed`. Before the fix: 2 FAIL (health 9, 12 blood); after 5/0.
- **Stream shake**: the AD flamethrower and poison sprayer tremble a little while they fire, and the hand buzzes. The QC's
  VR_ADGun_FireFlame (the player's: `self.ishuman`) sends `weaponfired(self, hand + 2)` each flame (every 0.1 s) in
  place of its fixed `VRGunHaptic(hand, 0.1, 40, 0.4)`; sendFired passes hands 2 and 3, the client's parseFired gives
  them to `weaponfx::streamed`, and recoilOffset adds `streamShake`: a lean (kick 0.1) and a tremble (+-0.06, three sine
  waves of cl.time: the same each run) and a sideways wobble, through the weapon's recoil (Kick Back, Muzzle Rise and the
  global Weapon Effects' ones), in over 50 ms and settling over Return Time x Recovery after the last flame (+0.16 s).
  At 1: 0..0.6 degrees up, 0.23 cm back, 0.12 degrees turned. The rumble: the same 0.4 at 40 Hz each flame (0.12 s), now
  times the weapon's Stream Rumble. New per-weapon keys (Weapon Offsets > Effects): Stream Shake
  (`vr_wofs_fx_stream_shake_NN`) and Stream Rumble (`vr_wofs_fx_stream_haptic_NN`), 0 for every slot but 30 and 31 (the
  pyro's and the fumigator's guns, 1). Test: `vr_recoil_test`'s stream part (7 checks: defaults, a flame starts it, it
  shakes every frame of a second of flames and trembles, under a degree, settled after, none at 0): 24/0. Not checked
  here: a real flamethrower (the kit's bases have no AD data); the network hop is two lines.

## Baked kick out, procedural recoil in (branch `agent/dekick`)

His words: get rid of the baked kick and muzzle raise from every weapon that has it, keep the parts' own animation
(pump, barrels), use the procedural recoil instead, then drop the code that only existed for the baked kick.

- **The cancel** (`vr_kickcancel.cpp`; the id models are never changed): for each listed weapon model (slots 1-7, 9-15:
  Quake's guns, the laser cannon and proximity gun, Rogue's lava nailguns, multi launchers and plasma gun), at load
  (`view::prepareWorldWeapons`, cached per model in a `mem::Cache`), each pose's rigid motion back to the rest pose
  (frame 0) is fitted over the gun's body. The body: per pose, the motion most vertices agree on within 3 byte steps,
  vertices near the hand counting most (the grip: the slot's Offset point), among the fit over all, none, the last
  pose's and 96 random triples' (deterministic), rolls about the gun's length over 8 degrees refused (a drum or Gatling
  barrels turning is a part, though it may be half the model), refitted over its inliers 3 times; the body is the
  vertices every pose keeps. Drawn: the two drawn poses' cancels blended (slerp, as R_SetupAliasFrame blends the poses)
  and put through the model's byte space (S^-1 T(-so) C T(so) S), the alias matrix ending with it
  (`VR_AliasPostTransform`), and `anchor::posedAnchor/posedVertex/posedVertices` putting their points through the same
  map, so muzzles, shell ports, mounts and the hand-on-a-part motion (`animationMotion`) follow what is drawn. The grip
  hotspots need nothing (the body they are on holds still). `anchor::drawnPoses` is the poses-as-drawn rule, shared.
  `rigpose::fitRigid` (Horn's quaternion) moved out of vr_hitreact_arms.cpp for it. Fit cost: 13 ms for the largest
  model tried (4720 vertices, 36 poses), at map load.
- What the fit found (`vr_kickcancel_info <model>`: per pose cm back at the hand, degrees up, the body's residual):
  the shotgun 11.6 cm / 9.4 deg at frame 2, back at frame 6; the super shotgun 28.7 cm / 6.3 deg; the grenade,
  proximity and multi grenade launchers 21 cm / 24 deg (their upper shell moves on the lower body: a part); the rocket
  launchers 15.5 cm / 8.9 deg; the super nailguns 3.4 cm back, no rise; the lightning and plasma guns 1 cm; the nailguns
  and lava nailgun none (their barrels only); the laser cannon hardly any. Not the Widowmaker (slot 34): its own model's
  frames (vr0-vr8 and the spin frames) turn its barrels only, no kick (measured 0), so it is not listed.
- **Recoil defaults** (vr_weapons.inc): Recoil on for slots 1-7, 9-15; from the measured kick toned down for a gun in
  the hand (half the way back up to 8 cm, 0.6 of the rise up to 12 degrees, the way back from the peak up to 0.4 s), at
  Weapon Effects' 1.5 cm and 4 degrees: shotgun Kick 3.85 Rise 1.4 Time 0.35; super shotgun 5.3 / 0.95 / 0.3; grenade,
  proximity, multi grenade 5.3 / 3 / 0.35; rocket and multi rocket 5.2 / 1.35 / 0.3; lightning and plasma 0.4 / 0.25 /
  0.1. The nailgun and lava nailgun, the super nailgun and lava super nailgun, the laser cannon: his own values (his
  config: Strength 0.65, Kick 0.5, Rise 0.5; 0.785, 0.76, 0.835; 0.71, 0.835, 0.735). His config is still a full one
  (vr_cfg_version 217: every slot's line), so its old "Recoil 0" lines would have read as his choice and left those guns
  with no kick at all: `vr_wofs_version` 73 (takeRecoil73) turns Recoil on where it is still the old 0, with Strength,
  Kick, Rise and Time still at the old defaults taking the new; slots he turned on are left alone. (CODE_STYLE says no
  step for a changed default; that holds for sparse configs, where this step matches nothing.)
- **Removed**: the weapons' blend towards frame 0 (`zb`, `zb_2h`: 97 lines of vr_weapons.inc; the two-handed steadying
  of the baked kick: 0.75-0.8 on the shotguns and launchers), retired quietly (`VR_RetiredCvar`: `vr_wofs_zb_*`);
  the per-slot Recoil's "Off (the model's own)" is "Off"; the help texts that said only the burst rifle needs the
  programmatic recoil. Kept: the shader's ZeroBlend (the fingers' curl and the gadget strap use it; its muzzle-flash
  reference vertices only mattered for the weapons' blend: dead for them now, left for a later pass) and
  `animationMotion` (a hand on a pump or slide still follows it).
- Debug (Debug > Tests, Weapons): "Take Out the Models' Own Kick" (`vr_weapon_kick_cancel`, 1; 0 shows the models' own
  kick, not archived), "Hold the Weapon Frame" (`vr_debug_weapon_frame`, -1: the held weapons drawn at that frame of
  their models), "Kick Cancel Test" (`vr_kickcancel_test`).
- Tests: `vr_kickcancel_test` (also part of `vr_recoil_test`): every listed model found draws each frame through the
  anchors' path with every body vertex within 4 byte steps of where frame 0 draws it, and a body of at least a fifth of
  it; 43/0 (14 models; body after the cancel within 1-3.4 steps; the parts left moving: 12-816 vertices a model).
  `vr_recoil_test`'s stream part now measures the shake at the held gun's own Kick and Rise 1 (the shotgun's are 3.85
  and 1.4 now). Eyeshots (`C:/OHWorkspace/qvr-kit/scratch/dekick/`: `after_sheet1/2.png`, each gun at rest, at its peak
  kick frame cancelled, and its own kick; `fire_sheet.png`: the shotgun and rocket launcher at rest and at the peak of
  the procedural kick): the pixels that change from rest to the cancelled peak are 1.6-3.4% of the eye (the flash, the
  parts) for the shotguns, nailguns and lightning gun, 7-9% for the launchers (their upper shells), against 4-17% for
  their own kick. Covering cases vs baseline.tsv: recoil 67/0 (was 9/0: the kick checks added), shot_shape, reload*,
  nail_1, collectfx as before, but reload_front 33/1 ("a rocket brought to the grenade launcher's muzzle stays out":
  `impulse 178` leaves the rocket launcher in the off hand); the same on origin/next e70960216 without this branch, so
  not this change. A/B bench, combat_48, ABBAABBAAB, ThinLTO: frame p50 11.11 both; cpu median A 2.53 ms, B 2.60 ms
  (spread 2.41-2.92 both); two B runs (6, 7) had p99 210-250 ms hitches while the A runs around them (5, 8) were also up
  (12-13 ms): machine noise suspected, a recheck got no result (the author's game). Worth a rerun.
- **Recoil settings that say the same thing** (not changed; a proposal): per slot, Recoil off = Strength 0 (nothing else
  reads the switch); Strength is a common factor of Kick Back and Muzzle Rise (only the burst cap of 2.5 strengths and
  the AD variants' feel multiply it alone); Return Time and Recovery both stretch the way back (Recovery only after the
  rise, which is min(35 ms, 0.3 Return Time)); the global Kick Back and Muzzle Rise are a unit and a master scale for
  every weapon's. A simpler set: per weapon Kick Back (cm) and Muzzle Rise (deg), absolute, and Return Time (s, peak to
  rest); globally the Programmatic Recoil switch and one Recoil Amount (x1). Migration keeping every feel: cm =
  vr_recoil_kick x Kick x Strength (0 where Recoil off), deg = vr_recoil_rise x Rise x Strength, time = (T - min(0.035,
  0.3 T)) x Recovery; the burst cap and the stream shake expressed in the weapon's own kick (2.5 of it; the lean 0.1).
  His: nailgun 1.46 cm / 2.44 deg, super nailgun 2.68 / 4.92, laser cannon 2.67 / 3.91, Widowmaker 11.5 / 11.4, AD
  plasma 5.34 / 8.74, revolver 14.6 / 21.9. (Approved and done: "Recoil in cm, degrees and seconds", below.)

Checklist:
- Each gun fires with no kick of its model's own: the body holds in the hand; the shotgun's and super shotgun's parts,
  the nailguns' barrels and the launchers' shells still move; the procedural kick is the only one (Debug > Tests,
  Weapons: "Take Out the Models' Own Kick" off shows the old one).
- The muzzle flash, shells and a hand on the pump stay on the gun through a shot.
- Recoil strengths per weapon to taste (Weapon Offsets > Effects; your global Kick Back 4.5 / Muzzle Rise 7.5 make the
  new defaults 3x / 1.9x as strong as measured).

## Recoil in cm, degrees and seconds (branch `agent/dekick`)

His approval of the simplification proposed above: per weapon Kick Back (cm), Muzzle Rise (deg) and Return Time (s,
peak to rest), all absolute; globally the Programmatic Recoil switch and one Recoil Amount.

- **Settings**: Weapon Offsets > Effects: Recoil Kick Back (`vr_wofs_fx_kick_cm_NN`, 0-10 cm, extends to 40), Recoil
  Muzzle Rise (`vr_wofs_fx_rise_deg_NN`, 0-15 deg, to 45; a quarter of it turns sideways as before), Recoil Return Time
  (`vr_wofs_fx_return_s_NN`, s from the peak; the kick up before it is min(35 ms, 3/7 of it), which is the old curve's
  min(35 ms, 0.3 T)); 0, 0 and 0.115 s for a slot with no default. Weapon Effects: Programmatic Recoil (as before) and
  Recoil Amount (`vr_recoil_amount`, 1x, 0-2, to 5: every weapon's cm and degrees times it). Gone: the per-weapon Recoil
  on/off, Recoil Strength, Recoil Recovery, the global Kick Back and Muzzle Rise (`vr_wofs_fx_recoil[_str|_kick|_rise|
  _time|_rec]_NN`, `vr_recoil_kick`, `vr_recoil_rise`): retired quietly, an old config's lines kept for the conversion
  (`weapons::retiredRecoilLine` from `VR_RetiredCvar`). A weapon fires a kick when the switch is on and its cm or degrees
  are over 0.
- **Conversion** (`vr_wofs_version` 74, `takeRecoilUnits74`; `weapons::convertRecoil`): cm = vr_recoil_kick x Kick x
  Strength (0 where Recoil was off), deg = vr_recoil_rise x Rise x Strength, time = (T - min(0.035, 0.3 T)) x Recovery.
  The old values are kept apart (`oldRecoil`: each slot's old shipped values, `oldRecoilDefaults`, under the config's
  lines), and the old steps that touched them (48: the AD soldiers' guns taking the burst rifle's, the crossbow's; 53:
  the Widowmaker's; 73: Quake's guns on) now act on those. The drawn kick is the same exactly where Recovery is 1 (every
  shipped value, all of his) or the time is over 0.117 s; otherwise only the 35 ms kick up differs a few ms. The burst
  cap (2.5) and the stream shake (its lean 0.1) are now in the weapon's own kick, not in strengths: the same for the
  burst rifle and the stream guns (Strength 1). The shipped defaults converted the same way (vr_weapons.inc: the shotgun
  5.775 cm / 5.6 deg / 0.315 s, super shotgun 7.95 / 3.8 / 0.265, launchers 7.95 / 12 / 0.315, rocket launchers 7.8 /
  5.4 / 0.265, nailgun 0.4875 / 1.3 / 0.115, super nailgun 0.8949 / 2.6219, laser cannon 0.889 / 2.087, lightning and
  plasma 0.6 / 1 / 0.07, burst rifle 1.5 / 4 / 0.085, Widowmaker 2.1075 / 5.62, revolver 2.4 / 6.4 / 0.125, ...).
- **Haptic Punch** (vr_gunsfx.cpp): its tail lasts the whole kick (`weapons::recoilKickSeconds`: the old Return Time x
  Recovery wherever Recovery is 1); its spike reads the kick in the old strength-1 units (`weapons::recoilFeel`: the
  mean of cm / 1.5 and deg / 4), which is the old Strength x mean(Kick, Rise) for the shipped values. His global 4.5 /
  7.5 now sit in each weapon's cm and degrees, so the conversion scales his Haptic Punch by sqrt(old / new) where the
  spike was not at full anyway (his nailguns, lightning, laser cannon, rifles: 0.62-0.64x; the shotguns and launchers
  were and stay at full: untouched). The toolgun (recoil off) has no kick to read: its punch goes.
- **His values** (his config of 2026-10-30 copied in, `vr_recoil_test`): nailgun 1.4625 cm / 2.4375 deg / 0.115 s,
  super nailgun 2.685 / 4.916, laser cannon 2.668 / 3.914, Widowmaker 11.54 / 11.43 / 0.176, AD plasma 5.344 / 8.736 /
  0.126, revolver 14.58 / 21.94 / 0.337 (as proposed above); every other gun its new default times his 4.5 / 7.5 (the
  shotgun 17.3 cm / 10.5 deg). 27 slots off the shipped values; 18 Haptic Punch lines.
- **Tests**: `vr_recoil_test` 69/0 (e1m1, shotgun): the conversion part checks every slot's old defaults converted are
  its shipped ones (51/51), draw the old kick every ms of a second within 1e-4 of the peak (the old code's curve,
  `oldCurve`), last as long (51/51), and, where this start converted a config (the kit's old baseline config, wofs 43,
  or his), every slot draws its old kick from the cvars (51/51; his: 27 slots printed with their old and new values);
  Return Time 2x / 0.5x twice / half as long back (0.315 -> 0.630 / 0.1575 s), Recoil Amount 2x doubles. `vr_gunsfx_test`
  24/0 (-Sound). Covering cases vs baseline.tsv: collectfx 9/0, mapflameburn 16/0, reload 112/3, shot_shape, reload_eject
  28/0, nail_1 7/0 as the baseline; recoil 69/0 (was 9/0: checks added); reload_front 33/1 as on this branch before
  (above: not this change). Menu paths 0 missing.
- **A/B bench** (the rerun the kick cancel asked for; combat_48, ABBAABBAAB, ThinLTO, exclusive; A = origin/next
  158faf89e, B = this branch (9c5866271) with the kick cancel and these units; `scratch/ab_run.sh`, labels
  `dekick2_*`): frame p50 11.11 ms every run; cpu A 3.22, 3.33, 2.59, 2.47, 2.50 (median 2.59), B 2.41, 3.24, 2.56,
  2.87, 3.05 (median 2.87). The spread is the hitches: the runs with p99 over 13 ms (A 1, 4; B 3, 7, 10: 2.87-3.33 ms
  cpu) are the slow ones on both sides; the clean runs (p99 under 11.3 ms) are A 2.47-2.59 (mean 2.52) and B 2.41-2.56
  (mean 2.49). No cost measured; machine hitches as before (B drew one more of them).

Checklist:
- Weapon Offsets > Effects: Recoil Kick Back (cm), Recoil Muzzle Rise (deg), Recoil Return Time (s) per gun; your
  guns feel as before (your global 4.5 / 7.5 are now in each gun's values: the shotgun reads 17.3 cm / 10.5 deg).
- Weapon Effects: Recoil Amount 0 / 1 / 2 scales every gun's kick; Programmatic Recoil off stops it.
- The haptic punch of each gun feels as before (nailguns, lightning gun, laser cannon: their Haptic Punch now about
  0.64x, which keeps the same spike).

## Settings tiers (branch `agent/tiers`; design review 2)

Every Quake VR setting is now one of three tiers: a **player option** (a cvar with a menu row), a **tuning constant**
(`QVR_TUNABLE` in `Quake/vr/vr_tunables.inc`: no menu row, not in the config) or a **test knob** (a cvar never saved).
CODE_STYLE.md, "Settings and the config", has the rules. The full list, page by page, with each one's reason and the
author's own value where he set one: the kit's `scratch/refactor/tiers.md` (made by `tiers_scan.py` beside it).

- **How each was sorted** (`tiers_scan.py`, from the code and the author's config of 2026-10-30, read only): anything
  he set off its default, anything personal (comfort, calibration, the tool gun, recording, height, seated play),
  anything a test, tool, map or shipped cfg names, anything code writes or looks up by name (`&name`, `Cvar_Set`,
  `Cvar_FindVar("...")`, a `##`-made table of cvar pointers, a QC `cvar_set`), every row on a Standard-level or player
  page (graphics, HUD, audio, controls, comfort), and every toggle or choice elsewhere stays a **player option**
  (2,699). Never-moved sliders on the gameplay pages, every row of the Developer-level tuning pages, and the archived
  settings with no menu row become **tuning constants** (1,094). Settings already not saved are the **test** tier
  (572). The weapons' and props' slot tables (`vr_wofs_*`, `vr_prop_*`) are design review 3's, not touched.
- **No behaviour change:** each constant keeps its default as its value; `tunableValue` is Q_atof's arithmetic at
  compile time, and the start-up check compares every constant with Q_atof of its default (none differ). His config
  loads with all 22,748 of his cvar values applied, and each of his lines for a constant equals it (1,091).
- **The QC** still reads constants by name: `Cvar_VariableValue`/`String` and `cvar_hmake` fall back to the
  constants' unregistered stand-in cvars (`tunableFind`). An old config's line for a constant is ignored like a retired
  setting's (`VR_RetiredCvar`).
- **Tuning build:** gone (`-p:QvrTuning=1`, QVR_TUNING): every development build tunes them live now ("Live tuning of
  the tuning constants", below).
- **Removed:** 1,094 cvars from vr_cvars.inc, about 1,000 menu rows with their help (menus 2,214 lines fewer), the
  migration rows of the constants. Not done: the test tier's few rows still on player pages (Show Melee Arcs and the
  like, 25 pages) are not yet moved to Debug pages; the "0: old side" switches he doesn't use are still options (a
  D1-style retirement is a separate pass).
- Checks: build; `vr_menu_path_check maps/vrcalibration.map` 0 missing; `config_sparse_test.sh` 16/0 with the
  default config and with a copy of his; the full baseline (`run_baseline.sh harness --no-bench`).

## Live tuning of the tuning constants (branch `agent/livetune`; his "Yes, build it")

His workflow stays: find a number in the menu, tune it in the headset, keep it across sessions, and it becomes the
shipped default. One accessor, two builds (CODE_STYLE.md, "Settings and the config"):

- **Development builds** (every build but a release; the agents' and his): each `QVR_TUNABLE` name is a `constexpr`
  reference into `tunables[]` (indexed by `TunableId`), so `name.value` stays one load from a fixed address, no lookup.
  `tune <name> <value>` sets it (a text or vector too: the rest of the line), `tune <name>` prints its value, default,
  range, page and help, `tune <part>` lists the names holding it, `tune` lists the changed ones; `tune_reset [name]`
  (all without one); tab completes names. The QC's readers (`cvar()`: Cvar_VariableValue/String; `cvar_hmake`/`hget`)
  read the stand-in cvars, kept equal on every set, so progs.dat is the same in both builds.
- **Debug > Tunables:** every constant (1,094) as the row it had before the tiers pass (963 recovered from the tiers
  commits' removed rows: label, range, step, format, extend, help; the 124 that never had a row get a range from their
  default; 7 texts/vectors are console-only lines), grouped under the page they came from. Show (All / Changed Only)
  and Page (one page's) filter it; Reset All to Defaults. The menu's **Search** finds them (one box for settings and
  tunables): its index takes the page's every row whatever the filter, and opening a result clears the filter and puts
  the cursor on that setting (matched by cvar, as labels like "Stiffness" repeat). The rows are data:
  `Quake/vr/vr_tunables_rows.inc` (one `QVR_TUNABLE_SLIDER`/`_CYCLE`/`_TEXT` per constant, `QVR_TUNABLE_GROUP` per page;
  a static_assert checks one row per constant in vr_tunables.inc's order) -- the start of design review 9's single
  settings schema. A row sets its stand-in; Cvar_SetQuick of an unregistered cvar hands it to `VR_TunableSetVar`.
- **tunables.cfg** (the game folder, git-ignored): `tune <name> "<value>"` for each constant off its default, written at
  quit with the config (not by a copy that never writes its config; removed when none is changed), read once after the
  config (`vr_migrate_config`); a console notice says how many it set ("N tuning constants set off their default").
- **Promotion:** `python Misc/quakevr/tune_promote.py <tunables.cfg> [--inc ...] [--check]` writes the file's values
  into vr_tunables.inc as defaults, on a worker's tree with his file read only (the game never writes a source). Then
  his lines equal the defaults and his game drops them at the next quit.
- **Releases** (`QVR_RELEASE`: make_release.ps1's `/p:QvrReleaseVersion`, or `/p:QvrRelease=1` to build the release
  code as a check): `inline constexpr Tunable name{...}`, defaults folded in; no `tune`, no page, no file, no table
  (the stand-ins stay, constant, for the QC). make_release.ps1 refuses an exe holding `tune_reset` or `tunables.cfg`.
  The old tuning build (`-p:QvrTuning=1`, QVR_TUNING) is removed: this covers it.
- `vr_menu_search -open <text>` opens Search's best result (scripts).
- Checks: build 0 warnings; the release code built once (`/p:QvrRelease=1`: 0 warnings, no `tune_reset` in the exe;
  e1m1 loads, `tune` unknown); `Misc/quakevr/tunables_test.sh <agent>`: set by tune, by a page row and a text, every
  reader (C++, cvar(), the stand-in) sees each, set-to-default-another-way is unchanged, tunables.cfg holds exactly the
  changed ones, read back, quit writes it, `tune_promote.py --check` would promote the 3, a new start reads them,
  tune_reset clears them and quit removes the file (38 + 20 checks, 5 stages). Covering cases (run_baseline.sh
  --only: slider_step, selfchecks, throw_spin, stealth, reload_contact, agrunt, arena, ragdoll, ...): 18 as baseline.tsv
  but recoil 9 -> 24 passed (its stream part, added on next since) and ragdoll_guard flaky; config_sparse_test 16/0;
  vr_menu_path_check 0 missing on both boards. A/B ThinLTO bench, combat_48 interleaved ABBAABBA (first pair gave no
  result): CPU ms A 3.18 3.42 3.02, B 3.34 2.50 2.53; frame p50 11.11 both: no cost from the reads through tunables[].

## One impact-effect table (branch `agent/impacttable`; design review 4, his "perform all")

The QC's hit ladders now sit on the table gunfx started (`Quake/vr/vr_impacts.cpp`): one row per material x hit kind
holds the world surface's extras (the client's: dust, sparks, chips, haze), the decal (a wall hit's own `TempEntity`
mark; none on a thing: its wound is the QC's), the thing's effect (one particle2 preset, count = amount x scale clamped,
direction = (dir x scale + add + spray) x leaf, origin along dir) and three sound slots (the hit's own, a blunt blow's
layer, what plays over it). Keys shared with the QC: `Quake/vr/vr_impact_shared.h` (`VR_MAT_*`: none, stone, dirt,
metal, wood, flesh, armour, torch, grenade; `VR_HIT_*`: bullet, nail, super nail, bolt, pellet, blow, blade, splash,
saw, bash). Builtins `impact(material, kind, org, dir, amount)` (sends the row's particle2) and
`impactsound(material, kind, slot)` (a sample of the row's set, one random() drawn as the old picks did; "" draws
none). QC (`vr_crates.qc`): `VR_MaterialOf(e)` (torch, grenade, metal, wood, armour, else flesh) replaces the five
ladders' own predicate chains; `VR_ImpactFx(m, kind, targ, org, dir, amount)` sends the row, a shot's metal ring once a
frame, and keeps the tests' counters (`vr_metal_hits`, `vr_splinters_sent`, `vr_blood_sent`) and `developer` lines.
On it: TraceAttack, spawn_touchblood, VR_HitBlood, VR_HitNonFlesh, VR_HitBloodSplash, the chainsaw's tick, a thrown
crate's splinters, SpawnBlood and SpawnBloodSplash (so their 14 + 9 call sites, and spawn_touchblood's 18, go through
the table), VR_Melee_HitSoundOn, VR_Blunt_HitLayer and the box/holster knocks (impactsound). Gone: VR_Metal_Hit,
VR_Metal_Sound, VR_METAL_*, VR_Pick4, VR_Crate_Splinters, VR_GrenShot_Sparks (gameplay QC 215 lines out, 128 in).
check_canonical: two new families freeze what is left, `qc-hit-fx` (a hand-made particle2 of blood, sparks or wood
dust: 41 sites, none of them a material hit) and `qc-material-ladder` (VR_Hit_Metal / VR_Crate_Wooden /
VR_Hit_Armoured calls: 16, rules not effects).

Debug > Tests: Print Impact Effects (`vr_debug_impacts 1`: every particle2 and QC sound the server sends, `impfx p2
...`, `impfx snd ...`), Every Hit Ladder on Every Material (`vr_impact_qc_test 1`; 1-7 one ladder, 9 all, verdict
`imptest: N calls`; QC/vr_impacts_test.qc).

Left on purpose: gore's own blood (9 direct particle2 sites: player gibs, decap, limbs, Chthon, vr_carry's gore
events: their own counts), SpawnMeatSpray (spawns gibs: gore that drives entities stays QC), the particle2 events
40-43 and decals::fromEffect, the second material enum (physsound::Material), the muzzles (proposal 3), the other
hit sound literals (only the metal and blunt-layer sets moved: 7 sets), AD's own ladders (ad_TraceAttack,
ad_SpawnBlood, ad_Resist_Shells: AD code; their no-flesh hits already go through VR_HitNonFlesh, the pellet row).

**Disagreement list (for him; every row keeps today's output):**
1. Armour plate: TraceAttack, spawn_touchblood and VR_HitBloodSplash ask an armed humanoid's plate
   (VR_AgArmor_HitFx: sparks, blood only for what gets through); VR_HitBlood does not, so a melee blow (and the
   chainsaw) on unbroken plate bleeds in full. Suggest: VR_HitBlood asks too.
2. Knights' armour (VR_Hit_Armoured) changes only the blunt layer's sound (light metal over a duller slap); every
   ladder's particles on them are plain blood. The Armour rows copy Flesh's.
3. Spray strength: sparks off metal go 0.3 x the pellet's bounce (TraceAttack), 0.2 x a blow's, 1 x a nail's;
   splinters 0.2 / 1 / 0.7 (AD pellet) / 12 (saw). TraceAttack's blood goes 0.04 x its bounce (scaled 0.2 before
   SpawnBlood and 0.2 inside it) where a nail's goes 0.2: hitscan blood barely sprays. Suggest one scale per material.
4. The chainsaw's own split: flesh = a monster, a player or a gib; wood = a crate; everything else sparks (4, straight
   up). So under the saw a torch, a grenade and anything else that takes damage without being a monster, a player, a
   gib or a crate sparks, where the other ladders go by material (a torch nothing, a non-monster body blood). Suggest: the saw uses VR_MaterialOf.
5. Grenade sparks are a fixed 8 straight up (at a nail's own origin, not 0.01 x its velocity along like every other
   nail row); metal's scale with damage (3-10) and go along the hit. Suggest: the metal row.
6. A blunt blow's layer slaps flesh on a torch and on a grenade (the layer counted them as bodies); their particles
   are nothing (torch) and sparks (grenade). Suggest: a wooden knock for the torch, the light metal for a grenade.
7. The world: every ladder bleeds on world (spawn_touchblood, VR_HitBlood called with it: blood); callers never pass
   it (they check takedamage first). The blunt layer calls it stone. VR_MaterialOf(world) is flesh to keep that.
8. Order: TraceAttack and spawn_touchblood asked metal, wood, grenade, torch; the other three torch, grenade, metal,
   wood. No thing is two of these, so VR_MaterialOf's one order changes nothing.
9. Sounds: shots (bullet, nail, AD pellet) ring on metal once a frame (tink, ricochets, 0.8); blows and splashes on
   metal send sparks and leave the sound to the melee (clang or knock, VR_Melee_HitSoundOn). Same as before.
10. A thrown thing into wood: splinters along its flight (1 x velocity); into anything else: spawn_touchblood's nail
   row (a wall bounce's direction, a ring on metal).

**Verified:** `vr_impact_qc_test 9` (7 ladders x 7 targets: the world, a crate piece, an explosive box, flesh, a
knight, a wall torch, a grenade; `sv_random_seed 7`, vrtesthall) prints the same 49 calls and the same effect lines,
byte for byte, before (step 0's trace on origin/next's code) and after: every particle2 (preset, count, origin,
direction) and sound (sample, volume), so the random draws kept their order. Eyeshots of ladders 1-6 x the 7 targets
before and after (kit `scratch/impacts/before.png`, `after.png`, `before_after.png`): the same effects in the same
places; pixel difference 0.5-2.9 mean per ladder (particle noise). The world's rows (vr_impact_test, all four kinds,
forced materials): the old counts. Baseline cases (run_baseline.py vs baseline.tsv): collectfx 9/0, mapflameburn
16/0, explosion_debris 13/0, shot_shape, ragdoll_* (stagger 52/5, ragtest 30/1, climb 24/4: as the baseline), limb_wear
9/3 (as the baseline), agarmor_1 26/0, agarmor_5 23/0 (baseline 22/1, flaky), nail_1, np_1 (23/0, baseline 21/0: the
test's own newer checks), flare_1, walltorch_grab, empty_melee, secret_hits and dead_gear (as the baseline): no case
changed. gunfx_test.py 29/0. Build: 0 warnings, check_canonical clean. A/B bench (LTO, exclusive, ABBA, scratch/ab_bench.sh: A = origin/next + the trace, B = the table; cpu busy ms): combat_48 A 2.64 (2.69, 2.58), B 2.44 (2.43, 2.46): -7.3%; play_gore_fight A 2.36 (2.30, 2.42), B 2.40 (2.41, 2.38): +1.4%. Both within the runs' spread (A's own runs differ 4-5%): no cost measured. (The planned ABBAABBA stopped after 4 runs: the exe was busy at the swap.)


## Worn backpacks, experimental (branch `agent/wornpack`; `QC/vr_wornpack.qc`, `vr_box3d_wornpack.cpp`)

His ask: enemies wear the backpack they drop, attached through ragdoll form, torn off by hard pulls, landings and
direct hits with a fabric rip, takeable only once dead, no second drop, and the reserve lost with a pack torn off alive.
Scope (his change): the armed **grunt** only (the enforcer's model already has a pack; ogres and others later).
- **Flag.** `vr_worn_backpacks` 0 (Combat > Armed Grunts > Worn Backpacks). Off: no pack is made and every hook returns
  at once (as before). Grunts spawned after it is turned on wear one (`VR_WP_Spawn` from `VR_AM_Spawn`; not where
  `MG3_NoBackpack` or the dummy's no-loot rule apply).
- **On the back.** A QC entity (`vr_wornpack`, progs/backpack.mdl, not solid) placed every frame from
  `limbpiece(g, 0, 20/21/22)` (origin, angles, box middle). Standing: `agarmor::packWhere`, the pack's rest place behind
  the cuirass's outer face at 62% of its height (the back measured where the pack lies, not the small of the back),
  carried by the chest bone's frame (three chest-weighted body vertices nearest the pack, hitmodel's pose). Fit cvars:
  `vr_worn_backpack_scale` 0.6, `_x` 1.5 (into the back), `_y` 0, `_z` -3, `_pitch/_yaw/_roll` 0 (menu: Fit). The pack
  follows the server's pose, so the client's aim IK/lean can shift it a little against the drawn body: for him to check.
- **Ragdoll.** `createRagdollPack` (after the parts, when `.vr_wp_worn` is 1): a box (90% of the pack) as a second shape
  of the chest part's body, where the standing pack was (`vr_worn_backpack_mass` 4 kg): it collides, weighs and is held
  with the body. A hand's grab within 3 units of the box marks the hold `onPack` (`beginGrab`); the drawn pack follows
  the shape. Builtins (table full): `ragdollbone(e, [what, player, hand], QVR_WORNPACK -1003)` 0 worn, 1 tear off (shape
  destroyed), 2 the hold's force, 3 its stretch; `QVR_WORNPACK_AT` -1004 a point's distance to the pack.
- **Tearing off** (`VR_WP_Off`, `vr/derived/fabric_rip.wav`: demon/dhit2 1.15x + player/tornoff2 1.35x + weapons/lhit
  1.6x 0.2 s, drive 0.6; `vr_worn_backpack_volume` 1):
  - a pull: a hand holding the ragdoll by the pack, past `_rip_force` 3000 N for `_rip_time` 0.04 s (force, or stretch x
    `_rip_stiffness` 150 N/unit), the hold's first 0.25 s ignored (the spring taking up the body jerks to the 4000 N
    cap): released from the ragdoll, the pack carried in that hand. Measured: grab 4000 N for 0.15 s; slow drag (43 u/s)
    700-1700 N, 1-3 units stretch; a 30-unit yank 4000 N, 13-34 units, for 2-3 frames;
  - a landing: `VR_RagImpact_Hit`'s strike at `_impact` 9 m/s or more (any ragdoll, alive or dead);
  - a hit on the pack (T_Damage, within 3 units of its box: blows, throws, shots, projectiles; not blasts, burns):
    `min(1, _hit_chance 0.35 x damage / _hit_ref 20)`;
  - its wearer gibbed (its model no wearer's) or gone, or a dead one's pack taken (`VR_Carry_Start`,
    `VR_Forcegrab_Launch`; `VR_Ragdoll_Handtouch` leaves a hand on a dead one's pack to the pack).
- **Ammo and loot.** Torn off a living grunt, the pack takes `_reserve` 1 of each weapon's reserve plus its pockets
  (`VR_WP_Lose`): it fights on with its magazine, then drops the gun as an empty one does. Dying, `VR_AG_SoldierDrop`'s
  loot goes into the worn pack (`VR_WP_DeathLoot`), which turns takeable (FL_ITEM, force-grabbable, carried; still placed
  on the corpse until taken); a wearer never drops another backpack (also when its pack was lost alive). Torn off, it is
  `DropBackpack`'s item (`Backpack_Setup`, split out of it), and is not flung into its own wearer for 1 s.
- **Test.** `vr_wornpack_test 1` (Debug > Tests > Worn Backpacks; e1m1, `god; notarget`): **17 passed, 0 failed**:
  on its back (14.8 units behind, 12.5 up); untakeable alive; a chest hit keeps it, a pack hit tears it (reserve 16 -> 0,
  pack 16 shells); the ragdoll made with the box, the pack 1.3 units from where it stood; kept through a mild knockdown
  and a 4 m/s landing, torn by 12 m/s; held by the pack, a slow drag keeps it, a yank tears it into the hand (5119 N);
  killed: no second backpack (the flag-0 control drops one), the pack takeable, taken it comes off. `2`: the eyeshot's
  grunts (Debug > Tests > Grunts Wearing Packs There; the kit's `scratch/wornpack/`).
- **Baseline (flag 0)**: agarmor_1 26/0, agarmor_5 23/0, ragdoll_grab 19/0, agrunt_1/12/33, pickup_mag, mapgun_pickup,
  flash_grab, walltorch_grab as the baseline; agrunt_31 4/1 (as the baseline) and ragdoll_stagger 51/6 (its known
  5-6 chance failures).
- **Not done.** A corpse's pack is not torn off by hits (only taken, pulled, gibbed or landing hard); the pack doesn't
  shield the back from hits; its sound can't be heard headless.

## Worn backpacks, round 2: no spin, grabbed off the living, toppling, damage pool (branch `agent/wornpack2`)

His feedback (items 1-6) and the coordinator's follow-up (damage pool, flame/poison/explosions, gibbing).
- **No spin.** The worn pack kept backpack.mdl's EF_ROTATE: the client overwrote its yaw every frame. The engine now
  reads the pack's `vr_wp_state` (new QVR_FIELD) and sends U_QVR_NOROTATE while it is worn (1 living, 2 dead); torn off
  it is the dropped item as before (a rigid body, no spin either). A dead one's pack lost Backpack_Setup's lying
  `model_offset` (it was drawn off the back). Eyeshot (`scratch/wornpack/spin_before.png`, vr_wornpack_test 2): upright
  on the back, behind and side on. Test: yaw 0.5 off its wearer's, pitch 0.8, roll 0.
- **Its box.** The entity's size is now the level box round the turned model box (`limbpiece(g, 0, 23)`: 13 x 13 x 15
  units at scale 0.6) instead of a 20-unit cube round its diagonal. Shots and projectiles (MOVE_HITMODEL) meet a living
  wearer's pack at its model box as posed (`box3d::wornPackSegment` in VR_HitModelClip), the monster hit there; a shot
  counts on the pack within 0.1 unit of its box (blows, throws, projectiles 3). Test: 29 lines across it from behind:
  9 inside met it, 18 outside never did, 0 wrong.
- **Grabbed off the living** (`vr_worn_backpack_tug` 1). A fist on a living wearer's pack takes the pack as a foegrab
  hold (vr_foegrab.cpp `takePack`; the pack before the body): the palm is put on the pack's nearest face (its middle a
  palm's sphere off it) and carried with the pack as the body is posed, so the drawn hand lies on the pack (item 5). It
  doesn't drag the grunt (it stands its ground), never lets go by stretch below 4x vr_foegrab_break, and wakes it
  (VR_FoeGrab_Taken: spotted). QC `VR_WP_Tug`: each hand's stretch (`ragdollbone` QVR_WORNPACK what 5) from
  `vr_worn_backpack_tug_stretch` 10 cm counts, harder sooner, `vr_worn_backpack_tug_time` 0.6 s of it tears the pack
  into the main hand (what 6 ends the holds, the grips the QC's at once). Test: 0.5 m/s, one hand 0.54 s, palm 3.5 cm
  off the box throughout.
- **Two hands** (`vr_worn_backpack_two_hand_mult` 1.5): their pulls added, times it; also for a ragdoll held by the
  pack. Test: 0.22 s against one hand's 0.54 s.
- **Topple** (`vr_worn_backpack_topple` 1, `_topple_push` 0.8; a ragdoll trigger): torn off a living one by a pull, it is
  knocked down as a shove knocks it (VR_Knockdown_Start, ragdollshovetopple) along the hands' travel since they took
  hold (`limbpiece(g, pnum, 24)`: not the spot's swing as it turns). Test: along the pull, dot 1.00 (one and two hands).
- **Damage pool** (`vr_worn_backpack_pool` 25): a living one's pack tears off once it took that much: hits located on it,
  `_pool_blast` 1 of an explosion's damage anywhere, `_pool_fire` 1 of fire's (vr_adflame, vr_burn); never poison
  (vr_adpoison, poison puffs). A hit on it spots the attacker. The per-hit chance is kept as `vr_worn_backpack_hit_roll`
  0 (renamed from `_hit_chance` 0.35 so his saved 0.35 doesn't stay on). Rip volume: `vr_worn_backpack_volume` (as
  before). Gibbing tears it off (as before; now tested). Test: hit 15 (12 x the zone's factor), blast +8, poison +0,
  fire: 33 of 30, torn ("fire").
- **Menu** (Combat > Armed Grunts > Worn Backpacks): Damage That Tears It, Explosions' Share, Fire's Share, Extra Hit
  Chance; Tearing It Off a Living One: Grab the Pack, Pull That Counts, Pull Time, Two-Hand Pull, Tearing Topples It,
  Topple Push.
- **Test** `vr_wornpack_test 1`: **39 passed, 0 failed** (was 17). Not done: the walk-cycle placement check (the posed
  pack checked standing only), the flag-off covering cases (foegrab, hitmodel tests) not rerun.
- **Checklist**: grab a living grunt's backpack from behind and pull back: it turns on you, the pack tears off into
  your hand after about half a second and the grunt falls towards you; with both hands sooner. Your hand stays on the
  pack while you pull. Shoot just beside a grunt's pack: no rip; shoot the pack a few times, or hit it with a rocket's
  blast or the flamethrower: it rips off; the poison sprayer never. Packs on living and dead grunts don't spin.
## Backpacks' hit box (branch `agent/bpfix`)

- **His report**: "backpacks dropped by enemies have a much larger hitbox than expected against player projectiles (e.g.
  nails)". Cause: a dropped backpack (DropBackpack) is a loose Box3D prop, `SOLID_NOT_BUT_TOUCHABLE` with `FL_ITEM`, and
  SV_LinkEdict grows an FL_ITEM's abs box 15 units each way across (for pickups: 45 x 45 units round a 15-unit bag). The
  nail's own move passed it (touchable, no takedamage), but with `vr_gameplayfix_touchsolids 1` (quakevr.cfg) the bag's
  link touched every solid whose abs box overlapped that grown box (vr_physics.cpp `touch`), so the nail's touch ran with
  the bag as `other`: stopped and puffed up to ~22 units off the bag, and where the overlap fell between two frames a nail
  went on through the bag having pushed it (the push, `physicsshot`, meets the real shape).
- **Fix** (engine): a flying missile (MOVETYPE_FLYMISSILE: nails, rockets, lasers) meets a dropped pickup lying loose
  (touchable, FL_ITEM, a Box3D prop: `box3d::missileMeetsItem`) in its own move, at the item's drawn box as it lies
  (`VR_ItemShotClip` in SV_ClipToLinks: the explosive boxes' turned shot box, `shotShape`/`traceProp`), and the touch of
  the two overlapping no longer calls either's touch. Pickups (the player's touches and hands), grabbing, force grab, carry
  and the bag's physics are untouched (only missile/item pairs). Under `vr_box3d_shot_shape` (Shots Meet Their Shape,
  default 1); 0 gives the old behaviour.
- **Test**: `vr_nail_test 3` (and vr_nail_test 1's new last step; Debug > Tests > Nail Pistol > Nails at a Dropped
  Backpack): a backpack dropped as a monster drops it, settled 30 frames; nails one at a time across it at its middle's
  height (-30..30, 2-unit steps) and over it (0..24 up), the drawn box computed in QC from drawnbounds and its angles as
  held::axesFromAngles turns it. Checks: the middle met, none beyond the box's reach (+1), every hit at the box (within
  1.5), no nail through it having moved it. After: 4/0 (met -8..8, reach 8.7; up to 8, reach 8.4; 14 hits, 0 off). With
  `vr_box3d_shot_shape 0` (the old path): 2 FAIL (13 of 13 hits away from the bag, 2 nails through it).
- **Checked**: the covering cases vs baseline.tsv (run_baseline.py, 12 cases): nail_1 7/0 -> 11/0 (the new step), np_1 21/0 -> 23/0 (known: its newer checks), the rest as the baseline (pickup_mag, mapgun_pickup, mapgun_transfer,
  flash_grab, shot_shape, held_over_props, throw_spin, ragdoll_grab, prop_climb; weapon_catch 3/2 as the baseline). Build: 0 warnings, checks clean.
- **Checklist**: shoot nails just past a backpack a grunt dropped: they fly by; at it: they hit it and nudge it; walk over it, grab it, force-grab it: as before.
## One hit record, part 1: where a hit struck (branch `agent/hitrecord`; design review 6)

**What:** the "where did it strike" half of proposal 6, in the review's three steps (one commit each). A shot's or a
blow's zone and a shotgun blast's pellets now live in one record, `hit_*` (combat.qc), with one writer each:
`VR_Hit_SetZone` (PositionalDamage, VR_Melee_Positional), AddMultiDamage / VR_HitReact_Pellet (the blast's pellets,
their parts, their points on its flesh) and ApplyMultiDamage (`hit_blast` set around the blast's damage only). Every
consumer reads it through `VR_Hit_On(targ)` / `VR_Hit_Zone(targ)` (the record is for that target this frame),
`VR_Hit_Pellets()` (0 outside a blast's damage) and `VR_Hit_PelletPart(limbs)` (the one majority rule); one location
vocabulary, the zones positional damage always used (1 head, 2 limb, 3 legs, 0 body, -1 none). **Deleted:** 17
globals (`vr_hitpart`, `_target`, `_time`, `_pos`; `vr_hitpellets`, `_head`, `_limb`, `_leg`; `vr_hr_pel_ent`,
`_time`, `_count`, `_sum`, `_dir`; `multi_pellets*` x4) for the record's 10, and 5 hand-copied validity checks and
2 pellet-majority ladders. Consumers switched: the ragdoll's death hit, the corpse's positional factor, the flinch
(VR_HitReact_Where), the stagger's head test, the highlights' headshot, the armour's helmet share and pellet pieces,
the dummy's report, the gore kind, decap's and limbs' Quad pops, limb wear, the wounds, the knockdown's shotgun rule,
the debug prints. No behaviour change intended.

**Step 2's check** (removed in step 3): at each of those consumers, the record against the old globals (zone,
validity, point; pellets, their parts, their mean point and direction), "hitcheck:" lines. Over agrunt 1 and 12,
agarmor 1 and 5, arena 1, decap_test.sh live/pop/corpse/refuse and the covering baseline run (agrunt sample, armour,
limb_wear, ragdoll_*, melee_phase, judo, eval, shot_shape, arena_1): **0 mismatches**; 13 of the 15 consumers were
reached (not reached: the highlights' and the Quad pops', which read the same expressions).

**Disagreements between the consumers (kept as they were; his call, decision "4 and 6: the disagreement lists"):**
1. A blast's single "part": the flinch takes the head if half the pellets, else the legs if most, else a limb if most,
   else the body; the ragdoll's death hit the same **without the limb** (a limb blast is a body hit); the highlights
   call a blast a headshot if **any** pellet struck the head; the corpse's positional factor weights each pellet's
   multiplier (no single part). One rule would be `VR_Hit_PelletPart(TRUE)` everywhere.
2. The armour's helmet share (vr_agarmor.qc) reads `hit_zone == 1` **without** checking the record is this target's
   this frame (as `vr_hitpart` was): an earlier shot's zone can count. The fix is `VR_Hit_Zone(targ) == 1`.
3. The stagger's head test ignores a blast's pellets (a head blast staggers as a body hit) and falls back to the top
   fifth of the box; decap's head test (VR_Decap_OnHead: its own sphere, grown for melee) is another head again.
4. The debug prints (T_DamageImpl's) take the zone if it is this frame's, whatever its target.
5. (Changed by the record, not reached by any test:) the blast's mean point for the flinch is now the blast's own
   (reset at each FireBullets); before, two blasts on one target in one frame (both hands' shotguns) were pooled.

**Left for part 2 (the review's estimate: 2-3 more rounds):** `vr_hitkind`/`vr_hithand`/`vr_hitpos`/`vr_hitdir`/
`vr_hit_nobleed` into the record behind `VR_Hit_Begin`/`VR_Hit_End` (24 VR_SetHitKind sites); the geometric deciders
(VR_Decap_OnHead, VR_SmallGib_Where, vr_wounds' and vr_carry's inline rebuilds, the 3 ragdoll-piece lookups, the
engine's `regionOf`) onto `hit_zone` + a bone; `VR_Hit_Class()` for the 9 kind classifiers and the 4 bleed predicates,
and the impact table's hit kind (VR_HIT_*) from it; the two T_Damage bypasses (vr_chainsaw.qc, vr_crates.qc).

**Verified:** build 0 warnings. Covering cases vs baseline.tsv after step 3: 0 of 21 changed (melee_phase passed; it
failed once on step 2's build, grunt_fast_on, and passes on next's runs about half the time). eval.sh: 163/171 as
next, the 3 takes that moved between step 2's run and step 3's differ in their replayed speed only (x1.28/x1.29), not
in zone or damage rules. A/B bench (LTO, one exe, two progs.dat: A = the base f4235be41, B = the record;
`scratch/ab_hit.sh`; cpu busy ms): only one A and one B ran before his game started (the rest: no result):
combat_48 A 2.573, B 2.424; armed_16 A 3.086, B 3.476 (B's run had a 58 ms hitch: max 58.43). One pair is noise;
**the ABBAABBA bench is still to run** (`bash scratch/ab_hit.sh hitrec2 ABBAABBA` with his game closed).

**Test in VR:** shotgun blasts at heads, legs and arms (flinch, ragdoll fall, highlights' headshot), the armour helmet,
the dummy's report at the firing range: all as before.
## Nail pistol: Full Slide the default, steel back end, screen on the slide, open when empty, a louder rack (branch `agent/npslide3`)

His words (2026-10-30): the moving-slide model is the default, the old one archived; bring back the old model's
silvery/steel back end, its colour less uniform; the ammo screen moves with the slide (an option, on); dropped and
holstered guns with the slide open look it; the automatic re-rack needs a strong click or clack.

- **Default:** `vr_nailpistol_model` 1 (Full Slide; a default edit, no migration). Classic archived: its row moved from
  Combat > Weapon Damage to Debug > Tests > Nail Pistol > Nail Pistol Model (Archived); the cvar still selects it.
- **Paint** (`build_nailpistol_slide.py`, rerun on the shipped file, deterministic: two runs byte-identical): the slide's
  and frame's texels behind x -2.4 (the classic rear housing's place) take the classic skin's texel at the same point of
  the housing (`classic_sampler`: its nearest face facing the same way, barycentric st), the serrations and worn edges
  drawn over them. The rest: wider mottle (`MOTTLE` 0.76/0.36/0.18, was 0.88/0.16/0.12), patches of bare steel where the
  rust is thin (`STEEL`, `STEEL_WEAR` 0.45), `SPECK` 0.75 (was 0.6). The new rows' luminance spread 25.2 -> 28.5 (the
  classic skin's 32.5); mean colour 61 42 29 (was 63 44 29).
- **Ammo screen rides the slide:** `vr_nailpistol_screen_ride` 1 (Combat > Weapon Damage > Nail Pistol Screen Rides
  Slide, under Slide Travel). The screen's anchor (-1.4 1.3 4.6) binds to the classic receiver (fixed); in the full-slide
  look (its left wall) it now moves by the slide's drawn travel each frame (`nailslide::screenShift`: the hand's slide
  place, else locked/frame 1; `view::screenPosition` for the held, holstered and lying guns' screens). Classic: fixed.
- **Open when empty, out of the hand** (vr_nailslide.cpp): holstered (`holsterLocked`: reloading on, its clip 0 or its
  magazine out; off, your nails spent) drawn as frame 1; lying about (`propLocked`: a weapon prop's networked clip 0 or
  no magazine) drawn locked back whatever its frame. Armed grunts' props as before (their server frame).
- **Rack:** `vr/nailpistol_rack.wav` remade (`make_nailpistol_sounds.py rack`): the auto pump's recorded click and slam
  (CC0, CREDITS.md) over the first synthesised slam, 44.1 kHz, its slam at 93 ms (the drawn slide home; 40 ms after the
  magazine-in sound's peak, so the two are heard apart); loudest 20 ms 0.70 RMS (was 0.31; reload_mag_in 0.57). Played
  on CHAN_AUTO (it was on the hand's channel, which the next sound there cut). The debug line names the reload mode.
- **Tests:** `vr_np_test 1` 35 passed, 0 failed (was 23): the screen 1.500 units back locked (travel 1.5) and 0 home
  after; holstered and lying (empty and loaded): 101 each (`vr_np_probe` makes the engine write `vr_np_seen_*`); the
  rack once in each of reload modes 0-3. New `vr_np_test 6` (Debug > Tests > Nail Pistol > Empty Nail Pistols:
  Holstered and Lying). Not verified: the rack at the mixer (headless `snd_show` lists no channels, the capture command
  printed nothing in the mock run).
- **Eyeshots** (low-impact runs, small eyes: not for judging the paint): qvr-kit/scratch/npslide/
  `npslide3_hand_closed_open_holster.png` (closed, closed tilted, locked open; the body and hip holsters),
  `npslide3_hand_open_full.png`.
## Surface impact sounds (branch `agent/impsnd`; his ask: the bullet impacts' sounds, at the surface, by its type)

A bullet's or a nail's hit on the world (cl_tent.c TE_GUNSHOT, TE_SPIKE, TE_SUPERSPIKE: VR_ImpactTempEntity) now also
sounds of the surface it hit, from the surface (S_StartSoundPitch at the hit's surface point, ATTN_NORM like Quake's
tink). The table's new world column `Row::surface` + `surfaceGain` (vr_impacts.cpp); the QC's `sound` column
(impactsound) is untouched, so a crate's or a thing's hit keeps its output. Bullet: stone a dull thud with a stony crack
and grit after, dirt a duller low-passed thud, metal Quake's ric1-3 over a light metal ring, wood a hollow knock and a
splinter's crack (gain 0.6, metal 0.55). Nail (0.42) and super nail (0.55): a shorter, higher, lighter thud or knock
under Quake's own tink, which still plays with every nail; none of ours on metal (that tink or ricochet is metal's own:
no doubling) and none for the bolt's beam (its hum). Three takes of each, made from the game's sounds as they load
(vr/derived/impact_*.wav: VR_SndLayered asks impacts::layered after the gunfire's; prewarm step "impact sounds"); each
hit a take at random, pitch 0.93-1.07, volume 0.82-1 x. Cap: at most 2 a frame, and none within 64 units of one played
less than 0.12 s before: a shotgun blast on one spot is one sound (two if its pellets land far apart), a super
nailgun's stream every other nail. Setting: Sound > Gunfire > Surface Impacts (`vr_gunsfx_impacts` 1, volume 0-2, 0
none; off with vr_gunsfx 0). Debug: `vr_debug_gunsfx 1` prints `impsnd <kind> on <material> <sample|none|capped> ...`;
`vr_impact_test <kind> [count] [material] [spread]` (spread is new); Debug > Tests: Impact Sounds: Six Pellets Spread.

Verified: `python Misc/quakevr/gunfx_test.py <agent> --only impactsnd` 21/0 (with -Sound: the derived sounds made, len >
0): 4 materials x 4 kinds each its own sample (or none: metal nails, bolts) 3 units from the hit, volume and pitch in
range; 12 pellets on one spot 1 played 11 capped; 3 pellets 70 apart 2 + 1 capped; a real shotgun blast 6 hits, 1
played; a real SNG stream 11 hits, 6 played (no two within 0.12 s and 64 units). Baseline cases vs baseline.tsv: nail_1,
np_1 (23/0, baseline 21/0: its newer checks), rv_1, flare_1, ammocolor_1, swim_sound: all PASS. Build: 0 warnings,
checks clean. (vr_gunsfx_test 19/1: "a gun held in the main hand (slot 16)", its setup's held weapon, not this change.)

## Save Default also saves the config and tunables.cfg (agent/savecfg)

His words: make the "save defaults" shortcut also write the tunables and config cfgs, so a crash loses nothing. The
menu's corner button Save Default (second press), Debug > Save My Settings as Defaults and the console's
`vr_propose_defaults` all go through `proposedefaults::write`, which now first calls `Host_WriteConfiguration` (the
one path quit and a game change use: the sparse ironwail.cfg, then tunables.cfg with only the changed constants;
nothing under -noconfigwrite/-evalcopy or a config not loaded), then prints `Saved your settings to disk: ironwail.cfg
and tunables.cfg` (a release: ironwail.cfg only), or a yellow "not saved" line. `Host_WriteConfiguration` and
`Host_WriteConfigurationToFile` now return whether they wrote; `VR_TunablesWrite` returns 0 on a failed write.

Test: `Misc/quakevr/savecfg_test.sh <agent>` changes vr_hull_width and a tunable (random values), runs
vr_propose_defaults, reads both files back from disk in the same run (`vr_config_test hasline <file> <word>... <value>`,
new) and is killed on the verdict, never quitting: 2/0 (in-game 4/0; a negative run without the save fails as it
should). config_sparse_test.sh 16/0. tunables_test.sh: run 1 38/0, quit's write and tune_promote pass; its run 2
(read at start, tune_reset removing the file) failed because run.ps1 empties tunables.cfg before every run; fixed:
its run 2 now sets QVR_KEEP_TUNABLES=1 (run.ps1 then keeps the file), and both tests leave tunables.cfg empty at the
end.

## The fists' guard in game as the Pose Studio shows it (branch `agent/enfguard`)

His report: an unarmed enforcer's guard posed with the fists at mouth level in the pose editor, but running at him in
game the fists were at its brow and crossed a little.

**Why they differed.** The pose keys are blended by the pose's weights (`VR_AGK_Keys`): the stance (Carry `1 - wa`,
Aim/Guard `wa`), and the Run key's share (`vr_agp_runw`) laid *on top* of whichever stance. The studio previews each key
alone: its Guard (Aim) key standing (`VR_AGK_Hold`: the stand frame, `runw` 0) and its Run key carried (no enemy:
`readyw` 0, the fists hanging). So each Run key is authored as a change on the carried stance; the fists' Run key raises
the hanging fists toward a guard (shipped `100020`: right +9.4 up and +9 across, left +10.7 up and -4 across; his own
now +16.9/+18.2 up). In game a fists holder runs only at an enemy, always in its guard (`readyw` 0.75, `wa` 1), so the
Run key was added on the raised guard: the fists up by another 9-18 units and pulled across each other. No other cause
was found (no clavicle, aim line, arm clear or dequantisation effect: with the Run key off the drawn guard is the
studio's within 1.3 units).

**The fix.** `vr_agrunt_run_key_stance` (1, new; 0 the old way): the Run key's share is `runw * (1 - wa)` (and the
pickup's `1 - wp` as before), the carried stance's only, as the studio previews it; running aimed or in a guard shows the
Aim/Guard key alone. Every class (a gun's Run key, a few units, no longer laid on its aim either); the off hand's mode
already took Aim's over Run's.

**Measured** (test 90, the drawn fists' middles less the head's middle, model space; the studio's view: frozen, held in
Aim by the studio's own path `VR_AGK_Hold`; the game's: 72 frames running at you in its guard):
- shipped keys: before 12.45 / 12.01 units off the studio's (right / left, means; the game's fists 2.5-3.8 *above* the
  head's middle against 5.5-6.3 below in the studio; crossed in all 72 frames); after 1.43 / 1.39 (worst frame 2.49,
  never crossed);
- his keys (his `vr_agrunt_poses`): before 10.36 / 11.91, after 1.27 / 1.28 (worst 2.35).
The 1.3 left is the run frames' own hand carried by the anchor (the guard placed from the animation's hand: the run's
sway kept on purpose) and the run's bob.

**Test 90** (`vr_agrunt_test 90`, `QC/vr_armedgrunt_guard_test.qc`; Debug > Tests > Armed Grunts: Fists' Guard as
Posed): checks each hand's mean within 2 units of the studio's, each frame within 3, never crossed; ends on `agtest:
test 90 over`. New for it: `weapondrawnpose(e, side, 29)` (the drawn hand less the head, `agarms::drawnHandFromHead`) and
QC `vr_agk_test_hold` (the studio's hold without the studio).

Checklist line: "An unarmed enforcer running at you holds its fists where the Pose Studio's Guard key shows them (mouth
level, not crossed)."

## Armed humanoid disarming, part 1: the tug's pull, two hands, a downed one's tug, projectiles (branch `agent/disarm1`)

His feedback (2026-10-30): no amount of pulling disarms a standing or tugged grunt; two hands do nothing more than one;
nails and the nail pistol never shoot the weapon out. Plan: `qvr-kit/scratch/disarm_plan.md` (items 2a, 3a/3b, 4; the
blow/parry order, item 1, and the grab arbitration, 2b/2c/3c, wait for agent/hitrecord and agent/grabquery).

- **Test 31's pull: the mock, not the physics.** `vr_mock_hand_lift` and `vr_mock_hand_pull` moved only a mock hand
  placed with `vr_mock_hand_to`; test 31 never places one, so its "pull" never moved the hand and its tug read the still
  hold. Bisect on today's harness: 231 N at 1c58a5435 (ROUND22's 1454 N does not reproduce), 231 at 670a9305b, 148 at
  63ff7f6e7 (posed arms), 84 at 09c84b4df^ and 09c84b4df, 72 at a35283c8b: the still hold's noise, never a pull. Fixed in
  `vr_backend_mock.cpp`: the lift and pull apply to the standing pose too; `vr_mock_hand_pull_off` pulls the off hand.
- **The measure** (`vr_agrunt_tug_measure` 2, `vr_agrunt_tug_stiffness` 1500 N/m, tunables). With the hand really pulled,
  the held limb follows it within 2 cm and a slow pull drags the whole active ragdoll (its pelvis pin gives at ~150 N),
  so neither the joint force nor "controller from the held point" reads a steady pull. `ragdolltug` what 3 is the tug's
  stretch: the hand's flat distance from where the grunt stands its ground (its pelvis pin's anchor), beyond the grab's.
  The pull is the larger of the joint force and stretch x stiffness. Test 31: the still hold 222 N (under 270), the
  0.6 m/s pull won in 0.47 s; test 92: 0.4 m/s won in 1.18 s (stretch 234 N at 0.5 s, 506 N at 1.0 s).
- **Two hands** (`vr_agrunt_tug_second_grab` 0: joins; 1: tears it free into the first hand). The same player's other
  hand's grab adds a second hold (engine: no re-pin, no stagger reset); its pull adds (`vr_agrunt_tug_two_hand_mult` 1),
  thresholds x `vr_agrunt_tug_two_hand_ease` 0.75 (the old tug: its time, the larger stretch). Either hand letting go
  keeps the tug on the other (`ragdolltug` what 4; what 2 ends both). Test 92: two hands 0.94 s vs one 1.18 s (0.80x; the
  mock's off hand reads little stretch, so the eased thresholds win it).
- **The free fist** (`vr_agrunt_tug_strike` 1, `vr_agrunt_tug_strike_swing` 0.6): in a tug the free fist's deliberate
  strike at 2.7 m/s tears the weapon into the holding hand (test 92: 2 m/s kept, 3.5 m/s yours). Any `VR_AG_Disarm` during
  a tug (a body blow by its grip) does the same instead of flinging it. `VR_AG_HandStrike` is split into
  `VR_AG_HandStrikeSpeed` (any share of the need) and the disarm, for item 1 to reuse.
- **His decision: a staggering or downed one still needs a tug** (`vr_agrunt_tug_downed_scale` 0.5; 0: at once, as
  before). The grab starts the old tug (distance and time, `vr_agrunt_tug_time` x 0.5), its knockdown going on; up again
  mid-tug it struggles as a standing one. A tug that goes down keeps going eased. Kept at once: stunned by a parry, badly
  hurt, and the physical tug whose grunt goes down ("it went down"). Test 92: staggering 0.89 s, knocked down 1.39 s at
  0.6 m/s; held still 0.4 s, kept.
- **Projectiles** (`vr_agrunt_shotdisarm_proj` 1, `_proj_chance` 0.2 at `_proj_ref` 9, `_proj_near` 4, `_proj_max` 0.8;
  rows under Disarming: Shooting It Out). `launch_spike` tags every nail (`.vr_ags_kind`), so the nail pistol's
  (`VR_NailPistol_SpikeTouch`) and lava nails count; each projectile on or near the weapon or hand rolls once, its
  holder's body not first. Test 93: nail 0.20, super 0.40, pistol 0.27, lava 0.20, rocket 0.80, forearm the same, 3 units
  off a quarter, 6 off never (40 or 12 shots each, the chance exact, the rate within 3 sd). Test 50's nail cases now
  expect the per-hit roll (3 nails 0.3-0.7, one far nail 0.1-0.35).
- Tests: `vr_agrunt_test 92` (vr_armedgrunt_twohand_test.qc, 14 checks), `93` (in vr_armedgrunt_shot_test.qc, 20).

Checklist lines:
- Grab a standing armed grunt's gun and pull back steadily (not a yank): within about 1-2 s the gun is yours.
- Grab it with one hand, then the other on the gun too, and pull: it gives sooner than one hand; let go with one hand:
  the tug goes on.
- Hold its gun with one hand, punch the gun with the other fist: the gun ends in the holding hand.
- Shove a grunt into a stagger (or floor it) and grab its gun: it is not yours at once; a short pull takes it.
- Shoot a grunt's gun with the nail pistol and with nails: some shots knock it out (about 1 in 4-5 on the gun).

## Show Disarm Volumes: the disarm tests' shapes drawn (branch `agent/disarmviz`)

His question: is there a debug view of the disarm volumes (his hands, the enemy's hands and weapon), with what is
hovered or grabbed, alive or an active ragdoll alike? There wasn't: `vr_debug_hitzones` draws the damage regions,
`vr_debug_carry` the grab test on carried props (not armed monsters' weapons), `vr_debug_melee_blades` the blow lines,
`vr_agrunt_shotdisarm_debug 2` sparks along the shot volumes, `vr_debug_ragdoll` / Show Physics Shapes every body.

**Debug > Show Disarm Volumes** (`vr_debug_disarm`, 0; 1 on, 2 also prints `disarmviz:` hits and hover changes). The
QC (QC/vr_disarmviz.qc `VR_DV_Frame`, from StartFrame: before the weapons' thinks test them) sends each held weapon's
volumes computed by the tests' own functions (builtin `debugdisarm`): its line (VR_AG_WeaponLine) with the strike and
parry reach (5, orange), the shot volumes of the weapon and of its hand and forearm (VR_AGS_HandLine by
`vr_agrunt_shotdisarm_parts`/`_size`, magenta and cyan; near misses faint), the grip spheres a body blow disarms within
(VR_AG_Hit: 10, a limb hit's 16, red), on a ragdoll the limb its weapon rides (VR_AG_RagProp's bone, white: box3d
`ragdollPartDraw`), a tug (the hand to the grip, green, and a progress bar), its state label (`debugdisarmnote`: a
tug's share, or what a grab does now: at once and why, or a tug), and each player's strike line (hand to muzzle, white)
and blow line (VR_Blow's, blue). The engine (Quake/vr/vr_disarmviz.cpp) measures each fist against each of those weapons
as the grab does (`physics::armedGrabGap`: handOn's armed branch without its grip test, sweep included): the fist's
spheres grey, yellow with the weapon when a grip would take it now (the gap and allowance in the label). The hits are
marked 2 s where they landed (VR_DV_Mark, guarded by `vr_dv_on`: from VR_AG_Disarm, VR_AG_Hit (disarm or body blow and
its distance from the grip), VR_AG_HandStrike, the parry (parry or disarm), VR_AGS_Add (shot) and VR_AGS_Stopped
(blocked)). Off: one cvar read in QC, the engine returns at once; fixed arrays (no allocation per frame). Flat: the
lines over the flat view (vr_ragtool.cpp's overlay); its labels only in the headset.

Test 97 (`vr_agrunt_test 97`, QC/vr_disarmviz_test.qc): standing, hovered (`disarmviz: main hand takes weapon 256 (gap
-0.91 cm, allowed 5.00 cm)`), tug, active ragdoll with a pellet's line through its weapon: 4 passed, 0 failed (VR and
flat); eye images in qvr-kit/scratch/disarmviz. Test 88 with it off: unchanged (3 passed, the known grunt topple FAIL).
Seen with it: on an active ragdoll a pellet aimed at the weapon's middle counted on its **hand** (VR_AGS_HandLine
builds the forearm from the standing pose's shoulder, not the ragdoll's arm): a lead for the disarm work.

Checklist lines:
- Debug > Show Disarm Volumes on, walk up to an armed grunt: its gun outlined orange; put your hand on the gun: your
  fist and the gun turn yellow; grip: green with a progress bar; shove it over: the white limb under its gun, the
  shapes stay on it.
## FBT regression check (agent/fbtfix)

The author (2026-10-30): with full body tracking the elbows and the body are way off; FBT off is playable. Checked:

- Settings tiers: no body tracking, avatar or calibration setting became a constant or an unsaved test switch while his
  config set it (`vr_bodycal_*`, `vr_body_tweak_*`, `vr_bodytrack`, `_arms`, `_arm_fit`, `vr_body_elbow_*`,
  `vr_height_calibration` are options and load; the constants `vr_bodytrack_torso/_legs/_confidence/_smooth/_fade/
  _arm_fit_stretch` and `vr_torso_*` keep the old defaults). No body default changed since 2026-10-19.
- Headless: his capture (bodytrack_dump.csv, 2026-10-17, VDXR) replayed with his whole vr_ config on a1fd90743 (before
  the refactor waves) and on origin/next 2234fc41f (no-LTO and ThinLTO): every solved joint (pelvis, neck top, shoulders,
  elbows, wrists, weights, elbow fit) the same within 0.14 units (3.5 mm) over 999 frames, each within 1-2 units of the
  tracked joints. The geometry helpers A1 merged (quakeFromTracking, wrap180, smoothstep, approachExp, safeNormalize)
  are the same functions; the B3/L splits (avatar legs/climb, view body, hands phases, OpenXR frame) are pure moves
  (body joints still located right after the hands, `bodytrack::place` in the same order). His previous calibration
  (vr_bodycal_undo) instead of the current one moves the joints under 1 unit.
- So no code change in the replayable path moved the solve. Not covered by a replay: the live OpenXR joints (the
  runtime's own body calibration), the drawn mesh's skinning, the legs and feet. Next: a `vr_bodytrack_dump 20` taken
  while it looks wrong, replayed here on both builds.

Regression test: `Misc/quakevr/bodytrack_solve_test.sh <agent> [--update]`: 300 frames of that capture
(quakevr/scenarios/bodytrack/take.csv) replayed with his body settings, 240 dumped frames against
quakevr/scenarios/bodytrack/solve_ref.csv (0.5 units per joint, 0.02 per weight); verdict line
`bodytrack_solve: N passed, M failed`.

## His tweaks of 2026-10-30 as defaults (branch `agent/promote1030`)

His "I've tweaked many values, please make them the defaults." Source: his `quakevr/ironwail.cfg` of 2026-10-30
(sparse, configVersion 218; no `tunables.cfg`: no tuning constant changed). The game loaded the defaults, then a copy
of his config, and `vr_config_test dumpfull` wrote each (23,072 settings): 219 vr_ settings differed. The full list,
promoted / personal / doubtful: the kit's `scratch/promote_1030.md`.

- **Promoted** (an edit of each default; no defaultChanges row, as the sparse config wants): 38 settings in
  vr_cvars.inc (armed grunts' armour, tug, strafe turn, parry disarm, replacement chances; ammo low fraction and pouch;
  every gun-smoke, spark and wisp setting; relight AO 1.2 and bounce on; the revolver's rod and spin; shore foam; the
  worn backpack's rip force); 100 weapon slot values in vr_weapons.inc (recoil cm / deg / s of 27 slots and 18 Haptic
  Punch, the revolver's flash and tracers, the AD stream gun's rumble and shake, the nailgun's third hotspot, the nail
  pistol's two-handed mode and text panel, the AD plasma gun's text panel, the Widowmaker's stock pitch; five of them
  on the slot's QVR_WEAPON_UNINHERITED line, which is read last and wins); the armed humanoids' 120 pose keys, 16 IK
  stretch rows and 3 grips (pose_defaults.py --cfg --write).
- **Weapon grips** had no default table since Refactor C1 (pose_defaults.py stopped at it): vr_aggrip.cpp `defaults[]`
  again ({kind, weapon id, hand, ...}; `defaultOf` finds a weapon's row, on top of its kind's Every Weapon one;
  `vr_agrunt_grip_dump all` prints them). His two: the grunt's crossbow and the enforcer's chainsaw, second hand.
- **Recoil of a config before vr_wofs_version 74:** a slot whose old recoil was the shipped one of then (and the
  globals 1.5 / 4) now keeps today's defaults (his) instead of its old feel converted (`takeRecoilUnits74`), as a
  setting left alone does; a changed slot converts as before.
- **Not promoted:** personal (body, calibration, elbow, tracker, his avatar's hands, nails, gloves and voice, toolgun
  and its holsters, rig studio, recording, voice notes, foveation, spectator and window view, XR runtime, menu state);
  doubtful, for him: vr_ad_upgrades 2, vr_agrunt_repl 1, vr_worn_backpacks 1 (feature switches for every map),
  vr_reload_mode_revolver 3, vr_relight_minlight 64, vr_relight_reload 1, vr_ghost_hands 1 (fade 9),
  vr_autosave_list_all 1, his six held-object slots with only an ID (they act as no slot), and float noise
  (muzzle_x/z_01, w_com_35/50).
- **Checks:** build 0 warnings; his config loaded before and after: all 23,072 values the same (0 differ); the
  defaults now differ from his in 81 settings, all of them the skipped ones; his 120 pose keys equal the shipped rows
  (to 0.005). config_sparse_test 16/0 (the default config and his). vr_recoil_test: the conversion checks now count
  27 slots at his values and check the conversion's curve on the converted values (not the defaults); a converted
  slot at the shipped recoil of then must hold today's defaults; the stream check takes slot 30's 1.25 / 1.75:
  69/0 (it was 65/4 before those changes). tunables_test 20/0. Full baseline (`run_baseline.sh harness --no-bench`)
  against baseline.tsv: recoil as said; nail_1 7/0 -> 11/0 and np_1 21/0 -> 35/0 (checks added on next since the
  baseline; nail_1 8/3 once in a loaded lane, 11/0 alone); agrunt_31, 36, 65 FAIL -> PASS (rerun: the same); reload
  111/4 once, 112/3 on the rerun (as the baseline); agrunt_76 (Aim Behind Cover) 25/0, 24/1, 22/3, 23/2 over four
  runs, a different gun and cover failing each time (a few shots a case: noise); the other flaky ones as the run
  listed them.

Checklist:
- Fire every gun: recoil, smoke, sparks and wisps as you had them (shotgun 10 cm / 12 deg, revolver 14.6 / 21.9).
- Armed grunts (Combat > Armed Grunts): poses, grips (the crossbow's and the chainsaw's second hand), armour and tug as
  you left them, with your own vr_agrunt_poses / grips / ik_stretch cleared (they now equal the defaults).
- Relit maps: AO 1.2 and bounced light by default.
## Kick cancel in the poses; two-handed recoil; no ghost hands from recoil (branch `agent/recoil2h`)

His words: the starter shotgun's new recoil "seems jittery" (frames still moving it); the grenade launcher "also seems to
have a few frames that still move the weapon, some vertices visibly move during recoil and the ammo screen intersects
with geometry"; a global and per-weapon multiplier reducing the programmatic recoil held two-handed; an option (on) for
the ghost hands not to appear from the programmatic recoil.

- **What jittered** (measured: `vr_debug_weaponfx 4`, a "recoiltrack" line a frame: the held gun's body as drawn, where
  its rest pose would be drawn, the recoil; shotgun fired at 72 and 90 Hz, fixed clock, and 250 Hz real time): the
  procedural curve is smooth (cl.time steps evenly; the drawn rest-pose body moves exactly as the envelope). What moved
  was the old cancel's leftover: one rigid motion a pose cannot undo each vertex's byte rounding (the fit's rms 0.5
  byte step, up to 3 steps, ~1 cm at the shotgun's muzzle; the grenade launcher's body rms 1.3 steps, the model
  squashing as it kicks), so the gun's vertices and its body's centre (up to 3 mm) shimmered, changing every 0.1 s pose.
- **The fix: the kick out of the poses** (`vr_kickcancel.cpp` ensure): the fit as before (at map load,
  view::prepareWorldWeapons; each frame for the held guns), then the model's poses in memory made again: the body's
  vertices are the rest pose's own bytes (and normals) in every kicked pose, a part's are its pose's put through the
  pose's cancel (re-rounded, its normal turned); the model uploaded again (its dequantised offsets made again). The
  matrix cancel (`byteCancel`, `applyDrawn`, the anchors' `d.kick`) is gone: the renderer, the anchors (muzzles, ammo
  screens, mounts, the hand on a pump) and the dequantiser read the cancelled poses. The originals are kept
  (`vr_weapon_kick_cancel 0` puts them back, the held guns' at the next frame); a model read again is found by its
  bytes (one pose compared a frame) and fitted again. The kick cache is never emptied (a model's entry is checked by its
  data). Drawn now: body == rest exactly (recoiltrack: body and rest identical every frame, shotgun and launcher).
- **The grenade launchers wholly at rest** (slots 5, 10, 13: the launcher, Armagon's proximity gun, the multi grenade
  launcher): their upper shells only lurch on the body (177-202 vertices; no mechanism), so every vertex of a kicked
  pose is the rest pose's: the ammo screen's anchor (on the shell) holds still; with the models' own kick it moved 49
  byte steps. The other guns keep their parts (the nailguns' barrels, the laser cannon's sliding barrel with its screen
  on it, the flashes). AD's grenade launcher is not a listed model (its own frames).
- **Two-handed recoil** (`weaponfx::twoHandRecoil`): Weapon Effects > Two-Handed Recoil (`vr_recoil_two_hand`, 0.6,
  0-1, to 2) times Weapon Offsets > Effects > Two-Handed Recoil (`vr_wofs_fx_recoil_2h_NN`, 1, 0-2, to 5), blended in
  by the two-handed grip's transition (`twohand::support`): a shot's kick back and muzzle rise times it and its Return
  Time times it (`shotKick`; the 35 ms kick up as before); the haptic punch's spike and tail follow (vr_gunsfx.cpp
  Punch.share). The heavy-weapon kick (vr_weight.cpp) already had its own two-handed share: unchanged.
- **No ghost hands from recoil** (`vr_ghost_hands_ignore_recoil`, 1; Settings > Ghost Hands: "No Ghost from Recoil"):
  the ghost's distance is measured against the real controller kicked as the recoil kicks the drawn hand
  (setupGhostHands; the ghost is still drawn at the real controller when something else holds the hand off).
- **Tests**: `vr_kickcancel_test` (also in `vr_recoil_test`) plays each listed gun's firing frames at 10 Hz drawn
  through the anchors' path at 72 and 90 Hz (the renderer's blend): every body vertex (every vertex of a launcher)
  within 0.02 byte steps of rest, its centre's step between drawn frames within 0.02, the launchers' ammo screen anchor
  within 0.02: 71/0 (all 0.000; with `vr_weapon_kick_cancel 0`: 35/36). `vr_recoil_test` 104/0 (was 69): the drawn kick
  sampled at 72 and 90 Hz on its curve (0.0000 of the peak off) and rising once, falling once; two-handed at 0.6 x 0.5:
  peak 1.680 / 5.600 deg, 1.733 / 5.775 cm, back in 0.0945 / 0.3150 s (0.300 each). `Misc/quakevr/ghost_recoil_test.sh
  <agent>`: the shotgun at Recoil Amount 3 fired once ignoring the recoil (25 kicked frames, no ghost: 0.29 cm apart)
  and once counting it (the ghost shows): 3/0. `vr_gunsfx_test` 24/0 (-Sound). Covering cases vs baseline.tsv (items 1
  and 3): collectfx, shot_shape, grip_state, reload 112/3, reload_* as the baseline; recoil 99/0 (was 9/0: checks
  added). No per-frame cost to A/B: the draw lost its matrix product, the held guns gained one pose's memcmp a frame.

Checklist:
- The shotgun, super shotgun and rocket launcher fire with the gun's body perfectly still in the hand (no shimmer of
  its edges or lighting through the kick); only the procedural kick moves it.
- The grenade launcher (and the proximity gun, multi grenade launcher) fires with nothing on it moving; its ammo screen
  never goes into the gun.
- Debug > Weapons: "Take Out the Models' Own Kick" off and on swaps the models' own kick back and forth at once.
- Weapon Effects: Two-Handed Recoil 0.6: a two-handed shotgun shot kicks visibly less and settles sooner than one-handed,
  the haptic punch lighter and shorter; 1: the same as one-handed. Weapon Offsets > Effects: Two-Handed Recoil per gun.
- Ghost Hands on: firing a strong gun (the shotgun at your 17 cm) shows no ghost; "No Ghost from Recoil" off shows it
  flash with each shot.

## Armed humanoid disarming, part 2: a deliberate strike on its weapon or hand disarms (branch `agent/disarm2`)

His words (2026-10-30): a well-aimed hit or throw (fist, gun or prop) on an armed grunt's weapon or hand parried or hurt
it, never disarmed it; precise, deliberate strikes (in slow motion too) should knock the gun out. Plan item 1
(`qvr-kit/scratch/disarm_plan.md`): the parry ran first and took every blow on the weapon whenever the grunt *could*
parry, raised or not; the disarm test was one point per frame and ignored a carried prop's size.

- **One decision, `VR_AG_BlowOnWeapon`** (vr_armedgrunt_parry.qc), from `VR_AGPar_Blow` (the melee's damage: a blow
  that met the weapon before the body) and `VR_AG_StrikeFrame` (the weapon's think: one that met only the weapon; it
  replaces `VR_AGPar_WeaponFrame` + the `VR_AG_HandStrike` loop under `vr_agrunt_strike_first` 1, the default; 0 is the
  old order, kept whole). Not a deliberate strike (`VR_AG_StrikeSpeed` 0: open hand, jolt, under the disarm swing): the
  parry's own rule as before. Deliberate and its parry not raised for it (unable, caught out, not up yet, pose under
  `vr_agrunt_strike_raised` 0.5): disarmed along the blow (`strike: disarmed (...)` under vr_agrunt_debug). Deliberate
  into a raised parry: roll `vr_agrunt_strike_beat` 0.4 x `_strike_precise_mult` 2 (within `_strike_precise` 4 units of
  the grip or hand line) x min(2, speed / 4.5): won, disarmed; lost, the parry (stamina as before). A blow disarmed on
  its way into the body still deals its body damage (as an unable grunt's always did).
- **Swept geometry, `VR_AG_StrikeMeets`**: the blow's points (VR_Blow, last pose to now, a held gun's whole line) at 4-16
  steps (every 2.5 units of their way), a carried prop's middle swept with the hand and widened by its size (min(16,
  mean size / 2)), against the weapon line and the hand + forearm line (`VR_AGS_HandLine`); plus the old hand-to-muzzle
  test. `VR_AG_HandStrikeSpeed` (the tug's free-fist strike) uses it too. Speeds are real time (unchanged), so slow
  motion works.
- **Throws**: `VR_AG_Hit` first asks `VR_AG_ThrowOnWeapon`: a thrown weapon, prop or gib that touched the body (its
  velocity possibly already bounced: then its way in is towards the body), its path one frame back plus its size, met
  the weapon or hand line: the momentum rule (`vr_agrunt_disarm_throw`), not the damage gate. The think-time throw check
  stays for throws that never touch the body. Body blows by the hand: `VR_AG_Hit` measures to the hand line too.
- **Ragdolls** (agent/disarmviz's lead): `VR_AGS_HandLine` on a ragdoll lays the forearm from the grip towards the middle
  of the limb its weapon rides (`ragdollpoint(g, vr_ag_rb - 1, '0 0 0', 1)`), not the standing pose's arm.
- Show Disarm Volumes marks the strike's and the throw's disarms (`VR_DV_Mark`, at the struck point).
- Settings: `vr_agrunt_strike_first` (Combat > Armed Grunts > Disarming: Strikes on Its Weapon Disarm); tunables
  `vr_agrunt_strike_beat`, `_strike_precise`, `_strike_precise_mult`, `_strike_raised` (Armed Grunts group).
- **Test `vr_agrunt_test 94`** (vr_armedgrunt_strike_test.qc; Debug > Tests > Armed Grunts: Strikes on Its Weapon
  Disarm), 16 cases, each a fresh shotgun grunt held still with its parry state set and the hand mocked as the tracker has
  it: fist on the barrel unaware / caught out: disarmed; caught out with strike_first 0: parried; raised, beat 0:
  parried, beat 1: disarmed; on the forearm (3.7 units off the weapon line): disarmed by the hand part; open hand, 2 m/s
  push: neither; held shotgun's far end (hand 22 units off): disarmed; carried 24-unit crate, fist 13 units off (the fist
  alone does not meet): disarmed; gun thrown across the barrel: disarmed; gun bounced off the body by its forearm (5
  damage, under the disarming blow): disarmed; melee punch on the legs: its full 12 damage, armed; through the barrel into
  the chest, caught out: disarmed, damage not cut; into a raised parry, beat 0: parried (5 of 20); fist at vr_timescale
  0.3: disarmed. 16 passed, 0 failed. Ends on `agtest: test 94 over`. Test 97 gained a check: the ragdoll's pellet
  through its weapon counts on the weapon (5/0).
- **Covering** (run_baseline agrunt + suite, `agrunt|parry|melee_phase|judo`, 89 cases): changed 31 and 36 FAIL -> PASS,
  65 0/6 -> 5/1; 66 PASS -> FAIL and melee_phase 7/0 -> 6/1 are flaky on next too (66 with `vr_agrunt_strike_first 0`
  failed 3 of 4 runs: the lying grunt drops its gun "as it fell", ground impact 5-12 m/s; melee_phase per the hitrecord
  notes passes about half the time). eval.sh --full: 163/171, as next.
- **The uppercut take (16.7 -> 13.2)** predates disarm1: eval.sh --full gives 13.2 on b2eca62bf (next before the
  disarm1 merge) and on origin/next alike (163/171 both; the same 9-15 rows differ from the baseline, by 0.1 noise
  except the uppercut and two older takes). Nothing to revert in disarm1.

Checklist lines:
- Punch an armed grunt's gun (or its gun hand) hard while it isn't guarding: the gun flies out; into its raised guard
  it is usually parried, sometimes knocked out.
- In slow motion, punch a grunt's gun out of its hand.
- Swing a held shotgun or a carried crate into a grunt's gun: knocked out. Throw a gun at its gun hand: knocked out.
- Punch a grunt's legs or chest away from its gun: it is hurt, keeps its gun.

## Armed humanoid disarming, part 3: an active ragdoll's weapon grabbed (branch `agent/disarm3`)

His note (2026-10-30): after a shove or any other ragdoll trigger, grabbing an armed humanoid's weapon out of its hand
seems impossible; the empty hand's grab sometimes misses the held weapon, and a grab on the weapon or its hand never
disarms however hard you pull. Plan: `qvr-kit/scratch/disarm_plan.md` item 2 (2b, 2c).

Found (test 91's first runs, before the fix): a lying grunt's grab at its grip or barrel started the tug but read no
pull (the test's pull, below); a grab on its hand or forearm held the limb ("limb held": the body dragged about, the
weapon never yours); a fist 5-8 cm off the gun took nothing or the arm. And the armed soldier's ragdoll weapon rides the
model's own hidden gun (rig bone 19 `weapon`, a loose piece welded to the part holding it in the rest pose: his left
**forearm**, `looseHolder`), so "its hand" is not the bone nearest the gun: the right hand drifts 20 units off it when
it lies.

**One arbitration, in the grab query (vr_grab.hpp):** a living ragdoll's armed weapon wins over its limbs.
- `physics::armedAllowance` (vr_physics.cpp): the fist's allowance from an armed monster's weapon's drawn surface (bias +
  `vr_agrunt_grab_slack` + `_leniency`, as before); on a living ragdoll at least `vr_agrunt_rag_grab_weapon` (8 cm,
  tunable; its row "Grab: Ragdoll's Weapon Within" after Grab: Moving Weapon). The weapon's own touch (handOn), Show Disarm Volumes'
  measure (`armedGrabGap`: it marks the 5-8 cm hover "takes") and the arbitration all read it.
- `physics::armedWeaponOver(body, player, hand, part)`: the weapon a hand's grab on a living ragdoll takes instead of the
  limb: the fist within that allowance (armedGrabGap's measure), or (`vr_agrunt_rag_limb_grab` 1, tunable cycle) the
  hand on a part `box3d::ragdollWeaponHold` names: the part the weapon rides (QC `.vr_ag_rb`, now a QVR_FIELD), the
  part holding that loose piece (`looseHolder`, moved out of its anonymous namespace) or the bone it rides, that arm's
  hand and forearm, or the other arm's with its hand within 4 units of the grip (a gun's second hand); never a torso
  part.
- `grabRagdoll` (box3d) asks it before taking a limb (it replaces the old `fistOnArmedWeapon` test there; foegrab keeps
  that one for standing grunts) and answers `RagdollGrabResult::Weapon` (QC `ragdollgrab` returns 2). QC
  `VR_Ragdoll_Handtouch` hands that grip to the weapon: `VR_AG_RagWeaponGrab` (of two weapons, the one nearer the
  hand) -> `VR_AG_HandGrab` (the old `VR_AG_PropHandTouch` body: empty hand, grip, speed, the dual wielder's swap, then
  `VR_AG_GrabBy`): the usual tug (part 1's measure; a staggering or downed one's eased tug, `vr_agrunt_tug_downed_scale`;
  two hands join). A hand already tugging that grunt's weapon takes no limb on its grip's later frames.
- **One grip window:** on a living ragdoll the weapon's own touch also takes the limb's window (`VR_Carry_GripPressed`,
  0.2 s or pressed this frame; `VR_AG_GripNow`); a standing grunt's weapon keeps its 0.125 s
  (`StartedHandGrabbing`): his tuned standing grabs (tests 36, 67-69) unchanged.
- `ragdolllimbs(e, 7, bone)`: the part holding loose piece `bone` (else `bone`): the test's "its hand".
- Debug: `vr_debug_ragdoll 1` prints `ragdoll: N hand H on its weapon (W): the weapon's grab`; 2 each weapon weighed
  (its gap and allowance, the part it rides, the hand's part, whether an arm holds it).
- Removed the dead `vr_walltorch_pull` tunable (vr_tunables.inc and its Tunables row; TESTING.md's mention), left by the
  busy-flag fold.

**Test 91** (`vr_agrunt_test 91`, QC/vr_armedgrunt_raggrab_test.qc): a fresh shotgun grunt toppled by a shove (lying
still) or staggered (about 0.3 s), the real grab (`+grabmain` with the mock hand on the point) at its grip, barrel,
the hand holding its gun, that hand's forearm and (lying) 5.5-7.5 cm over its barrel (the hand raised until the engine
reads that gap), then stepping back at 0.6 m/s; case 10 the main hand on its grip, the off hand on its forearm. Body
blows' disarm off meanwhile (`tune vr_agrunt_disarm_damage 9999`, put back a frame before the verdict: the mock hand's
jump onto the gun is a blow). Result: 9/9 won (each a tug, 1.1-1.7 s; two hands joined, 3.7 s), none "limb held", one
staggering case "no state" (up again before the hand was there); `disarmviz: main hand takes weapon 54 (gap 6.98 cm,
allowed 8.00 cm)` for the 6 cm case. Before the fix (the same test, first runs): lying grip/barrel STUCK, hand "limb
held", 0/5 won. (The test steps back rather than `vr_mock_hand_pull`: the mock's pull is along the head's back, which
after stepping in beside a lying grunt's gun ran across the tug's own way: stretch -26 units, never a pull.)

**Verified:** the covering cases against baseline.tsv (run_baseline.py: the agrunt group, ragdoll_grab, flash_grab,
walltorch_grab, carry, weapon_catch, grip_gap, judo; 93 cases): 3 changed, all FAIL -> PASS (agrunt_31 5/0, 36 4/0, 65
6/0), the new 90-97 pass; flaky-listed 63/72/76/86 and ragdoll_grab 18/1 ("grab rag enforcer", its known flake: 19/0 on
the rerun). eval.sh: canary 36/44, full 163/171 (the uppercut's 13.2, older than this branch, as part 2 notes). After
merging origin/next (preparry, agknight): 31, 36, 65, 67-69, 74, 91, 92, 94, 95, 96, 98 all PASS.

Checklist lines:
- Shove an armed grunt into a stagger, or topple it, and grab its gun (on the gun, or a few cm off it): a tug, not its
  arm; pull back steadily: within about 1-2 s the gun is yours.
- The same, grabbing the hand or forearm holding its gun: a tug on the gun (the arm is not held); pull: yours.
- Topple it, grab its gun with one hand and its gun forearm with the other: both pull; yours.
- Debug > Show Disarm Volumes on, hover a fist a few cm over a downed grunt's gun: yellow (takes) up to 8 cm.
- Grab a downed armed grunt's other arm, or its leg: that limb is held as before.


## Nail pistol: no flicker at the muzzle's back, a distinct steel back end (branch `agent/npslide4`)

His words (2026-10-30): some triangles on the back of the muzzle flicker, seen only with the slide drawn; the
steel/silvery back end more distinct, as in the pre-slide art.

- **The flicker:** the classic nozzle's back is a closed hexagonal cap (shared by both looks, x 7.19, facing back),
  hidden in either look until the full slide draws back past it. npslide2's ring (`nozzle_ring`, iron, closing what it
  took for an open back) lay on the same plane facing the same way: 12 triangles z-fighting with the cap's 4. The ring is
  gone (the cap closes the nozzle; the barrel passes through it). Nothing else: no triangle wound against its normal, the
  folds degenerate in frames 2, 3 and every blend past 3 (to 4.6, Slide Travel 3), no other same-facing coplanar pair
  with a new triangle (the slide's bottom on the frame's top face opposite ways: back faces culled).
- **Check in the script:** `check_faces` (build_nailpistol_slide.py) runs on every build in frames 2 and 3 and blends
  1, 2 and the most (4.6): folds, winding (Quake clockwise vs the vertex normal) and coplanar overlapping same-facing
  pairs (planes within 0.04 units, a 2D overlap test); it stops the build on a problem. With the old ring added back it
  reports 84 (the ring's 12 triangles against the cap's).
- **Steel back end** (behind x -2.4, the classic housing's place: the slide's walls, chamfers, top, end and the frame):
  the classic housing is about 1.5x its receiver's light (sampled: luminance 40 vs 26), and the full slide's front is as
  light as that housing (41), so the sampled texels alone did not stand out. Each texel now: its light lifted 1.2x
  (`REAR_GAIN`), its swing about the housing's mean cut to 0.75 (`REAR_FLAT`: less mottled), a cool steel hue
  (`REAR_TONE` 0.93 1.0 1.1, 30% of its own hue kept); the housing's bright bevelled rim on its outline (0.13 units), and
  a seam at x -2.4 (a dark groove in front, a lit line behind). Texels with no classic face near take the housing's mean.
  The rest of the paint, the classic frames and texels are unchanged (classic rows 0 changed, frames 0/1 identical);
  the script stays deterministic (a rerun on its output: byte-identical).
- **Eyeshots** (qvr-kit/scratch/npslide/): `npslide4_classic_closed_open_muzzle.png` (rows: classic closed, full slide
  closed, locked open (travel 1.5), the muzzle's back close up at travel 3; right side left column, left side right),
  `npslide4_rear_before_after.png` (full slide closed, before / after), `npslide4_thumbs.png`.
- **Tests:** `vr_np_test 1` fails on origin/next with the old model the same way (0 passed, 3 failed: "the trigger held
  0.55 s fired 0"): not this change; baseline.tsv's np_1 (21 passed) is older than the test's 35 checks.

## Armed humanoids: preventive and wind-up parries; parry by what you hold (branch `agent/preparry`)

His words (2026-10-30): an option (on by default) for armed humanoids to parry preventively when you get close and their
shove is on cooldown or they wield a melee weapon, for a while, so you wait it out or bait it and strike back; the
reactive parry kept, on by default; and when you raise a weapon or anything you can hit with overhead or to the side,
a preventive parry after a random delay, chance-based, so you can bait it. Lots of options. The coordinator's addition:
the mechanics depend on what you hold (a melee weapon: readier; a gun, a prop, fists: it waits for your wind-up and
reads it less reliably).

- **Its guard** (QC/vr_armedgrunt_guard.qc, class-agnostic: any armed humanoid with a weapon and `vr_agrunt_parry` on):
  idle -> reacting (a won roll, the delay) -> raised (its parry pose towards your striking hand, for the guard time) ->
  recovering (it can't parry at all: `VR_AGPar_Able` "its guard dropped (recovering)"; a strike on its weapon disarms, a
  blow lands in full) -> idle. While raised, its parry counts as raised for `VR_AG_BlowOnWeapon` (the strike-first
  roll, `vr_agrunt_strike_beat`) and the honest hit test puts its weapon in the blow's way. Raised or rising, it holds
  its attack (`vr_agrunt_guard_holdfire`, `VR_AG_CheckAttack`).
- **Preventive** (`vr_agrunt_preparry` 1; Combat > Armed Grunts > AI Behaviour, Parrying Your Blows): every
  `_interval` s (0.5), you within `_range` (64 units), it able to parry you, and its shove cooling down or off
  (`_shove` 1) or a melee weapon in its hand (`_melee` 1): a roll (`_chance` 0.5); delay `_delay_min.._max`
  (0.1-0.35 s), guard `_guard_min.._max` (0.8-1.6 s), recovery `_recover_min.._max` (0.6-1.2 s).
- **Wind-up** (`vr_agrunt_windup_parry` 1): your hand (a melee weapon, a gun, a carried prop, a closed fist; never an
  open hand) within `_range` (110 units), raised over your shoulders (20 cm under the eyes) by `_high` (0.2 m), or out
  to your side by `_side` (0.4 m) no further ahead than `_ahead` (0.15 m), or drawn back behind your head by `_back`
  (0.15 m), and above `_low` (0.6 m under the eyes); held `_read` s (0.08): one roll (`_chance` 0.6); delay 0.08-0.3,
  guard 0.6-1.2, recovery 0.5-1.0 s. `_rearm` 1: a raise held is read once (lower it to be read again). The player-side
  melee code has no wind-up detector (only "drawn back" for blows), so this is a pose test against your head and the way
  to it (`VR_AGG_RaisedHand`).
- **Reactive** (`vr_agrunt_react_parry` 1): the old parry for a blow it sees coming; off, only the guard raises it.
  Its chance and reaction stay the existing settings rows (`vr_agrunt_parry_chance`, `_react`); new tunable
  `vr_agrunt_parry_react_rand` (0: as before) adds a random 0..x s.
- **By what you hold** (`VR_AGG_Cat`: melee weapon, gun, prop (a carried thing or a club), fists): tunables
  `vr_agrunt_pcat_<melee|gun|prop|fist>_<chance|delay|guard|range|windup|pre>`; defaults melee 1.5/0.7/1.25/1.25/0.85/1,
  gun 0.6/1.5/0.8/0.85/1.3/0.25, prop 0.75/1.3/0.9/0.9/1.15/0.5, fist 0.9/1.15/1/0.9/1.1/0.6. Chances scale as odds
  (`1 - (1 - c)^m`: 0 stays never, 1 always, so the old tests' chance 1 still means always); delays (the reactive
  reaction too), guard time and ranges multiply; `windup` multiplies the wind-up's thresholds and read time (above 1 a
  plainer raise); `pre` the preventive chance besides (a gun's holder: it mostly waits for your wind-up). The reactive
  parry uses the striking hand's category (a monster's blow: none).
- **Tunables page groups:** Armed Grunts - Preventive Parry, - Wind-up Parry, - Reactive Parry, - Parry by What You
  Hold. **Show Disarm Volumes** appends the guard's state to its note: `preparry in 0.12 s`, `preparry 0.64 s`,
  `windup 0.49 s`, `recovering 0.59 s`.
- **Tests** (QC/vr_armedgrunt_preparry_test.qc; Debug > Tests > Armed Grunts: Preventive Parry, Wind-up Parry):
  `vr_agrunt_test 96`, 8 cases (shove ready: none, 0 rolls; shove cooling: raised 0.208 s after the roll (0.2), guard
  0.806 s (0.8), a fist on the barrel in it parried, in the recovery disarmed; recovery 0.611 s then raised again;
  the melee punch through its barrel: in the guard 5 damage (parried), in the recovery 30 and disarmed; 176 units off:
  none; chance 0: 9 rolls, no guard; a sword grunt, shove ready: raised; random ranges: 4 cycles, delays 0.139-0.389 in
  0.1-0.4, guards and recoveries in range): 8 passed. `vr_agrunt_test 98`, 6 cases (hand at rest: none; overhead:
  raised 0.250 s after the raise (read 0.08 + delay 0.15 + a frame), guard 0.500, labels, the bait's strike in the
  recovery disarms; held 2 s: 1 guard, 1 roll, lowered and raised again: a second; to the side: raised; chance 0: 1 roll,
  none; 400 bare rolls each at chance 0.5: wind-up axe 0.67 vs shotgun 0.34, mean delay 0.141 vs 0.306 s, preventive
  0.63 vs 0.12, every delay within its range): 6 passed. Both end on `agtest: test 9N over`. (Numbered 95/96 at
  first; next's knights took 95: now 96 and 98.)
- **Covering** (run_baseline agrunt + suite, `agrunt|parry|melee_phase|judo`, 91 cases before merging next, then the
  agrunt 60-69 / 90-98, parry, judo and melee_phase cases after): 31, 36, 65 FAIL -> PASS (as disarm2); 67-69 failed
  once from a rename slip (the grab tests' dispatch), fixed; test 95 (next's armed knights, the same parry code) 7/0.
  After the merge test 94's case 13 (the punch through the barrel, caught out) deals 0 damage with the new parries off
  too, and passed on this branch before the merge: next's (the worn-pack/armour changes?), not this. 66 (a lying one
  reached for) flaky: failed once, passed on rerun with the defaults; 63 and parry_pose fail as in baseline.tsv.
  eval.sh --full 163/171, as next (the uppercut's 13.2 predates disarm1).

## Knights as armed humanoids (branch `agent/agknight`)

His ask: "make knights a new armed humanoid kind". The knight is the armed humanoids' third row (`VR_AM_KNIGHT` 2): it
holds its sword as a real weapon (the player's `WID_SWORD`, the Knight's Sword), drawn in its posed, skinned hand, swung,
parried, disarmed, tugged, grabbed and dropped by the armed layer's own code. Behind **`vr_agknight`** (default 0; Combat >
Armed Grunts > Armed Humanoid Replacements: Armed Knights; takes effect at the next map load). Its own setting rather
than part of `vr_agrunt_repl`'s set: the replacements' machinery is the gun holders' (native shots, ammo tiers, variety
rolls), the knight's sword needs none of it, and he can try knights alone while the kind is new.

**The plan** (each step committed and tested):

1. **The model and the authored rig** (done). `progs/vrknight.mdl` (`make_vrgrunt.py` row `vrknight`: knight.mdl, Quake
   VR's own model, without the sword's 81 triangles; its vertices kept for the line). `quakevr/rigs/vrknight.rig2.txt`
   (`make_rig2.py knight`: `KNIGHT_SKELETON`, the 20 bones read off his mesh: mid-stride, right leg forward, the fist on
   the grip at 12 -6.7 1.1; the helmet the head's by a rule; `max_hops` 3 and the new `weld_gun False`: the sword's weight
   had reached the left pad and the neck, long red spikes as the arms moved). Only vrknight.rig2.txt: knight.mdl keeps
   its derived rig, so knights are unchanged with the setting off; grunt and enforcer files byte-identical. Engine: model
   id `VrKnight`, `monsterFiles`, its ragdoll seed row (the knight's seeds, deaths 76..96). The Rig Studio's subject:
   with `vr_agknight 1` the Subject menu's armed kinds list Knight (the toolgun's table). Its limits are the grunt's
   (the left upper arm's swing back to 60: he hangs it 51 back at rest). `vr_rig2_check progs/vrknight.mdl`: 12 passed,
   23 failed (the grunt's shipped rig: 22 / 13): see-through and flips at the joints' extremes. **Left**: his Range of
   Motion tuning, as the grunt's was.
2. **The sword held** (done). Row `VR_AM_Row(VR_AM_KNIGHT)`: hand 106 110 114 (stillest in the sword's space over his
   living frames, 2.3-2.9 rms), the hidden sword's pommel 568/571, tip 561/564, guard ends 537/594 (roll); frames stand
   0..8, run 9..16 (his steps), take kneel 66..70 (a knee); melee classes only; shoulders from the rig, the grunt-made
   poses fitted (`1 1.06 1`, `3.2 1.46 -2.2`). Engine `armedRigs` row 2 (a fifth gun range), `agpose::numKinds` 3 (his
   own pose keys: the Pose Studio edits them, names Knight), `aggrip::numKinds` 3 (his grips: none shipped, the rest grip
   from his own sword's place); the Weapon Holds stay two kinds (his sword holds as the grunt's: `agholds::hands`).
   `monster_vr_knight` (default weapon the sword; any melee weapon by "weapon").
3. **AI** (done, the layer's): `monster_knight` calls `VR_AKN_Replace` (vr_armedrepl.qc; not statues, not Honey's
   trigger copies' maker); the layer's hunt, approach and melee (`VR_AG_Melee`, `VR_AGP_*` swings); ai.qc's and fight.qc's
   `knight_attack` skipped for an armed one; pains, deaths, ragdolls are the knight's own. Parry: the layer's
   (`T_Damage_VRMelee` -> `VR_Parry`, whatever agent/preparry adds there comes with it).
4. **Disarm, tug, grab, drop** (done, the layer's): `knight_die` runs `VR_AG_Dying` first (the held sword falls, once;
   his own `VR_DropKnightSword` only when not armed).
5. **Tests** (done): `vr_agrunt_test 95` (QC/vr_armedknight_test.qc; Debug > Automation & Self-Tests: Armed Knights):
   off (a plain knight), on (kind 2, vrknight.mdl, a sword 3.1 from its hand), swing (hurt you 11 in 1.5 s), disarm (its
   sword lying by it), re-arm (took it up in 1.7 s), death (one sword lying): 7 passed, 0 failed. Test 88 with
   `vr_agknight 1` adds the knight's stagger and topple with a sword (grip 0.00 off the hand, torso 0.63 / 0.18): 6/0.
   `vr_agknight 1; vr_agrunt_test_kind 2; vr_agrunt_test 60` (they parry your blows): 6/0. Test 20 with kind 2: 3/0.
   Eyeshots: qvr-kit/scratch/agknight/`knight_attacks.png` (test 14, kind 2: fists, axe, sword, crowbar at the wind-up,
   strike, horizontal, rest; before the rig's weight fix: the red spikes), `knight_attacks2.png` (strike and rest, after).
   Debug > Automation & Self-Tests also: "Armed Knights: Spawn One Before You" (`vr_agknight 1;
   vr_toolgun_select_armed 2 13`; the toolgun's list is asked again when vr_agknight changes).
- **Covering** with the setting off (vs baseline.tsv): 83 63/0, 60 5/1, 20 3/0 (as baseline); 88 4/0 and 3/1 in two runs
  (the grunt's topple at 1.46 of the 1.5 torso limit: flaky before this). Kind loops that mean the gun holders
  (`VR_AM_GUN_KINDS` 2: test 83's pose categories, the replacements' precaches) are unchanged; the toolgun lists the
  knight only with the setting on.
- **Left / next**: his rig's Range of Motion tuning (above); the knight's own pose keys and grips (none shipped: the
  grunt-made code poses fitted); his melee frames' legs are his stand frames (his runattack and attackb frames unused);
  trigger-spawned knights (Honey's copies) stay plain. **Hell knights** would need: hknight.mdl's sword (knownSwords:
  42.., 332.., 525.. with the guard) in a `vrhknight` row, a rig2 skeleton (taller, pauldrons), a row 3 (his frames,
  kneel-less: a take from painb or deathb), his magic attack kept as the native ranged attack beside the sword (the
  layer has no "melee weapon plus spell" mode: a `vr_am_native` missile hook), `agpose::numKinds` 4.
- Checklist: `[agknight] vr_agknight 1, a map with knights (e1m2): each holds his sword in his posed hand, swings it at
  you, parries your blows, can be disarmed, tugged and grabbed; it falls when he dies, his ragdoll keeps it when knocked
  down`; `[agknight] vr_agknight 0: knights as before (their own sword, attacks, drops)`.

## Impact disagreements unified (branch `agent/impactunify`; his decisions on "One impact-effect table"'s list)

His five, as table rows and table use (no new ladders):
1. **Plate stops melee blood** (list item 1): `VR_HitBlood` asks `VR_AgArmor_HitFx` as the shots and the splash do
   (sparks; blood only for what gets through), and so does the chainsaw's tick (before its T_DamageImpl, sparks up:
   the plate as struck; VR_AgArmor_Damage's own sparks then skipped as for shots).
2. **Hitscan and nails bleed alike** (item 3): both use the **bullet's row** (blood(0.2): 0.04 x the bounce, count
   the damage, at the hit). Traced, the two already sprayed the same (spawn_touchblood scaled its bounce by 0.2
   before the row's 1 x 0.2; TraceAttack's row 0.2 x 0.2): the list's "hitscan barely sprays" missed the caller's
   0.2. Now spawn_touchblood passes the raw bounce (wall_velocity, as TraceAttack's) and the rows scale it: the nail's
   metal and wood rows `along(sparks/splinters(0.2), 0.002)` (the same output as before); its blood loses the
   1.2-unit offset along the bounce (the bullet's row has none). VR_AgArmor_HitFx normalizes its vel: unchanged.
3. **The chainsaw by material** (item 4): `VR_Saw_HitFx(e, m, at, dmg, bleed)` (vr_chainsaw.qc), m =
   `VR_MaterialOf(e)` before the damage, the table's saw rows. Changed: a wall torch nothing (was 4 sparks up), any
   other damageable non-monster, non-crate thing (a shootable button, a breakable) blood (was sparks), the spray's
   crandom() now drawn on metal too (its row ignores it). Monsters, players, gibs (flesh, armour), crates (wood),
   explosive boxes (metal), grenades (metal's saw row, as before: 4 up) unchanged.
4. **Grenade sparks spread** (item 5): the Grenade rows are Metal's `.thing` for every kind (count 0.5 x damage, 3-10,
   along the hit; the pellet's `back` spray in VR_HitNonFlesh like metal's), no ring. `grenadeSparks()` removed.
5. **No flesh slap on a torch or a grenade** (item 6): the blunt layer's rows: torch WoodLayer (wood_m), grenade
   MetalLight (metal_l).

Kept (his "the other 5: keep"): item 2 (knights' armour only changes the blunt layer's sound; the Armour rows copy
Flesh's particles), 7 (the world bleeds / is stone for the blunt layer), 8 (VR_MaterialOf's one order), 9 (shots ring
on metal once a frame, blows leave the sound to the melee), 10 (a thrown thing: splinters along its flight into wood,
else spawn_touchblood's nail row).

**Test.** `vr_impact_qc_test` gains ladder 8 (the chainsaw's tick, VR_Saw_HitFx) : `vr_impact_qc_test 9` now ends
`imptest: 56 calls` (was 49). Trace (sv_random_seed 7, vrtesthall) before / after: kit `scratch/impacts/
unify_trace_before.txt`, `unify_trace_after.txt`: the changed lines are the grenade's (ladder 1: 6 sparks spread, was
8 up; 2: 4; 3: 10; 4: 7), the nails' blood origins (-57.7 -> -57.9 etc., same directions), the blunt layer's torch
(wood_m1) and grenade (metal_l1), ladder 8's 7 calls (blood 8, splinters 4, sparks 4 up, blood 8, 8, torch nothing,
grenade 4 up); the splash's and the layer's other samples moved with the random draws (VR_HitNonFlesh's grenade now
draws its spray). `vr_agarmor_test 5` (agarmor_5): new check "a blow of 4 on a red cuirass: sparks (1), no blood (0)":
24/0. Eyeshots (low-impact) of ladders 1-5 (+ 8 after): kit `scratch/impacts/unify_before.png`, `unify_after.png`.
check_canonical: `qc-material-ladder` vr_chainsaw.qc 1 -> 0 (allowlist line removed).

**Verified.** Build 0 warnings, check_canonical clean. Covering cases vs baseline.tsv (18): collectfx 9/0,
explosion_debris 13/0, mapflameburn 16/0, empty_melee, melee_phase, walltorch_grab, arena_1 257/0, agarmor_1 26/0,
agrunt_12, agrunt_33, limb_wear 9/3, judo 12/4, secret_hits, dead_gear as the baseline; agarmor_5 PASS (baseline
22/1, flaky); agrunt_1 15/2 once (grenade and rocket launcher shots, flaky), 17/0 on the rerun; nail_1 11/0 and np_1
35/0 (the newer tests' own checks). gunfx_test.py 63/10: the same 10 fail on origin/next's build (wisps 6, slots 4:
the muzzle wisps' and the thunderbolt's newer changes, not impacts).
- Checklist: `[impactunify] hit an armoured armed grunt's plate with a melee weapon or the chainsaw: sparks, no blood
  until it breaks`; `[impactunify] shoot a grenade in flight (shotgun, nails): sparks fly out along the hit, not a
  fountain straight up`; `[impactunify] punch or club a wall torch: a wooden knock; a grenade: a light metal clink
  (not a flesh slap)`; `[impactunify] chainsaw a wall torch: no sparks`.

## Four more defaults promoted (branch `agent/promote1030b`; his decisions 2026-10-30)

His words: "we need to polish the feature and ship the replacements"; the relit maps "a tiny bit brighter in general".
`vr_cvars.inc` (sparse config: the default only, no configVersion step):

| cvar | was | now |
|---|---|---|
| vr_agrunt_repl | 0 | 1 (Armed Humanoid Replacements: every grunt and enforcer a map spawns is armed; its rarity tuning was promoted before) |
| vr_relight_minlight | 0 | 64 (Ambient Light Level of a relit map) |
| vr_relight_reload | 0 | 1 (a relit map reloaded where you are) |
| vr_ghost_hands_fade_from | 6 | 9 cm (vr_ghost_hands stays 0) |

Also: `relight_maps.py --ambient` defaults to 64 (the game's `vr_relight_defaults` says its defaults are the script's
look); RELIGHTING.md and ARMED_MONSTERS.md state the new defaults. The menus' help names no default values.

**Tests pinned to the old values** (each assumed plain grunts; no assertion changed), `vr_agrunt_repl 0` in:
mapflameburn (the dropped torch's edict number on e1m2), melee_phase (its grunt cases), weapon_catch (e1m1),
animsmooth (the soldier's model and edict numbers), teleporters chase (the grunt's ranged chase), ragdoll guard (the
grunt's guard), agrunt test 20 (`VR_AGPT_Ragdoll`: the armed grunt against a plain one), and
`vr_rigstudio_test_subjects` (later fixed in the code instead, below). ghost_recoil_test.sh pins
`vr_ghost_hands_fade_from 6` (its 17 cm kick against 6 cm). No test used the relight values.

**Tools spawn the plain monster.** With replacements on, the Rig Studio's `monster_army` subject and the debug
spawner's grunt came armed. Now every tool's spawn holds QC's `vr_agr_hold` (the training dummies' existing hold), which
both `VR_AGR_Replace` and now `VR_AKN_Replace` (the knights) read: in the engine one guard, `progs::ToolSpawn`
(vr_progs.hpp; the Rig Studio's subject, the ragdoll tool's, the toolgun's spawn, its ghost probe and its spawn
commands), in QC `VR_Test_Spawn` (impulse 241's `vr_test_spawn` and the tests' spawn helpers). The armed kinds stay
their own classes (`monster_vr_grunt`/`_enforcer`/`_knight`). The scenarios' spawns (`toolgun::spawnEntryAt` from
vr_scenario.cpp) stay a map's: replaced. The pins in `vr_rigstudio_test_subjects` and melee_phase came out again.
Test 70 has a new check: `vr_test_spawn 0` with replacements on gives a plain grunt, the hold back to 0 (22/0).
rigstudio_subjects 13/1 (the baseline's), melee_phase 7/0, agrunt_20 3/0; tests 71 5/5 and 72 6/1 (flaky, baseline 6/4
and 5/2).

**Verified.** Full baseline, then the changed cases on origin/next's defaults and on the new ones: 9 changed by the
defaults, all from the replacements. Pinned: mapflameburn 16/0, agrunt_20 3/0, ragdoll_guard 8/0, teleporters_chase
6/0; weapon_catch 3/2, animsmooth 7/2, rigstudio_subjects 13/1 back to the baseline's own fails; ghost_recoil 3/0.
melee_phase `grunt_fast_on` (0 hits) still fails 5 runs of 5 with the pin, but passes 3 of 3 with three more cvar
commands before `map` (the new values set explicitly too): sensitive to the run's perturbation (no `sv_random_seed`),
not to these values; it failed in older runs too. Left as is (flaky).
## One busy-hand check for every grab (branch `agent/busyhand`)

His note (2026-10-30): the force grab should refuse a busy hand too, "one single code path to check that".

- **The predicate (QC/vr_wpnforcegrab.qc):** `VR_Hand_Free(p, hand, xAllow)`: empty (VRIsHandEmpty: no weapon, the slot
  carries nothing), not busy (the slot's VR_HOLD_FOE and after: an enemy, the flashlight, a gun's fore-end, a just-seated
  magazine, a tug), not locked on to a force grab. A site's exceptions are its `xAllow` bits (QC/vr_fields.qc):
  `VR_HANDFREE_BUSY` (any busy kind takes it: a weapon's grab), `VR_HANDFREE_FOE` (the foe grab's hand-over),
  `VR_HANDFREE_LOCKED` (the force grab's own lock). It replaces VR_Hand_CanTake/CanTakeFrom and the hand-written mixes.
- **The sites:** none (0): carry grab, box touch, ragdoll limb grab, wall torch (take and query candidate), grenade catch,
  both pouches, magazine pull. FOE: the hand-over. LOCKED: the force grab's frame, flat grab (E), catch, the axe tug's
  flight. BUSY|LOCKED (a weapon's grab, as before): weapon_touch, a thrown weapon's catch, an armed grunt's weapon
  (tug/disarm), the holster draw (DoHandImpl), the hand-off, the grapple gun's pass, the water climb. BUSY, LOCKED only
  on its own catch: a lying weapon's grab.
- **Behaviour change (only):** the force grab's start (VR and flat), its catch and the tug's flight refuse a busy hand
  (before: only the flashlight's); a hand turning busy while locked on lets go of the lock.
- **Not folded:** the engine's climb (vr_climb.cpp handBusy: before the move, the client's flashlight bit and a pull in
  flight; it does not see the slot) and the client's grips (held::handEmpty: no slot on the client). Folding either
  changes answers (a hand on a fore-end or holding an enemy could no longer climb): his call.
- **Open (his call):** a busy hand still takes a weapon (lying, thrown, a grunt's, the holster's): the flashlight hand
  can grab a gun. Kept as it was (VR_HANDFREE_BUSY at those sites); dropping the bit there makes them refuse.
  (Decided 2026-10-31: refuse. See "Busy hands refuse weapon pickups too".)
- **check_canonical.py `qc-busy-hand`:** a new read of the force grab's lock, a slot kind compared to VR_HOLD_NONE or
  VR_HOLD_FLASHLIGHT, VR_Hand_IsBusyKind or the gone VR_Hand_CanTake outside today's sites fails the build.
- **Test:** `vr_pickup_test 8` (Misc/quakevr/carry/fgbusy_test.sh, e1m1): VR_Hand_Free's exceptions; the first
  force-grabbable thing put on the off hand's aim, the trigger held: free, it locks on; busy with the flashlight, a
  fore-end, a seated magazine or a tug, no lock-on; locked on then busy, it lets go. 11 passed, 0 failed (with
  vr_pickup_test 7, the slot test, 16/0).
- **Verified (run_baseline.sh --only, 34 covering cases: carry/props, weapon_catch, flash_grab, grip_*, mapgun_pickup,
  ragdoll_grab/climb, prop_climb, judo, walltorch_grab, reload, reload_pouchgren, agrunt 2/3/21/22/31/32/33/36/39/65-69/
  74/75/91/92):** against baseline.tsv agrunt_31/36/65 FAIL -> PASS, agrunt_66 PASS -> FAIL (its lying grunt drops its
  weapon as the hand reaches; this branch 2 of 4 runs fail, origin/next 1 of 1 passes: a flake under load is likely, but
  not proven), ragdoll_grab 18/1 and agrunt_67 1/1 (listed flaky); the rest as the baseline. Build 0 warnings,
  check_canonical clean.
- **Checklist:** hold the flashlight in one hand and point that hand at a box with the trigger: no lock-on (the other
  hand locks on); two-hand the super shotgun and point the fore-end hand: no lock-on; let go: it locks on. Force grab,
  weapon pickups, the holster draw and the hand-off feel as before.
## The parry's guard meets the blow (branch `agent/guardpose`)

Test 60's ogre case: the raised parry held the weapon beside the blow's way, not in it ("missed its weapon"; his call:
fix the pose, keep `VR_AGPAR_RMON` 5). `VR_AGPar_Pose` aims the weapon's middle at a point on the blow's way, but the
rest of the pose then moves it: the Pose Studio's keys (`vr_agk_rp`, its hold's offset), the body's turn, the reach
clamp. Kinds 1 and 2 held the sword up to 10-13 units to its right of the way (middle `14 -13 13`, `18 -10 26`).

- **The fix (`VR_AGPar_Meet`, vr_armedgrunt_parry.qc; called by `VR_AGP_Pose` after the weapon is placed and turned):**
  the weapon's line as the game has it (`VR_AG_WeaponLine`) is measured and the weapon (and its hand's target,
  `vr_agp_v`) moved across onto the middle the parry aimed at, by the parry's share, only across the weapon's own line
  (where along its line the keys hold it stays), at most `VR_AGPAR_AIM_MAX` 16 units, and never further out of the
  arm's reach than the pose already had it. The keys stay the base, the correction lies on top; every kind and weapon,
  the reactive parry and the guard (preventive, wind-up) alike, since they share the pose.
- **Test 60's ogre setup:** its ogre is put back face to face with the grunt before the blow (its AI's first second
  walked it round to its side once in 10 runs: "from behind", no parry raised). A parry that is not up prints why.
- **Numbers (10 runs each, `vr_agrunt_test_kind`):** before: grunt 7 of 8 clean, enforcer 5 of 10 (5 "missed its
  weapon", parry up 1.00), kind 2 6 of 7; the pose fix alone: 10/10, 19/20 (the one: the ogre from behind), 10/10;
  with the test's setup fixed: 10/10, 10/10, 10/10 (the sword's middle now 0-4.5 units off the way across it, the
  rest along its tilt). Regression (`--only agrunt|parry|judo|melee_phase|disarm`, 93 cases): no case worse than
  the baseline (melee_phase, agrunt_1 failed once each and passed on a rerun; agrunt_76's cover shots flaky as
  listed; parry_pose fails as in the baseline); eval: every verdict the same (a few damage digits jitter run to run).

Checklist:
- [guardpose] Swing at an armed grunt (sword, then an enforcer) so it raises its parry: its weapon sits across your
  blow's way, in front of its chest, not off to its right; your swing rings off it.
- [guardpose] Raise your weapon near a grunt (the wind-up parry) or step in close (the preventive guard): its raised
  weapon points at your hand, its hand still on the grip (not stretched off it).

## Busy hands refuse weapon pickups too (branch `agent/busyweapon`; his decision 2026-10-31: "Yes, refuse")

- **Change:** a busy hand (the flashlight, an enemy held, a gun's fore-end, a just-seated magazine, a tug) takes no
  weapon. `VR_HANDFREE_BUSY` dropped at the weapon-taking sites: weapon_touch (all four checks, the body's too), a lying
  weapon's grab (VR_PickupObj_WeaponTouch), a thrown weapon's catch (wpnthrow_handtouch_ent_hand), an armed grunt's
  weapon (VR_AG_HandGrab: the grab, the living ragdoll's disarm grab), the holster draw and a carried gun's handle
  (DoHandImpl's handEmpty).
- **New exception `VR_HANDFREE_GUNGRIP` (8):** busy on your own gun alone (VR_HOLD_SSGGRIP, VR_HOLD_MAGREHOLD) still
  takes it. The hand-off (VRTryHandOff) uses it: two-handing the super shotgun makes the fore-end hand SSGGRIP-busy, and
  the main hand letting go must still hand the gun over (no drop). The flashlight hand gets no hand-off.
- **Grapple pass:** it predicts the hand-off (the hook stays in when the other hand will take the gun), so it now asks
  exactly what VRTryHandOff asks (GUNGRIP | LOCKED); with BUSY it kept the hook in for a flashlight hand that then would
  not take the gun.
- **Water climb:** kept BUSY (VR_HANDFREE_BUSY is now its only user): it is a grip on the bank, not a weapon taken.
- **Weapon swaps:** VR_Hand_Free answers FALSE for a hand holding a weapon before any busy check, so swaps, two-handing
  and the fore-end release-then-take (VRTakeCarriedWeapon needs a fresh grip; SSGGRIP is cleared once the grip lets go)
  are unchanged. The hand-switch hotspot (VRSwitchWeaponHands: otherHandEmpty = VRIsHandEmpty) still moves a weapon
  into a flashlight hand: not a pickup, left as is (one line for him if he wants it refused too).
- **Moved:** `.vr_ag_tug_pl` to vr_armedgrunt_defs.qc, with a VR_AG_HandGrab prototype (the test, compiled earlier,
  reads them).
- **Test:** `vr_pickup_test 8` (Misc/quakevr/carry/fgbusy_test.sh, e1m1): VR_FgBusyTest_Offer gives the off hand,
  gripping, a weapon lying (weapon_touch), a thrown weapon (the catch) and an armed grunt's weapon (VR_AG_HandGrab, grab
  speed limit off for the test): the flashlight hand refuses each, the free hand takes each (a tug begun counts), then
  all put back. Plus GUNGRIP: a fore-end hand gets the hand-off, a flashlight hand not. 19 passed, 0 failed (was 11).
- **Verified:** run_baseline.sh --only (28 cases: weapon_catch, flash_grab, mapgun_pickup/transfer, pickup_mag,
  grip_state, ragdoll_grab, walltorch_grab, held_over_props, reload, agrunt 2/3/21/22/31-33/36/39/65-69/74/75/91/92):
  as agent/busyhand's run: agrunt_31/36/65 FAIL -> PASS against the old baseline, agrunt_66 PASS -> FAIL (known flaky),
  agrunt_67 TIMEOUT (listed flaky; his game was running for part of the run); the rest as the baseline.
  holsteritems_test.sh all PASS. Build 0 warnings.
- **Checklist:** hold the flashlight in one hand and grab a gun lying on the floor, catch a thrown gun, or grab a
  grunt's gun with that hand: nothing happens; the free hand takes them. Two-hand the super shotgun and let go with the
  main hand: the fore-end hand keeps it. Flashlight hand at a holster: no draw.
## Drawn pose: worn backpacks and disarm volumes (branch `agent/drawnpose`)

His notes: worn packs phase through a grunt bending to take a weapon ("where are they attached? It should be the upper
back"); the pack's grab box bigger than the pack; Show Disarm Volumes on an active ragdoll float off the hand and weapon,
follow the empty off hand of a toppled rocket-launcher grunt, a lying sword's angle differs from its volume; the magenta
and orange volumes 3-4x longer than a held nail pistol.
- **Where the pack was:** on the chest bone, but of the .mdl's animation frame (hitmodel's vertex pose: three chest
  vertices), not of the body as drawn (the authored rig's bones: the Rig Studio's takes, arm IK, hit reactions); its
  ragdoll box was laid on the chest part by that same frame pose. Now `drawnpose::bone` (Quake/vr/vr_drawnpose.cpp, the
  one "where is this bone drawn now" query): a ragdoll's part as drawn (ragdoll::drawnPart), else the authored rig's
  drawn bone (rig2::drawnBone) placed as the client draws the entity; none (dedicated server, out of view): the old frame
  pose. `agarmor::packWhere` rides it; `packRest` gives the rest placement, which `createRagdollPack` lays on the chest
  part's body directly (the body is the bone as drawn when the ragdoll began).
- **Grab box:** a dead one's pack went to the generic item touch (its entity's level box plus an item's pickup bonus) and
  the ragdoll and QC hand tests took it 3 units (11 cm) off the box; a living one's had vr_foegrab_leniency. Now a touch
  provider for worn packs (vr_physics.cpp `onWornPack`: the fist's spheres, else the palm's 3.5 cm sphere, on the pack
  model's box), the ragdoll's `packReach` and QC `VR_WP_HAND_REACH` the palm's 3.5 cm, no leniency. Shots: the same box.
- **Disarm volumes:** one query, `hitreact::drawnWeapon` (QC `weapondrawnpose(p, 0, 30/31/32)`, `VR_AG_DrawnFrame`):
  the weapon's client entity as finally drawn (after every client step), when the arms (placeProp) or a ragdoll's hand
  (ragdollProps) placed it in the last 0.05 s. `VR_AG_WeaponLine` and `VR_AGS_HandLine` read it; a ragdoll's weapon is
  placed there (`VR_AG_RagProp`), riding the hand bone that holds it as drawn (`weapondrawnpose(g, side, 36)`,
  agarms::ragdollGrip), not the limb nearest it as the ragdoll began.
- **The wrong hand (for the tug fix):** `.vr_ag_rb` (the limb a ragdoll's weapon rides; also box3d `ragdollWeaponHold`'s
  part) was `ragdollbone(g, p.origin)`, the part nearest the server's weapon place as the ragdoll began: a two-handed
  weapon's support (left) hand, and the soldier's hidden rig gun is welded to the left forearm (`looseHolder`). The drawn
  weapon follows `agarms::ragdollGrip(model, side)`'s hand bone (side 0 the main weapon). Now `.vr_ag_rb` is that bone.
- **Volume sizes:** orange is the strike/parry line (VR_AG_WeaponLine, VR_DV_STRIKE_R / VR_AGPAR_R 5 round it: the
  sweeping fist's or weapon's own thickness, kept); magenta the shot volume (VR_AGS_W_RADIUS 3 x vr_agrunt_shotdisarm_size)
  and its hand part (cyan). The line was 8 units behind the grip to the muzzle (24 ahead without one); now the weapon
  model's own ends along its forward (`held::drawnExtent`, `weapondrawnpose(p, 0, 33/34/35)`), the shot radius half its
  thickness (`vr_ag_wr`).
- **Test 99** (`vr_agrunt_test 99`, QC/vr_drawnpose_test.qc): nail pistol, shotgun (wearing a pack), rocket launcher,
  standing / falling / lying: volume grip < 2 units and axis < 3 degrees off the drawn weapon, length within 10% of the
  model, the ragdoll's weapon on the hand holding it, the pack's chest bone source and its entity < 3 units off; a pellet
  3 units past the nail pistol's front end not counted, one on its middle counted. NOT RUN YET (his game was running).
- Not done: the drawn pack still reaches the client through the network (one frame or so behind the drawn body); the
  hitmodel (shots on a live grunt's body) still reads the .mdl frame pose (another stale-pose reader). (Done since:
  "Armed humanoids: shots and grip spheres from the drawn pose".)

## Tug fixes verified; the physical downed tug (branch `agent/busyweapon`)

His note (2026-10-31): grabbing a knocked-down enemy's weapon "doesn't seem to actually physically grab the enemy's
weapon/hand and physically move it at all ... the enemy physically resists by pulling back."
- **The wrong arm, really fixed:** `VR_AG_TugGripAt` read `ragdollpoint(g, hand, '0 0 0', 1)`, the hand part's body
  frame origin, which lies by the spine: the hold took part 1 (spine) every time (tests 31/32 "ragtug arm" failed).
  Now `ragdollpoint(g, bone, _, 2)`: the part's middle (its mass centre; box3d `ragdollPartMiddle`). Tests 31, 32: 6/0.
- **Downed / staggering physical tug:** `ragdolltug(g, pl, h, 6, grip, back, force)` (box3d `tugRagdoll` what 6): the
  hand holds the limb nearest the weapon hand's middle by the limb grab's spring (any ragdoll of it, no stagger needed),
  its arm pulling that limb back towards its pelvis at `back` N (an internal force: the pelvis gets the opposite, so it
  never slides the body; `World::RagdollGrab::tugBack`, applied in `syncRagdollGrabs`). `back` =
  `VR_AG_TugDownBack`: 0.5 x Least Pull x Its Pull x Staggering or Down = 91 N as shipped. The weapon rides the hand
  bone (VR_AG_RagProp), so pulling moves weapon, hand and arm, and the body once the arm is out. Win/lose as the physical
  tug (`VR_AG_TugBreak`), its times (impulse, least pull time) eased by `vr_agrunt_tug_downed_scale`; the pull is the
  stretch until your hand has drawn the arm 5 cm from the body, then the larger of stretch and hold force (the hold's
  force alone carries its own fall: a falling sword grunt tore it out of a still hand in 0.3 s). The haptics feel its arm
  (max of the pull and the hold's force). Such a hold lets go only at the limb grab's 64-unit lag (a falling body drags
  the limb past Let Go Past). Up again (its ragdoll gone): the old tug goes on (`VR_AG_TugDownToOld`).
- **Tests:** 92 phases 5 (shotgun) and 6 (sword), knocked down: physical, its arm's pull on a still hold 140 / 378 N,
  the pull moved the weapon 14.7 / 32.6 units, won in 0.71 / 0.81 s: 22/0. Test 91: 9/9 won (case 10 now accepts the
  off hand, the hand placed last since d6ae5f30a). Tug group 23, 29-38, 74 and ragdoll_grab as before (29: FAIL in the
  baseline too). Test 92 phase 1 (two hands within 0.85x one hand) is borderline: 0.93-1.17 s against 1.36 s, failed
  1 run in 3 since the grip fix.
- **Step-1 fixes:** test 99 put its "3 units past the front end" pellet 3/length units past (`along` past 1 is a share):
  fixed. vr_wornpack_test 1: its "on its back" check wanted the entity within 0.1 of the .mdl frame-pose place; the pack
  rides the drawn chest bone since drawnpose (test 99: within 3 units): 3. OPEN: "dragged slowly by the pack: kept"
  fails (the hold reads 4000 N at the grab, the 43 u/s drag 1550-1940 N, over the 1500 N rip): not investigated.
## Armed knights: shipped poses and every weapon class (branch `agent/knightparity`)

His report (Rig Studio, armed Knight subject, `vr_agknight 1`): "the Knight has no poses at all and it doesn't support
ranged weapons at all".

1. **Poses.** Why none: `agpose` had shipped rows for kinds 0 and 1 only, and `shippedOf` fell back to nothing, so every
   knight key (kind 2) was zero in the Pose Studio, the Rig Studio's Pose Keys and in game. Now a kind with no rows of
   its own ships another kind's: `shippedFrom[numKinds] = {0, 1, 0}` (the knight the grunt's), each row fitted to his
   body by `fitScale`'s ratio exactly as "Copy to the other kind" fits them (one `fitKey` for both; legs' places too).
   His IK stretch likewise (`shippedStretchOf`). No data copied: tuning the grunt moves the knight until his own rows
   are shipped (then his rows win, code by code). Copy between kinds counts the donor kind's rows. E.g. key 201010
   (knight, rifle, Aim) R `1.82 7.1656 6.78` = the grunt's `1.82 6.76 6.78` x `1 1.06 1`.
2. **Every weapon class.** The knight row's `vr_am_classes` all (as the grunt's): the toolgun's and Rig Studio's armed
   Knight list every weapon, `monster_vr_knight`'s "weapon" takes any (QUAKED and FGD say so), map knights keep the
   sword (`VR_AKN_Replace`, unchanged). His guns use the layer's paths: the grunt's shot timing (`VR_AM_A_ATK`) over his
   stand frames with his posed arms aiming (`vr_am_atk_stand`), reloads on his stand frames, a gun butt's shove on his
   attackb frames. No knight-only code.
3. **Ranged pickups** (his addition): `vr_agknight_ranged_pickup` (default 0; Combat > Armed Grunts > Replacements:
   "Armed Knights: Pick Up Ranged Weapons"). The row's `vr_am_takes_ranged` (grunt, enforcer 1; the knight's from the
   setting) and `VR_AM_Takes(g, wid)` gate every take path: the search (`VR_AG_FindWeapon`), its own dropped weapon
   (`VR_AG_OwnBack`) and the take itself (`VR_AG_TakeBegin`). 0: a knight takes up only melee weapons (a gun knocked
   out of his hand stays down); spawned with a gun he holds it either way. Test 2 checks it: `rearm (takes no guns)`.
4. **Rig Studio**: the Subject menu's plain entries read "Grunt (plain)", "Enforcer (plain)", "Knight (plain)"; Range of
   Motion on knight.mdl: "... : pick the armed Knight (Armed Knights on)".
5. **`vr_ad_upgrades` default 2** (Weapon Upgrades: Everywhere; his decision). With `vr_ad_everywhere` 1 (default) the
   scope was already 2; no test pinned 1.

Checklist:
- [knightparity] vr_agknight 1, Rig Studio, Subject: the armed Knight: Pose Keys shows his poses (the grunt's, fitted)
  for every class; his Right Hand list has every weapon; with a shotgun he aims and fires at the aim target.
- [knightparity] Toolgun: spawn an armed Knight with a shotgun (then a nailgun, two pistols): he hunts you, aims, fires,
  reloads; knock the gun out of his hand: he does not go back for it (vr_agknight_ranged_pickup 0).
- [knightparity] Map knights (e1m2, vr_agknight 1) still hold swords; Rig Studio Subject's plain entries read "(plain)".
## Nail hits bleed and dent again (branch `agent/nailburn`)

His report: nail hits on an enemy left burn marks instead of a dent and blood. Cause (not the fire spots, not lava
nails): since "Projectiles: one path" (869e5568b, 10-19) VR_Proj_Begin tags every projectile's hit QVR_HITKIND_SHOT,
and VR_Wound_Hit read any non-pellet shot as lightning (QVR_WOUND_ZAP: a burn splat, no dent, no blood stream). The
small gibs' and corpses' kinds (VR_SmallGib_Kind, VR_Corpse_DamageKind) and the gib speed (VR_GibKind) likewise called
a nail's hit a hitscan's ("shots"; a plasma bolt's "light"). Fix, one vocabulary: the hit record says when a shot is a
projectile's (`vr_hitproj`, set by `VR_SetHitProjectile`: VR_Proj_Begin, the laser's bounce), and `VR_Hit_Hitscan()`
is the one test those four readers use. A projectile's hit takes its inflictor's kind again: a nail's wound
QVR_WOUND_NAIL (dent + blood), a lava nail's, a laser's, a lava ball's a burn; the nails' small gibs and corpse damage
"nails" again.
Test `vr_nail_test 4` (Debug > Tests > Nail Pistol > Nail Wounds Test): the nailgun's, super nailgun's, nail pistol's
and an armed grunt's real nails at a grunt: wound kind 2, blood spawned, not on fire, nail still a "spike"; a lava
nail: kind 5.

Checklist:
- [nailburn] Shoot a grunt with the nail pistol, the nailgun and the super nailgun: each nail dents its model and
  bleeds as a bullet does (no black burn marks); a lava nail (Dissolution of Eternity) still scorches and burns.

## A revolver's shell slides into its own chamber (branch `agent/nailburn`)

His report: a shell loaded into the heavy revolver's cylinder slid towards the cylinder's middle, not the hole it went
into. The held shell's slide (vr_collectfx.cpp's "into the gun") followed view::loadPath, the load point on the
cylinder's axis. Now VR_Revolver_LoadRound sends the chamber it chose (collectfx hotspot `QVR_CFX_INTO_CHAMBER` 243 +
the chamber), and the slide follows `view::revolverChamberPath`: the same way moved off the axis to that chamber as drawn
(the crane's swing, the cylinder's turn now: `rvChamberOff`, shared with setupRevolverParts' chambered shells).
Check (vr_rv_test 1, step 8): the slide's end is measured against the shell then drawn in that chamber
(`vr_test_collect_chamber_off`, a tests-only cvar the engine sets; with vr_debug_collect_fx also printed with the
cylinder middle's distance for comparison): want at most 0.5 units off its axis. Measured: 0.00 for each of the four
shells loaded (chambers 3, 5, 0, 2); the old way's end was 0.73 world units off them. (The way keeps the load point's
place along the bore only: its Load Point offsets can put it off the cylinder's axis, which first left the end 0.95 off.)
Before/after for the nails (the fix's test reverted to the old reading): wound kind 6 (lightning's burn) for all four
nail weapons and the lava nail; after, 2 (nail) and 5 (burn). `vr_nail_test 1`'s backpack step fails (3 checks: no
nail met the lying bag) with and without this fix: not this branch's.

Checklist:
- [nailburn] Swing the heavy revolver out and load shells one at a time: each shell slides straight into the hole it
  ends up in (any chamber, cylinder turned or not), not towards the middle of the cylinder.
## Starter shotgun: the auto pump's pieces kicked (branch `agent/sgrecoil`)

His words: "Starter shotgun recoil is still messed up, it really feels like there's an animation frame where the gun's
position/angle moves that conflicts with our procedural recoil."

- **Guilty: the auto pump's two models.** While the fore-end strokes (vr_autopump_delay 0.25 s after the shot, for
  vr_autopump_time 0.3 s) the shotgun is drawn as `progs/vr_pumpbody_on_v_shot.mdl` and `vr_pump_on_v_shot.mdl`
  (setupPumps), at the gun's own frames, its entity hidden. Both are split from v_shot.mdl with its 9 poses, so both
  carry its baked kick, and neither was in the kick cancel (not a slot's model): mid-stroke, at frames 3-6, the drawn
  gun jumped back 9.5 / 6.7 / 3.8 / 1.3 cm and tipped up 8.6 / 6.2 / 3.3 / 0.9 degrees (`vr_kickcancel_info
  progs/vr_pumpbody_on_v_shot.mdl`; offline the same: Kabsch fit of each pose, Python), easing back as the frames stepped,
  under the procedural kick that was returning. The recoiltrack check of the last round read the gun's own entity (still:
  body == rest), not what is drawn in its place.
- **Fix** (`vr_kickcancel.cpp` `preparePart`): a model drawn in a listed gun's place at its frames is fitted and cancelled
  as that gun's slot's (its body exactly the rest pose's bytes; its parts, the flash, through the pose's cancel); called
  by `setGunPart` (each frame it is drawn) and at map load for the shotgun's two pieces (prepareWorldWeapons: no fit
  hitch at the first stroke). A piece read again stays a piece. The SSG's opened parts (frame 0), magazines (their own
  round frames) and revolver parts are not drawn at the gun's frames: not affected.
- **Audited still** (code and numbers): v_shot.mdl's own 9 poses (all cancelled; parts left: the 55-vertex muzzle flash,
  3 collapsed vertices), the frame lerp (body identical in every pose), QC (W_FireShotgun: punchangle -2 only; the VR
  eye view overrides r_refdef.viewangles, so v_gunkick never reaches the headset; no velocity kick), the gun entity
  (recoiltrack resid 0.000 cm every frame at 72 and 90 Hz). Not separately measured: two-handed (the pieces copy the gun's
  entity, so the same), the off hand, collision push, aim filters (no frame-dependent motion found in setupWeapon).
- **Numbers after** (`scratch/sgrecoil_track.sh`, real client path, vr_debug_weaponfx 4, a new "recoiltrack part" line):
  72 Hz 22 stroke frames, 90 Hz 27: the drawn piece's body 0.000 cm off rest every frame (frames 2-5), the gun's 0.000.
  `vr_kickcancel_test` 79/0 (was 71: the two pieces' 8 checks; "14 listed models, 2 pieces").
- Open (not changed): the procedural rise is added to the controller's Euler pitch (vr_view.cpp), so with the hand rolled
  the muzzle rises partly sideways (about the world-level right axis, not the gun's).

## Two-handed recoil: one slider per value (branch `agent/sgrecoil`)

His words (via the coordinator): split the two-handed recoil multiplier into one slider per value it affects.

- Weapon Effects: "Two-Handed Kick Back / Muzzle Rise / Return Time / Haptics" (`vr_recoil_2h_kick`, `_rise`, `_time`,
  `_haptic`, 0.6 each); Weapon Offsets > Effects: the same four per weapon (`vr_wofs_fx_2h_kick_NN`, `_rise`, `_time`,
  `_haptic`, 1 each). The sideways turn goes with the rise; a burst's 2.5 cap per value. Defaults as before: nothing
  changes until moved. `weaponfx::twoHandRecoil` returns the four (`TwoHandShares`); the haptic punch reads `.haptic`.
- Old configs: `vr_recoil_two_hand X` and `vr_wofs_fx_recoil_2h_NN X` are retired lines (VR_RetiredCvar ->
  `weapons::retiredTwoHandLine`): X set on all four. No version step (a sparse config holds only changed values).
- Test (`scratch/sg2h_test.sh`): the old lines 0.4 and 0.7 land on all four each; `vr_recoil_test` 120/0 (the old
  two-handed checks unchanged with the four alike, plus 8: each slider alone, global and own, moves only its share and
  its drawn value, e.g. own rise 0.25: peak 1.812 deg, 3.000 cm, 0.0945 s against 3.624 deg, 3.000 cm, 0.0945 s).

Checklist:
- [sgrecoil] Fire the starter shotgun one-handed, then two-handed, and watch the gun through the auto pump's stroke: it
  kicks once and settles smoothly; no second lurch back and up about a quarter second after the shot.
- [sgrecoil] Weapon Effects: the four Two-Handed sliders (Kick Back, Muzzle Rise, Return Time, Haptics) each change only
  their own part of a two-handed shot; your old Two-Handed Recoil value is on all four.

## Muzzle rise about the gun's own axis (branch `agent/riseaxis`)

His call (after sgrecoil noted it): the procedural recoil's rise was added to the hand's Euler pitch, so with the hand
rolled the muzzle rose partly world-up instead of along the gun's up.

- **The fix** (`vr_weaponfx.cpp` recoilOffset, new `gunLocalTurn`): the kick's pitch and yaw now turn the hand about its
  own right and up axes (the hand's drawn angles, which carry the calibration `vr_gunangle`/`vr_gunyaw`; the pivot as
  before, the hand's origin), returned as the Euler change the callers already add (vr_view.cpp's shakes, the ghost
  hands' measured pose). The kick back was already along the hand's forward. Every weapon, one- and two-handed (the
  shares scale the kick before it is turned), stream shake included; `vr_ghost_hands_ignore_recoil` unchanged (it
  adds the same change). An unrolled hand's rise is the pitch, exactly as before; its sideways turn is now about the
  gun's up rather than the world's (differs only with the hand pitched; the tests' unturned hand gives the same).
- **Decision**: the hand's frame, not each weapon's small per-slot angle offsets (weaponAngleOffsets), is the gun's axis:
  the offsets are a few degrees, and the hand frame keeps one place and the unrolled case exact.
- **Tests**: `vr_recoil_test` 127/0 (new axis part: hands turned six ways incl. rolled 90/45, pitched+yawed+rolled: the
  muzzle's move and the kick back in the hand's axes match the unturned hand's to 0.0000; rolled 90 the move across
  the barrel is 0.000 world-up, rolled 45 0.707; unrolled pitched hand: the rise is its pitch). `vr_kickcancel_test`
  79/0, `gunfx_test.py` 73/0.

Checklist:
- [riseaxis] Roll your wrist 90 degrees (gun on its side) and fire the shotgun or nailgun: the muzzle kicks toward the
  gun's top (sideways in the world), not toward the ceiling; at 45 degrees it kicks diagonally along the gun's top.
- [riseaxis] Upright gun, one- and two-handed: the recoil feels the same as before.

## Blade on blade (branch `agent/bladecontact`)

His note (2026-10-31): a sword grunt's blade "doesn't react to any player attack ... it just goes through and nothing
happens. There should at least be a parry or sparks + sound effects (and the enemy should not take damage). Perhaps if
the blow is strong enough, the enemy should get briefly staggered and the arm holding the sword should react".
- **Today's behaviour** (test 77 before the change, 4/8): your sword on its blade's middle or tip at 3 m/s, it unaware:
  nothing (went through); at 5 or 8 m/s (a deliberate strike, 4.5+), caught out: disarmed, even on the tip; a fist at
  5 m/s: disarmed; into its raised parry (beat 1): disarmed by the roll; the melee's cut through its blade into its
  chest: disarmed and the full 30 damage. Aware and facing you, 4 m/s: parried (as it should).
- **Now** (QC/vr_armedgrunt_parry.qc `VR_AG_BladeMet`, `VR_AG_BladeContact`): a blow met a melee-class weapon's line
  further than `vr_agrunt_blade_hand_zone` (6 units) from its hand and forearm (the weapon's point met, against the hand
  line): into its raised parry a deliberate strike is that parry (no disarm roll); aware and facing you a lesser blow
  the parry's own rule; otherwise a blade contact: sparks (impact table metal/blow row, a metal thing in your hand) and
  the clang (metal/blade row; a fist or club: the metal knock at half volume), haptics, your hand knocked back along
  the blow (handimpact), the blow stopped there: no damage (T_Damage_VRMelee returns at once: `vr_agbc_stopped`), and
  that motion's later blows on it stop too (0.3 s mark, `.vr_agbc_by/.vr_agbc_mark`). At `vr_agrunt_blade_stagger_speed`
  (6 m/s) or faster it staggers `vr_agrunt_blade_stagger_time` (0.5 s; vr_parry_stagger's pain pose, no ragdoll) and its
  weapon arm is knocked along the blow (hitreact on its hand, part 2, share `vr_agrunt_blade_arm` 0.5 per 10 m/s). Under
  `vr_agrunt_blade_least` (1.5 m/s) or an open hand: no contact. The hand's disarm rules, guns' and throws' unchanged.
- Tunables (Debug > Tunables, "Combat > Armed Grunts - Blade Contact"): Hand Zone, Least Speed, Stagger Threshold,
  Stagger Time, Arm Impulse, Sparks and Sound (`vr_agrunt_blade_fx` 1).
- **Test 77** (`vr_agrunt_test 77`, Debug > Tests > Armed Grunts: Blade on Blade; QC/vr_armedgrunt_blade_test.qc):
  12/12. Regression: agrunt 31, 35, 36, 65, 94, 96, 97, 98, 99, melee_phase, empty_melee, parryinterrupt pass; 60 5/1
  (the ogre case, as in the baseline), parry_pose times out (as in the baseline); eval.sh as before.
- **Worn pack drag (open):** `vr_wornpack_test 1` fails "dragged slowly by the pack: kept" (1557-2045 N peak). With
  `vr_worn_backpack_debug 2` (new: each hold's force and the held point's lag) the hold reads 0 N with the held point
  already 3.6 units off the hand on its first frame, then the body is driven 12 units away from a still hand at the
  hold's 4000 N cap within 10 frames, settling to 1-4 units' lag. Not self-collision (`vr_ragdoll_self_collide 0`: the
  same), not the fist's reach body (skipping it for pack holds: the same numbers), not the test's hand going on into the
  pack (the hand kept where the grip closed: the same). The mock controller is aimed at the box's middle while the drawn
  hand stops a unit off the box: next, compare the grab's `handPose` (controller) with `p.handpos` and the box at the
  grab (the hold taking a point inside the pack the controller reached).
- **Test 92 phase 1 (open, not run):** both holds now take the same light hand part (the grip fix); two springs on one
  part halve each one's stretch, so the summed pull (`f1 + f2 x vr_agrunt_tug_two_hand_mult`, measure 2: stretch x
  stiffness) is about one hand's, and two hands gain only the eased thresholds (0.75): the 0.85x bar is borderline by
  design. A fix would add the second hand's pull as its own (e.g. the arm's resistance split, or the stretch of the
  combined hold), a design choice for the author.

Checklist:
- [bladecontact] Hold a sword and strike an armed sword grunt's blade (middle or tip): sparks and a clang, your hand
  kicked back, it takes no damage; it keeps its sword.
- [bladecontact] Strike its blade hard (a full swing): it staggers briefly and its sword arm is knocked along your blow.
- [bladecontact] Strike its sword hand instead: it can still be disarmed as before.
## The armed Knight in the Pose Studio (branch `agent/knightpose`)

The Pose Keys test with the armed Knight + shotgun (`vr_rigstudio monster_vr_knight 4`) was 7/4 (the grunt 13/1). Causes:

1. **No left-hand grip offset handle**: the knight (aggrip kind 2) had no "All Weapons" Second grip row, and his hands'
   grip frames are turned as his rest pose's sword, so his off hand on the shotgun's fore-end was 166 degrees off his
   forearm (`vr_agrunt_ik_debug 4`: "wrist bend 166 ... not"), never taken: no second grip, no handle. Now
   `{2, Second, 0 0 0, -54.5 176.5 74}` in `globalDefaults` (`vr_aggrip.cpp`): the turn that leaves his wrist straight in
   Aim (bend 5; the grunt's tuned row has 32). The grunt's row does not fit his rig (166 again). Tunable in the Grip Editor.
2. **The Pickup pelvis drag "timed out"**: a test bug. The settle check measured a turn by `2 acos(dot)`, which reads
   0.056 degrees between a turn and itself when the dot rounds to 0.99999994 (the knight's shotgun in Pickup, identical
   frame after frame): over its 0.05 limit, never settled. `quatDeg` (`vr_rigstudio.cpp`) now takes atan2 of the
   difference's axis part; the settle and the steps' checks share it.
3. **The elbow step** fails for the grunt too (the known failure above): unchanged, as the target asked.
4. `agholds::poseClass` gave kind 2 `Gun` for every gun (the knight's nail pistols posed as rifles): now the grunt's Pose
   Category, as `agholds::hands` already did.

Checked (before = origin/next): posekeys knight + shotgun 13/1 (the grunt's elbow failure), knight + sword (13) 10/2 =
before, grunt 13/1 = before, enforcer 9/3 = before (his grip offset and weapon forward steps fail on origin/next too);
`vr_rig2_check` 22/13, `progs/vrknight.mdl` 12/23 = before; `vr_agrunt_test` 1 17/0, 2 4/0, 20 3/0, 60 6/0, 95 7/0 = before.

Checklist:
- [knightpose] vr_agknight 1, Rig Studio, Subject: armed Knight with a shotgun, Aim: his left hand holds the fore-end,
  wrist straight, and Pose Keys shows the "left hand: grip offset" dot; drag it: the hand follows on the gun.
- [knightpose] Toolgun: an armed Knight with a nail pistol aims it in the pistol pose (off hand free or cupping), not
  shouldered as a rifle.

## Death knights as armed humanoids (branch `agent/dknight`; rig, kind 3, spell, grips, armour done; rig2 tuning open)

His words: "Add full support for Death Knights to the armed humanoid system, including poses, melee, ranged, etc. The
Death Knights should still be able to perform their signature fireball ranged attack." And: "Ensure that the new armed
humanoid Knight and Death Knight kinds also support armor."

1. **Rig (done).** `progs/vrhknight.mdl` (`make_vrgrunt.py` row `vrhknight`: hknight.mdl without his sword's 110
   triangles). `quakevr/rigs/vrhknight.rig2.txt` (`make_rig2.py hknight`: `HKNIGHT_SKELETON`, 20 bones read off his
   mesh: the waist joint at the belt where the body's mesh overlaps the skirt; the jaw and helmet the head's, the back of
   the neck not; each hip's hem ring, the knees' backs and the left cuff's top tied; the gauntlets' back corners the
   hands'; no_skip on both arms; the pauldrons, their spikes, the elbow spikes rigid on the upper arms, the horns on the
   head; knees blend 4.5; limits the grunt's). `vr_rig2_check progs/vrhknight.mdl`: **25 passed, 10 failed** (the
   grunt's 22/13); all poses: see-through 0 (grunt 3), flipped 63 (599), collapsed 51 (49), folds 59 (522); random
   1000: see-through 0, flipped 61 (grunt 515). Left failing: hands' stretch 0.29 (the 0.3 bar), the knees' deepest
   bends (100-130: collapse at the back), neck+head 2 folds. Engine: model id `VrHknight`, `monsterFiles`, its ragdoll
   row (the hell knight's seeds).
2. **Kind 3 (done).** QC `VR_AM_DKNIGHT` 3 (`VR_AM_COUNT` 4): row (fist 79 67 75, the stillest in the sword's space,
   2.2-2.6 rms; pommel 55/56, tip 46/45, guard ends 351/342; stand 0..8, run 29..36 with his steps, shove slice1..6,
   take death2..6 (no kneel: down on his knees, hand 12 over the floor), back 105/106 and 327/330, anchor 74, shoulders
   0.5 -+13.5 24, arm 16.4, fit 1 1.44 1 / 0.2 0.04 8.8). Engine: `agpose::numKinds`/`aggrip::numKinds` 4, `fitScale`
   1 1.44 1.15, kind names (Death Knight), `armedRigs` row (torso sides 0.6, as the enforcer's pads), toolgun list
   refresh on `vr_agdknight`. **`vr_agdknight`** (default 0; Armed Humanoid Replacements: "Armed Death Knights");
   `monster_vr_hknight` (any weapon, the sword by default; FGD); `monster_hell_knight` calls `VR_AKN_Replace` (map death
   knights keep their sword); `hknight_die` runs `VR_AG_Dying`; `CheckForCharge` skipped and `hknight_run1` hands over
   to the layer's run for an armed one. Ranged pickups: the knights' one setting, `vr_agknight_ranged_pickup`, for both.
   **Poses: the grunt's, fitted** (`shippedFrom` 0): measured both, the grunt-fitted rest hold put his sword 8.1 units
   from his own fist, the enforcer-fitted 16 (test 95's spawn check failed at 16); Pose Keys 9/3 either way. Weapon Holds
   the grunt's (as the knight's). The toolgun's armed list with `vr_agdknight` on lists rows 0..3 (the knight's row too).
3. **The spell (done):** `vr_am_spell` (the row's own ranged attack: `hknight_magicc1`); `VR_AM_SpellTry` in
   `VR_AG_CheckAttack`'s melee branch: out of reach with a melee weapon or fists, at near/mid range in a clear line, id's
   chances (0.2 near, 0.05 mid, times vr_agrunt_ai_aggression): his magicc frames, knightspikes and sounds; with a gun
   the gun's path. Disarmed (fists) the same branch casts.
- **Test 95 with `vr_agrunt_test_kind 3`** (QC/vr_armedknight_test.qc, `map vrtesthall;wait30;vr_agrunt_test 95`): off,
  on (kind 3, vrhknight.mdl), spawn (sword 8.1 from his fist), swing (hurt you 11 in 1.5 s), **spell** (moved 200-300
  away: cast in 0.8-1.8 s, sword still held), disarm, re-arm (1.1 s), death: 8/0. Kind 2: 7/0 (as before); test 1 (kind
  0) 17/0. Rig Studio Pose Keys, death knight + sword: 9/3 (knight + sword 10/2). Note: `god` makes the swing case fail.
- **Open (next session, in this order):** a Second grip row for kind 3 (wrist straight on a fore-end: measure with
  `vr_agrunt_ik_debug 4`, as the knight's), posekeys with a shotgun, `vr_rigstudio_rom test` on the subject; tests 1, 2,
  20, 60, 99, 77 with kind 3; stage 4 (armour for kinds 2 and 3: vr_agarmor zones from their rig bones, plates, helmet,
  blade sparks; tests for kinds 2 and 3); his rig's Range of Motion tuning; eval.sh.

**Part 2 (same branch).**
- **(a) Second grip row, kind 3:** `{3, Second, 0 0 0, -161 37 -55}` (`vr_aggrip.cpp` globalDefaults): with no row his
  wrist bent 93 degrees on the shotgun's fore-end in Aim; a search over the three angles (Rig Studio, `vr_agrunt_ik_debug
  4`, 45 / 10 / 3 degree grids) leaves it 1 (held). Tunable in the Grip Editor.
- **(b) Rig Studio:** Pose Keys, death knight + shotgun **13/1** (the grunt's elbow failure only; the target), + sword
  9/3 (as before: no second grip on a sword). `vr_rigstudio_rom test` 9/0 (grunt 9/0), `rom test weapon` 6/0 with the
  shotgun and with the sword. `vr_rig2_check progs/vrhknight.mdl` 25/10 unchanged (not tuned further: budget; the
  failures are hand_l stretch 0.29, the knees' 100-130 degree bends collapsing the back of the knee, neck+head 2 folds).
  Note: `vr_rigstudio_rom test` on the grunt rewrote `quakevr/rigs/vrgrunt.rig2.txt` (put back; a test side effect).
- **(c) agrunt tests with `vr_agrunt_test_kind 3`:** 1 17/0, 2 4/0, 20 3/0, 60 5/1 (the ogre case, failing for kind 0
  too), 77 12/0, 99 38/0 (kind 0: 41/0). No fixes needed.
- **(d) Armour for kinds 2 and 3:** `wearers` (and the self-check's list) take `vrknight.mdl` and `vrhknight.mdl`;
  QC `VR_AgArmor_Model` their plates, `VR_AgArmor_Spawn` rolls for kinds 0-3. The shells from their own rigs, three
  generic fixes (the grunt's and the enforcer's shells byte-identical: hashes fa858c7f, 11cc98dc):
  - a piece's surface keeps each edge to two triangles (the knights' meshes share edges three ways): shells closed
    (open edges 10 / 56 -> 0);
  - the helmet's fit only from the head proper (1.06 of the head bone's length up, 0.75 to the sides): the death
    knight's horns had made it 16 high and 22 wide (now 11);
  - a bone no vertex weighs half on (the knight's right upper arm, 0.38 at most) gets the region at 0.8 of its most
    (plate anchors too): his right pauldron had no plate at all.
  `vr_agarmor_check` 20/0 (all four wearers). **`vr_agarmor_test 7`** (rule 1; `map vrtesthall;wait30;vr_agarmor_test 7`,
  ends on `agarmor test: N passed`): per knight kind its pieces at their places, the cuirass absorbing, a pauldron's
  debris its own model's, a blade's blow on a plate sparking with no blood, the skill's roll, its wear reaching the
  client, the debris fading: **13/1**: the death knight's left pauldron's and shins' plate middles (the probe points)
  resolve to no plate (his big pauldrons' middles sit inside his arm, his shins under his skirt): open. `8`: a knight
  and a death knight in full yellow (eyeshots). `vr_agarmor_test 1` 26/0, `5` 24/0 (its "right thigh and shin worn"
  case flaky, failing on the base too).
- **Open, in order:** the death knight's probe points / shin and pauldron hits (test 7's failure); eyeshots of test 8;
  eval.sh; `run_baseline --only agrunt`; his rig's remaining rig2 failures.

**Part 3 (same branch).**
- **Test 7's 13/1: the probe points were wrong, not the game.** The plates' middles (`limbpiece 10`) of the death
  knight's left pauldron and both shins sit inside his arm / leg, nearer another bone's skin (no plate within reach);
  shots at them strike the plates. The tests now probe a limb plate as a player's shot lands (`VR_ART_PlateShot`,
  generic for every wearer): aimed at its middle from ahead of the wearer, then its side, the other side, behind,
  traced with MOVE_HITMODEL (as bullets), the piece taken where it lands (`ragdollbone` QVR_ARMOR_PIECE_AT); the cuirass
  and helmet stay their middles (pieceAt resolves them by their bones; a shot at the enforcer's chest from ahead meets
  his hands on the gun: no plate, rightly). All ten pieces checked now (was 4). From ahead every death knight plate is
  struck; the knight's right pauldron and left shin from their side (from ahead his thigh plate is in front of the shin's
  middle). `vr_agarmor_test 7` **14/0**, `vr_agarmor_test 1` **26/0** (the enforcer's check is the same probe).
  (Note: `traceline(..., MOVE_HITMODEL, world)` meets no monster: pass the player.)
- **Test 8 (eyeshots, `map vrtesthall;wait30;vr_agarmor_test 8;wait10;screenshot`):** knight and death knight in full
  yellow: helmet, cuirass, pauldrons, vambraces, thigh and shin plates drawn on both; nothing to fix. Seen: the
  knight's own red shoulder caps show above his pauldron plates (his mesh's pads, weighted to the chest); the death
  knight's upper thighs under the skirt's hem are bare (the thigh plate starts at the hem's bottom).
- **`vr_rigstudio_rom test` no longer writes the shipped rig:** its save-and-read-back step sets `vr_rigdata_dir
  rigstudio_test` for the save when it is empty (as the other rig tests) and puts the setting and the rig's path back:
  9/0, saved to `quakevr/rigstudio_test/rigs/vrgrunt.rig2.txt`, `quakevr/rigs` untouched. Run it flat-screen
  (`-flatscreenmode`: the studio's frame does not run under the mock headset; `map vrtesthall;wait30;vr_rigstudio
  monster_vr_grunt 4;vr_rigstudio_rom test`).
- **eval.sh:** 163/171, no verdict changes (8 takes' numbers moved, PASS -> PASS; nothing here touches melee).
- **`run_baseline --only agrunt`:** 93 cases; changed vs baseline.tsv: agrunt_2 4/0 -> 2/2 (4/0 run alone: flaky under
  load), 31/36/65 FAIL -> PASS, 32/70 one more check; flaky 63/71/86 as known. New agrunt_78 8/1 (directional parry,
  from origin/next, fails alone too: not this branch's).
- **Open:** the rig2 check failures on vrhknight (25/10; step 5, not started: budget).

**Rig polish (branch `agent/hkrig`).** `vr_rig2_check progs/vrhknight.mdl` **34 passed, 1 failed** (was 25/10): every
joint and pair clean; left failing: the 1000 random poses (flipped 37, collapsed 5, folds 36, was 61/41/55: the helmet's
rim and the right shoulder's top under the pauldron as the head and the arm move together; no limb pose). All in
`make_rig2.py`'s hknight rig, no shared code (the other rigs' files byte-identical; grunt 22/13, soldier 22/13, enforcer
and enforcer.mdl 28/7, knight 12/23, as before):
- hands (stretch 0.29, fold 173): each gauntlet's cuff back (its four corners by the pommel, 464-466/473 and
  478/484/492/494) painted hand 0.9 forearm 0.1 (half of each cuff was the forearm's, the cuff's edge squeezed as the
  wrist turned);
- knees (the back collapsing at 100-130): the knees' range 125 (the grunt's 130; the enforcer's 115), the left knee
  nudged half a unit toward its back (its back's edge stretched 2.8), the back of the right knee's ring (387/389/390)
  thigh 0.64 shin 0.36 (half and half, it rose into the hem's back);
- neck+head (2 folds, chin and helmet rim squeezed to 0.23): the throat under the jaw (74) head 0.55 (0.39), the neck's
  nod 25 forward / 15 back and tilt 15 (the grunt's 35 / 25 / 25), the head's 20 back (25): his helmet sits on his
  shoulders; the right shoulder's top beside the helmet (419, 421) less the head's (0.35 / 0.27 -> 0.25 / 0.15).
- `vr_rig2_check <model> <lines>` details print the stretched edges too (an edge past 2.5 or under 0.3, its vertices'
  weights), as the flips and folds.
- Tried, worse: knee blends 2-3.5, nudges past 0.5, the legs held to their blends, a reach (also only the head's),
  smaller shoulder webs; the knees back to 130 (thigh_l+shin_l: a fold).
- **Pauldrons on the clavicles** (his: "they seem to rotate too much with the arms"): each pauldron and its three
  spikes ride the clavicle rigidly (the elbow spikes stay the upper arms'); rig check unchanged 34/1, self-intersections
  50 -> 30. Not a bone of their own: the ragdoll turns a `fixed` bone into a loose one (dropped as the gun is), and a
  ball bone would add two ragdoll bodies and still need a driver to move at all: on the clavicle it is the same motion.
  `vr_agarmor_test 7` **14/0** (run with `developer 1;` first: without it the run printed nothing and timed out).
  Open: the armour's pauldron plates still ride the upper arms.
- Tests with `vr_agrunt_test_kind 3`: 95 8/0, 1 17/0 (13/4 once under load: 4 guns 0 shots), 2 4/0, 20 3/0, 60 5/1
  (the ogre case, as before), 77 12/0, 99 38/0.
- **Part 2: the pauldron plates follow the mesh pauldrons.** With his pauldrons on the clavicles they had left the upper
  arm's plate region: his pauldron plates shrank to the arm's skin (outer vertices 112 -> 63 left, 82 -> 59 right) and
  the mesh pauldrons went bare. `vr_agarmor.cpp` `girdleShells` (generic): a pauldron's region also takes every body
  part (triangles joined to no other) wholly on the upper arm's parent bone with its middle nearer the shoulder than the
  girdle's pivot; the plate there keeps those weights, so it rides the clavicle with the mesh pauldron, the arm's part
  the arm (left plate: clavicle 0.53, upper arm 0.36). Shells: grunt fa858c7f, soldier a1488e62, enforcers 11cc98dc,
  knight e21a7335 unchanged; death knight fd199a2c -> ee700e65 (origin/next's 1105fd08). `vr_agarmor_check` 20/0,
  `vr_agarmor_test 7` 14/0, `1` 26/0. Shots (Rig Studio, right arm raised high and across the chest): before, his
  plated pauldron swung up with the arm and across his chest; now it stays on the shoulder, the arm's plate moves.
- **`vr_agarmor_test 7` never hung:** its lines are dprints and its verdict too: a run without `developer 1` prints
  nothing and ends on its timeout. Always `developer 1;map vrtesthall;wait30;vr_agarmor_test 7`.
- **`vr_agarmor_test 9` (new):** a knight and a death knight in full yellow killed (health 1, small blows), their
  ragdolls awaited, then 3 s: both pauldrons still worn, each body within 48 units of where it died; then
  `vr_ragdoll_speeds` / `vr_ragdoll_list` and "agarmor deaths over" (the run's -QuitOn). **6/0**; fastest part 270
  (knight) / 332 (death knight) u/s, 20 parts each, no blow-up; eyeshot: the corpse's plates on.
- **Open: Pose Keys, death knight + shotgun 6/5** (origin/next's rig 13/1; + sword 3/6). Bisected over rig variants
  (`vr_rigdata_dir`): the knees, neck limits, cuffs, shoulder weights and pauldrons are innocent; **vertex 74's weight
  alone** (the throat under the jaw: head 0.39 -> 0.55, part 1's neck fold fix) turns 13/1 into 6/5 (the left hand's
  grip part never shown, the weapon's turn 23 deg for 20, the Pickup never settled). Something reads a bone frame from
  the vertices' weights (a heaviest-vertex pick?); not fixed (budget): either find that reader or drop 74's change in
  `make_rig2.py` (the neck+head fold may come back).
- **Part 3: the Pose Keys regression explained, gone.** The reader is the arms' body proxies (`vr_agbody.cpp` `fit`,
  from each bone's ragdoll hull points: the vertices weighing 0.5 or more on it, else those it is the heaviest of when
  under 4). On origin/next's rig the death knight's chest hull had **4** such points (74 one of them, chest 0.54), under
  the proxies' 8: `agbody::of` failed and he had **no body proxies at all** (his arms never kept out of his body). 74 to
  the head left the chest 3, the hull fell back to the chest's heaviest vertices and he got proxies (torso 5.8 front,
  head 6 front): the arms' pushes out of the body changed his Aim. Not a frame read from weights; the branch's rig is
  the right one (proxies, as every other armed humanoid). With his poses shipped (origin/next, posepromo/kngrip)
  Pose Keys + shotgun is **13/1** on the branch (the elbow step, as on the grunt); + sword 3/6 on both rigs (the test's
  steps are a gun's: the off hand's grip, the right weapon, "never shown"). 74 kept. `agbody` now says (developer,
  once a model) when a model gets no proxies. Not done: a sturdier proxy point set (a bone with 4-7 hull points still
  gets none; changing the hull's fallback would change other rigs' ragdoll hulls).
- **`vr_agarmor_test`'s lines print without `developer 1`** (VR_TEST_LOUD: its checks and verdict, every request).
- Regression subset (`run_baseline.sh harness --no-bench --only 'ragdoll|agrunt|agarmor|rigstudio'`, 107 cases):
  9 changed against baseline.tsv: better agrunt 31/32/36/65/70/85 (as on next); agrunt_11 a timeout under load (77 s
  PASS rerun); ragdoll_climb a timeout under load (24/4 rerun, as the baseline); ragdoll_stagger 51/6 (52/5), the same
  51/6 in the armor and review agents' runs of the hour (next's, not this branch's). Flaky ones only flaky.


## Directional parry (experimental; branch `agent/dirparry`)

His words (2026-10-31): parry armed humanoids' melee by the direction of the enemy weapon and strike: overhead strikes
by holding your weapon horizontally, horizontal strikes by holding it vertically; no poses written in; the parrying
weapon square to the enemy weapon, physically meeting it; sparks where they collide.
- **`vr_dirparry`** (0, Combat > Parry: "Directional Parry (Experimental)"; QC/vr_dirparry.qc). On: an armed humanoid's
  melee-weapon swing (VR_AGP_SWING; not the chainsaw's thrust, not punches, butts or shoves) is judged only by this rule
  (the level-across rule still judges every other blow; fists: the crossed arms as before). Off: everything as before.
- **Its side:** every frame of the strike its blade's line as drawn (VR_AG_WeaponLine) and last frame's: its way is the
  turn of the blade (the plane it sweeps: the hand's own forward motion, which makes the blade go along its length at
  the blow, is left out), else its point's motion square to it. **Yours:** each hand's weapon line from the hand to its
  far end (VR_Parry_WeaponLine / VR_Melee_Line: a sword's blade, a gun's barrel to its tip).
- **Score:** your line's angle off square to both its blade and its way: full marks within `vr_dirparry_angle` (40),
  none at 90; times the meeting (1 within `vr_dirparry_contact`, 6 units, 0 at twice it). `vr_dirparry_threshold`
  (0.75) parries: no damage, sparks (VR_ImpactFx metal) and the metal clang at the middle of the two lines' nearest
  points, the parry's own sound, haptics, stamina, counter and stagger (VR_Parry), the swing stopped (no blow; a
  `vr_dirparry_opening` 0.5 s flinch if the parry balance gave none). Under it: `vr_dirparry_partial` (0: off).
- **Timing:** the AG's blow is dealt at 55% of its strike, before its blade reaches you; so with your weapon within 48
  units of its way at the blow, the blow is held back for `vr_dirparry_window` (0.1 s) while its blade is followed
  frame by frame: met across, parried; else it lands then (T_Damage). Met before the blow: parried there.
- **Debug:** Show Disarm Volumes draws your line (green parried, red not), its blade where they met (white), its way
  (yellow) and the score label (vr_disarmviz.cpp kinds 12-14). Tunables group "Combat > Directional Parry".
- **Test 78** (`vr_agrunt_test 78`, Debug > Tests > Armed Grunts: Directional Parry; QC/vr_dirparry_test.qc): 8/1.
  Your weapon is posed in QC each frame (hand, turn, muzzle: the mock places no weapon muzzle) at its blade's place 3/4
  into a reference swing. Overhead vs sword level across: parried (45 deg off square, gap 5.6); horizontal vs upright:
  parried (14 deg); horizontal vs level across: lands; shotgun level across vs overhead: parried (46 deg); 40 units
  aside: lands; vr_dirparry 0: the old rule (no reading); sparks at the meeting, within half the contact distance of
  both lines. **FAIL: overhead vs upright parries** (33 deg off square): the grunt's drawn "overhead" (swing kind 0) is
  a diagonal cut where it comes onto you: its blade turns about an axis about 45 degrees between level and upright (the
  tip goes about 10 units sideways for 17 down), so both guards are square enough. The measure is right for the swing
  as drawn; making overheads read as overheads needs either a truer overhead animation (its keys in VR_AGP_SwingKeys) or
  a tighter angle (about 30 deg, which the test's level guard at 45 would then fail too). Not tuned: his call.
- Not done: the non-humanoid expansion (a knight's or ogre's swing from its animation frames): the humanoid case does not
  pass robustly yet (above). eval.sh as before (no leg takes; melee untouched).
- Also: `VR_AG_SegDist` now leaves its nearest points' shares in `vr_ag_seg_s`/`_t`.

Checklist:
- [dirparry] Combat > Parry: Directional Parry on. A sword grunt's horizontal swing: hold your sword upright where its
  blade comes: sparks where the blades meet, no damage, it flinches; held level across instead: the blow lands.
- [dirparry] Its overhead swing: hold your sword or shotgun level across above your head where its blade comes down:
  parried, sparks at the contact. (Known: upright also parries its current overhead, a diagonal cut.)
- [dirparry] Debug > Show Disarm Volumes during its swings: your weapon's line (green/red), its blade, its way, the score.
## Two-handed tug pulls for real; the worn pack drag (branch `agent/tugpull`)

His decision: a second hand on a tug adds real pull.
- **Why two hands were no stronger:** not the shared hand part. The tug's pull is the stretch (ragdolltug 3: the hand's
  flat distance from where it stands its ground, its pelvis pin, beyond its distance at the hold; per hand, from the
  controller), summed over both hands (`f1 + f2 x vr_agrunt_tug_two_hand_mult`) already. Test 92's second hand holds
  the gun's far end, by its chest, about 2 units past its pelvis from you: pulling it back shrank that distance, so its
  stretch read 0 the whole tug and only the 0.75 eased thresholds helped (0.86x one hand's time, 3 runs in 5 over 0.85x).
- **Now** (box3d `tugAwayOf`): a standing tug's hand past where it stands from you reads its pull towards you (the
  stretch along the flat way from its pelvis to you); every other hand reads the reach as before. Downed/staggering
  tugs (ragdolltug 6) keep the reach (towards you, test 91's lying grip never won: torn away). Both kinds sum the hands
  in VR_AG_TugRagFrame (one path). `vr_agrunt_tug_two_hand_mult` 1 -> **0.6** (Debug > Tunables, "Tug: Second Hand's
  Pull"): two hands win in 0.94 s against one hand's 1.36 s (0.69x; 1.0 gave 0.50-0.57x, 0.5 0.68x).
- **Test 92:** bar 0.75x (was 0.85); each phase starts from phase 0's place (the player drifted ~15 units a phase and
  seed 1's sword grunt fell off the hall's ledge, "no floor": torn away). 22/0 on seeds 1-5. Test 91 9/9 (seeds 1-3), tug
  group 23, 30-38, 74 pass, 29 fails as in the baseline; ragdoll_grab 19/0.
- **Worn pack drag ("dragged slowly by the pack: kept"):** the cause was the test: it put the player 20 units from the
  pack at his own height, but B had landed below him (its pack 10 units under his feet): he fell, his hand (placed in
  the play space) falling with him, and the hold dragged the pack down at its 4000 N cap. Now he stands on B's floor:
  39/0, hardest pull 1385 N (rip 1500). Also fixed: a pack hold began stretched (the palm fit moved the held point 4.5
  units back out of the pack while the joint held it at the controller); `startHold(still)` now holds the pack's point
  where it is from the hand, so the hold starts at 0 N, 0 units. `vr_worn_backpack_debug 2` shows your fall speed.

Checklist:
- [tugpull] Grab an armed grunt's gun with one hand and pull back steadily: note how long it takes; then grab another's
  gun with both hands (one on the barrel, one by its body) and pull: it comes out clearly sooner (about two thirds).
- [tugpull] Grab a grunt's worn backpack (knocked down or standing) and walk slowly backwards: it is dragged by the pack, which
  stays on; yank hard: the pack tears off into your hand.
## Two-handed recoil for any grip on the gun (branch `agent/offhandrecoil`)

His words: "Ensure that the 2H recoil reduction multipliers not only apply to the 2H aiming hotspots, but also when a
gun is held by the off-hand on any arbitrary point."

- The one input is `twohand::support(hand)` (read by `weaponfx::twoHandRecoil`: kick back, muzzle rise, return time,
  haptic punch; and by the weight's two-handed share and bullet time's two-handed test): the other hand gripping the
  gun at a hotspot (its aim transition) or anywhere on it (a free grip, `vr_weapon_grab_anywhere`, the carried
  weapon's too), whichever is more. A free grip already counted while held, but dropped to one-handed the frame it was
  let go of; now it eases out as a hotspot does (5 a second: two frames after letting go 0.861, shares 0.656 at
  0.6), also for a carried weapon (applyCarried eases its `t` out; before, a let-go carried grip's `t` stayed).
- Decisions: a grip anywhere counts fully, however far from the barrel or the handle (as asked: any point); a weapon
  with Two-Handed: Not Allowed held anywhere still gets the reduction (the hand holds it; only its aim and the server's
  two-hand state ignore that grip). A hand holding something (a shell, a magazine, a gun of its own: `held::handEmpty`)
  never grips the gun, and a hand only touching it doesn't either: one-handed. Gripping a gun's seated magazine where
  the magazine is its two-handed grip (nailguns, thunderbolt) is that gun's hotspot: two-handed, as before. A fist
  within `vr_weapon_grab_anywhere_min` (12 cm) of the handle that isn't a cup hotspot holds nothing (the game doesn't
  draw it on the gun): one-handed, unchanged.
- Debug > Tests: "Print Two-Handed Recoil" (`vr_recoil_2h_print`): each hand's support, the hotspot's share of it, how
  the other hand helps (0, 1 hotspot, 2 anywhere) and the four shares its next shot takes.
- Test `Misc/quakevr/twohand/offhand_recoil_test.sh <agent>` (e1m1, the shotgun in the main hand; ends on its own
  "recoil2h done"): one-handed 1; the hotspot gripped 0.6 x4; five free grips (the shotgun's top at 0.4-0.5 of its
  length; the thunderbolt by its muzzle, 11.8 units from the handle) the hotspot's 0.6 x4, each easing out (0.861) and
  back to 1 when let go; the off hand on the body not gripping, and holding a shell from the pouch on the body and on
  the hotspot: 1. "offhandrecoil: 24 passed, 0 failed". The shotgun's own hotspot is generous (most of its length takes
  it, 2.5-5 units off its point), so the free grips are where it isn't. `vr_recoil_test` 127/0, `vr_kickcancel_test`
  79/0, `gunfx_test.py` 73/0.

Checklist:
- [offhandrecoil] Hold the shotgun (or any gun) and grip it with the off hand somewhere that is not its two-handed
  hotspot (e.g. its top, near the muzzle): the shots kick as little as with the hotspot (Weapon Effects' Two-Handed
  sliders); let go: the next shots kick one-handed again, the change smooth.
- [offhandrecoil] Off hand holding a shell or magazine against the gun while firing: full one-handed kick.

## Worn backpack: a hard pull lifts its wearer instead of ripping (branch `agent/packragdoll`)

His words: "Add an option to disable ripping out worn backpacks by tugging ... and instead make it so that pulling an
enemy from their backpack with enough force is a ragdoll trigger that behaves similarly to a torso/leg lift ... Ensure
the backpack stays connected to the active ragdoll and transfers the force to the active ragdoll."
- **Setting** `vr_worn_backpack_pull` 0 (Combat > Armed Grunts > Worn Backpacks, "Pulling the Pack": Tears It Off /
  Lifts Its Wearer; experimental). 0 is today's rip. 1: a hand's pull never tears the pack off (the standing tug's
  stretch, a ragdoll held by the pack, living or dead); hits (shots, blows, projectiles), the damage pool, fire and
  blasts, landings, gibbing and the force grab/taking of a dead one's pack still do.
- **The lift** (`vr_worn_backpack_lift_force` 1500 N, "Pull That Lifts", kept up `vr_worn_backpack_rip_time`; QC
  `VR_WP_Lift`, engine `foegrab::packLift` via `ragdollbone(g, [8, player, 0], QVR_WORNPACK)`):
  - standing, held by the pack (the foegrab pack hold): the pull is the hands' stretch x `vr_worn_backpack_rip_stiffness`
    (150 N a unit; two hands x `_two_hand_mult`). Past it, it is knocked down as the two-hand lift does
    (`VR_FoeGrab_Lift`: launched up at `vr_foegrab_lift_up`, its get-up held off), and each hand on the pack holds its
    ragdoll **by the pack** (`ragdollHoldSoft(..., pack)`: the pack shape's point nearest the hand, on the chest part
    carrying it, the hold `onPack`; within 48 units; the lift's strength `vr_foegrab_lift_strength` x its weight at 8
    Hz). The pack is a shape of the chest part's body (createRagdollPack), so it is welded there and the hand's pull
    goes through it into the body. It flails, is carried and thrown as a lift's, breaks free after
    `vr_foegrab_lift_time` (foegrab's carryStep).
  - knocked down, held by the pack (the usual ragdoll hold): the hold's force (as the rip read it). Past it, the holds
    go on (at Grip Strength, not softened) and it becomes a lifted one: flailing, its get-up held off, breaking free.
    A staggering one is not lifted (a hand on it takes it down first). A gentler pull drags it as before.
  - dead: never lifted, never torn by a pull (a hand on a dead one's pack takes the pack, as before).
- **Test** `vr_wornpack_test 3` (Debug > Tests > "Worn Backpacks: Pulling Lifts"; e1m1, `god; notarget; developer 1`):
  **12 passed, 0 failed**: G held by its pack standing; a gentle pull (4 units back, at most 782 N) neither lifts nor
  tears; 1.5 m/s back lifts it after 0.22 s at 1846 N (a ragdoll, knocked down, the pack on, held by the pack at
  2632 N); carried 25.8 of the hand's 30 units aside; thrown 41.7 units; the pack's middle 16.4 units from the chest
  bone, 0.00 off that throughout (`ragdollbone` what 9), 0 rips; B knocked down: dragged slowly (hardest 1357 N): kept,
  not lifted; yanked: lifted at 4000 N, never torn; A's pack hit for 40 still tears it; D dead, its pack force-grabbed
  still comes off. Mode 0: `vr_wornpack_test 1` 39/0 (as on next); judo `lift` 9/1 (the ogre case's expected message
  is stale: it is not lifted, "its monster group's Lift is off"); ragdoll grab_test as the baseline.
- **Decisions** (he was asleep): the brief's "within 3 u of the chest bone" read as "within 3 units of its place on the
  chest bone" (the pack's middle sits 16 units from that bone; it doesn't move off it: 0.00); the standing pull's force is
  its stretch x the rip stiffness (the tug reads a stretch, not a force); one threshold for both; a knocked-down one
  keeps the usual (stiffer) ragdoll hold when lifted; the slow drag's 1357 N is close under 1500, so a brisk drag of a
  knocked-down one may lift it.

Checklist:
- [packragdoll] Worn Backpacks on, Pulling the Pack: Lifts Its Wearer. Grab a standing grunt's pack from behind and
  pull gently: nothing happens; yank back hard: it goes down flailing, held by the pack on its back; carry it, swing it
  and let go to throw it; the pack never comes off from pulling, and it breaks free after a few seconds.
- [packragdoll] Same mode: knock a grunt down, grab its pack and drag it slowly (it slides along, pack on), then yank:
  it flails in your hand, pack still on.
- [packragdoll] Same mode: shooting or hitting a grunt's pack still tears it off; force-grab a dead one's pack: it comes off.

## Knight two-pistol hold: the knights' Main and Off grips (branch `agent/kngrip`)

His ask: address knightpose's leftovers (kinds 2 and 3 had a Second grip row but no Main or Off). **Confirmed wrong.**
Measured with test 85 extended to every kind (each gun's barrel as drawn against its pose, the server's; at rest and
aimed after its shots once the arms settle), origin/next: grunt and enforcer every pair 0.1-1.2 degrees; the knight's
**left** pistol 27-28 degrees off at rest, 40-44 aimed (right 5.5-6.5 / 0.6-0.9); the death knight's left 21-23 at rest,
12-13 aimed, his right 13 aimed. Cause: with no row each hand's grip frame is turned as the rest sword in his right fist,
so a pistol sat in his hand like a sword; the left wrist went past `vr_agarms_wrist_most` (100) and the hand, and its
gun, turned back towards the forearm.

**Fix: fitted from the grunt, as the poses' shippedFrom.** `vr_aggrip.cpp`: a kind with no Main or Off row of its own
ships the hand's row of the kind it ships its poses from (`agpose::shippedKind`, new accessor of `shippedFrom`: the
knights the grunt's), turned by its fit (`gripFits`): the turn that keeps the weapon where it is on the hand, from each
hand's rest frame (x from its wrist to its middle, y its elbow's hinge; `agarms::restHand`). The fits come from the new
**`vr_agrunt_grip_fit`** (prints the rows for every kind drawn so far; test 85 runs it at its end): knight Main -10.4
106.5 66.0, Off -27.8 89.6 127.3; death knight Main 32.1 -80.3 -92.9, Off 38.8 163.8 160.4. No knight copies of the
grunt's rows: retuning the grunt's Main / Off moves the knights' with it; their Second rows stay their own; Grip Editor
entries go on top as for any kind. Grunt and enforcer untouched.

- **Test 85** (`map vrtesthall;wait30;vr_agrunt_test 85`, ends `test 85 over`): 5 new cases (enforcer two nail pistols,
  knight and death knight two nail pistols and two shotguns) and a `barrel` check per case (each gun within 12 degrees of
  its pose, rest and aimed). origin/next 36/4 (the four knight cases); now **40/0**: knights 0.4-0.5 at rest, 0.5-1.2
  aimed; grunt and enforcer numbers as before. (Unsettled, 4 frames after the shots, every kind's left gun reads 20-35
  off: the arms' goal ease after the aim's jump, `vr_agarms_jump_ease`; hence the settle.)
- Covering: Pose Keys, knight + shotgun (`-flatscreenmode`; `vr_rigstudio monster_vr_knight 4;
  vr_rigstudio_test_posekeys`) **13/1** (the grunt's elbow step, as before). Test 95 kind 2 7/0 (and 6/1 twice: "death:
  dead (40)", the armour's cuirass absorbing the test's blow; origin/next fails it the same way: kind 3 7/1 "dead (10)"),
  kind 3 8/0.

Checklist:
- [kngrip] vr_agknight 1, toolgun an armed Knight with two nail pistols (or Debug > Automation & Self-Tests: Armed
  Grunts: Dual Guns' Muzzles): both pistols point where he aims, each in his fist like a grunt's (not through it like a
  sword); the left one not turned off to the side.
- [kngrip] vr_agdknight 1: the same for an armed Death Knight with two pistols.
- [kngrip] vr_agknight 1 / vr_agdknight 1: a knight's and a death knight's own sword still sits in his fist as the
  grunt holds a sword (the fitted Main row is every weapon's; test 95's sword-to-hand 3.1 / 8.3 as before, not eyeballed).
## nail_1's backpack step: a test fault, not nails through packs (branch `agent/nail1pack`)

The step drops a backpack as a monster does (random throw and spin), waits until it rests, then fires nails at its drawn
box from 100 units before it, along the player's line to it. When the bag came to rest within 100 units of the player
(about 1 run in 4), that start lay behind him: after a nail's first frame his body overlapped it, the body touch
(vr_physics.cpp `touch`) called the test's own nail touch, which took the player for a hit and removed the nail. So no
nail met the bag and the step failed 3 checks. In the game spike_touch spares the owner (VR_OwnerSpared), so nails do not
pass through lying packs: 14 of 14 random rest poses (several different sides) were met at the drawn box once the test
was fixed (3 of 12 and 2 of 6 failed before). Fix (QC/vr_nail_test.qc only): VR_NLT_BagTouch spares the owner as
spike_touch does, and VR_NLT_BagNail starts no farther back than the player stands (min(100, his distance)), so a start
never lands behind him near a wall. Not timing: the flakiness under parallel load was the random rest place. No gameplay
change.

## His poses of 2026-10-31 shipped; the Rig Studio's subject clear of the floating text (branch `agent/posepromo`)

**The subject clear of the text.** His words: the floating texts get in the way. In vrtesthall the welcome boards
(`func_worldtext_banner` at 0 -144 62/88) stood 16 units before the stage (0 -128), between it and the default camera:
the board's text over the subject's head. `findStage` (vr_rigstudio.cpp) now checks the server's world texts
(`worldtext::serverTextList`, `halfSize`): one within 320 units of the stage whose board (1.5 times its half diagonal)
comes within 32 degrees of the default camera's line to the subject, not wholly behind the camera nor behind a wall, moves
the stage to the nearest clear place on a 48-unit grid (forward first, back towards the start costed 4x), with room for
a 64-unit hull at the subject and at the camera's eye and the floor within 24 units. vrtesthall: 96 forward, 48 left
(-48 -32 0). Every test reads `st.stage`, none a fixed place. A map without a text near: unchanged. `developer 1` prints
each place tried and why it was not taken. Screenshots (studio default camera, mock): grunt, death knight at rest and
in Pickup: no text over any.

**His pose keys, grips, IK stretch as shipped.** From his vr_agrunt_poses (186 keys: grunt 61, enforcer 59, knight 58,
death knight 8), vr_agrunt_grips (8: 3 already shipped), vr_agrunt_ik_stretch (26: 10 new or changed, all knight's but one
death knight's) of 2026-10-31, with `Misc/quakevr/pose_defaults.py --write`. The knights follow the grunt's keys
(`shippedFrom`), so their own rows had nowhere to go: `shippedOf` and `shippedStretchOf` now take a kind's own row of a
code first, else the followed kind's fitted, as an archived key does (its own rifle row is not laid on its gun
categories, as an archived one is not); `copyKind` lists a kind's own rows too. pose_defaults.py: knight names, and an
all-zero key (or a stretch of 1) keeps its row where dropping it would ship the followed kind's or the rifle's instead.
Grips: knight Main and Off, death knight Main (his rows win over agent/kngrip's fit; the death knight's Off keeps the
fit), the knight's sword Second and laser cannon Main. New: `vr_agrunt_pose_dump all` (every key and stretch in effect)
and `vr_agrunt_grip_dump effective` (every grip in effect). **Proof:** his three lines on the old tables vs the new
tables with the cvars empty: 476 keys, 48 stretch rows, 16 grip lines, byte for byte the same; again after merging
agent/kngrip (empty vs his cvars). His archived text now equals the shipped (no config version: 218 is the last).

**Knight rig.** His Range of Motion edits (`vrknight.rig2.txt`, as is): both clavicles swing x -15..35, y -15..35, twist
-65..45 (were -15..20, -10..20, -35..35); make_rig2.py's knight skeleton the same (a regenerate equals his file number for
number: only the header line and the number format differ). His rig2anim files are header-only: not added (optional).

Tests: Pose Keys grunt 13/1, knight + shotgun **14/0** (13/1 before: the elbow step passes with his poses), death knight
13/1 (the elbow step, as the grunt's). Test 85 40/0 with his knight grips (knight pistols 0.6-1.0 degrees off their pose).
run_baseline `agrunt|rigstudio|posestudio|rig2`: no regression (agrunt 31, 36, 65 FAIL -> PASS and the new tests are
other branches'; rigstudio cases as the baseline). eval.sh: 163/171 as origin/next; its few "differ" takes change from
run to run on origin/next and here alike (noise, no melee number from this branch).

Checklist:
- [posepromo] Rig Studio (flat screen): open it: the subject stands clear of the hall's welcome boards; change it to
  the knight and the death knight, Pose Keys > Pickup: no text over it.
- [posepromo] Armed knights and death knights look as in your last session with your vr_agrunt_* settings cleared
  (`vr_agrunt_poses ""; vr_agrunt_grips ""; vr_agrunt_ik_stretch ""`): your keys are the defaults now.
- [posepromo] Rig Studio > Range of Motion on the knight: the clavicles' limits are yours.

## Armed humanoids: shots and grip spheres from the drawn pose (branch `agent/stalepose`)

His ask: address drawnpose's leftovers, the two consumers still reading a stale pose.
- **Shots (and every precise hit) on a live armed humanoid:** hitmodel's `drawnOf` read the .mdl frame (its lerp), while
  the body is drawn on the authored rig (its arms posed on its weapons, the Rig Studio's takes, hit reactions): a shot at a
  raised guard's forearm met the frame's torso or head behind it, or nothing. Now `drawnpose::armedMesh` (the one drawn
  pose query's: `rig2::liveVertices`, now with its normals, placed as `drawnpose::bone` places a live bone, its placement
  shared in vr_drawnpose.cpp) gives its vertices as drawn; hitmodel tests them as it tests a ragdoll's (every triangle;
  the hit's triangle maps to the stand frame for the zone as before). Armed humanoids only (`hitreact::armsWanted`), alive
  (a ragdoll keeps its own), on a listen server's view (else the frame). Its users: segment/clip (shots, missiles, the
  hook, melee blows, thrown things), restPoint (zones), nearest/pose/anchorFrame (foe grabs, stuck axes). Not
  `vertexPoint` (QC modelvertex: the arms' own inputs stay the frame's). Setting `vr_hit_drawn_pose` (Combat > Hit
  Detection > Armed Bodies as Drawn, default 1; 0 the frame as before).
- **VR_AG_Hit's grip spheres** (a body blow within 10 units of its grip, 16 on a limb, disarms) were round the server's
  `p.origin`; now round its grip as drawn (`VR_AG_DrawnFrame`, as the hand line and the disarm volumes), the other hand's
  weapon chosen by the same; Show Disarm Volumes draws them there.
- **Test 79** (`vr_agrunt_test 79`, QC/vr_stalepose_test.qc; Debug > Tests > "Armed Grunts: Shots on the Drawn Body"):
  every kind (grunt, enforcer, knight, death knight). Guard (unarmed, frozen, held in Aim): a shot across the drawn
  forearm (`weapondrawnpose(g, 0, 37)`, new: its forearm's vertices' middle as drawn; 38 the frame's) hits within 5-6
  units of it, zone 2; one across the frame's forearm alone misses (enforcer 11.5, knights 10 units apart; the grunt's
  guard holds it 5.6 from the frame's, so its miss is tested in its swing). Swing (a sword, each swing kind in turn until
  met): its drawn grip 13.5-22 units from the server's weapon place at 0.38-0.40 s; a blow there keeps the weapon, one at
  the drawn grip disarms it. **22 passed, 0 failed.**
- **Regressions:** run_baseline `--only 'agrunt|hitmodel|disarm'`: 7 changed, all examined. 31, 36, 65 now pass; 32,
  70 more checks. 78 failed on the old base (its fix, 0d52eef93, merged since: 9/0). 95 (knight death, the armour's
  random break: 7/0 on a rerun), 66 (fails on origin/next too), 67/76/47/63/71 flaky as listed. 11 (blast) timed out
  twice in 8 runs; the stuck run's log ends with the mock's Body Calibration opening its menu ("close the menu to go
  on"), the game paused; 6 runs passed. Tests 99 41/0, 24 2/0. eval.sh did not run (its lock: `agentlocks/review`
  missing, a kit problem).

Checklist:
- [stalepose] Combat > Hit Detection > Armed Bodies as Drawn on: shoot an armed grunt's raised forearm (guarding, aiming,
  swinging): the hit lands on the arm (blood there, an arm hit), not its chest or head behind it; aim where its arm would
  hang in a plain animation: a miss.
- [stalepose] Show Disarm Volumes: the grip spheres sit on its hand as drawn while it swings; punching its drawn hand
  disarms it, punching where the old sphere was does not.
## Faster map loads, round 6: the smooth rigs at once (branch `agent/loadtime2`)

The leftovers of round 5 ("Not done": the smooth rigs one model after another, 42 ms of vrtesthall's load).

- **Each model's rig data on the pool at once** (vr_animsmooth.cpp): rigData split in three: `rigStale` (main thread:
  the model's rig found by ragdoll::rigFor, whose caches aren't the pool's; the settings noted), `rigFit` (any thread:
  each bone's fit to each pose and the rebuild, reading the model's poses and rig, writing that model's data alone) and
  `rigRest` (main thread: the rest rows; the arm and leg skins' rows, vr_agarms/vr_aglegs, are found once a model in
  caches that aren't the pool's). rigData runs the three in a row as before; the prewarm (`animsmooth::prepare`) runs
  every precached model's rigStale, then all their rigFits in one parallelFor (site "anim smooth rigs"; each fit's own
  loops nest on the pool), then their rigRests in the models' order. Headers checked again before the pool (warmRigs'
  way). `developer 1` prints the three phases: vrtesthall 3.2 ms rigs found, 17.5 fits, 2.5 rest rows (no-LTO).
- **Same outputs:** `vr_anim_smooth_rig_hash` (new; Debug menu, World and Physics, "Smooth Animation Rig Hashes": each rigged model's unfit masks, rebuilt keys, rest rows
  and fit statistics hashed by name) on vrtesthall, vrstart, hip2m5, e1m1: all 62 lines the same before and after.
- **Times** (interleaved ABBAABBAAB, exclusive, ThinLTO, one process per map so each is its first load; median
  [min..max] of 5): the "smooth rigs" prewarm step vrtesthall 49.7 [49.4..53.8] -> 23.0 [17.9..26.0] ms, vrstart 51.1
  [46.3..52.5] -> 19.2 [17.4..28.0], hip2m5 33.0 [30.4..34.0] -> 13.1 [12.2..16.9], e1m1 15.8 [14.5..16.6] -> 8.6
  [8.1..9.3]. Whole loads vrtesthall 1130 [1073..1183] -> 1087 [1060..1130], vrstart 1456 [1435..1556] -> 1419
  [1382..1486], hip2m5 1145 [1103..1198] -> 1061 [1057..1173], e1m1 983 [948..991] -> 961 [932..995] (the loads' spread
  is wider than the step's gain).
- **Not done: the disk cache of limb models and smooth rigs** (brief item 2, conditional): after the step above a cache
  could save at most the fits and rest rows, ~20 ms of vrtesthall/vrstart and ~10 of hip2m5/e1m1 less its own reading
  and keying (the rigs must still be found: the key holds the derived rig), and the limbs' builds already run on the pool
  beside the load (round 5: 22 ms there, not on the main thread). Under the brief's ~20 ms bar: skipped.

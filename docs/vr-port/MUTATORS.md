# Run setup and mutators

A **run** is a game from its start to the next start: Single Player > New Game, the Map Library's Play, an Official
Campaign's row, the hub's teleporter to the start map. Each of those opens the **Run Setup** screen first (a VR page,
`vr_menu_runsetup.inc`, opened from the flat menus too): Difficulty (and Bloody Nightmare once found), the presets
(Style = the Gameplay preset, Gore), the mutators, and Start. `vr_runsetup_ask 0` skips the screen (the last choices
are used). A start map with skill halls (id's start, a pack's hall, every campaign, New Game, the hub) has no
Difficulty row: its halls choose; the presets and mutators are still asked.

## How a choice works

Everything is one registry, `registry[]` in `Quake/vr/vr_runsetup.cpp`; each line is a `runsetup::Mutator`:

| field | what |
| --- | --- |
| `id` | `vr_runpick_<id>` (the screen's choice, archived, selected next time) and `vr_run_<id>` (the run's, not archived) |
| `label`, `options`, `def`, `help`, `section` | the screen's row (a cycle), its default, its help line, its header |
| `oldSave` | a saved game without the run line loads with this (-1: left as it is; the presets) |
| `apply(value, phase)` | what it sets beyond its cvar, at a run's start (`Phase::Start`) and on a save's load (`Phase::Load`); `nullptr` when the QC just reads `vr_run_<id>` |
| `forced()`, `heldBy` | a rule: the value another choice holds this one at (-1: free). The screen shows it as a line "Starting Weapon: Axe (Melee Only)", and `start()` uses it |
| `shown()` | hidden while false (Bloody Nightmare until found) |

- **Start** (`runsetup::start()`, also `vr_run start`): every `vr_run_<id>` from its pick (forced ones held), then each
  `apply(Start)` in registry order (the presets before the rows they set over).
- **Changelevel:** the cvars stay. **Saved games** (normal, quick, auto: all go through `SaveData_Fill`): the line
  `// qvr_run skill=2 bn=0 style=3 ...` (`saveLine()`, written with the save's context). **Loading** reads it before the
  old game ends (`readSaveLines`, from `VR_ReadSaveInfo`) and sets the run when the load goes ahead (`onLoadGame`, from
  `VR_OnBeginLoadGame`): each value and its `apply(Load)`, so a save of another run brings that run's presets back. A
  save without the line: the mutators at `oldSave`, the presets left, a console line says so.
- **The hub** is no run: `playHub` ends one (the mutators back to their defaults; the presets stay).
- **Your settings:** the presets go through their own machinery (`vr_presets.cpp`): the first apply keeps My Settings,
  and "My Settings" is a Style and Gore choice; Armed Humanoids and Smarter AI ("As Style", Off, On) set their Gameplay
  rows after keeping My Settings (`Kind::keepMine`). Nothing else of yours is written.

## Adding a mutator

1. `Quake/vr/vr_cvars.inc`: `vr_runpick_<id>` (CVAR_ARCHIVE, its default) and `vr_run_<id>` (CVAR_NONE), beside the
   others.
2. `vr_runsetup.cpp`: one `registry[]` line (options array above it; an `apply` only if it sets settings; a rule as a
   `forced` function reading the other's `vr_runpick_`).
3. `QC/vr_run_<id>.qc` (listed in `progs.src` after `vr_run.qc`): its gameplay, reading `cvarh_vr_run_<id>`, called from
   the hooks in `QC/vr_run.qc` (one line each). Run `build/qvr_checks/qvr_checks.exe cvars_qc` after adding a
   `cvarh_` use.
4. Tests: a line in `vr_run_check` (or its own), a checklist line and scenario.

### The QC hooks (`QC/vr_run.qc`)

| hook | called from | for |
| --- | --- | --- |
| `VR_Run_NewParms()` | `SetNewParms`'s end | a new game's loadout (Starting Weapon) |
| `VR_Run_Damage(targ, attacker, damage)` | `T_Damage`'s start | damage scaling (Glass Cannon) |
| `VR_Run_RefusesPickup(wid)` | `weapon_touch` | weapons not taken (Melee Only) |
| `VR_Run_RefusesFire(wid)` | `W_AttackImpl`'s start | weapons that only click (Melee Only) |
| `VR_Run_MonsterSpawned()` | `walk/fly/swimmonster_start` (self: the monster, its class spawn run) | phase 2: enemy count, randomized enemies |
| `VR_Run_ItemSpawned()` | `StartItem` (self: the item) | Randomized Pickups: the map's items of its table marked |
| `VR_Run_MapSpawned(held)` | the engine (`VR_OnSpawnServerSpawned`): a new map's entities all spawned, precaches open; not a saved game's load. `held`: the IT_ bits he enters with (0: a new game) | Randomized Pickups: the marked items swapped |
| `VR_Run_Respawn()` | `respawn` (a dead single player), after the tutorial's checkpoints; TRUE: taken over | Ironman: `restart fresh` |
| `VR_Run_MonsterSpawning()` | the engine's `ED_LoadFromFile`, before a map's `monster_*` spawns, while Enemy Count or Randomized Enemies is on (self: its keys parsed, its spawn function not run) | Randomized Enemies' swap (its classname: the new kind spawns from the map's own keys), then Enemy Count's number of copies (0..2) |
| `VR_Run_MonsterCopy(k)` | `ED_LoadFromFile`, once the whole map has spawned (self: copy k, its monster's keys parsed again; other: its monster) | Enemy Count's placement beside its monster; FALSE: no room, no copy |

A mutator that keeps state of its own in QC (Ironman's lives, say) keeps it in a saved global or field; the run's
choices themselves are the engine's line.

## The ones there are

| id | options | does |
| --- | --- | --- |
| `skill` | Easy..Nightmare | Quake's skill (pick: `vr_maps_skill`; held at Nightmare by Bloody Nightmare) |
| `bn` | Off, On | Dawn of the Machine's Bloody Nightmare (`vr_mg3_bn_start` at the start; serverflags carry it) |
| `style` | My Settings, Classic, Light, Unleashed (default) | the Gameplay preset |
| `gore` | My Settings (default), Full, Minimal, Off | the Gore preset |
| `armed` | As Style, Off, On | `vr_agrunt_repl`, `vr_agknight`, `vr_agdknight` |
| `smartai` | As Style, Off, On | `vr_ai_enhanced` |
| `startweapon` | Shotgun, Heavy Revolver, Axe, Unarmed | a new game's loadout (`vr_run_startweapon.qc`); held at Axe by Melee Only |
| `meleeonly` | Off, On | guns left on the floor, a gun in the hand only clicks (`vr_run_meleeonly.qc`) |
| `glasscannon` | Off, On | the player and monsters take double damage (`vr_run_glasscannon.qc`) |
| `pickups` | Off, Balanced, Everything Goes | the map's pickups swapped as it loads (`vr_run_pickups.qc`, below) |
| `ironman` | Off, On | no saving; a death restarts the level with what you entered it with (`vr_run_ironman.qc`, below) |
| `enemycount` | 1x, 2x, 3x | one or two copies of each map monster beside it as the map loads (`vr_run_enemycount.qc`; below) |
| `enemyrandom` | Off, Balanced, Everything Goes | map monsters swapped as the map loads (`vr_run_enemyrandom.qc`; below) |

### Randomized Pickups

- **What is swapped:** only the kinds of `vr_run_pickups.qc`'s table (ammo, health, armour, weapons, powerups; Rogue's and
  Hipnotic's when their data is there: a kind whose spawn function refuses is not tried again on that map, the item
  stays). Never an item the map names or counts: a targetname, target or killtarget, HONEY's secret items, items a
  trigger brings (trigger-spawned, Dawn of the Machine's SPAWNED, AD's STARTOFF). Keys, runes, sigils, the MG3 upgrades
  are not in the table. Not on Quake VR's own maps (vr*), not in deathmatch.
- **How:** StartItem marks; once the map has spawned (the engine calls `VR_Run_MapSpawned`), each marked item is rolled
  and the kind that comes up is spawned in its place by its own spawn function (a map's item in every way), the old one
  removed. A saved game's load keeps the save's items.
- **Balanced:** ammo for ammo of a weapon kind he enters with (his level-start parm1; the shells always) or one placed
  within 1536 units, the box's size kept; health 15 <-> 25 (the megahealth alone); armour a step away (green <-> yellow
  <-> red); a weapon of its tier (shotgun, revolver / super shotgun, nailgun, AD's Widowmaker / super nailgun, grenade
  and proximity launchers / rocket launcher, thunderbolt, laser cannon, Mjolnir, AD's Plasma Gun); a powerup of its tier
  (quad, pentagram, ring, empathy shields, power shield / suits, belt). AD's upgrades only with AD mounted and its rules
  on. **Everything Goes:** a kind first, then any of it. **Melee Only:** nothing becomes a gun that was not one.
- **The rolls:** a hash of the item's place and the run's seed (`vr_run_seed`, set at the run's start; sv_random_seed's
  when a test fixes it): a map gets the same pickups all through a run (an Ironman restart too). Not in the save line:
  a run loaded in a new session rolls its next maps anew. `vr_run_seed 0`: the server's random numbers.

### Ironman

- Every save is refused (host_cmd.c's save: save, quicksave and the autosaves all go through it), said on the console and
  the screen; no autosave is tried (host.c); the Single Player menu's Save is greyed and refused, the VR toolbar's
  Quick Save greyed ("Ironman: no saving"); no death menu (no saves to load: a press respawns). Loading is still allowed
  (a save of another run brings that run back).
- A death: `restart fresh` (never Ironwail's autoload of the last save), the level from its start.
- **"The start" (decided):** the start of the level he is on, as he entered it (by a changelevel, a new game), not the
  campaign's or the episode's first map; a hub (id's start, a pack's hall) is a level like any other.
- **The loadout (decided):** the engine's level-start parms (parm1..16 in its client record, parm17..72 beside them:
  `vr_progs.cpp`), set as he enters a level and written in every save: the restart gives them back, as Quake's own
  single-player restart did. Not a QC saved global: the restart loads the progs again, so a global would not outlive it.

### Enemy Count and Randomized Enemies

- **Order:** the swap first, then the copies, of the swapped kind (a copy is its monster's map keys parsed again).
- **Kinds** (`VR_RUN_ER_KINDS`): Quake's, AD's (only while AD's data is mounted, and outside AD's maps with
  `vr_ad_everywhere`), the packs' common ones (copied, never swapped); each with how it moves, its width (Quake's two
  hulls) and its threat tier. Bosses and scripted monsters are not listed: never swapped or copied; nor a crucified
  zombie or AD's ceiling spider; in AD's maps a monster with AD's class bits (2, 4: a lying zombie...) is not swapped.
- **Balanced:** the same tier and movement; **Everything Goes:** any tier, a walker's spot may hold a flyer. Never a
  larger kind than the spot's (doors and lifts spawn after it: a hull test then would miss them). The map's health and
  skin keys and the class bits are dropped; a zombie's ambush (2) becomes everyone's (1). Armed Humanoids: a grunt,
  enforcer, knight or death knight swapped in is armed as the run says (its spawn function decides).
- **Copies:** placed after the whole map spawned: a ring around the monster, reached in a straight line, clear of the
  world, entities and other monsters (their kinds' boxes, solid or not yet), in no trigger volume the monster is not in
  (closet teleporters), on a floor within 24 units of its monster's (walkers), in the same water (swimmers). They keep
  the targetname (wake, appear, are killtargeted together), never fire anything (no target, killtarget, AD deathtarget):
  the map's death-counting triggers count the map's own, as made. They count in `total_monsters`.
- **Saved games and changelevel:** the entities are saved as they are; a new map gets its own. The roll: the map's name,
  the spawn point and its content seed (`vr_map_content_seed`): deterministic per seed.

## Console and tests

- `vr_runpickups_test 1` ("runpickups: N passed", `vr_run_test.qc`): Balanced on e1m1 with seed 777 (rules, the map's
  named items kept), the same layout on a reload, a save and its load (the run's choice and the items back), e1m2 by
  changelevel, Everything Goes with Melee Only and Glass Cannon (kinds crossed, no new guns, the map's links and monster
  count unchanged, `VR_Run_TestCheck`), a new run without it. `vr_runpickups_test 2`: the map loaded now (any game's:
  AD's with `-game ad`), then again with it off: the rules, its unresolved targets, trigger_counter links and monsters.
- `vr_ironman_test 1` ("ironman: N passed"): save and the menu's Save refused (`vr_ironman_refused` counts them), e1m1's
  rocket launcher carried into e1m2, a death there restarts e1m2 with it (and not the grenade launcher given in e1m2),
  no death menu, the run's other choices kept; a new run without it saves.

- `vr_run` prints the run (and the screen's picks); `vr_run <id> <value>` sets one (applied); `vr_run start`;
  `vr_runsetup [hub|newgame]` opens the screen.
- `vr_runsetup_test` (36): the registry, a run's start, Melee Only's rule, the save line round trip (a run's choices and
  the presets' values), an old save, the hub. Presets' My Settings in the test files; settings set back.
- `vr_runsetup_screen_test` (13; `maps_skill_test` is its old name): each way in opens the screen, skill halls hide
  Difficulty, a held row is a line, Back, Start.
- `vr_run_check <startweapon> <glasscannon> <meleeonly> <gore>` / `vr_run_check verdict` ("runmutators: N passed"):
  the end-to-end check in a running game (QC `VR_Run_TestCheck`); the round notes' script starts run A, saves, starts
  run B, loads A, changes level.
- `vr_run_enemies_check <enemycount> <enemyrandom>` / `vr_run_enemies_check verdict` ("runenemies: N passed"): the
  running map as those choices spawned it (QC `VR_Run_EnemiesCheck`: copies beside their kind, not stuck, firing nothing,
  most monsters copied; swaps of allowed kinds; nothing dead at the start; `total_monsters` in id's maps) and its trigger
  chains (`VR_Run_EnemiesChains`: targets and killtargets resolved, trigger_counter uses) equal to the same map's first
  check with both off. ROUND25.md "mutenemies" has the scripts.

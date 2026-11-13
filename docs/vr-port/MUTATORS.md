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
| `VR_Run_ItemSpawned()` | `StartItem` (self: the item) | phase 2: randomized pickups (respect `VR_RunMeleeOnly_Refuses`) |

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

## Console and tests

- `vr_run` prints the run (and the screen's picks); `vr_run <id> <value>` sets one (applied); `vr_run start`;
  `vr_runsetup [hub|newgame]` opens the screen.
- `vr_runsetup_test` (36): the registry, a run's start, Melee Only's rule, the save line round trip (a run's choices and
  the presets' values), an old save, the hub. Presets' My Settings in the test files; settings set back.
- `vr_runsetup_screen_test` (13; `maps_skill_test` is its old name): each way in opens the screen, skill halls hide
  Difficulty, a held row is a line, Back, Start.
- `vr_run_check <startweapon> <glasscannon> <meleeonly> <gore>` / `vr_run_check verdict` ("runmutators: N passed"):
  the end-to-end check in a running game (QC `VR_Run_TestCheck`); the round notes' script starts run A, saves, starts
  run B, loads A, changes level.

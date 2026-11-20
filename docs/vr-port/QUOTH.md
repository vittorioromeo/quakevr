# Quoth: its maps under Arcane Dimensions' rules

Approved 2026-11-20 (the mod survey's rank 1, `qvr-kit/scratch/mod_survey_1119.md` sections 3-5). Quoth 2.2 (Kell,
Necros, Preach) has no public source, and its licence lets its zip be passed on only unmodified: Quake VR never runs,
copies or bundles any of it. The player installs Quaddicted's zip (`quoth2pt2full_2.zip`); Quake VR plays Quoth's maps by
Arcane Dimensions' rules, whose QuakeC re-implements nearly every Quoth classname ("Allows for Quoth maps to be loaded
into AD"), and fills the rest with a clean-room shim written from the maps' entity data and their behaviour.

## 1. How it plays

| Piece | Where | What |
|---|---|---|
| Quoth's data | `vr_quoth.cpp`, campaign row 8 in `vr_gamedir.cpp` | `<base>/quoth` (the zip's files, unmodified), checked by `pak2.pak`'s SHA-256 (2.2's; status `vr_quoth_status`: 0 missing, 1 2.2, 2 incomplete, 3 another version). Its folder supplies only its own files: its progs, `quake.rc`, configs and demos never; the 87 files an official pak or AD also has never (`vr_quoth_data.inc`, `quoth_data.py`: Quoth's knight, ogre, grunt, enforcer... meshes do not replace id's, which our VR rigs are built on; AD's models stay AD's) |
| Its own maps | campaign 8 `quoth`, experimental (`vr_campaign_native quoth`, Select Campaign after the notice) | `start` (its hub), `e1m1quoth`, `e1m2quoth`, `kellbase1`, `breakable`, the test maps |
| Its content elsewhere | `skipOutside` | while installed, in every campaign: its models, sprites, sounds and skies (after the rule above): what a Map Library package made for Quoth names |
| The ruleset | `vr_rules.cpp` | Quoth's campaign plays by AD's (`campaignRulesets`); so does a Map Library package made for Quoth (AD's row `madeFor` "Quoth"); a Quoth map from elsewhere is detected as AD's (every sample map is: AD's classnames for Quoth's monsters, and the shim's `ad_` names, are AD signals) |
| The map browser | `vr_mapinstall.cpp` `madeFor`, `vr_menu_maps.inc` | the `quoth` tag already named the mod; the detail now says "plays by Arcane Dimensions' rules (experimental)", or that Quoth 2.2 and AD are needed |
| The shim | `QC/vr_quoth.qc` | below |

`campaignForMap` treats only exact `e1m1`..`e4m9` names as Quake's episodes (it took `e1m1quoth`, `e1m2quoth` for
Quake's and left Quoth's campaign). The third-party campaigns' "another version" text is the row's `release` field
(one line, no per-campaign branch); `qvr::sha256::fileMatches` checks a pinned file for Dwell and Quoth alike.

## 2. The shim (`QC/vr_quoth.qc`): the classnames and keys AD lacks

Semantics are **inferred** from the 27 maps' entity data (Quoth's 12, the 15 sample maps) and checked by behaviour;
no Quoth documentation or code was read.

| Quoth | Instances | Played as | Inferred from |
|---|---|---|---|
| `monster_deathguard` | 13 | AD's `monster_death_guard` (Quoth's death guard, which AD has) | the other spelling in the same maps' spawners |
| `trigger_once_point`, `trigger_secret_point`, `trigger_monsterjump_point`, `trigger_drolejump_point` | 3, 9, 10, 1 | AD's trigger, its volume from `size` (else `mangle`; else the point itself), centred on its origin (spawned with AD's empty model: without one AD's trigger was an inert marker until phase 2's play-throughs found it) | point entities with a trigger's keys; Copper's "quoth2-style bounding box triggers" read the same keys the same way |
| `info_multispawn`, `func_hordespawn` | 32, 18 | Quake VR's (AD's remove themselves: the AD row's `keptOurs`): when used, `count` (3) monsters of the `spawnfunction` classname, `wait` (0.3 s) apart, each with the spawner's target, angles and flags | AD's own notes (count, wait, spawnfunction; "the same"); their targets are trigger_counters (metmon's stages, ne_marb's waves) |
| `spawnfunction` | 44 | the engine names the classname too (`spawnclassname`, unless set: Hipnotic's func_spawn asks for both); an AD-only monster resolves to AD's function (`ad_monster_drole`) | |
| `targetname2`, `targetname3` | 16, 1 | a second (third) name: an alias entity of that name hands its use to the entity (once a map, `VR_Quoth_Frame`); an entity with no `targetname` takes it as its name as it is parsed (`ED_PromoteSecondName`, before AD's spawn function, which removes a nameless counter) | doors, counters, relays and monsters with a second name their maps fire; kellbase1's `count2`, `count3` (named only so) fed by its three buttons' target2, target3 |
| `target3`, `target4`, `killtarget2` | 27, 13, 3 | fired after `target`, `target2` (AD fires those two): the entity's target2 becomes a relay of its own that fires its old target2 and target3, then target4, and kills killtarget2 (its delay and activator kept) | kellbase1's buttons (count1, count2, count3, their indicator light) |
| `info_rubble` | 28 | AD's breakable spawner; glass or metal from `modelpath`, else rock | `modelpath` progs/glass.mdl, progs/metal.mdl (Quoth's rubble models) |
| `mapobject_grill64`, `mapobject_grill128` | 1, 12 | AD's `mapobject_custom` with Quoth's `progs/grill64.spr`, `grill128.spr` | the maps' truncated `model` key |
| `ambient_driveshaft`, `ambient_thrum` | 2, 1 | AD's looped custom ambient sound with Quoth's `necros/drivshft.wav`, `ambience/thrum.wav` (always on) | Quoth's sound names |
| `misc_biobox_s/l`, `misc_plasmabox_s/l` | 7+3, 5+2 | AD's exploding boxes (small, large) | the names (Quoth's `b_bio_*.bsp`, `b_plas_*.bsp` boxes) |
| `info_command_spawn`, `trigger_command_contact` | 3, 1 | nothing, as AD's `info_command` (it runs no map console command; they set `r_wateralpha`) | |
| `item_flashlight` | 2 | nothing: Quake VR's flashlight is always at hand | |

Keys left as they are (inferred harmless): `use` (86: function names our progs has, `SUB_regen`, `trigger_once`...: the
engine resolves them; none missing), `should_roam` -1 (92, grunts, enforcers, ogres: "don't roam", i.e. stock),
`is_waiting` (0 on 7 untargeted triggers: no effect; 1 on quarantine's 12 trap teleporters, which are targeted and
already wait for their trap), `preset` (func_breakable's material, 1-8: AD's default style plays; only `info_rubble`'s
is mapped), `ritem`/`rsize` (likely the item a breakable drops and its size: not played), `respawncount` (11 items).

## 3. Acceptance

`Misc/quakevr/quoth_setup.py <base>` installs Quoth and the 14 sample packages into a test base from the kit's
downloads (no download); `Misc/quakevr/quoth_maps.sh [frames]` plays every map (skill 1), saves, prints the ruleset,
runs `vr_ad_test 211` (targetname2 and kellbase1's spawner), takes start's exit, loads three saves back;
`quoth_maps.py` gives the per-map table and `quoth_maps: N passed, M failed`.

Not played by the test: `metmon1d` (BSP2; its map is inside the zip's `pak0.pak`, which the Map Library's install
unpacks, `vr_mapinstall.cpp`, but `quoth_setup.py` does not yet: it keeps the pak, which a package folder does not
mount). Its 26 `info_multispawn` drive its stage counters: the first map to check by hand.

Results (2026-11-20, 600 frames a map, `scratch/quoth_maps/census.txt`):

| | before (the shim, the campaign fix) | after |
|---|---|---|
| package maps (15) | 10 clean; lpqsp1 22, ne_marb 15, dis_sp6 4, sksp2 2 missing classnames | 15 clean, AD's rules on each (detected) |
| Quoth's own (12) | start only (`e1m1quoth` switched to Quake's campaign; the rest then "Couldn't spawn server") | 12 clean, AD's rules (the campaign's) |
| start's exit, 3 saves loaded back | | all AD's rules, no error |
| `vr_ad_test 211` (kellbase1) | | targetname2 used once; the exit lock's spawner named `weapon_plasmagun` and made it; 2 grunts from a made spawner carrying its target |

`quoth_maps: 30 passed, 1 failed`: the one, the verdict line of `vr_ad_test 211` not seen (its echo queued after the
script's quit; its four checks all PASS); fixed (a broadcast verdict), checked on e1m1: `quothkeys: 2 passed, 0 failed`.
0 of 391 official and AD maps have a shim classname or `targetname2`: their censuses do not change. Phase 2's keys
(`targetname3`, `target3`, `target4`, `killtarget2`): 0 of the 360 maps in id1, AD, Hipnotic and Rogue's folders.

Phase 2 (2026-11-20): `quoth_setup.py` unpacks a package's root `pakN.pak` as the Map Library does (game code and
configs left out), so metmon1d's map `metmon` is in the census: `quoth_maps: 31 passed, 0 failed` (15 package maps, 12
of Quoth's, kellbase1's 211, start's exit, 3 loads).

## 4. The installer (phase 2)

`Installer/.../Packaging/Quoth.cs` on the shared `ModComponent` path (Installer/README.md, "Quoth"): Quaddicted's
`quoth2pt2full_2.zip` (54,792,367 bytes, SHA-256 `9fce1af3...1139`; content-addressed URL, then the filebase; also in
`support_assets.json`'s `thirdParty`) unpacked unmodified into `<install>\quoth` (6 files, 136 MB). Ticked while
Arcane Dimensions is chosen (ticked, in the install, or a usable copy found), off and greyed with the reason otherwise;
its step runs after AD's and only where AD now is. A 2.2 copy in an owned root (pak2.pak's hash, the engine's pin) is
used in place. CLI `--quoth`/`--quoth-zip`/`--no-quoth`. Tested: self-tests (pin, existing copies, the real zip: install,
repair restores a damaged file, update keeps, uninstall removes), the CLI into sandboxes (without AD: skipped with the
reason; with AD found: installed; update: "unchanged, not downloaded"; uninstall: 17 files, folder removed), and one
download of the pinned URLs from Quaddicted (checked).

## 5. Play-throughs (phase 2): `quoth_play.sh [flat]`

`vr_ad_test 212` (Debug > Tests > Arcane Dimensions, "Quoth: Drive This Map's Flow") plays a map's flow a round a
second: every trigger a player walks into touched, every monster awake killed by the player (god), so deaths fire
their targets (counters, waves, spawners); a sentinel sharing the map's last-stage targetname says when it fires (90
rounds: "timed out"), and on a time-out prints each counter's uses left and what targets the goal. Each round also
presses every button a player can, once: a hand's press (`handtouch`; a button's own touch is a foot's, for the ones
pressed by stepping on them) or a shot (one with health); one locked until a stage unlocks it is tried again next
round. It carries no keys. `quoth_play.sh` chains: start's exit to e1m1quoth, e1m1quoth's flow and exit (to
e1m2quoth), kellbase1's flow and exit (to start), ne_marb, lpqsp1, metmon (the verdict runs the next step: no frame
waits).

Phase 2's first run (VR mock, no buttons): 7 passed, 4 failed (e1m1quoth, metmon, lpqsp1 at button stages, kellbase1
at `count3`). Phase 2's second (2026-11-20, quoth3), VR mock and flat alike: `quoth_play: 11 passed, 0 failed`.

| Map | Result | What it took |
|---|---|---|
| start -> e1m1quoth, e1m1quoth -> e1m2quoth, kellbase1 -> start | exits taken | |
| e1m1quoth | `ramproom_door` in round 1 (10 buttons pressed, 39 killed) | the button press (its ramp room counts 3) |
| kellbase1 | `exitlock` in round 4 (7 buttons, 34 killed) | its 3 buttons fire `count1`, target2 `count2`, target3 `count3` (named only by targetname2, targetname3: AD removed both nameless counters; target3 was never fired) whose waves' 16 deaths open the exit |
| ne_marb | its four waves to `keyblockmarb` in round 21 (47 from the spawners, 94 killed) | |
| lpqsp1 | `voreo` in round 6 (8 buttons, 110 killed) | a shot button's target2, the point trigger `voreotr`, was an inert AD marker (every point trigger was: no model): now AD's trigger |
| metmon | `phase4_relay` in round 8 (23 buttons, 33 from the spawners, 80 killed) | the button press (its stage locks count presses) |

Not played: e1m2quoth's exit (to `e1m3quoth`, which Quoth 2.2 does not ship); the sample maps' own exits; keys.

## 6. Left

- `func_breakable`'s `preset` (material) and `ritem`/`rsize` (likely a dropped item): an AD import patch would map them.
- The driver plays flows, not routes: what it reaches a player reaches only if the map's geometry agrees (VR play).

# Arcane Dimensions (AD): analysis and plan for native Quake VR support

Status: Phase 0 (analysis + plan) done 2026-10-11; Phase 1a built (section 14): AD's QuakeC imported, namespaced and
compiled in. **Phase 1b built** (section 15): AD's campaign (developer launch) plays its 38 maps under AD's rules, 0
missing spawn functions, no errors headless; Vittorio's decisions in section 12. Branch `agent/arcane`. **The rules'
follow-ups built** (section 16, branch `agent/adrules`): AD's data in every campaign, its new artifacts, custom
pickups, ladders (climbed by hand) and debuffs (burning merged into ours) in any map, ammo resistance on our weapons,
Evil as a skill choice, the HUD's AD row, spawnflags by ruleset, crouching in AD. AD's assets are never committed. Target, from Vittorio's request:

- When AD is installed, all of its enemies, entities and weapons are available in every campaign and custom map,
  inside the merged QuakeC (the "giant QuakeC aggregation", MG3.md decision 5, EXPANSIONS.md).
- AD-specific rules that would change other campaigns are gated in QC, as for the other expansions.
- Conflicting entities get different names and are resolved when the map loads.
- Maps made for AD are detected when they load and switch on AD's flags and ruleset.
- AD weapons are made usable first and polished later.

Sources (read-only, never committed): `ad_v1_80p1final.zip`. Quaddicted calls it "Arcane Dimensions 1.81"; it is
1.80 with patch 1, released 2020-10-13 by Simon "sock" O'Callaghan and team. Its SHA-256 is
`7ad993da6c760c446ca31fad71e9f5b6c9eee99b6354f161aa746b572fe70a7d`, which matches Quaddicted's published hash and was
verified after download. It is 320,914,417 bytes. The QuakeC source ships inside the release
(`pak0.pak`/`pak2.pak`: `my_progs/`, 147 files; pak2 holds the patch-1 copy). There is no separate source archive for
1.80. Analysis scripts and raw tables are in the worktree's `scratch/an/` (not committed): `bspents.py`, `idents.py`,
`mons.py`, `keys.py`, `permap.py`, `detect.py`, `detect_fp.py`, `overlap.py`, `qd_progs.py`. The probe runs are in
`scratch/ad_probe.sh` and `scratch/ad_count.py`.

---

## 1. Licence findings (from `ad_v1_80_readme.txt` and `ad_v1_80_credits.txt`, quoted exactly where it matters)

| Item | Terms | Consequence for Quake VR |
|---|---|---|
| QuakeC (`my_progs/*.qc`) | "released under the terms of GNU General Public License v2 or later. You may use the source files as a base to build your own MODs as long as you release them under the same license and make the source available. Please also give proper credit." | **We may merge it.** Our QC is GPL-2 (id 1.06, Hipnotic and Rogue; MG under GPL-2+), and our source is public. Keep AD's file headers, credit sock and the team in CREDITS.md, and list the adaptations as OFFICIAL_QC_SOURCE.md does. AD's own code borrows from Hipnotic (`hiprot.qc`, `hipspawn.qc`), Rogue (traps) and others (Preach, necros, Lunaran, ...); all of it is GPL already. `dpextensions.qc`/`fteextensions.qc` are extension headers we would not import. |
| Assets (models, sounds, textures, maps, gfx) | "Please do not use any of these assets in ANY COMMERCIAL PROJECT. and remember to give credit if you use any of these assets." / "All of these resources may be electronically distributed only at NO CHARGE to the recipient in its current state and MUST include this readme.txt file." | Quake VR is free, so reading AD's assets from the player's AD install is fine. **Never ship modified AD assets** (a VR re-rig of an AD weapon model would be a modified asset: ship our own model, or read theirs in place and adapt it at runtime with metadata, as for MG3). Mirroring the *unmodified* zip with its readme at no charge appears allowed. Conservative choice: download from the official mirrors and ask sock before we host a copy (decision 4, section 12: we host the unmodified zip; Vittorio asks sock). Many AD textures and models are "based on existing Quake assets" (readme), which is another reason never to redistribute them. |
| Name | "Do not use the AD (Arcane Dimension) name for your own mod" | Call it "Arcane Dimensions support" or "plays Arcane Dimensions maps". Never brand our build or any QC fork as AD. |
| Quake trademark | standard id notice | none |

Distribution points (official): simonoc.com (`/files/ad/ad_v1_80p1final.zip`, the author's site), Quaddicted
(`/filebase/ad_v1_80p1final.zip` and the content-addressed
`/files/by-sha256/7a/7ad993da.../ad_v1_80p1final.zip`), ModDB (an HTML landing page, not a direct link) and
born-robotic.net (a European mirror listed on simonoc.com). The 1.70 map sources are a separate zip
(`ad_v1_70mapsource.zip`). We don't need them.

---

## 2. What AD is: size and structure

### 2.1 Data

| | Size | Content |
|---|---|---|
| `pak0.pak` | 258 MB | `progs/` 540 files (473 MDL v6, 67 SPR v1), `sound/` 825 wav, `gfx/` 302 (234 lmp + tga), `textures/` 83, `music/` 3, `particles/` 3 + `effectinfo.txt` (FTE/QSS/DP particles), `my_progs/` (QC source), `progs.dat`, `csprogs.dat` (QSS/FTE/DP CSQC HUD) |
| `pak1.pak` | 512 MB | `maps/`: 38 playable BSPs (`start`, 23 `ad_*` maps, 2 DM, 12 test maps), plus 245 brush-model BSPs for breakables and bmodels (`maps/ad_brk`, `base_brk`, `medieval_brk`, `rtex_brk`, `speedbz_brk`, `bmodels`) |
| `pak2.pak` | 21 MB | patch 1: `progs.dat`, `csprogs.dat`, `my_progs/` again (14 files differ), 68 map files (`ad_akalakha` again, more bmodels) |
| loose | | `quake.rc` (sets `max_edicts 8192` and binds impulses), FGD/DEF for editors, readme, credits, changelogs, documentation (147 KB) |

The zip is 306 MB and extracts to about 810 MB. Model limits are fine for Ironwail: the largest MDL has 1,376 vertices
and 2,048 triangles. Every model is MDL v6 and every sprite is SPR v1: there are no IQM, MD3 or SPR32 files.

**AD files that collide with official or Quake VR files** (`overlap.py`). AD replaces 72 official files:

- 23 `gfx/*.lmp` (menu art);
- 23 `progs/*.mdl` (v_/g_ weapons, h_ heads, `player.mdl`, `flame*.mdl`);
- 23 sounds (`items/*`, `weapons/ric*`, `sgun1.wav`, ...);
- `maps/start.bsp` and `progs.dat`.

25 of these names also exist in `quakevr/`, so our copies win once AD is mounted below quakevr. One name is a true
semantic clash: `progs/v_plasma.mdl` is Rogue's plasma lightning gun for us and AD's Plasma Gun for AD.

### 2.2 QuakeC

AD's QC is 116,719 lines in 144 `.qc` files. That includes the extension headers `dpextensions.qc` (2.5k) and
`fteextensions.qc` (3.3k), and the CSQC (`csqc_*`, about 3k). Ours is 122.6k lines in 241 files.

| progs.dat (header) | AD 1.80p1 | Quake VR today | Limit |
|---|---:|---:|---|
| statements | 176,943 | 224,782 | int32 (no practical limit) |
| globals | 22,799 | 26,746 | **65,535** (v6 16-bit operands) |
| entity fields (slots) | 1,129 | 1,934 | memory (edict size) |
| functions | 10,135 | 7,526 | |
| strings | 296 KB | 345 KB | |
| file | 2.35 MB | 2.63 MB | |

Identifier overlap with our QC (regex parse, comments stripped, extension headers excluded):

- **1,690 function names collide.** Most are frame functions of the id monsters, because AD rewrites every id monster:
  202 in the zombie, 172 in the hell knight, 143 in the ogre, and so on. Plus `T_Damage`, `CheckAttack`,
  `ClientConnect`, ...
- **173 field names collide.** Two of them differ in type: `.color` (vector in AD, float in ours) and `.th_pain` (AD's
  takes `(inflictor, attacker, damage)`, ours `(attacker, damage)`).
- AD adds 679 fields of its own: **813 slots**, 67 of them vectors and 90 strings.

**So AD cannot be pasted in.** It has to be namespaced: section 5.2.

AD's systems by file:

| System | Files | Notes |
|---|---|---|
| Settings / config flags | `settings.qc` (1.6k), `world.qc` | Persistent through `temp1`/`scratch1..4`/`saved1..4` cvars and `quake.rc` (layer 1). The worldspawn keys of section 6 are layer 2; impulses 100-255 are layer 3. |
| Entity state ("estate") | `estate.qc`, `triggers_states.qc` | on/off/disable/reset/aframe for most entities. 317 `trigger_entitystate_off` placements alone. |
| Breakables | `breakable.qc` (1.8k) | `func_breakable`, `_wall`, `_spawner`, `misc_breakable_pot*`. Debris are BSP bmodels from `maps/*_brk/`. Four material styles. |
| Particles | `part_manage/generate/emitter.qc` (3k) | A sprite-entity particle system (`particlemax` caps it) with an FTE/DP point-particle path (`effectinfo.txt`) when the engine has it. |
| AI framework | `ai_*.qc` (14 files, about 12k) | AD's own `T_Damage` (ammo resistance), check-attacks for every monster (2.7k), projectile library (2k), minions, tether, passive state, path corners with routes, Z-aware aim, gibs by material, infighting. |
| Monsters | `mon_*.qc` (65 files) | Table in section 3.1. Chaos mode (`chaosmode.qc`, 2.4k) wraps most vanilla and AD monster spawns for random substitution. |
| Player | `client*.qc` | Debuffs (poison, burning, bleeding), jump boots, ladders, grapple hook (`client_ghook.qc`, 1.3k), map variables (packed into `parm11..16`), cameras/cinematics, evil mode, intermission cycling, info/inventory prints. |
| Weapons/items | `weapons.qc`, `items.qc` (4.3k), `items_progspn.qc` | Shadow Axe, Widowmaker, Plasma Gun, projectile shotguns with casings, item respawn system, custom keys, backpacks, new artifacts. A second item word `.moditems`, carried in `parm10`. |
| World logic | `func*.qc`, `triggers.qc` (3.5k), `targets.qc`, `misc*.qc`, `traps*.qc`, `lights.qc`, `sounds.qc`, `hiprot.qc` | Extended id entities plus 120 new ones. |
| Fog/sky | `globalfog.qc` | Fog and skyfog sent as **stuffcmd** text (`fog d r g b`, `r_skyfog`) and `sky` for `trigger_skybox`. Our engine filters stuffed commands (EXPANSIONS.md: MG fog uses `svc_fog`). |
| CSQC | `csqc_*.qc` | The HUD only. Never used in VR (Map Library already refuses `csprogs.dat`). |

---

## 3. Inventory

Placement counts are summed over AD's 38 playable maps (`pak1`). The full table is `scratch/an/ad_spawnables.txt`:
335 spawnables, made of the 309 FGD classes and the classnames the maps use.

### 3.1 AD-only classnames (226; no Quake VR spawn function today, so no name conflict)

| Prefix (count) | Classnames with placements |
|---|---|
| `monster_` (66) | dknight 475, zombiek 435, dcrossbow 387, hogre 330, spider 268, dguard 254, skullwiz 225, lostsoul 177, hogremac 150, boil 117, gaunt 95, hogreham 86, defender 83, gargoyle 80, minotaur 72, dfury 69, eliminator 57, jim 52, army_rocket 51, swampling 48, centurion 46, voreling 46, golem 41, army_plasma 39, wraith 34, dsergeant 30, drole 28, army_grenade 25, boglord 25, scorpion 23, seeker 21, justice 17, dknightghost 13, pyro 13, minotaurminion 12, skullwizminion 12, freddie 11, fumigator 11, wraithminion 8, ogre_fishing 7, nour 6, shalrathminion 5, eidolon_wavetrig 4, hell_pointknight 4, icegolem_wavetrig 4, ogre_fishing_ghost 3, wraithminion2 3, icegolem 2, chthon 1, eidolon 1, firetopboss 1, judgeminion 1, judicator 1, nourminion 1, steelclaw 1; unplaced: baron, deflector, dprince, floyd, mammoth, megaforce\*, psyder\*, shub, shubupsd, sweeper, turretbal (\* FGD only, no function) |
| `weapon_` (5) | upgrade_ssg 53, upgrade_axe 36, upgrade_lg 27, plasmagun 1, axe 0 |
| `item_` (14) | backpack_armour 67, backpack 60, keyx 31, custom 29, progspawn 19, artifact_sharp_shooter 18, artifact_tomeofpower 18, weapon 8, artifact_blastbelt 4, artifact_jumpboots 4, artifact_nail_piercer 3, artifact_airtank 1, artifact_lavashield 1, healthvial 0 |
| `trigger_` (37) | entitystate_off 317, explode 197, entitystate_on 150, monsterbreak 136, ladder 68, monsterturret 54, entitystate_disable 40, mapvar_multiple 34, monsterkill 28, entitystate_aframe 27, void 20, itemrespawnupd 17, mapvar_query 11, entitystate_reset 7, monsterattack 6, drolejump 5, skyfog 5, gravity 4, rune 4, skybox 4, clientmsg 3, monsternojump 3, touchsound 3, doorstate 2, engine 2, monsterdrop 2, giveitems 1, nomonjump 1, timer 1; unplaced: cdtrack, mapvar_toggle, mapvar_update, maxhp, monstermovespeed, monsterpassivestate, takeartifact, velocity |
| `func_` (14) | breakable_wall 379, breakable_spawner 187, laser 89, skill 16, shadow 4, explobox 3, insidevolume 0, pressureswitch 0; compiler-only: detail, detail_fence, detail_illusionary, detail_wall, group, illusionary_visblocker |
| `misc_` (33) | gtkmodel 2173, drip 342, particle 315, shake 106, smoke 105, player 80, textbook 76, spark 57, breakable_pot1a..2d/1aflr/2aflr 225, targetnumber 36, demon 27, bob 25, particle_burst 23, textstory 20, targetdummy 8, builtineffects 7, marshlight 6, dknight 5, particletemplate 5, camera 2, textstoryctrl 1, weather 1, external_map 0, gibfountain 0 |
| `trap_` (10) | lightningshooter 120, gasshooter 34, sawbladey 8, pendshortx 6, sawbladex 6, pendlongx 4, pendshort 4, rocketshooter 1, grenadeshooter 0, pendlong 0 |
| `ambient_` (9) | custom_sound 1155, custom_rumble 250, custom_loop 206, custom_wind 140, custom_wood 120, custom_water 78, custom_chime 46, custom_rain 24, hum1 0 |
| `light_` (6) | fixture1 46, postlight 25, flame_brazier_short, flame_brazier_tall, flame_pyre, tubelight |
| `corpse_` (22) | 14 placements (impaled/flayed/lynched/crucified props) |
| `info_` (5) | target 119, skullwiz_destination 14, teleportinstant_dest 8, player_startdev 4, camera_destination 0 |
| other | fx_drip 77, mapobject_custom 0 |

Health and model per monster: `scratch/an/monsters.txt`. Most AD spawns go through `chaosmode.qc` wrappers, so the
static parse doesn't see their health.

### 3.2 Shared classnames (109): AD defines a spawn function Quake VR already has

The semantics change in an AD map for nearly all of them, through extra keys, spawnflags or estate. Measured with
`keys.py`: AD-only keys on these classnames across AD's maps.

| Classname(s) | Ours from | What AD adds (placements) | Conflict class |
|---|---|---|---|
| `light` (15,708), `light_*` torches/flames/globe/fluoro, `light_candle` | id / Rogue | `color` 1390, `customstyle_string`; AD models/skins for flames; switchable target lights with a style array | **logic + assets** |
| `path_corner` (2,197) | id | `targetback`, `corner_event`, `corner_route`, `target2`, `alpha` | logic |
| `func_door` (1,921), `func_door_secret`, `func_button` (482), `func_plat`, `func_train`, `func_wall` (737), `func_illusionary` (1,159), `func_episodegate`/`func_bossgate` | id | `locksounds` 562, `customkey`, `message2`, `alpha`, `estate`, `noradiusdmg`, trains with models/reverse/non-solid | logic |
| `trigger_once` (1,064), `_multiple`, `_relay` (2,655), `_counter`, `_secret`, `_teleport`, `_push`, `_hurt`, `_monsterjump`, `_changelevel`, `_setskill` | id | inventory checks (`upgrade_axe` ...), `target2`, `noradiusdmg`, `cooponly`, start-off, 7 spawn locations per changelevel | logic. Our trigger_multiple errored on AD's flag 1 in the probe. |
| `item_health` (1,323), `item_shells`/`_spikes`/`_rockets`/`_cells`, `item_armor*`, `item_artifact_*`, `item_key1/2`, `item_sigil` | id / MG | `respawn_time/count/trig`, `skin_override`, `frame_box`, `item_tossvel`, `progspawnlist`, A_LARGE/A_LID flags | logic + models |
| `item_plasma` (82) | Rogue (plasma cells) | AD: an alternative cells box | **name clash**: same class, different item |
| `item_artifact_wetsuit` | Hipnotic | AD: 5-minute air, skins | clash |
| `weapon_*` (7 vanilla) | id | `frame_box`, `progspawnlist`, `respawn_time`, `cooponly` | logic |
| `monster_knight` (939), `_wizard` 510, `_zombie` 219, `_demon1`, `_hell_knight`, `_enforcer`, `_army`, `_tarbaby`, `_shambler`, `_ogre`, `_shalrath`, `_dog`, `_fish`, `_boss`, `_oldone` | id (VR-extended) | `exactskin` 297+, `noinfighting`, `deathtarget`, `nomonstercount`, `spawndelay`, `gibondeath`, `bodyfadeaway`, `no_zaware`, `pain_ignore`, `poisonous`, `death_dmg`, `minion_maxtrigger`; spawnflags 8/16/32/64/128 (no sight sound, no idle, no spawn fx, **delayed spawn**, **angry spawn**), 4096 ghost, 8192 point knight; per-monster variant bits 2/4 | **AI + flags** (collide with MG's 4/8/16 and Rogue's statue 2 and tarbaby 2/4/8) |
| `monster_eel` (12) | Rogue | AD: plasma-spitting eel | **name clash** |
| `monster_mummy` (5) | Rogue | AD: a zombie variant | **name clash** |
| `func_bob` (68), `misc_model` (425), `trigger_fog` (131), `trigger_heal` (19) | MG1/MG3 (`vr_mg_*`) | AD's own keys and semantics | **name clash** with MG |
| `func_breakable` (1,810) | MG3 (`vr_mg3_shub_zombie.qc`) | AD's breakable system | **name clash** |
| `func_rotate_*`, `rotate_object`, `info_rotate`, `path_rotate`, `func_movewall`, `func_pushable` | Hipnotic | AD's copy of Hipnotic rotation (`hiprot.qc`) | near-identical; ours can serve |
| `misc_explobox(2)`, `misc_fireball`, `trap_spikeshooter`, `air_bubbles`, `ambient_*` (vanilla), `info_*`, `worldspawn` | id | delay, float, damage, sounds, liquid checks; worldspawn has 40+ AD keys (section 6) | logic |

---

## 4. What AD needs from the engine (and what ours has)

| Need | AD's use | Quake VR engine | Work |
|---|---|---|---|
| BSP2 | 9 of 23 main maps (`ad_tears` is 80.7 MB) | yes | none |
| Higher limits | `max_edicts 8192` in quake.rc; `ad_sepulcher` has 8,775 map entities | 16,384 default, 32,000 cap | none (but see edict size, section 9) |
| Fog, skybox, alpha | worldspawn `fog` and `sky`; `.alpha` on bmodels and models | yes | reroute AD's stuffcmd fog/sky/`bf`/`v_cshift` through `svc_fog` / MG fog (QC) and a sky change message |
| Translucent water | `water_alpha` worldspawn key, `r_wateralpha` cvar_set | yes (worldspawn alpha keys kept) | map the key, never `cvar_set` the player's cvar |
| Protocol 999 for cinematics | `misc_camera`/`info_camera_destination` (`SVC_SETVIEW`) | 999 is the default | VR comfort design (decision 7: fade to the camera, played out in VR; a "window" option) |
| `checkextension` | DP/FTE point particles, `DP_GFX_FOG`, rain/snow, rotating bmodels, FTE skin contents (ladders), `FRIK_FILE`, `FTE_STRINGS`, `DP_QC_SPRINTF`, `addstat` (CSQC) | Ironwail/QSS subset; no effectinfo particles, no rain/snow TE, no CSQC | The bridge sets AD's `engine`/`ext_*` globals itself: `ENG_FITZ` in P1, so AD takes its sprite-particle fallback. P4: our particle system plays AD's effects (see section 8). |
| `cvar_string` | none | missing | none |
| External bmodels | `precache_model("maps/ad_brk/*.bsp")` | yes | none |
| `owned/` in-place reads | AD assets whose names collide (`progs/v_plasma.mdl`, AD's `player.mdl`/`h_*` heads/`v_*` weapons) | `VR_OwnedFile` (`vr_gamedir.cpp:1446`) supports Dopa/MG1/MG3 | add `ad` as a discovered folder (campaign entry 6) |
| Spawn parms | AD: `parm1..10` inventory, `parm11..16` map variables | VR owns `parm1..50`, MG3 `51..56` | AD state in `parm57..72` (verify the engine's extended parm capacity) |

**The probe of 2026-10-11** ran our current build with AD's paks mounted below quakevr
(`-game hipnotic -game rogue -game ad -game quakevr`), headless mock, not exclusive. AD's maps load and run on Quake
VR's progs today. The only thing missing is the entities that have no spawn function:

| Map | Load (client R_NewMap end) | Edicts | Spawn functions missing (static count) | Frame CPU / GPU avg (mock, 4 s) |
|---|---|---:|---:|---|
| `ad_s1m1` | | 308 | 55 at runtime | |
| `ad_e1m1` | 1.8 s | 270 | 81 (88) | 4.1 / 1.6 ms |
| `ad_tears` (80.7 MB BSP2) | 10.6 s, **9.4 s of it lightmaps** | 2,113 | 994 (1,065) | 6.8 / 2.1 ms |
| `ad_sepulcher` (8,775 map ents) | 6.5 s | 2,416 | 2,986 (3,178), 1,703 of them `misc_gtkmodel` | 5.4 / 2.7 ms |
| `start` | (resolved to id1's start: AD's `start.bsp` is shadowed) | 257 | 0 | |

Per map (static: BSP version, MB, entities, missing, AD monsters missing / all monsters): `scratch/an/permap.md`.
Ranges: test maps 13-129 missing; the biggest maps 3,178 (`ad_sepulcher`), 1,065 (`ad_tears`) and 697 (`ad_magna`).
Earlier Map Library measures agree (ROUND21.md: 112-523 per installed AD package).

---

## 5. Integration architecture

### 5.1 Two switches: "AD data present" and "AD rules on"

| Switch | Set by | Read by QC as | Effect |
|---|---|---|---|
| **AD data** | Engine discovery of an `ad` folder: the installer's, or one in any owned Quake root, validated by pak hashes (1.80p1 `pak2.pak`) | `VR_Pack_AD()` (like `VR_Pack_Hipnotic`) | AD-only entities spawn in any map (toolgun, custom maps). Removed when the data is absent: no stubs, no aliases (EXPANSIONS.md:25). |
| **AD rules** (per map) | Engine, **before spawn**: the map is in the AD campaign (`-game ad`, AD's `start`/`ad_*` maps, a Map Library package `madeFor()=="ad"`), or its entity lump carries **at least 2 AD signals** (AD-only classnames from 5.4's table, AD worldspawn keys from section 6) | `AD_Rules()` (a read-only `vr_ad_rules` cvar, published like `vr_campaign`) | AD's semantics for shared classnames, AD spawnflags and keys, AD player rules (debuffs, ladders, jump boots, item respawn, projectile shotguns...). Off everywhere else. |

The AD campaign is entry **6** in `vr_gamedir.cpp`'s campaign table:
`{"ad", "Arcane Dimensions", "start", 6, nativeReady=false, adResources, ...}`. The installer mirror is
`ExpansionDetector.cs`. Being a campaign gives AD what the others have:

- its `start` hub isolated from the other campaigns' (`VR_SkipSearchPath`);
- its `progs.dat`/`csprogs.dat`/`quake.rc`/`gfx` menu lumps never used;
- the save records the campaign (`vr_save_campaign`);
- the Main Menu's campaign entry and the Map Library's "Made for Arcane Dimensions" become playable.

Custom maps made for AD (about 90 Quaddicted packages, section 7) get AD *rules* without being the AD *campaign*.

Detection was checked with `detect.py` and `detect_fp.py`:

- **All 38 AD maps** carry 6 or more AD-only classnames.
- **0 of 107 official maps** (id1, Hipnotic, Rogue) carry 2 or more signals, once `item_weapon` is excluded. It is a
  stray classname in id's own e2m2/e3m7/dm5/dm6.
- Quoth and Copper share a few names (`trigger_ladder`, `func_detail*`, `info_target`, `misc_particle`?). The
  signature table excludes names shared with them. A P1 acceptance test runs the detector over every installed Map
  Library package.
- Saves record `vr_save_ad_rules`, so a loaded save restores the rules before spawn. (As built in P1b: no key is
  needed: a load spawns the map from its BSP first, so the detection runs as for a new map, and the save's campaign
  (`vr_save_campaign`) selects the AD campaign.)

### 5.2 Namespacing AD's QC (a scripted import, not a hand port)

AD is about 100k lines once the CSQC and extension headers are dropped. Hand-porting it file by file the way MG3 was
ported (11k lines) would take many rounds and drift from upstream. Proposal: a **reproducible import tool**,
`Misc/quakevr/ad_import.py`, that:

1. Reads AD's `my_progs/` (the 1.80p1 snapshot; SHA-256 of each file recorded, as for MG3's `quakec_mg3`).
(The plan as written in P0. **As built in P1a, see section 14**: everything is prefixed `ad_`, spawn functions too, and
map keys AD needs that Quake VR lacks are renamed fields that the engine will fill in P1b.)

2. Leaves out the files we replace: `csqc_*`, `dpextensions.qc`, `fteextensions.qc`, and AD's `defs.qc` system header
   (ours is used). In P1 it also leaves out AD's player/client entry points (`ClientConnect`, `PlayerPreThink`,
   `W_Attack`, ...); the bridge takes those.
3. **Renames every AD-defined global, function and field** with the prefix `ad_` (functions, fields) / `AD_`
   (constants). Unchanged:
   - engine system globals and fields, and builtins (taken from our `defs.qc`/`builtins.qc`);
   - fields whose name **and type** match ours where the meaning is the same (`health`, `enemy`, ...). A small reviewed
     allowlist; the others stay prefixed.
   - classnames (spawn functions). AD-only classnames keep their names (`monster_gaunt`, `trigger_ladder`). The 109
     shared classnames' AD versions get `ad__<classname>` (two underscores).
   - map keys. Fields a mapper sets must keep their authored names (`exactskin`, `spawndelay`, `estate`,
     `upgrade_axe`...). Those not already ours keep the bare name, which costs field namespace. AD's other fields are
     prefixed.
4. Writes `QC/ad/*.qc`, or a flat `QC/ad_*.qc` (fteqcc with progs.src paths), headers kept. Adds an "adapted by
   ad_import.py" banner and a per-file adaptation list (OFFICIAL_QC_SOURCE.md rules). Then appends them to `progs.src`
   after `rogue_*`/`honey_*`.
5. Applies a small set of **reviewed patches** (`Misc/quakevr/ad_patches/*.diff`). These are the hooks into the bridge;
   everything else stays mechanical, so the next AD version can be re-imported.

The FGD generator (`fgdgen.py --check`) must cover the new spawn functions. Our generated FGD gets an "Arcane
Dimensions" group; AD's own `ad_1_8.fgd` key docs are the source.

### 5.3 The bridge (`QC/vr_ad_bridge.qc`, hand-written)

| AD expects | Bridge provides |
|---|---|
| `T_Damage` with ammo resistance (`resist_shells/nails/rockets/cells`), debuffs, `th_pain(inflictor, attacker, dmg)` | AD's `ad_T_Damage` becomes a thin adapter to **our** `T_Damage` (VR positional damage, wounds, knockdowns), applying AD's resistance when the target has AD fields. Our `th_pain` calls `ad_th_pain` through a shim for AD monsters. |
| Player state: `.moditems`, upgrades, debuffs, jump boots, ladders, grapple, map variables, 7 spawn locations | Mapped onto our player. Upgrades are weapon variants (section 8). Ladders use our VR climbing. AD's grapple is our `WID_GRAPPLE`. Map variables move to `parm57..72`. |
| `engine`/`ext_*` globals, `configflag`, `temp1`/`scratch*` cvars | The bridge sets them (`ENG_FITZ`, sprites off or reduced). Settings become VR menu options in an "Arcane Dimensions" page, not cvars AD writes. No `cvar_set` on player cvars. |
| stuffcmd `fog`, `r_skyfog`, `sky`, `bf`, `v_cshift` | MG fog (`vr_mg_fog.qc`) for fog, a sky message for `trigger_skybox`, our bonus flash/cshift helpers |
| AD gibs, blood, explosions, particles | Our gib/decal/debris/explosion systems (EXPLOSION_AND_FIRE_EFFECTS.md). AD's material gibs map to ours. |
| AD's `monster_start` (classtype/classgroup/bbox) | Our `walkmonster_start`/`flymonster_start`/`swimmonster_start` plus a per-archetype VR registration (MG3 Phase D: hit zones, decap head, ragdoll seeds, hulls, parry, knockdown, grapple, gibs, dummy) |
| Chaos mode / evil mode | Off (decision 6: an optional P4 task) |

### 5.4 Spawn-time resolution of conflicting classnames

Vittorio asked for conflicting entities to get different names, resolved at map load. Two ways, both cheap:

- **(Recommended) the engine's spawn lookup.** In `ED_LoadFromFile`, when `vr_ad_rules` is on and a function
  `ad__<classname>` exists, call it instead of `<classname>`. One generic hook (a per-ruleset prefix, reusable for
  Quoth or Copper later). Zero edits to the 109 shared spawn functions. Saved games are unaffected: entities are
  restored by fields, and AD code that compares `self.classname` sees the authored name.
- (Alternative, the existing pattern) each shared spawn function starts with `if(AD_Rules()) { ad__func_door(); return; }`,
  like `weapon_mjolnir`/`monster_lava_man`. Explicit, but 109 edits and 109 FGD entries.

Which implementation wins **in an AD map**:

| Category | Proposed owner in AD maps | Why |
|---|---|---|
| World logic (func/trigger/path/info/misc/light/ambient/trap, worldspawn) | AD (`ad__*`) | Estate, item checks, routes and delays are pervasive; reimplementing them on ours would be a port anyway. VR hand-pressing of buttons and VR door feel are kept through bridge hooks. |
| Items, ammo, health, armour, keys, vanilla weapons | AD placement and respawn logic, **our pickup** | VR pickup (hands, holsters, magazines) is ours. AD keys only set where and when an item exists. |
| Vanilla monsters (`monster_knight` ...) | **Ours (VR versions) + an AD-key adapter** (recommended) | Our knights, ogres and zombies have VR melee, limbs, decap and knockdowns. The adapter implements AD's keys on them (`spawndelay` + flags 64/128, `deathtarget`, `nomonstercount`, `noinfighting`, `gibondeath`, `bodyfadeaway`, `no_zaware`, `pain_ignore`, statue/ghost/point-knight, `exactskin` where the skin exists). Decision 1: ours; AD's own monsters get our VR features in P2. |
| Name clashes (`monster_eel`, `monster_mummy`, `item_plasma`, `item_artifact_wetsuit`, `func_breakable`, `func_bob`, `misc_model`, `trigger_fog`, `trigger_heal`) | AD's in AD maps; elsewhere the current owner (Rogue/Hipnotic/MG) | The `weapon_mjolnir` precedent. AD's versions are spawnable anywhere by their `ad__` name (toolgun), when the data is there. |

Outside AD maps, AD-only classnames spawn AD's entities under the **default ruleset**: AD's documented defaults, VR
tuning, no AD player rules. AD's spawnflag meanings apply only to AD classnames there.

### 5.5 Assets

Mount `ad` **below id1**, so it supplies only AD-unique files (the 72 overrides never shadow id1, Hipnotic, Rogue or
quakevr). (As built in P1b, section 15: mounted as the other campaigns' folders, above Quake's, it skips the files an
official pak also has, which is the same.) AD assets whose names collide are read in place through
`owned/ad/<path>` (`VR_OwnedFile`; not needed yet), e.g.
`owned/ad/progs/v_plasma.mdl` and AD's `player.mdl` and heads where AD code needs them. The import tool rewrites those
literal paths.

AD's 23 replacement sounds (ricochets, item pickups) and menu art stay unused. Using AD's sounds in AD maps only would
be a later option.

---

## 6. Global rules that differ from id1 (all gated by `AD_Rules()` unless noted)

| Area | AD | Gate |
|---|---|---|
| Shotguns | **Projectile pellets** with casings by default (impulse 130 toggles; worldspawn `no_sgprojectile`) | AD rules. Default on in AD maps, matching AD's balance (decision 2: AD's pellets, with VR tunables). |
| Ammo resistance | Monsters resist shells/nails/rockets/cells (`resist_*`), with visual tells | AD monsters everywhere (it is part of the monster) |
| Monster projectiles | Speed scales with skill (hard = id speed); Z-aware aim (`no_zaware`) | AD monsters everywhere; vanilla monsters only in AD maps |
| Monster melee | Checks Z (+-64) as well as XY | AD monsters |
| Liquid damage | Monsters take slime/lava damage at their feet (`no_liquiddmg`) | AD rules |
| Breakables | Monsters smash breakables in their way (by flags) | AD entities |
| Infighting | Focus, pain and damage parameters, `noinfighting` | AD rules |
| Bodies | Extra damage checked on the first 3 death frames; `bodyfadeaway`; floor checks (`bodyflrcheck`) | AD rules |
| Items | Respawn system (`respawn_time/count/trig`), coop item rules, `notarget` blocks pickup | AD rules |
| Player | Debuffs (poison, burning), jump boots, ladders, `fall_dmg`, map `gravity`, per-map inventory (`give/take_weapons`, `reset_health`, `max_health`, `currentammo`, ammo/armour keys), 7 spawn points | AD rules |
| Worldspawn keys | `fog`, `fog_density`, `fog_colour`, `fog_dpextra`, `water_alpha`, `particlemax`, `no_item_offset`, `no_zaware`, `no_sgprojectile`, `no_sgcasing`, `no_liquiddmg`, `knight_defskin`, `bodyflrcheck`, `bodyfadeaway`, `sprite_particles`, `no_bigprojectiles`, `mapvar_update`, `upgrade_axe/ssg/lg/ghook`, `give_weapons`, `take_weapons`, `max_health`, `reset_health`, `currentammo`, `ammo_*`, `armortype`, `armorvalue`, `gravity` | AD rules (also detection signals) |
| Persistent settings | `quake.rc` `temp1`/`scratch*` cvars | Replaced by VR menu options |
| Spawnflags | Monsters: 8 no sight sound, 16 no idle, 32 no spawn fx, 64 delayed spawn, 128 angry, 4096 ghost, 8192 point knight, 2/4 variants; any entity: 32 static, 64 start off | Per classname + AD rules. Collides with MG 4/8/16, Rogue statue 2, tarbaby 2/4/8, and VR's flags. |

Map detection signals: `vr_ad_rules` set from the AD campaign, or from 2 or more of: an AD-only classname
(section 3.1 minus `func_detail*`/`func_group`/`info_target`/`misc_external_map`/`item_weapon`/names shared with
Quoth or Copper), or an AD worldspawn key (`no_item_offset`, `upgrade_*`, `take_weapons`, `give_weapons`,
`particlemax`, `fog_dpextra`, `knight_defskin`, `bodyflrcheck`, ...). The game folder `ad` (`-game ad`, Map Library
extract `{base}/ad/`, tag `arcane_dimensions` / `mod=arcanedimensions`) sets the campaign or the package's "made
for".

---

## 7. Maps

AD's own maps (`pak1`):

- **Main** (`readme`): `start`, ad_crucial (the readme says `ad_cruical`), ad_dm1, ad_lavatomb, ad_mountain,
  ad_necrokeep, ad_obd, ad_swampy, ad_end.
- **1.5**: ad_chapters, ad_azad, ad_tfuma, ad_magna, ad_metmon, ad_zendar.
- **1.6**: ad_sepulcher.
- **1.7**: ad_ac, ad_s1m1.
- **1.8**: ad_akalakha, ad_grendel, ad_scastle, ad_tears.
- **Remixes**: ad_dm5, ad_e1m1, ad_e2m2, ad_e2m7.
- **Test maps**: start_test, ad_test1..11.

That is 38 BSPs, 9 of them BSP2. Runes for the secret `ad_end`: D in ad_crucial, N in ad_obd, S in ad_swampy.

**Community maps requiring AD** (Quaddicted index, 2026-10-11):

- 92 packages are tagged `arcane_dimensions`/`mod=arcanedimensions` or extract to `{base}/ad/`, about 4.7 GB in
  total. Among them: Tomb of Fenrir, The Tenacious Tentacle, Grendel's Blade, Chaotic Trinity, The Warden, Solomon's
  Tower, Explore Jam 3, Fortress of the Arcane Conduit, Tower of Minak, Trials of Koohoo (2026), and the Func/Xmas/
  Halloween jams.
- **15 carry their own `progs.dat`**, mostly jams with modified AD progs plus old AD versions. 8 carry `.pak` files.
- The Map Library already labels them "Made for Arcane Dimensions" (`madeFor`, `vr_mapinstall.cpp:1996`). It refuses
  packages carrying progs and drops the `ad` folder. With AD support, those maps get AD rules through detection. The
  progs-carrying jams stay refused until AD support lands (decision 8: they should then work).

---

## 8. VR adaptation list

**Weapons** (P3 makes them usable; P4 polishes them):

| AD weapon | Quake VR form | Work |
|---|---|---|
| Shadow Axe (`IT_UPGRADE_AXE`, +75% damage, knocks down and kills zombies, long pain on several monsters) | `WID_AXE` variant (weapon flag), AD's `v_shadaxe0/3.mdl` read in place | Melee damage scale, zombie knockdown through our knockdown system, grip metadata, holster |
| Widowmaker (`IT_UPGRADE_SSG`, 21 pellets, 3 shells a shot) | `WID_SUPER_SHOTGUN` variant | Ammo per shot vs our break-action reload (3 shells?), spread, AD's `v_shot3.mdl`; projectile pellets if AD rules |
| Plasma Gun (`IT_UPGRADE_LG`, 45 direct + 20 splash, 900 speed, 0.2 s, plasma-climbs walls, no discharge underwater) | **new `WID_AD_PLASMA` (20)**: the LG's secondary flag is already Rogue's plasma, so a variant would need flag combinations | Projectile, splash, wall climb (rocket-jump style), AD's `v_plasma.mdl` via `owned/ad/`, sights/grip metadata |
| Grapple hook (`IT_UPGRADE_GHOOK`, `upgrade_ghook`) | our `WID_GRAPPLE` | Map the item and the worldspawn key only |
| Projectile shotguns + casings | Ruleset option for SG/SSG/WM, our casing system | Projectile pellets fired from the VR muzzle, AD's speeds |

**Items:**

- **Artifacts:**
  - Sharp Shooter: tighter spread.
  - Nail Piercer: nails pass through bodies.
  - Jump Boots: 1-4 power levels.
  - Blast Belt: splash immunity.
  - Lava Shield.
  - Air Tank.
  - Tome of Power: an event item.
  - Each needs a VR powerup indicator (wrist/HUD) and holster-free pickup.
- **Custom keys** `item_keyx` (CKEY1-4, any model): our key inventory shows 4 more.
- `item_backpack(_armour)`, `item_custom`, `item_progspawn`: our pickup path.

**World:**

- `trigger_ladder` uses VR climbing (hands), with a fallback to AD's push.
- `misc_textbook`/`misc_textstory` are readable books. They need a VR reading panel: their text in the world, not a
  centerprint.
- `misc_camera`/cinematics are a comfort problem in VR. Proposed: fade to a fixed "window" panel or skip (open
  question 7).
- `misc_shake` uses our gentle camera-free shake (no view roll in VR).
- Breakables use our debris and physics (box3d) where possible.

**Monsters** (P2: usable everywhere; per-archetype VR registration as MG3 Phase D):

- **Humanoid melee** (dknight, dguard, dsergeant, dfury, zombiek, hogre/ham/mac, minotaur, golem, drole): hit zones,
  decap heads, parry windows (swords and hammers), knockdowns, limbs where the model allows.
- **Ranged soldiers** (army variants, defender, eliminator, pyro, fumigator, centurion on hover board, jim, seeker):
  enemy-gun drops (our `vr_enemyguns.qc` pattern: AD's soldier rocket and plasma guns as droppable guns is a natural
  extension).
- **Flyers** (gargoyle, gaunt, lostsoul, wraith, skullwiz teleporting, nour): foe-grab and grapple rules, hulls.
- **Small** (spider, voreling, swampling, scorpion, boil): grab, throw and stomp, ceiling starts, boils as proximity
  bombs (nail them to walls).
- **Bosses** (judicator, eidolon, icegolem, firetopboss, chthon/shub variants, mammoth): scale and hulls, scripted
  waves (`*_wavetrig`), minions.
- **Ghost variants** (dknightghost, ogre_fishing_ghost): melee-through rules.

Usage order for P2 (placements): dknight 475, zombiek 435, dcrossbow 387, hogre 330, spider 268, dguard 254,
skullwiz 225, lostsoul 177, hogremac 150, boil 117.

**Particles and effects:**

- P1 runs AD's sprite-entity fallback, capped by `particlemax`.
- P4 maps AD's named effects (`effectinfo.txt`: about 150 KB) onto our particle system, or adds
  `FTE_SV_POINTPARTICLES`/`particleeffectnum`/`pointparticles` with an effectinfo subset. Rain/snow (`misc_weather`,
  DP TE): our own weather.

---

## 9. Risks

| Risk | Measure / estimate | Mitigation |
|---|---|---|
| **Globals limit** (v6: 16-bit operands, 65,535) | Ours 26,746 + AD 22,799 (AD's own compile). After dropping CSQC, extension headers and player code, and with our `-O3` local overlap: estimated **+12-18k → about 40-45k**. **Measured in P1a: 42,279 (64.5%; 23,256 left).** | P1's first task builds the import with no behaviour and **measures**. If it ever nears 60k, the fallback is engine support for fteqcc's 32-bit-operand progs (a large engine task: loader, VM, saves). |
| **Edict size** | +813 AD field slots (+42%): edict about 7.7 → 11 KB. At `max_edicts` 16,384 the server allocates about 180 MB (+53 MB). Saves grow only by non-zero fields. **Measured in P1a: 1,934 → 2,815 slots (+881); saves +4.6% (AD's saved globals).** | Share fields with ours where the meaning matches (allowlist). Keep mapper keys as the only bare-named fields. Consider allocating edicts on demand. |
| Progs size / load | Statements +177k (to about 400k), file about 5 MB, a few hundred ms more at progs load, QC compile time up | Measure in P1 |
| Duplicate monster implementations | Our VR monsters vs AD's versions | Section 5.4: ours + an AD-key adapter (decision 1) |
| Spawnflag collisions | AD 8/16/32/64/128 vs MG 4/8/16, Rogue 2/4/8, VR flags | Every AD flag read behind `AD_Rules()` or on AD classnames only. The `MG_deferred` precedent: separate saved fields, never reuse VR's bits. |
| Field and function collisions | `.color` type, `th_pain` signature, 1,690 same-named functions | Namespacing tool (5.2), allowlist reviewed by hand |
| Asset collisions | 72 overrides, `v_plasma.mdl` semantic clash, AD `start.bsp` | Mount below id1; `owned/ad/` paths; campaign isolation |
| Detection false positives | 0/107 official maps; Quoth/Copper share some names | Threshold of 2 signals, Quoth/Copper exclusion list, test over every Map Library package |
| Big maps | `ad_tears`: 10.6 s load (9.4 s lightmaps); `ad_sepulcher`: 2,986 entities still missing, which will add think cost | Profile after P1 with entities present (`--exclusive`, `vr_profile`); lightmap build is engine work, independent of AD |
| QC per-frame cost | AD's emitters, ambient sounds, estate, breakables | Measure in P1; cap sprite particles; our particles in P4 |
| stuffcmd features | Fog, sky, flashes, cshift sent as text | Bridge (5.3) |
| Licence | Assets non-commercial and only "in its current state" | Never ship modified AD assets; download from official mirrors; credit; Quake VR stays free |
| Upstream drift | AD is unmaintained since 2020; 1.81 is the last | Import tool pinned to the 1.80p1 hashes; older AD versions unsupported (they are detected and refused) |
| Comfort | Cinematic cameras, screen shakes, ladders, jump boots, plasma climbing | VR designs per item (section 8) |

---

## 10. Installer plan

- **Component** `ad` ("Arcane Dimensions 1.81, free, by sock and team"). Pinned file `ad_v1_80p1final.zip`,
  **320,914,417 bytes**, SHA-256 `7ad993da6c760c446ca31fad71e9f5b6c9eee99b6354f161aa746b572fe70a7d`.
- **URLs in order:**
  1. `https://www.quaddicted.com/files/by-sha256/7a/7ad993da6c760c446ca31fad71e9f5b6c9eee99b6354f161aa746b572fe70a7d/ad_v1_80p1final.zip`
     (content-addressed, used for this download);
  2. `https://www.quaddicted.com/filebase/ad_v1_80p1final.zip`;
  3. `https://www.simonoc.com/files/ad/ad_v1_80p1final.zip`;
  4. `https://born-robotic.net/stuff/qstuff/ad_v1_80p1final.zip`.
- The pin goes in `Misc/release/support_assets.json` and `BuiltInComponents`, as for `HdTextures()`, and
  latest.json's `components.ad` can override it.
- **Install** into `<Quake VR base>/ad/`, unmodified: the three paks, `quake.rc` and the txt files. The readme must
  stay with the files, as AD's licence requires. Recorded in install.json as component `ad`.
- **"I already have it"**: detect an existing `ad` folder in owned Quake roots (Steam/GOG/the user's), validated by
  `pak2.pak`'s hash (1.80p1). If valid, use it in place, as for the expansions. Older AD versions are detected and
  reported, not used.
- **Size shown first:** 306 MB download, about 810 MB on disk.
- **Default:** Vittorio suggested on by default. Proposal: **off until P1's acceptance passes** (before that, AD only
  adds maps that are missing their entities), then **on by default**. It shows the size and a credit line, and links
  AD's readme.
- Never mirror it ourselves without sock's OK (section 1). The engine's `vr_campaign_status` reports the AD install
  like the others'.

---

## 11. Phased plan (effort in agent-days of qvr-agent work; Vittorio's playtests extra)

| Phase | Goal | Tasks | Effort |
|---|---|---|---|
| **P0** (this) | Analysis, plan, probe | This document | done |
| **P1a** foundation | AD code compiled into our progs with **no behaviour change** anywhere | Engine: campaign entry 6 `ad` (discovery, hash validation, mount below id1, `VR_SkipSearchPath` rules, `owned/ad/`), `vr_ad_rules` + pre-spawn entity-lump detection + save field, `ad__` spawn-prefix hook in `ED_LoadFromFile`, parm capacity for 57..72. QC: `ad_import.py` + patches, progs.src/FGD/precedence checks green, globals/fields/edict budget measured and recorded, `AD_Rules()`/`VR_Pack_AD()`. Checker `Misc/quakevr/check_ad_entities.py` (per-map missing classnames and keys, as `check_mg3_entities.py`). | done (the QC side; the engine side was built in P1b) |
| **P1b** AD maps play | Every AD map loads with 0 missing spawn functions and is playable start to end under AD rules | Bridge (5.3): damage, player hooks, parms/map variables, fog/sky/cshift, items to our pickups, vanilla-weapon placement logic, AD-key adapter for vanilla monsters, spawnflag gating, ladders (AD push first), books (centerprint first). AD monsters run on AD's AI with minimal VR registration (hulls, gibs, damage). Headless acceptance per map (load, entity census, scripted kill/secret/exit chain through the `start` hub and the rune gate), save/load. Installer component (default off). | built (section 15, with decisions 15, 19, 22, 23, 25 and 31; the remainder in 15.6) |
| **P2** AD content everywhere | AD monsters, items and entities spawn in any campaign and custom map when AD data is present | Default ruleset, gating audit (nothing changes in id1/Hipnotic/Rogue/MG maps; the regression suites prove it), toolgun/spawn menus, Debug menu pages (AD rules toggle, spawn any AD monster), FGD group, VR registration of the monsters by archetype in usage order (hit zones, decap, ragdoll seeds, parry, knockdown, foe-grab/grapple, dummy) | 12-20 |
| **P3** weapons usable in VR | Shadow Axe, Widowmaker, Plasma Gun, AD grapple mapping, projectile shotguns, artifacts, custom keys | Weapon variants/new WID, damage tables, metadata (grip, sights) from AD's models read in place, holsters, reload rules for the Widowmaker, artifact indicators, jump boots, ladders on VR climbing, book panel | 8-12 |
| **P4** polish | Feels native | Our VR models and textures for the AD weapons (shipped as our own assets), AD effects on our particle system (or effectinfo support), weather, comfort designs for cameras and shakes, per-monster VR polish, big-map performance (lightmap build time, think cost), installer default on, chaos mode (optional) | 10-20 (+ Vittorio's modelling time) |

Total: about 47-77 agent-days. P1a is the gate: it measures the globals and edict budget before anything else is
committed to.

**Vittorio's decisions (section 12) placed in the plan:**

- **P2:** AD's own monsters get our VR features (hit zones, dismemberment, knockdowns, shoves, ragdolls, parry), by
  archetype, in order of usage in maps (decisions 1 and 11). AD's rules travel with AD's content outside AD maps
  (decision 9).
- **P3/P4:** the cinematic camera's VR presentation: fade to the cutscene camera played out in VR by default, a
  "window in the world" as an option (decision 7). Chaos and evil modes are an optional P4 task (decision 6).
- **Installer (after P1):** AD's component is on by default once P1's acceptance passes (decision 3). The first URL is
  the unmodified zip on our support release, the official mirrors follow (decision 4).
- **After P1b:** AD-based packages that ship their own progs are allowed and played on our AD port; their QC changes
  are not run (decision 8).
- **Every phase:** latest AD only (decision 10); AD's shipped bugs listed with a fix/keep proposal, decided by
  Vittorio one by one (decision 12).

**Follow-up tasks from the decisions of 2026-10-11 (13-21), not in P1b:**

| Task | Files | Approach | Effort |
|---|---|---|---|
| **Rigs, dismemberment and animations per AD model** (13) | `Quake/vr/vr_ragdoll.cpp` (a seed table per AD model: pelvis, chest, head, limbs), `vr_hitmodel.cpp` (head and limb zones), `QC/vr_ad_monsters.qc` (new: per-archetype registration, the MG3 Phase D pattern: hulls, decap head model, gib table, get-up frames picked from AD's pain/death frames), `QC/vr_limbs.qc` (the rigged-head rule already covers any rigged monster) | The rig is derived from the model at first use, so each AD model needs only its seed points and joint limits. A test map per archetype (the bestiary-test pattern: `vr_mg3_bestiary_test.qc`) kills each monster with a blade, a bullet and a blast and checks ragdoll, limb and head counts. Usage order (decision 11). | about 0.5 agent-day per model family; 30 families: 12-18 |
| **Artifacts and custom pickups everywhere** (16; built: 16.1, 16.2) | `QC/vr_ad_items.qc` (new), our HUD/wrist indicators (`QC/vr_hud*.qc`, the engine's powerup icons), `QC/items.qc` (one `// AD support` call where a pickup is shared) | AD's `item_artifact_*` spawn as AD's entities (AD's own touch), with our player's timers in AD's `.ad_*` fields ticked by the bridge (`VR_AD_PlayerPostThink`) in every map when the data is present; their effects hooked where ours fire (Sharp Shooter: our spread; Nail Piercer: our nails' touch; Blast Belt: our `T_RadiusDamage`; Lava Shield/Air Tank: our water damage). Backpacks and custom keys only in AD levels if they override ours. | 4-6 |
| **Ammo resistance in our weapons** (18; built: 16.4) | `QC/combat.qc` (`T_Damage`: one `// AD support` call), `QC/vr_ad_bridge.qc` | Our weapons tag the damage's ammo type (`vr_damage_ammo`: shells, nails, rockets, cells, melee) where they fire; the bridge scales it by the target's `ad_resist_*` (AD's own `ad_Resist_Damage`), in AD levels or against AD monsters, and plays AD's resist tell. Melee: shells' resistance off (AD's axe ignores it), grenades and rockets the rockets' one. | 2-3 |
| **Crouch in AD** (20; built: 16.9) | `QC/vr_crouch*.qc`, AD's `ad_ai_*` height checks through the bridge | AD aims at `enemy.origin + view_ofs` and checks melee height (+-64): our crouch moves `view_ofs`; check AD's aim and melee read it (tests: a crouched player under AD's soldier and knight), and take AD's height-aware melee for our monsters everywhere if it plays better (an option). | 2-3 |
| **Spawnflags rewrite** (19; built, first step: 16.8) | `QC/vr_spawnflags.qc` (new), every `spawnflags &` test of the shared monsters (`QC/monsters.qc`, `orig_mon_*.qc`) and of the MG/Rogue/Hipnotic gates | One table per ruleset (id, Hipnotic, Rogue, MG, AD, VR) mapping authored bits to named meanings, read once at spawn into separate saved fields (`MG_deferred`'s pattern), so no code tests raw bits of another ruleset. Backwards compatible: the same meaning for every existing map (the campaign A/B spawn-dump regression proves it). | 4-6 |
| **Chaos mode** (17; TODO: 16.13) | AD's `ad_chaosmode.qc` through the bridge | After P2: AD's randomiser over our and AD's monsters, its restart dance replaced by our save hooks. | 3-5 |
| **Global AD projectile/pellet option** (21) | `QC/vr_ad_bridge.qc`, our monsters' missile launches | A cvar applying AD's skill-scaled projectile speeds and pellet shotguns to every monster (pending his answer). | 1-2 |

**Follow-up tasks from the decisions of 2026-10-11 (22-31), not in P1b:**

| Task | Files | Approach | Effort |
|---|---|---|---|
| **Debuffs everywhere, burning merged** (24; built: 16.3) | `QC/vr_ad_bridge.qc` (the player's debuff tick in every map when AD's data is present), our burning (`QC/vr_fire*.qc`), the HUD | AD's `ad_ClientDeBuff`/`ad_ResetDebuffSystem` run from our player think for any player AD's monsters or traps poison or bleed; AD's burning becomes ours (one flame state, our flame visuals and damage, AD's duration and damage table). | 2-3 |
| **Ladders on our climbing** (26; built: 16.5) | `QC/vr_ad_bridge.qc`, `Quake/vr/vr_climb.cpp` (a climbable volume type) | `trigger_ladder`'s brush becomes a hand-climbable volume (grab anywhere inside it, as a ledge's rungs); AD's push-up (jump held) stays for the thumbstick. | 2-3 |
| **HUD space for artifacts and debuffs** (28; built: 16.6) | the wrist/HUD (`Quake/vr/vr_hud*.cpp`) | A row of AD artifact and debuff icons with timers, only drawn when one is active. | 1-2 |
| **Evil mode in the skill selection** (30; built: 16.7) | the skill menu and hub buttons, `QC/vr_ad_bridge.qc` | Skill 4 when AD's data is installed: lower maximum health, each level started with the Shadow Axe (AD's `ad_Client_SetupEvilmode`); needs the Shadow Axe as a VR weapon (P3). | 1-2 after P3 |
| **AD performance in VR** (29; TODO: 16.13) | `vr_profile`, `bench.sh` | With chaos mode: AD's biggest maps profiled exclusive (lightmap build, think cost of emitters/estate/breakables, sprite particles), fixes per finding. | 3-5 |
| **Coop saves** (31) | `QC/vr_ad_bridge.qc`, the save keys | AD's per-client state (debuffs, artifacts, map variables) per player in coop saves. | 1-2 |

---

## 12. Decisions (Vittorio, 2026-10-11)

The open questions of P0 are answered. **Go: P1b (AD maps playable).** Architecture rule for every phase: AD-specific
code stays as isolated as possible and easy to update when a new AD patch comes out, the way the VR code is isolated
from Ironwail. Anything in shared engine or QC code carries a clear `// AD support` comment.

| # | Question | Decision | Phase |
|---|---|---|---|
| 1 | Vanilla monsters in AD maps | **Ours** (the VR versions, with AD's keys adapted). AD's own monster versions must **also** get our VR features later (melee hit zones, dismemberment, knockdowns, shoves, ragdolls...). | P1b (ours), P2 (AD's monsters' VR features) |
| 2 | Shotguns in AD maps | **AD's projectile pellets, tunable for VR**: cvars and VR menu sliders (spread multiplier, speed multiplier...). | P1b |
| 3 | Installer default | AD **on by default only after P1's acceptance**. | P1 end |
| 4 | Mirror | **Host the unmodified AD zip on our support release** (Vittorio asks sock; assumed OK). Our release is the first URL, the official mirrors follow. | installer |
| 5 | AD's player rules | **In AD maps only.** Some may later apply everywhere: the list of differences is below. | P1b |
| 6 | Chaos mode, evil mode | He asked what they are and what they cost: see below. Off until then. | P4 (optional) |
| 7 | Cinematic cameras | **Default: fade to the cutscene camera, played out in VR. Option: a "window in the world"** showing it (customisable). | P1b: not breaking play; the VR presentation is its own task (P3/P4) |
| 8 | AD-based packages with their own modified progs | **Should work** in our port once AD is supported (their QC changes are not run; the maps are). | after P1b |
| 9 | AD content outside AD maps | **AD's rules stay with AD's content** (an AD monster keeps its ammo resistance and projectile speeds in any map). He asked for the differences and concerns: below. | P2 |
| 10 | Versions | **Latest AD only** (1.80 patch 1, "1.81"); every older install is refused with a clear message. | P1b |
| 11 | P2 monster order | **By usage in maps** (dknight 475, zombiek 435, dcrossbow 387, hogre 330, spider 268, ...). | P2 |
| 12 | AD's shipped bugs | **Decided per bug**: each one found is listed with a fix/keep proposal (section 15.5); none is fixed silently. | every phase |

**Follow-up decisions (Vittorio, 2026-10-11, after the answers above).**

| # | Decision | Phase |
|---|---|---|
| 13 | **AD monsters: a ragdoll rig, dismemberment and the needed animations** (get-up frames, head zone and head gib) **for each AD model**. | P2 |
| 14 | **AD's damage code calls our hooks** (damage, death, melee, knockdown, gore). | P1b (the bridge) |
| 15 | **AD levels only:** fall damage rules; megahealth rot; level-change and death rules for items and powerups (`ITEMRESET`, the level-start inventory restore); AD-specific spawnflags. | P1b |
| 16 | **Everywhere:** the new artifacts (Sharp Shooter, Nail Piercer, Jump Boots, Blast Belt, Lava Shield, Air Tank, Tome of Power) and AD's custom pickups (custom keys, backpacks), **unless** one overrides an existing pickup or rule: then AD levels only. | P2/P3 |
| 17 | **Evil mode: implement.** Chaos mode: later, once every AD monster has VR support. | P1b (evil mode, if cheap), P4 (chaos) |
| 18 | **Ammo resistance in our weapons too** (melee and grenades included where sensible), effective only in AD levels, or against AD-specific monsters elsewhere. | P2 |
| 19 | **Spawnflags:** AD's gated to AD levels. A rewrite of the spawnflag handling is allowed if fully backwards compatible and it avoids or minimises conflicts (id1/Hipnotic/Rogue/MG/VR flags). | P1b (gating), follow-up (rewrite) |
| 20 | **Crouching:** our VR crouch adapted to AD (its height-aware aim and melee); AD improvements worth having apply in every mode. | follow-up |
| 21 | **Skill-scaled projectile speeds and pellet shotguns:** per entity by default (an AD monster uses AD's), plus a global option to apply them to every monster (pending his answer). | P2 |

**Follow-up decisions 2 (Vittorio, 2026-10-11).**

| # | Decision | Phase |
|---|---|---|
| 22 | **Projectile speeds and pellet shotguns: Auto by default** (vanilla in non-AD maps, AD's behaviour in AD maps), with a global override, cvar and menu: **Auto / Vanilla / AD** (settles 21). | P1b |
| 23 | **AD weapons: a first version on the "simple reload"** (move to the holster); grips, sights and reloads polished later. | P1b (simple reload), P3/P4 (polish) |
| 24 | **Debuffs (poison, burning, bleeding) everywhere**; AD's burning merged with our burning. | follow-up |
| 25 | **Item respawn timers: AD levels only.** | P1b |
| 26 | **AD ladders (`trigger_ladder`) work nicely with our hand-climbing.** | follow-up |
| 27 | **Jump Boots: no comfort limits** (the mod is hardcore). | P3 |
| 28 | **HUD: space reserved for artifacts and debuffs.** | follow-up |
| 29 | **Later, with chaos mode: deep performance profiling of AD maps in VR.** | P4 |
| 30 | **Evil mode in the skill selection**, available everywhere when AD's data is installed (it needs the Shadow Axe), not only in AD maps (the coordinator's call). | follow-up (settles 17's scope) |
| 31 | **Saves:** AD's state in save games; loading an AD save switches AD mode on and restores AD's state; loading a non-AD save from an AD session turns it off cleanly. Coop: later. | P1b |

**Answers to his questions.**

- **AD's vanilla monsters vs ours (1).** AD rewrote every id monster in its own framework: ammo resistance
  (`resist_*`), Z-aware aim, projectile speeds scaled by skill, melee that checks height, AD's gib materials and
  `bodyfadeaway`, minions, estate (on/off by triggers), the spawn keys of section 6 and variant spawnflags (statue,
  ghost, point knight, `exactskin`). Ours add the VR layer: positional damage and hit zones, limbs and decapitation,
  knockdowns, shoves, foe-grab, ragdolls, parry windows. **Reuse:** most of our VR layer is generic (positional damage,
  knockdowns, shoves, foe-grab, ragdolls and gibs run on any monster with a hull and a model) and is attached by a
  per-archetype registration (MG3 Phase D did the same for MG3's monsters). What is per monster is data: the hit
  zones and limb splits of each AD model, which frames are melee, and the pain/death hooks into AD's AI. So AD's
  monsters reuse the code; they need their data, archetype by archetype (P2, in usage order).
- **AD's player rules (5)**, everything AD's `client*.qc`, `weapons.qc` and `items.qc` change for the player:
  - debuffs: poison, burning, bleeding (set by AD monsters and traps, ticked every frame);
  - fall damage (`fall_dmg`, `fall_dmgwater`) and map gravity (`gravity` worldspawn key);
  - per-map inventory: `give_weapons`, `take_weapons`, `reset_health`, `max_health`, `currentammo`, `ammo_*`,
    `armortype`, `armorvalue`, `no_axestart`;
  - jump boots (1-4 power levels), ladders (`trigger_ladder`), AD's grapple hook (`upgrade_ghook`);
  - the weapon upgrades: Shadow Axe, Widowmaker, Plasma Gun (P3);
  - projectile shotguns with casings (decision 2);
  - item respawn (`respawn_time/count/trig`), coop item rules, `notarget` blocking pickups, backpacks keeping their
    items, custom keys (`item_keyx`, 4 more keys);
  - 7 spawn locations chosen by map variables, map variables carried between maps (`parm11..16` in AD; ours
    `parm57..72`);
  - cinematic cameras and the intermission camera cycle, secret-location cameras;
  - info prints (inventory, map variables) on impulses.
  Candidates to apply universally later: debuffs (they come with AD's monsters anyway), fall damage as an option,
  ladders (VR climbing), custom keys.
- **Chaos and evil modes (6).** *Chaos mode* ("Markie Madness", `chaosmode.qc`, 2.4k lines) randomises the map: each
  monster, ammo, health, armour and artifact is swapped for another of its class from a seeded random table, with
  saves and quickloads re-synchronised by a restart. *Evil mode* is AD's skill 4 ("Nightmare+"): a lower maximum health
  and every map started with the Shadow Axe. Evil mode is small (skill 4 plus three player rules). Chaos mode is
  significant: it wraps most spawn functions, ours included, and its save/restart dance fights our saves; it would
  be its own P4 task with its own tests.
- **AD content outside AD maps (9).** With AD's rules kept, AD's monsters behave as in AD everywhere: they resist some
  ammo types (shown by AD's tells), aim at height, scale projectile speed by skill. Concerns:
  - balance: an id map's ammo layout is not planned for resistant monsters;
  - AD's spawnflags (8/16/32/64/128...) collide with MG's, Rogue's and ours, so they are read only on AD's
    classnames there (section 6);
  - AD's world entities need AD's world setup (precaches, light styles, the particle system), which outside AD maps
    must be a reduced setup run on first use;
  - AD's player rules (debuffs from AD monsters, for one) need a decision per rule when an AD monster is placed in an
    id map; proposal: debuffs on (they are the monster's attack), the rest off.

---

## 13. Prototype notes (P0 probe, 2026-10-11)

- What was done:
  - `scratch/ad/ad` holds the extracted release, linked into the agent's kit game folder as `qbase/ad`.
  - The current build ran with `-game hipnotic -game rogue -game ad -game quakevr` on `ad_s1m1`, `ad_e1m1`,
    `ad_tears`, `ad_sepulcher` and `start` (`scratch/ad_probe.sh`).
- **No engine errors and no crashes.** BSP2, AD's skies and fog keys loaded. The four AD maps ran 300 frames with
  their AD entities absent.
- Our `trigger_multiple` printed OBJECT ERRORs on AD's flag 1 (`ad_s1m1`: 4), and `trigger_changelevel` printed one in
  `ad_tears`. Both are expected conflicts (3.2).
- `start` resolved to id1's start (AD's is shadowed), as the campaign isolation would also do. Playing AD's hub needs
  the campaign entry.
- Timings are mock-headless and not exclusive: for orientation only (section 4).

---

## 14. Phase 1a (built 2026-10-11): AD's QuakeC in the progs, inert

**What exists.**

- `Misc/quakevr/ad_import.py` regenerates `QC/ad/` (139 files plus `ad_builtins.qc`, about 106k lines) and the AD
  block at the end of `QC/progs.src` from the official release.
  - The release's `pak0.pak`, `pak2.pak` and `ad_1_8.fgd` are SHA-256 pinned. `--release <dir>` points at the unpacked
    zip; the default is the worktree's `scratch/ad/ad`.
  - `QC/ad/AD_IMPORT.txt` lists what was kept, renamed and dropped.
  - `--verify-frames` (after a build) checks every AD frame function against AD's own `progs.dat`. Current result:
    7,311 of 7,311 equal.
- `QC/vr_ad_bridge.qc` holds the two switches, `VR_Pack_AD()` and `AD_Rules()`. Both return FALSE until P1b.
- CREDITS.md credits AD's QC (GPL-2.0-or-later).

**The renaming, as built.** Every identifier AD defines becomes `ad_<name>`, including:

- spawn functions (`ad_monster_gaunt`, `ad_func_door`);
- engine entry points (`ad_StartFrame`, `ad_PlayerPreThink`, ...);
- constants.

Kept as written:

- QC keywords, compiler intrinsics, and `x`/`y`/`z`;
- the engine's system globals and fields;
- the 76 builtins Quake VR declares with the same signature (frikbot's `sound`/`stuffcmd`/`WriteByte` wrappers
  included);
- 43 map-key fields Quake VR already has with the same type (`target`, `wait`, `delay`, `alpha`, `angles`...).

Builtins:

- **By name (`#0`), in `ad_builtins.qc`:** the 11 extension builtins we lack (`pointparticles`, `particleeffectnum`,
  `addstat`, `te_particlerain`...). The engine binds them only if it has them; otherwise they error only if called.
- **By number:** AD's own names for standard builtins (`walkmove_builtin` #32, `centerprint_msg*` #73) and different
  signatures (`ad_strcat` #115, which takes up to 8 strings; ours declares 2).

**Change from the plan (5.2): map keys.**

- The plan was to keep AD's map-key fields that Quake VR lacks as bare names. As built, all 242 of them are renamed:
  `ad_exactskin`, `ad_spawndelay`, `ad_estate`, ...
- Three of them clash in type: `color` (AD vector, ours float) and `target2`/`target3` (AD string, ours entity).
- Why: P1a adds no field a map can fill, so no map changes.
- **P1b work:** the engine fills `ad_<key>` from a map's `<key>` when a key has no field, or always when AD rules are
  on (this settles `color`, `target2` and `target3`).

**Mechanical fixes.**

- Global arrays sized by a named constant get the number, because our fteqcc flags (autoproto) reject a constant
  there.
- `autocvar(name, ...)` keeps its cvar name.
- An array initializer's semicolon is kept.
- AD's CRLF becomes LF.
- One `progs.src` entry per file. fteqcc numbers `$frame` macros per entry; folded into one `#include` list, 7,130 of
  7,311 frame numbers shifted.
- Warnings F302 (possibly uninitialised local) and F322 (`if (string)` tests null, as in AD's own compile) are off
  from `ad_builtins.qc` on. That is why AD stays last in progs.src.

**Precedence patches.** There are 12 reviewed patches, in `PATCHES` in the script. They are the expressions
`check_qc_precedence.py` flags, parenthesised the way fteqcc's QC priorities read them, so the behaviour stays AD's as
shipped:

- `x & FLAG == 0` is `(x & FLAG) == 0`;
- `!a && b` is `!(a && b)`;
- `a && b || c && d` is `((a && b) || c) && d`.

Several are latent AD bugs, marked "AD as shipped": the spider/swampling/voreling ceiling test, the gargoyle/gaunt
perch test, the mammoth's walk-frame light, the books' screen tint, `ClientDeBuff`'s bleed/poison test, and the
lightning-effect target check. **P1b decision:** keep AD's behaviour, or fix the intent?

The flagged functions' instructions were compared before and after the patches. They are identical, except the `!`
cases, which compile to `NE_F`+`IF` instead of `NOT_F`+`IFNOT`, which is equivalent.

**Budget (measured).**

| | Before | With AD | Limit / note |
|---|---:|---:|---|
| globals | 26,746 | 42,279 | 65,535: 64.5% used, 23,256 left |
| entity fields (slots) | 1,934 | 2,815 | +881 slots: +3.4 KB for each edict in use; `max_edicts` 16,384 reserves 176 MB, was 121 MB |
| statements | 224,782 | 400,597 | |
| functions | 7,526 | 17,271 | |
| strings | 345 KB | 596 KB | |
| progs.dat | 2.63 MB | 4.79 MB | |
| progs load (each map) | 6.8-7.0 ms | 12.3-12.7 ms | `VR_TimeMark` "server: progs" |
| a save (e1m1) | 229,968 B | 240,541 B | +4.6%: AD's saved globals. A save made before loads with the new progs. |

**No behaviour change: how it was checked.**

1. **Instructions.** All 7,284 Quake VR functions compile to the same instructions as before. This is a name-based
   comparison of the two builds' `qc.asm` (globals, fields and immediates by name or value; line and temp numbers
   ignored). The only functions added outside AD are `VR_Pack_AD` and `AD_Rules`. The progs' tables are not a byte
   prefix of the old ones: fteqcc lays globals out differently. Nothing depends on offsets: the system section and the
   CRC (5927) are unchanged, and the engine finds Quake VR's fields by name.
2. **Campaign A/B.** Each campaign ran the same script with the old and the new progs (`-nomapindex -noaddons`): e1m1,
   start, hip1m1, r1m1, Dopa e5m1, MG1 mge1m1 and MG3 map1. The script loads the map, takes an `edicts` dump, wakes
   every monster (`vr_test_monsters 2`), lets them fight the god-mode player for 600 frames, kills them all (3), then
   dumps again.
   - No errors in either build.
   - The same monster totals in every map (live/awake/waiting): 23, 0, 58, 46, 37 + 7 waiting, 90 + 20 waiting, and
     75 + 13 waiting.
   - The same spawned entities (classname, model, targets, flags). The only differences are the transient weapon
     instances also seen between two runs of the same build: these runs are not frame-deterministic.
3. **Benchmarks.** `bench.sh --fast --reps 3`, exclusive: `ai_crowd_64`, `combat_48` and `idle_e1m1` show no change
   past 5%. The largest were combat_48's p99 at +1.2% and ai_crowd_64's CPU at -2.2%: noise.

**Not done in P1a (P1b's list, section 11; done in P1b, section 15, but the reduced setup outside AD maps: P2).**

- The engine side: campaign entry 6, AD data discovery, the `ad_` spawn hook, the `ad_<key>` key fallback,
  `vr_ad_rules` with detection before spawn.
- The bridge proper: damage, player, fog, items.
- Running any AD code. AD's `ad_worldspawn` setup (precaches, light-style and pendulum arrays) must run first in AD
  maps; outside them, AD entities need a reduced setup.

---

## 15. Phase 1b (built 2026-10-11): AD maps play

### 15.1 What exists

**Engine** (`Quake/vr/vr_ad.cpp`; the shared files' calls are marked "AD support"):

- **Campaign 6, `ad` "Arcane Dimensions"** (`vr_gamedir.cpp`), found like the other native campaigns (an `ad` folder in
  a base dir or an owned root). Developer launch only for now: `vr_campaign_native ad`, or Debug > Tests > Arcane
  Dimensions: Play (Developer); Official Campaigns lists it dimmed ("native in progress"). Single player only.
  The installer's expansion table has the row (`ExpansionDetector.cs`, not ready), as its self-test requires.
- **Latest release only** (decision 10): `pak2.pak` (patch 1) must be there with its pinned size and the SHA-256 of
  its header and directory (cheap at start-up). Another version is refused with a console line naming
  `ad_v1_80p1final.zip` (status 3: `vr_ad_status 3`, "another version" in `vr_campaign_status`).
- **AD supplies only its own files.** Mounted while AD is the campaign (above Quake's and the packs', under quakevr), its
  folder skips the 82 files an official pak also has (`vr_ad_data.inc`, written by `ad_import.py --engine-lists`
  from the paks' names: AD's menu art, weapon models, heads, `player.mdl`, 23 sounds, `progs.dat`), its own
  `quake.rc` (impulse binds, `max_edicts`) and `csprogs.dat`. AD's `start.bsp` is its hub.
- **`vr_ad_rules`** (ROM; QC `AD_Rules()`), set before each spawn, a saved game's too: the AD campaign, or a map whose
  entity text carries 2+ AD signals (AD-only classnames, AD's worldspawn keys) while AD's data is mounted.
  `vr_ad_mounted` says whether AD's files are on the search path.
- **Spawn ownership** (`ED_LoadFromFile`): in an AD map, `ad_<classname>` spawns AD's entities and AD's version of
  the shared world logic; Quake VR's stays for the classes in 15.2.
- **Map keys**: a key Quake VR has no field (of that type) for fills AD's `ad_<key>` (`ED_ParseEdict`; this settles
  `color`, `target2`, `target3`). AD's `water_alpha` worldspawn key is read as `wateralpha` (`gl_rmisc.c`).
- **Spawn parms 57..72** carry AD's level state (the engine's extended parms now go to 72).
- `vr_ad_info` prints the data, the rules (and why) and the spawn counts.

**QC**: `QC/vr_ad_defs.qc` (the switches and the bridge's declarations, early) and `QC/vr_ad_bridge.qc` (after AD's
files). `ad_import.py` applies 8 reviewed bridge patches to AD's code (listed in `QC/ad/AD_IMPORT.txt`); every
`VR_AD_` name is kept by the renaming.

| Area | In an AD map |
|---|---|
| World | AD's world setup after ours (`ad_worldspawn`: precaches, body queue, light-style/pendulum/map-variable arrays, gravity and fall rules, the particle chain), AD's frame (`ad_StartFrame`). AD's engine detection answers Fitz (sprite particles, no CSQC). |
| Fog | AD's fog controller by `svc_fog` (MG's `MG_SetFog`); AD's stuffed fog and sky-fog commands are not sent. |
| Damage | Quake VR's `T_Damage` for the player and our monsters, also when AD's code deals it (`VR_AD_DamageOurs`; AD's NOARMOR kept); AD's `T_Damage` for AD's entities, also when ours deals it (`AD_Damage`: our damage multipliers, sneak attacks, the motion log, melee bloodlust), with our hit hooks on AD's monsters (`VR_AD_Hit`: damage numbers, gore, small gibs, wounds) (decision 14). |
| Player | What AD's code reads of a player (class, entity state, AD's powerups and debuffs reset); AD's spawn points (`info_player_start2` by an exit's `startspawn2`, the hub's returns) and the spawn point's targets; ladders (`trigger_ladder`: jump climbs, AD's way); AD's fall rules (decision 15: the map's fall speeds and damage); AD's debuffs (poison, burning, bleeding) ticked; AD's megahealth rot (decision 15: one rot whatever the megahealths). |
| Levels | AD's state in parm57..72 (map variables, upgrades); the worldspawn inventory keys (`reset_health`, `max_health`, `ammo_*`, `currentammo`, `armortype`/`armorvalue`) at a level's start; the hub keeps the inventory (AD resets it only when all runes are found); exits: no map key = this map, `ad_tears`' exits go to the hub (AD's own hack), STARTOFF until triggered, AD's on-exit rules (debuffs and AD powerups end, `mapvar_update`, `startspawn2`, RESETINV). |
| Our monsters | AD's delayed spawn (64) and angry spawn (128), no spawn effect (32), `nomonstercount`, `deathtarget`; AD's path_corners move them as ours. The id/Hipnotic/Rogue/Honey spawnflag meanings are off (`VR_OtherFlags`, as in MG's maps; decision 19). |
| Items | Ours, with AD's STARTOFF (64: appears when triggered) and respawn (decision 25: RESPAWN 16, `respawn_time` default 30 s, `respawn_trig`, `respawn_count`). |
| Shotguns | AD's projectile pellets (decisions 2, 22): `vr_ad_projectiles` 0 Auto (AD's in AD maps, hitscan elsewhere), 1 Vanilla, 2 AD's wherever its data is mounted; a map's `no_sgprojectile` keeps AD's hitscan. Yours from the muzzle along the gun at AD's spread times `vr_ad_pellet_spread`, AD's speed times `vr_ad_pellet_speed`, the Weapon Damage pellet times `vr_ad_pellet_damage`; our grunts' at AD's monster speed; our hell knights', scrags', ogres' and zombies' projectiles at AD's speeds by skill. AD's monsters always fire AD's. VR Settings > Combat > Arcane Dimensions. |
| AD's weapons | A first version (decision 23): the upgrades are the player's, as in AD (carried in parm63). Shadow Axe: the axe hits 35 for 20. Widowmaker: the super shotgun fires 21 pellets for 3 shells (a third from the reserve) and reloads at the hip (the simple reload). Plasma Gun: the lightning gun fires AD's plasma bolts (0.2 s, a cell; misfires under water). Their pickups (`weapon_upgrade_axe/ssg/lg`, `weapon_hammer`, `weapon_plasmagun`) are Quake VR's weapon pickups with AD's models; Quake VR's models in the hands. AD's grapple pickup gives nothing (Quake VR's hook is equipment). AD's own weapon pickup code never runs on a player (it would overwrite the hands' weapon id). |
| Cinematic cameras | AD's own system, unchanged: the player is moved to the camera's view, held (no damage, not solid) and put back when it ends; "not breaking play" (15.3). The VR presentation (decision 7) is its own task (15.6). |

**Settings and test aids**: VR Settings > Combat > Arcane Dimensions (Projectile Rules, Pellet Spread, Speed, Damage);
Debug > Tests: Arcane Dimensions: Play (Developer), Play a Cinematic Camera (`vr_ad_test 1`), Cinematic Report
(`vr_ad_test 2`), Widowmaker Pickup Ahead (`vr_ad_test 5`), Upgrades Report (`vr_ad_test 6`), Status (`vr_ad_info`);
`vr_ad_test 3`/`4` fire the Widowmaker and the Plasma Gun (console). Whole-Map Monsters (`vr_test_monsters`) wakes AD's monsters by AD's AI and
brings in AD's delayed ones.

### 15.2 Acceptance (headless mock, fast mode, skill 1)

**Every AD map** (38: `start`, `start_test`, the 23 `ad_*` maps, 2 DM maps, 12 test maps) through
`scratch/ad_maps.sh` (the AD campaign, the map, `vr_ad_info`, every monster woken against the god-mode player for 600
frames, the waiting ones brought in, woken again, all killed, 300 frames more):

| Map | Entities by AD's code | Shared classnames by Quake VR's | Edicts after spawn | Monsters (live + waiting) | Missing spawn functions | Object errors | Errors |
|---|---:|---:|---:|---:|---:|---:|---|
| ad_ac | 1766 | 278 | 1349 | 115 + 45 | 0 | 0 | 0 |
| ad_akalakha | 851 | 105 | 1012 | 73 + 24 | 0 | 0 | 0 |
| ad_azad | 1403 | 205 | 1442 | 108 + 49 | 0 | 0 | 0 |
| ad_chapters | 759 | 35 | 631 | 6 + 0 | 0 | 0 | 0 |
| ad_crucial | 1461 | 238 | 1571 | 185 + 21 | 0 | 0 | 0 |
| ad_dm1 | 641 | 130 | 776 | 91 + 18 | 0 | 0 | 0 |
| ad_dm5 | 1063 | 149 | 860 | 65 + 37 | 0 | 0 | 0 |
| ad_e1m1 | 934 | 60 | 568 | 40 + 8 | 0 | 0 | 0 |
| ad_e2m2 | 1326 | 139 | 1085 | 69 + 0 | 0 | 0 | 0 |
| ad_e2m7 | 530 | 76 | 713 | 80 + 4 | 0 | 0 | 0 |
| ad_end | 336 | 42 | 454 | 40 + 4 | 0 | 0 | 0 |
| ad_grendel | 1860 | 275 | 1790 | 115 + 81 | 0 | 0 | 0 |
| ad_lavatomb | 797 | 271 | 1092 | 154 + 47 | 0 | 0 | 0 |
| ad_magna | 3695 | 421 | 3301 | 236 + 75 | 0 | 0 | 0 |
| ad_metmon | 1519 | 141 | 1074 | 62 + 22 | 0 | 0 | 0 |
| ad_mountain | 1738 | 189 | 1744 | 72 + 41 | 0 | 0 | 0 |
| ad_necrokeep | 2569 | 245 | 2435 | 170 + 21 | 0 | 0 | 0 |
| ad_obd | 1755 | 295 | 1333 | 159 + 74 | 0 | 0 | 0 |
| ad_s1m1 | 469 | 65 | 404 | 23 + 0 | 0 | 0 | 0 |
| ad_scastle | 1145 | 174 | 891 | 84 + 26 | 0 | 0 | 0 |
| ad_sepulcher | 8063 | 461 | 3897 | 231 + 82 | 0 | 0 | 0 |
| ad_swampy | 2954 | 525 | 2793 | 334 + 84 | 0 | 0 | 0 |
| ad_tears | 2981 | 788 | 3550 | 323 + 431 | 0 | 0 | 0 |
| ad_test1 | 695 | 93 | 825 | 46 + 4 | 0 | 0 | 0 |
| ad_test10 | 255 | 37 | 225 | 17 + 18 | 0 | 0 | 0 |
| ad_test11 | 110 | 45 | 314 | 0 + 0 | 0 | 0 | 0 |
| ad_test2 | 216 | 13 | 205 | 29 + 0 | 0 | 0 | 0 |
| ad_test3 | 550 | 85 | 696 | 70 + 2 | 0 | 0 | 0 |
| ad_test4 | 69 | 27 | 100 | 12 + 0 | 0 | 0 | 0 |
| ad_test5 | 241 | 24 | 220 | 8 + 2 | 0 | 0 | 0 |
| ad_test6 | 866 | 138 | 895 | 110 + 10 | 0 | 0 | 0 |
| ad_test7 | 571 | 28 | 451 | 29 + 0 | 0 | 0 | 0 |
| ad_test8 | 84 | 14 | 113 | 14 + 0 | 0 | 0 | 0 |
| ad_test9 | 126 | 19 | 149 | 10 + 4 | 0 | 0 | 0 |
| ad_tfuma | 1770 | 209 | 1724 | 172 + 49 | 0 | 0 | 0 |
| ad_zendar | 1716 | 249 | 1317 | 116 + 38 | 0 | 0 | 0 |
| start | 861 | 67 | 782 | 6 + 0 | 0 | 0 | 0 |
| start_test | 227 | 19 | 271 | 0 + 0 | 0 | 0 | 0 |

(The table is the last full run; after it, the vanilla monsters' projectile speeds and two test aids were added, and
`ad_e1m1`, `ad_s1m1`, `start`, Quake's `e1m1` and `hip1m1` ran again: no errors.)

- **0 missing spawn functions** on every map (P0's probe: 13-3,178 a map), no object errors, no QC or engine error.
- The first full run found one engine-side error in AD's big fights (`ad_tears`, `ad_magna`, `ad_metmon`: a client
  `Host_Error`, the message stream one byte off): Quake VR's beam temp entities carry a beam id byte after the entity
  (`VR_ParseBeamEntity`), AD's 17 beam writes did not. `ad_import.py` now adds it (a mechanical rule, counted in
  `AD_IMPORT.txt`), and the server's broadcast boundaries (`vr_server.cpp tempEntityLength`) count that byte for
  every beam: they put a boundary one byte early for Quake VR's own beams too, which a full datagram could cut there.
- **Weapons** (`vr_ad_test 3/4` on `ad_e1m1`): the Widowmaker fires 22 projectiles (21 pellets and AD's tracer), its
  two barrels and a reserve shell; with Projectile Rules Vanilla it fires hitscan (no projectile) for the same 3
  shells. The Plasma Gun fires one bolt for a cell of its magazine. The Shadow Axe's blow is x1.75. AD's pellets are
  off in `e1m1` (Auto).
- **Saves** (`scratch/ad_saves.sh`, decision 31): an AD save (`ad_e1m1`) loaded from a Quake session (`e1m1`) switches
  to the AD campaign, AD's rules on, 40 + 8 waiting monsters as saved; a Quake save loaded from an AD session (its hub)
  switches back to Quake, AD's rules off, 23 monsters, no error either way.
- **Version check**: an AD folder without patch 1's `pak2.pak` (a higher-priority base dir) is refused:
  "VR: Arcane Dimensions in ... is not the latest release (it has no pak2.pak (patch 1): AD 1.80 or older)...";
  `vr_campaign_native ad` refuses to start it; `vr_ad_status 3`.
- **Cinematic camera** (`vr_ad_test 1/2` on `ad_tears`, camera `camera_start`): the player is moved to the camera
  (not solid, no damage, no movement) and, the sequence over (within 900 frames, on its own timers), put back where he stood,
  walking, solid and damageable again. Not breaking play; drawn as a teleport and a fixed view in VR for now.
- **Regression**: P1a's A/B script (`scratch/ab_campaigns.sh`: e1m1, start, hip1m1, r1m1, Dopa e5m1, MG1 mge1m1, MG3 map1;
  wake, fight 600 frames, kill all) with P1a's build and P1b's: no errors in either; the same monster totals
  (23, 0, 58, 46, 37 + 7 waiting, 90 + 20 waiting, 75 + 13 waiting); the spawned entities differ only by the transient
  thrown weapons, fireballs and path-following targets that two runs of the same build also differ by (a second P1b
  run of rogue and Dopa showed the same kind of differences).
- **Budget**: globals 42,390 (64.7% of 65,535; P1a 42,279), entity field slots 2,825 (P1a 2,815), statements 402,379,
  progs.dat 4.81 MB.

### 15.3 Choices made in P1b (conservative; Vittorio confirms or changes)

- **The AD campaign is not "ready"** (`nativeReady` false): playable by the developer launch and the Debug page, listed
  dimmed in Official Campaigns, single player only, until Vittorio's VR playtest accepts it (then one flag here and
  in the installer's table, with decision 3's installer default).
- **AD's upgrades are the player's** (AD's own model), not per weapon: Quake VR's weapon flags have no free bit that
  survives a level change (bits 8 and up are the grapple's tag, stripped at every level), and a new weapon id would
  touch some 25 files for each weapon. Per-weapon AD guns (their own models, sights, the Widowmaker's three barrels)
  are P3.
- **The Widowmaker reloads at the hip** (decision 23's simple reload) even with Immersive reloading's break-open super
  shotgun; its third shell comes from the reserve.
- **Vanilla items keep Quake VR's pickups** in AD maps (VR hands, holsters, magazines), with AD's STARTOFF and respawn
  keys; AD's other item keys (`no_item_offset`, FLOAT 32, NOEFFECTS 128, `item_tossvel`, coop rules) are not read.
- **The exits are Quake VR's** (our intermission, autosaves, campaign rules) with AD's keys; AD's intermission
  cameras are not used.
- **AD's stuffed commands stay filtered** (screen tints of debuffs, books and powerups; `r_skyfog`; the skybox change;
  `bf` flashes): only fog is routed (`svc_fog`).
- **The vanilla monsters' projectiles follow the Projectile Rules** (decision 22): AD's speeds by skill for the hell
  knight's spikes, the scrag's, the ogre's grenades and the zombie's flesh, AD's pellets for the grunt's shotgun (in AD
  maps by Auto). The others (enforcer lasers, vore balls, shambler lightning) have no AD skill scaling.

### 15.4 Files

Engine: `Quake/vr/vr_ad.cpp/.hpp`, `vr_ad_data.inc` (generated), `vr_pack_ad.inc`; AD support lines in `pr_edict.c`,
`gl_rmisc.c`, `vr_gamedir.cpp`, `vr_progs.cpp/.hpp`, `vr_main.cpp`, `vr_api.h`, `vr_engine.hpp`, `vr_cvars.inc`,
`vr_menu.cpp`, `vr_server.cpp`. QC: `vr_ad_defs.qc`, `vr_ad_bridge.qc`, AD support lines in `world.qc`, `client.qc`,
`combat.qc`, `weapons.qc`, `items.qc`, `subs.qc`, `monsters.qc`, `vr_reload.qc`, `vr_monsters_test.qc`, `defs.qc`
(parm57..72), and the 76 `VR_OtherFlags` gates. Importer: `Misc/quakevr/ad_import.py` (8 bridge patches, the beam
rule, `--engine-lists`). Installer: `ExpansionDetector.cs` (the row).

### 15.5 AD's shipped bugs met (decision 12: one by one, Vittorio decides)

None is fixed silently: AD's code runs as shipped. Where the bridge reimplements AD's logic it follows AD, except
where noted (bug 7).

| # | Where | What AD does | Effect in Quake VR | Proposal |
|---|---|---|---|---|
| 1 | `monsters.qc` (spider, swampling, voreling ceiling start) | `a && b \|\| c && d \|\| e && f` read by QC's priorities as `((((a && b) \|\| c) && d) \|\| e) && f` | None: the three flags are the same bit (4), so it means what AD meant | **Keep** (P1a's parenthesised patch is behaviour-neutral) |
| 2 | `monsters.qc` (gargoyle, gaunt perch) | the same pattern | None: both flags are 4 | **Keep** |
| 3 | `mon_bossmammoth.qc` (walk-frame light) | `(4..8 \|\| >= 11) && <= 15` | None: the same frames as `4..8 \|\| 11..15` | **Keep** |
| 4 | `misc_textbook.qc` (the book's screen tint reset) | `!(cshift_upd && no artifacts)` for `!cshift_upd && no artifacts` | None in Quake VR: AD's tints are stuffed `v_cshift` commands, which our engine filters | **Keep** (moot until tints are ported, then fix) |
| 5 | `client_debuff.qc` (burning's screen colour priority) | `!(bleeding && !poisoned)` for `!bleeding && !poisoned` | None in Quake VR: the colour is a stuffed command (filtered) | **Keep** (moot; fix when the debuff tints are ported, decision 28) |
| 6 | `misc.qc` (misc lightning effect) | `(L1 \|\| L2 \|\| L3) && !movetarget` for `L1 \|\| L2 \|\| (L3 && !movetarget)` | A lightning effect of type 1 or 2 with a moving target does not fire | **Keep** (AD's maps were made and tested with it) |
| 7 | `triggers.qc` `trigger_changelevel_fire` | `if (startspawn2 < 1 && startspawn2 > 7) startspawn2 = 0;` never true | A key outside 1..7 would set the spawn bits from its low 3 bits (8 spawns at the default start, 9 at spawn 1) | **Fix** (the bridge's exit (`AD_ChangelevelTouch`) already takes only 1..7, as AD meant; no AD map uses another value) |
| 8 | `items.qc` `T_Heal` | compares with `other.max_health`, not the target's | Inherited from id's `T_Heal` (ours has it too); only AD's own health pickups call AD's | **Keep** (id's behaviour everywhere) |

### 15.6 What is left

**P1b's own remainder** (small; before the AD campaign is called ready):

- Vittorio's VR playtest of the hub and a few maps through: the exits and the hub's returns (`startspawn2`), ladders,
  books, the cinematic cameras, AD's pellets and the three upgrades (their pickups need a hand: untested headless).
- The worldspawn keys `give_weapons`, `take_weapons` and `no_axestart` on Quake VR's hands and holsters (AD's parm1
  weapon bits do not map one to one on our inventory).
- AD's item keys not read on our items (15.3); AD's custom keys (`item_keyx`) and artifacts are P2 (decision 16).
- AD's screen effects sent as stuffed commands (debuff/book/powerup tints, `bf`, `r_skyfog`, `trigger_skybox`'s sky
  change): through our own messages (the skybox needs an engine message).
- The installer component (decisions 3 and 4) and a vrstart hub lectern for AD.
- Map Library packages made for AD: AD's data mounted for them (the rules' detection already works once it is).

**Its own task: the cinematic cameras' VR presentation** (decision 7): today AD's own system teleports the player to
the camera and back. Default: fade out, play the camera's view (its moves and its angle smoothing) with the head free,
fade back; option: a "window in the world" panel showing the camera, customisable (size, distance). Files:
`QC/vr_ad_bridge.qc` (hooking AD's camera controller's start, next-camera and end), the engine's view (a camera
viewpoint the head is relative to, `vr_view.cpp`), a menu page. About 2-3 agent-days.

**The follow-up tasks** of decisions 13-31: section 11. AD content outside AD maps (P2) starts by mounting AD's data
in every campaign when it is installed (its 82 shadowed files already skipped), then `AD_Rules()`-free paths for AD's
own entities (a reduced world setup on first use). (Both built for the rules' follow-ups: section 16.)

---

## 16. The rules' follow-ups (built 2026-10-11): decisions 16, 18, 19, 20, 24, 26, 28, 30

Branch `agent/adrules` (from `feature/arcane`); AD's monsters' VR rigs and hooks (decision 13) are another worker's
(`admon`) and are not touched here. Every AD-specific line is in AD's own files (`QC/vr_ad_items.qc`,
`QC/vr_ad_player.qc`, `QC/vr_ad_test.qc`, `QC/vr_spawnflags.qc`, `Quake/vr/vr_ad.cpp`); a call from shared code
carries "AD support". `ad_import.py` has 9 more reviewed bridge patches (17 in all, listed in `QC/ad/AD_IMPORT.txt`).

### 16.1 AD's data and content in every map

- **Mounted in every campaign** while the latest release is installed (`vr_gamedir.cpp`): AD's folder goes under the
  active campaign's (whose files win) and over the packs'. Outside AD's campaign it supplies only its models,
  sprites and sounds (`progs/`, `sound/`), its HUD icons (`gfx/ad*`) and its brush models (`maps/<folder>/`): never its
  maps, skies, textures, effect files or menu art (`qvr::ad::skipOutside`), and never the 82 files an official pak has.
  So `vr_ad_mounted` is on in every campaign: the AD-rules detection (2 signals) works in any campaign and for Map
  Library packages; Quake's and the packs' official maps carry none (0 of 107, section 5.1).
- **AD's own entities outside AD's rules** (decision 16; `vr_ad.cpp everywhereClasses`, `vr_ad_everywhere` 1, VR
  Settings > Combat > Arcane Dimensions: Its Content in Every Map): the seven new artifacts (`item_artifact_airtank`,
  `_blastbelt`, `_jumpboots`, `_lavashield`, `_nail_piercer`, `_sharp_shooter`, `_tomeofpower`, and Quoth's names
  `_cross`, `_trinity`), the custom pickups that override none of ours (`item_keyx`, `item_custom`, `item_healthvial`,
  `item_healthgem`) and the ladders (`trigger_ladder`) spawn as AD's entities (AD's spawn and touch) in any map: from
  the map, the toolgun's or `vr_physics_spawn`'s spawn (`box3d::spawnClass` resolves AD's names as `ED_LoadFromFile`).
  The first one of a map runs AD's reduced world setup (QC `VR_AD_WorldOnDemand`: AD's defaults for the globals its
  items, ladders and debuffs read, a few sounds; nothing of AD's world itself: its gravity, fog, particles and light
  styles stay the map's). A map key no field of ours has fills AD's `ad_<key>` in any map (their own keys:
  `respawn_time`, `exactskin`...).
- **AD levels only** (they override one of our pickups or rules): AD's backpacks (`item_backpack`,
  `item_backpack_armour`: Quake's backpack), `item_progspawn` (AD's randomiser), and everything else of AD's.

### 16.2 The artifacts (decision 16)

AD's touch sets AD's fields on our player; their effects are ours, hooked where ours act (`vr_ad_items.qc`):

| Artifact | Effect in Quake VR | Hook |
|---|---|---|
| Sharp Shooter | The shotguns' spread as AD's sharp one: 0.04 to 0.01 (shotgun), 0.14 x 0.08 to 0.04 x 0.04 (super shotgun); AD's pellets too | `FireBullets` (`AD_SharpSpread`) |
| Nail Piercer | Nails go on through bodies (monsters, players) and out the other side at their speed, a little gore; a body that resists nails stops them (AD's) | `spike_touch`, `superspike_touch` (`AD_NailPierce`) |
| Blast Belt | No damage from blasts (any `T_RadiusDamage`), armour kept, the push given (rocket jumps), its reminder; with Quad, other damage halved (AD's combination) | `T_DamageDeal` (`AD_ArtifactDamage`) |
| Lava Shield | No lava damage, and lava sets you on no fire | `WaterMove`, `VR_Burn_LavaPlayers` |
| Air Tank | No drowning, AD's breathing sounds; AD's poison halved (AD's own) | the artifacts' tick |
| Jump Boots | AD's air jumps (its count, 1-4 or endless; 360 up each), falling off a ledge gives them back, the forward push; no comfort limits (decision 27) | `PlayerJump` (`AD_JumpBoots`) |
| Tome of Power | AD's own: an event item that fires its targets | none |

Their time is ticked for our player in every map (`AD_ArtifactsTick`, AD's ClientPowerups for them: the fading
message, sound and flashes 3 s before, the end; AD's endless ones last the level). AD's touch gives the Sharp
Shooter and the Nail Piercer AD's shared Quad HUD bit, which is our Quad (tint, icon, glow): a reviewed patch
(`VR_AD_ArtifactItems`) leaves it out. Custom keys (`item_keyx`) are AD's: held as AD's bits, shown on the gadget;
outside AD maps only AD's own doors would open with them.

### 16.3 Debuffs, burning merged (decision 24)

AD's poison and bleeding are ticked for our player in every map while AD's data is there (`AD_DebuffsTick`: AD's
`ClientDeBuff`); they come from AD's monsters and traps wherever those are. AD's burning (`BurnDeBuff`: pyros, wraiths,
fire traps) lights **our** fire on the player (a reviewed patch, `VR_AD_BurnDebuff`): our flames, burns, smoke and
crackle, out in water as ours, for AD's time (3-5 s plus 5 a skill level, added to what is left, at most 10) at
AD's damage (a point a second, no armour: `AD_BurnHurt` from `VR_Burn_Think`). AD's own rules for who burns (not in
deep water, not with the pentagram or the biosuit). With Burn Damage 0 (our burning off) AD's own debuff runs. AD's
screen tints for the debuffs are stuffed commands (filtered): the wrist gadget shows the debuffs instead.

### 16.4 Ammo resistance (decision 18)

Our weapons (and our monsters' missiles) apply AD's `resist_shells/nails/rockets/cells` (0 none, 1 immune, under 0
extra damage) in `T_DamageImpl` (`AD_Resist`): in AD maps on any target with the keys (AD's monsters, ours given them
by a mapper), elsewhere on AD's own monsters only. The kind is the inflictor's: hitscan shotguns shells; nails nails;
rockets, grenades (launched, thrown, multi, proximity) and any blast rockets; lightning (`VR_LightningHit` tags it),
plasma, lasers, Mjolnir's lightning and a discharge cells; melee (fists, the axe, guns as clubs, thrown weapons, a
stuck axe's bleeding) none, as AD's axe. AD's tells (sparks, smoke, a pain sound) play in AD maps (its world's
precaches). What AD's own code deals to ours is resisted once (`vr_ad_ourdamage`). Measured (`vr_ad_test 21`, a grunt
with shells 0.5, nails 1, rockets -0.5, hit 10 by each): e1m1 13 (a positional shot), 10, 10, 10, 10; ad_e1m1 7, 0, 10
(the blow), 15, 10; a stand-in for one of AD's monsters (nails 0.5) 5 in both, one of ours 10 in e1m1, 5 in ad_e1m1.

### 16.5 Ladders on our climbing (decision 26)

`trigger_ladder` (in any map) keeps AD's own climb (holding jump climbs at the ladder's speed: `AD_PlayerPreThink`,
now in every map) and is a climbable volume for our hands (`vr_climb.cpp ladderHold`, `vr_climb_ladders` 1; VR
Settings > Combat > Arcane Dimensions: Hand-Climbed Ladders): an empty hand gripping in it, or within the touch
distance, holds where it is (a rung under every hand), after the exact ledges and before a lenient one; the hanging,
the pulls, stamina and the mantle at the top are the ledges'. AD's state turning the ladder off (not solid) lets no
hand hold. Measured on ad_e1m1's ladder (x 2495-2561, y -305..-295, z -1..161): `vr_climb_try` in it: an exact hold at
the hand, facing the body; over it: none; Hand-Climbed Ladders off: none; a mock climb (main hand gripped at 70,
pulled to 30, the off hand at 75, pulled to 30): the body 24, 62, 80 over its start.

### 16.6 The HUD (decision 28)

The wrist gadget's info row (the keys, powerups and sigils at the right of the level's lines) also shows AD's
artifacts held (AD's own icons, `gfx/adib_*`, greyed like the status bar's), the soonest one's seconds left, the
custom keys held (AD's icon for each key's HUD skin) and the debuffs (BURN, POISON, BLEED, blinking), drawn only when
there is one; with many the row shrinks to half size as before. The engine's stats `STAT_QVR_ADHUD`, `_ADKEYS`,
`_ADTIME` from QC `.vr_ad_hud*` (`AD_HudUpdate`). Debug: Gadget Screen Readings "Arcane Dimensions Row".

### 16.7 Evil (decisions 17 and 30)

AD's skill 4 is Quake VR's Nightmare (skill 3: the engine and our monsters know 0-3) with `vr_ad_evil` (VR Settings >
Combat > Arcane Dimensions: Evil Skill; AD's own skill choices, its hub's Evil included, set it through four reviewed
patches: `VR_AD_SetSkill`). In any campaign while AD's data is installed it plays AD's Evil for our player: 50 health
at most (`VR_BaseHealth`, `VR_MaxHealth`), megahealth to 100 (`VR_MegaHealthCap`), the Shadow Axe's blows
(`AD_AxeScale`); AD's code reads its skill 4 (its monsters' hardest), AD's own flag (`scratch4`) agrees. Chosen like
the skill, it is the game's: saved (`vr_ad_evil_on`), a loaded game's restored (`OnLoadGame`), a change mid-game played
at once (the health capacity). Measured (`vr_ad_test 22`, e1m1): off 100/100, megahealth 250, axe x1; on at a new map
50/50, 100, x1.75, AD's skill 4; saved, the choice cleared, loaded: on again; turned off: capacity 100.

### 16.8 Spawnflags (decision 19): one meaning per ruleset

`QC/vr_spawnflags.qc`. A spawnflag bit means what the ruleset its map (or its entity) was made for says:

- **Rulesets:** Quake's (id1, with Hipnotic's, Rogue's and Honey's own meanings on their own classnames: the bits as
  ever), Machine Games' (its campaigns) and Arcane Dimensions' (AD's rules). `VR_SF_MapRuleset()` is the map's;
  `VR_SF_Ruleset(e)` an entity's: AD's own entities (AD's code runs them) are AD's in any map, the rest the map's.
- **Shared meanings** (AMBUSH 1 on monsters, the skill and deathmatch bits the engine reads) are read as they are.
- **Quake-family meanings** (Rogue's statues, Hipnotic's trigger spawns, id's variants...) are read only in Quake's
  ruleset: the existing raw tests are gated by `VR_OtherFlags()` (76 gates, unchanged; now `VR_SF_MapRuleset() !=
  VR_SF_ID`).
- **Every other ruleset's meanings are named** (`VR_SF_MON_DELAYED`, `_MON_ANGRY`, `_MON_NOSPAWNFX`, `_STARTOFF`,
  `_ITEM_RESPAWN`, `_EXIT_RESETINV`) and read by `VR_SF_Has(e, name)` from that ruleset's table (`VR_SF_Bit`): a name a
  ruleset has no flag for is never set there. AD's flags on our monsters, items and exits are read so (the bridge's
  raw tests migrated), only in AD maps.
- **Backwards compatible:** Quake's, the packs' and MG's maps read exactly the bits they did (their tables add
  nothing; the A/B census, 16.10). **Conflicts minimised:** no shared code tests another ruleset's raw bit. A new
  ruleset (Quoth, Copper) is a table and a map rule in this file.
- **Later (not done, mechanical):** MG's own flag reads into the table; the 76 gates as named Quake-family meanings.

Measured (`vr_ad_test 26`): e1m1 ruleset 0, our monster's 64|128|32 and item's 16|64 read as nothing; mge1m1 1,
nothing; ad_e1m1 2, all read; AD's artifact's RESPAWN read as AD's in every map.

### 16.9 Crouching (decision 20)

AD's monsters are height-aware: they look for their enemy at its eyes (`origin + view_ofs`: `visible`,
`visblocked_wide`), lob at its height (Z-aware arcs) and melee only within 64 units of its height. Our crouch lowers the
player's real head and his box (the engine's crouched hull) but keeps his origin and `view_ofs` (22) as a standing
player's. So monsters' sight now looks for a crouched VR player at his real eyes: AD's (two reviewed patches,
`VR_AD_ViewOfs`) and, in every mode, ours (`ai.qc VR_SightOfs`, `vr_crouch_sight` 1, Movement > Player Hitbox >
Crouching: Monsters See Your Eyes): behind cover lower than he stands he is out of sight. Never higher than a
standing player's, and standing nothing changes. AD's aim at his origin (24 over the feet) and its melee height check
already fit the crouched box (36 high); our monsters' bullets aim at what shows of him (`VR_Crate_ShotAim`). Measured
(mock head at 0.95 m): `view_ofs` 22, the eyes 7.95 over the origin, crouched: both sights 7.95; off: 22.

### 16.10 Acceptance (headless mock, fast mode, skill 1)

- Artifacts and debuffs in e1m1 (`scratch/t_art.sh`): `vr_physics_spawn` of the Jump Boots and a health vial spawn
  (AD's entities outside AD's rules: 3 counted by `vr_ad_info`, the custom key without its model removed as AD's),
  the backpack does not (AD levels only); every artifact's effect as 16.2 (Sharp 0.01 x 0.01 and 0.04 x 0.04; Blast
  Belt 0 health and 0 armour lost, pushed 194 u/s, without it 25 lost; Lava Shield; air 30 s; boots 360 then none);
  the HUD bits 31, timer 30, our Quad bit not given; the Nail Piercer's nail through one grunt into the next (30 -> 23
  and 24); AD's burning: our fire lit for AD's 8-9 s, 8 health lost at AD's rate, AD's counters untouched; poison 2
  (the Air Tank halving 4-5), 3 lost; bleeding 64 -> 16 with 48 to come back; a custom key: its HUD bit and skin.
- Ammo resistance (`t_res.sh`), ladders (`t_ladder.sh`, `t_ladder2.sh`), Evil (`t_evil.sh`), spawnflags (`t_sf.sh`),
  crouched sight: the numbers above.
- Regression (P1b's A/B script, `scratch/ab_mine.sh`, against P1b's own run): e1m1, start, hip1m1, r1m1, Dopa e5m1,
  MG1 mge1m1, MG3 map1 with AD's data mounted in each: no errors; the same monster totals (23, 0, 58, 46, 37 + 7
  waiting, 90 + 20 waiting, 75 + 13 waiting); the spawned entities differ only by the transient thrown weapons and
  weapon records, a fireball and r1m1's two ogres' random boss flag (25% on Normal: `orig_mon_ogre.qc`), as two runs
  of one build do.

### 16.11 Choices made (conservative; Vittorio confirms or changes)

- AD's folder in every campaign supplies only models, sounds, HUD icons and brush models (16.1); its textures and
  skies never reach another campaign's maps.
- Outside AD maps AD's world is a reduced setup (16.1): AD's particles (its emitters, item sparkles) are off there.
- Backpacks and `item_progspawn` are AD levels only (they are Quake's backpack and AD's randomiser).
- The Blast Belt protects from any blast our `T_RadiusDamage` deals; the Nail Piercer works on our nails, not Rogue's
  lava nails; the Air Tank's bubbles (AD's particles) are not drawn.
- AD's resist tells only in AD maps (precaches); outside them AD's monsters resist without them.
- Evil is a toggle beside the skill (Nightmare plus Evil), not a fifth skill value: the engine and our monsters stay on
  0-3. Quake's start map has no Evil hall: the menu row chooses it.
- `vr_crouch_sight` is on by default: it changes only a crouched player's sight.
- Ladders: a hand anywhere in the ladder's volume holds (no rung positions); AD's velocity ladders keep their own push.

### 16.12 Files

Engine: `Quake/vr/vr_ad.cpp/.hpp` (`everywhereClasses`, `VR_AD_SpawnsOutside`, `skipOutside`), AD support lines in
`vr_gamedir.cpp` (the mount, `VR_SkipSearchPath`, `VR_ShouldMountCampaignDirectory`), `pr_edict.c` (spawn and keys
outside AD's rules), `vr_box3d.cpp` (`spawnClass`), `vr_climb.cpp` (`ladderHold`), `vr_gadget.cpp` (the AD row),
`vr_server.cpp`, `vr_protocol.hpp`, `vr_fields.inc`, `vr_api.h`, `vr_cvars.inc` (`vr_ad_everywhere`, `vr_ad_evil`,
`vr_climb_ladders`, `vr_crouch_sight`), `vr_menu.cpp` (the rows, Debug > Tests > Arcane Dimensions' new tests). QC:
`vr_ad_items.qc`, `vr_ad_player.qc`, `vr_ad_test.qc`, `vr_spawnflags.qc` (new); AD support lines in `vr_ad_defs.qc`,
`vr_ad_bridge.qc`, `client.qc`, `combat.qc`, `weapons.qc`, `vr_burning.qc`, `vr_ammoutil.qc`, `ai.qc`. Importer:
`ad_import.py` (9 more bridge patches).

### 16.13 TODO

- **Chaos mode** (decision 17): AD's randomiser (`chaosmode.qc`, `item_progspawn`) over our and AD's monsters and
  items, its restart dance replaced by our save hooks; after every AD monster has VR support (P2). About 3-5 agent-days.
- **Deep performance profiling of AD's maps in VR** (decision 29), with chaos mode: the biggest maps (`ad_tears`,
  `ad_sepulcher`, `ad_magna`) exclusive (`--exclusive`, `vr_profile`, `bench.sh`): lightmap build time, the think cost
  of AD's emitters, estate and breakables, sprite particles; fixes per finding. About 3-5 agent-days.
- AD's debuff and artifact screen tints (stuffed `v_cshift`/`bf`) through our own messages; the Air Tank's bubbles.
- AD's resist tells outside AD maps (its sprite precaches in the reduced world setup), once AD's monsters spawn there
  (P2).
- AD's items' activator for our entities they target outside AD maps (`AD_SyncActivator` runs in AD maps only).
- The spawnflag table's later entries (16.8): MG's flags, the 76 Quake-family gates as named meanings.
- Coop: AD's per-player state (debuffs, artifacts, Evil) in coop saves (decision 31).

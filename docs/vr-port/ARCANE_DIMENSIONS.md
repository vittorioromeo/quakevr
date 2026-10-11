# Arcane Dimensions (AD): analysis and plan for native Quake VR support

Status: Phase 0 (analysis + plan) done 2026-10-11; **Phase 1a built** (section 14): AD's QuakeC is imported,
namespaced and compiled into the progs, inert (nothing of it runs yet), on branch `agent/arcane`. AD's assets are never
committed. Target, from Vittorio's request:

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
- Saves record `vr_save_ad_rules`, so a loaded save restores the rules before spawn.

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
quakevr). AD assets whose names collide are read in place through `owned/ad/<path>` (`VR_OwnedFile`), e.g.
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
| **P1a** foundation | AD code compiled into our progs with **no behaviour change** anywhere | Engine: campaign entry 6 `ad` (discovery, hash validation, mount below id1, `VR_SkipSearchPath` rules, `owned/ad/`), `vr_ad_rules` + pre-spawn entity-lump detection + save field, `ad__` spawn-prefix hook in `ED_LoadFromFile`, parm capacity for 57..72. QC: `ad_import.py` + patches, progs.src/FGD/precedence checks green, globals/fields/edict budget measured and recorded, `AD_Rules()`/`VR_Pack_AD()`. Checker `Misc/quakevr/check_ad_entities.py` (per-map missing classnames and keys, as `check_mg3_entities.py`). | 5-7 |
| **P1b** AD maps play | Every AD map loads with 0 missing spawn functions and is playable start to end under AD rules | Bridge (5.3): damage, player hooks, parms/map variables, fog/sky/cshift, items to our pickups, vanilla-weapon placement logic, AD-key adapter for vanilla monsters, spawnflag gating, ladders (AD push first), books (centerprint first). AD monsters run on AD's AI with minimal VR registration (hulls, gibs, damage). Headless acceptance per map (load, entity census, scripted kill/secret/exit chain through the `start` hub and the rune gate), save/load. Installer component (default off). | 12-18 |
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

**Not done in P1a (P1b's list, section 11).**

- The engine side: campaign entry 6, AD data discovery, the `ad_` spawn hook, the `ad_<key>` key fallback,
  `vr_ad_rules` with detection before spawn.
- The bridge proper: damage, player, fog, items.
- Running any AD code. AD's `ad_worldspawn` setup (precaches, light-style and pendulum arrays) must run first in AD
  maps; outside them, AD entities need a reduced setup.

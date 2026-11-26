# Dwell and Copper: analysis and plan for native Quake VR support

Status: **Phase 0** (analysis, plan, probe) done 2026-11-19, branch `agent/dwell`. No feature code yet. Modelled on
`ARCANE_DIMENSIONS.md` (AD): the same architecture (campaign entry, data and rules switches, spawn-prefix hook, scripted
import plus a hand-written bridge, `owned/` paths, installer pin), extended where Dwell needs more.

Target, from Vittorio's request (2026-11-19):

1. Dwell (both episodes) in the installer, **on by default**, as a recommended optional dependency.
2. All Dwell-unique entities (weapons, monsters, powerups) available in any game mode when Dwell is installed.
3. Maps made for Dwell **or Copper** work seamlessly when Dwell is installed.
4. The OST is downloaded and played as intended.
5. Conflicting entities are renamed or adapted to coexist.
6. New monsters and weapons fit Quake VR's systems (smarter AI, stealth, ragdolls, dismemberment, ...).
7. While integrating, simplify the amalgamation: fewer special cases and bespoke branches (section 8).

Sources (read-only, never committed):

- `qvr-kit/downloads/dwell/dwellv2p2.zip`: Dwell v2.2 (2023-02-12), 384,629,995 bytes, SHA-256
  `15bbc5dc0eb3fee5763835cea1aa62ab263545ca8c04aaa019982d45d1e8d11d` (Quaddicted's hash).
- `qvr-kit/downloads/copper_src`: Lunaran's Copper QuakeC, GitHub `itslunaranyo/copper` master `379dd5d` (v1.30,
  `constants.qc:5`), GPL-2.
- Nothing else was downloaded. The decompiler is the FTEQCC already in the repo (`Tools/fteqcc/fteqcc64.exe -d`).

Analysis: `Misc/quakevr/dwell_scan.py` (committed; reusable for the next Dwell/Copper version and for P1's detection
test). Raw tables in the worktree's `scratch/dwell_an/` (not committed): `classes.txt`, `permap.txt`, `assets.txt`,
`music.txt`, `detect.txt`, `funcs.txt`, `probe.txt`. Decompile in `scratch/dwell_decomp/`. Probe: `scratch/dwell_probe.sh`,
`scratch/dwell_probe_count.py`, `scratch/dwell_mkpak.py`, `scratch/dwell_flags.py`.

---

## 1. Licence and source strategy

| Item | Terms | Consequence |
|---|---|---|
| Dwell data (maps, models, sounds, gfx, music) | **No licence text** anywhere in the release. README: install notes, story, credits. Credits include "Croteam for Serious Sam, and for the extra models, ripped by Danz", and Raven Software, Digital Extremes and Lobotomy Software under "Additional Credits". | Some Dwell models are ripped from commercial games, so nobody (not even the Dwell team) can license them to us. **Read Dwell's files in place from the player's install; never ship, convert or host them.** A re-rig or VR edit of a Dwell model is our own model, or a runtime adaptation through metadata (as for MG3 and AD). |
| Dwell `progs.dat` | Compiled 2023-02-09 by fteqcc 5630 (the decompile's banner). No source released. | A derivative of Copper, so GPL-2 covers it (Copper's licence requires derivative source). The Dwell team never published the source. |
| Copper QuakeC | GPL-2 or later, derived from id 1.06 (`LICENSE`, `README.md`). Lunaran asks extenders to "use the complete corpus of Copper gameplay tweaks" rather than cherry-pick. | **We may merge it.** Our QC is GPL-2 and public. Keep file headers, credit Lunaran in CREDITS.md, list adaptations as in OFFICIAL_QC_SOURCE.md. His request suggests taking Copper's rules as a whole set per map, not a mix: that matches "Copper rules" as one ruleset (section 2.4). |
| Names | No naming clause in either. | Say "plays Dwell", "Copper maps supported". Never brand our build as Dwell or Copper. |

Credit lines (CREDITS.md, P1): "Dwell (2023) by Fairweather (lead), Khreathor, bmFbr, Shamblernaut, VoidForce (QuakeC),
Alekswithak (music) and the Dwell team; read from the player's install." "Copper by Lunaran (GPL-2); Quake VR's Copper
support adapts its QuakeC."

### 1.1 The decompile (tried now)

`fteqcc64 -d progs.dat` decompiles Dwell completely: 74 files, **56,276 lines**, with the original **file names, function
names, field names and local names** kept (the progs was not stripped). 15 "Indentation structure corrupt" warnings (in
`initiateProgram`, `runProgramA`, `chooseRandomTask`, `target_bullethell_fire`: the bullet-hell boss).

Its quality limits:

- **Constants are renamed by value.** The decompiler picks any named constant with the same value: `self.health =
  DEMON_LEAP_HVEL`, `WriteByte(FL_SWIM, endtrack)` (FL_SWIM = 2 = MSG_ALL), `intertrack = MOVETYPE_WALK` (3). Numbers
  must be read through the values, not the names.
- **It does not recompile.** fteqcc on the decompile stops with 12 errors in the third file (`maths.qc`: lookup-table
  arrays). Fixing it is hand work of unknown size.
- Dwell forked an **older Copper** than the public master. 473 functions of Copper 1.30 are absent from Dwell (its CSQC
  and extension builtins, the fish dash, the flytrain ghost, the door lock refactor...), and Dwell adds 1,410 functions
  (section 2.2). So "Copper rules" from the public source and Dwell's own Copper differ in details.

### 1.2 Source strategy (recommendation)

| Option | For | Against |
|---|---|---|
| **(a) Rebuild Dwell's unique entities on Copper's public source**, the decompile as behaviour reference | Clean GPL lineage from public source; reviewable; our style; Copper rules for every Copper map | Hand work (section 9: about 12-18 agent-days for the 5 monsters, 3 weapons, 3 powerups, world entities); Dwell-specific tweaks to Copper's vanilla code must be found by diffing |
| (b) Import the decompile (`dwell_import.py`, like `ad_import.py`) | Fastest route to exact behaviour | Does not compile as is; constant names are wrong; drifts from Copper's public master; a GPL derivative never published by its authors: legal but impolite to ship without asking |
| (c) Ask the Dwell team (Fairweather: lead; Khreathor: QuakeC) for the source | The real code, comments, constants; their blessing | Depends on a reply |

**Recommendation: (c) now, (a) as the plan, (b) never as shipped code.** Vittorio asks Fairweather/Khreathor for the
source and their OK (the source is GPL-2 through Copper, so this is a courtesy plus convenience). Meanwhile P1 builds
Copper (public source, scripted import, like AD) and P2 rebuilds Dwell's entities on it from the decompile's behaviour.
If the source arrives, P2 imports it instead of rebuilding (same `dw_` namespace, same bridge), and the rebuilt parts
are diffed against it.

---

## 2. Inventory

Counts are over Dwell's 24 playable maps (`start`, `d1m1`-`d1m9`, `d1end`, `d2m1`-`d2m9`, `d2end`, `dwelldm2`, `dwelldm3`,
`dwelldm7`) plus 11 brush-model BSPs (`b_*`). About 38,400 entity placements in total (12,930 of them `light`). `permap.txt` has every map's list.

### 2.1 Dwell-only content (no Copper, Quake VR or AD spawn function)

| Kind | Classname | Name in game | Placed (maps) | Notes |
|---|---|---|---|---|
| Monster | `monster_kamikaze` | Kamikaze | 334 (17) | `progs/e1/mon_kami.mdl`, 40 HP; explodes on contact |
| Monster | `monster_brute` | Brute | 131 (11) | `progs/e2/mon_brute.mdl` + `h_brute.mdl`; grenades and punch (`m_brute.qc`, 1,486 lines) |
| Monster | `monster_excavator` | Construct | 99 (10) | `progs/e2/mon_excavator.mdl`, 4 gib models; charges (changelog: "Construct") |
| Boss | `monster_d1boss` | Episode 1 boss | 1 (`d1end`) | `mon_d1boss.mdl`, homing missiles, sleep/channel phases |
| Boss | `monster_d2boss` | Episode 2 boss | 1 (`d2end`) | `mon_d2boss*.mdl`, crystals, shield, bullet-hell phase (`target_bullethell` 5, `m_d2boss_hell.qc`) |
| Monster (unplaced) | `monster_boss_egypt`, `monster_morph` | | 0 | In the code, in no map. `monster_morph` **clashes with Rogue's** (`rogue_morph.qc`) |
| Weapon | `weapon_quad_shotgun` | Rotary Shotgun | 30 (18) | `v_shot3`/`g_shot3`; shares the SSG key (impulse 33) |
| Weapon | `weapon_railgun` | Crystal Lance | 21 (12) | `v_rail`/`g_rail`, `proj_crystal.mdl`; shares the LG key (impulse 38) |
| Weapon (unplaced) | `weapon_rifle` | Rifle | 0 | `v_rifle`/`g_rifle`; a cheat impulse only. **Removed from Quake VR 2026-11-23** (see "The Rifle removed") |
| Powerup | `item_artifact_haste` | Haste | 32 (11) | `progs/e2/item_haste.mdl` |
| Powerup | `item_artifact_berserk` | Berserk | 27 (15) | 50% damage reduction while active (patch 2.1) |
| Powerup | `item_artifact_wetsuit` | Wetsuit | 31 (7) | **Clashes with Hipnotic's wetsuit** (`hip_items.qc`) and AD's (`adOwnedClashes`) |
| World | `misc_crystal` 99, `func_fade` 84, `info_storycamera` 1, `trigger_killboss` 1, `target_bullethell` 5 | | | Story camera (`storycam.qc`), text books (`misc_textbook.qc`; AD has the same names: `misc_textbook`, `misc_textstory`, `info_book_displaystory`) |

Also in the code: Dwell's alternate weapon models `v_axe2`, `v_axeb`, `v_nail3`, `v_rock2` (skins/variants), the
`fly.qc` player state, and new impulses 23/28/33/38 (weapon picks), 110 (shotgun start), 111 (coop monsters in single
player).

### 2.2 What Dwell changed in Copper's code

From `funcs.txt` (decompile vs Copper 1.30 source; a function present in both may still differ in body):

| File | Functions added | |
|---|---:|---|
| new monster files (`m_brute`, `m_d1boss`, `m_boss_egypt`, `m_morph`, `m_d2boss`, `m_excavator`, `m_kamikaze`, `m_d2boss_hell`) | 1,154 | Section 2.1 |
| `misc_textbook.qc`, `storycam.qc`, `fly.qc` | 127 | Books, cameras |
| `m_hknight.qc` | 27 | **A Dwell change to a vanilla monster** (to read in P2) |
| `plats`, `utility`, `misc`, `item_powerups`, `fog`, `impulse`, `combat`, `player`, `explobox`, `w_shotguns`, `weapons`, `doors`, `item_weap_ammo` and 10 others | 102 | Haste/Berserk/Wetsuit, weapon picks, fog fixes, autosave, coop tweaks |

So the vanilla monsters in Dwell maps are Copper's (section 2.4) with a few Dwell edits (the hell knight above all).

### 2.3 Copper-only classnames (Dwell inherits them; any Copper custom map can use them)

48 spawn functions that neither Quake VR nor AD has, from Copper's `/*QUAKED` blocks and the maps' use. Placed in
Dwell (placements/maps): `ambient_sound` 627/14, `target_explosion` 350/11, `misc_particlefield` 159/10, `func_bobbing`
148/10, `target_meat_fireworks` 115/7, `target_sound` 109/11, `target_screenshake` 97/17, `func_void` 72/15,
`target_autosave` 68/23, `target_lock` 47/4, `target_state` 22/2, `trigger_fogblend` 21/8, `target_fogblend` 16/5,
`target_telefog` 13/5, `func_wall_point` 11/3, `target_secret` 11/6, `target_lightstyle` 3, `info_player_start_test` 2,
`target_items` 2, `func_flytrain`, `info_teleport_target`, `misc_particlefield_box`, `target_changelevel`,
`target_drop`, `target_kill`, `trigger_once_box` 1 each. Unplaced in Dwell but Copper's: `target_music`,
`target_heal`, `target_debris`, `target_fade`, `target_multiprint`, `target_playercount`, `func_static`,
`func_button_skill`, the `*_box` triggers, `misc_external_map`, `func_detail*`/`func_group` (compiler-only).

**Shared with AD** (an AD spawn function exists, not ours): `func_explobox` 81/7, `item_backpack` 133/17,
`trigger_timer` 22/4. Same names, different code: in a Copper map Copper's version must win (section 5.4).

**Shared with Quake VR** (the 70 vanilla classnames, `misc_model`, `light_candle`, `trigger_fog`,
`monster_ogre_marksman`, `misc_noisemaker`, `trigger_reactivate`...): Copper redefines their spawnflags and keys
(section 2.4). This is the larger conflict, and it is semantic, not by name.

### 2.4 Copper's rules vs id 1.06 (they become the "Copper ruleset", per map)

Summarised from Copper's source (file:line references kept in the review notes of this phase; the list is long, the
point is that it is one coherent set):

- **Spawnflags on shared classnames** (the most important; `dwell_flags.py` counts their use in Dwell):
  - monsters: 1 ambush (as id; flag 2 no longer ambushes), **16 triggered spawn** (hidden until used: 3,343 of 6,082
    monster placements in Dwell, 2,926 on vanilla monsters), 32 no teleflash (879), 64 do not telefrag, wait (1,071),
    128 spawn angry (2,347), 16384 alternate target; zombie 4 lying down; `monster_boss` 1 shootable; `monster_oldone`
    1 do not end the level;
  - every entity: **4096 coop only** (450 monsters, 309 items in Dwell) and **8192 not in coop** (75 monsters, 26
    items);
  - items: 4 suspended (137); doors 64 crusher, 128 start invisible; `func_wall` 1/2/4 illusionary/toggle/invisible;
    `func_train` 1/2/4; triggers 4 start disabled, 8 monsters fire it, 16 all clients; `trigger_push` 2/4/8;
    `trigger_hurt` 1/2/8; `trigger_teleport` 4/8/16/32; `trigger_relay` 1/2/4; `trigger_counter` 2/4;
    `trigger_monsterjump` 1/2/8/16/32/64; `path_corner` 16 bezier.
  - **Today, in our progs, Dwell's triggered monsters spawn at once and its coop-only monsters and items appear in single
    player.** The ruleset's spawnflag table is therefore P1's first requirement.
- **Weapons**: pellets pass through bodies shot to death (2-4 damage, 8 hops); nailgun 1996 speed; SNG two 9-damage nails;
  rockets fire straight (no autoaim); grenade direct hits use the rocket path; LG range 720, chain and side prongs,
  pushes players up; axe 24 damage, delayed hit, knocks zombies down; no auto-switch to explosives under quad.
- **Monsters**: ranges 120/500/900/1250; nightmare refire volleys instead of zeroed attack_finished; waist-deep
  lava/slime hurts; infighting gives up after about 150 frames; same-class damage halved. Grunt 35 HP and a 0.3 s aim
  lock; dog and fish 35 HP; knight 72; ogre Z-aware grenades (50%, 66% on nightmare) and 1 rocket drop; enforcer keeps
  firing, 12-damage laser; shambler 640 range; vore limited homing; zombies ignore damage under 9, are knocked down by
  25+, explode on ZKILL, come back on nightmare.
- **Items**: Ring of Shadows jitters monster aim (`enemy_vispos`) and hides the player until they fire; armour pickup
  breakpoints and kept excess; overheal to 250, rotting 1/s after 5 s; drowning damage regenerates; placed backpacks turn
  excess ammo into armour; keys stack (16 per colour) and are consumed by doors.
- **World**: fall damage 5 below -650 (not in water or `func_void`); level-change health floor; coop weapon sharing;
  `notrace 1` brushes (creatureclip) and shooting through dying monsters; `version` key removes newer-version entities;
  `impulse` 20-39 key makes an entity usable by impulse.
- **Keys read on shared classnames**: worldspawn `fog`, `fog_density`, `fog_color`, `fog_sky`, `sky`, `gravity`; monster
  spawner keys `count`, `wait`, `rand`, `netname`, `movedir`, `rmovedir`, plus `deathtype`, `target2-4`, `delay`, `alpha`,
  `armortype/armorvalue`, powerup timers, `type` (infight group); items `wait`/`count` (respawn), `message`.

Which become **Copper rules** (gated per map, like `AD_Rules()`): all of the spawnflag and key meanings (they are the
map's language), the world rules, and the monsters' and items' numbers. The weapons are the player's: **Quake VR's
weapons stay ours in every map** (as in AD), with two exceptions proposed for the ruleset: rockets without autoaim (we
have none in VR anyway) and pellets through dying bodies (a small generic option; AD's `VR_AD_PelletsPass` already
exists). Decision 1 in section 10.

---

## 3. Assets

| | Files | Bytes |
|---|---:|---:|
| Whole release (`assets.txt`) | 529 | 799,404,786 |
| `pak0.pak` (gfx, some progs/sounds) | 87 | 4.1 M |
| `progs/` | 170 (145 MDL v6 `IDPO`, 1 SPR v1) | |
| `sound/` | 161 | |
| `gfx/` (skies, menu) | 106 | |
| `maps/` (35 BSP + `.lit`) | 59 | (`d2m8` is BSP2, the others BSP29) |
| `music/` | 30 | 178,852,815 |

- **Formats**: every model is MDL v6 and the sprite SPR v1, which the engine reads. No IQM/MD3/SPR32.
- **Overlap**: Dwell replaces 50 id1 files (none identical: its own `v_*`, `g_*` weapons, `h_*` heads, items, sounds), 4
  Hipnotic, 9 Rogue, 2 MG1, 3 MG3, 1 Dopa files, 75 AD names (29 identical, 46 different) and 21 of `quakevr/` (1
  identical). 134 `progs/` and 151 `sound/` files are Dwell-unique.
- **So mount Dwell like AD**: as a campaign folder that supplies only its own files outside its campaign
  (`VR_SkipSearchPath`, `skipOutside` pattern), and its overriding files only inside it. Dwell's code needs its own
  `v_shot`, `v_nail`, `v_rock`, `v_light`, `v_axe` only for its own HUD view models, which VR never draws: our weapons
  stay ours. Its colliding models it needs (e.g. `progs/h_zombie.mdl`, `progs/lavaball.mdl`, `misc_player.mdl`) are
  read through `owned/dwell/<path>` when a Dwell entity asks for them.

---

## 4. Music

- **Each map plays the CD track in worldspawn `sounds`**, as id's maps do, but numbered 69-254 so Dwell's music never
  collides with Quake's tracks: `start` 254, `d1m1`-`d1m9` 101, 102, 103, 104, 105, 192, 107, 108, 109, `d1end` 69, `d2m1`
  201, `d2m2` 112, `d2m3` 113, `d2m4` 115, `d2m5` 225, `d2m6` 222, `d2m7` 207, `d2m8` 208, `d2m9` 249, `d2end` 229,
  `dwelldm2` 107, `dwelldm3` 102, `dwelldm7` 113.
- **Intermission and end tracks**: Dwell's `t_level.qc` sends the intermission track from worldspawn `track_intermission`
  (one map sets 4) and the end track from `track_end`, with worldtype-dependent defaults (2, 3 or 5: the decompile's
  constant names are guessed by value, to confirm in P1). Copper's `target_music` (switch tracks on trigger, saved in
  `cdtrack`) is not in Dwell's progs and no Dwell map places it; Copper maps may.
- **Formats**: 13 OGG Vorbis and 17 MP3. Our build compiles `USE_CODEC_VORBIS` and `USE_CODEC_MP3` (and Opus, FLAC,
  WAV, mods); `bgmusic.c` finds `music/trackNN.<ext>` for any byte-sized track number. **Both play today.**
- **Duplicates**: 27 distinct contents for 30 files: `track03.mp3` = `track110.mp3`, `track113.mp3` = `track66.mp3`,
  `track255.mp3` = `track69.mp3` (18 MB). Kept as they are (the install is unmodified).
- **Case**: `Track107.ogg` has a capital T. Windows finds it; a case-sensitive file system (Linux, the Quest) would not.
  The installer (or the engine's music lookup) lower-cases it: decision for P1 (proposed: the installer renames it on
  install, recorded in install.json; a pure rename, not a modification).
- **"As intended"**: the OST comes inside the release zip (179 MB of the 799 MB), so installing Dwell installs its
  music; the campaign's music folder is on the search path while a Dwell map plays and plays by the maps' own tracks.
  Copper custom maps from the Map Library play their packages' music the same way. Nothing else to do but the case fix
  and an acceptance test (`music` command output per map in the probe).

---

## 5. Detection

### 5.1 Two switches, as for AD, but one ruleset

| Switch | Set by | Read by QC as | Effect |
|---|---|---|---|
| **Dwell data** | The engine finds a valid `dwell` folder (the installer's, or a `dwell`/`dwellv2p2` folder in an owned Quake root, validated by `progs.dat`'s or `pak0.pak`'s hash) | `VR_Content(VR_CONTENT_DWELL)` (section 8, S4; today's pattern would be `VR_Pack_Dwell()`) | Dwell-only entities spawn in any map (toolgun, custom maps, other campaigns), under the default ruleset |
| **Copper rules** (per map) | The engine, before spawn: the Dwell campaign; a Map Library package made for Copper or Dwell (Quaddicted's install folder `copper`/`dwell`, `vr_mapinstall.cpp:2136` already names it); or the entity lump's signals (5.2) | `VR_Ruleset() == VR_RS_COPPER` | Copper's spawnflags, keys, world logic and numbers on shared classnames; Dwell's edits to them when the map is Dwell's |

Dwell is a campaign (entry 7 in `vr_gamedir.cpp`'s table: `{"dwell", "Dwell", "start", 7, nativeReady=false, ...}`)
for the same reasons as AD: isolated `start`, its `progs.dat`/`quake.rc`/menu art never used, saves record the
campaign, the Main Menu and Map Library entries.

### 5.2 Signals and the false-positive check (`detect.txt`)

- **Copper signals**: the 48 Copper-only classnames minus the compiler-only ones (`func_detail*`, `func_group`,
  `misc_external_map`), plus the worldspawn `_tb_mod` key naming `copper` or `dwell*` (TrenchBroom's game configuration:
  17 of Dwell's 24 maps carry `dwell*` in some spelling, 2 `copper`). Threshold 2, as AD's.
- **Dwell signals**: the Dwell-only classnames of 2.1 (one is enough once Copper's are 2).
- **Dwell's maps**: all 24 have 2 or more Copper signals (the thinnest: `d1m2`, `d2m9`, `dwelldm3`, `dwelldm7` with
  exactly 2), 20 have Dwell signals (`d1m1`, `d1m2`, `d1m7`, `d1m9` have none: the campaign recognises them).
- **False positives: none.** 0 of 205 official maps (id1 51, re-release id1 55, Hipnotic 18, Rogue 23, MG1 25, MG3 20,
  Dopa 13) and 0 of AD's 38 maps have a Dwell signal or 2 Copper signals.
- **Installed Map Library packages** (Steam folder's `qvr_addons`, 21 packages, 50 maps): one hit, `ancient`
  ("Ancient (v1.1)"), with 4 Copper signals. Quaddicted installs it into `{base}/copper/`: a **true positive**.
- **Quaddicted's index** (the cached `maps_index.txt`): 30 packages install into `copper/` and 2 into `dwell/`; 31 more
  mention Copper in their description (most "tested with Copper, not required": vanilla maps, which keep id rules unless
  their signals say otherwise).
- **Cross-detection risk with AD**: AD's detector (`vr_ad.cpp` `countSignals`) counts any classname that has an `ad_`
  function and no Quake VR one. Copper's `func_explobox`, `item_backpack` and `trigger_timer` qualify. No Dwell map has
  two of them (the maximum is 1), but **a Copper custom map with two would switch AD's rules on today.** The generic
  detector (section 8, S1) scores the rulesets together and removes names two rulesets share from both signature lists.

---

## 6. Engine needs: the probe (2026-11-19)

The current build (`origin/next` 0247871ec plus a one-line build fix) ran Dwell's 24 maps headless (mock headset, fast
mode, skill 1), 300 game frames each, then a verdict line (`scratch/dwell_probe.sh`; the run ended on
`dwell_probe: done`, not on its timeout).

- **Mounting**: `-game dwell` alone is not enough. The campaign switch at `map` rebuilds the game folders
  (`"game" changed to "id1;hipnotic;rogue;ad;quakevr"`) and drops an unknown folder: every Dwell map was "Couldn't spawn
  server". **A campaign entry is required** (P1). The probe instead packed Dwell's files (no progs, no quake.rc, no
  music) into the agent's `id1/pak2.pak` (`dwell_mkpak.py`), moved aside afterwards.
- **No crash; 23 of 24 maps load and run.** BSP2 (`d2m8`), `.lit`, skies (`sky` key), fog, alpha brushes load.
  `d2m8` spawns 8,119 entities: within `max_edicts` 16384.
- **`d2end` stops with `Host_Error: ED_ParseEdict: parse error`**: an entity sets `"think" "bmodel_vanish"`, a Dwell QC
  function our progs lacks. Any map naming an unknown function in a function-typed key kills the server today. Engine
  fix (P1, generic): warn and skip such a key. Dwell's import provides `bmodel_vanish` too.
- **2,594 placements of 42 classnames have no spawn function** (all Copper-only or Dwell-only; `probe.txt`, per map).
  The largest: `ambient_sound` 627, `target_explosion` 331, `monster_kamikaze` 215, `misc_particlefield` 158,
  `func_bobbing` 148, `target_meat_fireworks` 115, `target_sound` 109, `misc_crystal` 99, `target_screenshake` 96,
  `monster_brute` 91, `func_fade` 82, `monster_excavator` 79, `item_backpack` 69, `target_autosave` 68.
- 5 maps print `Mod_LoadTexinfo: N texture(s) missing` and `d1m9` one `bad numedges` warning: map-side, harmless,
  noted for P1's acceptance (same in Ironwail presumably; to confirm).
- Silent today: Copper's spawnflags on shared classnames are read with id's meanings (2.4): triggered monsters spawn at
  once, coop-only content appears. Nothing prints; the census in P1 measures it.
- Engine pieces for P1: campaign entry 7 (discovery, hash validation, `VR_SkipSearchPath` rules, `owned/dwell/`), the
  ruleset detector and spawn-prefix table (section 8, S1/S2, generalising AD's), the spawn filter for coop bits (S3), the
  `ED_ParseEdict` leniency, and the fog stuffcmd path (Copper sends `fog` through `stuffcmd`, as AD does: the MG fog
  bridge takes it, section 8, S5).

---

## 7. Installer

- **Component** `dwell` ("Dwell, free, by Fairweather and the Dwell team: two episodes, 24 maps, music included").
  **On by default, recommended**, as Vittorio asked. Proposed timing: the component lands with P1b (Dwell's maps play),
  on by default from that build; before that it would download 385 MB of maps that are missing their entities.
- **Pinned file** `dwellv2p2.zip`, **384,629,995 bytes**, SHA-256
  `15bbc5dc0eb3fee5763835cea1aa62ab263545ca8c04aaa019982d45d1e8d11d`. In `Misc/release/support_assets.json` and
  `BuiltInComponents`, overridable by latest.json's `components.dwell`, as AD's.
- **URLs in order**:
  1. `https://www.quaddicted.com/files/by-sha256/15/15bbc5dc0eb3fee5763835cea1aa62ab263545ca8c04aaa019982d45d1e8d11d/dwellv2p2.zip`
  2. `https://www.quaddicted.com/filebase/dwellv2p2.zip`
  3. Slipseer `resources/dwell.21/download` (check it serves the same hash before pinning)
  (ModDB is an HTML landing page, not a direct link.) Never hosted by us: the ripped third-party models (section 1).
- **Install** into `<Quake VR base>/dwell/`, unmodified (the zip's `dwellv2p2/` folder renamed `dwell`, the READMEs kept
  beside it). Quaddicted's own packages already use `{base}/dwell/`. Music included; `Track107.ogg` renamed to lower case
  (section 4).
- **Size shown first**: 385 MB download, 800 MB on disk (the installer's units show 367 MB and 762 MB).
- **Landed 2026-11-19 (branch `agent/dwellinstall`)**; **on by default since P1b** (branch `agent/copper1b`): `Dwell.OnByDefault` (Installer Core, `Packaging/Dwell.cs`) is `true`. Built on the shared third-party path (`ModComponent`, the one AD uses). `Track107.ogg` is renamed to lower case on install (not copied: no duplicate, one name in install.json). Slipseer is not added (hash not checked).
- The README placement: inside `dwell/`, so the install root stays clean and the uninstall removes them with the folder.
- **"I already have it"**: a `dwell` or `dwellv2p2` folder in an owned Quake root, validated by `progs.dat`'s hash (v2.2),
  used in place. Older Dwell versions reported, not used. The engine's `vr_campaign_status` reports it.

---

## 8. Simplification audit of the amalgamation (plan only; nothing refactored yet)

Measured on `origin/next` 0247871ec (`QC/*.qc`, 241 files, excluding `QC/ad/`):

| Gate | Calls | What it answers |
|---|---:|---|
| `VR_OtherFlags()` | 83 | "the map's spawnflags are not id's" (MG or AD) |
| `AD_Rules()` | 47 (19 in `vr_ad_bridge.qc`) | AD's ruleset on this map |
| `MG_WorldCampaign()` | 42 | MG's ruleset (campaign ids 3-5) |
| `MG_MonsterInhibited()` | 35 | MG's coop/not-coop bits 32768/131072 + MG3's rune rule, called in each monster's spawn |
| `MG_WorldGuard()` | 17 | remove an MG entity outside MG's campaigns |
| `AD_Run(e)` | 14 | AD's code owns this entity |
| `VR_Pack_RequireSpawn()` | 65 | remove an entity whose pack is missing |
| `VR_Pack_WeaponAvailable/AmmoAvailable/ItemAvailable/KeepBit` | 27/10/3/13 | content availability |
| `cvarh_vr_ad_mounted` / per-pack `vr_*_status` handles | 30 / 6 cvars | content presence, one cvar per pack |

Three different mechanisms answer one question ("which ruleset does this map speak?"): the campaign id range for MG,
`vr_ad_rules` with its own signal detector for AD, and `VR_SF_MapRuleset()` built on both. Adding Copper the same way
would be a fourth cvar, a second detector, a third `*_Rules()` and new gates in the same places. Instead:

| # | Proposal | Files | Lines (est.) | Risk | Proof of no behaviour change | Before Dwell? |
|---|---|---|---|---|---|---|
| **S1** | **One ruleset id.** The engine computes `vr_ruleset` (ID, MG, AD, COPPER) once before spawn from a table: campaign → ruleset, Map Library package's `madeFor` → ruleset, else entity-lump signals scored for every ruleset together (AD's `countSignals`, `notSignals`, `worldSignals` become rows; names two rulesets share are nobody's signal). QC reads `VR_Ruleset()`; `AD_Rules()`, `MG_WorldCampaign()` and `VR_SF_MapRuleset()` become one-line wrappers first, then call sites move over file by file. | new `Quake/vr/vr_rules.cpp` (+150), `vr_ad.cpp` (-60), `vr_gamedir.cpp` (publish), `QC/vr_spawnflags.qc`, `vr_ad_defs.qc`, `vr_mg_defs.qc` (wrappers) | +170 / -80, later -40 more as call sites simplify | Low with wrappers (no call site changes in step 1) | The A/B spawn census (`ab_campaigns.sh`) over every campaign and AD's 38 maps: identical dumps; `dwell_scan.py`'s detection table over official/AD/package maps: same verdicts; `ad_world_test.sh`, `mg3_*` tests | **Yes** |
| **S2** | **One spawn-prefix table.** `ED_SpawnFunction`'s AD branch becomes a loop over the active ruleset's row: `{prefix "ad_", owns(classname), spawnsOutside(classname)}`; AD's `adOwnedClashes`, `everywhereClasses` and `VR_AD_OwnsClassname/SpawnsOutside` become that row's data. Copper adds `{"cu_", ...}`, Dwell's entities `{"dw_", ...}` (spawn anywhere while the data is there). | `Quake/pr_edict.c` (`ED_SpawnFunction`, -10/+15), `vr_ad.cpp` (-40), `vr_rules.cpp` (+60) | +75 / -50 | Low | Census (which function spawned each entity: `VR_OnEntitySpawned` counters already exist), `ad_hub_test.sh`, toolgun spawn lists unchanged | **Yes** |
| **S3** | **Spawnflags: one table, one spawn filter.** Add Copper's column to `VR_SF_Bit` (its monster 16/32/128 are the same named meanings as AD's 64/32/128: `MON_DELAYED`, `MON_NOSPAWNFX`, `MON_ANGRY`; new names `MON_NOTELEFRAG`, `ITEM_SUSPENDED`, `COOP_ONLY`, `NOT_COOP`). Move the coop bits out of `MG_MonsterInhibited` (MG 32768/131072) and Copper's 4096/8192 into one filter where the engine already drops skill and deathmatch bits (`ED_LoadFromFile`), reading the ruleset's bits. | `QC/vr_spawnflags.qc` (+30), `QC/vr_mg_activation.qc` (-6), `Quake/pr_edict.c` (+20) | +50 / -6 | Medium (spawn path) | Census on MG1/MG3/Dopa in single player and coop (same entities kept), `mg3_kill_test.sh`; Copper: Dwell's 759 coop-only placements absent in single player | **Yes** (Copper needs it on day one) |
| **S4** | **One content-presence table.** Replace the per-pack status cvars and `vr_ad_mounted` with one published bitmask (`vr_content`) and `VR_Content(id)`; `VR_Pack_Hipnotic/Rogue`, AD's mounted check and Dwell's become rows; `VR_Pack_RequireSpawn(available, pack)` keeps its shape. | `QC/vr_packutil.qc` (-15/+15), `QC/vr_ad_*.qc` (30 reads, mechanical), `vr_gamedir.cpp` (+20) | +35 / -25 | Low | Census with each pack removed in turn (the existing pack-absent tests), save/load across installs (`vr_save_packmask`) | Recommended (else Dwell adds two cvars) |
| **S5** | **One fog path.** MG's `svc_fog` bridge (`vr_mg_fog.qc`, 183 lines) already serves AD's stuffcmd fog. Rename it to a neutral `vr_fog.qc` API (`VR_Fog_Set`, `VR_Fog_Blend`) and route Copper's `fog` stuffcmds and `trigger_fogblend`/`target_fogblend` through it. | `QC/vr_mg_fog.qc` → `vr_fog.qc` (rename, +20), the AD bridge's 10 fog lines | +40 / -10 | Low | MG3 fog tests and AD's fog maps (`ad_world_test.sh`) unchanged; Dwell's fog blends in P1b | During P1 |
| **S6** | **Pickups: one VR pickup path with per-ruleset numbers.** Our item touch functions (29 across `items`, `hip_items`, `rogue_*`, `vr_ad_items`, `vr_mg*`) stay the only pickup code; Copper's differences (armour breakpoints and kept excess, overheal 250 and rot, key stacks, placed backpacks to armour, item respawn by `wait`/`count`) become a small numbers table selected by the ruleset, not new branches in each touch. AD's custom pickups already route to ours (decision 16). | `QC/items.qc`, `QC/vr_rules_items.qc` (new, +80) | +80 / -0 now; later AD's item branches fold in (-30) | Medium (pickup feel) | Item census and pickup tests (`dead_gear_test.sh`, the AD item tests), unchanged numbers under ID/MG/AD | During P1b |
| S7 | **Monster VR registration by archetype.** AD's (`vr_ad_monsters.qc`), MG3's (Phase D) and our own registrations (hulls, hit zones, ragdoll seeds, gib sets, get-up frames, parry) share one row format; Dwell's Brute/Construct/Kamikaze and the bosses become rows. To measure in P2's first task (the registrations live in the engine's per-model tables and three QC files). | `QC/vr_ad_monsters.qc`, `Quake/vr/vr_ragdoll.cpp`, `vr_hitmodel.cpp` | to measure | Medium | Bestiary tests (`ad_monsters_test.sh`, `vr_mg3_bestiary_test.qc`) | No (P2) |

**Order:** S1 → S2 → S3 land first (one branch each, each proven by the census before the next); S4 with them if cheap;
then Copper's import (P1) uses them; S5 and S6 inside P1; S7 opens P2. After S1-S3, adding a ruleset is: a table row
in `vr_rules.cpp`, a spawnflag column, an import with a prefix, and gates only where the ruleset's behaviour truly
differs.

Also worth a line (not proposals): `MG_WorldGuard` (17) is S2's "owned by ruleset" expressed in QC; once S2 exists, MG's
own classnames can be a row too and the 17 guards go. `VR_OtherFlags` (83) stays as is: it is already the single
"id-family meanings off" gate and S1 only changes what it reads.

### 8.1 Built: P0.5 (2026-11-19, branch `agent/rulesets`, one commit a step)

- **S1, one ruleset id** (built). `Quake/vr/vr_rules.cpp`: a row a ruleset (`ID`, `MG`, `AD`, `COPPER`: name, import
  prefix, detects, threshold, data-mounted check, AD's former `notSignals`/`notSignalPrefixes`/`worldSignals`), a
  campaign → ruleset table, and one detector scoring every detecting row together (a classname two rows both have is
  nobody's signal; a tie at the top is neither). Published as `vr_ruleset` before spawn; `vr_ad_rules` stays as AD's
  row of it (AD's code, tests). QC: `VR_Ruleset()` and `VR_RULES_*` (vr_packutil.qc); `AD_Rules()`,
  `MG_WorldCampaign()` are one-line wrappers; `VR_SF_MapRuleset()` and `VR_SF_ID/MG/AD` are gone (`VR_Ruleset()`,
  `VR_RULES_*`). `vr_ruleset_info` prints the verdict and every row's signals. COPPER's row: prefix `cu_`, detects
  nothing. **Deviation:** the campaign's own ruleset decides first (DWELL's order), where `VR_SF_MapRuleset` let AD's
  signals win in an MG campaign; no MG/Dopa map has an AD signal (58 maps, 0 signals each), so nothing changes. The Map
  Library's `madeFor` → ruleset is not wired (it would change which AD packages get AD's rules: P1b, with Copper's
  folders). Call sites were not moved: the wrappers already make them read the one id, and renaming 89 calls is churn.
- **S2, one spawn-prefix table** (built). `VR_Rules_SpawnName(classname, ours)` (vr_rules.cpp) is `ED_SpawnFunction`'s
  and the toolgun's `spawnClass`'s one answer: under an imported ruleset its `<prefix><classname>` where the row plays it
  (the kept-ours rule, AD's decision 1 and Copper's 11.1, is generic; `ownedClashes` is the row's data), else Quake
  VR's, else a row's `spawnsOutside` (AD's `VR_AD_SpawnsOutside`, `everywhereClasses`, stays AD's: its cvars and its
  on-demand world setup). `VR_AD_OwnsClassname` is gone; its counters are the rules'. **Skipped:** `MG_WorldGuard` as a
  row: the guards remove MG's entities from QC silently, as the entity spawns; an engine row would refuse the spawn
  function instead ("no spawn function" printed, the toolgun's lists changed). Not a mechanical move.
- **S3, one spawnflag table, one spawn filter** (built, in QC). `VR_SF_Bit` gains MG's column (`VR_SF_COOP_ONLY` 32768,
  `VR_SF_NOT_COOP` 131072) and Copper's (16/128/32 as AD's delayed/angry/no-spawn-effect, 64 `VR_SF_MON_NOTELEFRAG`,
  4096/8192 coop), inert until COPPER is detected. `VR_SF_SpawnInhibited()` (vr_spawnflags.qc) replaces
  `MG_MonsterInhibited()` at its 34 call sites: the map ruleset's coop bits and MG3's rune rule. **Deviation:** the filter
  stays in QC, called where it was, not moved into `ED_LoadFromFile`: there it would run before the spawn function's
  precaches (items call it from `StartItem`, after `precache_model`) and on entities whose spawn function never asked,
  both changes the census would show; Copper's own `SUB_ShouldSpawn` is a per-spawn-function call too, so P1b calls the
  same filter from Copper's spawns.
- **S4, one content-presence table: skipped.** The per-pack `vr_*_status` cvars carry four states (missing, valid,
  incomplete, another version) that the menus, the installer's readiness labels and tests read, so a `vr_content`
  bitmask would be added beside them, not replace them; the 30 `vr_ad_mounted` reads would only be renamed. Better done
  in P1a with Dwell's data switch, when there is a second row to justify it.

---

## 9. Phased plan (agent-days of qvr-agent work; Vittorio's playtests extra)

| Phase | Goal | Tasks | Effort |
|---|---|---|---|
| **P0** (this) | Analysis, plan, probe | This document, `dwell_scan.py` | done |
| **P0.5** simplification | S1, S2, S3 (and S4) with no behaviour change | Section 8; census and detection proofs | 4-6 |
| **P1a** Copper inert | Copper's QC compiled in, namespaced `cu_`, no behaviour change anywhere | `copper_import.py` (from `ad_import.py`: rename to `cu_`, drop CSQC/extension headers/`defs.qc`, keep mapper keys' names), patches for the bridge, progs/globals/fields budget measured, FGD group "Copper", `check_copper_entities.py` | 3-5 |
| **P1b** Copper rules and Dwell's maps play | Every Dwell map loads with 0 missing spawn functions (Copper's part) under Copper rules; Copper custom maps play | Campaign entry 7, ruleset row, spawn filter, `ED_ParseEdict` leniency, `vr_copper_bridge.qc` (damage to our `T_Damage`, our pickups with Copper numbers, our vanilla monsters with Copper's keys/flags/numbers through an adapter, as AD's), fog, autosave (`target_autosave` → our save hooks), music check, installer component (on by default from this build). Acceptance per map: load, census, a scripted exit chain on `start` → `d1m1`. | 6-10 |
| **P2** Dwell's entities | Dwell's 5 monsters, 3 weapons, 3 powerups and world entities, everywhere when the data is there | Rebuilt on Copper (`dw_` files) from the decompile, or imported if the team sends the source; Dwell's edits to the hell knight and others; each monster with our VR treatment by archetype (hit zones, dismemberment, ragdoll rigs, get-ups, stealth senses, smarter AI hooks) in usage order: Kamikaze (334), Brute (131), Construct (99), then the bosses | 12-18 |
| **P3** weapons in VR | Rotary Shotgun, Crystal Lance (and the Rifle) as VR weapons; Haste, Berserk, Wetsuit on our player | Weapon variants or new WIDs, grips/sights metadata read from Dwell's models in place, holsters, ammo, HUD/wrist indicators; the wetsuit clash resolved (section 10) | 6-10 |
| **P4** polish | Feels native | Our own VR models where Dwell's view models don't suit VR, the story camera in VR (AD's camera rules), books (AD's panel), the bullet-hell boss's readability in VR, performance on `d2m8` (8,119 entities), coop | 6-12 |

Total: about 37-61 agent-days. P0.5 is the gate: it proves the generic mechanism on AD and MG before Copper uses it.

### 9.1 Built: P1a, Copper's QuakeC in the progs, inert (2026-11-19, branch `agent/copper`)

- **The census as a tool**: `Misc/quakevr/spawn_census.sh <label> [coop]` and `spawn_census.py compare A B [prefix*]`
  (TESTING.md "The spawn census"): 168 maps of every campaign and AD, saved 3 frames after spawn, compared global by
  global and field by field. Two runs of one build are identical (the rulesets' S3 census and today's base: 168/168).
- **One importer**: `Misc/quakevr/qc_import.py` holds the import (tokenizer, renaming, header filtering, builtins,
  progs.src block, `--check`); a mod is a `Mod` config: prefix, folder, skipped files, headers, kept map keys,
  preprocessor symbols, entry points dropped or not. `ad_import.py` is AD's config (its pin, its patches, its report
  lines) and writes the same `QC/ad` as before; `copper_import.py` is Copper's. An import sees Quake VR's QC and the
  imports before it in progs.src, never its own or a later one (so AD's output cannot depend on Copper's). Re-running
  AD's import first said 17 files differ: hand edits made in `QC/ad` since the 2026-10-16 cleanup; they are AD
  patches now, and `ad_import.py --check` says 0 of 142 files differ.
- **Copper's import** (`QC/copper/`, 61 files, 35,475 lines, `COPPER_IMPORT.txt`): master `379dd5d` pinned by one
  SHA-256 over its progs.src and `.qc` files. Every name `cu_<name>`; `defs.qc` is a header (its system section and
  builtin declarations dropped, the rest filtered to what the code uses: 116 of 298 statements); CSQC left out
  (`csprogs.src` is not imported and `NO_CSQC` is taken as defined; its 3 stat fields kept, Copper's own NO_CSQC build
  needs them); `test.qc` skipped; the engine's entry points' definitions dropped (StartFrame, PlayerPreThink,
  PlayerPostThink, ClientConnect, ClientDisconnect, ClientKill, main), the ones its other code calls kept renamed
  (PutClientInServer, SetNewParms, SetChangeParms). All 67 builtins it uses are Quake VR's (65 by name, `centerprint2`
  and `checkextension` by number): no new engine builtin. Map keys: its /*QUAKED keys and the keys Dwell's 24 maps set
  (170); 29 that Quake VR has with the same type stay bare (`wait`, `delay`, `speed`, `lip`, `dmg`...); 20 others
  are `cu_<key>` for P1b's engine fill (as AD's `ad_<key>`). A field alias (`.float t_length = length;`) is never
  shared (new generic rule; AD has none). `fx.qc`'s `#define`s name Copper's fields: renamed with them.
- **Reviewed patches** (`PATCHES` in the script): 17 precedence patches parenthesised as fteqcc reads them; two of them
  are "Copper as shipped" look-alike bugs for P1b to decide with AD's: `WaterMove`'s gasp test (`a || b && c` is
  `(a || b) && c`) and `t_void.qc`'s `noTint = a && b && !coop` (QC: `noTint = a`). Mechanical: the CSQC stat fields
  out of the `#ifndef`, and `T_RadiusDamage`'s default damage type passed explicitly at its two short calls (fteqcc
  leaves it undefined, Q105). Warnings F210 (models precached through Copper's helpers) and F323 (one loop's tail) are
  off from `cu_builtins.qc` on: Copper's files are the last in progs.src.
- **Inert, proven**: the census over all 168 maps is identical (`compare scratch/census_base scratch/census_copper
  "cu_*"`: 168 identical, 0 different). The only new names in the saves are Copper's globals and its field aliases
  of shared fields (`cu_include`/`cu_model2` = `wad`, `cu_exclude`/`cu_model3` = `map`, `cu_rand` = `healamount`...),
  the same storage under a second name.
  Copper's spawn functions are reached only through the COPPER row (`VR_Rules_SpawnName`; it detects nothing) and the
  toolgun's explicit `cu_` names: the toolgun's prefixed variants are now one rule for every row
  (`VR_Rules_ToolgunPrefixed`, vr_rules.cpp; AD's `toolgunOutside` is its row's data), so `cu_<classname>` spawns only
  under Copper's rules. The AD and MG tests and the scenario check pass as before (ROUND25).
- **Budget** (the dev build; the fast fteqcc):

  | | Before | With Copper | Change |
  |---|---:|---:|---|
  | globals | 76,360 | 79,737 | +3,377 (version 7 progs: no 65,535 limit) |
  | entity fields (slots) | 4,015 | 4,089 | +74: +296 B an edict (16,060 → 16,356 B); `max_edicts` 16,384 reserves +4.8 MB |
  | statements | 707,180 | 752,194 | +45,014 (+6.4%) |
  | functions | 21,190 | 23,935 | +2,745 |
  | strings | 1,120,800 B | 1,175,796 B | +55 KB |
  | progs.dat | 13.99 MB | 14.91 MB | +0.92 MB |
  | compile (fteqcc, 3 runs) | 3.36-3.74 s | 3.68-3.73 s | about +0.3 s |
  | a save (e1m1, census) | 518,650 B | 520,012 B | +0.26%: Copper's globals |

- **FGD**: `fgdgen.py` reads Copper's `/*cu:QUAKED` comments: the 45 classnames only Copper has get entries in a new
  group "Copper" (described as Copper's, spawning under its rules); no existing entry changed (`--check` green, 432
  entities).
- **`check_copper_entities.py`** (Dwell's 24 maps, as `check_mg3_entities.py`): 133 classnames, 90 Quake VR's, 29
  Copper's, 14 missing: exactly Dwell's own (2.1: `monster_kamikaze` 334, `monster_brute` 131, `misc_crystal` 99,
  `monster_excavator` 99, `func_fade` 84...; 866 placements). 12 unknown keys: light-compiler keys and Dwell's own
  (`corpsetime`, `locksounds`, `skin_override`, `track_intermission`, `wateralpha`).
- **S4, one content-presence answer: skipped again.** `VR_Content(id)` would replace `VR_Pack_Hipnotic()`,
  `VR_Pack_Rogue()` (one line each) and 30 `cvarh_vr_ad_mounted` reads by one function and an id table: a rename, no
  code removed, while the four-state `vr_*_status` cvars must stay for the menus, the installer and tests. Dwell's data
  switch is not built yet (its discovery and pin are P1b's engine work), so there is still no new row to share it.
  Do it with Dwell's switch in P1b, if that switch would otherwise add a second QC reader.
- **Left for P1b**: the engine's `cu_<key>` fill in Copper maps; Copper's world setup (its `worldspawn`, light
  styles) and what its dropped `StartFrame` did; the precedence look-alikes above; the COPPER row's signals and data.

### 9.2 Built: P1b, part one (2026-11-19, branch `agent/copper1b`; not finished, see "Left")

- **Campaign 7 `dwell`** (`vr_gamedir.cpp`, `Quake/vr/vr_dwell.cpp`): found as `<root>/dwell` (the installer's) or
  `dwellv2p2` (the zip's) in any owned root; v2.2 only, by `progs.dat`'s size and SHA-256 (another version: status 3,
  said once); `vr_dwell_status`. Its folder never supplies its progs, `quake.rc`, `default.cfg` or the 64 files an
  official pak has (`vr_dwell_data.inc`, written by `Misc/quakevr/dwell_data.py`): our VR models, heads and sounds
  stay Quake's. Mounted only while it is the campaign (its content in other maps is P2). Like AD: a start map of its own
  only, experimental (Select Campaign's notice, Run Setup, `vr_campaign_native dwell`), saves record campaign 7.
  **Fewer special cases:** a campaign row now carries its own checks (alias folder, version check, `skipInFolder`,
  experimental), replacing AD's index tests in discovery, `VR_SkipSearchPath`, the status texts and the menu
  (`VR_CampaignExperimental`); Arena Mode's row follows the last campaign.
- **COPPER detected** (`vr_rules.cpp`): its own `cu_` classnames (none of ours, not `func_group`, `misc_external_map`,
  `func_detail*`), Dwell's 16 own classnames (no spawn function until P2), and the worldspawn `_tb_mod` naming copper or
  dwell; threshold 2; data needs none (Copper's code is in the progs). The Dwell campaign plays by it, and a Map
  Library package made for Copper or Dwell (`madeFor`, the active package's own map) does too (`vr_ruleset_info`
  says "the package's mod"); its detail says "plays natively by Copper's rules (experimental)". AD/Copper shared names
  (`func_explobox`, `item_backpack`, `trigger_timer`) are nobody's signal now that both rows detect.
  `misc_explobox(2)` stays ours under Copper (a row's `keptOurs`): Copper's brush box died under our prop physics
  (`SOLID_BSP without MOVETYPE_PUSH`).
- **Map keys** (`pr_edict.c`): AD's `ad_<key>` fill is generic (`ED_FindRulesetKeyField`, `VR_Rules_KeyPrefix`):
  Copper's `cu_<key>` (`include`, `target2`...) in Copper maps (fixed `target_state`'s 14 "no include" errors on
  `start`). A function key naming its mod's function (`"use" "func_void"`) resolves to `cu_func_void`; one naming a
  function the progs lacks (d2end's `bmodel_vanish`) is skipped with a warning, once, not a `Host_Error`.
- **Spawnflags** (S3's column live): our monsters' delayed (16), angry (128), quiet (32) and no-telefrag (64) spawns
  through **one adapter** for every ruleset (`QC/vr_rules_monsters.qc`, AD's `AD_MonsterPrepare` moved and generalised;
  AD's behaviour unchanged); coop bits by `VR_SF_SpawnInhibited` (our demon lacked the call: added).
- **Bridge** (`QC/vr_copper_defs.qc`, `QC/vr_copper_bridge.qc`; reviewed patches in `copper_import.py`): Copper's
  `T_Damage` on ours is our `T_Damage`; ours on Copper's damageable entities is Copper's (`CU_Run`: `cu_th_die` or
  `cu_th_pain`); Copper's `worldspawn` after ours and its `StartFrame`'s work (skill buttons, settling frames) in Copper
  maps; Copper's fog through `MG_SetFog` (svc_fog, the one path MG and AD use; the file rename to `vr_fog.qc` not done);
  `target_autosave` runs `vr_autosave_now`. The beam id byte rule is every import's (`qc_import.beam_id_bytes`; it
  fixed d1m1's "Bad server message"; AD's import output unchanged, `--check` 0 of 142).
- **Music**: nothing to change: the maps' tracks are found in Dwell's `music/` (the engine's track stems already
  include `Track`, so `Track107.ogg` is found on any file system).
- **Acceptance** (`Misc/quakevr/dwell_maps.sh`: each map loaded, saved 3 frames after spawn, 600 frames run; the exit
  chain; three saves loaded back; `dwell_maps.py` verdict): every map's ruleset Copper; **0 missing spawn functions
  but Dwell's own** (start 140, d2m8 150, ... P2's); 0 coop-only placements in single player; monsters live + waiting
  e.g. d1m1 31 + 62, d1m3 62 + 119, d2m4 53 + 167 (the 3,343 triggered ones now wait); start's exit goes to
  dwelldm3, d1m1's to d1m2; d1m3, d2m4, d2end saves load back by Copper's rules. **22 passed, 4 failed**: d1m7, d1m8
  and d2m8 end on `PR_ExecuteProgram: NULL function` with a `monster_wizard` as self (delayed angry wizards,
  spawnflags 208/400; `th_die` unset in the printed edict, called with no QC stack: from the engine); d2end's failure
  (an `info_notnull` using `func_void`) is fixed since (the function-key resolution; d2end runs 600 frames clean).
- **Part two** (branch `agent/copper1b`, continued): the wizard's NULL function was Copper's path_corner touch (`think
  = cu_th_stand`, NULL on our monsters: a patrolling wizard, not a delayed one). Every ruleset's path_corner now hands
  our monsters to our `t_movetarget` (`VR_Rules_PathCornerTouch`, `vr_rules_monsters.qc`; AD's hook generalised; an
  import patch in AD's and Copper's touch), and `VR_Rules_MonsterThCheck` names one of our monsters starting with a
  NULL th_stand/walk/run/die (a "QC error" line). Test `vr_ad_test 199`. `dwell_maps.sh`: **26 passed, 0 failed** (its
  saves 15 frames after the map: d2m8's signon is slow). Census vs origin/next over the 168 non-Copper maps: the moved
  adapter's function names only (27 AD saves), nothing else; 0 false Copper detections in 168 official/AD maps and 49
  installed Map Library maps (Ancient, made for Copper, detected). ROUND25 "Dwell P1b, part two".
- **Part three, ship prep** (branch `agent/copper1b`): **Copper custom maps** through the Map Library's path:
  Ancient (copied from an installed copy) and Underdark Overbright v1.1's `udob1` (Quaddicted's zip in
  `qvr-kit/downloads/underdark/`), put into the test base's `qvr_addons/<sha16>/` with their `maps_installed.txt`
  lines by `Misc/quakevr/copper_custom_setup.py` (idempotent, never downloads); `dwell_maps.sh` plays both first, in
  the stock game, the same way as Dwell's maps. Both stopped at `Host_Error: progs/v_axe2.mdl not found`: Copper's
  own models ship with Copper's release (or Dwell's folder), not with a map made for Copper. Each of Copper's 11 own
  model names (`v_axe2`, `s_explo2`, `debris1-3`, `k_spike2`, `v_spike2`, `w_spike2`, `zombienm`, `h_zombnm`) is now
  `VR_ModelOr(<Copper's>, <stock>)` (`vr_packutil.qc`'s existing helper; an import patch, `copper_import.py`), so the
  stock model stands in when Copper's is absent (debris: our `vr_rock1.mdl`). Copper's sounds need nothing (a missing
  sound is a warning). Ancient's one `OBJECT ERROR` (a `target_state` with no targetname: the map's own leftover,
  which Copper reports the same way) is the map's (`dwell_maps.py` `MAP_OWN`); `dwell_maps.py` now counts a map's
  spawn-time errors against that map (`dwell: map X` echoed before `map X`). Installer: `Dwell.OnByDefault = true`
  (self-test, README, INSTALLER.md); the release scripts' sandboxed installs pass `--no-dwell` (their checks stay
  the Quake VR install, not a 367 MB download). CREDITS: the Dwell team (its README). Patch notes 1.1.1: one bullet. `dwell_maps.sh`: **28 passed, 0 failed** (ancient 27
  live + 24 waiting, udob1 31 + 13, all Copper); ReleaseLTO 0 warnings.
  Regression: `mg3_route`'s main and bn legs stall after their planned player death (2 of 60, 4 of 15 steps) on a
  clean origin/next build too (likely the death menu, a25f4d0d4, waiting for a choice): not this branch's; exits 71 passed; saveload 30/0,
  scenario_check PASS (136 run, 0 failed), AD hub 10/0 and world PASS, AD scope ran clean (no verdict line), MG3 buttons, kill, rune PASS.
- **Left** (deferrable, after ship): our monsters' and pickups' numbers by Copper (S6's per-ruleset table); the
  WaterMove/t_void precedence decision with AD's; `vr_mg_fog.qc` renamed `vr_fog.qc`; Copper maps with Dwell
  installed could use Dwell's real Copper models (its folder mounted for a Copper package) instead of the stock
  stand-ins; Copper's `target_debris` lost its FGD preview (the model name is now inside `VR_ModelOr`).

### 9.3 Built: P2a, Dwell's own entities (2026-11-19, branch `agent/dwell2`)

Rebuilt on Copper and Quake VR, hand-written from Dwell's behaviour (the decompile read through
`scratch/dw_const.py`, which puts the value-renamed constants back as numbers, and `dw_seqs.py`, which lists a monster's
frame sequences; reading aids only, nothing pasted). Files `QC/dwell/` (GPL-2, header crediting the Dwell team):
`dw_defs.qc` (early: fields and the hooks our shared code calls), `dw_common.qc`, `dw_items.qc`, `dw_monsters.qc`,
`dw_bosses.qc`, `dw_test.qc`.

- **One ruleset-table row** (`vr_rules.cpp`, decisions 3 and 4): `DWELL`, prefix `dw_`, never a map's ruleset (no
  detection, no campaign); `spawnsOutside`/`toolgunOutside` = Dwell's folder mounted and `dw_<classname>` exists, so its
  entities spawn in any map, with their own behaviour, and the toolgun's `dw_<name>` anywhere. A row's
  `ownedClashes` + `clashesUnder` (new field): Dwell's `item_artifact_wetsuit` wins under Copper's rules, Hipnotic's/AD's
  elsewhere. `VR_Rules_SpawnName` now asks the content rows after an imported ruleset's own lookup (so Dwell's
  entities spawn under Copper's and AD's rules too). Dwell's `monster_morph` and `monster_boss_egypt` (placed in no map)
  are not rebuilt.
- **Dwell's data in every campaign**: a campaign row's `skipOutside` (new `CampaignChecks` field) mounts its folder in
  every campaign while installed, supplying only what the function lets through: AD's (models, sounds, icons: the old
  AD-only code, now the row's) and Dwell's (`qvr::dwell::skipOutside`: its models and sounds, after `skipInFolder`).
- **Monsters** (our monster start, `ai_*`, `T_Damage`, so stealth senses, smarter AI hooks, infighting, parries; their
  animations as table rows, `DW_Seq`: frames, AI, distance, a per-frame event, a follower):
  Kamikaze (40, runs at 28, bursts on contact or death: 60 within 32 units, a sine falloff to 0 at 190, Dwell's curve),
  Brute (600, punch 60 throwing up, grenade volleys of 3 per hand through our ogre's lob at Dwell's 632 speed, 3
  rockets dropped), Construct (flyer, 600, crystals 700/40, below 340 health a steering dive at 600 bursting 60).
  `vr_th_checkattack` (a monster's own attack choice in `CheckAnyAttack`). **VR registration, first pass**: head zones
  (`DW_PositionalHead`: Kamikaze, Brute, the Awoken; estimates from their boxes, P2b fits them with `headfit.py`),
  Dwell's hulls, gibs (the Brute's head `h_brute`, the Construct's 4 pieces, the Kamikaze bursts). Not done (P2b): rigs
  (ragdolls, limbs, knockdowns, corpse gibbing), `vr_mhull_*`.
- **Bosses** (`dw_bosses.qc`, first pass): the Awoken (d1end: 1400, three phases through `th_survive`, a new generic
  field: a monster's refusal to die, checked first in `Killed`; each phase fires target/target2/target3, sleeps through
  frames 89-123 and wakes at its `include`; homing missiles 750, 25/35/55 by skill, big volley, dashes and side steps;
  target4 at death). The Sculptor (d2end: dormant until used; flies its path from `include`; swarms of 7 missiles onto
  ground marks around `target2`; each phase fires `target` (the map's counter), shields, rests `wait`; marked by
  `trigger_killboss` it dies, firing target4). `target_bullethell`: line, two lines, ring, spiral, wheel of 5-damage
  bullets (Dwell's moving and turning patterns: P2b).
- **Powerups**: Haste 20 s (refire x0.6, in `RuneApplyHell`, the refire scaler every weapon calls), Berserk 30 s (melee
  x11: Dwell's axe 264 for 24; damage taken halved: `DW_DamageScale` in `T_DamageImpl`), Wetsuit 60 s (breath and a
  swim speed cap of 1.3x under water). Ends said 3 s before; Haste and Berserk glow. No HUD icons or screen tints yet.
- **Weapons, placeholders** (variants on the weapon instance, `wi_dwvar`, as AD's upgrades; set by the pickup through
  `DW_VariantGiven` beside `AD_UpgradeGiven`): Rotary Shotgun = super shotgun firing 20 pellets of 2 every 0.2 s;
  Crystal Lance = lightning gun firing a piercing beam (150 each body, 8 at most, 6 cells, 1.4 s); Rifle = shotgun firing
  one 50 bullet (1.4 s). The pickups show Dwell's models. Not kept across a level change (the instance's variant is not
  in the parms yet: P3).
- **World**: `misc_crystal` (Dwell's crystal BSP models by style), `func_fade` (alpha by the nearest player's distance,
  192-384 units; used, it fades out or back), `info_storycamera` (its pages: Dwell keeps the story text in `message`,
  `noise`, `noise1..4`; centre-printed 5 s each, then its targets; the camera flight is P4).
- **Copper's own models in Copper maps** (step 4): Dwell ships none of Copper's 11 (`v_axe2`, `s_explo2`, `debris1-3`,
  ...), so the stock stand-ins stay; with Dwell's folder mounted everywhere, `VR_ModelOr` would pick one up by itself.
- **Toolgun**: 11 rows, pack "Dwell" (`vr_toolgun_packs.inc`, appended), made while their model is on the search path.
  No new weapon id (the toolgun_weapons check unchanged). **FGD**: group "Dwell" (`/*dw:QUAKED`, `fgdgen.py`, 447
  entities).
- **Acceptance**: `check_copper_entities.py`: dwell 14, missing 0. `dwell_maps.sh`: 28 passed, 0 failed, 0 missing on
  every map (Dwell's own now required to spawn: `dwell_maps.py`). `vr_ad_test 200` (d1end: the Awoken woken, phases 3,
  died: PASS; d2end: the Sculptor used as its relay does, 3 phases, marked by the map's counter and killboss, died:
  PASS). e1m1 `vr_toolgun_spawn_test`: the 11 Dwell rows spawned.

### 9.4 Built: P2b, Dwell's monsters VR-native (2026-11-19, branch `agent/dwell2b`)

- **S7 measured, one row format**: AD's registration (`VR_AD_Mon`/`VR_AD_MonKd`: bits, head zone, head gib, corpse
  health, knockdown) was already the generic part; what made it AD's was 7 dispatches into AD's code (pain, run,
  Killed). Those now ask `VR_AD_Code(e)` (a row without the new bit `VR_ADM_OURS`), so any monster on our own code can
  be a row: Dwell's are (`DW_MonsterRow`, `dw_common.qc`), and `weapons.qc`'s Dwell head hook and `DW_PositionalHead`
  went. Our own and MG3's monsters keep their per-class chains (`PositionalHead`, `VR_Decap_HeadModel`,
  `VR_Corpse_Parts`: about 40 rows over 3 functions): moving them would trade each chain line for a row line (no code
  removed), so not now. The engine's per-class alias table (`vr_ad_monsters.cpp`) and monster groups (`vr_mgroups.cpp`)
  took Dwell's rows the same way.
- **Kamikaze**: head zone 0 19 6 (`headfit.py`), never beheaded (it bursts), no rig (no death frames); stealth all ears
  (hear 1.2, sight 0.8); grab mass 90 (thrown as a knight); monster group Spawns.
- **Brute**: rigged (`dwBruteSeeds`, `vr_ragdoll.cpp`; `rig.py` on Dwell's model with the new `RIG_MDL`, 22 clusters,
  13 bones 1.32 rms; the cannon the left hand; wrists and two seed centres set by hand), head zone 5 61 10, beheading
  (Dwell's `h_brute` thrown), limbs, knockdowns (its deaths backwards; the shambler's chance), corpse gibbing (the
  shambler's health), its punch parried (`VR_ADM_MELEE`), ragdoll settings and grab mass the shambler's, monster group
  Mini Bosses, stealth the ogre's senses.
- **Construct** (decided): no head zone (a crystal-and-stone machine), no rig (it breaks into its four pieces), blunt
  blows sound on armour; grab mass 600; monster group Scrags (a flyer).
- **The Awoken**: head zone 0 125 16 (its model is 248 units tall: the box now reaches 140 up, so shots at its head hit;
  movement still the shambler-size hull); a boss: never beheaded, no knockdown, no corpse; stealth off (bosses keep
  Quake's AI, as the Sculptor); grab mass 5000.
- **Test**: `Misc/quakevr/dwell_monsters_test.sh <agent>` (the AD bestiary's cases through `vr_ad_mtest`, whose frame now
  finds Dwell's monsters; the debug spawner's 70-73), verdict by `dwell_monsters_check.py`: 11 passed, 0 failed.
- **Left (P2b part two)**: Dwell's own bullet-hell programs and the Sculptor's motion: built (9.6); the Awoken's scale
  reviewed in the headset.

### 9.5 Built: P3, first part: Dwell's weapons and powerups on our player (2026-11-19, branch `agent/dwell3`)

- **Variants, not new weapon ids (the AD precedent).** AD's three weapons ended as variants on the weapon record
  (`wi_adup`), the Plasma Gun included, although section 8 of ARCANE_DIMENSIONS.md had proposed a new WID for it. The
  same reasons hold for Dwell's: each shares its base weapon's ammo, holster, reloading, wheel slot, ammo screen, flat
  HUD and toolgun row, so a variant gets all of that for free, and a new WID would need a weapon-table row, models,
  offsets, holster rules, a wheel slot and a toolgun_weapons change for each. So the Rotary Shotgun stays a super
  shotgun, the Crystal Lance a lightning gun, the Rifle a shotgun, each with Dwell's shot (`DW_FireVariant`).
- **The Rifle removed (2026-11-23, his note; branch `agent/dwrifle`)**: none of Dwell's 43 BSPs (24 maps, 11 brush
  models, the rest) places `weapon_rifle` or uses `v_rifle.mdl`/`g_rifle.mdl` (Dwell's progs: only `Cheat_Rifle`; its
  `g_rifle.mdl` is not even shipped), no pool or drop gives it, so it is gone: its spawn function, `toolgun_dw_rifle`
  and the toolgun row, `DW_VAR_RIFLE` (3, retired: a record still holding it, from an older save, is a plain shotgun:
  `DW_FireVariant`, `DW_WeaponModel` and the armed grunts' names check it), the made model `progs/dw_rifle.mdl#vr`
  (`vr_adweapons.cpp` recipe, housing; `vr_screenstyle.cpp`, `vr_gunfx.cpp`, `vr_notectx.cpp` entries) and
  weapons.txt slot 54 (left empty: slots 55 and 56 keep their numbers and his `vr_wofs_*_56/57`). `weapon_rifle` stays a
  Copper/Dwell detection signal (`vr_rules.cpp`). Test: `vr_ad_test 208` (dwell_weapons_test.sh). The Rifle notes
  below are history.
- **Kept with the weapon**: `.wi_dwvar` is now a weapon-record field beside `wi_adup` (`vr_weaponinst.qc`), copied by
  `WeaponInst_Clone`, saved with the record; across a level change and a coop rejoin through **parm76** (new:
  `DW_PackWeaponVariants`, 2 bits a record, the hands' then the holsters'; `vr_progs.hpp` `lastExtSpawnParm` 76). An AD
  upgrade and a Dwell variant exclude each other (each pickup clears the other).
- **VR handling, shared**: the Rotary Shotgun handles as the Widowmaker (both rotary auto shotguns: `AD_Widowmaker` is
  true for it): its tube (`vr_ad_widowmaker_tube`), filled on pickup, reloaded at the hip a handful at a time, never
  broken open, and the barrels' spin-up before the first shot (`vr_ad_widowmaker_spin*`), in VR and flat play alike;
  its shot stays Dwell's (20 pellets of 2, a shell, 0.2 s). The Crystal Lance's beam pierces up to 8 bodies (150 each),
  as P2a. **Bug fixed**: the Lance's beam message lacked the beam's disambiguator byte (Quake VR's `TE_LIGHTNING2`),
  so the client misread the stream ("Received signon 0") at its first shot.
- **Powerups on the wrist**: Haste, Berserk and the Wetsuit show on the wrist gadget's item row (and the flat visor's,
  the same code) as their names with seconds left, blinking in their last 3 s (`STAT_QVR_DWPOWER`, QC `.vr_dw_hud`).
  Dwell ships no icons for them (its CSQC draws them), hence text. The wetsuit clash stays as decision 3 (P2a's
  `clashesUnder`).
- **Enemy-gun drops**: none of Dwell's monsters carries a Dwell gun (the Brute throws grenades and drops rockets, the
  Construct fires crystals, the bosses missiles), so there is nothing to drop.
- **Test**: `Misc/quakevr/dwell_weapons_test.sh` (vr_ad_test 201-203): the three given (main hand, off hand, holster
  0), then a save and load (each still Dwell's, then fired at fresh grunts held on its line, the mock's hands levelled:
  the Rifle 63 to one and none past it, the Rotary 50 of its 20 pellets, the Lance 188 to each of two: Dwell's 50, 40
  and 150 with our bonus on an unaware target) or a level change to e1m2 (each still Dwell's): 4 passed, 0 failed.
- **P3, second part (2026-11-19, `agent/dwell3`, dwell3b)**: merged with next (P2b: `DW_PositionalHead` gone, P2b's
  section 9.4, this one 9.5) and verified: `dwell_maps.sh` 28/0, `dwell_weapons_test.sh`, `dwell_monsters_test.sh`
  11/0, `saveload_test.sh` 30/0, `scenario_check.sh` PASS, the spawn census against a fresh origin/next build (the
  168 non-Copper maps), 0 warnings dev and ReleaseLTO. Then:
  - **Armed grunts**: a grunt taking a Dwell weapon already kept its record (`VR_AG_TakeUp` hands it to `VR_AG_Arm`),
    but fired our weapon's shot (the Rotary the Widowmaker's: `AD_Widowmaker` is true for it). `VR_AG_ShootProp` now
    fires Dwell's (`DW_FireVariant`, the player's own code: its clip spent as your ammo, as every grunt gun), the
    Lance's beam drawn from the grunt's prop (`vr_fire_prop`, disambiguator 0 as its lightning); its prop is named
    as Dwell's (`VR_AG_NameOf`, `DW_VariantName`). Its drop carries the record, so the variant comes back to you.
  - **Screen tints**: Dwell stuffs `v_cshift` (its `client.qc`: Haste 255 255 0 at 10, Berserk 255 0 0 at 20, both
    255 128 0 at 20, the Wetsuit none); ours go through AD's tint (`VR_AD_Tint`: a stat the engine eases in, times
    VR Settings' Screen Tints), never over an AD debuff's own (`.vr_dw_tinted`), cleared when they end.
  - **Test**: `vr_ad_test 204` (in `dwell_weapons_test.sh`): two grunts armed from Dwell records (named, record and
    variant kept, one Dwell shot each: a shell, 6 cells), the tints (yellow, orange, none): 9 passed, 0 failed;
    the script 6 passed, 0 failed.
  - **Not done, decided**: the variant's name in the flat holster menu (the flat wheel) and the VR wrist follows its
    model: the client names a held or holstered weapon by its id and flags (`notectx::weaponName`), and AD's
    Widowmaker is named "super shotgun" there too. Naming by the view model (`#vr` model index -> name) covers AD's
    and Dwell's at once, once Dwell's own VR models exist; a variant stat now would be a second path to remove.
- **Left for P3's third part**: Dwell's own view models as VR models (`v_shot3`, `v_rail`, `v_rifle` through
  `vr_adweapons.cpp` recipes: measured grips, muzzles, the arm cut off; until then the variants show our models),
  the Rotary's drawn barrel spin with them (Dwell's `v_shot3.mdl` shares AD's Widowmaker's file name: its recipe must
  pin Dwell's release by its counts, as AD's are pinned), and the variant names named by those models. Done: 9.6.

### 9.6 Built: P3, third part: Dwell's weapons as VR models (2026-11-19, branch `agent/dwell3c`)

- **Recipes** (`vr_adweapons.cpp`, beside AD's three; `vr_adweapon_models` prints them): `progs/dw_shot3.mdl#vr`,
  `progs/dw_rail.mdl#vr`, `progs/dw_rifle.mdl#vr`, made as they load from Dwell's `v_shot3.mdl`, `v_rail.mdl`,
  `v_rifle.mdl`. Dwell's view models have **no arms** (nothing to cut but their muzzle flashes, pieces collapsed to a
  point). Each is levelled (the Lance is tilted 15.9 degrees up in Dwell's view, the Rifle 6.25 down), scaled to sit
  inside its base model's corner (the slot's Scale applies about it: the grip keeps its place) and given our base
  weapon's grip, trigger guard and trigger (the Rotary on the super shotgun at 0.8, its housing over the grip; the Lance
  on the lightning gun at 0.88, its wooden handle over the grip; the Rifle on the shotgun at 0.7, on its barrel line).
- **Pinned to Dwell's file**: `Source::folder` reads the source in place as `owned/dwell/<path>` (`VR_OwnedFile`:
  Dwell's discovered folder, whichever campaign is active), and its counts pin v2.2 (260/286, 311/339, 117/121). AD's
  Widowmaker keeps reading the search paths' file of AD's counts, so each mod's `v_shot3.mdl` makes its own weapon.
- **Slots** 52-54 (`weapons.txt`, `numSlots` 55): the base weapon's values copied as their own (no inheriting, round
  22's rule), the muzzle their own (the Rotary's barrels' middle, the Lance's middle crystal's tip, the Rifle's shroud's
  front). Sights, two-hand points and holster poses are the base's: a first fit for him to tune (his values become the
  defaults).
- **The Rotary's barrel spin** (shared flat and VR: `adweapons::spinFrame` draws both): its four barrels, clamps and
  front block (the pieces from x 16.5) get 27 spin frames ("quad0"..) about their own axis; the Widowmaker's spin code
  drives them (the trigger spins them up, they coast to a barrel on top), a barrel a shot at Dwell's 0.2 s.
- **Drawn**: `DW_WeaponModel` (`dw_items.qc`) through `AD_WeaponModel`/`AD_SyncModel`: hands, holsters, drops, armed
  grunts' props; without Dwell's data our models. gunfx knows the models; the Lance's muzzle light is violet.
- **Names by model**: `notectx::weaponName(wid, flags, model)` names a variant by its VR model (AD's Shadow Axe,
  Widowmaker, Plasma Gun; Dwell's three); the holster menu (flat, and the wrist's: the same slots) uses it.
- **Test**: `Misc/quakevr/dwell_vrmodels_test.sh` (vr_ad_test 205-208): the models in the hands and a holster, AD's
  Widowmaker still its own, the six made, the menu's names, the spin in mock VR and flat play (turning held, at rest a
  barrel on top): 13 passed, 0 failed; screenshots of each in hand to `scratch/dwell_vrmodels_vr.png` (side and eye
  views) and `scratch/dwell_vrmodels_flat.png` (and the holster menu).
- **Ammo screen housings** (dwellgun, 2026-11-20; ROUND25.md "dwellgun"): the AD Plasma Gun's housing (flat bezel,
  chamfered window, a rivet each side; `vr_adweapons.cpp housing()`, made with the model) on each rear face: the
  Rotary's housing back (Dwell's brown plate, bolt heads), the Lance's wooden handle's back (its frame's dark metal,
  violet crystal studs), the Rifle's butt (brass, the shotgun's brass nuts). Each window the glass the screen had
  without a housing (fillMax 1); `vr_screenstyle.cpp` entries `dw_shot3.mdl#vr`, `dw_rail.mdl#vr`, `dw_rifle.mdl#vr`;
  weapons.txt slots 52-54's `wpntxt_av` on the glass, upright, fonts 1 (the Lance's `wpntxt_scale` 0.25: its handle's
  back is 2.6 wide). The Lance sits 0.4 higher on its grip (the grip's top came through its frame) and its trigger
  guard and trigger 0.8 up onto its underside (they hung 0.37 under it), its `muzzle_av` 0.4 up.
- **Left**: no shell ejection port or magazine mounts of their own (the base's settings point at the base model's
  places); the Lance and the Rifle have no hand-tuned sights; the Rotary's big rear face is the dark hole cap (a
  checker of dark red and yellow in the game's light, as before), around the screen; its muzzle's two open barrel
  loops (check_mdl_holes, as made by dwell3c).

### 9.6 Built: P2b part two, bullet hell and the Sculptor's motion (2026-11-19, branch `agent/dwell2c`)

- **`target_bullethell`, Dwell's patterns** (`dw_bosses.qc`; read from `m_d2boss_hell.qc`, whose dispatchers are
  corrupt in the decompile: the calls are readable, rewritten): one table, `DW_HELL` (28 rows: style, kind, from/to,
  count, cycle, life, delay, speed, damage, flag), each row a task entity with one clock (`dw_hell_tick`): 1 a line of
  10 at each player (following him), 2 two lines straddling where he stood, 3 a ring of 24 (half a step turned each use,
  Dwell's alternation), 4 two spirals of 48, 5 a moving wheel (16 shots circling a drifting point, 225 degrees a second;
  its ring hurts), 6 a rocket barrage (Dwell left it empty: 6 rockets onto marks around each player, the Sculptor's
  swarm shot, `DW_SwarmShot`, shared), 7 a pulse (24 sectors swelling 64 to 320 and back twice; its ring hurts), 8
  Dwell's program A (never placed in its maps) once through (28 s; its loop forever dropped: a definite end), 0 random
  1-7. d2end uses 1-5 from its timers.
- **VR**: the shots are enemy projectiles (`vr_foeshot`, a new generic field `VR_Deflect_IsProjectile` accepts: a swing
  bats them back, then they hurt monsters), fly through monsters (Dwell's), never collide with each other (triggers),
  glow and streak (the engine gives `hell_bullet.mdl`, flagless, the hell knight's flame trail: `patchModelFlags`, so
  emissive and bullet-time trails as any enemy shot). The aimed lines are slower (500/550 for Dwell's 700/800); the
  rest at Dwell's speeds. Ring damage is Dwell's rate (40 a second on the ring) in ticks of 4 every 0.1 s, the ring's
  sweep between ticks counted. Comfort: a bullet-hell hit never pushes the player (his velocity kept: `DW_HellHit`).
- **The Sculptor's motion** (Dwell's `m_d2boss.qc`): a ring lying flat while dormant; flying its path it spins about
  its axis (`fixangle`, 180 a second) and tilts its face toward its enemy (pitch followed, yaw held at 0: Dwell's), its
  heading set again each tick (VR's knockback on flyers no longer pushes it off its path); a phase over, it lies flat
  again (0.5 s), raises its shield (`mon_d2bossf.mdl`, fading in over 0.6 s), teleports out (frames 1-8), to its
  `target2`, in (9-15), rests `wait` s turning slowly the other way, then flies again; dying, its spin runs down. Not
  restored: Dwell's 50-odd small hit boxes around the ring (one 96-unit box still).
- **Acceptance**: `vr_ad_test 200` now runs, where the map has a `target_bullethell` (d2end), a pattern sweep first
  (styles 1-8 next to the landed player in god mode, each when the last is gone): 8/8 fired, the line 7 of 10 on him,
  edicts at most +82, back where they were; then the fight, the Sculptor's steps all seen and its rest at target2:
  d2end PASS, d1end PASS. `dwell_maps.sh` 28/0, `scenario_check` PASS (136 run, 0 failed), the spawn census against
  origin/next identical on the 168 maps (the test's counters `nosave`).

### 9.7 Built: P4, first part: the deferred items of P1b-P2c (2026-11-19, branch `agent/dwell4`)

- **S6, one numbers table** (`QC/vr_rules_numbers.qc`, `VR_RN[]`: one row a ruleset in `VR_RULES_*` order; ID, MG and
  AD rows are Quake's numbers). Our own code reads it; no Copper branch in a touch: `armor_touch` and the worn-armour
  check (`VR_Armor_IsBetter`) take the protection (`VR_RN_ArmorType`: Copper's 0.3/0.5/0.7, Quake's 0.3/0.6/0.8) and
  the pickup rule (`VR_RN_ArmorTake`: Quake's type x value, or Copper's breakpoints 40/60/100, part of a bigger weaker
  store kept, capped 500); `health_touch` the megahealth rot (Copper's one rot a player from 5 s after the last
  megahealth, `VR_RN_MegaRot`/`VR_RN_PlayerFrame` in PlayerPostThink; AD's own `AD_MegaRot` first, unchanged);
  `key_touch` the key stack (Copper's 16 of a colour, counted in the player's `.worldtype` as Copper's doors read and
  take them: `VR_RN_KeyTake`); `VR_SF_MonsterPrepare` our monsters' health (Copper's grunt, dog, fish 35, knight 72;
  only where the spawn function left Quake's, so an armed grunt's or a variant's own stays); the enforcer's laser
  (Copper's 12). **Placed backpacks** need no row: `item_backpack` has no Quake VR spawn function, so in a Copper map it
  is Copper's own (its excess ammo to armour by its `armor_give`, the same protection values as the row). Test
  `rules_numbers_test.sh` (vr_ad_test 209): e1m1 12/0 (Quake's), Dwell's d1m1 12/0 (Copper's).
- **The imports' one rule for precedence look-alike bugs** (`copper_import.py`, `ad_import.py` comments): kept as the
  mod shipped it (its own compile read it the same way), unless the shipped reading changes what a Quake VR player
  gets and the mod's intent is plain; then fixed as meant, marked "bug fixed as meant" (AD's bugs 4 and 5, the
  precedent: its tints are drawn here). So `WaterMove`'s gasp stays as shipped (Copper's WaterMove never runs: our
  player's does), and `t_void`'s `noTint` is fixed (`(a && farclip && !coop)`): as shipped, a coop game with
  `gl_cshiftpercent` under 90 put func_void's darkening into CSQC stat fields nothing draws here; single player is
  unchanged (gl_farclip is never 0). AD's four "as shipped" ones stay (none reaches the player differently here).
- **S5, one fog path**: `QC/vr_mg_fog.qc` is `QC/vr_fog.qc`, its API `VR_Fog_Set`, `VR_Fog_InfoEntity`,
  `VR_Fog_PushFrom` (MG's entities, AD's `VR_AD_Fog`, Copper's import patch in `cu_fog.qc` for its fog commands and
  blends). The MG fields (`MG_fog_valid`...) keep their names (saves). `copper_import --check` 0 of 63 differ.
- **The Sculptor's ring** (`DW_D2_FightBox`): Dwell's 50-odd small hit boxes are our precise hits on its model (shots
  meet its triangles inside its box, vr_hitmodel.cpp): the fighting box grows from 96 to 288 units so the ring's stones
  (120-140 units out) are inside it; the gaps between stones let shots through; it moves noclip, so the box never
  stops it. The fighting model has a solid core (no hole: all 49 shots inside the old box hit). Test vr_ad_test 210
  (d2end): turned three ways, 93-142 hits past the old box and 98-147 shots flying on between its stones, 7/0.
- **d2m8's cost** (QC instructions, `profile`, deterministic; no timings: his game was running): 720 frames after 120
  of settling, d2m8 (4,826 edicts) 385.6 M instructions, 536 k a frame; AD's biggest map `ad_tears` (4,011 edicts)
  25.8 M, 36 k a frame. The difference is our armed grunts: d2m8 has 251 (Dwell's grunts replaced; AD's soldiers are
  AD's own), and `VR_AGP_Pose`, `VR_AGG_Frame`, `VR_AGS_Projectiles`, `VR_AG_Rotate`, `VR_AGPar_Frame`... lead the
  list, about 2 k instructions a grunt a frame; Dwell's own code is not in the top 25. No single hot spot to fix
  without changing the armed grunts' behaviour (a key feature); 536 k instructions is about 1 ms a frame. A later
  step if the headset shows it: a cheaper pose for armed grunts far out of sight (`VR_AGP_Lod` exists).
- **d2m8's cost by grunt state** (aglod, 2026-11-20; `vr_agrunt_test 226` idle at his start, `227` every grunt woken
  at him, god; each entity's think statements, `thinkcost`, given to its grunt; 360 frames; 156 live armed grunts,
  the rest waiting). Instructions a grunt a frame (grunts in that state on average):

  | state | before | after |
  |---|---|---|
  | idle, unseen | 2,463 (92) | 976 (136) |
  | idle, seen (in view and PVS, beyond 384 units) | 3,526 (64) | 2,875 (20) |
  | alert (woken, enemy out of sight) | 3,673 (110) | 2,918 (110) |
  | fighting (enemy in sight) | 3,507 (5) | 3,057 (5) |
  | dead | 12 | 11 |

  "Unseen" after counts a grunt out of every player's PVS as unseen (the split moves from 92/64 to 136/20). Others'
  thinks 55 k a frame (the pickups' forcegrabbable thinks 39 k of it). Totals (`profile`, 720 frames): d2m8 idle
  536 k to 274 k a frame; all woken, thinks 602 k to 464 k; e1m1 38 k after. Under 150 k is not reached without
  changing what can be seen: the 20 grunts in view (57 k), the other thinks (55 k) and non-think QC (29 k) are
  141 k alone. What changed: ROUND25.md, "Armed grunts' cost on crowded maps". Verified (aglod2, 2026-11-20):
  ThinLTO build 0 warnings; aglod_test.sh 13/0 (228 with two unarmed trials, heard and shot: the same traces and
  reaction frames with 8 frames, 2 and no LOD; 229 a worn pack walked into view); dwell_maps 28/0, dwell_monsters
  11/0, scenario_check 137/0, flatplay 31/0, adbugs and vr_ad_test 198 pass; the agrunt numbers' failures (16, 35,
  39, 42, 61, 91, 203) and tunables_test's two are the same on origin/next. `vr_agrunt_pose_lod_frames` 0 or 1:
  every frame, no LOD.
- **Left**: Copper's ogre rocket drop, nightmare refire, infight give-up and the other monster rules of 2.4 (behaviour,
  not numbers); item respawn by `wait`/`count` on our items in Copper maps.

### 9.8 Fixed: his first playtest's bugs (2026-11-21, branch `agent/expbugs`; ROUND25.md "expbugs")

- **Music**: Dwell's folder is mounted in every campaign for its models and sounds (`skipOutside`), but the track search
  read every search path, so its `music/track02..05` played over Quake's and the mission packs' (Quoth ships no music).
  `vr_music.cpp` now skips what `VR_SkipSearchPath` skips; and a track above 127 that the search path has as a plain file
  (Dwell's own numbering, section 4: start's 254) plays from it before the campaign-track encoding (it played nothing).
  `vr_music_which [track]` (Debug > Tools: Music File). Test `music_test.sh`.
- **Pickups**: `weapon_touch` found the weapon by its classname; Dwell's keep theirs (`weapon_rifle`..), so it asserted
  and removed them. It takes `self.weapon` (impl_weapon_item's). The toolgun lays Dwell's three and MG3's two bloody
  shotguns as props (`toolgun_dw_*`, `toolgun_bloody_*`: func_weapon_grabbable with `vr_grab_dwvar` / the prop's
  `vr_bloody_bit`, given on taking: `MG3_Bloody_PropTaken`). Test `vr_ad_test 213` in `dwell_weapons_test.sh`.
- **Start's rune gates**: Copper's spawn points fire their targets (its PutClientInServer); ours never did, so start's
  `start1_targets` (a target_lock unlocking the pedestals' relays) never ran: runes placed opened nothing and the walls
  before the episodes' teleporters stayed. `CU_PlayerSpawnedAt` fires them, after the START_LOCKED locks' 0.25 s (a
  player spawned sooner had them locked again). Test `vr_ad_test 214` in `dwell_maps.sh`.
- **Started in a deathmatch game** (Select Campaign while hosting, mpbugs3): start has coop starts but no
  `info_player_deathmatch`, and SelectSpawnPoint's deathmatch scan never ended ("runaway loop error", Host_Error in
  Host_Spawn_f). The scan now ends after two passes; with no deathmatch start it takes coop's starts
  (`VR_CoopSpawnPoint`: the next clear one, else beside one), so two players don't telefrag on the player start. Test
  `vr_dm_test 22` (set before `vr_campaign_native dwell`; deathmatch and coop, a bot as the second player).
- **Kamikaze**: `EntitiesTouching` compares mins/maxs (a brush's absolute box): two monsters' relative boxes always
  overlap, so it burst the moment it had an enemy. Now its box within 8 units or 64 units apart; its burst (and the
  Construct's death) sends `TE_EXPLOSION` (`DW_Boom`). **Brute**: the 14-vertex piece across his front was split
  between his hands; `quakevr/rigs/mon_brute.rig.txt` puts its 7 right-side vertices on hand_l
  (`vr_ragdoll_rig_spans <model>`: 6 spanning triangles, now 0). **Sculptor**: its 288-unit solid box flew noclip onto
  players; each think puts a player inside it just outside (`dw_d2_freeplayers`). Test
  `dwell_monster_fixes_test.sh` (`vr_ad_test 215`, `216`).

---

### 9.9 Built in part: his weapon notes of 2026-11-21 (branch `agent/dwellwpn2`; ROUND25.md "dwellwpn2")

- **The Crystal Lance as a lance** (`vr_dw_lance_pole`, 1 by default; 0 the pistol of 9.6): `progs/dw_lance.mdl#vr`, a
  recipe over Dwell's **world model** `g_rail.mdl` (pinned 672/464; a staff 67.5 long standing 22.6 degrees off upright,
  its pole 2.2 thick, its crystal head from 43 along it), laid along x at 1.5 (1.35 m drawn), the lightning gun's grip,
  guard and trigger under its pole 16 from the butt. New recipe options: `keepOpen` (Dwell's single faces kept open, as
  Dwell draws them) and `keepScale` (a model past the base model's corner moved by (corner - lo)(1 - Scale), so the base
  slot's values keep the hand on the grip). Slot 55 (numSlots 56): slot 53's values, its anchors moved with the model,
  `muzzle_av` the crystal head's tip, `anygrip_mode 3`, `bl_near 0.32` (the blow line from the butt to the tip).
- **The pole grip, general** (`vr_twohand.cpp` `FREE_POLE`, a weapon's own Other Hand Anywhere 3; any long weapon may use
  it): the other hand taking the weapon anywhere becomes a foregrip that slides along its long axis (its blade line
  through where it took hold, from Blade Near behind the hand to the tip): each frame the place along the pole as far
  from the holding hand as the hand is, the least turn putting it on the line through the hand (in front of the holding
  hand or behind it); within 4 units of the holding hand the last turn is kept; it lets go past the butt or tip by the
  hotspots' keep. In VR the weapon's hotspots never take the other hand (`twohand::poleGrip`, the view's free candidate
  ignores them); flat play keeps them (its fixed two-handed hold: hs1, slot 53's). The drawn hand slides with it.
- **Its blow**: `DW_LanceMelee` (QC, chosen in `VR_Melee_Deal` by `DW_LanceHeld`): the axe's damage (`vr_dmg_axe`, before
  speed and weight), a blade's sounds and wall chop, where every other gun clubs (`W_GunMelee`, `vr_dmg_gun_bash`). The
  beam is Dwell's as before, from the muzzle: the head's tip.
- **Test**: `Misc/quakevr/dwell_lance_test.sh` (vr_ad_test 217): the lance model and blow, the pistol and club with the
  setting off (5/0), seven models made; mock VR, the off hand takes the pole 0.75 of the way to the tip (21.1 from the
  handle), slides out (20.8 -> 26.2 along it) and in (8.9) without letting go, `vr_status` "on its pole": 8/0.
  `dwell_vrmodels_test.sh` 13/0 (its screenshots show the lance in hand, VR and flat).
- **Left (item 1)**: the main hand holds the grip only (it does not slide: a held weapon's origin is its hand's; the
  hand-off already lets the other hand carry it anywhere); no ammo screen housing (its `wpntxt_av` is slot 53's moved,
  inside the pole); flat's two-handed hold point (hs1) and the sights are slot 53's, unmeasured on the lance; the blow's
  damage on a monster is not measured by a test (only its choice); the lance's length in his hands to tune (k 1.5).
- **The Rotary Shotgun as a gauntlet (item 2; dwellwpn3, ROUND25.md "dwellwpn3")** (`vr_dw_rotary_gauntlet`, 1 by
  default; 0 the super shotgun's grip of 9.6). Why it could not be reloaded: ours loads as the Widowmaker (9.5: its tube,
  `vr_ad_widowmaker_tube` 16, filled a handful at a time, `vr_ad_widowmaker_handful` 2; never broken open; Dwell's own
  Rotary has no reload, Copper's guns feed from the pool), but its model had no load point (`vr_view_internal.hpp`
  loadPorts: none for `dw_shot3`), so a handful held anywhere never went in; only the gun brought to the pouch loaded it.
  Now `progs/dw_rotary.mdl#vr`: Dwell's view model laid round the super nailgun's hand (k 1, 40 long; its housing's back
  bottom on the super nailgun's back, -2.6 0 -5.5; the barrels' axis on z 0; nothing grafted: the hand holds inside the
  housing), packed from the super nailgun's corner, weapons.txt slot 56 (numSlots 57) the super nailgun's hold values
  (slot 4) with its own muzzle, screen and the Rotary's weight and kick. On the housing's inner side (+y) a block with a
  dark opening in a raised rim, the Widowmaker's way (`rotaryGauntletParts`): its load point (`DwVrRotary`, 5 7.25 0;
  the Widowmaker's Load Point offsets and `vr_reload_port_widow_radius`, as the QC loads it as the Widowmaker). Its back
  keeps the same ammo screen (`rotaryGauntletHousingSpec`, a screen style alias of `dw_shot3`). Dwell's shot kept.
- **Reliefs for Dwell and Quoth (item 3): the plan, nothing built.** AD's are made on the developer's machine by
  `Misc/quakevr/pbr_generate.py` (PBRify's two CC0 models through PyTorch; docs/vr-port/PBR_PIPELINE.md) from AD's
  installed PAKs and committed as `quakevr/textures/pbr/<hash>_norm.png` (looked up by the texture's pixels' hash in
  `vr_pbr.cpp`, before `_norm`/`_bump` files and the normal map made at load). Doing that for Dwell or Quoth would ship
  files made from their textures: not allowed (section 1, QUOTH.md). Allowed: generating on the user's machine from his
  own copy into a user folder the engine searches (the lookup needs no change). What that takes: a Setup step (or a
  console command) running the generator over `<base>/dwell` and `<base>/quoth`, writing outside the release's
  `quakevr/`; its dependencies are the blocker (Python, PyTorch about 2 GB, the 116 MB PBRify archive); the light route
  is the two models exported once to ONNX (CC0: shippable) and run by onnxruntime (DirectML) inside Setup or the
  engine. Meanwhile every Dwell and Quoth texture already gets the normal map made at load from its own shading
  (`vr_normalmaps.cpp` TexMgr_ShadingToNormals; height relief only with `vr_texture_smooth 2`).
- **Reliefs for Dwell and Quoth, the options (dwellwpn3; still a plan, nothing built: his call).** Shipping reliefs made
  from Dwell's or Quoth's textures is not allowed; whatever is made must be made on his players' machines from their own
  copies, into a folder the engine searches outside the release's `quakevr/` (`vr_pbr.cpp` looks a texture up by its
  pixels' hash: the same `<hash>_norm.png` names AD's pack uses, so a user folder on the search path needs no lookup
  change; to confirm when built). Agent-days are qvr-agent work, his playtest extra.

  | Option | What runs where | His players need | Quality | Cost |
  |---|---|---|---|---|
  | A. ONNX in Setup | PBRify's two CC0 models exported to ONNX once on the developer's machine (shippable: CC0), run by onnxruntime (DirectML, CPU fallback) in a Setup step "Make reliefs for Dwell and Quoth" over `<base>/dwell` and `<base>/quoth`; a few minutes on a GPU | nothing more: the runtime and the two models in the release (onnxruntime with DirectML about 20 MB, the models about 60-120 MB) | AD's (the same models) | 2-3 days: the export and a parity check against `pbr_generate.py`, the Setup step and its progress, the folder, a test on a small texture set |
  | B. ONNX in the engine | the same models run by the engine itself on a mod's first load (a background job, the reliefs written as they finish) | the same runtime and models | AD's | 3-4 days, and a new runtime dependency in the engine (start-up, threads, its failure paths) |
  | C. A script | `pbr_generate.py` documented for players with Python and PyTorch (about 2 GB) and the 116 MB PBRify archive | Python, PyTorch, the archive | AD's | half a day (docs, the user folder), for the few who will do it |
  | D. No model | the normal maps made at load from the textures' shading (`TexMgr_ShadingToNormals`) given height relief for these packs by default (`vr_texture_smooth 2`'s path, per pack) | nothing | below AD's (no learned depth: flat panels stay flat) | half a day to a day, no dependency |

  Recommendation (conservative): D first (cheap, nothing to install, every player gets it); A when he wants AD's look on
  Dwell and Quoth (one Setup step, no install for players). B only if a Setup step is unwanted; C as a stopgap.
- **Reliefs for Dwell and Quoth: option A built (his choice, 2026-11-21; branch `agent/reliefs`, ROUND25.md "reliefs").**
  `Misc/quakevr/pbr_export_onnx.py` exported PBRify 1.7.2's two CC0 models once to ONNX (1.6 MB each, in
  `Installer/src/QuakeVR.Installer.Core/Reliefs/Models/`; parity of the whole generator on 40 AD textures: mean texel
  difference 0.0001, worst 1). Setup's "Make reliefs for Dwell and Quoth" (Installer/README.md, "Reliefs for Dwell and
  Quoth"; on wherever either is there) runs them by onnxruntime (DirectML, CPU fallback) over the Dwell and Quoth folders
  the game plays, writing AD's format into `<Quake VR folder>\reliefs\textures\pbr` (the hash names and a
  `provenance.txt`); only the missing ones, again at every Setup run; the uninstaller removes it. The engine appends
  `<last basedir>/reliefs` at the bottom of the base search path (`VR_AddReliefsPath`, vr_pbr.cpp) and reads its
  provenance too: the lookup itself unchanged, as planned. Measured: 1768 reliefs for 2323 textures in 115 s (DirectML),
  82 MB; d1m1 draws 168 of them, kellbase1 60, at AD's generated strength. Setup grows by 9.7 MB (61.5 to 71.3 MB).

## 10. Open questions for Vittorio (real decisions only)

1. **Copper rules in Copper maps: whose weapons and monsters?** Proposal, as for AD: **our weapons and our vanilla
   monsters everywhere** (VR melee, limbs, knockdowns), with Copper's keys, spawnflags and numbers (health, attacks,
   aim) applied to our monsters in Copper maps; Copper's world logic (triggers, doors, trains, targets) runs Copper's
   code in Copper maps. Alternative: Copper's own monster code in Copper maps (exact behaviour, loses our VR monster
   features).
2. **Source strategy**: ask Fairweather/Khreathor for Dwell's source and their OK (recommended), rebuild on Copper
   meanwhile (section 1.2). Do you want to write to them, or should we draft the message?
3. **Name clashes**: `item_artifact_wetsuit` (Hipnotic's, AD's, Dwell's) and `monster_morph` (Rogue's, Dwell's,
   unplaced). Proposal: in Copper/Dwell maps Dwell's own (the `weapon_mjolnir` precedent); elsewhere the current owner;
   Dwell's spawnable anywhere as `dw_<name>` via the toolgun.
4. **Dwell content outside Dwell maps**: Dwell's monsters and weapons carry their Dwell/Copper behaviour wherever they
   spawn (AD's decision 9), or adopt the map's ruleset? Proposal: their own (they are content, not rules).
5. **Installer default timing**: on by default from P1b's build (proposed), or immediately (385 MB of maps that do not
   play yet)?
6. **Run S1-S3 first** (the simplification before Dwell; about 4-6 agent-days), or start P1a in parallel?

---

## 11. Vittorio's decisions (2026-11-19)

He answered section 10 with "Yes, all six": every proposal is taken.

| # | Question | Decision | Phase |
|---|---|---|---|
| 1 | Weapons and monsters in Copper maps | **Ours everywhere** (our weapons, our vanilla monsters with VR melee, limbs, knockdowns), with Copper's keys, spawnflags and numbers applied to them in Copper maps. Copper's own code runs only the world logic (triggers, doors, trains, targets). | P1b |
| 2 | Source | Vittorio contacts Fairweather/Khreathor for Dwell's source and their OK. Meanwhile Dwell is rebuilt on Copper's public source; the decompile is a behaviour reference only, never shipped. | P2 |
| 3 | Clashes `item_artifact_wetsuit`, `monster_morph` | **Dwell's own in Copper/Dwell maps**, the current owner (Hipnotic's/AD's, Rogue's) elsewhere; Dwell's version spawnable anywhere as `dw_<name>` by the toolgun. | P2/P3 |
| 4 | Dwell content outside Dwell maps | **Keeps its own Dwell/Copper behaviour** (content, not rules: AD's decision 9). | P2 |
| 5 | Installer default | **On by default from P1b's build** (when Dwell's maps play), not before. | P1b |
| 6 | Order | **S1-S3 land before Copper's import** (P0.5 before P1a). His standing direction with it: fewer special cases and bespoke branches. | P0.5 |

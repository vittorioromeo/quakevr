# Dawn of the Machine (MG3): native Quake VR port plan

Status: plan (2026-10-06); phase A (M3-01..04) built the same day (EXPANSIONS.md); phase B (M3-05..10) on 2026-10-07 (ROUND21.md). Vittorio's decisions
(the last section) override section 4's defaults and the tasks that assumed them (M3-11, M3-12). Target: **full native Quake VR gameplay** (VR hands, weapons, melee, holsters, magazines,
transition/save state), not compatibility mode. Campaign id **5** (`vr_campaign`/`vr_campaign_schema`), `nativeReady`
false at `Quake/vr/vr_gamedir.cpp:375` until the acceptance in section 5 passes.

Sources (read-only): official `quakec_mg3` (65 `.qc`, snapshot `634eefa`, SHA256 in EXPANSIONS.md) and the owned
Steam `rerelease/mg3/pak0.pak` (entity lumps scanned read-only; no asset copied). Licence/header rules:
OFFICIAL_QC_SOURCE.md (keep the GPL headers of adapted files, record the source in CREDITS.md).
Precedents: Dopa (ready, solo), MG1 hub/runes (`QC/vr_mg_hub.qc`), MG1 Horde (`QC/vr_mg_horde.qc`), shared world/
trigger/activation shipments (`QC/vr_mg_*.qc`), Hipnotic/Rogue monster and weapon ports.

Inventory scripts and raw tables: `qvr-kit/scratch/mg3plan/` (`inv.py`, `classes.txt`, `missing_per_map.txt`,
`routes.txt`, `deferred.txt`, `srcloc.txt`). M3-01 turned `inv.py` into the committed checker
`Misc/quakevr/check_mg3_entities.py` (per-map missing classnames and keys; run it after each task).

---

## 1. Inventory

### 1.1 Maps (22 BSPs: 19 playable SP, 1 DM, 2 brush models)

| Map | Title | Role | Exits (trigger_changelevel) | Missing ents |
|---|---|---|---|---:|
| start | The Thirtieth Passing | new game, 4 skill brushes (`trigger_setskill` 0..3) | map1 (NO_INTERMISSION, `endtext $mg3_start_intermission`) | 0 |
| map1 | Through the First Arch | ch.1 | map2, secret1 | 115 |
| map2 | Dawn of the All Mother | ch.1 (Super Axe) | map2b | 94 |
| map2b | Into the Abyss of Time | ch.1b (MG3 lava men) | map3 | 91 |
| map3 | Veils of the Old Ones | ch.2 | map4, secret5 | 48 |
| map4 | Of Shattered Minds | ch.2 (hell-knight head puzzle) | map5, secret6, hub | 83 |
| map5 | Requiem | ch.3 | map6, secret3 | 37 |
| map6 | The Matriarch of Decay | ch.3 (stock `monster_oldone`, axe button) | map7, secret4 | 43 |
| map7 | Revelation of the Arcane | ch.4 (sacrifice, axe buttons) | map8, hub | 69 |
| map8 | The Sacrifice | ch.4 finale (11 sacrifices, 18 axe buttons, rune 4) | hub (endtext) | 140 |
| hub | The Runic Nexus | rune hub, skill/Bloody Nightmare buttons, lore, heal | map1, map3, map5, map7, secret2 (-> boss2 under BN new game) | 36 |
| secret1 | The Sundered Column | rune 1 | hub, secret1 | 23 |
| secret2 | The Nightmare Machine | final gauntlet (60 explosion repeaters) | boss | 178 |
| secret3 | Mist of Torment | rune 3 (sigil bit 4) | hub | 16 |
| secret4 | The Crooked Village | secret, bloody SSG | map7 | 49 |
| secret5 | House of Delusion | rune 2, axe buttons | hub | 121 |
| secret6 | The Haunted Tower | secret (31 silent teleports) | map5 | 52 |
| boss | The Sleeper Awakens | Chthon finale (`monster_boss_final`) | hub; QC: -> map1 for BN new game | 131 |
| boss2 | A Dancing God | Shub finale (Bloody Nightmare only) | none; QC ends -> start/credits | 71 |
| dm1 | The Crooked Village | deathmatch (+ KEX bot `.nav`) | none | 0 |
| b_shot2, b_splash | brush models | precache only | - | 0 |

Hub worldspawn is `worldtype 0` in the shipped BSP, so official `WORLDTYPE_HUB(3)` resets in `DecodeLevelParms`/
`SetChangeParms` do **not** fire for MG3 (unlike MG1): inventory carries through the hub. Verify in task M3-06.
Monster load: map2 304 monsters, secret2 281, map1 254, map6 256; deferred (bit 4) 1,353 in total (see `deferred.txt`).

### 1.2 Classnames

156 distinct classnames; **109 already exist** in VR QC; **47 are missing** (the 2026-10-05 audit listed 67; the shared
world/trigger shipments since then supplied 20: ambient_generic, func_bob/toss/hurt/explode, info_fog, misc_corpse/
model/rope, particle_*, rotate_object_continuously, trigger_activate_coop_spawns/counter_timed/fog_transition/
lightning/repeater/screenshake/sound). **1,397 missing entity placements** in total. None of the 47 exists in id1
or in an MG1 map; all are defined in MG3 source (`trigger_multitouch` also exists in MG1 source, unused by MG1 maps).

| Group | Classname (placements / maps) | MG3 source |
|---|---|---|
| Upgrades | item_upgrade_health 16/15, _shells 15/15, _nails 15/15, _rockets 13/13, _cells 10/10 | mg3_upgrades.qc:337.. |
| Items | item_armor_shard 413/13, item_artifact_lavasuit 21/1 (boss), item_draught_insight 5/2, item_draught_stupor 5/3, item_head_hellknight 1 (map4) | mg3_items.qc, items.qc:1503 |
| Weapons | weapon_bloody_sg 1 (map1), weapon_bloody_ssg 1 (secret4) | mg3_items.qc:331/349 |
| Monsters | monster_knight_infected 133/6, monster_ogre_rocket 126/14, monster_demodog 107/9, monster_orb 48/6, monster_ranged_knight 43/5, monster_hell_knight_infected 31/8, monster_army_infected 27/2, monster_enforcer_infected 23/6, monster_ghost 7/2, monster_slime 6/4, monster_super_shambler 5/4 | monsters/mg3_*.qc, ogre.qc:558, tarbaby.qc:392 |
| Bosses | monster_boss_final 1 (boss), info_boss_teleport_first 4 / _second 4 / _boss 1, trigger_boss_teleport 1; monster_oldone_new 1 (boss2), info_szombie_spawn 36, func_breakable 8 | boss_final.qc, mg3_oldone_new.qc:1251, mg3_shub_zombie.qc:529 |
| Sacrifice | misc_sacrifice 12/2, trigger_sacrifice_counter 1 (map8) | mg3_sacrifice.qc, mg3_sacrifice_triggers.qc |
| Progression triggers | trigger_rune_relay 17/8, trigger_rune_counter 9/6, trigger_bloodynightmare_relay 10/2, trigger_relay_setskill 5 (hub), trigger_lore 35/8, trigger_music 1, trigger_heal 1 | mg3_triggers.qc, triggers.qc:743 |
| Map triggers | func_axe_button 33/4 (map6/7/8, secret5), trigger_teleport_silent 34/2, trigger_door_relay 31/3, trigger_explosion_repeater 60 (secret2), trigger_health_relay 8/4, trigger_multitouch 6/2, trigger_always 6/4 | buttons.qc:258, mg3_triggers.qc, triggers.qc |

**Existing classnames whose MG3 behaviour differs** (present in VR QC, so they spawn, but not with MG3 semantics):
`weapon_mjolnir` (1, map2: MG3 Super Axe, not Hipnotic Mjolnir), `weapon_laser_gun` (4: map2b/map4/map7/secret3),
`monster_lava_man` (2, map2b: MG3 lavaman.qc, not Rogue's), `item_sigil` (4: MG3 runes 1/2/4/8; map8 sigil
spawnflags 136), `monster_oldone` (map6), stock monsters (MG3 deltas: infected flag, Bloody Nightmare attacks,
hanging zombie, corpse frames; diff sizes ogre 306, tarbaby 171, enforcer 106, shalrath 89, shambler 74 lines),
`trigger_changelevel` (BN redirect hub->secret2 => boss2, boss -> map1 NG+), `func_button` (`func_axe_button` base),
`combat.qc` (244 diff lines: health_target, infected kill accounting, BN 80% player damage, axe-button filter),
`ai.qc` (aggro_target), `client.qc` (547: parms, BN reset, endings), `weapons.qc` (555: bloody, Mjolnir, impulses).

**Missing entity fields** (warned/dropped today): `health_target` 14 (stock monsters + super shambler: fire a target
at a health threshold), `aggro_target` 8 (wake a named group), `tele_target`/`wave1..3` (boss_final), `wave2`
(oldone_new), `fog_sky_factor` 2 (worldspawn dm1/secret4), editor noise (`comment`, `dirt`, `property 1`).

### 1.3 Owned data the port needs (from the user's PAK, never shipped)

New models: `ogre_rocket`, `dog_explosive`, `rknight`, `shambler_blood`, `teleporter_eye(_blink)`, `lavaman`,
`lasrspik`, `w_spike2`, weapons `v/g_hammer` (+`v_hammer_glow`), `v/g_laserg`, `v/g_bloodshot`, `v/g_bloodshot2`,
items `armorshard`, `lavasuit`, `item_h_hellkn`, `item_h_player`, `diamond(_trail)`, `gold_ring`, `onyx_ring`,
`backpack*`, decorative (`angel_statue*`, `cat_*`, `player_hanging*`, `mg_tree0x`, `mg1_rune1..6`, `ropex`, `candle`,
`flame3`). KEX-only companions: `*.md5mesh/.md5anim`, `*_00_00.lmp/.png/.tga`, `tactile/**` haptics, `wwheel.txt`,
`bots/navigation/dm1.nav`, `gfx/weapons/ui_h_weapon_*`. Sounds: `infected/`, `orb/`, `rknight/`, `rogre/`, `horde/`,
`hipweap/`, `pendulum/`, `armagon/`. Music: `rerelease/mg3/music` (already played via `vr_music.cpp`).

---

## 2. New gameplay content and how it maps to Quake VR

### 2.1 Persistent state (the parm collision)

Official: `parm10..14` = upgrade bitmasks (health/shells/nails/rockets/cells, one bit per map:
map1..map8, secret1..6, map2b = UPGRADE14), `parm15` = bloody bits (BLOODY_SG/BLOODY_SSG). VR already owns parm10..16
(second hand, holsters) and all extended slots 17..50 (`Quake/vr/vr_progs.hpp:14-16`: hands, holsters, weapon
instances, magazines, mission-pack ammo, flashlight). **Never copy the official assignments.**

Plan: extend the engine's named extended block by 6 slots (`lastExtSpawnParm` 50 -> 56: `parm51` health mask,
`parm52` shells, `parm53` nails, `parm54` rockets, `parm55` cells, `parm56` bloody bits) or, equivalently, a named
`mg3_*` global block the engine copies with the ext parms; same code path for changelevel carry, save/load and
`map`/new-game reset. Save compatibility: older saves lacking the slots load as 0 (no MG3 save exists yet); bump the
schema only for campaign 5. QC accessors `MG3_UpgradeMask(type)` / `MG3_SetUpgradeMask(type, v)` replace every
official `parm10..15` reference.

Reset rules (official, to reproduce): `map` (new campaign) clears everything; changelevel keeps upgrades even when
`SetNewParms` runs (death, DM, hub-type reset), because official `SetNewParms` never touches parm10..15; Bloody
Nightmare's DecodeLevelParms strips inventory to axe/shotgun (+Mjolnir, +SSG if bloody SSG, + best armour) and
`skill != 3` clears BLOODY_NIGHTMARE_ACTIVE. VR hands/holsters must be rebuilt consistently with that strip.

serverflags (campaign-scoped, the VR id1 rune logic in `QC/client.qc:604, 1003, 1263` must be gated off for MG3):
runes 1/2/4/8, BLOODY_NIGHTMARE_ACTIVE 64, _DISCOVERED 128, _NEWGAME 256, TETTE 512. MG3 disables the rune start-spot
logic (commented out in its client.qc); VR's `info_player_start2` branch must not run for campaign 5.

### 2.2 Capacities (upgrades)

Official bases: health 50 (all skills, non-DM: `BASE_HEALTH`, client.qc:196/874), shells 50, nails 100, rockets
20, cells 100; +10 per collected map bit; pickup also replenishes. VR caps are hard-coded today (`bound_other_ammo`
QC/items.qc:980, `ammo_touch`, `T_Heal` 250 mega cap, `SetChangeParms` carry, `VR_HGREN_MAX_ROCKETS` vr_grenade.qc:877,
backpacks, magazine reserve logic). Add per-player helpers `VR_MaxAmmo(e, AMMO_x)` / `VR_MaxHealth(e)` used
everywhere (stock campaigns return today's constants: zero behaviour change elsewhere), MG3 returns base + 10*bits.
Mission-pack ammo (multi-rockets, lava nails, plasma) does not occur in MG3.

### 2.3 Weapons

| MG3 weapon | Official behaviour | VR mapping |
|---|---|---|
| Super Axe (`weapon_mjolnir`, IT_MJOLNIR 128, map2) | melee 40 dmg, zombie-specific damage, hit debounce, finishing gibs; a second hit fires a lightning burst costing 15 cells; pickup can silent-teleport (spawnflag); also the only weapon besides the axe that opens `func_axe_button` | reuse `WID_MJOLNIR` (QC/vr_defs.qc:110) with an MG3 behaviour branch; physical swing damage through existing melee (per-hand attribution, no melee tuning); burst on the second qualifying hit within the debounce window; MG3 `v_hammer.mdl` needs a VR offset entry (free `vr_wofs` slots 24-31) unless the existing Mjolnir model/offset is reused; holsterable like the Hipnotic one |
| Laser cannon (`weapon_laser_gun`, IT_LASER_CANNON 8388608) | Hipnotic-style bouncing bolts, 1 cell/shot (mg3_weapons.qc:155) | reuse `WID_LASER_CANNON` (12); verify MG3 deltas vs Hipnotic; resolve `progs/v_laserg.mdl`/`lasrspik.mdl` from the MG3 PAK so no Hipnotic install is required |
| Bloody shotgun / SSG (`weapon_bloody_sg/ssg`) | only collectible in a Bloody Nightmare new game; sets parm15 bits; model swap `v_bloodshot(2)`; bloody SG refires in 0.28 s instead of 0.5 s; survives BN strip | a per-campaign "bloody" flag on the SG/SSG weapon instance (persisted via 2.1) selecting the MG3 model/skin and a faster pump/refire; holster/magazines unchanged |
| Axe / stock weapons | MG3 impulses 222-228 (BN toggles, debug) | VR has no impulses for these; expose the useful ones in Debug > Tests |

Item bits 128 and 8388608 are shared with Hipnotic: keep every MG3 meaning behind `MG3_Campaign()`.
`func_axe_button` (33): official filter is `attacker.weapon == IT_AXE || IT_MJOLNIR` (combat.qc:172). VR must test the
**hand that hit** (axe/Mjolnir instance in that hand), not the selected weapon; decide whether crowbar/swords count (Q1).

### 2.4 Monsters

MG3 is the first official campaign with new archetypes; each needs the VR registration set (PositionalHead /
`VR_Decap_HeadModel`, ragdoll seed tables, box3d hit-zone table, hulls, parry, knockdown, grapple, small gibs,
dummy/test spawn) and FGD entries. Stock-frame models can copy their base monster's tables; new models need new
hit zones and a rig check.

| Monster | Model | Behaviour (official) | VR notes | Size |
|---|---|---|---|---|
| infected army/knight (27/133) | stock soldier/knight | on death turns into a zombie (modelindex swap); kill counted once after transformation | death -> zombie swap must bypass ragdoll/decap, keep one kill; existing zombie VR tables | M |
| infected enforcer/hell knight (23/31) | stock | turn into a demon/fiend; hknight corpse rises | same, demon tables; hknight offset fix | M |
| monster_ogre_rocket (126) | ogre_rocket.mdl (new) | ogre firing rockets | copy ogre tables if frames match; rocket parry/deflect | M |
| monster_demodog (107) | dog_explosive.mdl (new, 915 v) | kamikaze explosion + grenades | dog tables; explosion on grab/throw | M |
| monster_ranged_knight (43) | rknight.mdl (new) | ranged knight projectiles | knight tables if frames match; sword/weapon drop? | M |
| monster_orb (48) | teleporter_eye_blink.mdl | flying eye, projectile/teleport | no ragdoll (rigid), flying grapple rules | M |
| monster_super_shambler (5) | shambler_blood.mdl (1,076 v, 96 frames) | 2000 hp, lightning + plasma, child/ghost attacks, health_target | shambler tables only if frame order matches; new hit zones likely | L |
| monster_ghost (7) | player-style ghost | phasing ghost (map1, secret5) | non-solid/phase rules vs hands | M |
| misc_sacrifice (12) + counter | | map8 sacrifice victims counted | grab/kill by hand must count | S |
| monster_slime (6) | tarbaby variant | slime that spawns tarbabies | tarbaby tables | S |
| monster_lava_man (2) | lavaman.mdl | MG3 lava man (not Rogue's) | campaign-scoped branch of the existing class | M |
| stock deltas | | Bloody Nightmare attacks (enforcer alt laser, shalrath trail, shambler, wizard, hknight), hanging zombie, health_target/aggro_target | apply on existing VR monsters, gated to campaign 5 | M |

**Bosses.** `monster_boss_final` (boss.bsp): Chthon, 12000 hp, phases, waves (`wave1..3`), teleports between
`info_boss_teleport_first/second/boss`, `trigger_boss_teleport` moves the player, 21 lavasuits on the map, music, lore.
`monster_oldone_new` (boss2.bsp): Shub, phases, child spawners (spammer/swiper/blaster/vortex/eye), 36
`info_szombie_spawn` shub zombies, 8 `func_breakable` lowering ceilings (boss-specific, not ordinary breakables).
Both end the campaign (boss -> hub, NG+ map1 under BN; boss2 -> start + credits).

### 2.5 Items and powerups

Armor shard (413: small armour, likely touch pickup; 77 on map8), lavasuit (boss only: lava immunity, HUD timer),
draughts insight/stupor (vertical teleport "rings" with effects; VR teleport comfort fade applies),
`item_head_hellknight` (map4 head puzzle + hub Bloody Nightmare toggle), upgrades (2.2), safety ammo respawn
(official items.qc change), coop item respawn per player (manifest define).

### 2.6 Hub, episodes, finale

Linear chapters map1 -> map8 with returns to the hub (map4, map7, map8, secret1/3/5 exit to hub; hub re-enters
map1/3/5/7). Runes: secret1 (1), secret5 (2), secret3 (4), map8 (8); hub `trigger_rune_relay`s light each, the
`trigger_rune_counter` (count 4) opens `exit` -> secret2 -> boss. Hub skill buttons (`trigger_relay_setskill` 0..4;
4 = Bloody Nightmare) and BN relays. Endings: boss (Chthon) -> hub/map1; Bloody Nightmare new game: hub -> boss2
(Shub) -> `menu_credits` + disconnect. VR's native completion menu knows only campaigns 3/4: add 5 (title and
attribution). Achievements (`SVC_ACHIEVEMENT`) have no VR equivalent: drop.

### 2.7 Localization

246 `$mg3_*`/QC/BSP identifiers, all resolved by the updated rerelease English table through the existing
`LOC_LoadFile` layering (mglocalize). Lore (`trigger_lore`, 35) is centerprinted text: needs readable VR placement
and a dwell time. Readiness requires the same language-data gate Dopa uses.

### 2.8 KEX-only features

Measured: **no `dynamiclight` entities in any MG3 BSP**; `_shadow`, `_shadowself`, `_dirt*`, `_phong`, `_surface*`,
`_sunlight*` are compile-time light keys (no runtime cost). Others: `.md5mesh/.md5anim` + `_00_00` skins (KEX
hi-res models: Ironwail loads the `.mdl`; keep that), tactile `.bnvib/.wav` haptics (optional: map the Mjolnir/laser
cues to existing VR haptics), `wwheel.txt` weapon wheel (ignore: VR uses holsters), `dm1.nav` bots (ignore),
achievements (drop), `fog_sky_factor` (map to existing sky fog if supported, else ignore with a note).
No engine work is required for MG3 lights; the MG1 `dynamiclight` no-op remains a separate item.

---

## 3. Risks

- **State ABI**: extending 17..50 touches engine save/changelevel code shared by every campaign; regressions show
  up as lost holster/magazine state. Test stock + Dopa + MG1 carries in the same task.
- **Cap refactor** touches hot pickup paths for all campaigns; must be behaviour-neutral outside campaign 5.
- **Item bit aliasing** (Mjolnir 128, laser 8388608, sigil/serverflag meanings) with Hipnotic/id1 code paths.
- **New models without rigs**: wrong hit zones/ragdolls look broken in VR; frame-order checks needed per model.
- **Performance**: 250-300 monsters per map plus 1,353 deferred spawns, 413 armour shards; VR frame budget and
  ragdoll/gib caps need a measured pass on map2/secret2 (`vr_profile`), after the profiling worker's numbers.
- **Comfort**: boss and draught teleports, silent teleports (31 in secret6), screen-shakes (already vibration),
  lowering ceilings in boss2.
- **Bloody Nightmare** changes behaviour of many stock monsters: high regression surface; keep every branch gated.
- Earlier unattributed rapid-transition crash/stall findings (EXPANSIONS.md) still apply to campaign switching.

## 4. Decisions for Vittorio

1. **Axe buttons**: which VR melee opens `func_axe_button`: only axe + Super Axe (official), or any melee
   (crowbar, swords, fists)? Default if undecided: official (axe/Mjolnir in the hitting hand).
2. **Super Axe lightning burst**: official fires on the second hit; in VR trigger it on a second qualifying swing
   hit within the debounce window (default), or on a hard swing / button?
3. **Base health 50** (official for all skills) and caps 50/100/20/100 + upgrades: keep official (default), or
   VR-tuned bases?
4. **Bloody shotguns**: apply the official faster refire to the VR pump cycle (default), or cosmetic only?
5. **New-model fidelity at readiness**: ship ogre_rocket/demodog/rknight/super shambler/orb with copied/box hit
   zones and ragdolls only where the frame order matches (default), or block readiness on authored rigs/zones?
6. **Readiness scope**: single-player only, like Dopa (default); coop and dm1 later?

## 5. Task breakdown

Each task: ~1 worker run, own worktree, `bash <kit>/build.sh`, headless mock runs with the owned MG3 data
(`vr_campaign_native mg3`, `-nomapindex -noaddons`), a `vr_mg3_test` request in Debug > Tests where useful,
FGD coverage for every new spawn function, GPL headers kept, notes in the current ROUND file and EXPANSIONS.md.
Melee-touching tasks also run `eval.sh` (archived settings; no melee tuning).

### Phase A: foundation

- **M3-01 MG3 scaffold and entity checker.** `MG3_Campaign()` helper, `QC/vr_mg3_*.qc` files in progs.src, unarchived
  `vr_mg3_test` + Debug > Tests page, `Misc/quakevr/check_mg3_entities.py` (owned PAK read-only: per-map missing
  classes/fields, totals). Gate VR id1 rune/start2 logic and Honey flag meanings off for campaign 5.
  Accept: checker prints 47 missing / 1,397 placements; all 22 BSPs load exit 0 with no Host_Error; stock e1m1/hub smoke.
- **M3-02 Extended state slots.** Engine + QC parm51..56 (or named block), accessors, save/load, changelevel, `map`
  reset. Accept: seeded masks survive changelevel, completed save/load, death restart; new campaign clears; stock,
  Dopa and MG1 hand/holster/magazine carry tests unchanged. Dep: M3-01.
- **M3-03 Capacity helpers.** `VR_MaxAmmo`/`VR_MaxHealth` across items/weapons/client/grenades/backpacks/horde.
  Accept: stock caps identical (100/200/100/100, health 100/250); MG3 health 50 base, shells 50 etc.; Dopa/MG1 Nightmare
  50 rules still pass. Dep: M3-02.
- **M3-04 Upgrade items.** `item_upgrade_*` physical pickups, per-map bit, +10 and replenish, localized messages.
  Accept: one of each on map3 raises caps and refills; revisit map: no second award; map2b bit 14; carry through hub
  and save/load. Dep: M3-03.

### Phase B: world and progression

- **M3-05 Map triggers I.** trigger_always, door_relay, teleport_silent, multitouch, explosion_repeater, music,
  heal. Accept: secret6 31 silent teleports land on targets; secret2 60 repeaters fire; checker shows them resolved.
- **M3-06 Map triggers II and fields.** `health_target`, `aggro_target`, trigger_health_relay, trigger_lore (VR-readable
  text), `fog_sky_factor`. Accept: scripted damage to a health_target monster fires once at the threshold (map3);
  aggro group wakes together; lore text resolves localized. Dep: M3-01.
- **M3-07 Items.** armor shard, lavasuit, draughts (VR teleport comfort path), hell-knight head. Accept: shard adds
  armour at the official rate; draught moves the player to its destination; lavasuit protects in lava; map4 head puzzle opens its door.
- **M3-08 Runes and hub.** MG3 `item_sigil` bits, rune relays/counter, hub exits, inventory through hub (verify
  worldtype 0), boss route. Accept: four rune pickups give flags 1/3/7/15 and light the hub; counter opens the exit only
  at 4; secret2 -> boss; save/load keeps runes.
- **M3-09 Start, skill and Bloody Nightmare state.** start skill brushes, hub `trigger_relay_setskill` 0..4,
  bloodynightmare relays, BN serverflags, DecodeLevelParms BN strip rebuilt for hands/holsters, hub -> boss2 redirect,
  player 80% damage. Accept: skill 4 sets ACTIVE|DISCOVERED; changing skill clears ACTIVE; BN transition keeps only
  axe/shotgun(+Mjolnir/bloody SSG/armour) with consistent holsters. Dep: M3-02, M3-08.
- **M3-10 Endings and credits.** boss -> hub/NG+ map1, boss2 -> credits, map8/start endtexts, campaign 5 in the native
  completion menu. Accept: forced boss-kill test reaches each ending; credits menu shows Dawn of the Machine.

### Phase C: weapons

- **M3-11 Super Axe.** MG3 Mjolnir branch on WID_MJOLNIR: damage/zombie/debounce/gibs, lightning burst (Q2), pickup
  silent teleport, MG3 model offset. Accept: scripted swing damage 40 (zombie rule), burst uses 15 cells, gib on
  finishing hit; eval.sh canary unchanged. Dep: M3-01. **Built 2026-10-07** as a weapon of its own (decision 2:
  `WID_SUPERAXE`, MG3's model read in place from the owned pack in any campaign; ROUND21.md, "Dawn of the Machine
  (MG3): weapons").
- **M3-12 Axe buttons.** `func_axe_button` with per-hand filter (Q1). Accept: map6 button opens on axe/Super Axe hand
  hit, not on a shotgun blast or a non-axe hand; map8 18 buttons all reachable by test. Dep: M3-11. **Built
  2026-10-07** with decision 1 (any melee blow, thrown things too; ROUND21.md).
- **M3-13 Laser cannon (MG3).** behaviour deltas vs Hipnotic, MG3-PAK model resolution, no Hipnotic install needed.
  Accept: map2b laser pickup works with Hipnotic absent; bolts bounce/damage as source. **Built 2026-10-07**: the same
  weapon (`WID_LASER_CANNON`), MG3's 15/20 bolt damage in campaign 5 (ROUND21.md).
- **M3-14 Bloody shotguns.** BN-only pickups, persistent bits (M3-02), model/skin swap, refire (Q4).
  Accept: BN new game collects bloody SG on map1, it survives changelevel/save and BN strip; non-BN game: pickup inert.
  **Built 2026-10-07** (decision 4: refire 0.28 s, 28 pellets; drawn as Quake VR's shotguns, their skins a BACKLOG
  item; the BN strip is M3-09's; ROUND21.md).

### Phase D: monsters (registration per archetype: hit zones, decap head, ragdoll seeds, hulls, parry, knockdown, grapple, gibs, dummy, FGD)

- **M3-15 Infected group.** generic + army/knight/enforcer/hknight infected, transformation, single kill count.
  Accept: killing each infected spawns the right zombie/demon, kills +1 once; map2 counts match the checker. **Built
  2026-10-07**: a full model change (not upstream's modelindex swap), any campaign, the lying death knights (ROUND21.md,
  "Dawn of the Machine (MG3): monsters").
- **M3-16 Rocket ogre.** Accept: spawns on map1, fires rockets, rocket parry, decap/ragdoll sane; frame-order check recorded.
  **Built 2026-10-07**: its own rig and head zone, any campaign with MG3's data, training dummy 19 (ROUND21.md).
- **M3-17 Demodog.** Accept: kamikaze explosion damages player and neighbours; grenades; grab/throw explodes per source.
  **Built 2026-10-07**: its own rig, any campaign with MG3's data, training dummy 20; every death spills its grenades
  (beheaded or cut: a ragdoll, else gibs) (ROUND21.md).
- **M3-18 Ranged knight.** Accept: projectile attack, parry, hit zones on rknight.mdl.
  **Built 2026-10-07**: its own rig and head zone, the death knight's tables, any campaign with MG3's data, training
  dummy 21 (ROUND21.md).
- **M3-19 Orb.** Accept: flies/teleports, projectiles, death; no ragdoll; grapple rules. **M3-19..23 built 2026-10-07**
  (ROUND21.md, "Dawn of the Machine (MG3): monsters II").
- **M3-20 Ghost, sacrifices, slime.** monster_ghost, misc_sacrifice + counter, monster_slime. Accept: map8 sacrifice
  count reaches its target; slime spawns tarbabies; ghost phases.
- **M3-21 MG3 lava man.** campaign-scoped branch of the existing class. Accept: map2b lava men behave per MG3; Rogue r2m* lava man unchanged.
- **M3-22 Super Shambler.** 2000 hp, lightning/plasma, children, health_target. Accept: map2 encounter scripted to
  death; children cleaned up; save/load mid-fight.
- **M3-23 Stock-monster deltas and BN behaviours.** MG3 changes in ogre/tarbaby/enforcer/shalrath/shambler/wizard/
  hknight/zombie (hanging), BN branches. Accept: per-monster BN test attack; stock campaigns unchanged (e1m1, hip1m1, r1m1 smoke).
- **M3-24 Chthon finale I.** boss_final spawn, phases, waves, teleport points, lavasuits. Accept: boss.bsp loads 0
  missing; forced phase changes teleport the boss; waves spawn.
  **Built 2026-10-07** (`QC/vr_mg3_chthon.qc`; ROUND21.md "Dawn of the Machine (MG3): the Chthon finale"): Debug spawner 50,
  training dummy 40, `vr_mg3_ctest`; boss.bsp 0 missing.
- **M3-25 Chthon finale II.** trigger_boss_teleport (player, comfort fade), music, kill -> ending. Accept: scripted
  fight reaches the hub/NG+ route. Dep: M3-24, M3-10.
- **M3-26 Shub finale I.** oldone_new phases and child spawners. Accept: boss2 loads 0 missing; each child type spawns and dies.
  **Built 2026-10-07**: `QC/vr_mg3_shub.qc`, any campaign with MG3's data (Debug spawner 60/62/63, training dummy 50/51),
  `vr_mg3_shubtest` (ROUND21.md, "Dawn of the Machine (MG3): the Shub finale").
- **M3-27 Shub finale II.** shub zombies (36 spawns), 8 func_breakable ceilings, ending -> credits. Accept: ceilings
  lower on phases, never trap the player outside source behaviour; death of Shub reaches credits. Dep: M3-26, M3-10.
  **Built 2026-10-07**: `QC/vr_mg3_shub_zombie.qc`; the 8 "ceilings" are pillars round Shub that any damage sinks 20
  units (upstream), so her attacks bring them down over the fight; boss2 0 missing (ROUND21.md, "the Shub finale").

### Phase E: acceptance and readiness

- **M3-28 Performance pass.** map2/secret2/map1 `vr_profile` with full monster counts and ragdoll caps; fixes or
  documented budgets. Run after the profiling worker; `--exclusive` for timings.
- **M3-29 Full-campaign route sweep.** all 22 BSPs 0 missing classes/fields; every normal/secret route; runes;
  hub returns; both endings; carry/save/load/death at each hop; language-gate checks.
- **M3-30 Readiness flip and docs.** `nativeReady` true for single-player (Q6), solo guard as Dopa, INSTALL/README/
  CREDITS/EXPANSIONS, Debug menu tests, human VR QA checklist (Super Axe contacts, axe buttons, lore readability,
  teleports/ceilings comfort, new monsters' hit zones).

Order: A (01-04) -> B (05-10, 05-07 can run in parallel) -> C (11-14, parallel to D) -> D (15-23 parallel by archetype,
24-27 after 10) -> E. About 30 worker runs.

**MG3 is "ready" (ungated)** when: every classname/field is resolved on all 22 BSPs; each archetype has spawn/attack/
death tests and VR registration; upgrades, runes, Bloody Nightmare and bloody weapons survive changelevel, save/load,
death and hub, and reset on new campaign; both endings and credits work; stock/Dopa/MG1 regression and smoke runs
pass; performance on the heaviest maps is within budget; language data is gated; and Vittorio's human VR QA of the
items in M3-30 passes.

## Decisions (Vittorio, 2026-10-06)

1. **Axe buttons** (`func_axe_button`): any melee blow opens them: axe, Super Axe, fists, swords, gun butts, held or thrown props. Not guns.
2. **Super Axe**: a separate weapon from Hipnotic's Mjolnir, so both coexist (own WID/IID, model, holster/weapon settings slot). MG3 maps' `weapon_mjolnir` is remapped to the Super Axe entity on map load (MG3 campaign only). The lightning burst stays "second hit on the same target within a short window", with a more lenient window for VR (tunable cvar).
3. **Progression** (50 base health, low ammo caps, +10 upgrades): only in MG3 maps (campaign 5); everywhere else unchanged (already so after M3-03).
4. **Bloody shotguns**: implemented as intended (faster refire / more pellets). Backlog TODO: distinct bloody textures/skins.
5. **New MG3 monsters** (infected variants, demodog, rocket ogre, super shambler, ranged knight, orb, ...): generally available whenever MG3's data is detected and readable — spawnable in any map (Debug menu monster spawner, training dummy types) — each with the full Quake VR treatment: ragdoll rig, hit zones, decapitation/head pops, limb gore, knockdown/parry/grapple registration as for the stock monsters.
6. **Release scope**: single-player first (co-op and dm1 later).

**General principle**: Quake VR's QuakeC is an agglomeration of all official expansions (and popular mods): every official expansion must play as intended, and expansion-specific items, monsters and weapons must also be usable in any other level when their data is available.

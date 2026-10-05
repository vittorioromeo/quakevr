# Official expansion support audit

Audit date: 2026-10-05. Documentation only; no engine/QC implementation and no new cvars.
Baseline: `agent/expansionaudit`, using the coordinator's already built worktree.

## Findings and measured checks

Both mission packs are currently mandatory for the merged VR progs, including when playing the base campaign.
`Quake/vr/vr_gamedir.cpp` automatically layers installed `hipnotic` and `rogue` below `quakevr`, but detects only
whether their directories exist. `QC/world.qc` unconditionally precaches their models; `QC/weapons.qc:W_Precache`
unconditionally precaches their sounds. The launcher and installation documentation describe the packs as optional,
which does not match the measured behavior.

Owned original and rerelease PAKs were hardlinked into separate `audit-local/<case>/id1` directories; only `pak0.pak`
and, where present, `pak1.pak` were mounted. Each case mounts this worktree's `quakevr`. Mission-pack PAKs were
added to separate local directories for the corresponding cases. No Steam data was written or distributed.
Headless mock runs used `-nomapindex -noaddons -nosteam -nogog -noegs -noconfigwrite`, disabled autosaves, and
ended with `toggleconsole;disconnect;wait;quit`. Failed-map runs timed out because error recovery opens the hub;
post-map echo markers alone are not evidence that a map loaded.

| Isolated data | Map | Result |
|---|---|---|
| Original id1 only | e1m1 | `Host_Error: Mod_LoadModel: progs/playham.mdl not found`; terminated after 30 s |
| Rerelease id1 only | e1m1 | Same missing `playham.mdl`; terminated after 30 s |
| Original id1 + Hipnotic | e1m1 | Missing `progs/rockup.mdl`; terminated after 10 s |
| Original id1 + both mission packs | e1m1 | Exit 0, 0 Host_Error, 0 missing spawn functions |
| Rerelease id1 + both mission packs | e1m1 | Exit 0, 0 Host_Error, 0 missing spawn functions |
| Rerelease + both packs + dopa below VR | e5m1 | Exit 0, **4 dropped entities**, 3 unknown-field warnings |
| Rerelease + both packs + mg1 below VR | hub | Exit 0, **94 dropped entities**, 4 unknown-field warnings |
| Rerelease + both packs + mg3 below VR | map1 | Exit 0, **115 dropped entities**, 4 unknown-field warnings |

The expansion runs deliberately keep VR's progs last/highest. They demonstrate the native-support gap, not
acceptance of gameplay. Dopa's four dropped entities are three `func_explode` and one `info_fog`. MG1 hub loses
its final exit, return spawnpoints, rune indicators, rotating parts and effects. MG3 map1 loses 13 infected knights,
9 rocket ogres, 9 ranged knights, 4 infected hell knights, 4 ghosts, 10 orbs, upgrades, teleporters and rune relays.

The kit's ordinary qbase is not an original-id1-only fixture: it includes `id1/pak10.pak` through `pak12.pak`,
and existing mission-pack directories are auto-mounted even with only `-game quakevr`. The first exploratory run
was therefore discarded as dependency evidence. It also hit an unrelated map-index libcurl worker crash despite
`-noaddons`; all valid measurements above used `-nomapindex`.

Static resource lookup against original id1 plus this worktree's loose VR assets finds these absent, unconditional
worldspawn models: Hipnotic supplies `progs/playham.mdl`; Rogue supplies `progs/rockup.mdl`, `rockup_d.mdl`,
`lspike.mdl`, `shield.mdl`, `p_shield.mdl`, and `plasma.mdl`. VR already supplies several other expansion view models,
so checking only `v_hammer.mdl` or `v_lava.mdl` would incorrectly report that the packs are available. Required
sounds include Hipnotic's `hipweap/*` and Rogue's `lavagun/snail.wav`, `plasma/*`, `shield/hit.wav`, `belt/use.wav`.
Sound precache failures and fatal missing-model failures must be distinguished.

## Native gameplay scope

Read-only approved sources: `Misc/quakevr/scratch/official-rerelease-qc` in the author's checkout. Its README says
Dopa shares the MG1 codebase. The source audit covered all 47 MG1 and 65 MG3 `.qc` files, their `progs.src` manifests,
and the entity lumps of every BSP in the owned rerelease dopa/MG1/MG3 PAKs. Disabled `func_fade.qc` is not a shipping
requirement merely because it exists in the source tree. All map/class counts below include campaign, secret, DM
and horde maps, including alternate `start` maps; they are not counts of campaign levels.

| Pack | BSPs | Distinct map classnames | Classnames absent from VR QC |
|---|---:|---:|---:|
| dopa | 13 | 68 | 2 |
| mg1 | 25 | 128 | 41 |
| mg3 | 22 | 156 | 67 |

Class presence is only a lower bound. Existing names such as `monster_boss`, `monster_lava_man`, `weapon_mjolnir`,
`weapon_laser_gun`, `func_button`, `trigger_changelevel`, `item_sigil` and standard monsters have different behavior
in the official sources. Renaming or aliasing unknown entities cannot supply native support.

- **Dopa:** port `func_explode` and the `info_fog` fields/handling. Verify original campaign exits, secrets and
  level transitions with MG1 shared behavior; existing documentation saying it simply plays with VR gameplay
  overstates the current entity coverage.
- **Shared MG1/MG3 map framework:** `frametick.qc`, `math.qc`, fog transitions, dynamic lights/light ramps, gas flames,
  bobbing and tossed/debris brushes, continuously rotating geometry and axis entities, corpses/models, ambience,
  particles, sound/lightning/repeater/counter/fade/freeze triggers, delayed trigger cancellation, target changes,
  and coop spawn/checkpoint activation. Preserve gameplay targets when adapting visual effects for VR.
- **Shared existing-monster changes:** deferred/triggered monster spawning (`MONSTER_SPAWNED=4`, angry=8, tfog=16,
  waitwalk=4096), future-monster counting, activation/fading, AI/liquid behavior and killable Chthon spawnflags.
  These flags and common function bodies need campaign gating; do not globally replace VR monster startup.
- **MG1 progression:** `map_specific/hub.qc` opens `hub_trigger_changelevel` only with all sigils; return positions
  require `info_player_start_hub`. `items_runes.qc` drives rune indicators. All four `mge2m2_*` entities implement
  electrode/rune-egg puzzles. Carry/reset inventory and runes according to official transitions. Horde has its
  own manager, spawnpoints, waves, keys, currency/item/powerup logic and coop revival; track it as a separate feature.
- **MG3 progression:** `mg3_upgrades.qc` gives persistent health/shell/nail/rocket/cell capacity upgrades (currently
  +10 per upgrade; base health 50). Map flags prevent collecting the same upgrade again; include `map2b` and six
  secret maps. Current VR ammo caps/healing rules must respect these capacities. `mg3_items.qc`, `mg3_triggers.qc`
  and sacrifice triggers also supply armor shards, draught teleports, hell-knight head puzzle, healing, rune
  relays/counters, lore/music, skill and Bloody Nightmare progression, sacrifices and both endings.
- **MG3 state conflict:** official `parm10..14` contain upgrade masks and `parm15` contains bloody-weapon flags.
  VR already uses those slots for the second hand and holsters, and uses extended parms through `parm50` for
  weapon instances, magazines, mission-pack ammo and flashlight state. Allocate separate persistent fields and
  extend the existing engine/QC transition and save mapping; never copy official parm assignments verbatim.
  Preserve official `serverflags` meanings per campaign, including Bloody Nightmare and rune completion, and
  define campaign-new-game reset independently from hub/secret/death/save transitions.
- **MG3 weapons:** `weapon_mjolnir` is the Super Axe with campaign-specific behavior, including 40 base damage,
  zombie-specific damage, hit debouncing, finishing gibs and optional silent teleport on pickup. Existing Hipnotic
  Mjolnir melee/lightning is not equivalent. Laser cannon and bloody shotgun/SSG need their official behavior and
  ownership/upgrades integrated with VR hand/holster instances. Item bits for Mjolnir (128) and laser (8388608)
  reuse Hipnotic bits; keep them campaign-scoped. `func_axe_button` also requires the combat damage filter that
  recognizes the actual attacking VR hand; a player's selected main weapon is insufficient.
- **MG3 monsters/bosses:** infected grunt/enforcer/knight/hell knight transformation and delayed kill accounting,
  demodog grenades, ghosts, orbs, ranged knights, rocket ogres, slime, Super Shambler and its child/ghost attacks,
  MG3 lava man, sacrifice logic, Chthon finale teleport/phase attacks and new Shub/zombie/endgame logic. Register
  new archetypes with VR animation, attack, hit-zone, damage/parry, grabbing/ragdoll and projectile helpers as
  applicable, preserving authored immunity/phase/health-trigger behavior. Existing Rogue lava man is not MG3's.

Both manifests enable grenade bounce fixes, liquid-aware monsters, coop per-player item respawn/keep-weapons and
cancellation of delayed thinks. Integrate relevant differences in existing `client`, `items`, `combat`, `subs`,
`triggers`, `buttons`, `doors`, `monsters`, `ai`, `weapons`, `world` and monster files; a new-files-only merge omits
these behaviors. Keep VR frame updates, hands and collision behavior. No melee tuning is part of this audit.

## Detection, installation and campaign selection

`VR_BeforeAddGameDirectory` tests only for a folder, so empty/broken mission-pack folders count as installed.
`COM_AddGameDirectory` mounts sequential numbered PAKs; a gap stops the loop. A directory, a PAK filename and an
available BSP are three different checks. Validate pack directory structure/PAK header and resolved required
resources, returning per-pack status and missing paths. Missing mandatory packs must produce an actionable message
before map spawn: pack names, detected source folder, and the files/folders to supply from the user's owned copy.
Do not offer commercial data through the community map downloader or package it with the VR release.

Steam original data and `rerelease/{id1,hipnotic,rogue,dopa,mg1,mg3}` are separate roots. `COM_SetBaseDir` accepts
id1/pak0 and does not establish full registration or mission-pack completeness. Existing automatic store discovery
runs after current-directory/ancestor detection, so having original id1 locally does not auto-mount rerelease's
Dopa/MG1/MG3. Use the existing Steam/GOG/local base discovery read-only; keep the writable VR/config/cache/save
root separate. Test multiple `-basedir` roots and duplicate packs with deterministic precedence. Never write into
Steam/rerelease as part of detection or installation.

`VR_SkipSearchPath` selects only id1/Hipnotic/Rogue `maps/start.*` using `vr_activestartpaknameidx % 3`.
The VR hub's entity patch has only indices 0/1/2, and the menu starts `map vrstart` without installing/selecting
other campaigns. Dopa has `e5start` and `start`; MG1 and MG3 both have `start` and `hub`; MG3 has `dm1` too.
Mounting all packs together can select another pack's identically named map, model or sound. Add an explicit active
campaign descriptor with source pack, start map, native-support status and progression schema; isolate the selected
expansion's search paths below VR. Selection must happen before loading its start map, and returning to VR Hub
must restore the proper default paths. Resolve provenance for relit maps too (`VR_ModelFile` is already per-pack).
Do not infer the campaign from `mapname == "start"`/`"hub"`; VR's Honey extension check currently recognizes
all `start` maps as Honey-capable and needs explicit campaign context.

`QuakeVR.bat` appends `%*` after `-game quakevr`; an expansion containing `progs.dat` appended afterward selects
compatibility-mode QC. Native launch must keep merged VR progs highest while resolving the intended pack's assets.
The map install/browser only starts a bare `map <startmap>` and has no expansion requirement model. Keep official
campaign installation/status separate from community BSP installation and represent missing pack versus unsupported
native gameplay explicitly. Update `docs/INSTALL.md`, README/package quick-start and hub labels together.

Localization is already looked up through `COM_LoadMallocFile("localization/loc_<language>.txt")` before KPF
fallback in `LOC_LoadFile`. The owned updated rerelease **id1 PAK contains** the English and other localization
tables; Dopa/MG1/MG3 PAKs do not. New official `$mg3_qc_*` strings therefore require the correct updated rerelease
id1 table (or an explicitly supplied, appropriately licensed text solution), not merely MG3's PAK or an old KPF.
Test absent/old language tables, placeholder formatting and non-English fallback. Existing KPF/store fallback
is read-only and can mask an incomplete local test fixture; verify resolved table provenance.

## Small implementation tasks, in order

1. **Required assets and pack status** (`vr_gamedir.cpp`, map-start/UX hooks): centralize mandatory Hipnotic/Rogue
   sentinel/resource checks, distinguish missing/corrupt/available, and explain remediation before `worldspawn`.
   Acceptance: isolated boot matrix above yields actionable status instead of `Mod_LoadModel` failure.
2. **Optional-pack resource policy** (`world.qc`, `weapons.qc`, related spawn paths): if base-only gameplay is required,
   gate all pack precaches, random expansion spawns, persistent inventory/items and pack-only mechanics together.
   Do not silently alias unavailable models. Acceptance: id1-only map boot and no reachable unprecached resources.
   Until this is implemented, UX must accurately state that both mission packs are required.
3. **Official campaign discovery and launcher order** (existing store/base discovery and `vr_gamedir.cpp`): detect
   owned Dopa/MG1/MG3 without network/install writes; validate content and maintain VR progs priority.
4. **Campaign selection and hub** (`vr_gamedir.cpp`, `vr_menu.cpp`, `vrstart.ent`, QC campaign helper): select paths,
   startmap and state schema together, restore VR Hub, guard unavailable/unsupported choices and map-name collisions.
5. **Dopa entities** (new prefixed QC helpers, `progs.src`, FGD): implement explosions/fog; acceptance e5m1/e5m2
   and e5start/e5end have zero dropped gameplay entities and correct progression.
6. **Shared map movement/effects/triggers** (prefixed MG QC helpers, `subs`/frame hook/FGD): implement bob/toss/rotate,
   fog/lights, corpse/model/ambient entities and trigger semantics in small dependency groups; preserve VR frame hook.
7. **Shared monster activation and checkpoints** (`monsters`, client/triggers/items): defer/activate/count monsters,
   official spawnflags, killable Chthon and coop checkpoint behavior, campaign-gated.
8. **MG1 hub/runes and electrode puzzle** (map-specific helpers, client/items): implement return spawnpoints,
   final gate, six rune indicators and four mge2m2 entities. Acceptance full five-episode-to-ending route.
9. **MG1 horde** (separate QC module, coop integration): waves/currency/key spending, boss/flying spawns and revival.
10. **MG3 persistent state** (QC fields/client/upgrade helpers and existing engine spawn/save mapping): allocate
    upgrade/bloody-weapon state without overwriting parm1..50; integrate dynamic capacities, reset and save transitions.
11. **MG3 world/progression** (items/triggers/runes/sacrifice): armor/draught/head puzzles, lore, skill/Bloody Nightmare,
    health/rune/door relays, secret/hub paths and terminal state. Add FGD coverage for every new spawn function.
12. **MG3 VR weapons** (weapon instance/util/item/melee integration): Super Axe, laser and bloody variants, melee-only
    buttons, pickup teleports and inventory persistence; retain per-hand damage attribution and existing melee tuning.
13. **MG3 monster groups** (new prefixed modules plus VR archetype registration): infected transformations first;
    then ranged/rocket/ghost/orb/slime/demodog groups; Super Shambler and boss phases/endings separately.
14. **Localization and install docs** (existing localization path, credits/package/docs): current PAK table provenance,
    translated strings, correct required-data instructions, campaign support status and source notices.

Each gameplay group needs its own headless scripted acceptance checks and smoke test. Physics/melee changes also
run the kit's proportional melee regression check with the prescribed archived-take hand settings. Do not advertise
native MG1/MG3 completion merely because all maps launch or all spawn symbols have stubs.

## Targeted acceptance plan

- Boot/data matrix: original/release id1 only, either mission pack absent, empty folders, corrupt/truncated PAK,
  fully installed packs, rerelease-only installations, duplicate data roots and one missing localization table.
- Dopa: all 13 BSP entity scans, e5m1/e5m2 exploding walls, fog/start/end, secret exits and sigil state.
- MG1: all 25 BSP scans; hub return spawns and all-rune final gate; mge2m2 electrode/rune egg; mge5m2 target changes,
  timed counters, repeaters, fade/lightning; moving solids and deferred monsters; one horde wave/key/coop revive.
- MG3: all 22 BSP scans; each new archetype spawn/attack/death; infected transformation counts one kill; Super
  Shambler child attacks; boss teleport/phase immunity and boss2 ending; axe buttons in map6/7/8 and secret5;
  map4 head puzzle; map8 sacrifices; draught and silent teleport paths; rune/health relays and Bloody Nightmare.
- State: collect one of each upgrade, verify +10 capacity and replenishment; revisit same map without a duplicate
  award; carry through hub/map2b/secrets, death/restart, save/load and new campaign. Verify both hands, six holsters,
  magazines, weapon instance IDs, mission-pack ammo and flashlight survive transitions independently.
- Selection: repeatedly switch all six campaigns from the hub; verify actual source pack of start/hub/end/dm1,
  progs identity, models/sounds/localization/relit data; an unavailable campaign cannot fall through to another start.
- Human VR review after implementation: Super Axe contacts/axe buttons, laser aim and pickup teleports; readable
  lore/rune UI; hub gating and missing-pack message; rotating/bobbing geometry and screen-shake comfort adaptation.

## Licensing and reproducibility

The approved local README states GPLv2; individual source headers grant GPLv2 or later and preserve id Software
copyright notices (MG3 headers extend to 2026). `progs.src` identifies Machinegames 2021/2026. Retain those notices
on imported code, add source/changed-file attribution to `docs/vr-port/CREDITS.md` and package source notices, and
include the applicable license text with source distribution. The local excerpt does not itself contain the README's
referenced `COPYING.txt`; check the complete approved source/license provenance before shipping imports. GPL QC
permission does not grant permission to redistribute owned commercial PAKs, maps, models, sounds or language tables.
No asset content or upstream QC was added by this audit.

Source-tree SHA256 fingerprints (sorted relative file name, NUL separator, then raw file contents, all files):
MG1 `640b4621bec476af490104641b34f2eb635c113a7dff89d8d6a1e006104425af`;
MG3 `4507b7216057803d4c60b4e5886c05f7292232430aa434faa02e477c03998cd6`.
This pins local audited bytes, not an upstream Git commit. Logs/scripts/owned-PAK links under `audit-local/` remain
untracked for coordinator review/removal; do not commit or distribute them.

## Missing map classname inventory

These are exact absent VR spawn symbols from the owned BSP entity scan. Existing symbols still require the semantic
ports described above. Counts refer to distinct classnames, not entity instances.

**dopa (2):**

`func_explode`, `info_fog`

**mg1 (41):**

`ambient_generic`, `dynamiclight`, `func_bob`, `func_explode`, `func_hurt`, `func_toss`, `horde_manager`,
`hub_trigger_changelevel`, `info_fog`, `info_horde_ammo`, `info_horde_item`, `info_horde_key`,
`info_monster_start`, `info_monster_start_boss`, `info_monster_start_flying`, `info_player_start_hub`,
`info_rotate_axis`, `light_flame_gas`, `mge2m2_electrode_button`, `mge2m2_electrode_target`,
`mge2m2_rune_egg_opener`, `mge2m2_rune_pickup_fixer`, `misc_corpse`, `misc_rune_indicator`, `particle_embers`,
`particle_embers_tall`, `particle_tele`, `particle_tele_fountain`, `rotate_object_continuously`,
`target_lightramp`, `trigger_activate_coop_spawns`, `trigger_changetarget`, `trigger_cleanup_corpses`,
`trigger_counter_timed`, `trigger_fade`, `trigger_fog`, `trigger_fog_transition`, `trigger_lightning`,
`trigger_repeater`, `trigger_screenshake`, `trigger_sound`

**mg3 (67):**

`ambient_generic`, `func_axe_button`, `func_bob`, `func_breakable`, `func_explode`, `func_hurt`, `func_toss`,
`info_boss_teleport_boss`, `info_boss_teleport_first`, `info_boss_teleport_second`, `info_fog`,
`info_szombie_spawn`, `item_armor_shard`, `item_artifact_lavasuit`, `item_draught_insight`, `item_draught_stupor`,
`item_head_hellknight`, `item_upgrade_cells`, `item_upgrade_health`, `item_upgrade_nails`, `item_upgrade_rockets`,
`item_upgrade_shells`, `misc_corpse`, `misc_model`, `misc_rope`, `misc_sacrifice`, `monster_army_infected`,
`monster_boss_final`, `monster_demodog`, `monster_enforcer_infected`, `monster_ghost`,
`monster_hell_knight_infected`, `monster_knight_infected`, `monster_ogre_rocket`, `monster_oldone_new`,
`monster_orb`, `monster_ranged_knight`, `monster_slime`, `monster_super_shambler`, `particle_embers`,
`particle_embers_tall`, `particle_tele`, `rotate_object_continuously`, `trigger_activate_coop_spawns`,
`trigger_always`, `trigger_bloodynightmare_relay`, `trigger_boss_teleport`, `trigger_counter_timed`,
`trigger_door_relay`, `trigger_explosion_repeater`, `trigger_fog_transition`, `trigger_heal`,
`trigger_health_relay`, `trigger_lightning`, `trigger_lore`, `trigger_multitouch`, `trigger_music`,
`trigger_relay_setskill`, `trigger_repeater`, `trigger_rune_counter`, `trigger_rune_relay`,
`trigger_sacrifice_counter`, `trigger_screenshake`, `trigger_sound`, `trigger_teleport_silent`, `weapon_bloody_sg`,
`weapon_bloody_ssg`

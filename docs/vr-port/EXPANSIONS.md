# Official expansion support audit

Initial audit: 2026-10-05, documentation only, baseline `agent/expansionaudit`.
Optional-pack implementation: 2026-10-05, `agent/optionalpacks` (tasks 1 and 2 below).

## Optional-pack implementation and verification

Neither Hipnotic nor Rogue is required for base Quake VR. Each pack is validated and mounted independently below
VR's progs. `vr_gamedir.cpp` checks the pack's own 100 Hipnotic / 131 Rogue QC resource and official BSP filenames,
PAK directory/entry bounds, model/sprite payload layout, BSP lumps and the WAV chunks used by the engine. It does
not require canonical checksums or consume copyrighted asset data from this repository. Original WAV trailing LIST
metadata/padding quirks are accepted as the engine accepts them. Empty directories, malformed archives and truncated
required resources are unavailable. `common.c` also ignores malformed PAKs during the earlier mod discovery pass,
which previously could terminate startup before the VR-specific validation ran. This is structural validation,
not an authenticity check or an exhaustive proof of every authored gameplay behavior.

Read-only `vr_hipnotic_available` / `vr_rogue_available` are 0 or 1 (compiled default 0, populated from installed data).
Read-only `vr_hipnotic_status` / `vr_rogue_status` are 0 missing, 1 available, 2 incomplete/corrupt (default 0).
`vr_pack_status` and Debug > Reports > Mission Pack Status print the result and restoration advice. Unavailable hub
buttons show their state, refuse selection, and campaign map/start requests report the missing pack before spawn.

`QC/vr_packutil.qc` centralizes availability policy so later campaign-specific implementations can extend it.
Guards cover global precaches, pack spawn functions, dispensers, ammo boxes, weapon/item ownership, random drops,
Rogue monster variants and secondary ammo, restored level parms, and torch-to-lava nail conversion/tests. Missing
resources are never represented by model aliases or native-support stubs. Shared Honey candle resources are checked
by file availability. A saved `vr_save_packmask` identifies the precache layout; saves require the same installed
pack set, and legacy merged-VR saves require both packs. Incompatible saves are rejected before disconnecting.

Isolated fixtures in `agent/optionalpacks/pack-tests` hardlink the original audit's owned PAKs read-only, with a
worktree-local VR junction and local autoexec/logs. Corrupt cases contain independent synthetic/copied files only.
All runs used `-nomapindex -noaddons -nosteam -nogog -noegs -noconfigwrite`, mock headset, hidden window, autosaves
disabled; audio checks used SDL's dummy driver. Maps were verified with the console's actual `Current map` report.

| Data/check | Verified result |
|---|---|
| Original and rerelease id1 only: e1m1, vrstart | Exit 0, no Host_Error/missing spawn functions/unprecached resources |
| Original id1 only: tutorial, firing range; rerelease id1 only: tutorial | Exit 0, correct map loaded; base weapon cycling, firing and spawn/drop checks pass |
| Original and rerelease + only Hipnotic: hip1m1 | Exit 0, Hipnotic available, Rogue missing |
| Original and rerelease + only Rogue: r1m1 | Exit 0, Rogue available, Hipnotic missing |
| Original and rerelease + both packs: e1m1 and hub; original both: range | Exit 0, both available |
| Empty pack directories, truncated/bad PAK headers, truncated playham payload | Exit 0, base e1m1/hub playable, damaged packs unavailable with specific diagnostics |
| Base-only unavailable start requests | Both refused with restoration advice |
| Completed base-only/both-pack saves, same-layout loads | Successful; mask 1 and mask 4 recorded |
| Base-only save under both packs and vice versa | Refused before disconnecting with saved/installed masks |

Shipping build: 0 QC warnings, FGD covers all 248 spawn functions, engine built; kit smoke exits 0. The kit melee
check cannot run because its canary recording `no_hit_reloading_2026-09-29_23-08-51.csv` is absent. An initial
immediate save/load test raced the existing background save writer; loading the completed file in a subsequent run
passes. No calibration or authored melee data was changed. Headset follow-up: unavailable hub labels/messages,
base-only range/tutorial, and each pack's weapons/monsters and portal selection.

## Initial audit findings and measured checks

At the initial audit baseline, both mission packs were mandatory for the merged VR progs, including when playing the base campaign.
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

At the initial audit baseline, `VR_BeforeAddGameDirectory` tested only for a folder. The implementation above
replaces that check; the following discovery/selection notes also guide the later native campaign work.
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

1. **Implemented: required assets and pack status** (`vr_gamedir.cpp`, map-start/UX hooks): centralize mandatory Hipnotic/Rogue
   sentinel/resource checks, distinguish missing/corrupt/available, and explain remediation before `worldspawn`.
   Acceptance: isolated boot matrix above yields actionable status instead of `Mod_LoadModel` failure.
2. **Implemented: optional-pack resource policy** (`world.qc`, `weapons.qc`, related spawn paths): if base-only gameplay is required,
   gate all pack precaches, random expansion spawns, persistent inventory/items and pack-only mechanics together.
   Do not silently alias unavailable models. Acceptance: id1-only map boot and no reachable unprecached resources.
   The implementation and measured acceptance results are recorded above; neither pack is now required for Quake.
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
include the applicable license text with source distribution. The complete approved upstream source at commit
`634eefa` includes the root `COPYING.txt` (GPLv2); retain it when shipping source imports. GPL QC
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


## Campaign discovery/selection implementation (agent/campaignselect)

Tasks 3/4 now have a central six-entry descriptor in `vr_gamedir.cpp`: folder, display title, start map,
state schema, native readiness, validated resource inventory, availability and resolved source root.
The native-readiness entries for Dopa, MG1 and MG3 remain **false** until their gameplay ports are accepted.
A successful developer map launch is not a native gameplay acceptance result.

### Context ABI for the subsequent QC ports

Read-only `vr_campaign` and `vr_campaign_schema` both default to 0 and currently use these values:

| ID/schema | Folder | Campaign | New-game start |
|---|---|---|---|
| 0 | id1 | Quake | start |
| 1 | hipnotic | Scourge of Armagon | start |
| 2 | rogue | Dissolution of Eternity | start |
| 3 | dopa | Dimension of the Past | e5start |
| 4 | mg1 | Dimension of the Machine | start |
| 5 | mg3 | Dawn of the Machine | start |

QC can gate authored behavior with `cvar("vr_campaign")`; it must not infer MG1/MG3 from `start` or `hub`.
The schema is an explicit versioned identifier, currently mirroring the campaign; the gameplay ports must allocate
persistent fields independently of VR's existing parm1..50. This change does not implement MG3 progression fields.
`vr_save_campaign` is a saved QC global assigned in worldspawn. Before spawning a saved map the engine restores
its descriptor and paths; the existing `vr_save_packmask` check is retained. The campaign-new-game commands run
`map`, which resets serverflags/spawn parms. Normal same-campaign `changelevel` preserves progression and context.
Hub return runs a fresh `map vrstart`, restores campaign 0 and removes the newer expansion's mounted paths.
The stock hub's legacy 0/1/2 buttons remain supported through the changelevel guard; out-of-range indices are
refused instead of wrapping modulo three. The archived legacy selector records only 0/1/2; newer native launches
leave it at 0 so a later ordinary boot does not inherit an unsupported start request. Honey start-map extensions use resolved Honey folder provenance through
read-only `vr_honey_context` (default 0); official start maps are no longer classified as Honey merely by name.

### Discovery, precedence and commands

New read-only `vr_dopa_status`, `vr_mg1_status`, `vr_mg3_status` default to 0: 0 missing, 1 owned data available,
2 incomplete/corrupt. Status 1 does **not** imply native gameplay readiness. Filename-only inventories cover all
13 Dopa, 25 MG1 and 22 MG3 BSPs and the packs' own model/sprite/WAV resources. The existing structural validator
also accepts official BSP2/2PSB headers. Archive entry bounds and complete resource payloads are validated before
mounting; VR's own models and progs are not evidence of owning an expansion. No asset payloads are committed.

Discovery reads the explicit basedirs, their rerelease child/sibling folders, and existing Steam/GOG resolvers.
The last explicit base has highest priority, then its child/sibling rerelease folders; store discovery is the fallback.
The first existing pack folder in this order owns the result: a corrupt higher-priority copy is reported rather than
masked by a lower-priority release. New pack copies must be individually complete; resources from duplicate
installations are not combined. `-nosteam`/`-nogog` disable those store lookups. Store roots are never appended to
`com_basedirs`, so config/save/cache roots remain the caller's writable roots. Detection and native switching perform
no installation, download, migration or configuration write to borrowed folders.

Only the selected Dopa/MG1/MG3 folder is mounted, below merged VR progs and above the optional mission packs.
`-game mg1 -game quakevr` and `-game quakevr -game mg3` both retain VR progs; the latter launcher ordering no
longer lets an official pack replace merged VR code. A manual `game mg1 quakevr` also publishes context 4.
Existing non-VR compatibility game launches retain their normal game-code behavior. Common colliding start/hub/
end/dm1 maps and sidecars are isolated by descriptor. Optional pack brush models used by VR global precaches,
such as `maps/b_explob.bsp`, remain available. Exact resolved root/archive provenance is retained by file lookup,
including duplicate basedirs, and relit replacements use that source folder's namespace.

- `vr_campaign_menu`: the selector, also available from Single Player > Official Campaigns, VR Settings and the hub board.
- `vr_campaign_status`: all six data/readiness results, source roots, start maps and schemas.
- `vr_campaign_select <folder>`: starts a ready campaign; missing/corrupt/native-in-progress choices are refused before spawn.
- `vr_campaign_native <folder>`: explicit developer native launch, with a gameplay/progression-incomplete warning.
- `vr_campaign_hub`: restores paths/context and starts a fresh VR hub.
- `vr_campaign_probe [virtual filename]`: exact actual source of a file; with no argument, probes VR progs and colliding maps.
  For a supplied map it also prints the relit selection. Status and probes are exposed in Debug > Reports.

### Measured checks

The engine/QC build reports 0 QC warnings and the FGD check covers all 248 existing spawn functions. Isolated,
worktree-local fixtures use read-only hardlinks/junctions to owned data; no payload copies or store writes. Runs use
mock/hidden headset, `-nomapindex -noaddons -noconfigwrite`, autosaves disabled and scripts ending `toggleconsole;quit`.
Original and rerelease id1-only cases both load e1m1 with the two mission packs and three new packs missing.
A local original-root fixture discovers its rerelease child; separate original/rerelease `-basedir` roots also work
while the local root stays last/writable. A duplicate MG1 folder in the highest base supplies its own start map;
a four-byte PAK in that location is status 2 and both ordinary/developer selection refuse it, leaving e1m1 active.
Both native launcher orders expose the requested context and resolve progs.dat from quakevr. All six selection
starts load with exit 0 and no Host_Error after preserving optional precached brush resources; MG1/MG3 start/hub
and MG3 dm1 probes point to their own PAKs. These are path/start checks; unknown gameplay entities remain outside
this task's acceptance. Hub return resolves hub.bsp as missing rather than leaving another expansion mounted.
MG1 save under optional-pack mask 1 records campaign 4; after switching to MG3, loading it restores MG1 context,
its start source and the expected map. Legacy hub index 999 is refused, while index 1 loads Hipnotic start and
continues to hip1m1 with context 1. Actual hub board path check: **1 found, 0 missing**. Model probes also confirm MG3 lavaman/rocket-ogre provenance; hub return removes the rocket-ogre model and
restores Rogue lavaman resources. Namespace-only relit fixtures (never loaded as maps) select relit/mg1 for MG1,
retain MG3 start when its own relit namespace is absent, and restore relit/id1 after hub return. Headless selector image
was inspected for placement; human headset follow-up should check row/help readability, board link activation,
missing-pack feedback, and repeated original-pack portal/selector transitions.

Scratch to remove after integration: `campaign-tests/`, `quakevr/campaign_context_test.sav`, the kit's
`campaign-menu.png`/matching screenshot, and the untracked `quakevr/ironwail.cfg.baseline`. No cleanup was performed.


## Native environment shipment (mgworld, 2026-10-06)

Dopa, MG1 and MG3 remain **native support in progress**. This shipment implements environment semantics;
it does not enable a campaign merely because its BSPs now load. Source snapshot/license are recorded in
`OFFICIAL_QC_SOURCE.md` and `QC/COPYING-rerelease.txt`; imported files retain the original notices.

`QC/vr_mg_*.qc` supplies authored fixed brush explosions/target activation, solid and non-solid bobbing,
tossed/cascaded/shattered brushes, continuous rotation and acceleration, fog definitions/triggers/axis fades,
light-style ramps and gas flames, ambient loops, embers/slipgate effects, decorative corpse poses/models and
MG3 segmented ropes. Helpers use the `MG_` prefix; mapper fields/classnames retain their authored names.
`MG_WorldCampaign()` gates them to `vr_campaign` 3/4/5. `MG_WorldFrame()` runs registered tick callbacks beside
existing VR frame hooks, skipping deleted entities without a dangling linked list. Reuse
`MG_RegisterFrameTickEntity`, `MG_RemoveFrameTickEntity`, `.MG_tick` and `.MG_registered` in later native modules.
The upstream `dynamiclight` is intentionally a KEX rendering marker without a QC callback; its shadow-cone
metadata still needs renderer adaptation. MG3's `func_breakable` is boss-specific falling ceiling behavior,
not an ordinary destructible wall, and stays with the future boss port. MG3 corpse rune/coop inhibition also
awaits the shared authored spawn-filter work.

Fog integrates player spawnpoints (world fallback), intermission cameras and teleport destinations. Ironwail
filters server-stuffed commands, so native fog uses its reliable `svc_fog` message (8-bit density/RGB and 0.01 s
fade timing). `Host_Spawn_f` sends saved fog after clearing spawn-QC messages; this handles fresh signon and
completed save loads without changing base/Honey fog. Geometry links after its final collision type is set;
`func_hurt` tests tracked main/off-hand points against the brush hull as well as its existing body touch,
sharing the authored cooldown. Fixed brush explosions retain the attacking player as target activator and use
VR's explosion/metal impacts, with no brush blood. Gas flames join the existing VR flame contact/relighting path.

`vr_mg_world_test` defaults to 0 (not archived): 1 prints world/player fog; 2 runs the focused environment checks
and explodes a barrel on e5m1. Both are in Debug > Tests. Test 2 is destructive; reload the map afterward.
No existing menu rows/calibration paths moved. The generated FGD covers 269 spawn functions, with native fog
and fixed brush-barrel fields documented.

Validation used owned rerelease id1 plus owned Dopa/MG1/MG3 PAK hardlinks, no Hipnotic/Rogue, a worktree-local VR
junction and saves, hidden mock headset, `-nomapindex -noaddons -nosteam -nogog -noegs -noconfigwrite` and disabled
autosaves. All 13 Dopa BSPs report the correct current map, exit 0, zero missing spawn functions/Host_Error and
correct fog density/RGB (within Fitz protocol quantization). The only unknown field is an empty **editor** key
`property 1` on an e5m6 light; no gameplay field is omitted. Focused e5m1 matrix: **19 passed, 0 failed**: degree-based
bobbing, rotation normalization/acceleration/toggle, translated fog bounds/world fallback, exact main/off-hand
and body hazard damage/cooldown/off state, delayed explosion removal, one target activation with the player,
and metal impact without blood. A completed e5m1 save loads with authored fog 0.05 (wire value 0.0509804), RGB
65/65/60. Base e1m1 and VR Hub smoke exit 0; calibration paths report **14 found, 0 missing**.
Final build has 0 QC warnings, passes QC priority/static checks, builds Release x64 and passes FGD coverage.
The required melee canary cannot run: `no_hit_reloading_2026-09-29_23-08-51.csv` is absent from the authored
archive/test path (kit eval exits before launching a replay). No calibration data was substituted.

Dopa readiness blockers: raw BSPs contain **62 deferred monsters** using bit 4 (8 e5m1, 2 e5m2, 9 e5m3,
11 e5m5, 8 e5m7, 24 e5sm1), including angry bit 8 combinations. Shared activation/future-monster counting,
activation targets and difficulty variants must be verified before native-ready. Its e5end `trigger_changelevel`
contains `endtext = $map_dopa_endtext_final`; the field is parsed, but existing VR exit logic does not yet implement
that authored finale. Normal/secret exit chains, inventory carry, deaths/restarts and final credits/progression
remain acceptance work for the shared activation/progression task.

A broader MG1 audit stopped on **mge1m1**: existing Honey ogre flag handling attempts to precache
`progs/s_light2.spr`, absent from owned MG1/base data. `honey_mon_ogre.qc` is reached by existing monster spawning;
new native contexts need to gate Honey-specific flag interpretations and apply authored official ones.
MG3 map5 (Requiem) also reaches its correct map with decorative model/rope spawns; map6 hits the same existing
`s_light2.spr` Honey-variant failure. That exploratory run was stopped after error recovery; it is not campaign acceptance.
`speed2` moving-geometry fields also require the shared existing-function port. Broad MG1/MG3 gameplay acceptance
is not claimed. Existing optional-pack footstep paths can print `misc/foot*.wav not precached` in this isolated
base-only fixture; they remain separate from the environment changes. No head/hand calibration or melee tuning changed.

Human follow-up: native campaign fog/fades, bobbing/rotating geometry, gas-flame hand contact, visible ambient
particles and model/rope placement. Retained untracked diagnostics: `world-tests/`, `port_world.py`, local build/config
outputs and `mgworld_fog.sav`; owned asset fixtures are hardlinks, not distributable release content.

## Native activation and Dopa progression (mgactivation, 2026-10-06)

Dopa remains **native support in progress**. Gameplay activation/progression is ported; portable localization
and the rapid transition/switch reliability findings below still block readiness. MG1/MG3 also remain gated.

`vr_mg_activation.qc` adapts official MG1 monster/client/target behavior without replacing VR monster frame
functions, bounds, hitzones, attack/parry/shove callbacks or death/ragdoll paths. Official flags 4 (deferred),
8 (angry), 16 (teleport effect) and 4096 (wait for path use) have campaign-scoped semantics. Models/bounds and
startup callbacks are saved while dormant; target activation restores them and retains its triggering player.
Dormant monsters are counted at placement, not twice at activation. A deferred killtarget records one kill
without materializing the monster. Decorative crucified zombies do not enter the official monster count.

The official FL_FUTUREMONSTER bit 16384 is **already VR FL_SPECIFICDAMAGE**. Saved `MG_deferred` supplies future
state separately; no foreign edict flag is copied. Mission-pack/Honey meanings are gated out of contexts 3/4/5:
Rogue knight/hell-knight statues (2), boss ogres (2), tarbaby variants (2/4/8 and random mitosis), extra zombie
lying/sneaky flags, and Honey trigger/multiple/tfog/angry/corpse-removal/AI flags. Official marksmen use the base
ogre model, ordinary grenade/attack behavior and no Honey crown sprite; actual Honey retains its own behavior.
Coop-only 32768/not-in-coop 131072 inhibition and named checkpoint activation/cycling are scoped too. MG1 horde
and collected-weapon checkpoint/revival rules still belong to their future gameplay tasks.

Dopa changelevel triggers retain authored normal/secret destinations. Trigger/world endtext is sent as a finale;
NO_INTERMISSION cannot bypass an authored endtext. Nightmare new-player health is 50; other Dopa starts use 100.
The existing VR transition serialization retains hands, holsters, weapon records and secondary ammo. The formerly
empty engine `menu_credits` command now opens a native completion/attribution menu, with Official Campaigns and
Main Menu controls (keyboard/controller/laser mouse; Escape/B returns to Main). It uses existing menu rendering,
no external assets or forced headset motion. This is a native attribution screen, not a recreation of the
commercial rerelease's full scrolling team-credit assets.

Acceptance driver distinction: an autoexec prequeues its quit. QC changelevel/localcmd appends commands after
that quit; a probe before it can misleadingly see the old map. Thirteen normal/secret route selections and
explicit dispatch checks cover the engine carry/spawn path; separate end-to-end tests drain the initial queue
and use test request 10 to append `exec mgactivation_after.cfg` **after the actual QC changelevel**. Request 8
seeds test inventory; mock hands use sticky grip mode so unpressed grips do not immediately drop their guns.
Requests 4 (progress report) and 3 (activation acceptance) are exposed in Debug > Tests. 5/6 select authored
normal/secret exits, 7 advances an intermission, 9 uses a named dormant-monster target relay; these are destructive
acceptance operations and require reloading afterward. The default existing `vr_mg_world_test` remains 0.

Retained worktree-only evidence: `activation-tests/`, authored-asset hardlinks/junctions, generated
`quakevr/mgactivation_after.cfg`, test saves/screenshots, edit scripts and `evaltakes/`. Do not redistribute fixtures.
The melee canary is unavailable: authored `no_hit_reloading_2026-09-29_23-08-51.csv` is missing. No calibration
recording was substituted. Tests use hidden mock, `-nomapindex -noaddons`; default map-index startup is not claimed.

Behavioral evidence: expanded activation suite **18 passed, 0 failed**, including hidden/idle/invulnerable future
monsters, one placement count, activation/anger, repeated use, death/future-kill count, telefrag collision,
teleport fog/sound, path wait/resume, solo/coop inhibition and activated coop spawn selection. All 13 authored
normal/secret route selections and explicit engine dispatch matched their BSP destinations. Real QC-queued
normal e5m1 -> e5m2 and secret e5m3 -> e5sm1 passed. With sticky mock hands, normal carry preserved health 73,
shells 42, main shotgun magazine 3/id 6, off nailgun magazine 7/id 7 and holstered SSG magazine 2/id 8. Completed
save/load retained deferred state/counts; restart and death/restart retained that level's start inventory.
New campaign restored health 100, shells 25, empty hands/holsters and zero progress. The end chain opens native
credits. Its rendering was visually inspected; headset readability/laser/button behavior remains human QA.
Stock e1m1, actual Honey honey/saint and VR Hub smoke exit 0. Calibration reports **14 found, 0 missing**.

Localization blocker: rerelease id1 + Dopa with English tables displays the authored congratulations text.
Original id1 + Dopa emits the literal `$map_dopa_endtext_final`; this was reproduced with a one-entry local test
language table so store/KPF fallback cannot supply that key. See `finale-rerelease.log`,
`finale-original-store.log` and `finale-original-no-lookup.log` in the worktree's `activation-tests/`.
A dedicated read-only rerelease localization resolver is required before enabling Dopa readiness.

Integration findings (do not interpret launch coverage as acceptance): one rapid all-map count sweep crashed
with process exit 3221225477 while moving from e5m7 toward e5sm1 after 9 matching total/deferred count reports.
A subsequent launch cleared the initial crash files before they were retained; cause is unassigned. An additional
Dopa -> MG1 runtime switch stalled immediately after its developer-launch warning and timed out at 240 seconds.
Its log and diagnostic snapshot are retained as `mg1-switch-hang-console.log` and
`mg1-switch-hang-20261006-005424.dmp`. Local cdb output is `mg1-switch-hang-stack.txt` / `mg1-switch-hang-main.txt`;
main thread is inside SDL, but matching engine symbols were unavailable for that earlier binary, so this is
not a root-cause attribution. Future failing runs copy crash/log files to timestamped names before another
launch. The owned e5m6 BSP contains an empty editor typo key `property 1`; that warning is not a gameplay field.
A fresh matching Release build repeated the complete normal-difficulty sweep successfully: 13/13 BSP total/deferred counts match the owned entities, process exit 0. The only field warning is the empty editor typo noted above. The prior rapid-sweep crash remains intermittent/unattributed; this pass does not erase it.
Final narrow retests with the fresh built engine also passed: isolated MG1 mge1m1 and MG3 map6, one Dopa -> MG1
runtime switch, and the exact earlier Dopa -> MG1 -> MG3 switch sequence all exit 0. These runs no longer request
Honey s_light2.spr. Unknown MG1/MG3 mapversion/speed2/classes remain future port scope and their campaigns remain
gated. The earlier crash/stall remain retained, unassigned intermittent findings; no claim of a runtime fix.
Preliminary builds had link failures when a headless run still held the executable; final verification used the
successful fresh Release build. Final QC has 0 warnings; priority/static checks and FGD 270 pass. Git diff check passes.
Nightmare acceptance also exits 0: fresh Dopa e5start health50, and e5m1 health50 with 53 total / 8 deferred monsters; normal-difficulty new campaign health100 was checked separately.

## Portable campaign language data (mglocalize, 2026-10-06)

`LOC_LoadFile` now layers local requested-language text, read-only owned requested-language tables, local English,
and owned English. Earlier valid entries win. Empty values and literal untranslated `$identifier` values leave
room for the fallback. PAK reads validate header/directory/payload bounds and cap one language table at 8 MiB.
The resolver shares campaign root precedence and honors disabled Steam/GOG lookup; KPF fallback also honors
Steam/GOG/Epic disable flags. Borrowed language lookup never mounts general id1 content or changes writable bases.
No translated text, maps or other commercial assets are committed. The identifier-only inventories cover
30 Dopa BSP strings, 198 MG1 QC/BSP strings and 246 MG3 QC/BSP strings; the installed updated English table
resolves all of them. MG1/MG3 gameplay readiness remains false.

Each parsed entry retains its actual source. Read-only `loc_probe $identifier [arguments...]` uses the same
placeholder formatter as QC and prints the winning table; Debug > Reports has Dopa Finale Text. Campaign
status reports missing-key counts, and ordinary selection gives actionable language-data instructions before
loading unsupported content. Empty/raw placeholder entries do not hide an owned fallback; literal templates
with `{0}`/`{1}` format correctly even when no table loaded. No new cvars/defaults.

Private owned-data fixture matrix passes original-id1 with Steam lookup, rerelease-only with stores disabled,
original with explicit owned roots and stores disabled, standalone missing tables, a one-entry old table, custom
Italian with real owned Italian, and partial Italian with English-only owned tables. Custom strings are preserved;
missing Italian, empty finale and raw-placeholder upgrade values fall back to readable English. Representative
MG3 messages produce health capacity120 and shell capacity80 with no braces/literal identifiers. Original e1m1
map/player-model probes retain original asset provenance while tables come from rerelease.

Tests use hidden mock, `-nomapindex -noaddons`; default map-index startup remains unverified. Timestamped evidence
and authored-asset fixtures are worktree-only under `localization-tests/`. Early diagnostics whose campaign/map was refused timed out at150s; their logs are retained. Those scripts
ended `toggleconsole;quit` while the disconnected startup console was already open, causing the engine quit command
to open its confirmation menu instead of exiting (Host_Quit_f checks key_dest). Loading an accepted stock map
before the final toggle/quit lets the same language probes exit0. This driver correction is not an attribution
or repair for the earlier transition crash/stall. Real transition/campaign cases are recorded with the Dopa readiness acceptance below.

## Dopa single-player readiness acceptance (mglocalize, 2026-10-06)

Dimension of the Past `nativeReady` is now true **for single-player**, subject to complete owned campaign assets
and language coverage. MG1/MG3 remain false. Ordinary launch refuses Dopa coop with explicit checkpoint/revival
acceptance guidance; its supported solo scope is stated in the selector and installation guide. Developer bypass
remains available with warnings. Ready Dopa CLI context does not grant a language bypass, and direct map commands
recheck its readiness/language/coop policy. Earlier sections describe the state at their respective shipment.

Final updated-baseline hidden acceptance: real QC normal e5m1 -> e5m2 and secret e5m3 -> e5sm1 exit0, with
health73, shells42, main SG/off nailgun magazines3/7 and distinct weapon IDs, plus holstered SSG magazine2.
Completed save/load and level restart/death restart preserve the start state; new campaign resets health100,
shells25, hands/holsters and progress to zero. Dopa ending is advanced through both real finale stages: the
original-id1 client logs readable congratulations, then opens the native completion menu (visually inspected),
with Official Campaigns and Main Menu controls. The menu's actual headset readability/laser/button interaction
remains human VR QA. No claim to recreate commercial scrolling credits.

The earlier suspect e5m7 -> e5sm1 manual map pair was repeated three times on the updated baseline, followed by
Dopa -> MG1 -> MG3 -> Dopa runtime switches, all exit0. No new crash/hang occurred in these cases; prior
unattributed timestamped findings remain recorded above and are not claimed fixed. Prior accepted environment,
activation, all13-map counts and authored route-selection coverage still apply. Final stock smoke/style/FGD and
menu path check pass (14 found, 0 missing). Default map-index startup remains outside this acceptance.

Coop follow-up scope (not implemented by localization): owned Dopa BSPs have zero
`trigger_activate_coop_spawns` relays and zero named `info_player_coop` checkpoints. Shared MG1 source enables
`COOP_RESPAWN_KEEP_WEAPONS`: checkpoint activation aggregates all players' weapon ownership into spawnpoints,
and respawn restores those weapons plus nails30/rockets4/cells12. Native VR's checkpoint state activation/cycling
is present, but that ownership/VR-instance restoration is absent. The source's no-valid-spawn deferred spectator
state, retry and forced telefrag after5 seconds are also absent (native selection falls back to info_player_start);
this is especially relevant to future horde support, not an authored Dopa checkpoint relay. Source team assignment
and Nightmare respawn health behavior need cooperative acceptance. Native local `vr_campaign`/schema/root switches
are not synchronized to remote clients before signon or changelevel; peer asset/language/VR-progs validation,
late join/reconnect, per-player carry/death respawn and the authored cooperative finale return still need a
multiclient acceptance pass. Dopa normal launch remains solo-only until that work is accepted.

Final cap review found native Dopa Nightmare used initial health50 but still max_health100 and transition
carry50–100. Official MG1 client uses max_health50 and carries between half-cap25 and cap50. Those three bounds
are now scoped to Dopa/skill3/non-deathmatch; other contexts retain their existing100 cap and50 carry minimum.
Real ordinary health-box pickup from40 reaches50/max50; a real megahealth pickup reaches150/max50. Completed
save/load retains both ordinary50 and mega150 states. The actual QC normal exit carries megahealth down to50
in e5m2; a separate health1 exit carries25, both max50. Debug > Tests exposes destructive ordinary/mega pickup
setups (`vr_mg_world_test`11/12, existing default0) and progress reports now include max_health. Normal-difficulty
and stock carry acceptance is recorded in the final worker report.

Final normal-difficulty real ordinary/mega pickups yield65/165 with max100; completed mega save/load retains165,
then the real exit caps carry100. Stock Nightmare forwarded give values were drained through server frames:150
carried100, and1 carried50, both max100. Missing-language direct Dopa CLI refuses before loading its map; a missing-
language save refuses while preserving the current connected e1m1. Duplicate explicit roots/stores disabled preserve
higher local custom English while missing text comes from owned updated rerelease tables. All exit0. Ready Dopa
saves recheck language/solo policy even if context3 is already active, before disconnecting. The solo guard also
rejects deathmatch or multiple clients; it does not implement networking. Diagnostic quit timeouts remain archived.

Final solo-mode guard checks refuse deathmatch1 and a two-client server, then accept coop0/deathmatch0/maxplayers1
Dopa with Nightmare initial health50/max50 and an ordinary pickup still capped50. After the final guard build,
three more e5m7 -> e5sm1 repeats and Dopa -> MG1 -> MG3 -> Dopa switches exit0 with no Host_Error/crash; the
retained earlier findings remain unassigned. The supported campaign also returns to VR Hub correctly.


## Native shared trigger and mover acceptance (mgtriggers, 2026-10-06)

Audit steps 6/8 now include source-backed shared behavior from upstream MG1/MG3
`triggers.qc`, `misc_fx.qc`, `misc.qc`, `subs.qc`, `buttons.qc` and `doors.qc`.
The licensed implementation is `QC/vr_mg_triggers.qc`; existing VR trigger, button,
door and delayed-target paths dispatch to native behavior only in official context.
MG1/MG3 still require developer bypass and remain unready. Hub/runes/electrode/horde
and Dawn-specific gameplay belong to later work; this does not grant campaign readiness.

Implemented authored repeater toggle/jitter/activator, timed counter inactivity reset,
looping regular counter, lightning endpoint activation/flags/damage, positional sound,
dead-target fade/removal, scheduled-think freeze/resume, changetarget, cooperative-only
dead-monster cleanup and delayed point explosions (including no-damage). Delayed uses
carry the source targetname so killtarget cancels every pending use. Lightning endpoint
lookup excludes those internal delayed entities to prevent self-expanding target chains.
Native beams retain the VR protocol disambiguation byte. Message-all bits differ between
MG1/Dopa and Dawn; silent lightning is Dawn-only. Authored coop spawn inhibition is retained.

Native doors/buttons preserve exact authored movedir translation and speed2 return speed.
Native crusher64 and button key8/16/always32 are scoped away from Honey flag/item meanings.
Physical hand, held-prop/weapon and thrown-actor button activation respects native key
requirements and consumption. Empty authored model strings no longer produce a VR model
precache warning. Rogue brush-explosion behavior remains in its original context.
Native quake sound and duration/ramp are retained; controller vibration replaces view
punch so authored screenshakes never move the tracked head or camera.

Visible slipgate destination caches now invalidate when native changetarget rewrites a
gate's target. This updates both traversal and preview. Native ignore-targetname4 is
honored alongside existing legacy bit8 semantics. Debug portal reports print target and
cached standing destination; standing height retains existing floor correction.

Acceptance uses private owned original-id1 plus official campaign PAK fixtures, hidden
mock and `-nomapindex -noaddons -noconfigwrite`. Updated language tables are read-only
borrowed data; no commercial asset is committed. `vr_mg_trigger_test` defaults0 and
Debug > Tests exposes shared acceptance1, authored mge5m2 route2, lightning damage3 and
visible slipgate retarget4. These tests deliberately alter the current map; reload it after use.

Shared timing/target/cancellation/key/mover/effect tests pass34/0 in each native context
Dopa3/MG1 4/MG3 5. Dopa pending-trigger save/load passes34/0 before and after load.
Actual positional beam damage passes3/0 (100 -> 90 -> 80, wetsuit remains80), including
ordinary ready-Dopa selection. The real mge5m2 route passes10/0: a missed two-second
button window resets; coordinated physical presses remove both buttons and pending
flash, open the authored unlock door, awaken its delayed shalrath, then remove the
quake blocker and repeater. This is representative shared progression, not full campaign QA.

A separate private native-context fixture uses owned stock e1m5 slipgate geometry under
a private MG1 map name. Its five-side cache remains active and carry-enabled while the
real changetarget advances destination X/Y by128/64 immediately; standing Z receives
its existing13-unit floor correction. This fixture is not an authored MG1 map claim.
Stock VRHub/e1m1 and Honey h/saint smoke exit0; menu paths14 found/0 missing.
QC/Release/style/precedence/FGD checks pass (FGD279; QC0 warnings). The single-job melee
canary cannot start because the existing exact authored motion CSV
`no_hit_reloading_2026-09-29_23-08-51.csv` is absent; no replacement or tuning was used.

Human VR QA should check keyed physical buttons, slipgate preview/traversal after
retargeting and authored quake audio/vibration comfort. Corpse cleanup's isolated
coop-context test is not network cooperative acceptance; Dopa remains solo-only.
Default map-index startup and earlier unassigned rapid-transition failures remain outside
this acceptance. Private fixtures/logs are retained in `trigger-tests/`; the first raw
beam/recursive-endpoint diagnostic failures informed the fixes above. An initial kit
quit timeout ended after a passing menu check; an explicit console-state test-driver
correction exits normally and does not claim a runtime hang fix.

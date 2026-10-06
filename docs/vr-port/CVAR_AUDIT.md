# Quake VR cvar audit (2026-10-06)

Every Quake VR setting checked for being read, and ranked for removal or merging. The data comes from
`Misc/quakevr/cvar_inventory.py` (2 s; run it again after changes):

    python Misc/quakevr/cvar_inventory.py                 # the counts below
    python Misc/quakevr/cvar_inventory.py --csv inv.csv   # one row a cvar: default, archived, flags, reads and writes
                                                          # (C++ logic, QC, menus, migration, motion recorder), cfgs,
                                                          # tools, docs, last commit on its line
    python Misc/quakevr/cvar_inventory.py --dead          # the cvars no engine logic or QC reads
    python Misc/quakevr/cvar_inventory.py --refs --name '^vr_mhull_'   # every reference

Scope: `QVR_CVAR` in `Quake/vr/vr_cvars.inc` plus the stand-alone `cvar_t` in Quake/vr (vr_backend, vr_hull_audit, five
vr_retro ones). The generated per-slot families (`vr_wofs_*` 8,320, `vr_prop_*` 2,880, `vr_retro_<cat>_*` ~280) are
built from tables and not counted. Ironwail's own cvars are out of scope. MENU_INVENTORY.md (2026-10-03; removed 2026-10-06; git history) was the
earlier, menu-centred pass; its section 5a listed the ~320 cvars removed then.

## Counts

| What | Count |
|---|---|
| Quake VR cvars (vr_cvars.inc 2,059 + 5 stand-alone; 2,065 before this audit) | **2,064** |
| archived (saved in the config) | 1,836 |
| not archived | 228 (10 of them read-only status, CVAR_ROM) |
| named by a menu row or read by menu code | 1,895 (92%) |
| archived, on no menu page ("hidden tuning", personal measurements left out) | 86 |
| read by engine logic (C/C++, comments stripped) | 1,435 |
| read by QC (a cvar_hget handle or cvar("name")) | 671 |
| read by neither (all checked by hand below) | 27, then 26 |
| test or debug knobs (named `*_test*`, `vr_test_*`, `*debug*`) | 125: 66 test (52 in the menus), 59 debug (57); none archived |
| set by a shipped .cfg (vr_defaults.cfg, quakevr.cfg, default.cfg, vr_bindings.cfg) | 232 |
| defaults changed by the config migration (vr_cvars.cpp defaultChanges) | 303 entries |
| the author's own config: our cvars in it / different from the shipped default | 1,836 / 40 (mostly personal: calibration, menu positions, versions) |

The last number matters most for the candidates: almost every knob sits at its shipped default in the one config that
has been tuned in the headset, so most per-class and A/B knobs have never been moved off it.

## Removed (definitely dead): 1 cvar, 1 QC handle

| What | Evidence | Removed from |
|---|---|---|
| `vr_throw_lookahead` (archived, 0.01) | "unused since 2026-10-06" in vr_cvars.inc; no read anywhere | vr_cvars.inc, vr_throw.cpp comment, THROWING.md, TESTING.md, MENU_INVENTORY.md (ROUND21.md's history left as written) |
| QC handle `cvarh_vr_throw_hit_min_speed` | made (`VR_CVAR_HMAKE`) and never read: the C++ side reads the cvar (vr_weight.cpp) | QC/vr_cvars.qc (the cvar stays) |

Old configs stay quiet: `vr_cvars.cpp retiredCvars[]` lists removed names and cmd.c's unknown-command branch asks
`VR_RetiredCvar` first, so a config, a shipped .cfg or an exec'd script still setting one drops the line silently
(`developer 1` notes it); the next config write leaves it out. Checked: autoexec with `vr_throw_lookahead 0.01` loads
e1m1 with no message, a made-up name still prints "Unknown command". Add every future removal to that list.

Why so few: the 2026-10-03 cull already took every cvar with no reader. The checks run this time, all automated in the
script or in one-off greps listed here:

1. No read outside the declaration, the menus, the config migration and the motion recorder: 27 hits. Only
   vr_throw_lookahead is dead. The rest are read where the script does not count it as logic: the menus' own state
   (`vr_menu_*`, `vr_comfort_preset`, `vr_handedness`, `vr_spectator_preview`, the holster and mount previews, read
   by vr_menu*.cpp), `vr_cfg_version` (the migration), `vr_jobs_threads` (a callback), `vr_mg_horde_test` (QC's
   `cvar()` on a `cvar_set` line), and the read-only status cvars (below, candidate D1).
2. Read only inside functions nothing calls (enclosing C++/QC function resolved for 2,724 of 4,180 read sites): 0.
3. Read only inside report/log functions: 9, all of them the switches of those logs (`vr_window_log`...). Not dead.
4. QC handles made and never read: 1 (removed, above).
5. Names in shipped cfgs, menu rows, the calibration board, QC `cvar("...")`/`cvar_set` and C++ `Cvar_FindVar("...")`
   that point at no cvar: 0.
6. Documented "unused", "kept registered", "retired", "no longer": 1 (vr_throw_lookahead).

## Candidates, ranked

Ranked by cvars removed per risk. "Simplifies" is the code a change would delete. None of these is done: each needs
the author's yes, as each ends a tuning or comparison ability.

### A. Per-class overrides nobody sets (overkill: about 160 cvars, low risk)

| Family | Cvars | Defaults | Simplifies |
|---|---|---|---|
| `vr_ragdoll_<class>_{start,friction,joint_friction,joint_stiffness,limits,damping,blast,inherit}` | 104 (13 classes x 8) | all -1 (= the global) | the 7 Ragdolls - <class> pages' classSliders and reset buttons (vr_menu.cpp ~2830-3490), the per-class pointer table in vr_box3d.cpp (~2090-2110) and its -1 fallbacks: one global each |
| `vr_smallgibs_mult_<class>`, `vr_smallgibs_brains_mult_<class>` | 32 | all 1 | QC vr_smallgibs.qc's 16-way `th_die` if-chain (~510-545) and 32 handles; two Small Gibs sub-pages |
| `vr_mhull_<class>` | 13 | all -1 (QC setsize's box) | the "Prototype" Monster Hitbox page and the class table in vr_hull.cpp (~2170). **vr_hull.cpp is hullsmall's file this round: do it after.** |
| `vr_ragdoll_<class>_mass`, `vr_corpse_health_<class>`, `vr_knockdown_chance_<class>` | 40 | real per-class data | keep as data, but as a `static constexpr` table with one global multiplier each (`vr_corpse_health_mult` already exists): 40 rows and pages go, the tuning goes to code |

Risk: none for players (every value is the global's). The author loses per-monster tuning from the headset.

### B. A/B switches whose new side won ("0: as before"; about 25 cvars, low to medium risk)

Each was added with its old behaviour kept for comparison; the new one is the default and in the author's config.
Removing the switch deletes the old branch.

| cvar | Old side kept | Simplifies (reads) |
|---|---|---|
| `vr_particle_retro_fast` | "original fragment path for comparison" | the original retro-particle shader path (vr_gfx_gl.cpp, 2). **retropart's area: list only.** |
| `vr_retrolight_ab`, `vr_ambient_light_ab`, `vr_extmaps_ab`, `vr_retro_ab` | Debug A/B hides | 4 debug toggles; keep only if A/B shots are still taken |
| `vr_particle_freeze` | "freeze for same-state render comparisons" | 1 read (vr_particles.cpp, retropart's file) |
| `vr_normalmap_cache` 2 | "made and compared with the file" | the compare mode in vr_texcache.cpp (0/1 stay) |
| `vr_hull_method` 0 | Quake 2's brush sweep | the whole brush-sweep path in vr_hull.cpp (5 reads). Large win (each trace's `else` branch and the sweep itself), **hullsmall's file** |
| `vr_hand_rig` 0 | the palm and five finger models | the palm-and-fingers path after `setupRigHand` returns false (vr_view.cpp ~3640). The 27 `vr_finger_*`/`vr_fingers_*` offsets stay (the rig fits to them) but become constants (C) |
| `vr_gore_wash_once` | edges washed 2-3 times | vr_wounds.cpp, 2 reads |
| `vr_torso_mode` | the old engine's 0.8 towards the hands | vr_torso.cpp, 1 |
| `vr_body_elbow_spread` 0 | elbow as before | 1 (it is also a strength 0..1: keep as a slider or fix at 1) |
| `vr_2h_sticky` | 1 = as before (a multiplier) | 1 |
| `vr_gun_wall_slide` | the old push-back along the aim | vr_handpose.cpp, 1 |
| `vr_button_weapon` | only the hand-to-muzzle line | QC buttons.qc, 1 |
| `vr_carry_grab_drawn`, `vr_weapon_grab_drawn`, `vr_box3d_hand_push_fist` | the 4.5 cm sphere / box grabs | vr_physics.cpp, vr_box3d.cpp: 5 |
| `vr_climb_mantle_lenient` | no sloped mantle | vr_climb.cpp, 1 |
| `vr_grapple_rope_depth` -1 | the old rope anchor places | vr_client.cpp, 3 |
| `vr_decap_own_motion`, `vr_decap_pop_chance` 0, `vr_limbs_mass_scale` 0 | the old pop/walk/mass rules | box3d 2, QC 4 |
| `vr_parry_interrupt`, `vr_bash_sound` 0 | the old parry push / old sounds | QC combat.qc, 3 |
| `vr_ragdoll_grab` 2 | "legacy 2" (= 1) | fold 2 into 1 (keep 0/1) |
| `vr_snd_antialias` | Quake's folding lowpass | vr_audio.cpp, 2 (only with vr_snd_fullband 0) |

Risk: each loses its fallback if a regression turns up; all date from 2026-09-24..10-06, so wait one release.

Not candidates, though they also say "as before": `vr_hit_precise`, `vr_knockdown`, `vr_slipgates`,
`vr_melee_positional`, `vr_dummy_gore`, `vr_walltorch_shot`, `vr_limbs_blast`, `vr_messages_hologram`,
`vr_pickup_prop_models`, `vr_grenade_catch` (player-facing taste or comfort), `vr_compat_muzzle` and the
`vr_gameplayfix_*` set by quakevr.cfg (other mods' progs get vanilla), `vr_controller_legacy_pose` (runtimes differ).

### C. Hidden tuning that could be constants (86 archived cvars on no page, medium risk)

Archived, so they ride every config, but no page shows them and the author never changed one. Largest families:
`vr_finger_*`/`vr_fingers_*` (28 offsets, read in vr_view.cpp ~1380-1425), `vr_throw_*` internals (13: window, peak
span, lever arm, angular gains, release thresholds), `vr_box3d_*` (4 substeps/pushes), `vr_bullettime_*` (4 sound
paths), `vr_forcegrabbable_*` (4), `vr_upscale_*`/`vr_foveated_*` (4), `vr_spinreload_*` (2), and ~25 singles
(`vr_item_float_height`, `vr_melee_wrist_speed`, `vr_2h_spread_reduction`, `vr_player_stepsize`...). As
`static constexpr` they drop out of vr_cvars.inc, the configs and QC's handle table. Risk: live tuning from the console
goes; the throw internals are the ones most likely to be wanted again (THROWING.md).

### D. Duplicated concepts (medium risk: some are map or QC interfaces)

1. `vr_hipnotic_available`/`vr_rogue_available` (QC's `VR_Pack_*`) and `vr_hipnotic_status`/`vr_rogue_status`
   (written, never read; "0 missing, 1 available, 2 corrupt"): available == (status == 1). Keep the status pair, read
   it in vr_packutil.qc, drop the two available ones. Same for `vr_dopa_status`, `vr_mg1_status`, `vr_mg3_status`,
   `vr_campaign_schema`: written, read by nothing; documented in EXPANSIONS.md as the "official native campaign ABI"
   for QC and maps. Remove only if no map is meant to read them.
2. `vr_activestartpaknameidx` (the old three-campaign hub selector, archived) beside `vr_campaign`: start-map buttons'
   targetnames carry it (QC buttons.qc:85, 466) and vr_gamedir.cpp syncs it. Merging needs the hub maps changed; high
   risk, low gain.
3. Per-feature haptics: `vr_disablehaptics` plus 8 strengths/toggles (`vr_pain_haptics`, `vr_holster_haptics`,
   `vr_counter_haptic`, `vr_grapple_haptics`...). One master strength plus on/off per feature (MENU_INVENTORY.md 5d, removed 2026-10-06; git history).
4. Effect hues: 10 `*_hue`; most default to the player's (-1). Keep `vr_player_hue`; the others as overrides on one
   page, or gone.
5. Menu duplicates with different labels or ranges for one cvar (MENU_INVENTORY.md 5c: `vr_snap_turn`, `vr_turn_speed`,
   `vr_disablehaptics` inverted...): menu-only fixes, no cvar removed.

### E. Test and debug knobs (125; keep, but prune finished ones)

None is archived, and the Debug pages are where they belong (the project's rule: test aids go to the Debug menu). Prune those
whose feature is finished and whose QC test file is no longer run: `vr_smallgibs_test*` (8, QC/vr_smallgibs_test.qc),
`vr_test_weaponinst*` (2), the per-feature `vr_*_test` that only scripts in Misc/quakevr use. Each removal takes its QC
test file and Debug rows with it. Low risk, small gain.

### F. The config migration (no cvar removed; a code path)

vr_cvars.cpp keeps 303 defaultChanges entries and ~25 version-gated blocks (configs 1..94). Before the first public
release that carries this config format, a cut-off (configs older than N reset to defaults) deletes most of them.
Risk: a tester's old config keeps old defaults.

## Not done here, by design

- Nothing in vr_hull.cpp, vr_ao.cpp, vr_particles.cpp or the shadow/lighting code was touched (parallel workers):
  their candidates are only listed (A: vr_mhull_*; B: vr_hull_method, vr_particle_retro_fast, vr_particle_freeze).
- eval.sh was not run (the melee takes are missing in this worktree's base); no melee or physics code changed.

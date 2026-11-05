# Round 24: from 2026-11-05, towards 1.1

A per-feature reference: current behaviour only. Earlier work: `ROUND23.md` (1.1.0-beta4 to 2026-11-05: 8,600 lines, grep it, never
read it whole), `ROUND22.md` (1.0.1 to beta4). Add new work as a short section per feature, or fold it into its section: what it does,
settings and defaults, files, tests, open issues. Keep sections short: this file is meant to stay small.

## Still grips (super nailgun's off hand) and the shotgun's stray vertex

- Cause of the super nailgun's drifting off hand: `animationMotion` (vr_view_weapon.cpp) carries a hand holding a weapon with the rigid motion of the model's vertices round the grip, so a spinning barrel beside the grip turned the hand (5.1 units at the grip over the spin frames).
- New hotspot field 25, `hsN_still` (Weapon Offsets > Hotspots > "Ignore Animation", Grip only; default 0): the hand on it ignores the firing animation, frame 0 assumed (`gripMotion`). On for the super nailgun's hotspot 1 (weapons.txt slot 4, and the slots that inherit it).
- Test: `vr_still_grip_test` (console; `map e1m1;wait30;vr_still_grip_test`): every Grip hotspot over every pose; a still grip's hand moves 0.0000 (the animation alone: 5.116), all other grips equal `animationMotion`. 4 passed, 0 failed.
- Shotgun: `Misc/quakevr/mdl_motion.py` reads a model's poses (per-vertex motion and deviation from its neighbours). In `progs/v_shot.mdl` no single stray vertex shows in the data: near the stock (x < 12) the largest deviation from the neighbours' median is 0.77 u (v75, v83, v172, v173, a symmetric group) in shot3; the whole stock follows the kick to the byte grid. Not fixed, no model change. Candidate left: the engine's kick cancel (vr_kickcancel.cpp, `vr_kickcancel_info progs/v_shot.mdl`: 58 moving-part vertices, 55 are the muzzle flash).

Follow-up: `vr_kickcancel_info` now lists the moving parts by vertex. Besides the 55 flash vertices, v55, v74, v81 (coincident, the stock's top corner at 4.0 -1.8 5.7) were parts; `Misc/quakevr/fix_shot_stock_corner.py` puts them on the gun's fitted rigid kick in v_shot.mdl and vr_pumpbody_on_v_shot.mdl: parts now 55 (flash only). The second still-grip slot is slot 11 (inherits slot 4, correct). `vr_menu_path_check maps/vrcalibration.map`: 6 found, 0 missing.

## Weapons by ammo boxes (nail pistol, heavy revolver)

- `SpawnAmmoBoxWeaponsImpl` (items.qc), after the 45% box gate (times `vr_ammobox_drops_chance_mult`): nail boxes get the nail pistol last in the chain after the axe (chance 0.15, no ownership gate: 5.4% per box with no nail weapon, 3.2% with the nailgun, 1.6% with both); shell boxes get the heavy revolver after the axe, before the shotgun row (chance 0.10, only with the super shotgun or a revolver held: about 3.6% per box). Not the flare gun or chainsaw.
- The rules live in `TryAmmoBoxDropRule` (vr_weaponutil.qc; `AMMOBOX_RULE_*` in vr_defs.qc); `TryAmmoBoxDrop` keeps its signature (rule: owned). `VR_WeaponUtil_EntIsEligibleForWeapon` (shared with monster drops and armed replacements) is untouched. In a Survival Arena run the rules do nothing (the run's pack rule as before); `vr_ammobox_drops` always drops the revolver without the gate, disable drops nothing. Chances are literals like the chain's others (no tunable).
- Armed grunts (read only): the replacement tiers (vr_armedrepl.qc) put the revolver in the rare tier, gated on a player owning one (`VR_AGR_Owned`) and the rare map index, so they cannot hand it over before the player has it. The pool (`VR_AG_PickWeapon`: an armed grunt placed with no gun; `vr_agrunt_w_revolver` 0.25) has no ownership or map gate.
- Test: `vr_arena_test 37` (any open map off the arena: `map e1m1;wait60;vr_arena_test 37`): 8 rows of 10000 real boxes; ends `arena: test 37 over: P passed, F failed`.

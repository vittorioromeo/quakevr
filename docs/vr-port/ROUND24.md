# Round 24: from 2026-11-05, towards 1.1

A per-feature reference: current behaviour only. Earlier work: `ROUND23.md` (1.1.0-beta4 to 2026-11-05: 8,600 lines, grep it, never
read it whole), `ROUND22.md` (1.0.1 to beta4). Add new work as a short section per feature, or fold it into its section: what it does,
settings and defaults, files, tests, open issues. Keep sections short: this file is meant to stay small.

## The armour's look (branch `agent/armorlook`)

Player and armed humanoids alike, in the shared generator; his mockup (too clean and cartoon-saturated before; shards, grime, less red).

- **Shapes:** `vr_agarmor.cpp` `platesOf`: a piece is rows of separate plates, each a closed bevelled slab of 20 triangles. Places from the piece's own surface (the clipped, smoothed region as before), by rays out from its axis: rows up it, columns round it, each row offset by half a column. Each row laps over the one below and stands further out at its lower edge; neighbours keep a small gap; edges lean a little (the trapezoids), sizes and tilts vary by `vr_agarmor_seed` (0; with the model's name: the same seed, the same armour). A plate rides rigidly on the weights under its middle, so it slides over its neighbours as the body bends. It stands off over the body's bulge and ridges under it (at most 1.2 units); a plate wholly inside the body at rest (under an enforcer's pack) isn't made. A one-sided patch gets a grid instead, so every piece has plates. The helmet keeps its shape, new paint.
- **Paint:** `make_agarmor.py`, now from noise and shapes alone in full colour: `progs/vragarmor.mdl_<n>.png` (512 x 256, nine skins: tier x wear, `VR_ExternalSkin`), `vragarmor.mdl_0_norm.png` (the relief: rivets, pits, scratches, dents, plate edges). A plate maps onto one of 20 tiles: its own tone, worn steel on its bevel in broken lengths, grime under the plate above and in its corners, rust as an accent on edges and a patch on some, painted rivets. Sides and gaps are the grimy `rim` block. Red is held back most, and still red: `PAINT` is tuned against the real renderer, which reads more saturated than the numbers (a red of 126,30,28 looked orange and clean, 106,38,32 brown). The `.mdl` keeps 8-bit copies (nearest palette colour), used by the debris: a broken plate's colours are that bit rougher. Rivets are painted and in the relief, not modelled: the budget went on plates.
- **Triangles:** limit 3400 a body (`vr_agarmor_look_check`). Player 2 392 on every build (default bare forearms 23 016 before), grunt 2 712 (3 436), enforcer 2 512, knight 2 872, death knight 2 792. Model made in 4-20 ms, as before.
- **Hues** (plates' mean, in the check): green 87, yellow 43, red 5 degrees; red's saturation 0.60. In the map's light green still reads teal.
- **Checks:** `vr_agarmor_look_check` (Debug > Tests; `agarmor look: N passed, M failed`): same seed same hash, another seed another, within budget, every piece has plates on all wearers and builds; the plates' mean hue at least 30 degrees apart between tiers; red's saturation within 0.35 .. 0.72. `vr_agarmor_check` still applies (closed slabs, the rest pose's fit). `vr_pp_test 10` the sheet's shots where you stand; 11 every wearer and body build. Scene: `armor_look`.
- **Painted body skin:** not retired; the cuirass's last row reaches the belt so it doesn't show at the waist.
- **Performance** (`--lto`, `bench.sh`, 3 reps, 2048 eyes, 90 Hz paced, exclusive; every armed humanoid in full red, `scratch/al_bench.cfg`), before -> after: `play_e1m1_fight` CPU 3.17 -> 3.00 ms, GPU 3D 2.02 -> 2.03; `crowd32_armed` CPU 4.33 -> 4.30, GPU 3D 2.85 -> 2.76. Within noise; p99 frame 11.27 -> 11.29 and 11.23 -> 11.26.
- **Open:** the green reads teal in the game, more than its paint: as it did before. Plates leave the joints bare (knees, elbows, upper chest beside the neck): by design, for his eye.

## The player's armour plates (branch `agent/playerplates`)

The armour you wear drawn as plates on your body, a look alone: `.armorvalue`, `.armortype` and T_Damage's rules are untouched.

- **Plates:** `vr_agarmor.cpp`'s generator on `progs/vrbody*` (each build, bare or not): its own bones taken for a rig (`playerRig`), its mesh's weights (`buildSkeletalBody`), skinned on the body's matrices, so they follow the IK. No second armour system; only the player's state is apart.
- **State:** QC `vr_playerplates.qc` polls the armour each frame (`PlayerPostThink`), whatever changed it. `.vr_pp_type` (tier by `.armortype`) and `.vr_pp_mask`, fields of their own because combat.qc reads a wearer's `.vr_ar_*` for damage. Sent to the clients as `QVR_SVC_ARMOR` (`serverFrame` now loops the client edicts too). A save keeps them; parm75 carries the mask to the next level (only the base game's parms: on an Arcane Dimensions level the count is right, the pieces canonical).
- **Count:** `1 + floor(min(1, value / most) * 9)` with most 100/150/200: 10 at full, 5 at half, cuirass alone at 1, none at 0.
- **Shed:** limbs and helmet first, nearest `.vr_pp_hit` (T_Damage: the inflictor's middle) when a hit just came; else the reverse of the put-back order; the cuirass last. The humanoids' crack (`VR_AR_BREAK_SOUND`) and debris (`VR_AgArmor_DebrisOf`, models `progs/vrbody.mdl#plate<n>`, precached at worldspawn, no debris in deathmatch) and a pulse in both hands. The helmet sheds no debris: it would be round your eyes.
- **Morph:** `setupPlates` (`vr_view_body.cpp`), `Entities::plate[5]`: 0 the settled pieces, the others one change each. A gained piece comes in with the guns' ammo morph (the alias shader's `Morph`, kind 1's amber seam; `VR_AliasMorph` now allows skeletal models, their noise in the rest pose, `bind_pos`, so it rides the body). A new tier: the old colour dissolves out where the new one appears. A level's or load's first second is taken as it comes (no morph).
- **Helmet:** drawn only where the head is (`plateBonePoses`: the body's own rule, `VR_AliasBonePoses`' `away`): never from the eyes, VR or flat-screen; yes in the mirror clone, spectator cameras and shadows.
- **Limits so nothing obstructs:** the bracer is the forearm's elbow half (forearm and `foretwist1` bones), leaving the wrist gadget, its straps and the hands clear. The pauldron caps (`vr_body_pauldrons`) are hidden on a side whose plate is on, leather (style 1) on a side without: they showed through the plates and would stay armour-coloured after the plate fell.
- **Painted skin (`vrbody_*` armour * 4 + damage, `armor_texel`):** five lames on the torso, in the armour's colour, belt to collar. It is entirely under the cuirass, which is on as long as any armour is, so it doesn't double up in play; only a morph's dissolve shows it through. Left as is; retiring it (the unarmoured skins' torso under the plates) is the author's call.
- **Settings:** `vr_player_plates` (1; Player Appearance), `vr_player_plates_morph_time` (0.6 s). `vr_player_plates_info` prints what's drawn. `vr_agarmor_info` lists the body's plates too.
- **Cost:** model made in 3 ms (lean mesh) to 25 ms (bare forearms' denser mesh: 23k triangles, the lean mesh's 6k) as the body first shows. With `--lto --exclusive`, full red in view, 1024 x 1024 eyes, RTX 4090: host frame 1.08 against 1.01 ms (+0.07), `view body` 0.03 ms CPU.
- **Also fixed:** `makeModel` read the skins' header after growing the hunk, which may evict and clear it: a big model came out without skins, not drawn. `loadMd5Replacement` took `progs/vrbody.mdl#plate0` for the body's md5mesh (no model with a `#` has one).
- **Test:** `vr_pp_test` (`vr_playerplates_test.qc`). `map vrtesthall; vr_pp_test 1` `pptest: 53 passed` (flat: `-ExtraArgs -flatscreenmode`, 52: no spectator camera there); `vr_pp_test 2` `ppsave: 5 passed`; `vr_pp_test 3` `pplevel: 5 passed`; `vr_pp_test 8` the picture sheet; 5-7 Debug > Tests. Scene: `player_plates`.
- **Open:** another player's plates aren't drawn (bots and clients keep the state; nothing draws it, no crash: deathmatch with bots run). Debris is `vrbody.mdl`'s at its model size, whatever the build and body scale.

## Magnetic holster chain on fast swings

The backlog's "worst piece 78 %" was a squeeze, not a stretch: on the fast swing the longest piece was 15.7 % over, the
shortest 78 % under. A hand at 5-8 m/s moves the pinned end most of a piece's length a frame, and the pull-only
`holdLengths` has nothing to push the next point away, so the end runs into it and the pieces bunch (why more iterations
never helped). Second cause: the drawn gun's weight spring snaps back (`vr_weight_spring_snap`), a 60 cm jump in a frame,
and the tether's jump check restarted it dead straight, each piece 12 % short at 65 cm (more closer in) with nothing to
bend them out of the line.

- `verlet::limitPieces` (`vr_verlet.cpp`): after `holdLengths`, each piece kept within `vr_magholster_chain_limit` (0.1,
  0: off; Immersion > Magnetic Holsters > Chain Physics, "Chain Piece Limit"). Up to 3 rounds of a pass from the weapon
  back and one from the holster on, each point moved only to its neighbour's limit, `prev` with it (no speed from it, so
  no energy in); it stops at the first round that moves nothing. A chain within its limits is not touched. Points lying
  on the world are left to the collision (limited too, the corner case's worst piece went from 43 % to 122 %). The tether
  only: the grappling rope and the AD hose are unchanged.
- `hangAfresh` (`vr_magholster.cpp`): a tether started afresh hangs in a parabola of its length, its points even along it,
  stopped by the world below the line.

Test: `Misc/quakevr/magchain_stretch_test.sh <agent>` (`RATE=72` for 72 Hz): slow, fast and whipped moves, the limit off
then on, verdict `magchain_stretch: ... PASS`. `vr_magholster_chain_log` also prints a `magpiece` line per step (stretch,
squeeze, sag). Worst piece on fast swings, limit off / on: 78.0 / 10.0 % at 90 Hz, 78.1 / 10.0 % at 72 Hz (78.0 before any
change too); the slow swing's sag differs by 0.0 mm at both; jerk p95 31 / 30 mm; 0 not finite. The tether's step 0.009 ms
either way (no-LTO exe, ratio only). `magchain_test.sh`: fast tether 78 -> 10 %, slow 2.5 % and fast jerk p95 27.7 mm as
before; jerk max 220 -> 300 mm, on the gun's snap frame. `magchain_sim_test.sh`: 0 pieces through the world; worst piece
the same with the limit on and off: corner 43 %, floor 94 % (a chain lying on the world, not limited).

Open: the lanyard is a spring cord, not this chain: 16.7 % on the same fast swing, untouched. A chain dragged along the
world on a fast swing is not limited where it touches.
## Shoves off ledges: a backpedal, not a launch

His words: "Shoves off ledges look too perfect: whatever the force, humanoids get exactly the force that makes them fall,
every time." A shove with a ledge in its reach (`VR_Knockdown_Ledge`, unchanged) on one that can stagger now goes through
`VR_Knockdown_LedgeBackpedal` (vr_knockdown.qc): the engine's stagger in a ledge mode (`ragdollstagger`'s `sure` 2, the
builtins' table being full; `box3d::ragdollBackpedal`). It backs up on its feet and tips over where the floor ends under its
pelvis, keeping what is left of the shove. Shoves away from a ledge, those that can't stagger (launched as before,
`vr_knockdown_shove_ledge_*`) and hits by a ledge (`vr_kdtrig.qc`'s Over a Ledge) don't change.

- **Tunables** (Debug > Tunables, `vr_knockdown_ledge_backpedal*`): on/off 1 (0: the old launch); `_speed` 0.4 of the shove's
  own speed for its first speed, between `_least` 70 and `_most` 300 u/s; `_vary` 0.2 on that and on how far it would go;
  `_carry` 2, how far it would go at least, times the way to past the edge (it slows evenly over that); `_balance` 0.7 to 1
  by chance; `_yaw` 6 degrees off the shove by chance; `_push` 0.35 of the shove's push on its top; `_hertz` 10, its pelvis's
  pin (a stagger's 5); `_tip` 100 u/s and `_past` 32 units, carried on past the lip so a slow one doesn't sit on it.
- **Ledge mode in the stagger** (vr_ragactive.cpp, `Stagger::ledge`): its steps uncounted; `sure`, so only the floor's end
  takes it over; a foot steps at most to the lip (`lastOnFloor`), never onto air; a foot its hip has left behind slides
  rather than holding (held, it swung the pelvis down and sat the body down about 100 units in, for good).
- **Test**: `sv_random_seed <n>;map vrclimb;wait30;god;vr_agrunt_test 210`, `-QuitOn "test 210 over"`
  (QC/vr_ledgeshove_test.qc): real shoves of a grunt, its back to the long ledge's lip; one hand, two hands, two hands running,
  each from 48 units and from far, 6 trials each; then 6 away from the lip. Measured over seeds 1-4: 144 of 144 over; on its feet 98-99% of the way
  to the lip (one and two hands), 81-89% (running); lip speeds from 48 units: one hand 73-85 u/s (59 to 90), two hands
  133-156 (120 to 176), running 224-239 (193 to 267). Scene: `{test:shove_ledge}`.
  The old launch for comparison (`tune vr_knockdown_ledge_backpedal 0`): lip speeds 234, 378, 557 u/s, each within 3 u/s of
  the shove's own speed; 34 of 36 over.
- `vr_agrunt_test 202` (ledges, vrclimb): the block's case now goes over every time, on a lesser landing: it keeps or drops
  its weapon by the impact rule alone (`vr_agrunt_fall_keep_impact`), and the test checks that.
- **Open**: the look is for him to judge in VR (the far backpedals' sliding back foot above all). A tall drop's ledge grab
  (`vr_ragdoll_active_pullup`) may still catch a slow one at the lip, as it could before.
## Lightning on water

His note: lightning shot into water (thunderbolt, Mjolnir hit, Shambler) should electrify the surface; "used to work".

**What the history says.** No commit broke it. `git log -S QVR_SHOCK_SURFACE` and `-S VR_LGWater_Beam` have one hit,
`35f25522f` (2026-09-30), and nothing since removed a call: the surface's arcs only ever came from the thunderbolt, fired
from the open. Mjolnir's water code (`7ab734e25`) was only the shock (self, burst), the Shambler never had any. The
thunderbolt's own path still works on the old code: fired by `W_FireLightning` the client draws the surface (test below;
the old code: 1 passed, 3 failed). It was never seen for a bolt that shocks you (hand, muzzle or waist in water): burst only.

**Now.** `VR_LGWater_Zap(from, end)` (weapons.qc; `vr_lg_water`, off with it): the look only, where the line crosses a
liquid's surface. `LightningDamage` and AD's `ad_LightningDamage` call it, so every bolt has it: thunderbolt, Shambler, Super
Shambler, Death Lord, traps. `VR_LGWater_ZapBelow(at)` is for a point: in a liquid the surface above it, else within 64 below:
Mjolnir's blow, its arcs wandering over water, and any shock (`VR_LGWater_ShockAt`, also the thunderbolt's). Damage through
the water is unchanged: still only the thunderbolt's (`VR_LGWater_Beam`) and the shock's, a monster's bolt is only the look.
Not done: a hammer slapping the surface with nothing solid is still no blow (melee traces ignore water).

**Test.** `vr_waterzap_test 1` (Debug > Tools > Test Effects > Lightning into Water; `vr_shock.cpp` counts the surface's
drawn frames, QC `watershockdrawn`): `waterzap: 4 passed, 0 failed` on vrfiringrange, e1m3, e1m7, e2m3, e3m4, e4m3, and with
`-RealTime` on vrfiringrange. Checklist scene: `water_lightning`.
## Close two-hand grips: both hands stay where they gripped (branch `agent/cratebound`)

The grip fit of the second hand (`vr_carry2h.cpp`, `fitGrip`) moved its grip out of the prop but left the hand behind: the
grips' middle was then off the hands' middle, so `solve` shifted the prop and each hand sat half the move off its grip (the
first hand pulled along), and the grips' line was off the hands' line (a phantom turn, bigger the shorter the line). Now the
move is also the hand's `Hold::reach`, as a hand held out of a prop already was: at the grab's pose `solve` gives the prop
where it was. A grab with nothing to fit runs as before. When the first hand lets go, the prop jumps by the reach as it does
for a hand held out.

Test: `Misc/quakevr/crate_grip_test.sh <agent>`, `vr_carry2h_grip_test` (a small crate in both mock hands, a second of fixed
frames, each hand's largest distance from its grip, limit 0.5 cm; verdict line `vr_carry2h_grip_test: N passed, M failed`).
The bug reproduces in the mock:

| case | grips | before: each hand off its grip | now |
| --- | --- | --- | --- |
| close, off hand beside the main | 17.8 cm | 0.70 cm (FAIL) | 0.03 cm |
| close, pushed 10 cm in | 17.8 cm | 0.70 cm (FAIL) | 0.03 cm |
| close, pushed in, keep-out off | 17.8 cm | 2.60 cm (FAIL) | 0.03 cm |
| wide, nothing to fit | 94 cm | 0.01 cm | 0.01 cm (the same) |
| wide, the fit moving the grip 23 cm | 94 cm | 11.75 cm (FAIL) | 0.01 cm |

The mock's hands themselves move 0.72 cm over that second, and the crate follows them (2.5 cm at its middle, before and after).
The test's first grab uses `vr_grip_fit_reach 3` (a spawned crate lands on the hand). Scene: `{test:crate_close_grip}`.

Not tested: the drawn hand in a real headset, or hands turning together in a close grip.

## A turned teleporter's exit: the body turned with the gate (branch `agent/cratebound`)

The body carried through a gate was met at the destination with the axis-aligned bound of its turned box: at 45 degrees 41%
wider each way, so a narrow exit the body fits blocked it. Now the destination half of a move (`portals::destHalf`, for
`VR_PortalBodyMove` and `VR_PortalReachMove`) sweeps the box itself, turned with the gate: `SV_MovePortalTurned`, then
`hull::clipPortal`'s `turned` and the sweep's `BoxTurn` (`vr_hull.cpp`). The bound still finds what is near. The box's corners
meet each brush's faces and bevels, and its own faces meet the brush's corners (`clipToTurnedFaces`, worked out only for a brush
its planes haven't missed, near such a gate). Right-angle turns take the old path, unchanged.

Still conservative, never through a wall: recovered clip brushes meet the bound; edge-against-edge axes are left out, so a sloped
edge can stop the box slightly early. A prop tossed through a gate keeps its bound (`toss`'s placement test).

Test: `Misc/quakevr/teleporters/turned_exit_test.sh <agent>` (vrteleporters' new room d45, west of T's west wall door: walks into
gates that come out at yaw 45 into free-standing corridors; verdict line `vr_portals_turned_test: N passed, M failed`). With the
old bound: the 28.3-wide exit stops the 16-unit body (FAIL, the player stays at the gate, 1 passed, 1 failed). Now: through it
and on; the 14.1-wide exit still stops him, his origin 8.4 units short of the gate's plane (2 passed, 0 failed).
`teleporters_test.sh walk` on the new code: `turn45` crosses both ways, and the other gates cross or stop as the map intends (no
run of the old code to compare line by line). Open, from the box sizes and not tested: once carried the body is axial again and
needs 22.6 across a 45-degree corridor, so an exit between 16 and 22.6 wide passes the gate's test and may stop or stick him just
past it.
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
## Shotgun and nailgun looks (branch `agent/gunlooks`)

The author's two mockups as a guide (textures that fit the geometry, no flat faces, wood grain, subtle rust, low rivets; not their rust-brown overall colour, and the glowing sights untouched). His nailgun mockup is `v_nail.mdl` (two tubes, hex rings, muzzle boxes, magazine under the grip), not the nail pistol, compared in renders first.

- **Generator:** `Misc/quakevr/gunlooks.py` (`--capture` once on what polish_weapons.py leaves, then it reruns byte for byte), `bake_normals.py v_shot.mdl v_nail.mdl v_lava.mdl`, `gunlooks_check.py [--kick log]` (`gunlooks_kick.sh <agent>` makes the log). Rerun after polish_weapons.py. Inputs in `src_models/gunlooks/`: each model with its skin blanked but for the fullbright texels and what the unchanged triangles use, and each old triangle's mean colour: no old paint in the new art.
- **Skin:** every face re-mapped into charts of one facing at one density (6 texels a unit on the guns, 4 on the pickups; the old maps ran from stretched streaks to single colours), the shotgun's skin widened from 128 x 215 to 512 x 219, the nailgun's 512 x 210. Painted per texel from the surface point it shows (a solid texture, continuous over seams): metal, wood rings and fibres, worn edges, grime, inset panel lines, flutes on the nailgun's tubes, rust patches (10-12% of the metal) and stains under rivets. Palette only, never a fullbright index. Metal's mean colour goes from about (21,20,23) to (30,29,32): off black, same family.
- **Geometry:** 14 rivets on the shotgun (420 triangles), 10 on each nailgun (300), low hex heads appended and carried by their piece through every frame. None within 1.41 of the sight line, none on the pump's rings; the body part file gains them. Old vertices and triangles keep their bytes and order (a corner may be a coincident copy at the end). The fullbright texels (3292 on the shotgun) stay where they are, byte for byte, with the 225 triangles that use them.
- **Relief:** the bake now takes these heights (`src_models/gunlooks/*.relief.npz`, `normaltiles.py` `MODELS`), not guesses from the paint, whose dither is no shape. Sidecars: only `_0_norm.png` exists for these models (the three guns and two pump parts, all rebaked); they have no glow or bump files, the glow is the skin's fullbright indices.
- **Variants:** `v_lava.mdl` the same mesh and paint from its own colours, its glow texels carried onto the new charts; `g_shot0.mdl`, `g_nail.mdl` (pickups) the same paint, no rivets, no baked relief. The magazines and wells are unchanged. `v_nail2`, `v_lava2`, `g_nail2`, `g_shot` were not part of this.
- **Traps met:** `fx_gfx_mask` names its triangle count and reads as none if it differs: weapons.txt's three masks extended (the author's config holds none). The engine draws the nailgun's tubes about three times darker than the plate beside them, whatever their vertex normals (it was so before: near black): painted 2.4 times lighter (`TUBE_LIFT`), the cause not found. The rivets nudged the kick fit enough to make the stock's other corner (4.0 +1.7 5.7) a moving part of the pump body: `fix_shot_stock_corner.py` now fixes both sides, on the input (6 vertices' firing poses, up to 0.67 units): the one change to old vertices, against the shipped model.
- **Verified:** `gunlooks_check: 68 passed, 0 failed` (faces of 0.5+ square units and 6+ texels wide: shotgun 4.8-6.5 texels a unit, limits 3.9-7.5, stretch at most 1.28, limit 1.7; none of 934 faces of 8+ texels in one colour; moving places none new: 55 parts' vertices on the shotgun as before; nail 232 places as before, more vertices only as copies). `autopump_test.sh`: 7 passed, 7 failed, the same seven on the old models (shell timing; not from this). `vr_menu_path_check` not touched.
- **Sheets:** `C:/OHWorkspace/qvr-kit/scratch/gunlooks/shotgun_before_after.png`, `nailgun_before_after.png` (`r_fullbright 1` for neutral light).

# Round 24: from 2026-11-05, towards 1.1

A per-feature reference: current behaviour only. Earlier work: `ROUND23.md` (1.1.0-beta4 to 2026-11-05: 8,600 lines, grep it, never
read it whole), `ROUND22.md` (1.0.1 to beta4). Add new work as a short section per feature, or fold it into its section: what it does,
settings and defaults, files, tests, open issues. Keep sections short: this file is meant to stay small.

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

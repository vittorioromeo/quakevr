# Armed ragdolls that keep firing (research, and a prototype behind `vr_agrunt_ragfire` 0)

His note (2026-11-28, item 17): active ragdolls still holding a gun (knocked down, held by a player) keep firing it,
trying to aim at the player, at a lower rate and accuracy; one-handed ranged guns only (shotgun, nailgun, nail pistol,
revolver...), never big, explosive or melee weapons.

**Verdict: feasible and cheap as a prototype (done, off by default); good looks need one more engine step (the wrist).**
**ragfire2 (section 4b): the wrist done and the arm lifts; on its back the gun now points 8 degrees off (median), its
shots turned at most 10.**
The weapon already rides the ragdoll's hand, the shot path takes any muzzle and line, and the active ragdoll already
has per-arm reaches. What does not work well yet: a lying ragdoll's gun points 35 to 55 degrees off the player even
with its arm reaching for him (measured below), so the prototype fires along a line turned up to 30 degrees from the
gun's (a visible mismatch at close range) and holds its fire when even that would go into the floor.

## 1. What happens today (before this branch)

- **Its weapon stays in its hand.** A ragdoll (stagger, tug, knockdown, held by a player) keeps its weapons on the
  hand bone that held them: `VR_AG_PlaceProp` -> `VR_AG_RagProp` (vr_armedgrunt_play.qc) places the prop on the drawn
  hand (the engine's `hitreact::drawnWeapon`, `agdrawnframe`), or, on a dedicated server, on the ragdoll limb nearest its
  grip as it was when the ragdoll began (`ragdollpoint`). A violent fall makes it let go (`VR_AG_FallDrop`,
  `vr_agrunt_fall_keep_impact`); a player's grab on a downed one's gun takes it at once.
- **Its AI is off.** `VR_Knockdown_Think` (vr_knockdown.qc) replaces its think: no `ai_run`, no `CheckAnyAttack`, no
  attack frames, so nothing ever calls `VR_AG_ShootProp`. `.enemy` is kept. Its weapon's own think
  (`VR_AG_PropThink`) keeps running every frame (placing the prop, tugs, blows on the weapon, falls).
- **Firing needs** (`VR_AG_ShootProp`): `self` the monster with an `.enemy`, the prop placed (`VR_AG_SetHand` places it,
  and on a ragdoll that is the ragdoll's hand: nothing breaks), the muzzle (`weaponmuzzle(p)`: the drawn pose, so on the
  ragdoll's hand), and the aim from the AI (`VR_AG_AimDir`: grip to target, error, dual kick). Called mid-ragdoll as it
  is, the shot would leave the ragdoll's muzzle but along the AI's line to the target, then converge onto the target
  (`vr_ag_shot_on`) and turn onto the drawn line (`vr_agrunt_shot_drawn`): a perfect shot whatever way the gun points.
  A replacement's own gun (`VR_AGR_NativeShot` -> `army_fire` / `EnemyFireBullets`, the enforcer's laser) re-aims from
  its body's place at the target and bends to its facing's aim cone (`VR_AimCone_Bend`, from `.angles`, meaningless on
  a ragdoll). Its own shots cannot hit it: bullets leave the shooter out of their traces, nails and lasers are owned.

## 2. Design (as prototyped)

- **Which guns** (`VR_AG_RagFireGun`): the shotgun, nailgun, nail pistol, revolver, and the grunt's and enforcer's own
  guns (burst rifle, laser rifle: most map grunts hold these with armed replacements on, so leaving them out would make
  the feature invisible in normal play). Not the super shotgun, super nailgun, launchers, Thunderbolt, Laser Cannon,
  flamethrowers, AD's guns and upgrades, the crossbow, Dwell's variants, the flare gun (it sets you on fire), any melee
  weapon. Dual wielders fire their first slot only.
- **When**: `.vr_knockdown` 1 (staggering, down, held), a ragdoll, not getting up (`vr_kd_rise`), no tug-of-war over its
  gun, a clip with rounds (a native gun always), a live target; held in a player's hands only with
  `vr_agrunt_ragfire_held` 1 (default).
- **Aim = the gun's own line, steered within limits.** Every frame its gun arm (the hand nearest the grip) reaches from
  its shoulder towards the target: a new active-ragdoll drive, `ragactive::Aim` (vr_ragactive.cpp `aimFrame`, the
  guard's pattern: the arm's two-bone reach, eased in 0.2 s and out 0.3 s, at `vr_agrunt_ragfire_aim` strength, under
  a head guard, over a stagger's, fall's or struggle's arms; the environment's hand reaches leave that arm alone; its
  shoulder cone widened as a struggle's is, `vr_struggle_arm_reach`). The QC learns the gun's turn off the arm
  (`vr_ag_rf_corr`, at most ~35 degrees) and aims the arm past the target by it. It fires only while the gun points
  within `vr_agrunt_ragfire_cone` (60) of the target; the shot is turned from the gun's line by at most
  `vr_agrunt_ragfire_steer` (30), then off by up to `vr_agrunt_ragfire_error` (5) at random, on top of the gun's spread.
  The kinds not driven alive (AD's soldiers outside the active classes) get no reach: they fire only when the gun
  happens to point near.
- **Rate**: one shot every `vr_agrunt_ragfire_interval` (1.6 s; nails `_nail_interval` 0.6 s), up to half again at
  random, the first an interval after it became a ragdoll.
- **Never itself, never through the floor**: the grip-to-muzzle line must be clear (the gun lying across the floor or a
  wall holds), and the muzzle-to-target line along the chosen direction must reach the target or the target's distance
  without hitting the world or another monster (else held, looked at again in 0.2 s); `VR_AG_ShootProp`'s own friend
  and blast checks still apply.
- **The shot itself is the standing one's** (`VR_AG_ShootProp`: clip, ammo, flash, sounds, kick, counts), with a global
  `vr_ag_rf_dir` that skips the AI aim, the convergence and the drawn-line turn; `EnemyFireBullets` and
  `VR_AGR_NativeShot` take it too (from the gun's muzzle, no aim-cone bend).
- **Held by a player**: it keeps firing (the struggle's arm drive and the aim share the arm: the aim wins on the gun
  arm). Point-blank at the holder is possible; it is fair because a downed or held one's gun is taken at once by a
  grab (`vr_agrunt_fall_keep`'s "down, a grab takes it at once"), it fires at most every 1.6 s with an error, and the
  holder can throw it. `vr_agrunt_ragfire_held 0` turns this off if it feels cheap.
- **MP**: all server side (the QC, the ragdoll drive). A listen server aims from the drawn hand; a dedicated server from
  the ragdoll limb (`VR_AG_DrawnFrame`'s fallback). Clients see the shot as any armed humanoid's (flash on the prop,
  tracers from the muzzle). No new network messages.
- **Saves**: fields `.vr_ag_rf_next`, `.vr_ag_rf_since`, `.vr_ag_rf_shots`, `.vr_ag_rf_corr` (saved by name, all
  harmless when stale); the engine's aim state is not saved (re-asked every frame).

## 3. Results (test 237, headless, vrtesthall, the gun holder toppled 80 units ahead)

| Case | Shots while down (6 s) |
| --- | --- |
| grunt, revolver | 1 to 2 (passes at 1; 10 of 11 due shots blocked after the origin/next merge: it lay with its line into the floor) |
| enforcer, nail pistol | 2 |
| grunt, nail pistol | 2 |
| grunt, rocket launcher | 0 (excluded) |
| grunt, revolver, setting off | 0 |

Measured on the way (vr_agrunt_debug 2, `vr_debug_ragdoll 2` prints `ragaim:` lines): lying on its back, its arm's
reach on at weight 1 and strength 0.8, its hand stays ~0.45 m from the reach's goal and the gun points 36 to 59 degrees
off the player, down along the floor. An enforcer's shotgun (a long gun) lay muzzle down: every due shot was held as
blocked (correct: it would hit the floor), so the long-gun case only fires when it lands well. The cause is most likely
the limp hand/wrist (the reach drives the upper arm and forearm only; the gun rides the hand, which droops) and the arm
lying on the floor under its own weight. With a 35-degree cone and a 10-degree wrist (the first defaults) it fired
about once in 6 s.

## 4. Feasibility, cost, risks, effort

- **Cost**: per armed ragdoll per frame, two short traces only when a shot is due (at most every 0.2 s), one
  `ragdollaim` call (a few vector ops), the reach IK on one arm (already paid by guards and grabs). Nothing when off.
  Quest: negligible (armed ragdolls are few; the struggle and guard already run the same IK).
- **Risks**: the visible mismatch (shots leaving up to 30 degrees off the drawn gun); downed enemies feeling unfair at
  point-blank (tunables, and the grab counter); stray shots into allies near the downed one (the friend check covers
  the line, not the spread); interplay with ragknock's knockdown criteria (none: the triggers are untouched).
- **Effort to finish**:
  1. *Drive the hand* (engine, ~1 day): add the wrist to the aim (rotate the hand bone so the gun's forward meets the
     target: the gun's local forward is known from the grip offsets, `aggrip`), and raise the arm off the floor (a
     stronger upper-arm drive, or a short lift of the forearm). Then cut `vr_agrunt_ragfire_steer` to ~10 degrees.
  2. *Pose the fall*: a downed one rolling to its side towards the target before firing (the get-up's roll exists).
  3. *Dual guns*, the long guns' second hand, AD's armed variants outside the active classes, a reload while down.
  4. Tuning in VR (rate, error, cone) and a decision on the held case.

## 4b. ragfire2: the wrist, and why the arm never lifted (branch agent/ragfire2, 2026-11-28)

His words: "Can you make that fix?" (steer the hand, then bring the shot's turn back to ~10 degrees).

- **The wrist** (`vr_agrunt_ragfire_wrist` 1, Debug > Tunables "Ragfire: Wrist"): the QC hands `ragdollaim` the gun's
  drawn line too (`ragdollguard`'s optional `barrel`); the engine learns it in the hand bone's axes (`Aim::local`,
  eased: the gun is fixed in the hand) and turns the hand bone's joint target so that line meets the way from the grip
  to the target (`aimWrist`, vr_ragactive.cpp, after the arm's reach), clamped by the rig's wrist (the soldier's and
  enforcer's hand_l/_r swing 30 degrees), at the aim's strength. Rigs without a hand bone get no wrist (as before).
- **Three things held the arm on the floor** (found with `vr_debug_ragdoll` 2/3's `ragaim:`/`ragaim2:` lines):
  1. *The shoulder's cone was never widened*: `shoulderReach` asked Box3D for 150 degrees, and
     `b3SphericalJoint_SetConeLimit` skips anything over 90 (`B3_CHECK_INPUT`), so the arm kept the rig's 40 and its
     goal was out of range. The aimed arm's cone is now clamped to 90. **A struggle's own 150 meets the same skip** (its
     hands never go over its head): left as it is here (a BACKLOG line).
  2. *The knockdown's stirring* (`vr_knockdown_stir_*`) picked the gun hand or arm and took it from the drive (its
     `yield` bit zeroes the joint's strength): the wrist went limp. The aimed arm's bones are never stirred now
     (`aimedArm`, vr_box3d_ragdoll.cpp).
  3. *Too weak*: with the cone fixed, the shoulder's motor still sat at its cap 15 degrees short. The aimed arm and
     wrist get a struggle-like arm drive (`aimArmDrive` 2: springs' hertz and motors' cap), still short of a turret: it
     sways and lags as it lies.
- **Defaults**: `vr_agrunt_ragfire_cone` 60 -> 35, `vr_agrunt_ragfire_steer` (now "Shot Turn") 30 -> 10; rate unchanged;
  the muzzle-in-floor and line checks unchanged. Still off by default (experimental). The QC's learnt arm turn
  (`vr_ag_rf_corr`) stays: without it (measured) the gun was further off, the wrist's 30 degrees too few on their own.

Measured (test 237: 3 guns on their back, front, left and right side in two rounds, then staggering, each down 6 s;
the drawn gun's angle off the player each frame once the arm has eased in; before = the prototype, cone 60, turn 30):

| Pose | Before: median / 90th % | After: median / 90th % |
| --- | --- | --- |
| On its back | 44 / 50 deg | 8 / 20 deg |
| On its left side | 74 / 104 | 44 / 58 |
| On its right side | 56 / 86 | 38 / 58 |
| On its front (gun under it, you behind it) | 122 / 148 | 82 / 170 |
| Staggering | not measured (the first stagger case recovered at once) | 20 / 58 |
| All poses | 64 / 128 | 32 / 122 |
| The shots' own drawn gun angle | 40 / 50 (56 shots) | 8 / 20 (82 shots) |
| Shots per 10 s down | 3.8 (cone 60) | 5.0 (cone 35) |

Left as is: on its front it can't bring its gun round (needs section 4's "Pose the fall", a roll); on its side the gun
arm is often the one under it. Held by a player the same aim and wrist run (not in the test: it needs the mock hand's
grab).

## 5. Try it

`vr_agrunt_ragfire 1` (VR Settings > Armed Grunts > Loadout: Fire While Down (Experimental)), the tunables under
Debug > Tunables (search "Ragfire"), the checklist scene `agrunt_ragfire` (a revolver and a nail pistol grunt, a
shotgun enforcer, a rocket launcher grunt that must never fire while down). Test: `vr_agrunt_test 237`.

Files: QC `vr_armedgrunt_ragfire.qc` (the rules), `vr_armedgrunt_ragfire_test.qc`, hooks in `vr_armedgrunt.qc`
(`VR_AG_PropThink`, `VR_AG_ShootProp`, `VR_AG_DrawnMuzzle`), `vr_armedrepl.qc` (`VR_AGR_NativeShot`), `weapons.qc`
(`EnemyFireBullets`), `builtins.qc` (`ragdollaim`: `ragdollguard`'s optional grip and strength, the table being full);
engine `vr_ragactive.cpp` (`Aim`, `aimFrame`, `aim`; envFrame leaves the aimed arm), `vr_ragstruggle.cpp`
(`shoulderReach`), `vr_box3d_knockdown.cpp` (`ragdollAim`), `vr_builtins_ragdoll.inc`.

# Flinch: hits as brief active ragdolls

Every hit strong enough briefly makes a humanoid an active ragdoll. The hit's impulse goes into its body: part of it at
the point struck (the jolt), the rest along the hit, so it steps. Its arms and torso balance it. Once it is balanced, it
blends back into its animation and its AI goes on. If it is not balanced in time, or the hit took it past its balance's
edge, it topples into the knockdown and gets up as any knockdown does. `vr_flinch 1` is the default. `vr_flinch 0`
gives exactly the behaviour from before: the small hit reactions, Quake's pains and Hits Unbalance.

Code: `QC/vr_flinch.qc` (the impulses, the safeguards, the tests), `Quake/vr/vr_box3d_knockdown.cpp`
(`ragdollFlinch`, the budget, the hand-back, `vr_flinch_status`), `Quake/vr/vr_ragactive.cpp` (`beginFlinch`, the
recovery test inside `staggerFrame`, `flinchDraw`). Settings: Combat > Flinch. Tests: Debug > Tests, "Flinch". Notes
and numbers: ROUND22.md, "Flinch".

## Architecture

The flinch reuses the shove's stagger (KNOCKDOWNS.md, ROUND22 "Active ragdolls: stagger, judo, get-up"):

1. `T_DamageDeal` calls `VR_Flinch_Hit` before the small reaction. It covers shots, nails, missiles and blasts. A melee
   blow and a thrown prop come later, from `VR_HitStagger_Melee` and `_Throw`, after their damage, because those carry
   the zone and the momentum. Shoves, bashes and headbutts stay with the knockdown.
2. `VR_Flinch_Try` adds the impulse to the hits summed on that enemy (half-life 0.2 s), checks the cooldown, then asks
   the engine (`ragdollflinch` mode 0). The engine checks the class, the budget, the range and room, and returns the
   change of speed the summed impulse gives the whole body (|level impulse| / mass, its attack frames guarded).
3. Under `vr_flinch_min`, the small reaction plays (`VR_HitReact_Hit`). Past it, `VR_Knockdown_Start` makes its ragdoll
   from the frame it is drawn in (`.vr_knockdown 1`), and `ragdollflinch` mode 1 begins the flinch. `VR_Knockdown_Think`
   then polls `ragdollstaggered` as it does for a shove's stagger: 2 means recovered (back on its AI), 0 means it fell (a
   knockdown).

AD's monsters take damage in AD's own `ad_T_Damage`: its bridge (`VR_AD_Hit`, vr_ad_bridge.qc) calls
`VR_Flinch_Hit` the same way, and AD's pain then gives way to the flinch as it does to a knockdown.

## Impact model

| hit | impulse (kg m/s) | where |
|---|---|---|
| shotgun | 7 per pellet that hits (6: 42) | the pellets' mean point |
| super shotgun | 12 per pellet (14: 168) | the same |
| shotgun distance | full within 128 units, easing to 0.25x at 1024 | |
| nails, missiles' own hits | 1.2 per damage (a nail 11, a super nail 22) | where it touched |
| other hitscan, monsters' blows | 0.6 per damage | |
| melee blow | 3 per damage (its speed, weapon and zone are in its damage) | the point it met (a head blow jolts the head) |
| thrown prop or weapon | its momentum (mass x speed) x 1 | its middle |
| blast | 3 per damage, away from it | the box's nearest point |

A grunt weighs 80 kg, so 6 close pellets give about 0.52 m/s, a point-blank super shotgun 2.1 m/s, a 30-damage blow
1.1 m/s, a 60-damage blast 48 units away 2.25 m/s, and a 12 kg crate at 10 m/s 1.4 m/s. An ogre (200 kg) takes 40% of
that.

## Switch-in

The knockdown's path: the ragdoll is built from the current pose. Rigs are made once per class and cached; a body is
made in about 0.15-0.24 ms (the query to the drive begun, measured in `vr_flinch_status`), so no pool is needed. A
`vr_flinch_local` share of the impulse (0.35) goes onto the part nearest the point (`b3Body_ApplyLinearImpulse` at the
point). The rest becomes the stagger's travel: its pin's first speed is that share of the change of speed.

A blow or a throw reaches the flinch after its damage, when its pain has already set the pain's first frame. The body
is made in the frame it had when the damage met it (`vr_fl_prepain`, set in `T_Damage`), not in the pain's: the
enforcer's `paind1` bends its chest 35 degrees over, and it never straightened.

## Balance controller

This is the stagger's (`staggerFrame`). The pelvis is pinned and carried along the hit, easing out. The muscles hold
up its weight, and the whole body turns upright (`turnWhole`). The feet stay planted, and the arms wheel as strongly as
the hit (`wheel`: the change of speed over Topple Speed). The torso and head come back to full tone once the travel is
half done, which straightens the spine: as their tone comes back, their joints' motors turn them towards the stand
frame (4 of the way a second, at most 3 rad/s; `drive`). Held still, a motor keeps any bend up to twice its weight's
torque, so a torso struck bent (an attack's aim) stayed bent and failed the tilt test. A flinch takes a capture step only while its capture point (its middle plus its
level speed times sqrt(height / g)) is off its feet's support. A shove's stagger backpedals regardless. A flinch's
steps put each foot where its stand frame has it round its middle (`Stagger::stance`, from the stand frame in
`beginFlinch`), not where it stood when struck: struck mid-stride or in an attack frame (its middle up to 9 units off its
feet), keeping that stance would leave it as far off its support after every step.

Balance is 1 - 0.5 x the change of speed / Topple Speed (2.0 m/s). Topple Speed is times `vr_flinch_uneven` (0.65) when
the floor under it is uneven (four rays round its origin: a slope steeper than about 10 degrees, stairs, an edge).

- At or under the edge (balance 0.5 or more): it means to recover.
- Past it: it takes a step or two, then falls (no recovery).
- At 1.5 x Topple Speed or more: it goes down at once.

## Recovery test

Every frame, from `vr_flinch_recover_min` (0.25 s):

- its centre of mass is over its feet's support, reckoned round where its stand frame's own centre of mass stands
  over its feet (`supportOffset`: an enforcer leans over his rifle): each foot a disc of Foot Support (0.18 m, larger for legs longer than
  0.75 m), and the strip between them;
- its middle's level speed is under 0.35 m/s;
- its pelvis and chest are tilted at most 22 degrees off how its stand frame holds them;
- no step is under way;
- each foot is over floor within 0.75 m (a foot over a stair's edge), no steeper than 35 degrees, and one foot is
  within 0.3 m of it.

When this holds for 0.08 s, it has recovered. At `vr_flinch_most` (2 s), it settles if it is still, leaning less than
18 degrees over its feet, not bent past 1.5x the tilt and on firm ground. Otherwise it topples into the knockdown. If a
step is under way at that point, it gets 0.3 s more to land.

## Blend-back

The candidate frames are its stand cycle (every class's first eight frames), the frames named stand or run, and the
frames within 3 of the one it was struck in. Death frames are left out. Each candidate is fitted to the body
(`fitFrame`: its turn about the vertical and its place). The best fit wins, and the entity's origin and yaw follow the
body there. On slopes and stair edges, the stand spot may lift the box up to a stair (`standSpotLifts`, the flinch's
only). The ragdoll's last pose blends into the animation over `vr_flinch_blend` (0.22 s), scaled 0.7x to 1.4x by the
pose's gap (its mean vertex distance from that frame; 0.15-0.3 s). The gap, the pop metric, is in `vr_flinch_status`
and in the `flinch: ... back in frame` lines.

## Budget and fallback

- At most `vr_flinch_max` (6) flinch at once.
- Beyond `vr_flinch_range` (1536 units) from every player, it does not flinch.
- With `vr_ragdoll_max` full, a dead body may go for it, never a living knockdown (`VR_Flinch_Room`).
- Anything refused plays the small hit reaction.
- The drive costs about 0.006 ms per flinching enemy per frame.

## Gameplay safeguards

- `vr_flinch_min` (0.3 m/s): a weaker hit plays only the small reaction. Quick hits add up (a burst of nails).
- `vr_flinch_cooldown` (0.6 s): no new flinch until then (no stun-lock).
- `vr_flinch_attack_guard` (1: off): during its attack frames (named shoot, attack, magic, char_, slice, smash, swing,
  ...) the hit counts as this many times less.
- Pains: a flinch never plays Quake's pain frames (it is a ragdoll; its pain sound plays). `vr_flinch_pain 0` also keeps
  them off for hits too small to flinch.
- Classes (`vr_flinch_classes`): every class that can stagger as a ragdoll, except the zombies and the mummy (they keep
  their own falls). Hits Unbalance gives way to the flinch for the classes that flinch.

## Debug

- `vr_flinch_debug 1`: the overlay and the log.
  - The overlay: the hit (a white arrow, a second), the centre of mass with a line to the floor, and each foot's disc
    with the strip, coloured yellow (balancing), green (steady), cyan (recovered) or red (toppled).
  - The log: a `flinch:` line for each hit, recovery, topple and hand-back.
  - `2` also prints the refusals.
- `vr_flinch_status [reset]`: counts, switch-in cost, recovery time, hand-back gap, and each flinching enemy's balance.
- `vr_flinch_test <n>`:
  - 1-9: shotgun, super shotgun, nail, super nail, blow, head blow, blast, throw, crowd;
  - 20: a grunt at `vr_flinch_test_x/_y/_z`;
  - 21: ten grunts ahead.
- The test room is `vrtesthall`: flat floor, a 22-degree slope (x -240..-80, y 460..620) and the stairs (x 96..288,
  y 544..640).

## Later

Leg sweeps, leg grabs and judo stay as they are; they may come from "enough force on a limb" later. Rough edges are in
ROUND22.md, "Flinch".

# Feedback round 8: plan and notes

| # | Request | Status |
|---|---|---|
| 1 | Parry unarmed: arms crossed in an X in front | done |
| 2 | Bash: push forward hard while parrying: low damage, strong knock-back, stagger | done |
| 3 | Bash unarmed: shove with both hands | done |
| 4 | Pickup particles much more subtle and slower | done |
| 5 | Glowing outline on pickups targeted by force grab, fading smoothly, subtle | done |
| 6 | A force grab effect (trail, lightning from the hand, ...) | done |
| 7 | Blood and scorch decals, performant, from most weapons and interactions | done |
| 8 | Pick up and throw gibs and heads; their box matching the model | done |

## Notes

1. **Unarmed parry** (`VR_Parry_ArmsCrossed`, `combat.qc`; Gameplay > Parry and Bash). The condition: both hands
   empty, in front of the body between the hips and the head, within 16 units of each other, and on each other's
   side (crossed). The blow is taken on the forearms:
   - `vr_parry_unarmed_reduction` (0.5) off the damage;
   - a thud, both hands knocked, a puff;
   - the attacker pushed back, you less.

   Nothing can be dropped. **Not tested in the mock** (it needs a monster's blow at the right moment).
2. **Bash** (`VR_Bash`, `vr_juice.qc`): a guard driven forward faster than `vr_bash_speed` (1.6 m/s).
   - The guard is a weapon held across in front (the parry pose) or both empty hands together, crossed or side
     by side.
   - Monsters just ahead take `vr_bash_damage` (8), are thrown back (`vr_bash_push`), and stagger: their pain
     animation is forced whatever their own chance of flinching, and they cannot attack for a second.
   - The feedback is a clang (with a weapon) or a thud (hands), both hands knocked back, and haptics.
   - The next bash waits 0.8 s; a push at the air costs nothing.
3. **Two-handed shove:** the bash with both empty hands. **Tested:** two empty hands jumping 35 cm forward at an
   e1m1 grunt: "bash: 1 (hands)", the grunt in its pain frames.
4. **Pickup sparkles:** weapons in the level give one faint, small sparkle every quarter second, drifting up
   slowly (Quake VR's were two bright, fast ones every twentieth of a second). The force grab target's sparkles
   are as faint and rarer, since the outline now shows the target.
5. **Force grab outline** (`vr_forcegrab_outline`; Force Grab page):
   - What a hand points at glows softly: a rim light (brighter where the surface turns away from you) and a faint
     tint, in a cool blue.
   - It fades in and out smoothly and breathes a little: fainter when aimed at, full when locked on or flying.
   - It is drawn by the model shaders (alias models, and brush models for the ammo and health boxes), from a
     glow value per entity.
   - The server sends each hand's force grab target and state as two stats (`STAT_QVR_FGMAIN`,
     `STAT_QVR_FGOFF`), and `vr_fgfx.cpp` fades each entity's glow.
6. **Force grab effect** (`vr_forcegrab_fx`):
   - aiming: a faint straight beam from the palm;
   - locked on: a crackling tendril of energy, two strands of jittered segments redrawn 24 times a second, with a
     bright core in a soft blue glow, bulging in the middle and pulsing;
   - in flight: the tendril follows the object, which leaves a trail of blue sparkles.

   The beams are additive (a new `lines::glow`). **Tested** in the mock on e1m1's shells box: aimed (beam, glow),
   locked (tendril), pulled (tendril and trail).

   While testing, the world went black during every pull. It was not a bug: that spot is lit by a flickering
   light (style 10), and the scripted pulls always fell in its dark phase.
7. **Decals** (`vr_decals.cpp`; Graphics > Decals, Decal Lifetime, Max Decals):
   - **What makes them:**
     - blood: a pool on the floor under what bleeds (wider the farther it falls) and spatter on a wall near by;
     - flying gibs: drops of blood on the floor under them;
     - explosions: a scorch mark on the nearest surface;
     - bullets, nails, spikes and lasers hitting walls, and melee sparks: chips;
     - lightning and lava spikes: small scorches.
   - **Placing:** each mark lies on the surface a trace finds, on the static world only (none on doors or lifts),
     turned at random. It is shrunk or dropped where it would hang over an edge. The same mark twice in one place
     is skipped.
   - **Drawing:** a modulating blend (the surface times the mark), so marks take the light of where they are with
     no lighting of their own. The marks are an atlas drawn at start-up: 6 blood splats with drops and streaks,
     2 drop clusters, 3 scorches, 3 chips.
   - **Cost:** at most `vr_decal_max` (512, oldest first), each fading out at the end of `vr_decal_life`
     (120 s); at most 24 new ones a frame. One draw an eye.
   - `vr_decal_count` prints how many there are.

   **Tested:** two super shotgun blasts down e1m1's corridor made 11 chips, a rocket a scorch, a knight shot dead
   in e1m2 3 blood marks.
8. **Gibs and heads** (`VR_MakeGibGrabbable`, `vr_carry.qc`; `vr_grab_gibs`):
   - They can be picked up, carried, thrown and force-grabbed like the boxes.
   - Their box is their model's bounds (Quake gave gibs none, a head a fixed 20-unit one); checked on a hell
     knight's gibs, from about 7 x 16 x 7 to 32 x 31 x 6 units.
   - Thrown hard they hurt what they hit and splat (a squelch and blood) on walls.
   - They stay about 30-45 s, and not while held.

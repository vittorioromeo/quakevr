# October 5 voice-note feedback

All ten recordings were transcribed with `transcribe_notes.py --device cpu`. The local transcript is `quakevr/notes/NOTES.md`.

## Implemented

- **Room-scale slipgate entry:** check crossing immediately after physical walking, while its movement direction is still available. Rotate the existing movement velocity when the gate turns the player. The mock walks from the sill at `(544,1376,24)` across the gate and arrives in its destination room.
- **Force grabs through slipgates:** aim at an object's mapped image through an active aperture, using the same folded distance and view mapping as the gate. Selection checks both rooms for obstructions. The object homes in its original room until it crosses the exit aperture, then maps back to the hand's room and can be caught normally. Closing a gate or pulling a trajectory outside its aperture cancels the flight and drops the object in its current room. One gate per pull; player-only/inactive gates exclude props. Existing item eligibility still applies, including gibs' `vr_grab_gibs 2` requirement and ragdolls' hand-only grabbing.
- **Blunt head pops:** fists, guns used as clubs, crowbars, Mjolnir, clubs and pommel strikes pop a corpse's head when they hit its head. A living enemy pops only if that head hit kills it. Body hits and surviving head hits preserve the head. Existing global decapitation/corpse options apply; blade cuts retain their behavior.
- **Small fragments and buttons:** small gibs and brain chunks no longer qualify as thrown button presses. Larger gibs/props still do. Explosion chunks are client-side effects, already incapable of activating server-side buttons.
- **Gib/head size defaults:** captured eleven changed per-model sizes from the author's current config. The prop settings version is 57. Fresh configs receive them; older configs migrate matching old default sizes by model ID and keep custom sizes or reassigned slots. No player config was overwritten.
- **Gremlin flip corpses:** the looping final flip frame no longer resets a touchable corpse to non-interactive. It remains damageable and grabbable after its death animation. Regression tests exercise an actual hand pickup, blunt head pop and shotgun corpse damage (100 to 44 corpse health).
- **Living knockdown movement:** replace unrelated limb torques with a small coordinated chest curl and head nod around the body's settled relative joint pose. A bounded pose controller applies equal/opposite forces through those joints; the limbs follow the body physically. Large pose changes from throws/grabs rebase the resting pose. Corpses never run it. Combat > Knockdowns keeps Struggling Strength, Frequency and Pause. Mock checks verify movement on, stillness off and bounded angular deflection. This is a visual prototype for headset review; automated checks do not establish whether it looks natural.

## Reaching through a slipgate

The proposed design was authorized and implemented in the follow-up. See `NOTES_FEEDBACK_20261005_BATCH2.md` for the clipped rendering, folded interaction, collision handling and test results.

## Verification and local build

Release engine build succeeds; QuakeC compiles with zero warnings. `check_statics.py` and `git diff --check` pass. `notes_oct5_review.ps1` plus `check_notes_oct5.py` cover room-scale entry, portal aim/flight/catch, corpse/live blunt outcomes, fragment button eligibility and gremlin pickup/damage. The existing `notes_feedback_review.ps1` / `check_notes_feedback.py` suite passes, including flashlight saves, ragdoll capacity, drowning, weapon pickups and torch behavior. Struggle checks now measure actual pose deflection as well as motion, rather than requiring the old all-limb torque speed.

The original session has since closed. The follow-up Release build and matching QuakeC are installed in the usual engine and `quakevr/progs.dat` locations. The staged installer described by the original patch is no longer needed for this build.

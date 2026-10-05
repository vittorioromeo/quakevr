# Recorded feedback, October 4, 2026

Ten voice notes were transcribed with `Misc/quakevr/transcribe_notes.py --device cpu`.
The transcript and recording context are in the local `quakevr/notes/NOTES.md`.
Changes are on `vr-ironwail`.

- Monsters drown after 12 seconds underwater, taking increasing damage each second.
  Knockdowns use the physical head's location to determine submersion. Getting up
  underwater does not refill their breath. Fish and the existing scripted/liquid
  boss exceptions remain immune. Controlled by Enemies Hurt by Liquids.
- Every new ragdoll death uses the knockdown pool replacement policy: remove the
  oldest dead body first, then the oldest living knockdown if necessary. Nail
  deaths follow the same path as other deaths.
- Current gameplay/effect/graphics tuning is shipped in `vr_defaults.cfg`.
  Version 87 migrates untouched previous defaults and the legacy inverted-torch
  size. Saved custom values remain in force. Menu history and internal counters
  are excluded from defaults capture.
- Flashlight on/off, attachment, holding hand, head side and flipped grips are
  saved in a player field and carried in `parm50`. Models and poses are rebuilt
  after loading or changing level. A fresh game still starts with the lamp off.
- Debug > Tests > Spawn Pickup Weapons creates grabbable pickups ahead of you.
- Burning > Torch Flame has Hand Motion Strength and Motion Smoothing, alongside
  Swing Lean and Flatten When Fast. Motion of the held torch tip includes both
  translation and rotation of the hand, separated from locomotion for scaling.
- Fire particles rotate 90 degrees clockwise. Fire Particles > Emission Height
  moves their source from base (0) through centre (0.5) to tip (1). Held torches
  keep a single upward flame attached to their end; its height still shrinks past
  horizontal to a short flame at full inversion.
- Corpse headshots use the existing head-pop weapon toggles and head-share test.
  The head pops while the physical body stays intact. Other corpse hits retain
  the existing body-gib behavior. Decapitate Corpses also controls these pops.
- Ragdolls cannot be force grabbed. Direct grabbing still works; legacy Grab
  Ragdolls value 2 is equivalent to hand grabbing.
- Combat > Knockdowns has Struggling Strength, Frequency and Pause. Living
  knockdowns receive gentle oscillating limb torques; corpses do not. Strength
  zero disables this, and pause zero makes motion continuous.

## Verification

Release engine build, QuakeC build (zero warnings), static checks and diff checks.
`notes_feedback_review.ps1` and `check_notes_feedback.py` cover head/weapon
flashlight saves and level transitions, real nail death, pool replacement,
corpse head pops, force-grab exclusion, physical-head drowning, struggling on/off,
weapon spawns and the single upward shortened torch flame. Effects regression
checks cover particle sources, counts, expiry and physical debris behavior.

Use a disposable game base for reviews; they replace its autoexec and write test
saves. Runtime checks use the headless mock backend. Human headset assessment is
still useful for the new motion/particle tuning.

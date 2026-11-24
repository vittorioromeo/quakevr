# Round 26: from 2026-11-24, towards 1.2

A per-feature reference: current behaviour only. Round 25 (2026-11-12 to 11-24, 12,400 lines) is history: `ROUND25.md`;
earlier: `ROUND24.md` to `ROUND22.md`. Grep them; never read one whole.

Open work and bugs: BACKLOG.md (section 1, headed "Open on 2026-11-24", names the ROUND25.md sections that hold the notes).

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND27.md when
this file passes about 2,000 lines.
## polewall: the flat spear's other hand stays on the pole at a wall; no elbow snap in a pole slide (branch agent/polewall, 2026-11-24)

- **Lance at a wall** (lanceoffhand's open end): head-on (no along-the-wall share) the wall state let go of the other hand
  (flatsight `wallLetGo`): pin 0, the hand 18-20 units off the tilted pole. A pole weapon now keeps hold (vr_flatsight.cpp
  `wallLetGo` returns 0 for `twohand::poleWeapon`): pinned, it rides the tilt (vr_view.cpp's rigid move, setupHand's grip).
  Measured (vr_flat_pole_track, vrtesthall 0 560 north, `vr_flat_wall_slide 0`): off 18.1 / along 6.8 -> off 4.4 (5.2 in the
  tilt's ease) / along 9.1 (standing 2.3 / 9.2). Along a wall (slide share 1, the default case there) it already kept hold:
  off 2.0-2.6. **Open:** the 2 units more off at the tilt is the drawn hand's turn on the tilted pole (not the collisions:
  the same with vr_model_collide/vr_body_collide 0); flatlance_test's new `pwall` check allows standing + 3.5.
- **Pole slide elbow snap** (flatfb1124b's open end): found in the arm IK trace (vr_debug_arm 2): ~10 frames in, the crouch
  lowering the shoulders under the locked hands straightens the arm; a two-bone elbow swings ever faster near straight
  (124 -> 143 -> 180 degrees in two frames, 8.6 cm), and the other arm snapped straight 8 cm as its shoulder reach ran out.
  Now a soft reach (vr_avatar.cpp solveArm, `vr_body_arm_soft` 0.05, Tunables > Body > Body "Arm Soft Reach"; 0 = old):
  over the last 5% of the arm the elbow eases straight and the bones lengthen by at most 1.9% to keep the wrist; past all
  reach the arm eases on towards the wrist (`ikWrist`) instead of snapping straight. All drawn arms (VR, flat, others).
  rope_flat_test poleslide: elbow move most 2.74 -> 1.46 units a frame, shake 2.56 -> 1.09 (its bound 3 -> 1.5);
  rope slide unchanged (shake 2.38-2.52: the swaying rope's wrist targets jump 8 cm at its end, not the IK; bound 3 kept).
- Tests: flatlance_test 33/0 (new pwall), rope_flat_test slide 9/0, poleslide 10/0, spear_test 4/0. menu_paths.txt regenerated.
